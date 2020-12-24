
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/proc_fs.h>
#include <linux/net.h>
#include <net/tcp.h>
#include <net/inet_connection_sock.h>

#define numOfState	3

#define	state0_max	100
#define	state1_max	100
#define	state2_max	100

#define	Q_CONG_SCALE	1024

#define	numOfAction	3

#define	sizeOfMatrix 	state0_max * state1_max * state2_max * numOfAction

static const u32 probertt_interval_msec = 10000;
static const u32 training_interval_msec = 100;
static const u32 max_probertt_duration_msecs = 200;
static const u32 estimate_min_rtt_cwnd = 4;

static const u32 alpha = 200;
static const u32 beta = 1;
static const u32 delta = 1; 

static const char procname[] = "qcong";


/* 
 *	Learning parameter Definintion
 *	Scaled by "Q_CONG_SCALE"
 */
static const u32 learning_rate = 512;
static const u32 discount_factor = 10; 
enum action{
	CWND_UP,
	CWND_NOTHING,
	CWND_DOWN,
};

enum Q_cong_mode{
	NOTHING,
	TRAINING,
	ESTIMATE_MIN_RTT,
	STARTUP,
};

typedef struct{
	u8  enabled;
	int mat[sizeOfMatrix];
	u8 row[numOfState];
	u8 col;
}Matrix; 

static Matrix matrix;

struct Q_cong{
	u32	mode:3,
		exited:1,
		unused:28;
	u32 	last_sequence; 
	u32	estimated_throughput;
	u32	last_update_stamp;
	u32	last_packet_loss;
	u32 	retransmit_during_interval; 

	u32	last_probertt_stamp; 
	u32 	min_rtt_us; 
	u32	prop_rtt_us;
	u32	prior_cwnd;

	u32	current_state[3];
	u32	prev_state[3];
	u32 	action; 
};

static void createMatrix(Matrix *m, u8 *row, u8 col){
	u32 i;
	
	if (!m)
		return;

	m->col = col; 

	for(i=0; i<numOfState; i++)
		*(m->row+i) = *(row+i);

	
	for(i=0; i<sizeOfMatrix; i++)
		*(m->mat + i) = 0; 

	m -> enabled = 1; 
}

static void eraseMatrix(Matrix *m){
	u8 i=0; 
	if (!m)
		return;

	for(i=0; i<numOfState; i++)
		*(m->row + i) = 0; 

	m -> col = 0; 

	//for(i=0; i<sizeOfMatrix; i++)
	//	*(m->mat + i) = 0; 

	m -> enabled = 0; 
}

static void setMatValue(Matrix *m, u8 row1, u8 row2, u8 row3, u8 col, int v){
	u32 index = 0; 
	if (!m)
		return;

	index = m->col * (row1 * m->row[1] * m->row[2] + row2 * m->row[2] + row3) + col;
	*(m -> mat + index) = v;
}

static int getMatValue(Matrix *m, u8 row1, u8 row2, u8 row3, u8 col){
	u32 index = 0; 
	if (!m)
		return -1; 

	index = m->col * (row1 * m->row[1] * m->row[2] + row2 * m->row[2] + row3) + col;
	
	return *(m -> mat + index);
}

static u32 tcp_q_cong_ssthresh(struct sock *sk){
	return TCP_INFINITE_SSTHRESH; /* TCP Q-congestion does not use ssthresh */
}

static void reset_cwnd(struct sock *sk, const struct rate_sample *rs){
	
	struct Q_cong *qc = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	u32 loss; 

	if (qc -> mode == STARTUP){
		if(inet_csk(sk) -> icsk_ca_state >= TCP_CA_Recovery){
			qc -> mode = NOTHING; 
		}
		else{
			tp -> snd_cwnd += rs -> acked_sacked;
		}
		printk(KERN_INFO "cwnd : %u", tp -> snd_cwnd);
	}

}

