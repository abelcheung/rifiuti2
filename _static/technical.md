---
layout: page
title: Technical
subtitle: In-depth knowledge about recycle bin
permalink: /technical/
---

Recycle Bin has 2 major formats, which can be roughly divided as
&ldquo;before Vista&rdquo; and &ldquo;after Vista&rdquo;.

<div class="row">

<div class="col-lg-6 col-md-6 col-sm-6" markdown="1">

### Before: `INFO2` file

Though widely known as `INFO2` file, it is actually named `INFO`
for Windows 95 and NT 4.0. This hidden file contains relevant
meta info for all deleted items. Its location [varies with
different file system][1] (using C drive as example):

| Location | Filesystem |
| --- | --- |
| `C:\RECYCLED` | FAT16/32 |
| `C:\RECYCLER\<sid>` | NTFS |
{: .table .table-condensed}

Since Windows 98, metadata about permanently purged items or
restored items would be kept inside `INFO2`.

Although researched info about this older format has been widely
circulated [^1], they generally covers Windows XP/2003 only, which is
a bit different from earlier Windows (95, 98, ME, etc).

</div>

<div class="col-lg-6 col-md-6 col-sm-6" markdown="1">

### After: `$Recycle.bin` folder

For this format, recycle bin folder is located in
`C:\$Recycle.bin\<sid>` (C drive as example).
Deletion info for recycled files are
not stored in single file.  Instead, each recycled file has its own
accompanied index file with very similar name. For example, if a
PNG image is deleted, the deleted file name and its index would
look like this inside recycle bin folder:

| File name of | &nbsp; |
| --- | --- |
| Deleted Item | `$RDNLPD4.png` |
| Index | `$IDNLPD4.png` |
{: .table .table-condensed}

When deleted item is permanently purged, the corresponding index
file would be removed too. However, if deleted item is restored,
index file would be kept intact.

</div>

</div><!-- class=row -->

_Note_: `<sid>` stands for [Security Identifier][2], which is unique
for each user on a system.
{: .box-note}

[^1]: The URL used to be http://www.csisite.net/INFO2.htm , but it was
      taken down by new owner, and sadly not avaialble from
      Internet Archive, therefore permanently lost.

[1]: https://blogs.msdn.microsoft.com/b/oldnewthing/archive/2006/01/31/520225.aspx
[2]: https://en.wikipedia.org/wiki/Security_Identifier
