---
title: "Guessing Windows version from artifacts"
date: 2019-07-22T10:07:00+08:00
category: internals
excerpt: >
  The logic of determining which Windows version generated a given recycle
  bin item. Both `INFO2` and `$Recycle.Bin` included, covering whole range
  of Windows.
tags: [info2,recycledir,windows]
---

When investigators are given an index file, it is immediately apparent,
from its file name, for one to have a quick grasp of the coarse generation
of Windows. However, by &ldquo;coarse&rdquo; I mean it is *very,
very imprecise*. With a file name like `$I87kHp4.jpg` one can only conclude
it's from Vista or above, no more or no less. Usually for real investigations
the Windows version is easily determined from other items (registry etc);
but on the rare case of only having recycle bin artifact available, one
must search for clue by directly peeking into the data.

## Determination for `$Recycle.Bin`

With `$Recycle.Bin` folder the rule is very simple, yet still quite
limited in the sense that Windows versions are not that accurate:

{% include fullwidth-figure.html src='/images/recycle-dir-logic.svg'
	alt="Diagram about how rifiuti2 determines Windows version for Vista or above"
	caption="How `rifiuti2` determines Windows version for Vista or above" %}

The check is very simple: just scan for version number, and violà. However,
unlike `INFO2` format which has undergone frequent changes, `$Recycle.Bin`
index format is very stable, so that's no way of pinpointing the exact
Windows version unfortunately.

## Determination for `INFO` and `INFO2`

On the other hand, pre-Vista artifacts need relatively more complex logic,
and in some place heuristical technique is needed. Yeees, some people may say
it's possible to guess from filename itself (`INFO2` only occurs since Win98),
but I'd rather play safe as files can be renamed easily.

{% include fullwidth-figure.html src='/images/recycle-INFO2-logic.svg'
	alt="Diagram about how rifiuti2 determines Windows version for 95 &ndash; 2003"
	caption="How `rifiuti2` determines Windows version for 95 &ndash; 2003" %}

Essentially it means:

<div class="table-responsive" markdown="1">
| Step | Check | Extra notes |
| --- | --- | --- |
| 1 | Version number | Result can be determined unless version = 5 |
| 2 | Record size | Must be Windows ME if size of each record is 280 bytes, otherwise continue to next step |
| 3 | Unicode path | Heuristically scan for junk data in trailing padding area after unicode path. Windows 2000 if found, otherwise XP/2003 [^1] |
{: .table}
</div>

[^1]: It *might* be possible to misidentify as XP/2003 in case padding area contains no junk data, though so far no such evidence has been encountered yet. Developers *finally* cared to zero out allocated memory since Windows XP/2003.