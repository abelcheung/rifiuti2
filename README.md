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

## Special notes

### (Upcoming) 0.8.0

- Windows binaries will be published via MSYS2 GitHub workflow.
- Package maintainers would need to rewrite their package files,
  in light of undergoing document restructure
  [#22](https://github.com/abelcheung/rifiuti2/issues/22)
  and CMake migration
  [#21](https://github.com/abelcheung/rifiuti2/issues/21).

### 0.7.0

Systems supporting UTF-8 encoding is mandatory, except on Windows
Command Prompt. File output in Windows is also in UTF-8.

- UTF-8 locale is pretty much standard for Linux and macOS these years.
- On Windows front, there are already many featureful text editors
  capable of opening UTF-8 unicode text files.
- As a result, `-8` option is obsolete and no more affects output in any way.


## Usage

`rifiuti2` is designed to be portable (just download and use without
need for installation), and runs on command line environment.
Although utilities provide `-h` option for brief help message,
it is suggested to [consult Wiki page][wiki1] for
full detail on all of the options; following are a few examples
on how to use them:

[wiki1]: https://github.com/abelcheung/rifiuti2/wiki/Usage-and-Examples

- `rifiuti-vista.exe -x -z -o result.xml \case\S-1-2-3\`
> Scan for index files under `\case\S-1-2-3\`, adjust all deletion time
> for local time zone, and write XML output to `result.xml`
- `rifiuti -l CP932 -t "\n" INFO2`
> Assume INFO2 file is generated from Japanese Windows (codepage 932),
> and display each field line by line, instead of separated by tab


## Download

### Supported platforms

`rifiuti2` is guaranteed usable on Windows, Linux and FreeBSD,
with success reports for MacOS (using `brew`). Some testing on
big endian platforms are done with Qemu emulator.
More compatibility fix for other architectures welcome.

### Windows
Windows binaries are officially provided
[on Github release page][rel].

[rel]: https://github.com/abelcheung/rifiuti2/releases/

For various technical reasons, users of very ancient Windows
platforms may not be able to use latest `rifiuti2` on their OS.
For following versions, get their corresponding `rifiuti2`
version instead of latest one:

Windows        |  Latest supported version
:------------------- | :--------------------------
XP or Server 2003    | [0.6.1][rel_061]
Vista or Server 2008 | [0.7.0][rel_070]
7 or above           | [all][rel_latest]

[rel_061]: https://github.com/abelcheung/rifiuti2/releases/tag/0.6.1
[rel_070]: https://github.com/abelcheung/rifiuti2/releases/tag/0.7.0
[rel_latest]: https://github.com/abelcheung/rifiuti2/releases/latest

### Linux (DEB)

DEB packages available officially on [Debian][deb] and [Ubuntu][ub],
hence also available on derivatives focusing on information security:

  - [Kali Linux][kali]
  - BackBox Linux

[deb]: https://packages.debian.org/search?keywords=rifiuti2
[ub]: http://packages.ubuntu.com/search?keywords=rifiuti2
[kali]: https://pkg.kali.org/pkg/rifiuti2

### Linux (RPM)

[Linux Forensics Tools Repository (LiFTeR)][lifter] is the earliest
to provide 3rd party RPM package, which can be used on Fedora
and Red Hat Enterprise Linux. Later Fedora incorporated `rifiuti2`
[since Fedora 29][fedora], and the same goes for Extra Package
Repository for Enterprise (`epel8`). These packages can be used on
corresponding derivative distro as well, such as Rocky Linux and
now-defunct CentOS.

On non-Redhat lineage, ALT Linux also provides `rifiuti2`
[since version 10][alt].

[lifter]: https://forensics.cert.org/ByPackage/rifiuti2.html
[fedora]: https://packages.fedoraproject.org/pkgs/rifiuti2/rifiuti2/
[alt]: https://altlinux.pkgs.org/p10/autoimports-x86_64/rifiuti2-0.7.0-alt2_5.x86_64.rpm.html

### Linux (Other)

- [ArchStrike (formerly ArchAssault)][strike], a penetration testing
  derivative, has it packaged since late 2014
- [AUR (Arch User Repository)][aur] incorporated this utility on 2019,
  thus should be available to all other Arch derivatives
- [BlackArchLinux][blarch], another pen test distro, includes `rifiuti2`
  into default distribution

There can be other distro missing mentions.

[strike]: https://archstrike.org/packages/rifiuti2
[blarch]: https://www.blackarch.org/forensic.html
[aur]: https://aur.archlinux.org/packages/rifiuti2

### FreeBSD

Official [FreeBSD port][fbsd] is available since 8.4.

[fbsd]: https://www.freebsd.org/cgi/ports.cgi?query=rifiuti2

### Others
For OS where `rifiuti2` is not readily available, it is always
possible to [compile from source][wiki2].

[wiki2]: https://github.com/abelcheung/rifiuti2/wiki/Compile-From-Source
