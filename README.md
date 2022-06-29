# concurrency
this repository stores 3 exercises that implement concurrency in some way
### p1 - accounts
(C) we are given a code that implements several "transactions" in a bank 
we can use a different number of threads for different processes & are asked to:
 - create different mutexes that protect each account
 - add a new type of thread tha performs transfers betwen two random accounts
 - add a thread that sums the balances of the accounts and prints the total reapeatedly
 - change the number of iterations to define the number of threads in each kind of operation

### p2 - break password (c)
(C) we are given a code that breaks a hash by brute force, we are asked to:
 - add a progress bar 
 - add an estimation of the number of passwords checked each second
 - make the program multithreadded
 - implement the ability to break several passwords at once

### p3 - break password (erl)
(Erlang): this practical is conceptually identical to the previous one, we are asked to:
 - break several hashes at the same time
 - print the number of password checks per second
 - break the hashes using several processes
