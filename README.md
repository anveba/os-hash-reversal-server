# SHA-256 Hash Reversal Server

SHA-256 hash reversal server created for DTU course 02159 Operating Systems. It currently has thread scheduling, hash table lookups for previously-seen requests, and an SIMD-based implementation for SHA-256. 

**Group members**: Andreas Barba (s214971)

# Repository Structure

The source code is in the `src` directory. The main function is in `main.c`. The server and networking code is in `server.c`. The scheduler code in is `sched.c`, and the code for the accompanying priority queue is in `pqueue.c`. The hash brute-forcing code is in `hreversal.c` and the SHA-256 implementations are contained in `sha256.c`. The hash lookup table is implemented in in `htable.c`.

The `main` git branch contains the newest version of the server optimised for the DTU Compute execution environment. All other branches can be though of as snapshots of earlier versions. The `milestone` branch contains the milestone version of the server. The `fair_scheduling` and `scheduling` branches contain the versions that use fair and unfair scheduling, respectively, used for the experiments. The `lookup_table` branch contains the experiment code for the implementation of the hash lookup table for previously-seen requests. The `sha_implementation` branch contains code for various implementations of SHA-256 used in the experiments. 

# Design

An overview of the execution of the final solution is as follows. When the program is run, the server is opened and a scheduler, essentially a thread pool, is initialized, creating a number of threads equal to double the number of cores in the machine. The main thread operates the server interface and listens to requests from clients. When a request is recieved, it will be added to the scheduler's task queue. The scheduler contains a priority queue to appropriately order the processing of tasks, based on an internally-used priority metric. Each thread associated with the scheduler will repeatedly retrieve a chunk of the request at the front of the priority queue, compute hashes in the chunk, and finally, if the hash reversal was successful, send the result to the client(s). The fact that some requests are repeated is also taken advantage of. When a task is recieved by the scheduler from the server interface, it will first query the lookup table to check if the request has been received before. If it has and it was completed, it will send the result immedietely to the client. If it is in progress, the client will be added to the list of clients interested in the pending result.

# Experiments

All experiments were conducted by Andreas Barba (s214971). All experiments use the following bash script for the client configuration.

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

An experiment's performance is evaluated by executing it five times and taking the average score. The five executions repectively use the seeds from 1 to 5. The execution environment is a machine with a 12th Gen Intel Core i7-1260P with 16 cores running Linux (Ubuntu).

## Scheduling and Parallelism
**Motivation and Description**
Brute-forcing SHA-256 hashes can be easily parallelized as each computation is independent of each other. And in almost all modern machines, there are several processors to take advantage of. Therefore, a parallel approach using multiple threads should introduce a performance increase. Furthermore, each request from the client has a priority. A task with a higher priority will have a higher penalty than a task with lower priority if they are delayed by the same amount. Therefore, a good scheduling system needs to be used in order to minimize the score. 

Both of these issues involve managing the workload, so we therefore implemented a solution addressing both at the same time. We first implemented a scheduling system based on the O(1) scheduler formerly used in the Linux kernel. This is a fair scheduler which spreads out the processing resources on a variety of different requests at the same time, but works more on tasks with higher priority. However, it turns out that mean turnaround time, accounting for priority, is what matters for the score, so a fair scheduling system is not actually desirable. That is, completing tasks will decrease the number of tasks waiting for a response and thereby decrease the amount of score that accumulates. Therefore, we implemented an unfair scheduler which simply focuses all processing resources on one task at a time. The task focused on is the one with the smallest key, defined by `computation_remaining / priority`, where `computation_remaining` is the remaining number of unattempted inputs in the hash brute-forcing procedure. With this metric, we account for the estimated time a task will take to complete as well as the increased cost of delaying tasks with higher priority. The data structure for getting the appropriate task is implemented using a binary heap-based priority queue. It takes O(1) time to get the task with smallest key and O(log(n)) time to insert and remove a task. 

The scheduler is implemented as a thread pool. Each thread repeatedly works on a chunk associated with the task with the smallest key. Each thread executes the following three steps on repeat: 

  1. Get a task. Lock the scheduler and retrieve a chunk of the task with the smallest key.
  2. Work. Begin brute-forcing hashes based on the inputs in the chunk gotten in the previous step.
  3. Finalize. If the brute-forcing was successful, lock the scheduler, remove the task from the scheduler, notify other workers than they can drop the task, send the result to the client, and clean.

