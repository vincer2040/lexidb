# LexiDB

An in-memory data structure database

## Getting started

Note: you must be running Linux on a 64 bit architecture

### Requirements

1. cmake >= 3.10

2. gcc

#### building

```console
git clone git@github.com:boreddad/lexidb.git
cd lexidb
mkdir build && cd build
cmake ..
make
```

#### running the server

```console
./lexidb
```


#### running the cli

in a new terminal

```console
./lexi-cli
```

#### run some commands from the terminal

```console
./lexi-cli

lexi> set foo bar
ok
lexi> get foo
"bar"
lexi> keys
"foo"
lexi>set baz :500
ok
lexi>get baz
(int) 500
lexi> set foo "bar baz"
ok
lexi> get foo
"bar baz"
```

