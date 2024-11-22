# General

SHA-256 hash reversal server created for DTU course 02159 Operating Systems. It currently has thread scheduling, hash table lookups for previously-seen requests, and an SIMD-based implementation for SHA-256. 

**Group members**: Andreas Barba (s214971)

# Code Structure

Source code is in the `src` directory. Main function is in `main.c`. Server and networking code is in `server.c`. Scheduling code in is `sched.c` (and accompanying priority queue is in `pqueue.c`). Hash bruteforcing code is in `hreversal.c` and the SHA-256 implementation is in `sha256.c`. Hash lookup table is implemented in in `htable.c`.

# Experiments

Milestone is in git branch `milestone`. All experiments were conducted by Andreas Barba (s214971). All experiments use the following bash script for the client configuration.

```
SERVER=localhost
PORT=5003
SEED=1
TOTAL=100
START=0
DIFFICULTY=30000000
REP_PROB_PERCENT=20
DELAY_US=80000
PRIO_LAMBDA=0.5

./bin/client $SERVER $PORT $SEED $TOTAL $START $DIFFICULTY $REP_PROB_PERCENT $DELAY_US $PRIO_LAMBDA
```

An experiment's performance is evaluated by executing it five times and taking the average score. The five executions repectively use the seeds from 1 to 5. The execution environment is a machine with a 12th Gen Intel® Core™ i7-1260P with 16 cores running Linux (Ubuntu).

## Scheduling & Parallelism
**Motivation and Description**
Brute-forcing SHA-256 hashes can be easily parallelized as each computation is isolated. And in almost all modern machines, there are several processors to take advantage of. Therefore, a parallel approach should introduce a performance increase.Furthermore, each request from the client has a priority. A task with a higher priority will have a higher penalty than a task with lower priority if they are delayed by the same amount. Therefore, a good scheduling system needs to be used in order to minimize the score. 

Both of these issues involve managing the workload, so we therefore implemented them together. We first implemented a scheduling system based on the O(1) scheduler formerly used in the Linux kernel. This is a fair scheduler which spreads out the processing resources on a variety of different requests at the same time, but works more on tasks with higher priority. However, it turns out that mean turnaround time (accounting for priority) is what matters for the score, so a fair scheduling system is not actually desirable. Therefore, we implemented an unfair scheduler which simply focuses on only one task at a time. The task focused on is the task with the smallest value of `computation_remaining / priority`, where `computation_remaining` is the remaining number of unattempted inputs in the hash brute-forcing procedure. With this metric, we account for the estimated time remaining for a task as well as the increased cost of tasks with higher priority. The data structure for getting the appropriate task is implemented using a binary heap-based priority queue. It takes O(1) time to get the task with smallest key and O(log(n)) time to insert and remove a task. The sceduler is essentially a thread pool. Each thread repeatedly works on a chunk associated with the task with the minimum key. Each thread does the following three things on repeat: 
  1. Get a task. Lock the scheduler and take a chunk of a task to work on.
  2. Work. Begin brute-forcing hashes based on the inputs in the chunk gotten in the previous step.
  3. Finalise. If the brute-forcing was successful: lock the scheduler, remove the task from the scheduler, notify other workers than they can drop the task, send the result to the client, and clean up.

The number of threads used is double the number of processors in the system. 

**Setup**
The experiment code is containted in the git branches `scheduling` and `fair_scheduling`, and it is compared to the milestone version (in branch `milestone`). The relevant code is in `sched.c`. The client configuration is as noted above. The score decrease is calculated relative to the milestone version.

**Results**
| Seed     | Milestone  | Fair scheduling | Unfair scheduling |
|----------|------------|-----------------|-------------------|
| 1        | 49401036   | 700724          | 464059            |
| 2        | 51436081   | 5861893         | 362672            |
| 3        | 40944227   | 408137          | 280985            |
| 4        | 46511216   | 363631          | 278866            |
| 5        | 54794274   | 2079951         | 1014743           |
| Avg      | 48617366.8 | 1882867.2       | 480265            |
| Decrease | 1          | 28.82           | 101.23            |

**Conculusion**
We see that both scheduling versions yield an increase in performance, with the unfair scheduler being most performant. The execution environment had 16 cores, and since the computation is almost perfectly parallelizable, a performance increase of around 16 times was expected. We do indeed see a large performance increase. The unfair scheduling version was the version we continued with.

## Hash Table

**Motivation and Description**
If we store information about previous requests, we can skip the computation of requests that have already been seen before. Motivated by these potential time savings, we implemented a lookup table using a hash table, with the key simply being the SHA-256 hash sent in the request. With the lookup table, when we recieve a request, we use the hash table to check if the request has been seen before. If we have a table miss, we place the request in the scheduler's queue as normal. If we have a hit and the computation was completed, we immediately respond with the result to the request. If, instead, the result is not stored but is currently being computed or in queue, we can simply register the new request as also being interested in the pending result and update the internally used priority. When the computation is done, we send the result to all interested parties.

