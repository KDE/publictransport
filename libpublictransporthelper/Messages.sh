#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.ui` >> rc.cpp
$XGETTEXT *.cpp *.h -o $podir/libpublictransporthelper.pot
rm -f rc.cpp
