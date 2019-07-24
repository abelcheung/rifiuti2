---
---

define ['jquery','jqueryui','jqSmoothScroll','domReady!'], ($) ->

  ###
  Open external link in new tab. Discussion about rel=noopener:
  https://mathiasbynens.github.io/rel-noopener/
  ###

  $('a').filter ->
    this.hostname and ( this.hostname isnt location.hostname )
  .attr "target", "_blank"
  .attr "rel", "noopener"

  ###
  Add icon to main content external links
  ###
  $('.main-content a[target="_blank"]')
  .not ".btn-download" # revisit later
  .addClass "link-icon link-external"

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

  Beware that changing address bar anchor target also requires suitable
  workaround in CSS. Changing URL in address bar would cause another
  jump, thus:
  - Must be used in afterScroll
  - And as last statement, after all animations are done
  - offset value must match those in CSS

  Here is the CSS part:
  http://nicolasgallagher.com/jump-links-and-viewport-positioning/demo/#method-C
  ###
  $ 'a[href^="#"]'
  .not '[href="#"]'
  .not '.btn-tag'
  .smoothScroll
    offset   : - $('.navbar').height() - 10
    autoFocus: true
    speed    : 'auto'
    afterScroll: (options) ->
      unescapeSelector = (str) ->
        str.replace /\\([:\.\/])/g, '$1'
      id = options.scrollTarget
      $ id
        .addClass "bg-danger"
        .removeClass "bg-danger", 2000
      location.hash = unescapeSelector id

# vim: set sw=2 ts=2 sts=-1 et :
