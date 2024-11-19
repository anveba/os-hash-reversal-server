# General

Program created for DTU course 02159 Operating Systems. It currently has thread scheduling, hash table lookups for for previously-completed requests, and an SIMD-based implementation for SHA-256. 

**Group members**: Andreas Barba (s214971)

# Code Structure

Source code is in the `src` directory. Main function is in `main.c`. Server and networking code is in `server.c`. Scheduling code in is `sched.c` (and accompanying priority queue is in `pqueue.c`). Hash bruteforcing code is in `hreversal.c` and the SHA-256 implementation is in `sha256.c`. Hash lookup table is implemented in in `htable.c`.

# Experiments

## Scheduling & Parallelism

It turns out mean turnaround time (accounting for priority) is what matters for the score, so a fair scheduling system is not actually desirable.

## Hash Table
If we store information about previous requests, we can skip the computation requests that have already been seen before. This is implemented using a hash table, with the key simply being the SHA-256 hash sent in the request. When we recieve a request, we use the hash table to check if the request has been seen before. If we have a lookup table miss, we place the request in the queue as normal. If we have a hit and the result is stored, we immediately respond to the request. If, instead, the result is not stored but instead is currently being computed, we can simply register the new request as also being interested in the pending result. When the computation is done, we send the result to all interested parties.

Experiment implementation in `htable.c`.

## SHA-256 Computation
