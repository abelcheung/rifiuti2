---
title:  "Version 0.7.0 released"
date:   2019-05-08 21:02:22
tags: [release]
---

Another prolonged &ldquo;hiatus&rdquo; passed before this update.
Quite a lot was done recently; right now there aren't
many features left before its completion in my opinion. And given
current technology trend, similar artifact carving tools may not be very
relevant now &#x2639; (actually it was already the case for like 10 years
ago, when people started relying on web services and mobile communication).

Perhaps the most time spent on this release is handling various character
sets, and have them display correctly; it was always a headache battling
with various character conversion implementations, and there's another
hurdle with displaying characters correctly under older Windows' archaic
console (it gets significantly better since Windows 10). Right now the
later part is not complete yet; error messages would still be garbled
if one changes console code page, which would (hopefully) be addressed soon.

Other than the usual bug fixes,
this release is more like one for archaeological and research purpose.
Almost nobody uses ancient Windows (95, NT 4.0 etc) for work and personal
computing purposes now. I can only generate recycle bin artifacts for those
systems using virtual machines. But still, they provide an interesting
historical insight on how the recycle bin features change over time.

Another feature I find exciting is setting up recycle bin on network shares.
Though there was wide claim that such thing can't be done, somebody has
managed to enable it for any mapped and even unmapped network drives!

See: [Enable Recycle Bin on mapped network drives][1]

[1]: https://social.technet.microsoft.com/Forums/windows/en-US/a349801f-398f-4139-8e8b-b0a92f599e2b/enable-recycle-bin-on-mapped-network-drives?forum=w8itpronetworking

Personally I use a more simplistic approach [based on an older article][2],
that is, move personal folders to a UNC path. It surprised me on how far
this feature is dated back; Windows ME and 2000 was verified to work!
Windows 98 would ask for permanent deletion of personal files in UNC path
though.

[2]: https://forums.mydigitallife.net/threads/tip-network-recycle-bin.16974/

Last but not least, there are a few important changes in bundled Windows
binaries:
* It doesn't work on Windows XP/2003 anymore, due to glib breaking
XP compatibility by using Vista-only API somewhere during 2.43 version.
* File output is always in UTF-8 encoding now (without Byte Order Mark).
Users are expected to open it with UTF-8 capable text editors.
* 32 and 64 bit binaries are bundled as separated zip files. On the
surface it means less bloat, though this change actually arised from
need of compromise for Windows building platform (Appveyor).

