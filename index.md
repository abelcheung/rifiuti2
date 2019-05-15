---
title: Rifiuti2
subtitle: Windows Recycle Bin Analysis Tool
use-site-title: true
---

<img alt="Recycle Bin full icon" style="border:0; float:right"
src="{{ '/images/rbin.png' | prepend: site.baseurl }}" />

`Rifiuti2` analyse recycle bin files from Windows. Analysis of
Windows recycle bin is usually carried out during Windows computer
forensics. Rifiuti2 can extract file deletion time, original
path and size of deleted files. For more ancient versions of Windows,
it can also check whether deleted items were not in recycle bin anymore
(that is, either restored or permanently purged).

It is a [rewrite of `rifiuti`][1], which is [originally written][2]
by FoundStone folks for identical purpose. Then it was extended to
cover more functionalities, such as:

* Handles oldest (Win95) to newest (Win 10 and Server 2019) recycle bin format
  * Windows 95 &ndash; 2003 uses a single index file named `INFO` or `INFO2`
  * Vista or above uses one index file for each deleted item
* 64-bit file size support
* Supports all localized versions of Windows &mdash; both Unicode-based
  ones and legacy ones (using ANSI code page)
* Supports output in XML format as well as original tab-delimited text
* Obscure features such as recycle bin on network share
  (`\\server\share_name`)

Latest features and bug fixes [are listed inside NEWS file][3]; you're
also welcome to [check out blog news][4] for more insight
(and some grumbles &#x263A;).

[1]: {{ '/history' | prepend: site.baseurl }}
[2]: https://web.archive.org/web/20101121070625/http://www.foundstone.com/us/resources/proddesc/rifiuti.htm
[3]: {{ '/blob/master/NEWS.md' | prepend: site.repourl }}
[4]: {{ '/news' | prepend: site.baseurl }}

# Download and Usage

Click "Download" link on top right corner of this website, and one will be
redirected to Github release page, where Windows binaries and source
tarball are available for download.

`Rifiuti2` is designed to be portable, and runs on command line environment.
Two programs `rifiuti` and `rifiuti-vista` are chosen depending on relevant
Windows recycle bin format.

Please consult manpage (Unix) or README.html (bundled with Windows binaries)
for complete options and detailed usage description. There are some
usage samples [on Github page][5] as well.

[5]: {{ '/blob/master/README.md' | prepend: site.repourl }}

# Screenshots

Normal tab delimited output:  
![Tab-separated list sample]({{ '/images/screenshot-tsv.png' | prepend: site.baseurl }})

XML output with recycle bin on network share:  
![XML sample]({{ '/images/screenshot3.png' | prepend: site.baseurl }})

Deleted items containing multilingual path names:  
![Localized path sample]({{ '/images/screenshot4.png' | prepend: site.baseurl }})

