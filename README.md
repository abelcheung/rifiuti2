`rifiuti2` is a rewrite of [rifiuti](https://web.archive.org/web/20101121070625/http://www.foundstone.com/us/resources/proddesc/rifiuti.htm), a great tool from Foundstone folks for analyzing Windows Recycle Bin INFO2 file. Analysis of Windows Recycle Bin is usually carried out during Windows computer forensics. Quoting from original Foundstone page (now purchased by McAfee):

> Many computer crime investigations require the reconstruction of a subject's Recycle Bin. Since this analysis technique is executed regularly, we researched the structure of the data found in the Recycle Bin repository files (INFO2 files). Rifiuti, the Italian word meaning "trash", was developed to examine the contents of the INFO2 file in the Recycle Bin. ...... Rifiuti is built to work on multiple platforms and will execute on Windows (through Cygwin), Mac OS X, Linux, and *BSD platforms."

But since rifiuti (last updated 2004) is restricted to English version of Windows (fail to analyze any non-latin character), thus this rewrite. However it does more:

* Supports legacy Windows file names in any character set supported by `iconv()`, as well as Unicode ones
* Handles recycle bin up to Windows 10 (no more uses INFO2 file since Vista)
* Preliminary guard against specially crafted recycle bin files
* Supports output in XML format as well as original tab-delimited text

Latest features can be found in NEWS file.

###Usage

Two command line programs are available, depending on relevant Windows recycle bin format:
* `rifiuti2`: Reads INFO2 file in `\RECYCLED` or `\RECYCLER` folder (Windows 98 to 2003)
* `rifituti-vista`: Scans `\$Recycle.bin` folder (since Vista)

Run `rifiuti --help-all` or `rifiuti-vista --help-all` for their corresponding command line options.

###Compiling

Please refer to README file within the repository. **Warning**: Since 0.6.x, old [MinGW32](http://www.mingw.org/) would not be guaranteed to compile (project has become stagnant since 2012). Any future releases will be compiled on MinGW64 in case of Windows platform. For reference author is using [MSYS2](https://msys2.github.io/) as development platform.

###Availability

Windows binaries, if applicable, would be officially provided here.

For Linux, DEB format packages are available officially on Debian and Ubuntu. There are some third party RPM packages, such as from [CERT Linux Forensics Tools Repository](https://forensics.cert.org/)

###Build status
[![Build Status](https://travis-ci.org/abelcheung/rifiuti2.svg)](https://travis-ci.org/abelcheung/rifiuti2)
