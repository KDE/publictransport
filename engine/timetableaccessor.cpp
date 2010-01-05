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

// Qt includes
#include <QTextCodec>
#include <QtXml>

// KDE includes
#include <KIO/NetAccess>
#include <KDebug>
#include <KStandardDirs>

// Own includes
#include "timetableaccessor.h"
#include "timetableaccessor_html.h"
#include "timetableaccessor_xml.h"
#include "timetableaccessor_htmlinfo.h"
#include <KLocale>


TimetableAccessor::TimetableAccessor() {
}

TimetableAccessor::~TimetableAccessor() {
}

TimetableAccessor* TimetableAccessor::getSpecificAccessor( const QString &serviceProvider ) {
    QString fileName = KGlobal::dirs()->findResource( "data", QString("plasma_engine_publictransport/accessorInfos/%1.xml").arg(serviceProvider) );
    if ( fileName.isEmpty() ) {
	qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "Couldn't find a service provider information XML named" << serviceProvider;
	return NULL;
    }

    QFile file( fileName );
    file.open( QIODevice::ReadOnly );
    QByteArray document = file.readAll();
    file.close();
    if ( document.isEmpty() ) {
	qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "Found service provider information XML named" << serviceProvider << "but is empty."
	    << "Filename =" << fileName;
	return NULL;
    }

    // Get country code from filename
    QString country = "international";
    QRegExp rx("^([^_]+)");
    if ( rx.indexIn( serviceProvider ) != -1 && KGlobal::locale()->allCountriesList().contains(rx.cap()) )
	country = rx.cap();

    QDomDocument domDoc;
    domDoc.setContent(document);
    QDomElement docElement = domDoc.documentElement();
    QDomNodeList nodeList, subNodeList;
    QDomNode node, childNode, subChildNode, subSubChildNode;

    QString lang = KGlobal::locale()->country();

    //     <name>
    QString name, nameEn;
    node = docElement.firstChildElement("name");
    if ( node.isNull() ) {
	qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "No <name> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }
    while ( !node.isNull() ) {
	if ( !node.hasAttributes() || node.toElement().attribute("lang") == lang ) {
	    name = node.toElement().text();
	    break;
	}
	else if ( node.toElement().attribute("lang") == "en" )
	    nameEn= node.toElement().text();

	node = node.nextSiblingElement("name");
    }
    if ( name.isEmpty() )
	name = nameEn;

//     <author> <fullname>...</fullname><email>...</email> </author>
    QString author, email;
    node = docElement.firstChildElement("author");
    if ( !node.isNull() ) {
	author = node.firstChildElement("fullname").toElement().text();
	email = node.firstChildElement("email").toElement().text();
    }

//     <version>
    node = docElement.firstChildElement("version");
    if ( node.isNull() ) {
	qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "No <version> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }
    QString version = node.toElement().text();

//     <type>
    node = docElement.firstChildElement("type");
    if ( node.isNull() ) {
	qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "No <type> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }
    AccessorType type = accessorTypeFromString( node.toElement().text() );
    if ( type == NoAccessor ) {
	qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "The <type> tag in service provider information XML named" << serviceProvider
	    << "is invalid. Currently there are two values allowed: HTML and XML.";
	return NULL;
    }

//     <country> // DEPRECATED is now parsed from the filename
//     node = docElement.firstChildElement("country");
//     if ( node.isNull() ) {
// 	qDebug() << "TimetableAccessor::getSpecificAccessor"
// 	    << "No <country> tag in service provider information XML named" << serviceProvider;
// 	return NULL;
//     }
//     QString country = node.toElement().text();

//     <cities> <city>...</city> </cities>
    QStringList cities;
    QHash<QString, QString> hashCityNameReplacements;
    node = docElement.firstChildElement("cities");
    if ( !node.isNull() ) {
	nodeList = node.childNodes();
	for ( uint i = 0; i < nodeList.length(); ++i ) {
	    if ( nodeList.at(i).isComment() )
		continue;
	    if ( nodeList.at(i).toElement().tagName() != "city" ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		    << "Warninbg: Invalid child tag (not <city>) found in the <cities> tag in service provider information XML named"
		    << serviceProvider << "The name of the invalid tag is" << nodeList.at(i).toElement().tagName();
		continue;
	    }

	    if ( !nodeList.at(i).toElement().text().isEmpty() ) {
		QDomElement elem = nodeList.at(i).toElement();
		if ( elem.hasAttribute("replaceWith") )
		    hashCityNameReplacements.insert( elem.text().toLower(), elem.attribute("replaceWith", "").toLower() );
		cities << elem.text();
	    }
	}
    }

    //     <useSeperateCityValue>
    bool useSeperateCityValue = false;
    node = docElement.firstChildElement("useSeperateCityValue");
    if ( !node.isNull() ) {
	QString sBool = node.toElement().text().toLower();
	if ( sBool == "true" || sBool == "1" )
	    useSeperateCityValue = true;
	else
	    useSeperateCityValue = false;
    }

    //     <onlyUseCitiesInList>
    bool onlyUseCitiesInList = false;
    node = docElement.firstChildElement("onlyUseCitiesInList");
    if ( !node.isNull() ) {
	QString sBool = node.toElement().text().toLower();
	if ( sBool == "true" || sBool == "1" )
	    onlyUseCitiesInList = true;
	else
	    onlyUseCitiesInList = false;
    }

//     <defaultVehicleType>
    QString defaultVehicleType;
    node = docElement.firstChildElement("defaultVehicleType");
    if ( !node.isNull() ) {
	defaultVehicleType = node.toElement().text();
    }

//     <description>
    QString description, descriptionEn;
    node = docElement.firstChildElement("description");
    while ( !node.isNull() ) {
	if ( !node.hasAttributes() || node.toElement().attribute("lang") == lang ) {
	    description = node.toElement().text();
	    break;
	}
	else if ( node.toElement().attribute("lang") == "en" )
	    descriptionEn= node.toElement().text();

	node = node.nextSiblingElement("description");
    }
    if ( description.isEmpty() )
	description = descriptionEn;

//     <url>
    node = docElement.firstChildElement("url");
    if ( node.isNull() ) {
	qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "No <url> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }
    QString url = node.toElement().text();

    //     <shortUrl>
    node = docElement.firstChildElement("shortUrl");
    if ( node.isNull() ) {
	qDebug() << "TimetableAccessor::getSpecificAccessor"
	<< "No <shortUrl> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }
    QString shortUrl = node.toElement().text();

    //     <charsetForUrlEncoding>
    QString charsetForUrlEncoding;
    node = docElement.firstChildElement("charsetForUrlEncoding");
    if ( !node.isNull() ) {
	charsetForUrlEncoding = node.toElement().text();
    }

//     <rawUrls>
    node = docElement.firstChildElement("rawUrls");
    if ( node.isNull() ) {
	qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "No <rawUrls> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }

//     <rawUrls><departures>
    QString rawUrlDepartures;
    childNode = node.firstChildElement("departures");
    if ( childNode.isNull() ) {
	if ( type == HTML ) {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No child tag <departures> found in the <rawUrls> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}
    } else {
	if ( childNode.firstChild().isCDATASection() )
	    rawUrlDepartures = childNode.firstChild().toCDATASection().data();
	else {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "No CData section found in <rawUrls><departures> in service provider information XML named"
	    << serviceProvider;
	    return NULL;
	}
    }

//     <rawUrls><stopSuggestions>
    QString rawUrlStopSuggestions;
    childNode = node.firstChildElement("stopSuggestions");
    if ( !childNode.isNull() ) {
// 	if ( type == XML ) {
// 	    qDebug() << "TimetableAccessor::getSpecificAccessor"
// 	    << "No child tag <stopSuggestions> found in the <rawUrls> tag in service provider information XML named"
// 	    << serviceProvider;
// 	    return NULL;
// 	}
//     } else {
	if ( childNode.firstChild().isCDATASection() )
	    rawUrlStopSuggestions = childNode.firstChild().toCDATASection().data();
	else {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "No CData section found in <rawUrls><stopSuggestions> in service provider information XML named"
	    << serviceProvider;
	    return NULL;
	}
    }

//     <rawUrls><journeys>
    QString rawUrlJourneys;
    childNode = node.firstChildElement("journeys");
    if ( !childNode.isNull() ) {
	if ( childNode.firstChild().isCDATASection() )
	    rawUrlJourneys = childNode.firstChild().toCDATASection().data();
	else {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "No CData section found in <rawUrls><journeys> in service provider information XML named"
	    << serviceProvider;
	    return NULL;
	}
    }
    // </rawUrls>

    // <regExps>
    node = docElement.firstChildElement("regExps");
    if ( node.isNull() ) {
	qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "No <regExps> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }

// <regExps><departures>
    QString regExpDepartures;
    QList<TimetableInformation> infosDepartures;
    QString regExpDeparturesPre;
    TimetableInformation infoPreKey = Nothing, infoPreValue = Nothing;
    childNode = node.firstChildElement("departures");
    if ( childNode.isNull() ) {
	if ( type == HTML ) {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No child tag <departures> found in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}
    } else {
	// <regExps><departures> <regExpPre></regExpPre>
	subChildNode = childNode.firstChildElement("regExpPre");
	if ( !subChildNode.isNull() ) {
	    if ( subChildNode.toElement().hasAttribute("key") && subChildNode.toElement().hasAttribute("value") ) {
		infoPreKey = timetableInformationFromString( subChildNode.toElement().attribute("key", "") );
		infoPreValue = timetableInformationFromString( subChildNode.toElement().attribute("value", "") );
	    } else {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No attributes 'key' and 'value' found in <regExps><departures><regExpPre> in service provider information XML named"
		<< serviceProvider;
		return NULL;
	    }

	    if ( subChildNode.firstChild().isCDATASection() ) {
		regExpDeparturesPre = subChildNode.firstChild().toCDATASection().data();
		if ( !QRegExp(regExpDeparturesPre).isValid() )
		    qDebug() << "TimetableAccessor::getSpecificAccessor" << "The regular expression in <regExps><departures><regExpPre> is invalid." << QRegExp(regExpDeparturesPre).errorString();
	    }
	    else {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No CData section found in <regExpPre> in the <departures> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
		return NULL;
	    }
	}

	// <regExps><departures> <regExp></regExp>
	subChildNode = childNode.firstChildElement("regExp");
	if ( subChildNode.isNull() ) {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No child tag <regExp> found in the <departures> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}
	if ( subChildNode.firstChild().isCDATASection() ) {
	    regExpDepartures = subChildNode.firstChild().toCDATASection().data();
	    if ( !QRegExp(regExpDepartures).isValid() )
		qDebug() << "TimetableAccessor::getSpecificAccessor" << "The regular expression in <regExps><departures><regExp> is invalid." << QRegExp(regExpDepartures).errorString();
	}
	else {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No CData section found in <regExp> in the <departures> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}

	// <regExps><departures> <infos> <info>...</info>... </infos>
	subChildNode = childNode.firstChildElement("infos");
	if ( subChildNode.isNull() ) {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No child tag <infos> found in the <departures> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}

	nodeList = subChildNode.childNodes();
	for ( uint i = 0; i < nodeList.length(); ++i ) {
	    if ( nodeList.at(i).isComment() )
		continue;
	    if ( nodeList.at(i).toElement().tagName() != "info" ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		    << "Warning: Invalid child tag (not <info>) found in the <infos> tag in service provider information XML named"
		    << serviceProvider << "The name of the invalid tag is" << nodeList.at(i).toElement().tagName();
		continue;
	    }
	    if ( !nodeList.at(i).toElement().text().isEmpty() )
		infosDepartures << timetableInformationFromString( nodeList.at(i).toElement().text() );
	}
    }	// </departures>

    // <regExps><journeys>
    QString regExpJourneys;
    QList<TimetableInformation> infosJourneys;
    childNode = node.firstChildElement("journeys");
    if ( !childNode.isNull() ) {
	// <regExps><journeys><regExp>
	subChildNode = childNode.firstChildElement("regExp");
	if ( subChildNode.isNull() ) {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No child tag <regExp> found in the <journeys> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}
	if ( subChildNode.firstChild().isCDATASection() ) {
	    regExpJourneys = subChildNode.firstChild().toCDATASection().data();
	    if ( !QRegExp(regExpJourneys).isValid() )
		qDebug() << "TimetableAccessor::getSpecificAccessor" << "The regular expression in <regExps><journeys><regExp> is invalid." << QRegExp(regExpJourneys).errorString();
	}
	else {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
	    << "No CData section found in <regExp> in the <journeys> tag in the <regExps> tag in service provider information XML named"
	    << serviceProvider;
	    return NULL;
	}

	// <regExps><journeys> <infos><info>...</info>..</infos>
	subChildNode = childNode.firstChildElement("infos");
	if ( subChildNode.isNull() ) {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No child tag <infos> found in the <journeys> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}
	nodeList = subChildNode.childNodes();
	for ( uint i = 0; i < nodeList.length(); ++i ) {
	    if ( nodeList.at(i).isComment() )
		continue;
	    if ( nodeList.at(i).toElement().tagName() != "info" ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		    << "Warning: Invalid child tag (not <info>) found in the <infos> tag in the <journeys> tag in the <regExps> tag in service provider information XML named"
		    << serviceProvider << "The name of the invalid tag is" << nodeList.at(i).toElement().tagName();
		continue;
	    }
	    if ( !nodeList.at(i).toElement().text().isEmpty() )
		infosJourneys << timetableInformationFromString( nodeList.at(i).toElement().text() );
	}
    }
//     </journeys>

//     <regExps><journeyNews> <item>
//		<regExp>...</regExp> <infos> <info>...</info>... </infos>
//     </item>... </journeyNews>
    QStringList regExpListJourneyNews;
    QList< QList<TimetableInformation> > infosListJourneyNews;
    childNode = node.firstChildElement("journeyNews");
    if ( !childNode.isNull() ) {
	nodeList = childNode.childNodes();
	for ( uint i = 0; i < nodeList.length(); ++i ) {
	    if ( nodeList.at(i).isComment() )
		continue;
	    if ( nodeList.at(i).toElement().tagName() != "item" ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		    << "Warning: Invalid child tag (not <item>) found in the <journeyNews> tag in the <regExps> tag in service provider information XML named"
		    << serviceProvider << "The name of the invalid tag is"
		    << nodeList.at(i).toElement().tagName();
		continue;
	    }

	    subChildNode = nodeList.at(i).toElement();
	    subSubChildNode = subChildNode.firstChildElement("regExp");
	    if ( subSubChildNode.isNull() ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		    << "No child tag <regExp> found in the <item> tag int the <journeyNews> tag in the <regExps> tag in service provider information XML named"
		    << serviceProvider;
		return NULL;
	    }
	    if ( subSubChildNode.firstChild().isCDATASection() ) {
		regExpListJourneyNews << subSubChildNode.firstChild().toCDATASection().data();
		if ( !QRegExp(regExpListJourneyNews.last()).isValid() )
		    qDebug() << "TimetableAccessor::getSpecificAccessor"
			<< "The regular expression in <regExps><journeyNews><item><regExp> is invalid."
			<< "item:" << i << QRegExp(regExpListJourneyNews.last()).errorString();
	    }
	    else {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		    << "No CData section found in <regExp> in <item> in the <journeyNews> tag in the <regExps> tag in service provider information XML named"
		    << serviceProvider;
		return NULL;
	    }

	    subSubChildNode = subChildNode.firstChildElement("infos");
	    if ( subSubChildNode.isNull() ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		    << "No child tag <infos> found in the <item> tag in the <journeyNews> tag in the <regExps> tag in service provider information XML named"
		    << serviceProvider;
		return NULL;
	    }
	    QList<TimetableInformation> infoList;
	    subNodeList = subSubChildNode.childNodes();
	    for ( uint i = 0; i < subNodeList.length(); ++i ) {
// 		qDebug() << "subNodeList journey news infos:" << i << subNodeList.at(i).toElement().tagName();
		if ( nodeList.at(i).isComment() )
		    continue;

		if ( subNodeList.at(i).toElement().tagName() != "info" ) {
		    qDebug() << "TimetableAccessor::getSpecificAccessor"
			<< "Warning: Invalid child tag (not <info>) found in the <infos> tag int the <item> tag in the <journeyNews> tag in the <regExps> tag in service provider information XML named"
			<< serviceProvider << "The name of the invalid tag is"
			<< subNodeList.at(i).toElement().tagName();
		    continue;
		}

		if ( !subNodeList.at(i).toElement().text().isEmpty() ) {
// 		    qDebug() << "TimetableAccessor::getSpecificAccessor" << "timetableInformationFromString"
// 			<< subNodeList.at(i).toElement().text() << "=>"
// 			<< timetableInformationFromString( subNodeList.at(i).toElement().text() );
		    infoList << timetableInformationFromString( subNodeList.at(i).toElement().text() );
		} else {
		    qDebug() << "TimetableAccessor::getSpecificAccessor"
			<< "Warning: No text found in the <info> tag in the <infos> tag int the <item> tag in the <journeyNews> tag in the <regExps> tag in service provider information XML named"
			<< serviceProvider;
		}
	    }
	    infosListJourneyNews << infoList;
	} // for (all nodes in <regExps><journeyNews>)
    } // <regExps><journeyNews> isn't null

//     <regExps><possibleStops> <item>
// 		<regExpRange>...</regExpRange>
//		<regExp>...</regExp>
// 		<infos> <info>...</info>... </infos>
//     </item>... </possibleStops>
    QStringList regExpListPossibleStopsRange, regExpListPossibleStops;
    QList< QList<TimetableInformation> > infosListPossibleStops;
    childNode = node.firstChildElement("possibleStops");
    if ( !childNode.isNull() ) {
	nodeList = childNode.childNodes();
	for ( uint i = 0; i < nodeList.length(); ++i ) {
	    if ( nodeList.at(i).isComment() )
		continue;
	    if ( nodeList.at(i).toElement().tagName() != "item" ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "Warning: Invalid child tag (not <item>) found in the <possibleStops> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider << "The name of the invalid tag is" << nodeList.at(i).toElement().tagName();
		continue;
	    }

	    subChildNode = nodeList.at(i).toElement();
	    subSubChildNode = subChildNode.firstChildElement("regExpRange");
	    if ( subSubChildNode.isNull() ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		    << "No child tag <regExpRange> found in the <item> tag int the <possibleStops> tag in the <regExps> tag in service provider information XML named"
		    << serviceProvider;
		return NULL;
	    }
	    if ( subSubChildNode.firstChild().isCDATASection() )
		regExpListPossibleStopsRange << subSubChildNode.firstChild().toCDATASection().data();
	    else {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No CData section found in <regExpRange> in <item> in the <possibleStops> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
		return NULL;
	    }

	    subChildNode = nodeList.at(i).toElement();
	    subSubChildNode = subChildNode.firstChildElement("regExp");
	    if ( subSubChildNode.isNull() ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No child tag <regExp> found in the <item> tag int the <possibleStops> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
		return NULL;
	    }
	    if ( subSubChildNode.firstChild().isCDATASection() )
		regExpListPossibleStops << subSubChildNode.firstChild().toCDATASection().data();
	    else {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No CData section found in <regExp> in <item> in the <possibleStops> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
		return NULL;
	    }

	    subSubChildNode = subChildNode.firstChildElement("infos");
	    if ( subSubChildNode.isNull() ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "No child tag <infos> found in the <item> tag in the <possibleStops> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
		return NULL;
	    }
	    QList<TimetableInformation> infos;
	    subNodeList = subSubChildNode.childNodes();
	    for ( uint i = 0; i < subNodeList.length(); ++i ) {
		if ( subNodeList.at(i).isComment() )
		    continue;
		if ( subNodeList.at(i).toElement().tagName() != "info" ) {
		    qDebug() << "TimetableAccessor::getSpecificAccessor"
		    << "Warning: Invalid child tag (not <info>) found in the <infos> tag int the <item> tag in the <possibleStops> tag in the <regExps> tag in service provider information XML named"
		    << serviceProvider << "The name of the invalid tag is" << subNodeList.at(i).toElement().tagName();
		    continue;
		}
		if ( !subNodeList.at(i).toElement().text().isEmpty() )
		    infos << timetableInformationFromString( subNodeList.at(i).toElement().text() );
	    }
	    infosListPossibleStops << infos;
	} // for (all nodes in <regExps><possibleStops>)
    } // <regExps><possibleStops> isn't null

    //     <regExps><departureGroupTitles>
    //		<regExp>...</regExp>
    // 	<infos> <info>...</info>... </infos>
    //     </departureGroupTitles>
    QString regExpDepartureGroupTitles;
    QList<TimetableInformation> infosDepartureGroupTitles;
    childNode = node.firstChildElement("departureGroupTitles");
    if ( !childNode.isNull() ) {
	// TODO: Test
	subChildNode = childNode.firstChildElement("regExp");
	if ( subChildNode.isNull() ) {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "Warning: No child tag <regExp> found in the <departureGroupTitles> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    goto DepartureGroupTitlesTagNotComplete;
	}
	else if ( subChildNode.firstChild().isCDATASection() )
	    regExpDepartureGroupTitles = subSubChildNode.firstChild().toCDATASection().data();
	else {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "Warning: No CData section found in <regExp> in the <departureGroupTitles> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    goto DepartureGroupTitlesTagNotComplete;
	}

	subChildNode = childNode.firstChildElement("infos");
	if ( subChildNode.isNull() ) {
	    qDebug() << "TimetableAccessor::getSpecificAccessor"
		<< "Warning: No child tag <infos> found in the <departureGroupTitles> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    regExpDepartureGroupTitles = "";
	    goto DepartureGroupTitlesTagNotComplete;
	}
	subNodeList = subChildNode.childNodes();
	for ( uint i = 0; i < subNodeList.length(); ++i ) {
	    if ( subNodeList.at(i).isComment() )
		continue;
	    if ( subNodeList.at(i).toElement().tagName() != "info" ) {
		qDebug() << "TimetableAccessor::getSpecificAccessor"
		    << "Warning: Invalid child tag (not <info>) found in the <infos> tag int the <departureGroupTitles> tag in the <regExps> tag in service provider information XML named"
		    << serviceProvider << "The name of the invalid tag is" << subNodeList.at(i).toElement().tagName();
		continue;
	    }
	    if ( !subNodeList.at(i).toElement().text().isEmpty() )
		infosDepartureGroupTitles << timetableInformationFromString( subNodeList.at(i).toElement().text() );
	}
    }
DepartureGroupTitlesTagNotComplete:


    TimetableAccessorInfo accessorInfos( name, shortUrl, author, email, version, serviceProvider, type );
    accessorInfos.setFileName( fileName );
    accessorInfos.setCountry( country );
    accessorInfos.setCities( cities );
    accessorInfos.setCityNameToValueReplacementHash( hashCityNameReplacements );
    accessorInfos.setUseSeperateCityValue( useSeperateCityValue );
    accessorInfos.setOnlyUseCitiesInList( onlyUseCitiesInList );
    accessorInfos.setDescription( description );
    accessorInfos.setDefaultVehicleType( vehicleTypeFromString(defaultVehicleType) );
    accessorInfos.setUrl( url );
    accessorInfos.setShortUrl( shortUrl );
    accessorInfos.setDepartureRawUrl( rawUrlDepartures );
    accessorInfos.setStopSuggestionsRawUrl( rawUrlStopSuggestions );
    accessorInfos.setCharsetForUrlEncoding( charsetForUrlEncoding.toAscii() );
    accessorInfos.setJourneyRawUrl( rawUrlJourneys );
    accessorInfos.setRegExpDepartures( regExpDepartures, infosDepartures,
			    regExpDeparturesPre, infoPreKey, infoPreValue );
    if ( !regExpDepartureGroupTitles.isEmpty() )
	accessorInfos.setRegExpDepartureGroupTitles( regExpDepartureGroupTitles, infosDepartureGroupTitles );
    if ( !regExpJourneys.isEmpty() )
	accessorInfos.setRegExpJourneys( regExpJourneys, infosJourneys );
    for ( int i = 0; i < regExpListJourneyNews.length(); ++i ) {
	QString regExpJourneyNews = regExpListJourneyNews[i];
	QList<TimetableInformation> infosJourneyNews = infosListJourneyNews[i];
	accessorInfos.addRegExpJouneyNews( regExpJourneyNews, infosJourneyNews );
    }
    for ( int i = 0; i < regExpListPossibleStopsRange.length(); ++i ) {
	QString regExpPossibleStopsRange = regExpListPossibleStopsRange[i];
	QString regExpPossibleStops = regExpListPossibleStops[i];
	QList<TimetableInformation> infosPossibleStops = infosListPossibleStops[i];
	accessorInfos.addRegExpPossibleStops( regExpPossibleStopsRange, regExpPossibleStops, infosPossibleStops );
    }

    if ( type == HTML )
	return new TimetableAccessorHtml( accessorInfos );
    else if ( type == XML )
	return new TimetableAccessorXml( accessorInfos );
    else {
	qDebug() << "TimetableAccessor::getSpecificAccessor" << "Accessor type not supported" << type;
	return NULL;
    }
}

AccessorType TimetableAccessor::accessorTypeFromString( const QString &sAccessorType ) {
    QString s = sAccessorType.toLower();
    if ( s == "html" )
	return HTML;
    else if ( s == "xml" )
	return XML;
    else
	return NoAccessor;
}

VehicleType TimetableAccessor::vehicleTypeFromString( QString sVehicleType ) {
    if ( sVehicleType == "Unknown" )
	 return Unknown;
    else if ( sVehicleType == "Tram" )
	return Tram;
    else if ( sVehicleType == "Bus" )
	return Bus;
    else if ( sVehicleType == "Subway" )
	return Subway;
    else if ( sVehicleType == "TrainInterurban" )
	return TrainInterurban;
    else if ( sVehicleType == "Metro" )
	return Metro;
    else if ( sVehicleType == "TrolleyBus" )
	return TrolleyBus;
    else if ( sVehicleType == "TrainRegional" )
	return TrainRegional;
    else if ( sVehicleType == "TrainRegionalExpress" )
	return TrainRegionalExpress;
    else if ( sVehicleType == "TrainInterregio" )
	return TrainInterregio;
    else if ( sVehicleType == "TrainIntercityEurocity" )
	return TrainIntercityEurocity;
    else if ( sVehicleType == "TrainIntercityExpress" )
	return TrainIntercityExpress;
    else if ( sVehicleType == "Ferry" )
	return Ferry;
    else if ( sVehicleType == "Plane" )
	return Plane;
    else
	return Unknown;
}

TimetableInformation TimetableAccessor::timetableInformationFromString(
			const QString& sTimetableInformation ) {
    if ( sTimetableInformation == "Nothing" )
	return Nothing;
    else if ( sTimetableInformation == "DepartureDate" )
	return DepartureDate;
    else if ( sTimetableInformation == "DepartureHour" )
	return DepartureHour;
    else if ( sTimetableInformation == "DepartureMinute" )
	return DepartureMinute;
    else if ( sTimetableInformation == "TypeOfVehicle" )
	return TypeOfVehicle;
    else if ( sTimetableInformation == "TransportLine" )
	return TransportLine;
    else if ( sTimetableInformation == "FlightNumber" )
	return FlightNumber;
    else if ( sTimetableInformation == "Target" )
	return Target;
    else if ( sTimetableInformation == "Platform" )
	return Platform;
    else if ( sTimetableInformation == "Delay" )
	return Delay;
    else if ( sTimetableInformation == "DelayReason" )
	return DelayReason;
    else if ( sTimetableInformation == "JourneyNews" )
	return JourneyNews;
    else if ( sTimetableInformation == "JourneyNewsOther" )
	return JourneyNewsOther;
    else if ( sTimetableInformation == "JourneyNewsLink" )
	return JourneyNewsLink;
    else if ( sTimetableInformation == "DepartureHourPrognosis" )
	return DepartureHourPrognosis;
    else if ( sTimetableInformation == "DepartureMinutePrognosis" )
	return DepartureMinutePrognosis;
    else if ( sTimetableInformation == "Status" )
	return Status;
    else if ( sTimetableInformation == "Operator" )
	return Operator;
    else if ( sTimetableInformation == "DepartureAMorPM" )
	return DepartureAMorPM;
    else if ( sTimetableInformation == "DepartureAMorPMPrognosis" )
	return DepartureAMorPMPrognosis;
    else if ( sTimetableInformation == "ArrivalAMorPM" )
	return ArrivalAMorPM;
    else if ( sTimetableInformation == "Duration" )
	return Duration;
    else if ( sTimetableInformation == "StartStopName" )
	return StartStopName;
    else if ( sTimetableInformation == "StartStopID" )
	return StartStopID;
    else if ( sTimetableInformation == "TargetStopName" )
	return TargetStopName;
    else if ( sTimetableInformation == "TargetStopID" )
	return TargetStopID;
    else if ( sTimetableInformation == "ArrivalDate" )
	return ArrivalDate;
    else if ( sTimetableInformation == "ArrivalHour" )
	return ArrivalHour;
    else if ( sTimetableInformation == "ArrivalMinute" )
	return ArrivalMinute;
    else if ( sTimetableInformation == "Changes" )
	return Changes;
    else if ( sTimetableInformation == "TypesOfVehicleInJourney" )
	return TypesOfVehicleInJourney;
    else if ( sTimetableInformation == "Pricing" )
	return Pricing;
    else if ( sTimetableInformation == "NoMatchOnSchedule" )
	return NoMatchOnSchedule;
    else if ( sTimetableInformation == "StopName" )
	return StopName;
    else if ( sTimetableInformation == "StopID" )
	return StopID;
    else {
	qDebug() << "TimetableAccessor::timetableInformationFromString" << sTimetableInformation << "is an unknown timetable information value! Assuming value Nothing.";
	return Nothing;
    }
}

QStringList TimetableAccessor::features() const {
    return m_info.features();
}

KIO::TransferJob *TimetableAccessor::requestDepartures( const QString &sourceName, const QString &city, const QString &stop, int maxDeps, const QDateTime &dateTime, const QString &dataType, bool useDifferentUrl ) {
    qDebug() << "TimetableAccessor::requestDepartures" << "URL =" << getUrl(city, stop, maxDeps, dateTime, dataType, useDifferentUrl).url();

    // Creating a kioslave
    if ( useDifferentUrl )
	qDebug() << "using a different url";
    KIO::TransferJob *job = KIO::get( getUrl(city, stop, maxDeps, dateTime, dataType, useDifferentUrl), KIO::NoReload, KIO::HideProgressInfo );
    m_jobInfos.insert( job, QList<QVariant>() << static_cast<int>(ParseForDeparturesArrivals) << sourceName << city << stop << maxDeps << dateTime << dataType << useDifferentUrl );
    m_document = "";

    connect( job, SIGNAL(data(KIO::Job*,QByteArray)), this, SLOT(dataReceived(KIO::Job*,QByteArray)) );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(finished(KJob*)) );

    return job;
}

KIO::TransferJob *TimetableAccessor::requestJourneys ( const QString &sourceName, const QString &city, const QString &startStopName, const QString &targetStopName, int maxDeps, const QDateTime &dateTime, const QString &dataType,  bool useDifferentUrl ) {
    qDebug() << "TimetableAccessor::requestJourneys" << "URL =" << getJourneyUrl(city, startStopName, targetStopName, maxDeps, dateTime, dataType, useDifferentUrl).url();

    // Creating a kioslave
    if ( useDifferentUrl )
	qDebug() << "using a different url";
    KIO::TransferJob *job = KIO::get( getJourneyUrl(city, startStopName, targetStopName, maxDeps, dateTime, dataType, useDifferentUrl), KIO::NoReload, KIO::HideProgressInfo );
    m_jobInfos.insert( job, QList<QVariant>() << static_cast<int>(ParseForJourneys) << sourceName << city << startStopName << maxDeps << dateTime << dataType << useDifferentUrl << targetStopName );
    m_document = "";

    connect( job, SIGNAL(data(KIO::Job*,QByteArray)), this, SLOT(dataReceived(KIO::Job*,QByteArray)) );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(finished(KJob*)) );

    return job;
}

void TimetableAccessor::dataReceived ( KIO::Job*, const QByteArray& data ) {
    m_document += data;
}

void TimetableAccessor::finished(KJob* job) {
    QList<QVariant> jobInfo = m_jobInfos.value(job);
    m_jobInfos.remove(job);

    ParseDocumentMode parseDocumentMode = static_cast<ParseDocumentMode>( jobInfo.at(0).toInt() );
    QString sourceName = jobInfo.at(1).toString();
    QString city = jobInfo.at(2).toString();
    QString stop = jobInfo.at(3).toString();
    int maxDeps = jobInfo.at(4).toInt();
    QDateTime dateTime = jobInfo.at(5).toDateTime();
    QString dataType = jobInfo.at(6).toString();
    bool usedDifferentUrl = jobInfo.at(7).toBool();
    QString targetStop;
    if ( parseDocumentMode == ParseForJourneys )
	targetStop = jobInfo.at(8).toString();
    m_curCity = city;
    QList< PublicTransportInfo* > *dataList;
    QHash< QString, QString > *stops;

    // TODO: less confusing?
//     qDebug() << "usedDifferentUrl" << usedDifferentUrl;
    if ( !usedDifferentUrl ) {
	if ( (dataList = new QList<PublicTransportInfo*>()) != NULL
		    && parseDocument(dataList, parseDocumentMode) ) {
	    if ( parseDocumentMode == ParseForDeparturesArrivals ) {
		qDebug() << "TimetableAccessor::finished"
			 << "emit departureListReceived" << sourceName;
		QList<DepartureInfo*>* departures = new QList<DepartureInfo*>();
		foreach( PublicTransportInfo *info, *dataList )
		    departures->append( (DepartureInfo*)info );
		emit departureListReceived( this, *departures, serviceProvider(),
					    sourceName, city, stop, dataType, parseDocumentMode );
	    } else if ( parseDocumentMode == ParseForJourneys ) {
// 		qDebug() << "TimetableAccessor::finished" << "emit journeyListReceived";
		QList<JourneyInfo*>* journeys = new QList<JourneyInfo*>();
		foreach( PublicTransportInfo *info, *dataList )
		    journeys->append( (JourneyInfo*)info );
		emit journeyListReceived( this, *journeys, serviceProvider(),
					  sourceName, city, stop, dataType, parseDocumentMode );
	    }
	} else if ( hasSpecialUrlForStopSuggestions() ) {
// 	    qDebug() << "request possible stop list";
	    requestDepartures( sourceName, m_curCity, stop, maxDeps, dateTime, dataType, true );
	} else if ( (stops = new QHash<QString, QString>()) != NULL && parseDocumentPossibleStops(stops) ) {
// 	    qDebug() << "possible stop list received";
	    emit stopListReceived( this, *stops, serviceProvider(), sourceName, city, stop, dataType, parseDocumentMode );
	} else
	    emit errorParsing( this, serviceProvider(), sourceName, city, stop, dataType, parseDocumentMode );
    } else if ( (stops = new QHash<QString, QString>()) != NULL && parseDocumentPossibleStops(stops) ) {
// 	qDebug() << "possible stop list received ok";
	emit stopListReceived( this, *stops, serviceProvider(), sourceName, city, stop, dataType, parseDocumentMode );
    } else
	emit errorParsing( this, serviceProvider(), city, sourceName, stop, dataType, parseDocumentMode );
}

KUrl TimetableAccessor::getUrl ( const QString &city, const QString &stop, int maxDeps, const QDateTime &dateTime, const QString &dataType, bool useDifferentUrl ) const {
    QString sRawUrl = useDifferentUrl ? stopSuggestionsRawUrl() : departuresRawUrl();
    QString sTime = dateTime.time().toString("hh:mm");
    QString sDataType;
    QString sCity = city.toLower(), sStop = stop.toLower();
    if ( dataType == "arrivals" )
	sDataType = "arr";
    else if ( dataType == "departures" || dataType == "journeys" )
	sDataType = "dep";

    sCity = timetableAccessorInfo().mapCityNameToValue( sCity );

    // Encode city and stop
    if ( charsetForUrlEncoding().isEmpty() ) {
	sCity = QString::fromAscii(QUrl::toPercentEncoding(sCity));
	sStop = QString::fromAscii(QUrl::toPercentEncoding(sStop));
    } else {
	sCity = toPercentEncoding( sCity, charsetForUrlEncoding() );
	sStop = toPercentEncoding( sStop, charsetForUrlEncoding() );
    }

    // Construct the url from the "raw" url by replacing values
    if ( useSeperateCityValue() )
	sRawUrl = sRawUrl.replace( "{city}", sCity );
    sRawUrl = sRawUrl.replace( "{time}", sTime ).replace( "{maxDeps}", QString("%1").arg(maxDeps) ).replace( "{stop}", sStop ).replace( "{dataType}", sDataType );

    QRegExp rx = QRegExp("{date:([^}]*)}", Qt::CaseInsensitive);
    if ( rx.indexIn(sRawUrl) != -1 )
	sRawUrl.replace( rx, dateTime.date().toString(rx.cap()) );

    return KUrl( sRawUrl );
}

KUrl TimetableAccessor::getJourneyUrl ( const QString& city, const QString& startStopName, const QString& targetStopName, int maxDeps, const QDateTime &dateTime, const QString& dataType, bool useDifferentUrl ) const {
    Q_UNUSED( useDifferentUrl );

    QString sRawUrl = m_info.journeyRawUrl();
    QString sTime = dateTime.time().toString("hh:mm");
    QString sDataType;
    QString sCity = city.toLower(), sStartStopName = startStopName.toLower(), sTargetStopName = targetStopName.toLower();
    if ( dataType == "arrivals" )
	sDataType = "arr";
    else if ( dataType == "departures" || dataType == "journeys" )
	sDataType = "dep";

    sCity = timetableAccessorInfo().mapCityNameToValue(sCity);

    // Encode city and stop
    if ( charsetForUrlEncoding().isEmpty() ) {
	sCity = QString::fromAscii(QUrl::toPercentEncoding(sCity));
	sStartStopName = QString::fromAscii( QUrl::toPercentEncoding(sStartStopName) );
	sTargetStopName = QString::fromAscii( QUrl::toPercentEncoding(sTargetStopName) );
    } else {
	sCity = toPercentEncoding( sCity, charsetForUrlEncoding() );
	sStartStopName = toPercentEncoding( sStartStopName, charsetForUrlEncoding() );
	sTargetStopName = toPercentEncoding( sTargetStopName, charsetForUrlEncoding() );
    }

    // Construct the url from the "raw" url by replacing values
    if ( useSeperateCityValue() )
	sRawUrl = sRawUrl.replace( "{city}", sCity );

    sRawUrl = sRawUrl.replace( "{time}", sTime ).replace( "{maxDeps}", QString("%1").arg(maxDeps) ).replace( "{startStop}", sStartStopName ).replace( "{targetStop}", sTargetStopName ).replace( "{dataType}", sDataType );

    QRegExp rx = QRegExp("{date:([^}]*)}", Qt::CaseInsensitive);
    if ( rx.indexIn(sRawUrl) != -1 )
	sRawUrl.replace( rx, dateTime.date().toString(rx.cap()) );

    return KUrl( sRawUrl );
}

QString TimetableAccessor::gethex ( ushort decimal ) {
    QString hexchars = "0123456789ABCDEFabcdef";
    return "%" + hexchars[decimal >> 4] + hexchars[decimal & 0xF];
}

QString TimetableAccessor::toPercentEncoding ( QString str, QByteArray charset ) {
    QString unreserved = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~";
//     QString reserved = "!*'();:@&=+$,/?%#[]";
    QString encoded;

    QByteArray ba = QTextCodec::codecForName(charset)->fromUnicode(str);
    for ( int i = 0; i < ba.length(); ++i )
    {
	char ch = ba[i];
	if ( unreserved.indexOf(ch) != -1 )
	{
	    encoded += ch;
// 	    qDebug() << "  Unreserved char" << encoded;
	}
	else if ( ch < 0 )
	{
	    encoded += gethex(256 + ch);
// 	    qDebug() << "  Encoding char < 0" << encoded;
	}
	else
	{
	    encoded += gethex(ch);
// 	    qDebug() << "  Encoding char >= 0" << encoded;
	}
    }

    return encoded;
}

bool TimetableAccessor::parseDocument( QList<PublicTransportInfo*> *journeys,
				       ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED( journeys );
    Q_UNUSED( parseDocumentMode );
    return false;
}

bool TimetableAccessor::parseDocumentPossibleStops ( QHash<QString,QString> *stops ) const {
    Q_UNUSED( stops );
    return false;
}

QString TimetableAccessor::departuresRawUrl() const {
    return m_info.departureRawUrl();
}

QString TimetableAccessor::stopSuggestionsRawUrl() const {
    return m_info.stopSuggestionsRawUrl();
}

QByteArray TimetableAccessor::charsetForUrlEncoding() const {
    return m_info.charsetForUrlEncoding();
}
/*
QString TimetableAccessor::regExpSearch() const
{
    return m_info.searchJourneys.regExp();
}

QList< TimetableInformation > TimetableAccessor::regExpInfos() const
{
    return m_info.searchJourneys.infos();
}

QString TimetableAccessor::regExpSearchPossibleStopsRange() const
{
    return m_info.regExpSearchPossibleStopsRange;
}

QString TimetableAccessor::regExpSearchPossibleStops() const
{
    return m_info.searchPossibleStops.regExp();
}

QList< TimetableInformation > TimetableAccessor::regExpInfosPossibleStops() const
{
    return m_info.searchPossibleStops.infos();
}*/

TimetableAccessorInfo TimetableAccessor::timetableAccessorInfo() const {
    return m_info;
}




