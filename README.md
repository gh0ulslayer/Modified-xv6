To compare the result of the scheduling algorithms a bench.c file is created it creates 10 processes, with decreasing CPU burst time and increasing IO time as the creation time increases.

The number of ticks correspond to the waiting time of the benchmark program :-
    • Round Robin(DEFAULT): 3112 ticks.
    • PBS : 3249 ticks. 
    • MLFQ: 3036 ticks.
    • FCFS: 4039 ticks.
      
# Conclusion and Comparison

All three gave similar result except FCFS , it's considered the worst case.
But most of the time MLFQ gave better result.
FCFS < PBS <= RR <= MLFQ 

MLFQ has the least time as there are many queues with different priorities and a process which takes more CPU time is shifted to less priority queue and also prevents starvation by aging i.e shifting to upper queues after some fixed time.Thus , it takes the least time.

In PBS we give higher prioirty preference to IO bound processes hence they run first and gives a better result. 

Round Robin is premptive and yields after every clock cycle hence gives better reults.

FCFS is waiting for the largest time as there is no preemption and if a process with larger time comes first all the rest processes have to starve for cpu execution.


