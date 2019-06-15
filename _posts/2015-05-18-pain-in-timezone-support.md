---
title: "Pain in timezone support"
date:  2015-05-18 09:05:29
category: development
tags: [bug fix,time]
---

Haven't anticipated the addition of timezone info has caused so much
grief for me, though lots of &ldquo;fun&rdquo; are uncovered during the
process.

**Four years later:** [`GDateTime` structure][7] will be used to simplify cross
platform handling of such date / time issue. Let's see how far it can go.
{: .box-note}

### `strftime()` is not very platform neutral

[`strftime()` on Windows][1] is less capable then the Unix ones. For
compatibility, the date / time format would need to be expressed as
`%Y-%m-%d %H:%M:%S` in place of just `%F %T` (supporting ISO C89
standard but not C99); nor does it print numerical time zones.

### `TZ` environment variable on Windows is crap

Current systems don't use `TZ` variable for any common purpose now. [^1]
Nowadays, Linux / BSD make use of [Olson time zone database][3] which
automatically handles GMT offset and
<abbr title="Daylight Saving Time" class="initialism">DST</abbr>,
while `TZ` can also be set in well-defined manner to temporarily override
system setting. But `TZ` variable in Windows is arbitrary and there is no
rigorous checking [^2], resulting in hilarious scenarios:

1. For example, I can happily use the value `ABC123XYZ` as timezone and it
   would be accepted as a timezone having -123 hours offset from UTC.
   The letters are merely junk &mdash; except that using 4 letters (like
   `EEST` which is a valid timezone in Istanbul) would cause functions
   utilitizing `TZ` variable to fail.

2. Compare these 2 commands:

   <kbd>set TZ=</kbd><br /><kbd>set TZ= </kbd>

   The first line unsets TZ variable as expected, so that Windows would
   retrieve regional setting from control panel. But with an extra space
   in 2nd line, timezone is set to **UTC with [Daylight Saving Time][5]
   forcefully turned on**!!! It costs me days of head scratching and several
   faulty &ldquo;fixes&rdquo;.

### `_timeb` structure does not respect `TZ` variable

The DST value returned from `_timeb` structure is faulty, in that it
only respects the timezone setting from Control Panel and not `$TZ`
variable. That's one of the bug addressed in 0.6.1 version.
The following table shows how the values of `_timeb.dstflag` and
`tm.tm_isdst` vary with `TZ` and Control Panel settings (undesirable
values <span class="bg-danger">marked in red background</span>):

<div class="row">

  <div class="col-lg-6 col-md-6 col-sm-6">
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

  <div class="col-lg-6 col-md-6 col-sm-6">
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
[64-bit `FILETIME`][6], when 32-bit systems were still not mature yet.
And this `FILETIME` stores UTC time, not local time which is still dominant
in system time of current Windows. That saved lots of headache when
constructing event timeline.

[1]: https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/strftime-wcsftime-strftime-l-wcsftime-l
[2]: http://science.ksc.nasa.gov/software/winvn/userguide/3_1_4.htm
[3]: https://en.wikipedia.org/wiki/Tz_database
[4]: https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/tzset
[5]: https://en.wikipedia.org/wiki/Daylight_saving_time
[6]: https://support.microsoft.com/en-us/kb/188768
[7]: https://developer.gnome.org/glib/stable/glib-GDateTime.html

<hr class="small"/>

| Date | ChangeLog |
| --- | --- |
| 2015-05-28 | Add description about problem in `_timeb` |
| 2019-06-04 | Use of `GDateTime` to replace the whole mess |
{: .table}

[^1]: `TZ` variable used to be a common mechanism to
      [set time zone for Windows 3.1][2]. Same applies to ancient Linux
      systems.

[^2]: Format of `TZ` variable is [documented in `_tzset()` function][4].
      *However*, it doesn't mention the behavior if supplied value does
      not satisfy documented format. In fact virtually infinite randomly
      invented values would be accepted.
