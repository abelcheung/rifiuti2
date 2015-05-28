---
---

<img alt="Recycle Bin full icon" style="border:0; float:right" src="{{ site.baseurl }}/images/rbin.png" />

`Rifiuti2` analyse recycle bin files from Windows. Analysis of
Windows recycle bin is usually carried out during Windows computer
forensics. Rifiuti2 can extract file deletion time, original
path and size of deleted files and whether the deleted files have
been moved out from the recycle bin since they are trashed.

It is a [rewrite of `rifiuti`][1], which is [originally written][2]
by FoundStone folks for identical purpose. Then it was extended to
cover more functionalities, such as:

* Handles recycle bin up to Windows 10
  * Different recycle bin format since Vista
  * 64-bit file size support
* Handles ancient Windows like 95, NT4 and ME since 0.7.0
* Supports all localized versions of Windows &mdash; both Unicode-based
  ones and legacy ones (using ANSI code page)
* Supports output in XML format as well as original tab-delimited text

Latest features and bug fixes [are listed here][3].

[1]: history.html
[2]: https://web.archive.org/web/20101121070625/http://www.foundstone.com/us/resources/proddesc/rifiuti.htm
[3]: {{ site.repo_url }}/blob/master/NEWS.md

# Download and Usage

Please click on blue "Download on Github" link on top right corner of this
website to download and use `rifiuti2`.

`Rifiuti2` is designed to be portable, and runs on command line environment.
Two programs `rifiuti` and `rifiuti-vista` are chosen depending on relevant
Windows recycle bin format.

Please consult manpage (Unix) or README.html (bundled with Windows binaries)
for complete options and detailed usage description. There are some
usage samples [on Github page][3] as well.

[4]: {{ site.repo_url }}

![Tab-separated list sample]({{ site.baseurl }}/images/screenshot-tsv.png)

![XML sample]({{ site.baseurl }}/images/screenshot-xml.png)

