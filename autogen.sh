#! /bin/sh -e

[ ! -d m4/ ] && mkdir -p m4
autoreconf --verbose --install --force
