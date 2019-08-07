---
layout: page
use-site-title: true
comments: false
shareable: false
slide:
  - image: /images/screenshot01.png
    desc : Default tab delimited output
  - image: /images/screenshot02.png
    desc : XML output of same file
  - image: /images/screenshot03.png
    desc : Unicode path and 8.3 legacy path
  - image: /images/screenshot04.png
    desc : Japanese Windows and Win95 extra metadata
---

<div class="jumbotron row">
<div class="col-xs-6 text-center">
  <a class="btn btn-primary btn-lg btn-download center-block" href="{{ '/releases/latest' | prepend: site.repourl }}" role="button">
    <div class="text-center">Download<br>Latest</div>
    <i class="fa-3x fa-inverse fas fa-download"></i>
  </a>
  <p>Windows 32/64 bit binary and source code</p>
</div>
<div class="col-xs-6 text-center">
  <a class="btn btn-primary btn-lg btn-download center-block" href="{{ 'news/' | relative_url }}" role="button">
    <div class="text-center">News &amp;<br>Announce</div>
    <i class="fa-3x fa-inverse fas fa-book-reader"></i>
  </a>
  <p>Latest: {{ site.posts[0].title }}</p>
</div>
</div>

![Recycle Bin full icon]({{ '/images/rbin.png' | relative_url }}){: .pull-right }

`Rifiuti2` analyse recycle bin files from Windows. Analysis of
Windows recycle bin is usually carried out during Windows computer
forensics. Rifiuti2 can extract file deletion time, original
path and size of deleted files. For more ancient versions of Windows,
it can also check whether deleted items were not in recycle bin anymore
(that is, either restored or permanently purged).

It is a [rewrite of `rifiuti`][1], originally written by FoundStone folks
for identical purpose. Later `rifiuti2` was extended to
cover more functionalities, such as:

* Handles oldest (Win95) to newest (Win 10 and Server 2019) recycle bin format
  * Windows 95 &ndash; 2003 uses a single index file named `INFO` or `INFO2`
  * Vista or above uses one index file for each deleted item
* 64-bit file size support
* Supports all localized versions of Windows &mdash; both Unicode-based
  ones and legacy ones (using ANSI code page)
* Supports output in XML format as well as original tab-delimited text
* Obscure features such as recycle bin on network share
  (`\\server\share`)

Latest features and bug fixes [are listed inside NEWS file][3]; you're
also welcome to [check out blog news][4] for more insight during
development and documentation stages.

[1]: {{ 'history/' | relative_url }}
[3]: {{ '/blob/master/NEWS.md' | prepend: site.repourl }}
[4]: {{ 'news/' | relative_url }}

# Usage

`Rifiuti2` is designed to be portable, and runs on command line environment.
Two programs `rifiuti` and `rifiuti-vista` are chosen depending on relevant
Windows recycle bin format.

Please consult manpage (Unix) or README.html (bundled with Windows binaries)
for complete options and detailed usage description. There are some
usage samples [on Github page][5] as well.

[5]: {{ '/blob/master/README.md' | prepend: site.repourl }}

# Screenshots

<div id="rifiuti2-screenshot" class="carousel slide hidden" data-interval="7000">
  {% comment %}<!-- Indicators -->{% endcomment %}
  <ol class="carousel-indicators">
  {%- for sl in page.slide %}
    <li data-target="#rifiuti2-screenshot" data-slide="next"{% if forloop.first %} class="active"{% endif %}></li>
  {%- endfor %}
  </ol>

  {% comment %}<!-- Wrapper for slides -->{% endcomment %}
  <div class="carousel-inner" role="listbox">
  {%- for sl in page.slide -%}
  {%- assign desc = sl.desc | default: "(no description)" %}
    <div class="item{% if forloop.first %} active{% endif %}">
      <img src="{{ sl.image | relative_url }}" alt="{{ desc }}">
      <div class="carousel-caption">{{ desc }}</div>
    </div>
  {%- endfor %}
  </div>

  {% comment %}<!-- Controls -->{% endcomment %}
  <a class="left carousel-control" href="#rifiuti2-screenshot" role="button" data-slide="prev">
    <span class="icon-prev" aria-hidden="true"></span>
    <span class="sr-only">Previous image</span>
  </a>
  <a class="right carousel-control" href="#rifiuti2-screenshot" role="button" data-slide="next">
    <span class="icon-next" aria-hidden="true"></span>
    <span class="sr-only">Next image</span>
  </a>
</div>

<noscript>
{%- for sl in page.slide -%}
  {%- assign desc = sl.desc | default: "(no description)" -%}
  {% include fullwidth-figure.html src=sl.image alt=desc %}
{%- endfor -%}
</noscript>