static u32 getAction(struct sock *sk, const struct rate_sample *rs){
	struct Q_cong *qc = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);

	u32 Q[numOfAction];
	u32 state[numOfState]; 
	u8 i;
	u32 max_index = 0; 
	u32 max_tmp = 0 ;
	u32 rand; 

	state[0] = (qc -> min_rtt_us - qc -> prop_rtt_us) >> 8;
	state[1] = tp -> snd_cwnd;
	state[2] = rs -> rtt_us >> 10;

	for(i=0; i<numOfAction; i++){
		Q[i] = getMatValue(&matrix, qc -> current_state[0], qc->current_state[1], qc->current_state[2], i);
	}

	if(Q[0] == Q[1] && Q[1] == Q[2]){
		get_random_bytes(&rand, sizeof(rand));
		return (rand%3);
	}
	else{
		if(Q[0] == 0)
			return 0; 
		else if(Q[1] == 0)
			return 1;
		else if(Q[2] == 0)
			return 2;
		else{
			max_tmp = Q[0];
			for(i=0; i<3; i++){
				if(max_tmp >= Q[i]){
					max_tmp = Q[i];
					max_index = i; 
				}
			}
			return max_index;
		}

	}
	return CWND_NOTHING; 
}

static u8 checkEnvironment(struct sock* sk){
	struct tcp_sock *tp = tcp_sk(sk);
	struct Q_cong *qc = inet_csk_ca(sk);

	return 0; 
}

static u32 getRewardFromEnvironment(struct sock *sk, const struct rate_sample *rs){
	struct tcp_sock *tp = tcp_sk(sk);
	struct Q_cong *qc = inet_csk_ca(sk);
	u32 retransmit_division_factor; 

	/* 
	 * Utility Function
	 *
	 * Utility = (alpha * throughput) / ((beta * sRTT) * (delta * packetloss_for_interval))
	 *
	 */
	
	retransmit_division_factor = qc -> retransmit_during_interval + 1;
	if(retransmit_division_factor == 0 || rs->rtt_us == 0)
		return 0;

	printk(KERN_INFO "throughput : %u / rtt : %lu / retransmission : %u", qc -> estimated_throughput, rs -> rtt_us, qc -> retransmit_during_interval);

	return (alpha * qc -> estimated_throughput) / (beta * rs->rtt_us) / (delta * retransmit_division_factor);
}

static void executeAction(struct sock *sk, const struct rate_sample *rs){
	struct tcp_sock *tp = tcp_sk(sk);
	struct Q_cong *qc = inet_csk_ca(sk);

	qc -> prev_state[0] = (qc -> min_rtt_us - qc -> prop_rtt_us) >> 8;
	qc -> prev_state[1] = tp -> snd_cwnd;
	qc -> prev_state[2] = rs -> rtt_us >> 10;

	switch(qc -> action){
		case CWND_UP:
			tp -> snd_cwnd  = tp->snd_cwnd +1; 
			break; 
			
		case CWND_DOWN:
			tp -> snd_cwnd  = tp->snd_cwnd -1; 
			break; 

		default : 
			break;

	}

}

static u32 tcp_q_cong_undo_cwnd(struct sock* sk){
	struct tcp_sock *tp = tcp_sk(sk);
	
	printk(KERN_INFO "undo congestion control");
	return max(tp->snd_cwnd, tp->prior_cwnd);
}

static void calc_retransmit_during_interval(struct sock* sk){
	struct tcp_sock *tp = tcp_sk(sk);
	struct Q_cong *qc = inet_csk_ca(sk);

	qc -> retransmit_during_interval = (tp -> total_retrans - qc -> last_packet_loss) * training_interval_msec / jiffies_to_msecs(tcp_jiffies32 - qc -> last_update_stamp);
	
	qc -> last_packet_loss = tp -> total_retrans; 
	printk(KERN_INFO "Packet loss for interval : %u", qc -> retransmit_during_interval);
}

static void calc_throughput(struct sock *sk){
	struct tcp_sock *tp = tcp_sk(sk);
	struct Q_cong *qc = inet_csk_ca(sk);

	u32 segout_for_interval;
	
	segout_for_interval = (tp -> segs_out - qc ->last_sequence) * tp ->mss_cache; 

	qc -> estimated_throughput = segout_for_interval * 8 / jiffies_to_msecs(tcp_jiffies32 - qc -> last_update_stamp); 

	printk(KERN_INFO "Throughput : %u\ttime : %u",qc -> estimated_throughput, jiffies_to_msecs(tcp_jiffies32 - qc->last_update_stamp));

	qc -> last_sequence = tp -> segs_out;

}

