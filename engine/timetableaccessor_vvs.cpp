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

#include "timetableaccessor_vvs.h"
#include <QRegExp>
#include <QDebug>

/*
QList< DepartureInfo > TimetableAccessorVvs::parseDocument( const QString &document )
{
    QList< DepartureInfo > journeys;
    */
  //  QString sFind = "(?:<tr><td class=\"center\" /><td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*</td><td class=\".*\" style=\".*\"><div style=\".*\"><img src=\".*\" .* title=\")(.*)(?:\" border=.*/></div><div style=\".*\">\\s*)(\\w*\\s*[0-9]+)(?:\\s*</div></td><td>\\s*)(.*)(?:\\s*</td><td>.*</td><td>.*</td></tr>)";
	// Matches: Departure Hour, Departure Minute, Product, Line, Target
/*
    qDebug() << "Parsing...";
    QRegExp rx(sFind, Qt::CaseInsensitive);
    rx.setMinimal(true);
    int pos = 0;
    while ((pos = rx.indexIn(document, pos)) != -1) {
	if (!rx.isValid())
	{
	    qDebug() << "Parse error";
	    continue; // parse error
	}

	QString sType = rx.cap( 3 );
	QString sLine = rx.cap( 4 );
	QString sTarget = rx.cap( 5 );
	QString sDepHour = rx.cap( 1 );
	QString sDepMinute = rx.cap( 2 );
	QRegExp rxNumber("[0-9]+");
	rxNumber.indexIn(sLine);
	int iLine = rxNumber.cap().toInt();
	// qDebug() << sType << ", " << sLine << ", " << sTarget << ", " << sDepHour;

	DepartureInfo::LineType lineType = DepartureInfo::Unknown;
	if ( sType == "U-Bahn" )
	    lineType = DepartureInfo::Subway;
	else if ( sType == "S-Bahn" )
	    lineType = DepartureInfo::Tram;
	else if ( sType == "Bus" )
	    lineType = DepartureInfo::Bus;
	DepartureInfo departureInfo(lineType, iLine, sLine.isEmpty() ? false : sLine.at(0) == 'N', sTarget, QTime(sDepHour.toInt(), sDepMinute.toInt()));
	departureInfo.setLineString(sLine);
	journeys.append( departureInfo );

	pos  += rx.matchedLength();
    }
    
    return journeys;
}
*/

QString TimetableAccessorVvs::rawUrl()
{
    return "http://www2.vvs.de/vvs/XSLT_DM_REQUEST?language=de&type_dm=stop&mode=direct&place_dm=%1&name_dm=%2&deleteAssignedStops=1";
}

DepartureInfo TimetableAccessorVvs::getInfo ( QRegExp rx )
{
    QString sType = rx.cap( 3 );
    QString sLine = rx.cap( 4 );
    QString sDirection = rx.cap( 5 );
    QString sDepHour = rx.cap( 1 );
    QString sDepMinute = rx.cap( 2 );
    QRegExp rxNumber("[0-9]+");
    rxNumber.indexIn(sLine);
    int iLine = rxNumber.cap().toInt();
    // qDebug() << sType << ", " << sLine << ", " << sTarget << ", " << sDepHour;

    LineType lineType = Unknown;
    if ( sType == "U-Bahn" )
	lineType = Subway;
    else if ( sType == "S-Bahn" )
	lineType = Tram;
    else if ( sType == "Bus" )
	lineType = Bus;
    
    return DepartureInfo(sLine, lineType, sDirection, QTime(sDepHour.toInt(), sDepMinute.toInt()), sLine.isEmpty() ? false : sLine.at(0) == 'N');
}

QString TimetableAccessorVvs::regExpSearch()
{
    return "(?:<tr><td class=\"center\" /><td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*</td><td class=\".*\" style=\".*\"><div style=\".*\"><img src=\".*\" .* title=\")(.*)(?:\" border=.*/></div><div style=\".*\">\\s*)(\\w*\\s*[0-9]+)(?:\\s*</div></td><td>\\s*)(.*)(?:\\s*</td><td>.*</td><td>.*</td></tr>)";
	// Matches: Departure Hour, Departure Minute, Product, Line, Target
}

