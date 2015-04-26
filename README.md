`rifiuti2` is a rewrite of [rifiuti][1], a great tool from Foundstone folks for analyzing Windows Recycle Bin INFO2 file. Analysis of Windows Recycle Bin is usually carried out during Windows computer forensics. Quoting from original Foundstone page (now purchased by McAfee):

> Many computer crime investigations require the reconstruction of a subject's Recycle Bin. Since this analysis technique is executed regularly, we researched the structure of the data found in the Recycle Bin repository files (INFO2 files). Rifiuti, the Italian word meaning "trash", was developed to examine the contents of the INFO2 file in the Recycle Bin. ...... Rifiuti is built to work on multiple platforms and will execute on Windows (through Cygwin), Mac OS X, Linux, and *BSD platforms."

But since rifiuti (last updated 2004) is restricted to English version of Windows (fail to analyze any non-latin character), thus this rewrite. However it does more:

* Supports legacy Windows file names in any character set supported by `iconv()`, as well as Unicode ones
* Handles recycle bin up to Windows 10 (no more uses INFO2 file since Vista)
* Preliminary guard against specially crafted recycle bin files
* Supports output in XML format as well as original tab-delimited text
* User interface and messages can be translated

Latest features and changes can be found in NEWS file.

[1]: https://web.archive.org/web/20101121070625/http://www.foundstone.com/us/resources/proddesc/rifiuti.htm

###Usage

`rifiuti` is designed to be portable command line programs. Depending on relevant Windows recycle bin format, there are 2 binaries to choose (most users would probably want first one):

* `rifituti-vista`: Scans `\$Recycle.bin` style folder (Vista or above)
* `rifiuti`: Reads INFO2 file in `\RECYCLED` or `\RECYCLER` folder (Windows 98 to 2003)

Run `rifiuti --help-all` or `rifiuti-vista --help-all` for their corresponding command line options.

###Compiling

The only mandatory requirement is [Glib][2] ≥ 2.16. It should be bundled with any Linux / Unix systems since 2008. Windows libraries and headers are also available for download from [GTK+ project][3].

**Warning**: Since 0.6.x, `rifiuti` would not be guaranteed to compile on old [MinGW32][4] (project has become stagnant since 2012). Any future releases will be compiled on MinGW64 in case of Windows platform. For reference author is using [MSYS2][5] as development platform on Windows.

[2]: https://git.gnome.org/browse/glib
[3]: http://www.gtk.org/download/index.php
[4]: http://www.mingw.org/
[5]: https://msys2.github.io/

###Availability

Windows binaries, if applicable, would be officially provided here.

For Linux, DEB format packages are available officially on [Debian][6] and [Ubuntu][7]. There are some third party RPM packages, such as from [CERT Linux Forensics Tools Repository][8].

[6]: https://packages.debian.org/search?keywords=rifiuti2
[7]: http://packages.ubuntu.com/search?keywords=rifiuti2
[8]: https://forensics.cert.org/

###Build status
[![Build Status](https://travis-ci.org/abelcheung/rifiuti2.svg)](https://travis-ci.org/abelcheung/rifiuti2)
