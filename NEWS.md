## 0.7.0 (to be released)
#### Feature Addition
* Support recycle bin from jurassic Windows: 95, NT4, ME
* Display timezone in tab-delimited output header
* Guess Windows version based on recycle bin artifacts
* Distributed Windows binaries:
  * Copes better with Windows ACL, detecting folder with
    insufficient permissions
  * Attempts to detect Windows locale setting and automatically determine
    translation to use

#### Change
* Now **mandates UTF-8 locale except on Windows**
  * File output is also in UTF-8 encoding under Windows
  * `-8` option is rendered obsolete as a result
* **Distributed Windows binaries can only run on Vista or above**
  * Windows XP/2003 support removed due to glib changes
* Won&apos;t overwrite destination file if it already exists
* `$Recycle.bin` version:
  * Not printing file size field if it is corrupt
  * Exit with error status whenever errors are found in any entry,
    not just the last entry
* `INFO2` version:
  * Restricts the choice of legacy path character encoding; generally,
    all encodings not ASCII compatible are disallowed
* Remove GNUism for part of build toolchain (`make`, `awk`)
* Requires GNU gettext

#### Bug fix
* Fix unicode display on Windows console (Issue #12)
* More robust handling of invalid or undecipherable characters,
  displaying escaped hex or unicode sequences in such cases (Issue #5)

----

## 0.6.1
#### Bug fix
* Restore old date/time format for tab-delimited output, in order to be
  more spreadsheet friendly (Issue #8)
* Fix timezone offset for ISO8601-format date, to account for DST
* Fix data retrieval on big endian systems
* No more attempt to limit usage of TZ environment variable (which
  doesn&apos;t work anyway)

----

## 0.6.0
#### Feature
* Windows 10 recycle bin support (Issue #1)
* Add GUI dialog to notify first time Windows users (Issue #2)
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
  * Result is sorted by deletion time, instead of random order
  * Show version info in order to differentiate between Vista & Windows 10 formats
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
