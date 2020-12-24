Q-learning based Congestion Control Algorithm model for Linux kernel
================================================================================

This is the congestion control algorithm model using the Q-learning, one of the reinforcement learning algorithm, to test the possibility of machine learning in congestion control.


Based on
--------------------------------------------------------------------------------
This Congestion control algorithm is implemented with linux kernel module, therefore, you have to compile this module and insert it in order to use this algorithm.

The linux kernel version where this CCA was implemented: Linux kernel version 4.14
The version of Operating stystem: Ubuntu 18.04

Requirements
--------------------------------------------------------------------------------
Nothing special...
This congestion control module can be linux kernel with "make"

Running
--------------------------------------------------------------------------------

1) **Compile**    
make all    

2) **insert module**    
insmod q_cong.ko    

3) **check the congestion control**
sysctl net | grep congestion

If compiled and inserted normally, q_cong would have been added to 'net.ipv4.tcp_allowed_congestion_control'.

4) **Remove module**
rmmod q_cong.ko