static void update_state(struct sock *sk, const struct rate_sample *rs){

	struct tcp_sock *tp = tcp_sk(sk);
	struct Q_cong *qc = inet_csk_ca(sk);

	u8 i; 
	for (i=0; i<3; i++)
		qc -> prev_state[i] = qc -> current_state[i];

	qc -> current_state[0] = qc -> min_rtt_us >> 8;
	qc -> current_state[1] = tp -> snd_cwnd; 
	qc -> current_state[2] = rs -> rtt_us >> 8; 

	for(i=0; i<3; i++){
		if(qc -> current_state[i] < 0)
			qc -> current_state[i] = 0;

		else if (qc -> current_state[i] > 100)
			qc -> current_state[i] = 100; 
	}

	printk(KERN_INFO "c_state : %u %u %u\tp_state : %u %u %u / smoothedRTT : %u", qc -> current_state[0], qc -> current_state[1], qc -> current_state[2], qc -> prev_state[0], qc->prev_state[1], qc->prev_state[2], rs->rtt_us);
}

static void update_Qtable(struct sock *sk, const struct rate_sample *rs){
	/* 
	 * 	1. Select state 
	 *	   1) minimum RTT - propagation delay
	 *	   2) packet loss for interval
	 *         3) smoothed RTT
	 *      
	 *      2. Scaling state
	 *	
	 *	3. Q(s_i,a_i) <- Q(s_i,a_i) + learning_rate * [ reward + discount_factor * Q(s_(i+1), a) ] 
	 *
	 */

	struct tcp_sock *tp = tcp_sk(sk);
	struct Q_cong *qc = inet_csk_ca(sk);

	u32 thisQ[numOfAction]; 
	u32 newQ[numOfAction];
	u8 i;
	u32 updated_Qvalue;
	u32 max_tmp; 


	
	for(i=0; i<numOfAction; i++){
		thisQ[i] = getMatValue(&matrix, qc->prev_state[0], qc->prev_state[1], qc->prev_state[2], i);
		newQ[i] = getMatValue(&matrix, qc->current_state[0], qc->current_state[1], qc->current_state[2], i);
	}

	//max_tmp = max_t(u32, newQ[CWND_UP], newQ[CWND_NOTHING]);
	//max_tmp = max_t(u32, max_tmp, newQ[CWND_DOWN]);

	max_tmp = newQ[0];
	for(i=0; i<numOfAction; i++){
		if(max_tmp >= newQ[i])
			max_tmp = newQ[i]; 
	}

	updated_Qvalue = ((Q_CONG_SCALE-learning_rate)*thisQ[qc ->action] +
			(learning_rate * (getRewardFromEnvironment(sk,rs) + discount_factor * max_tmp - thisQ[qc -> action] )))>>10;

	if(updated_Qvalue == 0){
		qc -> exited = 1; 
		return;
	}
	
	printk(KERN_INFO "reward : %u, updated Q : %u", getRewardFromEnvironment(sk, rs),updated_Qvalue);
		
	setMatValue(&matrix, qc->prev_state[0], qc->prev_state[1], qc->prev_state[2], qc->action, updated_Qvalue);

}


static void training(struct sock *sk, const struct rate_sample *rs){
	struct tcp_sock *tp = tcp_sk(sk);
	struct Q_cong *qc = inet_csk_ca(sk);
	u32 reward;

	
	u32 training_timer_expired = after(tcp_jiffies32, qc -> last_update_stamp + msecs_to_jiffies(training_interval_msec)); 

	if(training_timer_expired && qc -> mode == NOTHING){

		/* Step1. Check if newstate is valid */ 
		if (qc -> action == 0xffffffff)
			goto execute;

		/* Step2. Estimate state */ 
		calc_throughput(sk);
		calc_retransmit_during_interval(sk);

		/* Step3. Check if the scenario is exited. */
		if (qc -> exited == 1){
			tp -> snd_cwnd = TCP_INIT_CWND; 
			qc -> exited = 0; 
			return; 
		}


		/* Step4. calculate reward & update Q-table */
		reward = getRewardFromEnvironment(sk,rs);
		printk(KERN_INFO "Reward : %u", reward);
		update_Qtable(sk,rs);

execute:
		printk(KERN_INFO "execute Action");
		qc -> action = getAction(sk,rs);
		executeAction(sk, rs);
		qc -> last_update_stamp = tcp_jiffies32; 
		printk(KERN_INFO "This action : %u",qc -> action);
	}
}

