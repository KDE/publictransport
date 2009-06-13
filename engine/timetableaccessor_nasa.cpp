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

#include "timetableaccessor_nasa.h"
#include <QRegExp>

QString TimetableAccessorNasa::rawUrl()
{
    return "http://www.nasa.de/delfi52/stboard.exe/dn?ld=web&L=vbn&input=%1 %2&boardType=dep&time=actual&showResultPopup=popup&disableEquivs=no&maxJourneys=20&start=yes";
}

DepartureInfo TimetableAccessorNasa::getInfo ( QRegExp rx )
{
    QString sDepHour = rx.cap( 1 );
    QString sDepMinute = rx.cap( 2 );
    QString sType = rx.cap( 3 );
    QString sLine = rx.cap( 4 );
    QString sDirection = rx.cap( 5 );
    
    return DepartureInfo(sLine, sType == "Str" ? Tram : Bus, sDirection, QTime(sDepHour.toInt(), sDepMinute.toInt()), sLine.at(0) == 'N');
}

QString TimetableAccessorNasa::regExpSearch()
{
    return "(?:<tr class=\".*\">\\s*<td class=\".*\">)([0-9]{2})(?::)([0-9]{2})(?:</td>\\s*<td class=\".*\">\\s*<a href=\"/delfi52/.*\"><img src=\".*\" width=\".*\" height=\".*\" alt=\")(.*)(?:\\s*.?\" style=\".*\">\\s*)(\\w*\\s*[0-9]+)(?:\\s*</a>\\s*</td>\\s*<td class=\".*\">\\s*<span class=\".*\">\\s*<a href=\"/delfi52/stboard.exe/dn.*>\\s*)(.*)(?:\\s*</a>\\s*</span>\\s*<br />\\s*<a href=\".*\">.*</a>.*</td>\\s*<td class=\".*\">.*<br />.*</td>\\s*</tr>)";
    // Matches: Departure Hour, Departure Minute, Product, Line, Direction
//... zwischenhalte:   <a href=\"/delfi52/stboard.exe/dn.*\">\\s*)(Halle(Saale)Hbf DB)(?:\\s*</a>\\s*)(21:03)
}

