---
title: "Pain in timezone support"
date: 2015-05-18T09:05:29+08:00
last_modified_at: 2024-05-01T02:31:00Z
category: development
redirect_from: /development/2015/05/18/pain-in-timezone-support.html
excerpt: >
  The bitter fight against Windows timezone support …
tags: [bug fix,info2,timestamp]
bigimg:
  - /images/time-zone-bg.png: "Time Zone for countries, courtesy of Wikipedia Commons"
---

Haven't anticipated the addition of timezone info has caused so much
grief for me, though lots of &ldquo;fun&rdquo; are uncovered during the
process.

**Four years later:** [`GDateTime` structure][gdt] will be used to
simplify cross platform handling of such date / time issue.
Let's see how far it can go.
{: .callout .callout-info}

### `strftime()` is not very platform neutral

[`strftime()` on Windows][ms_strftime] is less capable than Unix counterpart.
For compatibility, the date / time format would need to be expressed as
`%Y-%m-%d %H:%M:%S` in place of just `%F %T` (supporting ISO C89
standard but not C99); nor does it print numerical time zones.

### `TZ` environment variable on Windows is crap

Nowadays systems don't use `TZ` variable for common purpose anymore. [^1]
Linux / BSD make use of [Olson time zone database][olson] which
automatically handles GMT offset and
<abbr title="Daylight Saving Time" class="initialism">DST</abbr>,
while `TZ` can also be set in well-defined manner to temporarily override
system setting. Windows users would be familiar with Control Panel settings
instead. But `TZ` variable in Windows is arbitrary and there is no
rigorous checking [^2], resulting in hilarious scenarios:

[^1]: `TZ` variable used to be a common mechanism to
      [set time zone for Windows 3.1][set_tz]. Same applies to ancient Linux
      systems.

[^2]: Format of `TZ` variable is [documented in `_tzset()` function][ms_tzset].
      *However*, it doesn't mention the behavior if supplied value does
      not satisfy documented format. In fact virtually any randomly
      invented values would be accepted.

1. For example, I can happily use the value `ABC123XYZ` as timezone and it
   would be accepted as a timezone having *-123 hours offset from UTC*.
   The letters are merely junk &mdash; except that using 4 letters (like
   `EEST` which is a valid timezone in Istanbul) would cause functions
   utilitizing `TZ` variable to wreak havoc.

2. Compare these 2 commands:

   <kbd>set TZ=</kbd><br /><kbd>set TZ= </kbd>

   The first line unsets TZ variable as expected, so that Windows would
   retrieve regional setting from control panel. But with an extra space
   in 2nd line, timezone is set to ***UTC with Daylight Saving Time
   forcefully turned on***!!! It costs me days of head scratching and several
   faulty &ldquo;fixes&rdquo;.

### `_timeb` structure does not respect `TZ` variable

The DST value returned from `_timeb` structure is faulty, in that it
only respects the timezone setting from Control Panel and not `TZ`
variable. That's one of the bug addressed in 0.6.1 version.
The following table shows how the values of `_timeb.dstflag` and
`tm.tm_isdst` vary with `TZ` and Control Panel settings (undesirable
values <span class="bg-danger">marked in red background</span>):

<div class="row">

  <div class="col-sm-6">
  <table class="table text-center">
  <caption><code>_timeb.dstflag</code> value</caption>
  <thead>
    <tr>
    <th colspan="2" rowspan="2">&nbsp;</th>
    <th colspan="2">Control Panel</th>
    </tr>
    <tr><th>Use DST</th><th>No DST</th></tr>
  </thead>
  <tbody>
    <tr>
      <th rowspan="3"><code>TZ</code></th>
      <th>(unset)</th>
      <td>1</td>
      <td>0</td>
    </tr>
    <tr>
      <th>UTC</th>
      <td class="danger">1</td>
      <td>0</td>
    </tr>
    <tr>
      <th>PST8PDT</th>
      <td>1</td>
      <td class="danger">0</td>
    </tr>
  </tbody>
  </table>
  </div>

  <div class="col-sm-6">
  <table class="table text-center">
  <caption><code>tm.tm_isdst</code> value</caption>
  <thead>
    <tr>
    <th colspan="2" rowspan="2">&nbsp;</th>
    <th colspan="2">Control Panel</th>
    </tr>
    <tr><th>Use DST</th><th>No DST</th></tr>
  </thead>
  <tbody>
    <tr>
      <th rowspan="3"><code>TZ</code></th>
      <th>(unset)</th>
      <td>1</td>
      <td>0</td>
    </tr>
    <tr>
      <th>UTC</th>
      <td>0</td>
      <td>0</td>
    </tr>
    <tr>
      <th>PST8PDT</th>
      <td>1</td>
      <td>1</td>
    </tr>
  </tbody>
  </table>
  </div>

</div>

It is immediately apparent that `_timeb.timezone` ignores `TZ` completely.
OTOH `tm.tm_isdst` consults both settings, so is reliable enough for use
in `rifiuti2`.

### Nice stuff: `INFO` file stores UTC time since 95

Enough Windows bashing. Actually, Microsoft developers are surprisingly
forward-thinking in some aspects.
The `INFO` file (in Win95, predates `INFO2` used in Win98) already uses
[64-bit `FILETIME`][kb], when 32-bit systems were still not mature yet.
And this `FILETIME` stores UTC time, not local time which is still dominant
in system time of current Windows. That saved lots of headache when
constructing event timeline.

[ms_strftime]: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/strftime-wcsftime-strftime-l-wcsftime-l
[set_tz]: https://web.archive.org/web/20201029072825/http://science.ksc.nasa.gov/software/winvn/userguide/3_1_4.htm
[olson]: https://en.wikipedia.org/wiki/Tz_database
[ms_tzset]: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/tzset
[kb]: https://mskb.pkisolutions.com/kb/188768
[gdt]: https://developer-old.gnome.org/glib/stable/glib-GDateTime.html

<hr class="short" />

<div class="table-responsive small" markdown="1">

| Date | ChangeLog |
| --- | --- |
| `2015-05-28` | Add description about problem in `_timeb` |
| `2019-06-04` | Use of `GDateTime` to replace the whole mess |
| `2024-05-01` | Replace links to make them reachable |
{: .table .table-condensed}

</div>
