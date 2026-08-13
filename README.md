<h1 align="center">Shine</h1>

<p align="center">
  <img width="256" height="256" alt="logo" src="https://github.com/user-attachments/assets/4f123d95-977b-4b3d-b247-31ae4178d8af" />
</p>

<p align="center">
  <strong>A low level language that compiles straight to machine code, no runtime, no VM.</strong>
</p>

---

A small compiled language with its own lexer, parser, and codegen, targeting LLVM IR.

Shine compiles down to native object files and links straight to an executable. No interpreter, no VM, no runtime.

Status: Active Development - compiles functions with parameters, calls, and string/int literals.

---

## License

Compiler: GPL-3.0
Standard Library: GPL-3.0 with runtime library exception

This means programs written in Shine and linked against the stdlib 
are NOT required to be GPL-licensed.

See [STDLIB-LICENSE](STDLIB-LICENSE)

---

## Features

**Native compilation** - Shine source goes through a hand-written lexer, recursive-descent parser, and LLVM-based codegen, then straight to an object file and a linked executable.

**Functions with parameters** - `fn int add(a: int, b: int) { ... }`. Parameters are passed as real LLVM arguments and referenced by name in the function body.

**No runtime** - Shine programs link against libc and nothing else. There's no garbage collector, no interpreter loop, no bytecode.

---

## Language Basics

- `fn type name(param: type, ...) { ... }`
- `int` / `void` types
- int and string literals
- function calls, including passing arguments
- `r/` (return)
- `write(string)` - a compiler builtin for now, lowers to `puts`
- `let` is an immutable variable and `var` is a mutable one
- Assignment to variables (`var`)
- `+ - * /` and comparisons

No control flow yet - see `ROADMAP.md`.

```
fn int identity(x: int) {
    r/x;
}

fn int main() {
    let(int) base = 10;
    var(int) total = identity(base);
    total = total + 5;
    write("Hello, World!");
    r/total >= 15;
}
```

---

## Using The Compiler

To use the compiler you just need to run this command (replace the names with yours):

`shinec yourfile.shine -o output.exe`

After that your Shine code has been compiled and is ready to run!

---

## Building from Source (Windows, MSYS2/MinGW)

Clone The Repository
```
git clone https://github.com/Shine-Language/Shine-Compiler.git
cd Shine-Compiler
```

Build It (May need tweaking if you are not using Windows)
```
cmake -B build -G Ninja
cmake --build build
./build/tests/shine_tests.exe
./build/shinec.exe examples/hello.shine -o hello.exe
./hello.exe
```

**NOTE:**

You must be in MSYS2 MINGW64 for this to work, every command must be ran from there and no where else.

---

## Layout

```
include/shine/   headers
src/             lexer, parser, codegen, driver (main.cpp)
tests/           unit tests
examples/        sample .shine programs
```

---

## Roadmap

See [ROADMAP.md](ROADMAP.md) for what's done and what's next.
