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

#include "timetableaccessor_xml.h"
// #include "timetableaccessor_html_infos.h"
#include <QRegExp>
#include <QtXml>
#include <KLocalizedString>


TimetableAccessorXml::TimetableAccessorXml( TimetableAccessorInfo info )
    : m_accessorHTML(0)
{
    m_info = info;
    m_accessorHTML = new TimetableAccessorHtml( m_info );
}

QStringList TimetableAccessorXml::features() const
{
// 	list << i18nc("Support for getting information about the type of vehicle of a journey with public transport. This string is used in a feature list, should be short.", "Type of vehicle");

    return QStringList() << i18nc("Autocompletion for names of public transport stops", "Autocompletion")
	<< i18nc("Support for getting delay information. This string is used in a feature list, should be short.", "Delay")
	<< i18nc("Support for getting the information from which platform a public transport vehicle departs / at which it arrives. This string is used in a feature list, should be short.", "Platform")
	<< i18nc("Support for getting information about the type of vehicle of a journey with public transport. This string is used in a feature list, should be short.", "Type of vehicle")
	<< i18nc("Support for getting the news about a journey with public transport, such as a platform change. This string is used in a feature list, should be short.", "Journey news")
	<< i18nc("Support for getting the id of a stop of public transport. This string is used in a feature list, should be short.", "Stop ID");
}

bool TimetableAccessorXml::parseDocument( QList<PublicTransportInfo*> *journeys, ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED( parseDocumentMode );

    if ( m_document.isEmpty() ) {
	qDebug() << "TimetableAccessorXml::parseDocument" << "XML document is empty";
	return false;
    }

    QString document = QString(m_document);
    QDomDocument domDoc;
    domDoc.setContent(document);
    QDomElement docElement = domDoc.documentElement();
    QDomNodeList errNodes = docElement.elementsByTagName("Err");
    if ( !errNodes.isEmpty() ) {
	QDomElement errElement = errNodes.item(0).toElement();
	QString errCode = errElement.attributeNode("code").nodeValue();
	QString errMessage = errElement.attributeNode("text").nodeValue();
	QString errLevel = errElement.attributeNode("level").nodeValue();
	qDebug() << "TimetableAccessorXml::parseDocument" << "Received an error:" << errCode << errMessage << "level" << errLevel << (errLevel.toLower() == "e" ? "Error is fatal" : "Error isn't fatal");
	if ( errLevel.toLower() == "e" )
	    return false;
    }

    qDebug() << "TimetableAccessorXml::parseDocument" << "Parsing...";
    QDomNodeList journeyNodes = docElement.elementsByTagName("Journey");
    int count = journeyNodes.count();
    for ( int i = 0; i < count; ++i ) {
	QString sTime, sDelay, sDirection, sLine, sVehicleType, sPlatform, sJourneyNews;
	QDomNode node = journeyNodes.at(i);

	sLine = node.firstChildElement("Product").attributeNode("name").nodeValue().trimmed();
	sLine = sLine.replace("tram", "", Qt::CaseInsensitive).trimmed();
	sJourneyNews = node.firstChildElement("InfoTextList").firstChildElement("InfoText").attributeNode("text").nodeValue();

	QDomElement stop = node.firstChildElement("MainStop");
	stop= stop.firstChildElement("BasicStop").firstChildElement("Dep");

	sTime = stop.firstChildElement("Time").text();
	sDelay = stop.firstChildElement("Delay").text();
	sPlatform = stop.firstChildElement("Platform").text();

	QDomElement journeyAttribute = node.firstChildElement("JourneyAttributeList").firstChildElement("JourneyAttribute");
	while( !journeyAttribute.isNull() )
	{
	    QDomElement attribute = journeyAttribute.firstChildElement("Attribute");
	    if ( attribute.attributeNode("type").nodeValue() == "DIRECTION" ) {
		sDirection = attribute.firstChildElement("AttributeVariant").firstChildElement("Text").text();
	    } else if ( attribute.attributeNode("type").nodeValue() == "CATEGORY" ) {
		QDomElement category = attribute.firstChildElement("AttributeVariant");
		while( !category.isNull() ) {
		    if ( category.attributeNode("type").nodeValue() == "NORMAL" ) {
			sVehicleType = category.firstChildElement("Text").text();
			break;
		    }
		    category = category.nextSiblingElement("AttributeVariant");
		}
	    }

	    journeyAttribute = journeyAttribute.nextSiblingElement("JourneyAttribute");
	}

// 	qDebug() << "TIME =" << sTime << ", LINE =" << sLine << ", DIRECTION =" << sDirection << ", DELAY =" << sDelay;

	if ( sDelay.isEmpty() )
	    journeys->append( new DepartureInfo( sLine, DepartureInfo::getVehicleTypeFromString(sVehicleType), sDirection, QTime::currentTime(), QTime::fromString(sTime, "hh:mm"), false, false, sPlatform, -1, "", sJourneyNews ) );
	else
	    journeys->append( new DepartureInfo( sLine, DepartureInfo::getVehicleTypeFromString(sVehicleType), sDirection, QTime::currentTime(), QTime::fromString(sTime, "hh:mm"), false, false, sPlatform, sDelay.toInt(), "", sJourneyNews ) );
    }

    return count > 0;
}

bool TimetableAccessorXml::parseDocumentPossibleStops ( QStringList *stops,
			    QHash<QString,QString> *stopToStopId ) {
    // Let the document get parsed for possible stops by the HTML accessor
    return m_accessorHTML->parseDocumentPossibleStops( m_document, stops, stopToStopId );
}

QString TimetableAccessorXml::departuresRawUrl() const {
    return m_info.departureRawUrl();
}

QString TimetableAccessorXml::stopSuggestionsRawUrl() const { // TODO: needed?
    return m_info.stopSuggestionsRawUrl();
}

