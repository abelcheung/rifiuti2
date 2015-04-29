`rifiuti2` is a rewrite of [rifiuti][1], a great tool from Foundstone folks for analyzing Windows Recycle Bin INFO2 file. Analysis of Windows Recycle Bin is usually carried out during Windows computer forensics. Quoting from original Foundstone page (now purchased by McAfee):

> Many computer crime investigations require the reconstruction of a subject's Recycle Bin. Since this analysis technique is executed regularly, we researched the structure of the data found in the Recycle Bin repository files (INFO2 files). Rifiuti, the Italian word meaning "trash", was developed to examine the contents of the INFO2 file in the Recycle Bin. ...... Rifiuti is built to work on multiple platforms and will execute on Windows (through Cygwin), Mac OS X, Linux, and *BSD platforms."

But since rifiuti (last updated 2004) is restricted to English version of Windows (fail to analyze any non-latin character), thus this rewrite. However it does more:

- [x] Supports all localized versions of Windows &mdash; both newer Unicode-based ones and legacy ones
- [x] Handles recycle bin up to Windows 10 (no more uses INFO2 file since Vista)
- [x] Preliminary guard against specially crafted recycle bin files
- [x] Supports output in XML format as well as original tab-delimited text
- [x] User interface and messages can be translated

Latest features and changes can be found in [NEWS](News.md) file.

[1]: https://web.archive.org/web/20101121070625/http://www.foundstone.com/us/resources/proddesc/rifiuti.htm

### Usage

`rifiuti` is designed to be portable command line programs. Depending on relevant Windows recycle bin format, there are 2 binaries to choose (most users would probably want first one):

Program | Recycle bin from OS | Purpose
--------|---------------------|--------
`rifiuti-vista`|Vista or above|Scans `\$Recycle.bin` style folder
`rifiuti`  |Windows 98 to 2003|Reads `INFO2` file in `\RECYCLED` or `\RECYCLER` folder

Run programs without any option or with `--help-all` option for more detail. Here are some of the more useful options:

 Option | Purpose
-------:|:--------
-8      | Always output in UTF-8
-o      | Output to file
-x      | Output XML instead of tab-separated list
-l      | Display legacy (8.3) filenames and specify its codepage

#### Examples

* <dt>`rifiuti-vista -x -o recyclebin.xml S-1-2-3\`</dt>
<dd>Parse recycle bin folder `S-1-2-3` and write XML result to `recyclebin.xml`</dd>

* <dt>`rifiuti -l CP932 -8 INFO2`</dt>
<dd>Assume INFO2 file is generated from Japanese Windows, and display result on console in UTF-8 encoding</dd>

### Supported platform

It has been tested on Linux (as early as Ubuntu 8.04), Windows XP and Windows 7.
More compatibility fix for other architectures welcome.

### Availability

Windows binaries, if applicable, would be officially provided here.

For Linux, DEB format packages are available officially on [Debian][6] and [Ubuntu][7]. There are some third party RPM packages, such as from [CERT Linux Forensics Tools Repository][8].

[6]: https://packages.debian.org/search?keywords=rifiuti2
[7]: http://packages.ubuntu.com/search?keywords=rifiuti2
[8]: https://forensics.cert.org/

### Build status
[![Build Status](https://travis-ci.org/abelcheung/rifiuti2.svg)](https://travis-ci.org/abelcheung/rifiuti2)
