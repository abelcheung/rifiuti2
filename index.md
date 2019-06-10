---
layout: page
title: Rifiuti2
subtitle: Windows Recycle Bin Analysis Tool
use-site-title: true
---

<!-- Download -->
<div class="jumbotron text-center">
  Windows 32 bit, 64 bit, or source code
  <p><a class="btn btn-primary btn-lg btn-download" href="{{ '/releases/latest' | prepend: site.repourl }}" role="button">
    <span style="display: flex; align-items: center">
      <span style="display: inline-block; margin-right: 0.5em" class="text-center text-btn-download">Download<br />Latest</span>
      <i class="fa-3x fa-inverse fas fa-download" data-fa-transform="shrink-6" data-fa-mask="fas fa-square"></i>
    </span>
  </a></p>
</div>

<img alt="Recycle Bin full icon" class="pull-right"
src="{{ '/images/rbin.png' | relative_url }}" />

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

[1]: {{ '/history' | relative_url }}
[2]: https://web.archive.org/web/20101121070625/http://www.foundstone.com/us/resources/proddesc/rifiuti.htm
[3]: {{ '/blob/master/NEWS.md' | prepend: site.repourl }}
[4]: {{ '/news' | relative_url }}

# Usage

`Rifiuti2` is designed to be portable, and runs on command line environment.
Two programs `rifiuti` and `rifiuti-vista` are chosen depending on relevant
Windows recycle bin format.

Please consult manpage (Unix) or README.html (bundled with Windows binaries)
for complete options and detailed usage description. There are some
usage samples [on Github page][5] as well.

[5]: {{ '/blob/master/README.md' | prepend: site.repourl }}

# Screenshots

<figure class="text-center" style="margin-top: 30px">
	<img src="{{ '/images/screenshot-tsv.png' | relative_url }}" />
	<figcaption>Tab delimited output sample</figcaption>
</figure>

---

<figure class="text-center">
	<img src="{{ '/images/screenshot3.png' | relative_url }}" />
	<figcaption>XML output with recycle bin on network share</figcaption>
</figure>

---

<figure class="text-center">
	<img src="{{ '/images/screenshot4.png' | relative_url }}" />
	<figcaption>Deleted items containing multilingual path names</figcaption>
</figure>

