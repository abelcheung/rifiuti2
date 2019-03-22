## Compile requirements

Building current git source can be done for Unix/Linux:

  * `glib` ≥ 2.28 (deveopment headers and libraries, available since 2010)
    * For ancient platforms, only 0.6.1 source is buildable, which
      requires `glib` 2.16.
  * `pkg-config` (or its replacement on \*BSD, `pkgconf`)
  * basic autotools-based development toolchain
    * C compiler &mdash; both `gcc` and `clang` are tested
    * make
    * automake ≥ 1.11
    * autoconf

A few extra programs are also checked (`iconv`, `xmllint`), but they are
for testsuite only and not necessary for building software.

## Compile Procedure

### Compile on Linux / BSD / Cygwin

First execute `autogen.sh` to generate autotools files necessary for compilation:
```
$ ./autogen.sh
```

After that, everything follows the usual autotools procedure:
```
$ ./configure && make check
```

*Optional*: install software with administrator privilege:
```
# make install
```

`rifiuti2` can be used even without installing, only that output messages
would not be translated; functionality would not change at all.

If compile or `make check` fails, please [report problem on Github page][3],
describing your OS and compile environment, and attach relevant testsuite
log file(s).

### Compile on MinGW / MSYS

`rifiuti2` has only been thoroughly tested on [MSYS2][1], a Unix-like
environment based on [mingw-w64][2], akin to the [old MSYS][5] which was
32-bit only. There are other similar distributions like [win-build][4];
your mileage may vary.

Follow the following procedure to setup MSYS2 necessary for building `rifiuti2`:

1. Grab installer from [MSYS2 page][1], and follow all instructions on that page.
In particular, please **tread carefully** when using `pacman` for upgrading
packages; read [section III of detailed installation page][6] carefully.
It should be relatively easier to handle this part with recent MSYS2 though.

2. Please refer to [MSYS2 introduction][7] for installing basic development
environment; for 64-bit build target, extra devel packages below are needed.
For 32-bit builds, replace all instances of `x86_64` with `i686`.
    * `mingw-w64-x86_64-gcc` and/or `mingw-w64-x86_64-clang`
    * `automake`
    * `autoconf`
    * `mingw-w64-x86_64-glib2`

3. Follow usual autotools procedure to run `configure` script and make.
   However, invoke `make` as `mingw32-make` instead, like:
   ```
   ./configure MAKE=mingw32-make CC=clang
   mingw32-make check
   ```

4. If one intends to run `rifiuti2` without MSYS dependency (as a standalone
binary, that is), either:
    * Compile program statically with `./configure --enable-static`.
    * Compile program dynamically (without options), and then copy all
    necessary libraries under either `/mingw64/bin` or `/mingw32/bin` to
    the same folder `rifiuti` binaries reside in.

    *Hint*: Use `ldd rifiuti.exe` to discover external libraries `rifiuti2`
    is linked against.

**Note 1**:
0.6.1 Windows binary distributed on Github are statically linked to
glib version 2.44, which was available in MSYS2 circa May 2015.

**Note 2**:
It is noted that the once-stagnant old MinGW project is moving again. It
*might* be able to compile `rifiuti2`, but no effort will be spent to
make sure this is the case.

### Compile on macOS
(To be filled)

[1]: https://msys2.github.io/
[2]: http://mingw-w64.yaxm.org/doku.php
[3]: https://github.com/abelcheung/rifiuti2/issues
[4]: https://mingw-w64.org/doku.php/download/win-builds
[5]: http://www.mingw.org/wiki/msys
[6]: https://github.com/msys2/msys2/wiki/MSYS2-installation
[7]: https://github.com/msys2/msys2/wiki/MSYS2-introduction
