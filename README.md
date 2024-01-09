# LexiDB

An in-memory data structure database

Note: This project is for currently for educational purposes only. If you are looking
to use something similar in a production environment, you should use [redis](https://github.com/redis/redis).

## Getting started

### Requirements

1. cmake >= 3.10

2. gcc

#### building

```bash
git clone git@github.com:vincer2040/lexidb.git
cd lexidb
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

from withing the build durectory, optionally run tests:

```bash
make test
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
lexi> set baz 500
ok
lexi> get baz
500
lexi> set foo "bar baz"
ok
lexi> get foo
"bar baz"
```

### Client libraries

currently, there are client implentations in:

1. C - Included in this repo (hilexi.c)

2. [nodejs](https://github.com/vincer2040/lexi-ts)

3. [C++](https://github.com/vincer2040/lexi-cpp)

4. [rust](https://github.com/vincer2040/lexi-rs)

See those repos for for more information

### Configuring the server

change the default address

```bash
./lexidb --address <address>
```

change the default port:

```bash
./lexidb --port <port>
```

change the loglevel

```bash
./lexidb --loglevel info | debug | verbose
```

change the path of lexi.conf configuration file

```bash
./lexidb --config <path to config>
```

info - only logs when the server starts, connections are established, and connections are closed

debug - logs info, commands, and other debugging information

verbose - logs everything

### users

add users for the database by editing the lexi.conf file

see lexi.conf for more details

