---
title: "Pain in timezone support"
date:  2015-05-18 09:05:29
category: development
---

Haven&apos;t anticipated the addition of timezone info has caused so much
grief for me, though lots of &ldquo;fun&rdquo; are uncovered during the
process.
<!--more-->

## `strftime()` is not very platform neutral

[`strftime()` on Windows][1] is less capable then the Unix ones. For
compatibility, the date / time format would need to be expressed as
`%Y-%m-%d %H:%M:%S` in place of just `%F %T`; nor does it print
numerical time zones.

## `TZ` environment variable on Windows is crap

Current systems don&apos;t use `$TZ` variable for any common purpose now.
It [used to be][2], in Windows 3.1. Older Linux systems also use it for
setting timezone. Nowadays, Linux / BSD make use of [zoneinfo database][3]
which automatically handles GMT offset and DST, and `$TZ` can also be
optionally set to location of these zoneinfo files under `/usr/share/zoneinfo`
to override system setting. But the situation on Windows is different. 

1. The TZ value is arbitrary and there is no checking. (The format for `TZ`
   variable in Windows is [documented in `_tzset()` function][4].) I can
   happily use the value `ABC123XYZ` as timezone and it would be accepted
   as a timezone having -123 hours of offset from UTC. The letters are
   junk &mdash; except that using 4 letters (like `EEST` which is a valid
   timezone in Istanbul) and the parser for `$TZ` variable immediately fails.

1. Compare these 2 commands: (_hint_: drag and highlight lines with mouse)
{% highlight sh %}
set TZ=
set TZ= 
{% endhighlight %}
The first line unsets TZ variable as expected, so that Windows would
retrieve regional setting from control panel. But with an extra space
in 2nd line, timezone is set to UTC with [Daylight Saving Time][5]
forcefully turned on!!! It costs me days of head scratching and several
faulty &ldquo;fixes&rdquo;. God knows what the parser is doing!

## `_timeb` structure does not respect `$TZ` variable

The DST value returned from `_timeb` structure is faulty, in that it
only respects the timezone setting from Control Panel and not `$TZ`
variable. That&apos;s one of the bug addressed in 0.6.1 version.
The following table shows comparison of values of `_timeb.dstflag` and
`tm.tm_isdst`:

<table style="border:1px black; text-align:center">
<tr>
<th style="border: 0" colspan="2" rowspan="2"></th>
<th colspan="2">Control Panel</th>
</tr>
<tr><th>Use DST</th><th>No DST</th></tr>
<tr>
<th rowspan="3"><code>$TZ</code></th>
<th>(unset)</th>
<td>1 / 1</td>
<td>0 / 0</td>
</tr>
<tr>
<th>UTC</th>
<td style="background: #fcc">1 / 0</td>
<td>0 / 0</td>
</tr>
<tr>
<th>PST8PDT</th>
<td>1 / 1</td>
<td style="background: #fcc">0 / 1</td>
</tr>
</table>

Besides, `_timeb.timezone` does not take DST into account at all,
requiring one to perform extra steps to detect and adjust the value
based on DST status. Only `tm.tm_isdst` is reliable enough for use in
`rifiuti2`.

## `INFO` file stores UTC time since 95

Enough Windows bashing. Actually, Microsoft developers are surprisingly
forward-thinking in some aspects.
The `INFO` file (in Win95, predates `INFO2` used in Win98) already uses
[64-bit FileTime][6], when 32-bit systems were still not mature yet.
And this FileTime stores UTC time, not local time which is still dominant
in system time of current Windows. That saved lots of headache when
constructing event timeline.

[1]: https://msdn.microsoft.com/en-US/library/fe06s4ak(v=vs.80).aspx
[2]: http://science.ksc.nasa.gov/software/winvn/userguide/3_1_4.htm
[3]: https://en.wikipedia.org/wiki/Tz_database
[4]: https://msdn.microsoft.com/en-us/library/90s5c885(VS.80).aspx
[5]: https://en.wikipedia.org/wiki/Daylight_saving_time
[6]: https://support.microsoft.com/en-us/kb/188768

----

2015-05-28 edit: Add description about problem in `_timeb`.
