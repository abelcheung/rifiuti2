---
layout: page
title: Technical
subtitle: In-depth knowledge about recycle bin
permalink: /technical/
---

Recycle Bin has 2 major formats, which can be roughly divided as
&ldquo;before Vista&rdquo; and &ldquo;after Vista&rdquo;.

### Before Vista, known as the `INFO2` file

This hidden file is located in `\RECYCLED` folder (FAT16/32) or
`\RECYCLER\<sid>` folder (NTFS). MSDN [has a complete explanation][1] about the
name difference.
 Although researched info about this
older format has been [widely circulated][2], there are some inaccuracies
in my opinion regarding the INFO2 data fields. Besides, available info
generally covers XP/2003 format, which is a bit different from earlier
Windows (95, 98, ME, etc).

_Note_: `<sid>` stands for [Security Identifier][3], which is unique
for each user on a system.

[1]: http://blogs.msdn.com/b/oldnewthing/archive/2006/01/31/520225.aspx
[2]: http://www.csisite.net/INFO2.htm
[3]: https://en.wikipedia.org/wiki/Security_Identifier

### After Vista, known as `\$Recycle.bin` folder

For this format, recycle bin folder is located in 
`$Recycle.bin\<sid>`.  Deletion info for recycled files are
not stored in single file.  Instead, each recycled file has its own
accompanied index file with very similar name.  When original file is
permanently deleted or restored, the corresponding index file would
be removed too.

