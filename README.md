# pacman-win

pacman for Windows.

## Build

This project uses CMake because it has multiple C++ executables, header-only
dependencies, and needs to remain portable across Windows and Linux toolchains.

```sh
cmake --preset debug
cmake --build --preset debug
```

The built binaries are placed under `build/debug/`.

To cross-compile Windows binaries with MinGW-w64:

```sh
cmake --preset win64-release
cmake --build --preset win64-release
```

The Windows binaries are placed under `build/win64-release/`. Use
`win32-release` instead if you need 32-bit Windows binaries.
