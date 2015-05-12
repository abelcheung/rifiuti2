## Compile requirements

Bootstraping process can be done for Unix/Linux like OS released since 2010.

  * `glib` ≥ 2.16 (deveopment headers and libraries, available since 2008)
  * `pkg-config` (or its replacement on *BSD, `pkgconf`)
  * basic autotools-based development toolchain
    * C compiler &mdash; both `gcc` and `clang` are tested
    * make
    * automake ≥ 1.11
    * autoconf

A few extra programs are also checked (`perl`, `iconv`, `xmllint`), but they are 
for testsuite only and not necessary for building software.

## Compile procedure on Linux / BSD

First execute `autogen.sh` to generate autotools files necessary for building software:
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

`rifiuti2` can be used even without installing, only that there would be no
translations in messages and result.

If compile or `make check` fails, please report problem to Github page, describing your OS
and compile environment, and attach `test/testsuite.log` on Gist.

## Compile procedure on MinGW / MSYS

`rifiuti2` has only been thoroughly tested on [MSYS2][1], a Unix-like
environment based on [mingw-w64][2], akin to the old [MSYS 1.0][5] which was mingw32 based.
There are other mingw-w64 based distributions like [Cygwin][3] and [win-build][4],
your mileage may vary.

Follow the following procedure to setup MSYS2 necessary for building `rifiuti2`:

1. Grab installer from [MSYS2 page][1], and follow all instructions on that page.
In particular, please **tread carefully** when starting to use `pacman` for upgrading
packages. [Some pitfalls may result from careless update.][6] (Read section III of
this page carefully)

2. After basic setup is done, grab Glib runtime and development packages
  - Install with `pacman -Sy mingw-w64-x86_64-glib2` for 64bit build, and/or
  - Install with `pacman -Sy mingw-w64-i686-glib2` for 32bit build

3. If one intend to run `rifiuti2` outside of MSYS bash environment
(such as under Windows `cmd`), either:
  1. Link program statically with `make LDFLAGS="-static"`. In this case
  `/mingw{64,32}/lib/pkgconfig/glib-2.0.pc` need to be manually edited to
  append `-liconv` to `Libs.private` line.
  2. Copy all necessary libraries under `/mingw64/bin` or `/mingw32/bin` to
  the same folder `rifiuti` binaries reside in

**Note 1**:
Windows binary distributed on Github are statically linked to existing
version of glib available in MSYS2 &mdash; 2.44 as of May 2015.

**Note 2**:
MSYS 1.0 might still be able to compile `rifiuti2`, but no effort will be
made to ensure it's working at all.

[1]: https://msys2.github.io/
[2]: http://mingw-w64.yaxm.org/doku.php
[3]: https://cygwin.com/
[4]: http://win-builds.org/doku.php
[5]: http://www.mingw.org/wiki/msys
[6]: https://sourceforge.net/p/msys2/wiki/MSYS2%20installation/
