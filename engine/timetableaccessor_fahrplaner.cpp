/*
 *   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "timetableaccessor_fahrplaner.h"
#include <QRegExp>

QString TimetableAccessorFahrplaner::rawUrl()
{
    return "http://www.fahrplaner.de/hafas/stboard.exe/dn?ld=web&L=vbn&input=%1 %2&boardType=dep&time=actual&showResultPopup=popup&disableEquivs=no&maxJourneys=20&start=yes";
}

DepartureInfo TimetableAccessorFahrplaner::getInfo ( QRegExp rx )
{
    QString sType = rx.cap( 1 );
    QString sLine = rx.cap( 2 );
    QString sDirection = rx.cap( 3 );
    QString sDepHour = rx.cap( 4 );
    QString sDepMinute = rx.cap( 5 );
    
    return DepartureInfo(sLine, sType == "Str" ? Tram : Bus, sDirection, QTime(sDepHour.toInt(), sDepMinute.toInt()), sLine.at(0) == 'N');
}

QString TimetableAccessorFahrplaner::regExpSearch()
{
    return "(?:<td class=\"nowrap\">\\s*<span style=\".+\">\\s*)(Str|Bus).*(N?[0-9]+)(?:\\s*</span>\\s*</td>\\s*<td class=\"nowrap\">\\s*<span style=\".+\">\\s*)(\\w+.*\\w+)(?:\\s*(?:<br />\\s*<img .+ />&nbsp;\\s*<span class=\"him\">\\s*<span class=\"bold\">.*</span>.*</span>\\s*)?</span>\\s*</td>\\s*<td>\\s*<span style=\".+\">&nbsp;)([0-9]{2})(?::)([0-9]{2})(?:&nbsp;</span></td>\\s*</tr>)";
	// Matches: Str|Bus, Line, Target, Departure Hour, Departure Minute
}

