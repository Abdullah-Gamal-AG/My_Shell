# my_shell

A small educational Unix-like shell written in C++.

**Prerequisites**
- `g++` (supports C++17)

**Build**
Run from the project root:

```bash
g++ -std=c++17 main.cpp -o my_shell
```

**Run**

```bash
./my_shell
```

Type commands as you would in a normal shell. Background a process using `&` at the end of a command.

**Key files**
- [main.cpp](main.cpp) — program entry, readline loop, and signal setup.
- [executor.h](executor.h) — command execution logic (pipes, redirection, forking).
- [tokenizer.h](tokenizer.h) — tokenization logic.
- [models.h](models.h) — AST node definitions and token enums.

**Notes**
- `main.cpp` ignores `SIGINT`/`SIGQUIT` in the parent; child processes reset these to default so Ctrl-C affects running commands.
- `executor.h` contains inline implementations; build by compiling `main.cpp` as shown above.

**Contributing**
- Open an issue or submit a PR with improvements or bug fixes.
