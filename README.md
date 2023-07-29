# LexiDB

An in-memory data structure database

## Getting started

Note: you must be running Linux on a 64 bit architecture

-- This project is still very much so a WIP --

### Requirements

cmake >= 3.10
gcc

#### building

```console
git clone https://github.com/boreddad/lexidb
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

