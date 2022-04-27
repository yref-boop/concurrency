# concurrency
this repository stores 3 exercises that implement concurrency in some way
### p1 - accounts
(C) we are given a code that implements several "transactions" in a bank, we can use a different number of threads for different processes, we are asked to:
 - 1: create different mutexes that protect each account
 - 2: add a new type of thread tha performs transfers betwen two random accounts
 - 3: add a thread that sums the balances of the accounts and prints the total reapeatedly
 - 4: change the number of iterations to define the number of threads in each kind of operation

### p2 - break password (c)
(C) we are given a code that breaks a hash by brute force, we are asked to:
 - 1: add a progress bar 
 - 2: add an estimation of the number of passwords checked each second
 - 3: make the program multithreadded
 - 4: implement the ability to break several passwords at once

### p3 - break password (erl)
(Erlang): this practical is conceptually identical to the previous one, we are asked to:
 - 1: break several hashes at the same time
 - 2: print the number of password checks per second
 - 3: break the hashes using several processes
