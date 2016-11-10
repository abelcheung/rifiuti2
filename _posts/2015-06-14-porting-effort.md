---
title: "Porting effort"
date: 2015-06-14 11:46:35
category: development
---

Porting `rifiuti2` to Solaris is proven to be the most problematic
of all so far.  Effort spent on FreeBSD and NetBSD are trivial; they
almost work instantly except incompatibility of tools used in some
test cases. Time spent on Solaris is more than the other platforms
combined, partly due to fruitless effort to make Sparc64 emulation
under Qemu work, and remaining time is spent on struggling with Solaris
implementation of `iconv()`. It is ~~utter crap~~ too limited to be
useful for `rifiuti2`; while containing lots of IBM code pages, it
lacks support for many Windows code pages, which would be necessary
for `rifiuti2`.

So in the end decision is made to simply ignore Solaris `iconv()`
and require glib2 compiled with GNU libiconv instead. [OpenCSW][2]
packages fit for such purpose, as well as the (now deprecated)
[SunFreeware][3]. Though [UnixPackages][4] is the successor
of SunFreeware, paying subscription service just for testing a rare
use case (using `rifiuti2` on Solaris) does&apos;t justify the cost,
so this is the status quo.

[1]: http://opensxce.org/
[2]: https://www.opencsw.org/
[3]: https://www.sunfreeware.com/
[4]: https://unixpackages.com/
