---
---

define ['jquery','jqSmoothScroll','domReady!'], ($) ->

  ###
  Open external link in new tab. Discussion about rel=noopener:
  https://mathiasbynens.github.io/rel-noopener/
  ###

  $('a').filter ->
    this.hostname and ( this.hostname isnt location.hostname )
  .addClass "link-icon link-external"
  .attr "target", "_blank"
  .attr "rel", "noopener"

  ###
  Mark links of specific file types with CSS styling
  ###

  [ "pdf", "zip", "xz", "xlsx", "docx" ]
  .forEach (e) ->
    $("a[href$='.#{e}']")
    .addClass "link-icon link-#{e}"

  ###
  Smooth scrolling, useful for footnote jumping. Tag page excluded.
  Uses kswedberg/jquery-smooth-scroll
  ###
  $('a[href^="#"]')
  .not('[href="#"]')
  .not('.btn-tag').smoothScroll
    offset   : - $('.navbar').height() - 10
    autoFocus: true
    speed    : 'auto'


# vim: set sw=2 ts=2 sts=-1 et :
