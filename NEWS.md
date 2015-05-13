## 0.6.0
#### Feature
* Windows 10 recycle bin support
* 8.3 path names can also be used in XML output now

#### Bug fix
* Win98 INFO2 trashed file size not retrieved correctly
* Substantial rework on showing translation and file names in different
  lanuages, especially on Windows platform

#### Change
* Display file deletion time in UTC time zone by default
* Vista version:
  * No more accepts multiple file arguments
  * Invalid file or dir in command argument treated as fatal error
  * Result is sort by deletion time, instead of random order
  * Show version info, to differentiate between Vista & Windows 10 formats
* INFO2 version:
  * No more accepts standard input as input data

----

## 0.5.1
* New manpage
* Test cases added to repository
* Debian packaging stuff added to repository

----

## 0.5.0
* Complete rewrite, using glib for I18N support and unicode handling
* This means INFO2 records from any localized version of Windows can
  be parsed correctly
* Since Vista recycle bin format changed completely, there will be no
  INFO2 file. A new program, `rifiuti-vista`, handles such format.
* Both program can output in XML format as well as tab-delimited
  plain text.
* Can choose to output long path name or legacy one (like "Progra~1")
* Some preliminary check to guard against specially crafted recycle
  bin files.
