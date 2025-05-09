# MetaCall PLT/IAT Hooking Proof of Concept
Proof of Concept for implementing PLT/IAT hooking for MetaCall.
This will be used in order to allow MetaCall to be loaded in places where a runtime is already started.

The feature once implemented will solve the following isuses:
  - https://github.com/metacall/core/issues/231
  - https://github.com/metacall/core/issues/460
  - https://github.com/metacall/core/issues/493
  - https://github.com/metacall/core/issues/31

## How it works

This PoC is based on a modified version of [PLTHook Library](https://github.com/metacall/plthook) from [@kubo](https://github.com/kubo).

First of all we have the following preconditions:
 - `libmetacall` which loads `libnode_loader`.
 - `libnode_loader` is not linked to anything but we are going to weakly link it to `libnode`, this means that in Windows it must be linked with `/DELAYLOAD`, in Linux and MacOS it must not be linked.

There are two possible cases, this happens before loading libnode_loader:
  - MetaCall is not being executed by `node.exe`, then:
    1) Windows:
        - `libmetacall` loads dynamically all the dependencies of `libnode_loader` (aka `libnode`).
        - We list all the symbols of each dependency (aka `libnode`) so we construct a hashmap of symbol string to symbol function pointer.
        - We list all the unresolved symbols of `libnode_loader` and we link them to `libnode`.

    2) MacOS & Linux:
        - `libmetacall` loads dynamically all the dependencies of `libnode_loader` (aka `libnode`).
        - Linking is resolved by the linker automatically.

  - MetaCall is being executed by node.exe, then we have two possible cases:
    1) `node.exe` compiled statically (without `libnode`):
        - We get all the library dependencies from `node.exe` and we do not find `libnode`, so we get the handle of the currrent process.
        - We list all symbols of `node.exe` and we construct a hash map a hashmap of symbol string to symbol function pointer.
        - We list all the unresolved symbols of `libnode_loader` and we link them to `node.exe`.

    2) `node.exe` compiled dynamically (with `libnode`):
        - We get all the library dependencies from `node.exe` and we find `libnode` so we get the handle from it.
        - We list all the symbols of each dependency (aka `libnode`) so we construct a hashmap of symbol string to symbol function pointer of those dependencies (`libnode`).
        - We list all the unresolved symbols of `libnode_loader` and we link them to `libnode` of `node.exe`.

## Outcome

With this methodology we prevent loading a library that contains a runtime. This is very dangerous because numerous runtimes rely on constructors (C++ constructors of static class delacarations or C compiler dependant constructor mechanisms like GNU or Clang `__attribute__((constructor))`) that are mutually exclusive between them. So if we only load the library but we do not call method of the library, it can still cause errors.
The loaders will be redirected to the proper runtime, reusing the functions and instance of the already running runtime.

## Features

 - Works for Linux, Windows and MacOS with most of the architectures of each platform: https://github.com/metacall/plthook?tab=readme-ov-file#supported-platforms
 - Hooks the functions and prevents runtime instances to be initialized, so it's fully transparent and has no side effects on the runtimes.

## Limitations

 - Currently it does not support `-O3` on Linux with GCC compiler, neither `/O2` and `/Ob2` in Windows with MSVC. It works in MacOS with `-O3` and Clang.
