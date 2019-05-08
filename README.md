| Appveyor | Travis |
|----------|--------|
| [![Appveyor status](https://ci.appveyor.com/api/projects/status/github/abelcheung/rifiuti2?svg=true&branch=master)](https://ci.appveyor.com/project/abelcheung/rifiuti2) | [![Travis status](https://travis-ci.org/abelcheung/rifiuti2.svg?branch=master)](https://travis-ci.org/abelcheung/rifiuti2) |

## Introduction

`Rifiuti2` is a for analyzing Windows Recycle Bin INFO2 file. Analysis of
Windows Recycle Bin is usually carried out during Windows computer
forensics. `Rifiuti2` can extract file deletion time, original path
and size of deleted files and whether the trashed files have been
permanently removed.

For those interested in what it does, and what functionality it
provides, please [check out official site][1] for more info.
Latest features and changes can be found in [NEWS file](NEWS.md).

[1]: https://abelcheung.github.io/rifiuti2

## Special note for 0.7.0
* Windows binaries will be automatically built from
  [Appveyor](https://www.appveyor.com/) and published to Github.
* **Systems supporting UTF-8 encoding is mandatory, except on Windows console
  (file output is also in UTF-8).** This shouldn't be problematic though,
  as UTF-8 locale is pretty much standard for Linux and macOS these years.
  On Windows front, there are already many featureful text editors
  capable of opening UTF-8 unicode text files.
* As a result, `-8` option is obsolete and no more affects output in any way.


## Usage

`rifiuti2` is designed to be portable, and runs on command line environment.
Depending on relevant Windows recycle bin format, there are 2 binaries to
choose from (most users would want first one):

Program        | Recycle bin from OS | Purpose
---------------|---------------------|--------
`rifiuti-vista`|Vista &ndash; Win10  | Scans `\$Recycle.bin` style folder
`rifiuti`      |Win95 &ndash; XP/2003| Reads `INFO` or `INFO2` file in `\RECYCLED` or `\RECYCLER` folder

Run programs without any option for more detail. Here are some more
frequently used options:

 Option    | Purpose
:----------|:--------
`-o <FILE>`| Output to file
`-x`       | Output XML instead of tab-separated fields
`-l <CP>`  | Display legacy (8.3) filenames and specify its codepage

Please consult manpage (Unix) or README.html (bundled with Windows binaries)
for complete options and detailed usage description.

### Examples

* `rifiuti-vista.exe -x -z -o result.xml \case\S-1-2-3\`
> Scan for index files under `\case\S-1-2-3\`, adjust all deletion time
> for local time zone, and write XML output to `result.xml`
* `rifiuti -l CP932 -t "\n" INFO2`
> Assume INFO2 file is generated from Japanese Windows (codepage 932),
> and display each field line by line, instead of separated by tab

## Supported platform

It has been tested on Linux, Windows 7 and FreeBSD.
Some testing on big endian platforms are done with Qemu emulator.
More compatibility fix for other architectures welcome.

## Download

### Windows
Windows binaries are officially provided
[on Github release page][6].

Note that 0.6.1 version is the last version that can run on
Windows XP and 2003; upcoming versions would require Vista or above.

### Linux
* DEB packages available officially on [Debian][7] and [Ubuntu][8],
hence also available on most (if not all) derivatives focusing on
security and forensics, such as (this is incomplete list):
  * [Kali Linux][9]
  * [Deft X Virtual Appliance][10]
  * BackBox Linux
* RPM packages from [Linux Forensics Tools Repository (LiFTeR)][11]
  can be used on Fedora, and very likely CentOS and RHEL.
* [ArchStrike (formerly ArchAssault)][12], a penetration testing
  derivative of Arch Linux, has `rifiuti2` packaged since late 2014.

### FreeBSD
Official [FreeBSD port][13] is available since 8.4.

### Others (Compile from source)
For OS where `rifiuti2` is not readily available, it is always
possible to compile from source.

`rifiuti2` follows the usual `autotools` based procedure:
```sh
./configure && make check && make install
```
Please [refer to wiki page][14] for more detail.

## License

`rifiuti2` is released under BSD license. Please refer to
[license file](docs/LICENSE.md) for more detail.

[6]: https://github.com/abelcheung/rifiuti2/releases
[7]: https://packages.debian.org/search?keywords=rifiuti2
[8]: http://packages.ubuntu.com/search?keywords=rifiuti2
[9]: https://pkg.kali.org/pkg/rifiuti2
[10]: http://www.deftlinux.net/package-list/deft-x-va/
[11]: https://forensics.cert.org/ByPackage/rifiuti2.html
[12]: https://archstrike.org/packages/rifiuti2
[13]: https://www.freebsd.org/cgi/ports.cgi?query=rifiuti2
[14]: https://github.com/abelcheung/rifiuti2/wiki/Compile-From-Source