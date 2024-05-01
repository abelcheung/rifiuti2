---
title: "Rifiuti2 0.8 released"
date: 2024-01-04T14:25:40+08:00
last_modified_at: 2024-05-01T02:55:00Z
category: release
redirect_from: /release/2024/01/04/version-0-8-released.html
tags: [feature,github,cmake]
excerpt: >
  Announcement for 0.8.0 release -- migration to CMake, etc.
---

This is a major rewrite of everything except source code. The
whole thing was lying around for too long without dusting,
so major cleanup takes quite some time to complete.

## CMake adoption

Migration away from `Autoconf` / `Automake` has been on radar since
last release, but nothing materialized until now. As expected, the
basic building infrastructure was not a problem, but rewriting test
suite in `CTest` proves to be a great challenge as expected.
Platform specific stuff, such as different encoding support of
`iconv()` in various platforms, is the major source of headache.

## Migration to GitHub workflow

One of the major source of security events during recent years is
building infrastructure and supply chain attack. GitHub has gone
through great pain in preventing token stealing and whatnot, but
not Appveyor and Travis-CI, so moving away from them is an obvious
step. Luckily [MSYS2 provides github action][setup_msys2] so that
migration is a breeze.

[setup_msys2]: https://github.com/msys2/setup-msys2

## Other infrastructure changes

Being able to translate text UI is never a selling point of this
utility. In fact I always find it redundant when no translation
was contributed all these years, while `gettext` m4 support has
caused too many grievances at the same time. Removing all that
feels like a burden off shoulder.

Manpage is similar. This is year 2024, not like 1994 where the
major source of documentation and help comes from offline ones.
It is sort of like another burden when one doesn't use `roff`
macros in everyday work, and need to relearn them over and over
again in order to update manpage.

## Live system inspection

The idea was already there long time ago, almost as old as the
utility itself, but not until recently did I take the effort.
Implementing on different platforms (native Windows, and
Subsystem for Linux) requires lots of code refactor, but not
a major obstacle though. Now only [one roadblock left][issue]
before 1.0 release.

[issue]: https://github.com/abelcheung/rifiuti2/issues/33

<hr class="short" />

<div class="table-responsive small" markdown="1">

| Date | ChangeLog |
| --- | --- |
| `2024-05-01` | Add link to Github issue |
{: .table .table-condensed}
