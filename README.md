# LexiDB

An in-memory data structure database

Note: This project is for currently for educational purposes only. If you are looking 
to use something similar in a production environment, you should use [redis](https://github.com/redis/redis). 

## Getting started

### Requirements

1. a 64 bit machine running a linux distrobution that supports `epoll`

2. cmake >= 3.10

3. gcc

#### building

```bash
git clone git@github.com:vincer2040/lexidb.git
cd lexidb
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

#### running the server

```bash
./lexidb
```


#### running the cli

in a new terminal

```bash
./lexi-cli
```

#### run some commands

```bash
./lexi-cli
lexi> set foo bar
ok
lexi> get foo
"bar"
lexi> keys
"foo"
lexi> set baz :500
ok
lexi> get baz
(int) 500
lexi> set foo "bar baz"
ok
lexi> get foo
"bar baz"
lexi> push bar
ok
lexi> push foo
ok
lexi> pop
"foo"
lexi> pop
"bar"
```
