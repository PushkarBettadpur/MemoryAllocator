***********
Main Components:
***********

memfiles/mm.c:  
        This file contains the implementation of malloc, free and realloc
        using an segregated free list structure

memfiles/mdriver.c:  
        The malloc driver that tests the implementations of 
        this memory allocator for space utilization and throughput

traces/:  
        Sequence of malloc, free, realloc calls that are called by 
        mdriver.c to test the implementation

**********************************
Other support files for the driver
**********************************

fsecs.o:  
Wrapper function for the different timer packages  

clock.o:  
Routines for accessing the Pentium and Alpha cycle counters  

fcyc.o:  
Timer functions based on cycle counters  

ftimer.o:  
Timer functions based on interval timers and gettimeofday()  

memlib.{o,h}:  
Models the heap and sbrk function  

*******************************
Building and running the driver
*******************************
To build the driver, type "make" to the shell.

To run the driver on a tiny test trace:

        unix> mdriver -V -f short1-bal.rep
        
To run the driver over all the provided traces and compare performance to libc malloc:

        unix> mdriver -Vl

The -V option prints out helpful tracing and summary information.

To get a list of the driver flags:

        unix> mdriver -h
