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

#include "timetableaccessor_bvg.h"
#include <QRegExp>
#include <QDebug>
/*
QList< DepartureInfo > TimetableAccessorBvg::parseDocument(const QString& document)
{
    QList< DepartureInfo > journeys;*/

  //  QString sFind = "(?:<tr class=\"\">\\s*<td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*</td>\\s*<td>\\s*<img src=\".*\" class=\"ivuTDProductPicture\" alt=\".*\"\\s*class=\"ivuTDProductPicture\" />)(\\w{1,10})(?:\\s*)((\\w*\\s*)?[0-9]+)(?:\\s*</td>\\s*<td>\\s*<a class=\"ivuLink\" href=\".*\" title=\".*\">)(.*)(?:</a>\\s*</td>\\s*<td>\\s*<!-- .* -->\\s*<a class=\"ivuLink\" href=\".*\" title=\".*\">[0-9]+</a>\\s*</td>\\s*</tr>)";
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
	QString sLine = rx.cap( 4 ).trimmed();
	QString sTarget = rx.cap( 5 );
	QString sDepHour = rx.cap( 1 );
	QString sDepMinute = rx.cap( 2 );
	QRegExp rxNumber("[0-9]+");
	rxNumber.indexIn(sLine);
	int iLine = rxNumber.cap().toInt();
	qDebug() << sType << ", " << sLine << ", " << sTarget << ", " << sDepHour << ":" << sDepMinute;

	DepartureInfo::LineType lineType = DepartureInfo::Unknown;
	if ( sType == "U-Bahn" )
	    lineType = DepartureInfo::Subway;
	else if ( sType == "S-Bahn" )
	    lineType = DepartureInfo::Tram;
	else if ( sType == "Tram" )
	    lineType = DepartureInfo::Tram;
	else if ( sType == "Bus" )
	    lineType = DepartureInfo::Bus;
	DepartureInfo departureInfo(lineType, iLine, sLine.isEmpty() ? false : sLine.at(0) == 'N', sTarget, QTime(sDepHour.toInt(), sDepMinute.toInt()));
	departureInfo.setLineString(sLine);
	journeys.append( departureInfo );

	pos  += rx.matchedLength();
    }
    
    return journeys;
}*/

QString TimetableAccessorBvg::rawUrl()
{
    return "http://www.fahrinfo-berlin.de/IstAbfahrtzeiten/index;ref=3?input=%2+(%1)&submit=Anzeigen";
}

DepartureInfo TimetableAccessorBvg::getInfo ( QRegExp rx )
{
    QString sType = rx.cap( 3 );
    QString sLine = rx.cap( 4 ).trimmed();
    QString sTarget = rx.cap( 5 );
    QString sDepHour = rx.cap( 1 );
    QString sDepMinute = rx.cap( 2 );
    QRegExp rxNumber("[0-9]+");
    rxNumber.indexIn(sLine);
    int iLine = rxNumber.cap().toInt();
    qDebug() << sType << ", " << sLine << ", " << sTarget << ", " << sDepHour << ":" << sDepMinute;

    DepartureInfo::LineType lineType = DepartureInfo::Unknown;
    if ( sType == "U-Bahn" )
	lineType = DepartureInfo::Subway;
    else if ( sType == "S-Bahn" )
	lineType = DepartureInfo::Tram;
    else if ( sType == "Tram" )
	lineType = DepartureInfo::Tram;
    else if ( sType == "Bus" )
	lineType = DepartureInfo::Bus;
    DepartureInfo departureInfo(lineType, iLine, sLine.isEmpty() ? false : sLine.at(0) == 'N', sTarget, QTime(sDepHour.toInt(), sDepMinute.toInt()));
    departureInfo.setLineString(sLine);
    
    return departureInfo;
}

QString TimetableAccessorBvg::regExpSearch()
{
    return "(?:<tr class=\"\">\\s*<td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*</td>\\s*<td>\\s*<img src=\".*\" class=\"ivuTDProductPicture\" alt=\".*\"\\s*class=\"ivuTDProductPicture\" />)(\\w{1,10})(?:\\s*)((\\w*\\s*)?[0-9]+)(?:\\s*</td>\\s*<td>\\s*<a class=\"ivuLink\" href=\".*\" title=\".*\">)(.*)(?:</a>\\s*</td>\\s*<td>\\s*<!-- .* -->\\s*<a class=\"ivuLink\" href=\".*\" title=\".*\">[0-9]+</a>\\s*</td>\\s*</tr>)";
	// Matches: Departure Hour, Departure Minute, Product, Line, Target
}

