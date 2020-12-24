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

1) **Compile**    
make all    

2) **insert module**    
insmod q_cong.ko    

