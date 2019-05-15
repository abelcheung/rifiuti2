---
title:  "Reason for abandoning MSYS 1.0"
date:   2015-05-15 11:26:02
tags: [development]
---

There are multiple reasons for abandoning MSYS 1.0 as supported compile
platform, in favor of [MSYS2][1].

* Testsuite is guaranteed to fail. There are 2 issues here:
  1. MSYS 1.0 bash simply won&rsquo;t work with non-ASCII path, which
     is listed as a test case in `rifiuti2`. All such paths are treated
     as &lsquo;No such file or directory&rsquo;. Same problem has been
     observed on MSYS2 until [g_win32_get_command_line()][2] call is used.
  1. Some test cases mysteriously fail, but the difference of result inspected
     with bare eye seems identical. Apparently there is some discrepancy
     of new line characters at work &mdash; it is not an issue on Linux / Unix,
     but problem will be very observable on Windows.
* There was no more update for MSYS / MinGW32 since 2012. Not a fatal sin,
  but if there is any problem, I have to maintain everything and fix
  manually. And that&rsquo;s not a reproduceable development environment.
* No 64-bit support. It is possible to compile both 32 and 64-bit binaries
  with MSYS2, but old version is 32-bit only.

[1]: https://msys2.github.io
[2]: https://developer.gnome.org/glib/stable/glib-Windows-Compatibility-Functions.html#g-win32-get-command-line
