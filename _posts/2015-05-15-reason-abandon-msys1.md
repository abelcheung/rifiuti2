---
title:  "Reason for abandoning MSYS 1.0"
date:   2015-05-15T11:26:02+0800
last_modified_at: 2019-05-31T14:02:48+0800
category: development
redirect_from: /development/2015/05/15/reason-abandon-msys1.html
tags: [mingw,windows]
---

There are multiple reasons for abandoning MSYS 1.0 as supported compile
platform, in favor of [MSYS2][1].

* Testsuite is guaranteed to fail. There are 2 issues here:
  1. MSYS 1.0 bash simply won't work with non-ASCII path, which
     is listed as a test case in `rifiuti2`. All such paths are treated
     as &lsquo;No such file or directory&rsquo;. Same problem has been
     observed on MSYS2 until [`g_win32_get_command_line()`][2] call is used.
  1. Some test cases mysteriously fail, but the difference of result inspected
     with bare eye seems identical. Apparently there is some discrepancy
     of new line characters at work &mdash; it is not an issue on Linux / Unix,
     but problem will be very observable on Windows.
* There was no more update for MSYS / MinGW32 since 2012. Not a fatal sin,
  but if there is any problem, I have to maintain everything and fix
  manually. And that's not a reproduceable development environment.
* No 64-bit support. It is possible to compile both 32 and 64-bit binaries
  with MSYS2, but old version is 32-bit only.

**Four years later:** Looks like MinGW + MSYS [has a new home][3] and moving
again (e.g. 64-bit support) since 2018, but it's too little too late now.
{: .box-note}

[1]: https://www.msys2.org/
[2]: https://developer.gnome.org/glib/stable/glib-Windows-Compatibility-Functions.html#g-win32-get-command-line
[3]: https://osdn.net/projects/mingw/
