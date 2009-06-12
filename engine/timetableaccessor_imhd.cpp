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

#include "timetableaccessor_imhd.h"
#include <QRegExp>

QString TimetableAccessorImhd::rawUrl()
{
    // only Bratislava
    return "http://www.imhd.zoznam.sk/ba/index.php?w=212b36213433213aef2f302523ea&lang=en&hladaj=%1";
}

DepartureInfo TimetableAccessorImhd::getInfo ( QRegExp rx )
{
  // Linien-Typ: [line] [type (=Bus|Express Bus|Night Line - Bus)
  // "(?:<tr><td class="tab0"><center><b>)(N?[0-9]+)(?:</b></center></td><td><center>)(Night Line - Bus)(?:</center></td><td><b><a href="index.php?w=3023ef2f302523ea&l=480&x=838861199396&lang=en&lang=en">)(.*)(?:</a></b></td><td><center><a href="javascript:Mapa(30576);"><img src="../_/cepo/mapa.gif" style="border:0" alt="Show on map"></a></center></td></tr>)"
//     QString sType = rx.cap( 1 );
    QString sLine = rx.cap( 3 );
    QString sDirection = rx.cap( 4 );
    QString sDepHour = rx.cap( 1 );
    QString sDepMinute = rx.cap( 2 );

    return DepartureInfo( sLine, Unknown, sDirection, QTime(sDepHour.toInt(), sDepMinute.toInt()), sLine.at(0) == 'N' );
}

QString TimetableAccessorImhd::regExpSearch()
{
    return "(?:<tr><td><b>)([0-9]{2})(?:\\.)([0-9]{2})(?:</b></td><td><center><b><em>)(N?[0-9]+)(?:</em></b></center></td><td>)(.*)(?:</td></tr>)";
	// Matches: Departure Hour, Departure Minute, Line, Target
}

