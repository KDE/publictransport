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

#include "timetableaccessor_rmv.h"
#include <QRegExp>
#include <QtXml>

QList< DepartureInfo > TimetableAccessorRmv::parseDocument(const QString& document)
{
    QList< DepartureInfo > journeys;

    QDomDocument domDoc;
    domDoc.setContent(document);
    QDomElement docElement = domDoc.documentElement();
    QDomNodeList journeyNodes = docElement.elementsByTagName("Journey");
    for ( int i = 0; i < journeyNodes.count(); ++i )
    {
	QString sTime, sDelay, sTarget, sLine;
	QDomNode node = journeyNodes.at(i);

	sLine = node.firstChildElement("Product").attributeNode("name").nodeValue();
	QRegExp rx("[0-9]+");
	rx.indexIn(sLine);
	int iLine = rx.cap().toInt();
	qDebug() << "LINE= " << sLine << " toInt() = " << iLine << " ||| " << rx.cap();
	
	QDomElement stop = node.firstChildElement("MainStop");
	stop= stop.firstChildElement("BasicStop");
	stop= stop.firstChildElement("Dep");
// 	qDebug() << (stop.isNull() ? "NULL!!!" : "OK");
// 	qDebug() << stop.text();
	
	sTime = stop.firstChildElement("Time").text();
	sDelay = stop.firstChildElement("Delay").text();
// 	qDebug() << "TIME = " << sTime << ", DELAY = " << sDelay;

	QDomElement journeyAttribute = node.firstChildElement("JourneyAttributeList").firstChildElement("JourneyAttribute");
	while( !journeyAttribute.isNull() )
	{
	    QDomElement attribute = journeyAttribute.firstChildElement("Attribute");
	    if ( attribute.attributeNode("type").nodeValue() == "DIRECTION" )
	    {
		QDomElement direction = attribute.firstChildElement("AttributeVariant").firstChildElement("Text");
		sTarget = direction.text();
// 		qDebug() << "TARGET = " << sTarget;
		break;
	    }
	    
	    journeyAttribute = journeyAttribute.nextSiblingElement("JourneyAttribute");
	}

	DepartureInfo departureInfo(DepartureInfo::Tram, iLine, false, sTarget, QTime::fromString(sTime, "hh:mm"));
	departureInfo.setLineString(sLine);
	journeys.append( departureInfo );
    }
    
    return journeys;
}

QString TimetableAccessorRmv::rawUrl()
{
    QString sTime = QTime::currentTime().toString("hh:mm");
    
    return
    "http://www.rmv.de/auskunft/bin/jp/stboard.exe/dn?L=vs_rmv.vs_sq&selectDate=today&time=" + sTime + "&input=%1 %2&maxJourneys=10&boardType=dep&productsFilter=1111111111100000&maxStops=1&output=xml&start=yes";
}