The number of threads used is double the number of processors in the system. The reasoning for this is to take advantage of simultaneous multithreading (e.g. Intel's hyper-threading) and to ensure that work is done while some threads are waiting (e.g. for I/O or for a lock).

**Setup**
The experiment code is contained in the git branches `scheduling` and `fair_scheduling`, and it is compared against the milestone version (in branch `milestone`). The relevant code is in `sched.c` (and the code for the binary heap is in `pqueue.c`). The client configuration is as noted above.

**Results**
The table showing the score for various executions of the versions being compared is displayed below. The score decrease is calculated relative to the milestone version and is calculated as the average score of the milestone version divided by the experiment's average score. 

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
We see that both scheduling versions yield an increase in performance, with the unfair scheduler being most performant. The execution environment had 16 cores, and since the computation is almost perfectly parallelizable, a performance increase of around 16 times was expected. We do indeed see a large performance increase. The score does not scale linearly with the speed-up, so the actual speed-up is difficult to discern based on these results. The unfair scheduling version performed best, so it was the version we continued with.

## Hash Table

**Motivation and Description**
We know that some requests may be repeated. Therefore, if we store information about previous requests, we can skip the computation of requests that have already been seen before. Motivated by these potential time savings, we implemented a lookup table using a hash table to store information about requests. The key to each item is simply the SHA-256 hash sent in the request. The hash table uses linear probing to deal with collisions. Its size is a power of two, meaning that we can use the faster bit-wise AND operation instead of modulo when computing a key's position in the table. Since we do not know how many requests will be sent, the hash table will variably change its capacity. When the fraction of occupied slots exceeds the load factor of 0.5, the hash table will double its capacity.

With the lookup table, when a request is received, the hash table is used to check if the request has been seen before. If we have a table miss, we place the request in the scheduler's queue as normal and add it to the hash table marked as being in progress. If we have a hit and the computation was previously completed, we immediately respond with the result to the client. If, instead, the result is not stored but is currently in progress, we can simply register the new client as also being interested in the pending result. When the computation is done, we send the result to all interested parties.

This addition means that the scheduler needs to be slightly modified to account for every task waiting for a result. The scheduling metric used is simply modified to `computation_remaining / sum(priority)`, where `sum(priority)` is the sum of the priorities of the requests waiting for the pending result.

**Setup**
The experiment is contained in git branch `lookup_table` and is compared to the version in the `scheduling` branch (the unfair scheduling version). The relevant code is in `htable.c` with some modifications in `sched.c`. The client configuration is as noted above.

**Results**
The table showing the score for various executions of the versions being compared is displayed below. The score decrease is calculated relative to the unfair scheduling version and is calculated as the average score of the unfair scheduling version divided by the experiment's average score. 

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
The OpenSSL implementation of SHA-256 hashing is generalized with no assumptions made about its usage. However, the usage of SHA-256 for the server is slightly specialized. We know that the input size is always 64 bits long, and we know that we are brute-forcing hashes. For the former, this means we can remove some of the overhead associated with digesting chunks of the input as well as have the some things associated to the input size compiled directly into the program binary. For the latter, this means we can parallelize using SIMD. And since we are only interested in the input and not the computed hash itself, it means that we can directly reverse the very last part of the hash received from the client and use this to compare against. This means individual hashing attempts do not need to complete the very last part of the hash, reducing individual hash computation times during brute-forcing. 

With these ideas, we first implemented a naive solution, solely based on the first ideas, i.e. no digest overhead and compiler optimization associated with the fixed input length. Afterwards, we implemented a solution based on SIMD (using GCC vector extensions), tested with several configuration: 4-way, 8-way, and 16-way. We also implemented a version using SIMD that did not use as many assignment operations. Finally, we implemented a version using Intel's x86 SHA instructions. However, these instructions are not present on all systems.

**Setup**
The experiment is contained in git branch `sha_implementation` and compared with the lookup table version (contained in branch `lookup_table`). The lookup table version is almost identical other than the fact that it uses OpenSSL for the SHA-256 implementation. The client configuration is as noted above.

Depending on preprocessor flags set, different SHA-256 implementations will be used. Defining `SB_VECTORIZE` will use the *x*-way SIMD implementation (set `VEC_SIZE` in `sha256.h` to choose *x*). The flag can be defined by compiling with `-DSB_VECTORIZE` when using GCC. This can be added in the `FLAGS` variable in the Makefile provided in the repository. Defining `SB_USE_SLOW_VECTORIZATION` as well as `SB_VECTORIZE` uses the *x*-way SIMD implementation that uses fewer assignment operations. Defining `SB_USE_x86_64_SHA_SIMD` uses Intel's x86 SHA hardware acceleration. The x86 SHA instructions may or may not be present which may lead to compilation failure. Defining `SB_USE_NAIVE_SHA` uses a naive implemention of SHA-256 based on simple serial code. Defining none of these flags uses the OpenSSL implementation. 

**Results**
The table showing the score for various executions of the versions being compared is displayed below. The score decrease is calculated relative to the lookup table version and is calculated as the average score of the lookup table version divided by the experiment's average score. 

| Seed     | Lookup table | Naive       | SIMD, 4-way | SIMD, 8-way | SIMD, 16-way | Few ass. SIMD, 8-way | x86 SHA instructions |
|----------|--------------|-------------|-------------|-------------|--------------|----------------------|----------------------|
| 1        | 271257       | 13862304    | 2772468     | 772398      | 1454051      | 828646               | 666927               |
| 2        | 179512       | 9224684     | 1337205     | 320269      | 588856       | 362476               | 285993               |
| 3        | 158510       | 9102744     | 1571096     | 335111      | 734934       | 320791               | 301479               |
| 4        | 142385       | 6954266     | 1051481     | 264858      | 466636       | 286707               | 229955               |
| 5        | 265912       | 12843628    | 2653683     | 706441      | 1295861      | 740375               | 483347               |
| Avg      | 203515.2     | 10397525.2  | 1877186.6   | 479815.4    | 908067.6     | 507799               | 393540.2             |
| Decrease | 1            | 0.02        | 0.11        | 0.42        | 0.22         | 0.40                 | 0.52                 |

**Conclusion**
Looking at the table, we see that none of the implemented SHA solutions performed better than the OpenSSL implementation. After some investigation, we saw that the OpenSSL's SHA-256 implementation for x86 uses Intel's SHA instructions that we had also used in one implementation. But it seemingly uses them in a smarter than us, since it is faster, despite not taking advantage of some of the other factors unique to our use case such as the fixed input size. Therefore, we can conclude that is faster to use the OpenSSL solution for the local execution environment. However, the DTU compute execution environment does not support Intel's x86 SHA instructions despite having an Intel CPU with an x86 architecture (likely due to the processor model being from 2012). Here, the fastest solution is the 4-way SIMD implementation. Also, unlike the local execution environment, in DTU's execution environment, the 8-way SIMD implementation is slower than the 4-way implementation, likely due to it not supporting wide enough SIMD instructions.
