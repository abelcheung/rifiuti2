---
---

<img alt="Recycle Bin full icon" style="border:0; float:right" src="{{ site.baseurl }}/images/rbin.png" />

`Rifiuti2` analyse recycle bin files from Windows. Analysis of
Windows recycle bin is usually carried out during Windows computer
forensics. Rifiuti2 can extract file deletion time, original
path and size of deleted files and whether the deleted files have
been moved out from the recycle bin since they are trashed.

It is a rewrite of `rifiuti`, which is originally written by FoundStone
folks for identical purpose. Then it was extended to cover more
functionalities, such as:

* Handles recycle bin up to Windows 10 (no more uses INFO2 file since Vista)
* Supports all localized versions of Windows &mdash;
both newer Unicode-based ones and legacy ones
* Supports output in XML format as well as original tab-delimited text

[1]: https://web.archive.org/web/20101121070625/http://www.foundstone.com/us/resources/proddesc/rifiuti.htm

# Usage

`Rifiuti2` is designed to be portable, and runs on command line environment.
Depending on relevant Windows recycle bin format,
there are 2 binaries to choose (most users would probably want first one):

Program | Recycle bin from OS | Purpose
--------|---------------------|--------
`rifiuti-vista`|Vista or above|Scans `\$Recycle.bin` style folder
`rifiuti`  |Windows 98 to XP/2003|Reads `INFO2` file in `\RECYCLED` or `\RECYCLER` folder

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

# Examples

* <dl><dt><code>rifiuti-vista.exe -x -z -o result.xml \case\S-1-2-3\</code></dt>
  <dd>Scan for index files under <code>\case\S-1-2-3\</code>, adjust all deletion
  time for local time zone, and write XML output to result.xml</dd></dl>
* <dl><dt><code>rifiuti -l CP932 -8 INFO2</code></dt>
  <dd>Assume INFO2 file is generated from Japanese Windows, and display
  result on console in UTF-8 encoding</dd></dl>

![Tab-separated list sample]({{ site.baseurl }}/images/screenshot-tsv.png)

![XML sample]({{ site.baseurl }}/images/screenshot-xml.png)