This addition means that the scheduler needs to be slightly modified to account for every task waiting for a result. The scheduling metric used is simply modified to `computation_remaining / sum(priority)`.

**Setup**
The experiment is contained in git branch `lookup_table` and is compared to the version in the `scheduling` branch (the unfair scheduling version). The relevant code is in `htable.c` with some modifications in `sched.c`. The client configuration is as noted above. The score decrease is calculated relative to the unfair scheduling version.

**Results**
| Seed     | Unfair scheduling | Lookup table |
|----------|-------------------|--------------|
| 1        | 464059            | 271257       |
| 2        | 362672            | 179512       |
| 3        | 280985            | 158510       |
| 4        | 278866            | 142385       |
| 5        | 1014743           | 265912       |
| Avg      | 480265            | 203515.2     |
| Decrease | 1                 | 2.36         |


**Conclusion**
We see that using a lookup table is faster. This was as expected, as the server can skip 20 % of the computation if the repeated request probability is 20 %. Due to the decrease in score, we continued with the lookup table version.

## SHA-256 Computation

**Motivation and Description**
The OpenSSL implementation of SHA-256 hashing is generalized with no assumptions made about its usage. However, our usage is slightly specialized. We know that the input size is always 64 bits long, and we know that we are brute-forcing hashes. For the former, this means we can remove some of the overhead associated with digesting chunks of data as well as have the compiler optimize for the fixed input size. For the latter, this means we can parallelize using SIMD. Since we are only interested in the input and not the computed hash itself, it means that we can directly reverse the very last part of the hash received from the client and comparing to this, reducing individual hash computation times during brute-forcing. 

With these ideas, we first implemented a naive solution, solely based on the first ideas, i.e. no digest overhead and compiler optimization associated with the fixed input length. Afterwards, we implemented a solution based on SIMD (using GCC vector extensions), tested with several configuration: 4-way, 8-way, and 16-way. We also implemented a version using SIMD that did not use as many assignment operations, but this seemingly made it slower. (This version is referred to in the result section as Slow SIMD.) Finally, we implemented a version using the x86 SHA instructions. However, these instructions are not present on all systems.

**Setup**
The experiment is contained in git branch `sha_implementation` and compared with the lookup table version (contained in branch `lookup_table`). The lookup table version is almost identical other than the fact that it uses OpenSSL for the SHA-256 implementation. The client configuration is as noted above. The score decrease is calculated relative to the lookup table version.

Depending on preprocessor flags set, different SHA-256 implementations will be used. Define `SB_VECTORIZE` for *x*-way SIMD (set `VEC_SIZE` in `sha256.h` to choose *x*). The flag can be defined by compiling with `-DSB_VECTORIZE` (for GCC). This can be added in the `FLAGS` variable in the Makefile. Define `SB_USE_SLOW_VECTORIZATION` as well as `SB_VECTORIZE` to use the slower *x*-way SIMD implementation. Define `SB_USE_x86_64_SHA_SIMD` to use x86 SHA hardware acceleration (instructions may or may not be present which may lead to compilation failure). Define `SB_USE_NAIVE_SHA` to use a naive implemention of SHA-256 based on simple serial code. Define none of these flags to use the OpenSSL implementation. 

**Results**
| Seed     | Lookup table | Naive      | Faster SIMD, 4-way | Faster SIMD, 8-way | Faster SIMD, 16-way | Slow SIMD, 8-way | x86 SHA instructions |
|----------|--------------|------------|--------------------|--------------------|---------------------|------------------|----------------------|
| 1        | 271257       | 13862304   | 2772468            | 772398             | 1454051             | 828646           | 666927               |
| 2        | 179512       | 9224684    | 1337205            | 320269             | 588856              | 362476           | 285993               |
| 3        | 158510       | 9102744    | 1571096            | 335111             | 734934              | 320791           | 301479               |
| 4        | 142385       | 6954266    | 1051481            | 264858             | 466636              | 286707           | 229955               |
| 5        | 265912       | 12843628   | 2653683            | 706441             | 1295861             | 740375           | 483347               |
| Avg      | 203515.2     | 10397525.2 | 1877186.6          | 479815.4           | 908067.6            | 507799           | 393540.2             |
| Decrease | 1            | 0.02       | 0.11               | 0.42               | 0.22                | 0.40             | 0.52                 |

**Conclusion**
Looking at the table, we see that none of the implemented SHA implementations performed better than the OpenSSL implementation. After some investigation, we saw that the OpenSSL SHA-256 implementation for x86 implementation uses the specialized SHA instructions as we had also done in one implementation. But it seemingly uses them in a smarter than us, since it is faster. Therefore, we can conclude that is faster to use the OpenSSL solution for the local execution environment. However, the DTU compute execution environment does not support the x86 SHA instructions despite having an x86 archicture (likely due to the processor model being from 2012). Here, the fastest solution is the 4-way SIMD implementation. The 8-way SIMD implementation is slower than the 4-way one, likely due to it not supporting wide enough SIMD instructions.