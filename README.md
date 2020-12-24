Mininet-based Dumbbell topology Emulator and Analyzer for AQM router
================================================================================

Mininet-based dummbell topology which supports the AQM routers and ECN hosts
referred to [measurement-framework](https://gitlab.lrz.de/tcp-bbr/measurement-framework) configured by J. Aulbach.

  1) This emluation allows the bottleneck router to support active queue management (AQM) scheduler that the host provides such as Taildrop [CoDel](https://man7.org/linux/man-pages/man8/tc-codel.8.html), [RED](https://man7.org/linux/man-pages/man8/tc-red.8.html), [FQ_CoDel](https://man7.org/linux/man-pages/man8/tc-fq_codel.8.html) and so on.

  2) This emulation enables the sender to support ECN feedback with simple option activation.

  3) Additionally, this emulator fully support BBRv2 congestion control algorithm introduced by Google. Therefore, if you install the BBR v2 algorithm in the linux kernel and use it, this emulator provides the traced result in .xls files for each BBRv2 hosts.


Based on
--------------------------------------------------------------------------------
This emulator is written by Python and can be executed on the linux supported mininet`

- [run_mininet.py](https://github.com/syj5385/bbr_dumbbell/blob/master/run_mininet.py): This file was fully configured by the author to configure the dumbbell topology and perform the experiment where the senders transmits bulk data to the receivers

- [analyze.py](https://github.com/syj5385/bbr_dumbbell/blob/master/analyze.py): This file was fully implemented by the J. Aulbach, to analyze the pcap file and plot the results in pdf. 

Requirements
--------------------------------------------------------------------------------
1) python-pip	>> apt-get install python-pip
2) mininet	>> apt-get install mininet
3) ethtool	>> apt-get ethtool
4) moreutils	>> apt-get netcat
5) python	>> apt-get install python
6) dpkt		>> pip install dpkt==1.9.1
7) numpy	>> pip install numpy==1.14.0
8) matplotlib	>> pip install matplotlib==2.1.1

	You can install all of them using 'install.sh' shell script
	ex>  sudo ./install.sh
	
9) Only Reno and CUBIC congestion control algorithms can be used in the Linux kernel for general distribution. Therefore, If you want to perform an experiment using various congestion control algorithms, you should compile and add the congestion control you want to test in the form of a kernel module, or download the full Linux kernel from the web site and install the congestion control you want to add. 

Running
--------------------------------------------------------------------------------
**Installation**
1) ./install.sh

**Execution**    
Usage: run_mininet.py [-h] [-b RATE] [-l LIMIT_BYTE] [-r DELAY] [-d DURATION] [-n OUTPUT] [-q QDISC] [-c HOST] [-e MY_ECN] [-p LOSS] [-i INTERVAL]

**-h**: [help]		show the help message and exit    
**-b**: [BtlBw]		bottleneck bandwidth in Mbps (default: 10 Mbps]    
**-l**: [limit]		bottleneck buffer size in byte (default: 25000b)    
**-r**: [router delay]	Initial delay between switch 1 and switch 2 (default: 0ms)    
**-d**: [duration]		Test duration in second (default: 10 sec)    
**-n**: [output]		The name of output directory (default: Congctl)    
**-q**: [Qdisc]		Configuring Active Queue management; netem, CoDel, FQ_CoDel, RED etc.. / e.g. '': no AQM / 'codel [limit PACKETS] [target TIME] [interval TIME] [ecn | noecn] [ce_threshold TIME]': Set CoDel    
**-c**: [CCA Host]		Set sending host / cubic:10ms,cubic:10ms -> 2 cubic host with 10ms access delay / bbr:10ms,cubic:30ms -> 1 bbr host with 10ms access round-trip delay and 1 cubic host with 30ms access round-trip delay    
**-e**: [ECN]		Activation of ECN in sending host (default: 1) / 1: enable / 0: disable    
**-p**: [LOSS]		set packet loss rate in percentage (default: 0)    
**-i**: [host interval]	set the interval when multiple flows enter the bottleneck link if the number of flows is larger than 1 (default: 0)     

**Analyze**    
python analyze.py [-d OUTPUT_DIRECTORY]

**Result File**    
 Congctl_MMdd_HHmmSS    
 |    
 +-- 10.1.0.#.bbr:	 The traced result in BBR    
 |    
 +-- s1.pcap / s3.pcapa: The packet captured in switch 1 and 3    
 |    
 +-- s2-eth2-tbf.buffer: Buffer backlog in bottleneck link     
 |    
 +-- csv_data    
 |   |    
 |   +-- *.csv: The results for each sending host    
 |    
 +-- pdf_plots    
     |    
     +-- plt_complete.pdf: The plot for sending hosts    

**Example**
python run_mininet.py -b 50 -l 62500 -d 20 -n test -q '' -c cubic:10ms -e 0 -p 0
