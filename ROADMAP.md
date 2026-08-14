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
- [x] Add user inputs
    - Would be written as `user_input("TEXT")`
    - Prints the prompt, reads an int from stdin (via scanf), returns it
- [x] Allow `write()` to support non string literals
    - Any int-valued expression can now be passed to `write()`

## v0.5.0 - Error Handling
- [x] Better default error messages for compiler
- [x] General improvements to error handling

## v0.6.0 - real types
- [x] Type hierarchy instead of string-named TypeRef
- [x] Pointers, fixed-width ints
- [x] Type-checking pass (codegen currently does no validation of its own)

## v0.7.0 - build/driver improvements
- [x] `-32` flag for 32-bit compilation
- [x] `-c` flag (compile only, skip linking)
- [x] Success/failure messages colored (green/red) in the driver output

## v0.8.0 - structs
- [x] `struct Name { field: type, ... }` declarations
- [x] Field access (`x.field`) and field assignment (`x.field = expr;`)
- [x] Struct values as function params/returns
- [x] Struct literal / initialization syntax
- [x] return/0; is also valid like r/0;

## v0.9.0 - arrays
- [ ] Fixed-size arrays
- [ ] Indexing (`arr[i]`), with a decided and documented bounds-check behavior
- [ ] Array/pointer decay interaction
- [ ] Simple slices (ptr + length) if time allows, otherwise deferred past v1.0.0

## v0.10.0 - standard library
- [ ] Minimal extern mechanism, just enough to call into libc (printf, scanf, malloc)
- [ ] Move `write`/`user_input`/`terminal.pause` out of the compiler and into Shine-source stdlib functions
- [ ] Basic `String`/`Buffer`-style type built on structs + arrays

## v0.11.0 - error handling
- [ ] Result/error-union style return values
- [ ] Propagation syntax, or at minimum pattern-matching on Result

## v0.12.0 - multi-file modules
- [ ] `import`/module resolution
- [ ] Symbol visibility (pub/private)
- [ ] Multi-translation-unit linking in the driver

## v1.0.0 - out of beta
- [ ] Syntax freeze, written language spec/reference (not just README examples)
- [ ] Full test coverage pass, basic parser/lexer fuzzing
- [ ] Documented versioning/back-compat policy going forward
- [ ] README status line updated to reflect 1.0, not active-development beta

## Later (Will be added in the future, post-v1.0.0)
- [ ] Full C FFI (extern function declarations, calling into arbitrary native libs)
- [ ] `defer` statements for guaranteed cleanup
- [ ] Function pointers and callbacks
- [ ] Closures with explicit capture (allocator-controlled, no GC)
- [ ] Allocator story in the stdlib (arena, pool, bump allocators)
- [ ] Package manager + package ecosystem (Cargo/pip-style)
- [ ] Ability to generate .dll's
- [ ] UI Tool
- [ ] Freestanding codegen mode (no libc, no CRT startup, custom entry point/linker script)

## TBD (May or may not be added in the future)

- [ ] Simple interpreter