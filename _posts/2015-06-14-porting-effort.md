---
title: "Porting effort"
date: 2015-06-14 11:46:35
category: development
tags: [porting,unix]
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

<del>So in the end decision is made to simply ignore Solaris `iconv()`
and require glib2 compiled with GNU libiconv instead. [OpenCSW][2]
packages fit for such purpose, as well as the (now deprecated)
[SunFreeware][3]. Though [UnixPackages][4] is the successor
of SunFreeware, paying subscription service just for testing a rare
use case (using `rifiuti2` on Solaris) does&apos;t justify the cost,
so this is the status quo.</del>

**4 years later**: Turns out it's just my ignorance, `iconv()` from
Solaris is fine to use, just that the extra encodings are not installed
by default (at least on some Solaris spin-off, say [Illumos][5]). Besides
it was actually hard to find glib2 package linking with external iconv
library; it's an exception rather than the norm.

[1]: http://opensxce.org/
[2]: https://www.opencsw.org/
[3]: https://www.sunfreeware.com/
[4]: https://unixpackages.com/
[5]: https://illumos.org/
