## Introduction

`Rifiuti2` is a for analyzing Windows Recycle Bin INFO2 file. Analysis of
Windows Recycle Bin is usually carried out during Windows computer
forensics. `Rifiuti2` can extract file deletion time, original path
and size of deleted files and whether the trashed files have been
permanently removed.

For those interested in what it does, and what functionality it
provides, please [check out official site][site] for more info.

[site]: https://abelcheung.github.io/rifiuti2

## Special notes

Latest features and changes can be found in [NEWS file](NEWS.md).

### 0.8.2

- Bug fix release for a few less common architectures.
- Enable GitHub artifact attestation.

### 0.8.1

JSON output format, WSL v2 support, and improve robustness
when reading broken data.

### 0.8.0

- Windows binaries will be published via [MSYS2 GitHub workflow](https://github.com/msys2/setup-msys2).
- Package maintainers would need to rewrite their package files,
  in light of multiple renovations: [CMake migration](https://github.com/abelcheung/rifiuti2/issues/21), [gettext removal](https://github.com/abelcheung/rifiuti2/issues/18), document restructuring etc.


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
[on Github release page][rel]. Some info for ancient Windows
version are [available on wiki][wiki_pkg_win].

[rel]: https://github.com/abelcheung/rifiuti2/releases/

[wiki_pkg_win]: https://github.com/abelcheung/rifiuti2/wiki/Packages#packages-for-windows

### Unix packages

Most Linux and FreeBSD users can use pre-packaged software for
convenience. Check out [the status here][wiki_pkg].

[wiki_pkg]: https://github.com/abelcheung/rifiuti2/wiki/Packages#packages-for-linux-and-bsd

### Others
For OS where `rifiuti2` is not readily available, it is always
possible to [compile from source][wiki2].

[wiki2]: https://github.com/abelcheung/rifiuti2/wiki/Compile-From-Source
