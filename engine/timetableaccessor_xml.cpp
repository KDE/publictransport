/*
 *   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#include <QRegExp>
#include <QDomDocument>

#include <KLocalizedString>
#include <KDebug>


TimetableAccessorXml::TimetableAccessorXml( const TimetableAccessorInfo &info )
		: m_accessorHTML( new TimetableAccessorHtml( info ) )
{
	m_info = info;
}

TimetableAccessorXml::~TimetableAccessorXml()
{
	delete m_accessorHTML;
}

QStringList TimetableAccessorXml::features() const
{
// 	list << i18nc("Support for getting information about the type of vehicle of a journey with public transport. This string is used in a feature list, should be short.", "Type of vehicle");
	return QStringList() << "Autocompletion" << "Delay" << "Platform"
						 << "Type of vehicle" << "Journey news" << "Stop ID";
}

bool TimetableAccessorXml::parseDocument( const QByteArray &document,
		QList<PublicTransportInfo*> *journeys, GlobalTimetableInfo *globalInfo,
		ParseDocumentMode parseDocumentMode )
{
	Q_UNUSED( globalInfo );
	Q_UNUSED( parseDocumentMode );

	if ( document.isEmpty() ) {
		qDebug() << "TimetableAccessorXml::parseDocument" << "XML document is empty";
		return false;
	}

	QString doc = QString( document );
	QDomDocument domDoc;
	domDoc.setContent( doc );
	QDomElement docElement = domDoc.documentElement();
	QDomNodeList errNodes = docElement.elementsByTagName( "Err" );
	if ( !errNodes.isEmpty() ) {
		QDomElement errElement = errNodes.item( 0 ).toElement();
		QString errCode = errElement.attributeNode("code").nodeValue();
		QString errMessage = errElement.attributeNode("text").nodeValue();
		QString errLevel = errElement.attributeNode("level").nodeValue();
		kDebug() << "TimetableAccessorXml::parseDocument" << "Received an error:"
				 << errCode << errMessage << "level" << errLevel
				 << ( errLevel.toLower() == "e" ? "Error is fatal" : "Error isn't fatal" );
		if ( errLevel.toLower() == "e" ) {
			return false;
		}
	}

	qDebug() << "TimetableAccessorXml::parseDocument" << "Parsing...";
	QDomNodeList journeyNodes = docElement.elementsByTagName("Journey");
	int count = journeyNodes.count();
	for ( int i = 0; i < count; ++i ) {
		QString sTime, sDelay, sDirection, sLine, sVehicleType, sPlatform, sJourneyNews;
		QDomNode node = journeyNodes.at( i );

		sLine = node.firstChildElement("Product").attributeNode("name").nodeValue().trimmed();
		sLine = sLine.replace( "tram", QChar(), Qt::CaseInsensitive ).trimmed();
		sJourneyNews = node.firstChildElement("InfoTextList").firstChildElement("InfoText")
						   .attributeNode("text").nodeValue();

		QDomElement stop = node.firstChildElement("MainStop");
		stop = stop.firstChildElement("BasicStop").firstChildElement("Dep");

		sTime = stop.firstChildElement("Time").text();
		sDelay = stop.firstChildElement("Delay").text();
		sPlatform = stop.firstChildElement("Platform").text();

		QDomElement journeyAttribute = node.firstChildElement("JourneyAttributeList")
										   .firstChildElement("JourneyAttribute");
		while ( !journeyAttribute.isNull() ) {
			QDomElement attribute = journeyAttribute.firstChildElement("Attribute");
			if ( attribute.attributeNode("type").nodeValue() == "DIRECTION" ) {
				sDirection = attribute.firstChildElement("AttributeVariant")
									  .firstChildElement("Text").text();
			} else if ( attribute.attributeNode("type").nodeValue() == "CATEGORY" ) {
				QDomElement category = attribute.firstChildElement( "AttributeVariant" );
				while ( !category.isNull() ) {
					if ( category.attributeNode("type").nodeValue() == "NORMAL" ) {
						sVehicleType = category.firstChildElement("Text").text();
						break;
					}
					category = category.nextSiblingElement("AttributeVariant");
				}
			}

			journeyAttribute = journeyAttribute.nextSiblingElement("JourneyAttribute");
		}

		if ( sDelay.isEmpty() ) {
			journeys->append( new DepartureInfo( sLine,
					DepartureInfo::getVehicleTypeFromString(sVehicleType),
					sDirection, QTime::currentTime(), QTime::fromString( sTime, "hh:mm" ),
					false, false, sPlatform, -1, "", sJourneyNews ) );
		} else {
			journeys->append( new DepartureInfo( sLine,
					DepartureInfo::getVehicleTypeFromString(sVehicleType),
					sDirection, QTime::currentTime(), QTime::fromString( sTime, "hh:mm" ),
					false, false, sPlatform, sDelay.toInt(), "", sJourneyNews ) );
		}
	}

	return count > 0;
}

bool TimetableAccessorXml::parseDocumentPossibleStops( const QByteArray &document,
		QStringList *stops, QHash<QString, QString> *stopToStopId,
		QHash<QString, int> *stopToStopWeight )
{
	// Let the document get parsed for possible stops by the HTML accessor
	return m_accessorHTML->parseDocumentPossibleStops( document, stops,
	        stopToStopId, stopToStopWeight );
}

QString TimetableAccessorXml::departuresRawUrl() const
{
	return m_info.departureRawUrl();
}

QString TimetableAccessorXml::stopSuggestionsRawUrl() const   // TODO: needed?
{
	return m_info.stopSuggestionsRawUrl();
}
