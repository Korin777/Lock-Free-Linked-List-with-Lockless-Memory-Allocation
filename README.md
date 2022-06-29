# Script
* `run_ll.sh`,`create_plots_ll.sh` : Get linked list throughput and scability , then plot it
* `run_ll2.sh`,`create_plots_ll2.sh` : Get linked list throughput and scability with glibc memory allocaotr and my memory allocator repectively , then plot it to compare them


# Note
1. Change `HP_MAX_THREADS` in ``include/hp.h` if number of thread more than default(30)
2. Operate linked list with insert and delete or push and pop , don't mix them , it can modified from `test` function in `src/main.c`
3. insert and delete don't support memory reclamation
4. use `-DMY_MALLOC` to use my memory allocator
5. Eliminate factors that interfere with performance analysis via `shell.sh`
6. [Lock-Free Linked List with Lockless Memory Allocation](https://hackmd.io/@Korin777/linux2022-final)

# Reference
Non-blocking singly-linked list originally implemented from [gist](https://gist.github.com/jserv/1532f87510ba75204edcfecd5efafa83)

Lock-free linkedlist implementation based on [Lock-Free Linked Lists and Skip Lists](http://www.cse.yorku.ca/~ruppert/papers/lfll.pdf)

Test program modified from [concurrent-ll](https://github.com/sysprog21/concurrent-ll)

Hazard Pointer modified from [hp_list](https://github.com/sysprog21/concurrent-programs/tree/master/hp_list)

[linux2022-quiz11](https://hackmd.io/@sysprog/linux2022-quiz11#%E6%B8%AC%E9%A9%97-2)

[linux2022-quiz13](https://hackmd.io/@sysprog/linux2022-quiz13#%E6%B8%AC%E9%A9%97-1)