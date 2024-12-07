Lab 3 

Group 20

Karen Lee, Izat Temiraliev


Paper Implemented: https://ieeexplore.ieee.org/document/7446087

Code Implemented in:
pref_bop.param.def  – to be able to enable BO prefetcher 
pref_bop.param.h 
pref_table.def - we added our “bop” here

Implementation of the Prefetcher itself:
pref_bop.c - implementation for BO prefetcher
pref_bop.h - header file for BO prefetcher

We used per-core implementation, since UL1 and UMLC operate on different cores.
