---
title: "Rifiuti2 0.6.1 released"
date: 2015-05-28T14:08:51+0800
last_modified_at: 2019-07-13T12:05:43+0800
category: release
redirect_from: /release/2015/05/28/version-0-6-1-released.html
excerpt: >
  This is a bug fix release addressing 3 issues: endianness, date time format and timezone value.
tags: [bug fix,timestamp]
---

This is a bug fix release addressing 3 issues:

1. Big-endian systems have been ignored for all these days, as
   very few people belong to such user case. Most users would:
   - Either directly run `rifiuti2` on Wintel systems (little endian), or
   - Take snapshot of file system and extract files on Linux for
     further inspection (also most likely little endian)

   But there is actually no reason not to fix it &mdash; especially original
   `rifiuti` already coped with big-endian systems from the start.

   So made up my mind and set up a [Qemu PPC Debian virtual machine][1].
   Hopefully this is enough for addressing any big-endian issues in future.

1. For tab-delimited output, use the old `YY:MM:DD hh:mm:ss` date/time format
   again. This would hopefully ease problem of having spreadsheet programs
   (like MS Office and OpenOffice) not recognizing the format. Although
   the question of addressing ISO8601 format in Excel has been discussed
   in some places ([like this StackOverflow answer][2]), it is always better
   to let users handle the data as efficient as possible.

1. Timezone value for places using [Daylight Saving Time][3] was wrong &mdash;
   it was not tested as rigorously as it should be, and the support for date/time
   in Windows C runtime library [turns out to be shaky][4].  Recent testing
   indicates [`Windows _ftime()`][5] to be unreliable for use. In particular,
   `_timeb.dstflag` does not respect `TZ` environment variable and always use
   control panel setting, so users modifying `TZ` variable would see wrong
   result.

[1]: https://people.debian.org/~aurel32/qemu/powerpc/
[2]: https://stackoverflow.com/a/4896796
[3]: https://en.wikipedia.org/wiki/Daylight_saving_time
[4]: {{ site.baseurl }}{% post_url 2015-05-18-pain-in-timezone-support %}
[5]: https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/ftime-ftime32-ftime64
