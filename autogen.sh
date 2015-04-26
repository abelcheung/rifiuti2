#!/bin/sh -ev

glib-gettextize --copy --force

AUTOMAKE="automake --foreign" autoreconf -f -i -v
