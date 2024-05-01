---
title: "Rifiuti2 0.8.1 released"
date: 2024-05-01T03:20:00Z
category: release
redirect_from: /release/2024/05/01/version-0-8-1-released.html
tags: [feature,json]
excerpt: >
  Announcement for 0.8.1 release -- JSON output, live
  inspection on WSLv2 and such.
---

Other than the usual cleanup and refactoring after a major
rewrite, there are some features and additions worth
mentioning.

And &hellip; eh &hellip; I know this post should have been
written on the same day as 0.8.0 announcement do. Just slacked
off for a while 😉

## JSON format output

While XML output support has already been there, it isn't
so concise as some other formats do. Therefore JSON output
was implemented to fill that gap. It was initially considered
easy peasy, but later found to be cumbersome, due to the
need of filtering different characters for different output
formats. But the whole process was made more generic, so
adding yet another output format in the future may not be
as intensive as this one.

## Live inspection for Windows Subsystem on Linux

The intended support of live system inspection on WSL v2,
announced in previous release, was only materialized in this
release. So `rifiuti2` can be compiled as a native ELF binary
under WSL distro, and still manages to probe the Windows
system through mount points.

## Face lift for error report

It is now possible to report error for each and every entry,
instead of having a single collective error. Besides, efforts
were made to pay attention to truncated file; path display
can handle truncated bytes if the truncation doesn't go beyond
recycled file path.

