`Rifiuti2` is a for analyzing Windows Recycle Bin INFO2 file. Analysis of
Windows Recycle Bin is usually carried out during Windows computer
forensics. `Rifiuti2` can extract file deletion time, original path
and size of deleted files and whether the trashed files have been
permanently removed.

For those interested in what it does, and what functionality it
provides, please browse [official site][1] for more info.
Latest features and changes can be found in [NEWS](NEWS.md) file.

[1]: https://abelcheung.github.io/rifiuti2

## Usage

`rifiuti2` is designed to be portable, and runs on command line environment.
Depending on relevant Windows recycle bin format, there are 2 binaries to choose
(most users would probably want first one):

Program | Recycle bin from OS | Purpose
--------|---------------------|--------
`rifiuti-vista`|Vista or above|Scans `\$Recycle.bin` style folder
`rifiuti`  |Windows 95 to XP/2003|Reads `INFO` or `INFO2` file in `\RECYCLED` or `\RECYCLER` folder

Run programs without any option for more detail. Here are some of the
more useful options:

 Option | Purpose
-------:|:--------
-8      | Always print result in UTF-8 encoding
-o      | Output to file
-x      | Output XML instead of tab-separated fields
-l      | Display legacy (8.3) filenames and specify its codepage

Please consult manpage (Unix) or README.html (bundled with Windows binaries)
for complete options and detailed usage description.

#### Examples

* `rifiuti-vista.exe -x -z -o result.xml \case\S-1-2-3\`
> Scan for index files under `\case\S-1-2-3\`, adjust all deletion time
> for local time zone, and write XML output to `result.xml`
* `rifiuti -l CP932 -8 INFO2`
> Assume INFO2 file is generated from Japanese Windows, and display
> result on console in UTF-8 encoding

## Supported platform

It has been tested on Linux (as early as Ubuntu 8.04), Windows XP,
Windows 7, and recent FreeBSD, on both 32 and 64-bit intel CPU.
Some testing on big endian platforms are done with Qemu emulator.
More compatibility fix for other architectures welcome.

## Download

Windows binaries, if applicable, would be officially provided
[on Github release page][6].

On Linux side:
* DEB format packages are available officially on [Debian][7]
and [Ubuntu][8].
* There are some third party RPM packages, such as from
[CERT Linux Forensics Tools Repository][9], which might work on CentOS,
RHEL and Fedora.
* [ArchAssault][10], a penetration testing derivative of Arch Linux, has
`rifiuti2` packaged since late 2014.

Official [FreeBSD port][11] is available since 8.4.

For platforms not listed above, users would need to compile program themselves.
[Instructions are provided](docs/Compile.md) on how to compile on Linux,
\*BSD and Windows.

[6]: https://github.com/abelcheung/rifiuti2/releases
[7]: https://packages.debian.org/search?keywords=rifiuti2
[8]: http://packages.ubuntu.com/search?keywords=rifiuti2
[9]: https://forensics.cert.org/
[10]: https://archassault.org/packages/?q=rifiuti2
[11]: http://portsmon.freebsd.org/portoverview.py?category=security&portname=rifiuti2

