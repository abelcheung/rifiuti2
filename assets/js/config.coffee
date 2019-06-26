---
---
require.config

  baseUrl: `'{{ "assets/js" | relative_url }}'`

  shim:
    bootstrap:
      deps: ['jquery']

  # http://plnkr.co/edit/kzqLjUThJRtoEruCCtMt?p=preview
  #
  onNodeCreated: (node, config, module, path) ->
    sri =
      jquery: 'sha256-ZosEbRLbNQzLpnKIkEdrPv7lOy9C27hHQ+Xp8a4MxAQ='
      jqueryui: 'sha384-Dziy8F2VlJQLMShA6FHWNul/veM9bCkRUaLqr199K94ntO5QUrLJBEbYegdSkkqX'
      bootstrap: 'sha384-aJ21OjlMXNL5UyIl/XNwTMqvzeRMZH2w8c5cRVpzpU8Y5bApTppSuUkhZXN0VxHd'
      'smooth-scroll': 'sha384-ymcePxFhqHnKRIFPVh6sf/gPPGCGNRQ028+QOgRk0pucOhhOsoyVuX6l82Gd3+YD'
    if sri[module]
      node.setAttribute 'integrity', sri[module]
      node.setAttribute 'crossorigin', 'anonymous'

  paths:
    jquery: '//ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min'
    jqueryui: '//ajax.googleapis.com/ajax/libs/jqueryui/1.12.1/jquery-ui.min'
    bootstrap: '//stackpath.bootstrapcdn.com/bootstrap/3.4.1/js/bootstrap.min'
    'smooth-scroll': '//cdnjs.cloudflare.com/ajax/libs/jquery-smooth-scroll/2.2.0/jquery.smooth-scroll.min'

# vim: set sw=2 ts=2 sts=-1 et :