static void update_min_rtt(struct sock *sk, const struct rate_sample* rs){
	struct tcp_sock *tp = tcp_sk(sk);
	struct Q_cong *qc = inet_csk_ca(sk);
	u32 estimate_rtt_expired; 

	u32 update_filter_expired = after(tcp_jiffies32, 
			qc -> last_probertt_stamp + msecs_to_jiffies(probertt_interval_msec));


	if (rs -> rtt_us > 0){	
		if (rs -> rtt_us < qc -> min_rtt_us){
			qc -> min_rtt_us = rs -> rtt_us;
			qc -> last_probertt_stamp = tcp_jiffies32; 
			if (qc -> min_rtt_us < qc-> prop_rtt_us)
				qc -> prop_rtt_us = qc -> min_rtt_us; 
		}
	}

	if(update_filter_expired && qc -> mode == NOTHING){ 
		printk(KERN_INFO "Enter ESTIMATE_MIN_RTT Mode");
		qc -> mode = ESTIMATE_MIN_RTT; 
		qc -> last_probertt_stamp = tcp_jiffies32; 
		qc -> prior_cwnd = tp -> snd_cwnd;
		tp -> snd_cwnd = min(tp -> snd_cwnd, estimate_min_rtt_cwnd);
		qc -> min_rtt_us = rs -> rtt_us;
	}

	if(qc -> mode == ESTIMATE_MIN_RTT){
		estimate_rtt_expired = after(tcp_jiffies32, 
				qc -> last_probertt_stamp + msecs_to_jiffies(max_probertt_duration_msecs)); 
		if(estimate_rtt_expired){
			/* end of ESTIMATE_MIN_RTT mode */
			qc -> mode = NOTHING; 
			printk(KERN_INFO "Exit ESTIMATE_MIN_RTT mode");
			tp -> snd_cwnd = qc -> prior_cwnd;
		}
	}

}


static void tcp_q_cong_main(struct sock *sk, const struct rate_sample *rs){

	struct tcp_sock *tp = tcp_sk(sk);
	struct Q_cong *qc = inet_csk_ca(sk);

	//reset_cwnd(sk, rs);
	update_state(sk,rs);
	training(sk, rs);
	update_min_rtt(sk,rs);

}


static void init_Q_cong(struct sock *sk){
	struct Q_cong *qc;
	struct tcp_sock *tp = tcp_sk(sk);
	int i,j,k,l, index =0; 
	u8 Q_row[numOfState] = {state0_max, state1_max, state2_max};
	u8 Q_col = numOfAction; 

	printk(KERN_INFO "Q_cong : Initialize Q_congestion control");

	qc = inet_csk_ca(sk);
	
	qc -> mode = STARTUP;
	qc -> last_sequence = 0;
	qc -> estimated_throughput = 0; 
	qc -> last_update_stamp = tcp_jiffies32;  
	qc -> last_packet_loss = 0;

	qc -> last_probertt_stamp = tcp_jiffies32;
	qc -> min_rtt_us = tcp_min_rtt(tp);
	qc -> prop_rtt_us = tcp_min_rtt(tp);
	qc -> prior_cwnd = 0; 
	qc -> retransmit_during_interval = 0; 

	qc -> action = -1; 
	qc -> exited = 0; 
	qc -> prev_state[0] = 0;
	qc -> prev_state[1] = 0; 
	qc -> prev_state[2] = 0; 
	qc -> current_state[0] = 0;
	qc -> current_state[1] = 0;
	qc -> current_state[2] = 0; 

	createMatrix(&matrix, Q_row, Q_col);
}

static void release_Q_cong(struct sock* sk){
	eraseMatrix(&matrix);
}

struct tcp_congestion_ops q_cong = {
	.flags		= TCP_CONG_NON_RESTRICTED, 
	.init		= init_Q_cong,
	.release	= release_Q_cong,
	.name 		= "q_cong",
	.owner		= THIS_MODULE,
	.ssthresh	= tcp_q_cong_ssthresh,
	//.cong_avoid	= tcp_q_cong_cong_avoid,
	.cong_control	= tcp_q_cong_main,
	.undo_cwnd 	= tcp_q_cong_undo_cwnd,
};

static int __init Q_cong_init(void){
	printk(KERN_INFO "Q_cong : Initialize Q_congestion control Module");

	return tcp_register_congestion_control(&q_cong);
	//return 0;
}

static void __exit Q_cong_exit(void){
	printk(KERN_INFO "Q_cong : Exit Q_congestion control Module");
	tcp_unregister_congestion_control(&q_cong);
}

module_init(Q_cong_init);
module_exit(Q_cong_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jjun");
MODULE_DESCRIPTION("Reinforcement Learning Congestion Control Algorithm");

