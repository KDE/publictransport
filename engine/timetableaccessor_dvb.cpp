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

#include "timetableaccessor_dvb.h"
#include <QRegExp>
#include <QDebug>

QString TimetableAccessorDvb::rawUrl()
{
    return "http://www.dvb.de:80/de/Fahrplan/Abfahrtsmonitor/abfahrten.do/%1#result";
}

DepartureInfo TimetableAccessorDvb::getInfo ( QRegExp rx )
{
    QString sDepHour = rx.cap( 1 );
    QString sDepMinute = rx.cap( 2 );
    QString sType = rx.cap( 3 );
    QString sLine = rx.cap( 4 );
    QString sDirection = rx.cap( 5 );

    LineType lineType = Unknown;
    if ( sType == "U-Bahn" )
	lineType = Subway;
    else if ( sType == "Straßenbahn" ) // "Stra&#223;enbahn"?
	lineType = Tram;
    else if ( sType == "Bus" )
	lineType = Bus;
    
    return DepartureInfo(sLine, lineType, sDirection, QTime(sDepHour.toInt(), sDepMinute.toInt()), sLine.isEmpty() ? false : sLine.at(0) == 'N');
}

QString TimetableAccessorDvb::regExpSearch()
{
    return "(?:<tr class=\".*\">\\s*<td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*.?\\s*</td>\\s*<td><img src=\".*\" title=\")(.*)(?:\" alt=\".*\" class=\".*\" /></td>\\s*<td>)(\\w*\\s*[0-9]+)(?:</td>\\s*<td>\\s*)(.*)(?:.*</td>\\s*</tr>)";
	// Matches: Departure Hour, Departure Minute, Product, Line, Target
}

