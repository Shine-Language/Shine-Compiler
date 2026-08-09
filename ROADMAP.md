# Roadmap

## v0.1.0 - first steps
- [x] Lexer, parser, codegen for fn/return/call/literals
- [x] `write` builtin via `puts`
- [x] Object file emission + link, builds hello.shine
- [x] Function parameters, identifier expressions, per-function argument passing

## v0.2.0 - variables and operators
- [x] `let(type) name = expr;`
- [x] `var(type) name = expr;`
- [x] Assignment to `var`
- [x] `+ - * /`, comparisons

## v0.3.0 - control flow
- [x] if/else, while, for, etc.
    - while / for will be combined to `loop(condition) {}`
- [x] break (written as stop) and continue (written as cont) loop controls

## v0.4.0 - inputs & non string literals
- [ ] Add user inputs
    - Would be written as `user_input("TEXT")`
    - Prints the prompt, reads an int from stdin (via scanf), returns it
- [ ] Allow `write()` to support non string literals
    - Any int-valued expression can now be passed to `write()`

## v0.5.0 - Error Handling
- [ ] General improvements to error handling
- [ ] Better default error messages

## v0.6.0 - real types
- [ ] Type hierarchy instead of string-named TypeRef
- [ ] Pointers, fixed-width ints
- [ ] Type-checking pass (codegen currently does no validation of its own)

## Later
- [ ] Structs, arrays
- [ ] Standard library (move write out of the compiler)
- [ ] Multi-file modules
- [ ] C FFI (extern function declarations, calling into existing native
      libs).
- [ ] Package manager + package ecosystem (Cargo/pip-style)
- [ ] Ability to generate .dll's
- [ ] UI Tool
- [ ] Freestanding codegen mode (no libc, no CRT startup, custom entry
      point/linker script)
