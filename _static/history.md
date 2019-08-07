---
layout: page
title: History
last_modified_at: 2019-08-07T23:03:09+0800
shareable: true
redirect_from: /history.html
permalink: /history/
---

Rifiuti2 is a rewrite of `rifiuti`, a tool of identical purpose written
by Foundstone.[^1] Quoting from [the original FoundStone page][1]:

> Many computer crime investigations require the reconstruction of
> a subject's recycle bin. Since this analysis technique is executed
> regularly, we researched the structure of the data found in the Recycle
> Bin repository files (INFO2 files). Rifiuti, the Italian word meaning
> &ldquo;trash&rdquo;, was developed to examine the contents of the INFO2 file in
> the Recycle Bin. &hellip; Rifiuti is built to work on multiple platforms
> and will execute on Windows (through Cygwin), Mac OS X, Linux, and
> \*BSD platforms.

However, since [the original rifiuti][3] (last updated 2004) can't analyze
recycle bin from any localized version of Windows (restricted to
English), this rewrite effort is born to overcome the limitation. The neccesity
arised from a task needed to handle localized Windows during around 2008,
and since then the effort was maintained. Later `rifiuti2` was extended to
cover more functionalities, such as (including but not limited to):

* Handles oldest (Win95) to newest (Win 10 and Server 2019) recycle bin format
  * Windows 95 &ndash; 2003 uses a single index file named `INFO` or `INFO2`
  * Vista or above uses one index file for each deleted item
* 64-bit file size support
* Supports all localized versions of Windows &mdash; both Unicode-based
  ones and legacy ones (using ANSI code page)
* Supports output in XML format as well as original tab-delimited text
* Obscure features such as recycle bin on network share
  (`\\server\share`)

[^1]: Foundstone was later [purchased by Mcafee][4] as a security consulting
      division. Though selected free tools are still available for download
      under McAfee, rifiuti is not one of them, at the <time
      datetime="{{ page.last_modified_at | date_to_xmlschema }}">time of writing</time>.

[1]: https://web.archive.org/web/20101121070625/http://www.foundstone.com/us/resources/proddesc/rifiuti.htm
[3]: https://sourceforge.net/projects/odessa/files/
[4]: https://www.mcafee.com/enterprise/en-us/services/foundstone-services.html
