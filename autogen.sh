#!/bin/sh -ev

glib-gettextize --copy --force

autoreconf -f -i -v
