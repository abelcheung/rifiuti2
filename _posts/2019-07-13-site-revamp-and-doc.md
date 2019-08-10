---
title:  "Site revamp and documentation"
date: 2019-07-13T12:38:13+0800
category: site
excerpt: Random babbling about this site and TODOs. Feels like I'm more into web development …
tags: [info2]
---

Visitors should have noticed the site has been undergoing a major facelift, replacing
archaic theme with a more responsive, mobile friendly one. I start feeling like more
into Jekyll and web development instead of doing any work on `rifiuti2` now. &#x263A;

But there is a missing piece in website content which I always wished to write about:
the slight variations between different `INFO`/`INFO2` formats supported by generations
of Windows. The `$Recycle.Bin` format is relatively clean and clear, but the subtleties
in `INFO2` format was not very well known, or forgotten in history of internet.

For example, following is a sample comparison of 95 / 2003 INFO2 header broken down
into 4-byte group:

<div class="table-responsive" markdown="1">

| Offset | Meaning | 95 sample | 2003 sample |
| --- | --- | --- | --- |
| `0x00` | Version     | `0000 0000` | `0500 0000` |
| `0x04` | ???         | `0B00 0000` | `0000 0000` |
| `0x08` | ???         | `1000 0000` | `0000 0000` |
| `0x0C` | Record size | `1801 0000` | `2003 0000` |
| `0x10` | ???         | `0000 2C00` | `0000 0000` |
{: .table .table-striped .table-responsive}

</div>

Few people would talk about offset `0x04`, `0x08` and `0x10` (all marked with ???), as
they were only used in 95 and NT4. Those fields were from the era when `INFO2`
doesn't keep purged records at all, and developers decide to only keep a tally count
of recycled items. The 3 fields correspond to:

- Total items still inside recycle bin
- Total items ever been recycled
- Total cluster size of all available items

**Trivia:** the 32 bit size guarantees that recycle bin can't ever
exceed 2GB. Not that 2GB hard drive existed during that dynasty &hellip;
{: .callout .callout-info}

They were ignored probably because 98 / ME / 2000 filled those bytes with seemingly
random data. Faintly recalled that those are in fact memory chunks on system, and
can potentially leak sensitive information (unencrypted file contents, credentials,
you name it). I really want to document all these stuff clearly, as few people
would ever be knowledged in such ancient and minor file format.
