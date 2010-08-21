#! /usr/bin/env bash
$EXTRACTRC *.ui >> rc.cpp
$XGETTEXT *.cpp *.h -o $podir/plasma_applet_publictransport.pot
