# CuckyDOOM
Literally the worst DOOM Source Port you'll ever see

Seriously, don't use this.

It's unstable, has no sound, and uses SDL2 for the window and event response, but old ass X11 code for everything else.

I threw this together in a few hours because I thought it'd be funny.

## Dependencies

* SDL2
* pkg-config (for builds that require static-linkage)

## Building

This project uses CMake, allowing it to be built with a range of compilers.

Switch to the terminal and `cd` into this folder.
After that, generate the files for your build system with:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

MSYS2 users should append `-G"MSYS Makefiles" -DPKG_CONFIG_STATIC_LIBS=ON` to this command, also.

You can also add the following flags:

Name | Function
--------|--------
`-DLTO=ON` | Enable link-time optimisation
`-DPKG_CONFIG_STATIC_LIBS=ON` | On platforms with pkg-config, static-link the dependencies (good for Windows builds, so you don't need to bundle DLL files)
`-DMSVC_LINK_STATIC_RUNTIME=ON` | Link the static MSVC runtime library, to reduce the number of required DLL files (Visual Studio only)

You can pass your own compiler flags with `-DCMAKE_C_FLAGS` and `-DCMAKE_CXX_FLAGS`.

You can then compile the executable with this command:

```
cmake --build build --config Release
```
