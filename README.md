# General

Program created for DTU course 02159 Operating Systems. It currently has thread scheduling and hash table lookups for for previously-completed requests. 

# Code Structure

Source code is in the `src` directory. Main function is in `main.c`. Server and networking code is in `server.c`. Scheduling code in is `sched.c` (and accompanying priority queue is in `pqueue.c`). Hash bruteforcing code is in `hreversal.c` and custom SHA256 hashing code is in `sha256.c`. 

# Experiments

## Scheduling

It turns out mean turnaround time (accounting for priority) is what matters for the score, so a fair scheduling system is not actually desirable.