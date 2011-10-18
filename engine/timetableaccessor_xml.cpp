/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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


TimetableAccessorXml::TimetableAccessorXml( TimetableAccessorInfo *info )
{
    m_info = info;

    // Create a script accessor object to parse stop suggestions if a script filename is given
    if ( !m_info->scriptFileName().isEmpty() ) {
        m_accessorScript = new TimetableAccessorScript( info );
    } else {
        m_accessorScript = NULL;
    }
}

TimetableAccessorXml::~TimetableAccessorXml()
{
    m_info = NULL; // Don't delete m_info in ~TimetableAccessor()
    delete m_accessorScript; // This also deletes m_info
}

QStringList TimetableAccessorXml::features() const
{
//     list << i18nc("Support for getting information about the type of vehicle of a journey with public transport. This string is used in a feature list, should be short.", "Type of vehicle");
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
        kDebug() << "XML document is empty";
        return false;
    }

    QString doc = TimetableAccessorScript::decodeHtml( document, m_info->fallbackCharset() );
    QDomDocument domDoc;
    domDoc.setContent( doc );
    QDomElement docElement = domDoc.documentElement();

    // Errors are inside the <Err> tag
    QDomNodeList errNodes = docElement.elementsByTagName( "Err" );
    if ( !errNodes.isEmpty() ) {
        QDomElement errElement = errNodes.item( 0 ).toElement();
        QString errCode = errElement.attributeNode("code").nodeValue();
        QString errMessage = errElement.attributeNode("text").nodeValue();
        QString errLevel = errElement.attributeNode("level").nodeValue();
        // Error codes:
        // - H890: No trains in result (fatal)
        kDebug() << "Received an error:" << errCode << errMessage << "level" << errLevel
                 << (errLevel.toLower() == "e" ? "Error is fatal" : "Error isn't fatal");
        if ( errLevel.toLower() == "e" ) {
            // Error is fatal, don't proceed
            return false;
        }
    }

    // Use date of the first departure (inside <StartT>) as date for newly parsed departures.
    // If a departure is more than 3 hours before the last one, it is assumed that the new
    // departure is one day later, eg. read departure one at 23:30, next departure is at 0:45,
    // assume it's at the next day.
    QDate currentDate;
    QDomNodeList list = docElement.elementsByTagName( "StartT" );
    if ( list.isEmpty() ) {
        kDebug() << "No <StartT> tag found in the received XML document";
    } else {
        QDomElement startTimeElement = docElement.elementsByTagName( "StartT" ).at( 0 ).toElement();
        if ( startTimeElement.hasAttribute("date") ) {
            currentDate = QDate::fromString( startTimeElement.attribute("date"), "yyyyMMdd" );
            if ( !currentDate.isValid() ) {
                currentDate = QDate::currentDate();
            }
        } else {
            currentDate = QDate::currentDate();
        }
    }

    // Find all <Journey> tags, which contain information about a departure/arrival
    QDomNodeList journeyNodes = docElement.elementsByTagName("Journey");
    int count = journeyNodes.count();
    QTime lastTime( 0, 0 );
    for ( int i = 0; i < count; ++i ) {
//         QString sTime, sDelay, sDirection, sLine, sVehicleType, sPlatform, sJourneyNews, sOperator;
        QDomNode node = journeyNodes.at( i );
        QHash<TimetableInformation, QVariant> departureInfo;

        // <Product> tag contains the line string
        QString line = node.firstChildElement("Product").attributeNode("name").nodeValue().trimmed();
        line = line.remove( "tram", Qt::CaseInsensitive ).trimmed();
        departureInfo.insert( TransportLine, line );

        // <InfoTextList> tag contains <InfoText> tags, which contain journey news
        QDomNodeList journeyNewsNodes = node.firstChildElement("InfoTextList").elementsByTagName("InfoText");
        int journeyNewsCount = journeyNewsNodes.count();
        QString journeyNews;
        for ( int j = 0; j < journeyNewsCount; ++j ) {
            QDomNode journeyNewsNode = journeyNewsNodes.at( i );
            QString newJourneyNews = journeyNewsNode.toElement().attributeNode("text").nodeValue();

            if ( !journeyNews.contains(newJourneyNews) ) {
                if ( !journeyNews.isEmpty() ) {
                    journeyNews += "<br />"; // Insert line breaks between journey news
                }
                journeyNews += newJourneyNews;
            }
        }
//         kDebug() << "journeyNews" << journeyNews;
        departureInfo.insert( JourneyNews, journeyNews );

        // <MainStop><BasicStop><Dep> tag contains some tags with more information
        // about the departing stop
        QDomElement stop = node.firstChildElement("MainStop")
                               .firstChildElement("BasicStop").firstChildElement("Dep");

        // <Time> tag contains the departure time
        QTime time = QTime::fromString( stop.firstChildElement("Time").text(), "hh:mm" );
        departureInfo.insert( DepartureHour, time.hour() );
        departureInfo.insert( DepartureMinute, time.minute() );

        // <Delay> tag contains delay
        QString sDelay = stop.firstChildElement("Delay").text();
        departureInfo.insert( Delay, sDelay.isEmpty() ? -1 : sDelay.toInt() );

        // <Platform> tag contains the platform
        departureInfo.insert( Platform, stop.firstChildElement("Platform").text() );

        // <PassList> tag contains stops on the route of the line, inside <BasicStop> tags
        QDomNodeList routeStopList = node.firstChildElement("PassList").elementsByTagName("BasicStop");
        int routeStopCount = routeStopList.count();
        QStringList routeStops;
        QVariantList routeTimes;
        for ( int r = 0; r < routeStopCount; ++r ) {
            QDomNode routeStop = routeStopList.at( r );

            routeStops << routeStop.firstChildElement("Location").firstChildElement("Station")
                    .firstChildElement("HafasName").firstChildElement("Text").text().trimmed();
            routeTimes << /*QTime::fromString(*/
                    routeStop.firstChildElement("Arr").firstChildElement("Time").text()/*, "hh:mm" )*/;
        }
        departureInfo.insert( RouteStops, routeStops );
        departureInfo.insert( RouteTimes, routeTimes );

        // Other information is found in the <JourneyAttributeList> tag which contains
        // a list of <JourneyAttribute> tags
        QDomElement journeyAttribute = node.firstChildElement("JourneyAttributeList")
                                           .firstChildElement("JourneyAttribute");
        while ( !journeyAttribute.isNull() ) {
            // Get the child tag <Attribute> and handle it based on the value of the "type" attribute
            QDomElement attribute = journeyAttribute.firstChildElement("Attribute");
            if ( attribute.attributeNode("type").nodeValue() == "DIRECTION" ) {
                // Read direction / target
                departureInfo.insert( Target, attribute.firstChildElement("AttributeVariant")
                        .firstChildElement("Text").text() );
            } else if ( attribute.attributeNode("type").nodeValue() == "CATEGORY" ) {
                // Read vehicle type from "category"
                QDomElement category = attribute.firstChildElement( "AttributeVariant" );
                while ( !category.isNull() ) {
                    if ( category.attributeNode("type").nodeValue() == "NORMAL" ) {
                        departureInfo.insert( TypeOfVehicle,
                                DepartureInfo::getVehicleTypeFromString(
                                category.firstChildElement("Text").text().trimmed()) );
                        break;
                    }
                    category = category.nextSiblingElement("AttributeVariant");
                }
            } else if ( attribute.attributeNode("type").nodeValue() == "OPERATOR" ) {
                // Read operator
                departureInfo.insert( Operator, attribute.firstChildElement("AttributeVariant")
                        .firstChildElement("Text").text() );
            } else if ( !departureInfo.contains(TransportLine)
                    && attribute.attributeNode("type").nodeValue() == "NAME" )
            {
                // Read line string if it wasn't read already
                departureInfo.insert( TransportLine, attribute.firstChildElement("AttributeVariant")
                        .firstChildElement("Text").text().trimmed() );
            } else if ( attribute.attributeNode("type").nodeValue() == "NORMAL" ) {
                // Read less important journey news
                QString info = attribute.firstChildElement("AttributeVariant")
                                        .firstChildElement("Text").text().trimmed();
                if ( !departureInfo[JourneyNews].toString().contains(info) ) {
                    QString journeyNews;
                    if ( departureInfo.contains(JourneyNews)
                            && !departureInfo[JourneyNews].toString().isEmpty() )
                    {
                        journeyNews = departureInfo[JourneyNews].toString().append("<br />");
                    }
                    journeyNews.append( info );
                    departureInfo.insert( JourneyNews, journeyNews );
                }
            } /*else {*/
//                 kDebug() << "Unhandled attribute type" << attribute.attributeNode("type").nodeValue()
//                          << "with text (tags stripped)" << attribute.text();
//             }

            // Go to the next journey attribute
            journeyAttribute = journeyAttribute.nextSiblingElement("JourneyAttribute");
        }

        // Parse time string
        if ( lastTime.secsTo(time) < -60 * 60 * 3 ) {
            // Add one day to the departure date
            // if the last departure time is more than 3 hours before the current departure time
            currentDate = currentDate.addDays( 1 );
        }
        departureInfo.insert( DepartureDate, currentDate );

        // Add departure to the journey list
        journeys->append( new DepartureInfo(departureInfo) );

        lastTime = time;
    }

    return count > 0;
}

bool TimetableAccessorXml::parseDocumentPossibleStops( const QByteArray &document,
        QList<StopInfo*> *stops )
{
    if ( m_accessorScript ) {
        // Let the document get parsed for possible stops by the script accessor
        return m_accessorScript->parseDocumentPossibleStops( document, stops );
    } else {
        return false;
    }
}

QString TimetableAccessorXml::departuresRawUrl() const
{
    return m_info->departureRawUrl();
}

QString TimetableAccessorXml::stopSuggestionsRawUrl() const
{
    return m_info->stopSuggestionsRawUrl();
}
