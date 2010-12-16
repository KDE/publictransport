#! /usr/bin/env bash
$EXTRACTRC config/*.ui >> rc.cpp
$XGETTEXT *.cpp *.h config/*.cpp config/*.h -o $podir/plasma_runner_publictransport.pot
