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
#include "timetableaccessor_html_script.h"
#include "timetableaccessor_xml.h"
#include "timetableaccessor_htmlinfo.h"
#include <KLocale>


TimetableAccessor* TimetableAccessor::getSpecificAccessor( const QString &serviceProvider ) {
    QString fileName = KGlobal::dirs()->findResource( "data",
	    QString("plasma_engine_publictransport/accessorInfos/%1.xml")
	    .arg(serviceProvider) );
    if ( fileName.isEmpty() ) {
	kDebug() << "Couldn't find a service provider information XML named" << serviceProvider;
	return NULL;
    }

    QFile file( fileName );
    file.open( QIODevice::ReadOnly );
    QByteArray document = file.readAll();
    file.close();
    if ( document.isEmpty() ) {
	kDebug() << "Found service provider information XML named"
	    << serviceProvider << "but is empty." << "Filename =" << fileName;
	return NULL;
    }

    // Get country code from filename
    QString country = "international";
    QRegExp rx("^([^_]+)");
    if ( rx.indexIn( serviceProvider ) != -1
	    && KGlobal::locale()->allCountriesList().contains(rx.cap()) )
	country = rx.cap();
    
    // TODO: create a subclass of QXmlReader for reading accessor infos
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
	kDebug() << "No <name> tag in service provider information XML named"
		 << serviceProvider;
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
	kDebug()
	    << "No <version> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }
    QString version = node.toElement().text();

//     <type>
    node = docElement.firstChildElement("type");
    if ( node.isNull() ) {
	kDebug()
	    << "No <type> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }
    AccessorType type = accessorTypeFromString( node.toElement().text() );
    if ( type == NoAccessor ) {
	kDebug() << "The <type> tag in service provider information XML named"
	    << serviceProvider << "is invalid. Currently there are two values "
	    "allowed: HTML and XML.";
	return NULL;
    }

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
		kDebug()
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
	kDebug() << "No <url> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }
    QString url = node.toElement().text();

    //     <shortUrl>
    node = docElement.firstChildElement("shortUrl");
    if ( node.isNull() ) {
	kDebug() << "No <shortUrl> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }
    QString shortUrl = node.toElement().text();
    
    //     <minFetchWait>
    node = docElement.firstChildElement("minFetchWait");
    int minFetchWait = 0;
    if ( !node.isNull() )
	minFetchWait = node.toElement().text().toInt();

    //     <charsetForUrlEncoding>
    QString charsetForUrlEncoding;
    node = docElement.firstChildElement("charsetForUrlEncoding");
    if ( !node.isNull() ) {
	charsetForUrlEncoding = node.toElement().text();
    }

//     <rawUrls>
    node = docElement.firstChildElement("rawUrls");
    if ( node.isNull() ) {
	kDebug() << "No <rawUrls> tag in service provider information XML named" << serviceProvider;
	return NULL;
    }

//     <rawUrls><departures>
    QString rawUrlDepartures;
    childNode = node.firstChildElement("departures");
    if ( childNode.isNull() ) {
	if ( type == HTML ) {
	    kDebug() << "No child tag <departures> found in the <rawUrls> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}
    } else {
	if ( childNode.firstChild().isCDATASection() )
	    rawUrlDepartures = childNode.firstChild().toCDATASection().data();
	else {
	    kDebug() << "No CData section found in <rawUrls><departures> in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}
    }

//     <rawUrls><stopSuggestions>
    QString rawUrlStopSuggestions;
    childNode = node.firstChildElement("stopSuggestions");
    if ( !childNode.isNull() ) {
	if ( childNode.firstChild().isCDATASection() )
	    rawUrlStopSuggestions = childNode.firstChild().toCDATASection().data();
	else {
	    kDebug() << "No CData section found in <rawUrls><stopSuggestions> "
		"in service provider information XML named" << serviceProvider;
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
	    kDebug() << "No CData section found in <rawUrls><journeys> in "
		"service provider information XML named" << serviceProvider;
	    return NULL;
	}
    }
    // </rawUrls>
    
    
    QString regExpDepartureGroupTitles, regExpDepartures, regExpDeparturesPre, regExpJourneys;
    QList<TimetableInformation> infosDepartureGroupTitles, infosDepartures, infosJourneys;
    QList< QList<TimetableInformation> > infosListPossibleStops, infosListJourneyNews;
    QStringList regExpListPossibleStopsRange, regExpListPossibleStops, regExpListJourneyNews;
    TimetableInformation infoPreKey = Nothing, infoPreValue = Nothing;
    
    bool useRegExps = true;
    node = docElement.firstChildElement("javaScript");
    QString javaScriptFile;
    if ( !node.isNull() ) {
	javaScriptFile = QFileInfo( fileName ).path() + "/" + node.toElement().text();
	if ( !QFile::exists(javaScriptFile) ) {
	    kDebug() << "The script file " << javaScriptFile << "referenced by "
		     << "the service provider information XML named"
		     << serviceProvider << "wasn't found";
	    return NULL;
	}
	
	kDebug() << "Using " << javaScriptFile << " as javaScript file for parsing";
	useRegExps = false;
	goto DepartureGroupTitlesTagNotComplete; // The naming could be better...
    }
    
    // <regExps>
    node = docElement.firstChildElement("regExps");
    if ( node.isNull() ) {
	kDebug() << "No <regExps> tag in service provider information XML named"
	    << serviceProvider;
	return NULL;
    }
    
    // <regExps><departures>
    childNode = node.firstChildElement("departures");
    if ( childNode.isNull() ) {
	if ( type == HTML ) {
	    kDebug() << "No child tag <departures> found in the <regExps> tag "
		"in service provider information XML named" << serviceProvider;
	    return NULL;
	}
    } else {
	// <regExps><departures> <regExpPre></regExpPre>
	subChildNode = childNode.firstChildElement("regExpPre");
	if ( !subChildNode.isNull() ) {
	    if ( subChildNode.toElement().hasAttribute("key") && subChildNode.toElement().hasAttribute("value") ) {
		infoPreKey = timetableInformationFromString(
			subChildNode.toElement().attribute("key", "") );
		infoPreValue = timetableInformationFromString(
			subChildNode.toElement().attribute("value", "") );
	    } else {
		kDebug() << "No attributes 'key' and 'value' found in "
		    "<regExps><departures><regExpPre> in service provider "
		    "information XML named" << serviceProvider;
		return NULL;
	    }

	    if ( subChildNode.firstChild().isCDATASection() ) {
		regExpDeparturesPre = subChildNode.firstChild().toCDATASection().data();
		if ( !QRegExp(regExpDeparturesPre).isValid() )
		    kDebug() << "The regular expression in <regExps><departures><regExpPre> "
			"is invalid." << QRegExp(regExpDeparturesPre).errorString();
	    }
	    else {
		kDebug() << "No CData section found in <regExpPre> in the "
		    "<departures> tag in the <regExps> tag in service provider "
		    "information XML named" << serviceProvider;
		return NULL;
	    }
	}

	// <regExps><departures> <regExp></regExp>
	subChildNode = childNode.firstChildElement("regExp");
	if ( subChildNode.isNull() ) {
	    kDebug() << "No child tag <regExp> found in the <departures> tag in "
		"the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}
	if ( subChildNode.firstChild().isCDATASection() ) {
	    regExpDepartures = subChildNode.firstChild().toCDATASection().data();
	    if ( !QRegExp(regExpDepartures).isValid() )
		kDebug() << "The regular expression in <regExps><departures><regExp> "
		    "is invalid." << QRegExp(regExpDepartures).errorString();
	}
	else {
	    kDebug() << "No CData section found in <regExp> in the <departures> "
		"tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}

	// <regExps><departures> <infos> <info>...</info>... </infos>
	subChildNode = childNode.firstChildElement("infos");
	if ( subChildNode.isNull() ) {
	    kDebug() << "No child tag <infos> found in the <departures> tag in "
		"the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}

	nodeList = subChildNode.childNodes();
	for ( uint i = 0; i < nodeList.length(); ++i ) {
	    if ( nodeList.at(i).isComment() )
		continue;
	    if ( nodeList.at(i).toElement().tagName() != "info" ) {
		kDebug() << "Warning: Invalid child tag (not <info>) found in "
		    "the <infos> tag in service provider information XML named"
		    << serviceProvider << "The name of the invalid tag is" << nodeList.at(i).toElement().tagName();
		continue;
	    }
	    if ( !nodeList.at(i).toElement().text().isEmpty() )
		infosDepartures << timetableInformationFromString( nodeList.at(i).toElement().text() );
	}
    }	// </departures>

    // <regExps><journeys>
    childNode = node.firstChildElement("journeys");
    if ( !childNode.isNull() ) {
	// <regExps><journeys><regExp>
	subChildNode = childNode.firstChildElement("regExp");
	if ( subChildNode.isNull() ) {
	    kDebug() << "No child tag <regExp> found in the <journeys> tag in "
		"the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}
	if ( subChildNode.firstChild().isCDATASection() ) {
	    regExpJourneys = subChildNode.firstChild().toCDATASection().data();
	    if ( !QRegExp(regExpJourneys).isValid() )
		kDebug() << "The regular expression in <regExps><journeys><regExp> "
			"is invalid." << QRegExp(regExpJourneys).errorString();
	}
	else {
	    kDebug() << "No CData section found in <regExp> in the <journeys> "
		"tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}

	// <regExps><journeys> <infos><info>...</info>..</infos>
	subChildNode = childNode.firstChildElement("infos");
	if ( subChildNode.isNull() ) {
	    kDebug() << "No child tag <infos> found in the <journeys> tag in "
		"the <regExps> tag in service provider information XML named"
		<< serviceProvider;
	    return NULL;
	}
	nodeList = subChildNode.childNodes();
	for ( uint i = 0; i < nodeList.length(); ++i ) {
	    if ( nodeList.at(i).isComment() )
		continue;
	    if ( nodeList.at(i).toElement().tagName() != "info" ) {
		kDebug() << "Warning: Invalid child tag (not <info>) found in "
		    "the <infos> tag in the <journeys> tag in the <regExps> tag "
		    "in service provider information XML named"
		    << serviceProvider << "The name of the invalid tag is"
		    << nodeList.at(i).toElement().tagName();
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
    childNode = node.firstChildElement("journeyNews");
    if ( !childNode.isNull() ) {
	nodeList = childNode.childNodes();
	for ( uint i = 0; i < nodeList.length(); ++i ) {
	    if ( nodeList.at(i).isComment() )
		continue;
	    if ( nodeList.at(i).toElement().tagName() != "item" ) {
		kDebug() << "Warning: Invalid child tag (not <item>) found in "
		    "the <journeyNews> tag in the <regExps> tag in service "
		    "provider information XML named"
		    << serviceProvider << "The name of the invalid tag is"
		    << nodeList.at(i).toElement().tagName();
		continue;
	    }

	    subChildNode = nodeList.at(i).toElement();
	    subSubChildNode = subChildNode.firstChildElement("regExp");
	    if ( subSubChildNode.isNull() ) {
		kDebug() << "No child tag <regExp> found in the <item> tag int "
		    "the <journeyNews> tag in the <regExps> tag in service "
		    "provider information XML named" << serviceProvider;
		return NULL;
	    }
	    if ( subSubChildNode.firstChild().isCDATASection() ) {
		regExpListJourneyNews << subSubChildNode.firstChild().toCDATASection().data();
		if ( !QRegExp(regExpListJourneyNews.last()).isValid() )
		    kDebug() << "The regular expression in <regExps><journeyNews><item><regExp> "
			"is invalid." << "item:" << i
			<< QRegExp(regExpListJourneyNews.last()).errorString();
	    }
	    else {
		kDebug() << "No CData section found in <regExp> in <item> in "
		    "the <journeyNews> tag in the <regExps> tag in service "
		    "provider information XML named" << serviceProvider;
		return NULL;
	    }

	    subSubChildNode = subChildNode.firstChildElement("infos");
	    if ( subSubChildNode.isNull() ) {
		kDebug() << "No child tag <infos> found in the <item> tag in "
		    "the <journeyNews> tag in the <regExps> tag in service "
		    "provider information XML named" << serviceProvider;
		return NULL;
	    }
	    QList<TimetableInformation> infoList;
	    subNodeList = subSubChildNode.childNodes();
	    for ( uint i = 0; i < subNodeList.length(); ++i ) {
// 		qDebug() << "subNodeList journey news infos:" << i << subNodeList.at(i).toElement().tagName();
		if ( nodeList.at(i).isComment() )
		    continue;

		if ( subNodeList.at(i).toElement().tagName() != "info" ) {
		    kDebug()
			<< "Warning: Invalid child tag (not <info>) found in the <infos> tag int the <item> tag in the <journeyNews> tag in the <regExps> tag in service provider information XML named"
			<< serviceProvider << "The name of the invalid tag is"
			<< subNodeList.at(i).toElement().tagName();
		    continue;
		}

		if ( !subNodeList.at(i).toElement().text().isEmpty() ) {
// 		    kDebug() << "timetableInformationFromString"
// 			<< subNodeList.at(i).toElement().text() << "=>"
// 			<< timetableInformationFromString( subNodeList.at(i).toElement().text() );
		    infoList << timetableInformationFromString( subNodeList.at(i).toElement().text() );
		} else {
		    kDebug()
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
    childNode = node.firstChildElement("possibleStops");
    if ( !childNode.isNull() ) {
	nodeList = childNode.childNodes();
	for ( uint i = 0; i < nodeList.length(); ++i ) {
	    if ( nodeList.at(i).isComment() )
		continue;
	    if ( nodeList.at(i).toElement().tagName() != "item" ) {
		kDebug()
		<< "Warning: Invalid child tag (not <item>) found in the <possibleStops> tag in the <regExps> tag in service provider information XML named"
		<< serviceProvider << "The name of the invalid tag is" << nodeList.at(i).toElement().tagName();
		continue;
	    }

	    subChildNode = nodeList.at(i).toElement();
	    subSubChildNode = subChildNode.firstChildElement("regExpRange");
	    if ( subSubChildNode.isNull() ) {
		kDebug() << "No child tag <regExpRange> found in the <item> tag "
		    "int the <possibleStops> tag in the <regExps> tag in service "
		    "provider information XML named" << serviceProvider;
		return NULL;
	    }
	    if ( subSubChildNode.firstChild().isCDATASection() )
		regExpListPossibleStopsRange << subSubChildNode.firstChild().toCDATASection().data();
	    else {
		kDebug() << "No CData section found in <regExpRange> in <item> "
		    "in the <possibleStops> tag in the <regExps> tag in service "
		    "provider information XML named" << serviceProvider;
		return NULL;
	    }

	    subChildNode = nodeList.at(i).toElement();
	    subSubChildNode = subChildNode.firstChildElement("regExp");
	    if ( subSubChildNode.isNull() ) {
		kDebug() << "No child tag <regExp> found in the <item> tag int "
		    "the <possibleStops> tag in the <regExps> tag in service "
		    "provider information XML named" << serviceProvider;
		return NULL;
	    }
	    if ( subSubChildNode.firstChild().isCDATASection() )
		regExpListPossibleStops << subSubChildNode.firstChild().toCDATASection().data();
	    else {
		kDebug() << "No CData section found in <regExp> in <item> in "
		    "the <possibleStops> tag in the <regExps> tag in service "
		    "provider information XML named" << serviceProvider;
		return NULL;
	    }

	    subSubChildNode = subChildNode.firstChildElement("infos");
	    if ( subSubChildNode.isNull() ) {
		kDebug() << "No child tag <infos> found in the <item> tag in "
		    "the <possibleStops> tag in the <regExps> tag in service "
		    "provider information XML named" << serviceProvider;
		return NULL;
	    }
	    QList<TimetableInformation> infos;
	    subNodeList = subSubChildNode.childNodes();
	    for ( uint i = 0; i < subNodeList.length(); ++i ) {
		if ( subNodeList.at(i).isComment() )
		    continue;
		if ( subNodeList.at(i).toElement().tagName() != "info" ) {
		    kDebug() << "Warning: Invalid child tag (not <info>) found "
			"in the <infos> tag int the <item> tag in the "
			"<possibleStops> tag in the <regExps> tag in service "
			"provider information XML named" << serviceProvider
			<< "The name of the invalid tag is"
			<< subNodeList.at(i).toElement().tagName();
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
    childNode = node.firstChildElement("departureGroupTitles");
    if ( !childNode.isNull() ) {
	// TODO: Test
	subChildNode = childNode.firstChildElement("regExp");
	if ( subChildNode.isNull() ) {
	    kDebug() << "Warning: No child tag <regExp> found in the "
		"<departureGroupTitles> tag in the <regExps> tag in service "
		"provider information XML named" << serviceProvider;
	    goto DepartureGroupTitlesTagNotComplete;
	}
	else if ( subChildNode.firstChild().isCDATASection() )
	    regExpDepartureGroupTitles = subSubChildNode.firstChild().toCDATASection().data();
	else {
	    kDebug() << "Warning: No CData section found in <regExp> in the "
		"<departureGroupTitles> tag in the <regExps> tag in service "
		"provider information XML named" << serviceProvider;
	    goto DepartureGroupTitlesTagNotComplete;
	}

	subChildNode = childNode.firstChildElement("infos");
	if ( subChildNode.isNull() ) {
	    kDebug() << "Warning: No child tag <infos> found in the "
		"<departureGroupTitles> tag in the <regExps> tag in service "
		"provider information XML named" << serviceProvider;
	    regExpDepartureGroupTitles = "";
	    goto DepartureGroupTitlesTagNotComplete;
	}
	subNodeList = subChildNode.childNodes();
	for ( uint i = 0; i < subNodeList.length(); ++i ) {
	    if ( subNodeList.at(i).isComment() )
		continue;
	    if ( subNodeList.at(i).toElement().tagName() != "info" ) {
		kDebug() << "Warning: Invalid child tag (not <info>) found in "
		    "the <infos> tag int the <departureGroupTitles> tag in the "
		    "<regExps> tag in service provider information XML named"
		    << serviceProvider << "The name of the invalid tag is" << subNodeList.at(i).toElement().tagName();
		continue;
	    }
	    if ( !subNodeList.at(i).toElement().text().isEmpty() )
		infosDepartureGroupTitles << timetableInformationFromString( subNodeList.at(i).toElement().text() );
	}
    }
    
DepartureGroupTitlesTagNotComplete:
    TimetableAccessorInfo accessorInfos( name, shortUrl, author, email, version,
					 serviceProvider, type );
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
    accessorInfos.setMinFetchWait( minFetchWait );
    accessorInfos.setDepartureRawUrl( rawUrlDepartures );
    accessorInfos.setStopSuggestionsRawUrl( rawUrlStopSuggestions );
    accessorInfos.setCharsetForUrlEncoding( charsetForUrlEncoding.toAscii() );
    accessorInfos.setJourneyRawUrl( rawUrlJourneys );

    // Set regular expressions
    if ( useRegExps ) {
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
    } else {
	accessorInfos.setScriptFile( javaScriptFile );
    }

    if ( type == HTML ) {
	if ( useRegExps )
	    return new TimetableAccessorHtml( accessorInfos );
	else {
	    TimetableAccessorHtmlScript *jsAccessor = new TimetableAccessorHtmlScript( accessorInfos );
	    if ( jsAccessor->isScriptLoaded() )
		return jsAccessor;
	    else
		return NULL; // Couldn't correctly load the script (bad script)
	}
    } else if ( type == XML )
	return new TimetableAccessorXml( accessorInfos );
    else {
	kDebug() << "Accessor type not supported" << type;
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
    else if ( sVehicleType == "Feet" )
	return Feet;
    else if ( sVehicleType == "Ferry" )
	return Ferry;
    else if ( sVehicleType == "Ship" )
	return Ship;
    else if ( sVehicleType == "Plane" )
	return Plane;
    else
	return Unknown;
}

TimetableInformation TimetableAccessor::timetableInformationFromString(
			const QString& sTimetableInformation ) {
    QString sInfo = sTimetableInformation.toLower();
    if ( sInfo == "nothing" )
	return Nothing;
    else if ( sInfo == "departuredate" )
	return DepartureDate;
    else if ( sInfo == "departurehour" )
	return DepartureHour;
    else if ( sInfo == "departureminute" )
	return DepartureMinute;
    else if ( sInfo == "typeofvehicle" )
	return TypeOfVehicle;
    else if ( sInfo == "transportline" )
	return TransportLine;
    else if ( sInfo == "flightnumber" )
	return FlightNumber;
    else if ( sInfo == "target" )
	return Target;
    else if ( sInfo == "platform" )
	return Platform;
    else if ( sInfo == "delay" )
	return Delay;
    else if ( sInfo == "delayreason" )
	return DelayReason;
    else if ( sInfo == "journeynews" )
	return JourneyNews;
    else if ( sInfo == "journeynewsother" )
	return JourneyNewsOther;
    else if ( sInfo == "journeynewslink" )
	return JourneyNewsLink;
    else if ( sInfo == "departurehourprognosis" )
	return DepartureHourPrognosis;
    else if ( sInfo == "departureminuteprognosis" )
	return DepartureMinutePrognosis;
    else if ( sInfo == "status" )
	return Status;
    else if ( sInfo == "departureyear" )
	return DepartureYear;
    else if ( sInfo == "routestops" )
	return RouteStops;
    else if ( sInfo == "routetimes" )
	return RouteTimes;
    else if ( sInfo == "routetimesdeparture" )
	return RouteTimesDeparture;
    else if ( sInfo == "routetimesarrival" )
	return RouteTimesArrival;
    else if ( sInfo == "routeexactstops" )
	return RouteExactStops;
    else if ( sInfo == "routetypesofvehicles" )
	return RouteTypesOfVehicles;
    else if ( sInfo == "routetransportlines" )
	return RouteTransportLines;
    else if ( sInfo == "routeplatformsdeparture" )
	return RoutePlatformsDeparture;
    else if ( sInfo == "routeplatformsarrival" )
	return RoutePlatformsArrival;
    else if ( sInfo == "routetimesdeparturedelay" )
	return RouteTimesDepartureDelay;
    else if ( sInfo == "routetimesarrivaldelay" )
	return RouteTimesArrivalDelay;
    else if ( sInfo == "operator" )
	return Operator;
    else if ( sInfo == "departureamorpm" )
	return DepartureAMorPM;
    else if ( sInfo == "departureamorpmprognosis" )
	return DepartureAMorPMPrognosis;
    else if ( sInfo == "arrivalamorpm" )
	return ArrivalAMorPM;
    else if ( sInfo == "duration" )
	return Duration;
    else if ( sInfo == "startstopname" )
	return StartStopName;
    else if ( sInfo == "startstopid" )
	return StartStopID;
    else if ( sInfo == "targetstopname" )
	return TargetStopName;
    else if ( sInfo == "targetstopid" )
	return TargetStopID;
    else if ( sInfo == "arrivaldate" )
	return ArrivalDate;
    else if ( sInfo == "arrivalhour" )
	return ArrivalHour;
    else if ( sInfo == "arrivalminute" )
	return ArrivalMinute;
    else if ( sInfo == "changes" )
	return Changes;
    else if ( sInfo == "typesofvehicleinjourney" )
	return TypesOfVehicleInJourney;
    else if ( sInfo == "pricing" )
	return Pricing;
    else if ( sInfo == "nomatchonschedule" )
	return NoMatchOnSchedule;
    else if ( sInfo == "stopname" )
	return StopName;
    else if ( sInfo == "stopid" )
	return StopID;
    else {
	kDebug() << sTimetableInformation
		 << "is an unknown timetable information value! Assuming value Nothing.";
	return Nothing;
    }
}

QStringList TimetableAccessor::features() const {
    QStringList list;
    
    if ( m_info.m_departureRawUrl.contains( "{dataType}" ) )
	list << "Arrivals";

    if ( m_info.scriptFileName().isEmpty() ) {
	if ( m_info.supportsStopAutocompletion() )
	    list << "Autocompletion";
	if ( !m_info.m_searchJourneys.regExp().isEmpty() )
	    list << "JourneySearch";
	if ( m_info.supportsTimetableAccessorInfo(Delay) )
	    list << "Delay";
	if ( m_info.supportsTimetableAccessorInfo(DelayReason) )
	    list << "DelayReason";
	if ( m_info.supportsTimetableAccessorInfo(Platform) )
	    list << "Platform";
	if ( m_info.supportsTimetableAccessorInfo(JourneyNews)
		    || m_info.supportsTimetableAccessorInfo(JourneyNewsOther)
		    || m_info.supportsTimetableAccessorInfo(JourneyNewsLink) )
	    list << "JourneyNews";
	if ( m_info.supportsTimetableAccessorInfo(TypeOfVehicle) )
	    list << "TypeOfVehicle";
	if ( m_info.supportsTimetableAccessorInfo(Status) )
	    list << "Status";
	if ( m_info.supportsTimetableAccessorInfo(Operator) )
	    list << "Operator";
	if ( m_info.supportsTimetableAccessorInfo(StopID) )
	    list << "StopID";
    } else
	list << scriptFeatures();

    list.removeDuplicates();
    return list;
}

QStringList TimetableAccessor::featuresLocalized() const {
    QStringList featuresl10n;
    QStringList featureList = features();

    if ( featureList.contains("Arrivals") )
	featuresl10n << i18nc("Support for getting arrivals for a stop of public "
			      "transport. This string is used in a feature list, "
			      "should be short.", "Arrivals");
    if ( featureList.contains("Autocompletion") )
	featuresl10n << i18nc("Autocompletion for names of public transport stops",
			      "Autocompletion");
    if ( featureList.contains("JourneySearch") )
	featuresl10n << i18nc("Support for getting journeys from one stop to another. "
			      "This string is used in a feature list, should be short.",
			      "Journey search");
    if ( featureList.contains("Delay") )
	featuresl10n << i18nc("Support for getting delay information. This string is "
			      "used in a feature list, should be short.", "Delay");
    if ( featureList.contains("DelayReason") )
	featuresl10n << i18nc("Support for getting the reason of a delay. This string "
			      "is used in a feature list, should be short.",
			      "Delay reason");
    if ( featureList.contains("Platform") )
	featuresl10n << i18nc("Support for getting the information from which platform "
			      "a public transport vehicle departs / at which it "
			      "arrives. This string is used in a feature list, "
			      "should be short.", "Platform");
    if ( featureList.contains("JourneyNews") )
	featuresl10n << i18nc("Support for getting the news about a journey with public "
			      "transport, such as a platform change. This string is "
			      "used in a feature list, should be short.", "Journey news");
    if ( featureList.contains("TypeOfVehicle") )
	featuresl10n << i18nc("Support for getting information about the type of "
			      "vehicle of a journey with public transport. This string "
			      "is used in a feature list, should be short.",
			      "Type of vehicle");
    if ( featureList.contains("Status") )
	featuresl10n << i18nc("Support for getting information about the status of a "
			      "journey with public transport or an aeroplane. This "
			      "string is used in a feature list, should be short.",
			      "Status");
    if ( featureList.contains("Operator") )
	featuresl10n << i18nc("Support for getting the operator of a journey with public "
			      "transport or an aeroplane. This string is used in a "
			      "feature list, should be short.", "Operator");
    if ( featureList.contains("StopID") )
	featuresl10n << i18nc("Support for getting the id of a stop of public transport. "
			      "This string is used in a feature list, should be short.",
			      "Stop ID");
    return featuresl10n;
}

KIO::TransferJob *TimetableAccessor::requestDepartures( const QString &sourceName,
		const QString &city, const QString &stop, int maxDeps,
		const QDateTime &dateTime, const QString &dataType,
		bool useDifferentUrl ) {
    KUrl url = getUrl( city, stop, maxDeps, dateTime, dataType, useDifferentUrl );
    kDebug() << url;
    
    KIO::TransferJob *job = KIO::get( url, KIO::NoReload, KIO::HideProgressInfo );
    int parseType = maxDeps == -1 ? static_cast<int>(ParseForStopSuggestions)
	    : static_cast<int>(ParseForDeparturesArrivals);
    m_jobInfos.insert( job, QVariantList() << parseType
	<< sourceName << city << stop << maxDeps << dateTime << dataType
	<< useDifferentUrl << (QUrl)url );
    m_document = "";

    connect( job, SIGNAL(data(KIO::Job*,QByteArray)),
	     this, SLOT(dataReceived(KIO::Job*,QByteArray)) );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(finished(KJob*)) );

    return job;
}

KIO::TransferJob* TimetableAccessor::requestStopSuggestions( const QString &sourceName,
							     const QString &city,
							     const QString &stop ) {
    if ( hasSpecialUrlForStopSuggestions() ) {
	KUrl url = getStopSuggestionsUrl( city, stop );
	KIO::TransferJob *job = KIO::get( url, KIO::NoReload, KIO::HideProgressInfo );
	m_jobInfos.insert( job, QVariantList() << static_cast<int>(ParseForStopSuggestions)
	    << sourceName<< city << stop << (QUrl)url  ); // TODO list ordering... replace by a hash?
	m_document = "";
	
	connect( job, SIGNAL(data(KIO::Job*,QByteArray)),
		 this, SLOT(dataReceived(KIO::Job*,QByteArray)) );
	connect( job, SIGNAL(finished(KJob*)), this, SLOT(finished(KJob*)) );
	
	return job;
    } else
	return requestDepartures( sourceName, city, stop, -1,
				  QDateTime::currentDateTime() );
}

KIO::TransferJob *TimetableAccessor::requestJourneys( const QString &sourceName,
		const QString &city, const QString &startStopName,
		const QString &targetStopName, int maxDeps,
		const QDateTime &dateTime, const QString &dataType,
		bool useDifferentUrl ) {
    // Creating a kioslave
    KUrl url = getJourneyUrl( city, startStopName, targetStopName, maxDeps,
			      dateTime, dataType, useDifferentUrl );
    KIO::TransferJob *job = requestJourneys( url );
    m_jobInfos.insert( job, QVariantList() << static_cast<int>(ParseForJourneys)
	    << sourceName << city << startStopName << maxDeps << dateTime
	    << dataType << useDifferentUrl << (QUrl)url << targetStopName << 0 );

    return job;
}

KIO::TransferJob* TimetableAccessor::requestJourneys( const KUrl& url ) {
    kDebug() << url;
    
    m_document = "";
    KIO::TransferJob *job = KIO::get( url, KIO::NoReload, KIO::HideProgressInfo );
    
    connect( job, SIGNAL(data(KIO::Job*,QByteArray)),
	     this, SLOT(dataReceived(KIO::Job*,QByteArray)) );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(finished(KJob*)) );
    
    return job;
}

void TimetableAccessor::dataReceived ( KIO::Job*, const QByteArray& data ) {
    m_document += data;
}

void TimetableAccessor::finished( KJob* job ) {
    QList<QVariant> jobInfo = m_jobInfos.value( job );
    m_jobInfos.remove( job );
    
    QList< PublicTransportInfo* > *dataList;
    QStringList *stops;
    QHash< QString, QString > *stopToStopId;
    ParseDocumentMode parseDocumentMode = static_cast<ParseDocumentMode>( jobInfo.at(0).toInt() );
    kDebug() << "\n      FINISHED:" << parseDocumentMode;
    
    QString sourceName = jobInfo.at(1).toString();
    QString city = jobInfo.at(2).toString();
    QString stop = jobInfo.at(3).toString();
    if ( parseDocumentMode == ParseForStopSuggestions ) {
	QUrl url = jobInfo.at(4).toUrl();

	kDebug() << "Stop suggestions request finished" << sourceName << city << stop;
	if ( (stops = new QStringList)
		&& (stopToStopId = new QHash<QString, QString>)
		&& parseDocumentPossibleStops(stops, stopToStopId) ) {
	    kDebug() << "finished parsing for stop suggestions";
	    emit stopListReceived( this, url, *stops, *stopToStopId, serviceProvider(),
				   sourceName, city, stop, QString(), parseDocumentMode );
	    kDebug() << "emit stopListReceived finished";
	} else {
	    kDebug() << "error parsing for stop suggestions";
	    emit errorParsing( this, url, serviceProvider(), sourceName, city,
			       stop, QString(), parseDocumentMode );
	}

	return;
    }
    
    int maxDeps = jobInfo.at(4).toInt();
    QDateTime dateTime = jobInfo.at(5).toDateTime();
    QString dataType = jobInfo.at(6).toString();
    bool usedDifferentUrl = jobInfo.at(7).toBool();
    QUrl url = jobInfo.at(8).toUrl();
    QString targetStop;
    int roundTrips = 0;
    if ( parseDocumentMode == ParseForJourneys ) {
	targetStop = jobInfo.at(9).toString();
	roundTrips = jobInfo.at(10).toInt();
	kDebug() << "\n    FINISHED JOURNEY SEARCH" << roundTrips;
    }
    m_curCity = city;

//     kDebug() << "usedDifferentUrl" << usedDifferentUrl;
    if ( !usedDifferentUrl ) {
	QString sNextUrl;
	if ( parseDocumentMode == ParseForJourneys ) {
	    if ( roundTrips < 2 )
		sNextUrl = parseDocumentForLaterJourneysUrl();
	    else if ( roundTrips == 2 )
		sNextUrl = parseDocumentForDetailedJourneysUrl();
	}
	kDebug() << "     PARSE RESULTS" << parseDocumentMode;
	
	dataList = new QList<PublicTransportInfo*>();
	if ( parseDocument(dataList, parseDocumentMode) ) {
	    if ( parseDocumentMode == ParseForDeparturesArrivals ) {
// 		kDebug() << "emit departureListReceived" << sourceName;
		QList<DepartureInfo*>* departures = new QList<DepartureInfo*>();
		foreach( PublicTransportInfo *info, *dataList )
		    departures->append( (DepartureInfo*)info );
		emit departureListReceived( this, url, *departures, serviceProvider(),
					    sourceName, city, stop, dataType,
					    parseDocumentMode );
	    } else if ( parseDocumentMode == ParseForJourneys ) {
// 		kDebug() << "emit journeyListReceived";
		QList<JourneyInfo*>* journeys = new QList<JourneyInfo*>();
		foreach( PublicTransportInfo *info, *dataList )
		    journeys->append( (JourneyInfo*)info );
		emit journeyListReceived( this, url, *journeys, serviceProvider(),
					  sourceName, city, stop, dataType,
					  parseDocumentMode );
	    }
	} else if ( hasSpecialUrlForStopSuggestions() ) {
// 	    kDebug() << "request possible stop list";
	    requestDepartures( sourceName, m_curCity, stop, maxDeps, dateTime,
			       dataType, true );
	} else if ( (stops = new QStringList)
		&& (stopToStopId = new QHash<QString, QString>)
		&& parseDocumentPossibleStops(stops, stopToStopId) ) {
	    kDebug() << "Stop suggestion list received" << parseDocumentMode;
	    emit stopListReceived( this, url, *stops, *stopToStopId, serviceProvider(),
				   sourceName, city, stop, dataType, parseDocumentMode );
	} else
	    emit errorParsing( this, url, serviceProvider(), sourceName, city,
			       stop, dataType, parseDocumentMode );

	if ( parseDocumentMode == ParseForJourneys ) {
	    if ( !sNextUrl.isNull() && !sNextUrl.isEmpty() ) {
		kDebug() << "\n\n     REQUEST PARSED URL:   " << sNextUrl << "\n\n";
		++roundTrips;
		KIO::TransferJob *job = requestJourneys( KUrl(sNextUrl) );
		m_jobInfos.insert( job, QList<QVariant>() << static_cast<int>(ParseForJourneys)
		    << sourceName << city << stop << maxDeps << dateTime << dataType
		    << usedDifferentUrl << (QUrl)url << targetStop << roundTrips );
// 		return;
	    }
	}
    } else if ( (stops = new QStringList)
	    && (stopToStopId = new QHash<QString, QString>()) != NULL
	    && parseDocumentPossibleStops(stops, stopToStopId) ) {
// 	kDebug() << "possible stop list received ok";
	emit stopListReceived( this, url, *stops, *stopToStopId, serviceProvider(), sourceName,
			       city, stop, dataType, parseDocumentMode );
    } else
	emit errorParsing( this, url, serviceProvider(), city, sourceName,
			   stop, dataType, parseDocumentMode );
}

KUrl TimetableAccessor::getUrl( const QString &city, const QString &stop,
				int maxDeps, const QDateTime &dateTime,
				const QString &dataType, bool useDifferentUrl ) const {
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
    sRawUrl = sRawUrl.replace( "{time}", sTime )
		     .replace( "{maxDeps}", QString("%1").arg(maxDeps) )
		     .replace( "{stop}", sStop )
		     .replace( "{dataType}", sDataType );

    QRegExp rx = QRegExp("\\{date:([^\\}]*)\\}", Qt::CaseInsensitive);
    if ( rx.indexIn(sRawUrl) != -1 )
	sRawUrl.replace( rx, dateTime.date().toString(rx.cap(1)) );

    return KUrl( sRawUrl );
}

KUrl TimetableAccessor::getStopSuggestionsUrl( const QString &city,
					       const QString& stop ) {
    QString sRawUrl = stopSuggestionsRawUrl();
    QString sCity = city.toLower(), sStop = stop.toLower();
    
    // Encode stop
    if ( charsetForUrlEncoding().isEmpty() ) {
	sCity = QString::fromAscii(QUrl::toPercentEncoding(sCity));
	sStop = QString::fromAscii(QUrl::toPercentEncoding(sStop));
    } else {
	sCity = toPercentEncoding( sCity, charsetForUrlEncoding() );
	sStop = toPercentEncoding( sStop, charsetForUrlEncoding() );
    }
    
    if ( useSeperateCityValue() )
	sRawUrl = sRawUrl.replace( "{city}", sCity );
    sRawUrl = sRawUrl.replace( "{stop}", sStop );

    return KUrl( sRawUrl );
}

KUrl TimetableAccessor::getJourneyUrl( const QString& city,
				       const QString& startStopName,
				       const QString& targetStopName,
				       int maxDeps, const QDateTime &dateTime,
				       const QString& dataType,
				       bool useDifferentUrl ) const {
    Q_UNUSED( useDifferentUrl );

    QString sRawUrl = m_info.journeyRawUrl();
    QString sTime = dateTime.time().toString("hh:mm");
    QString sDataType;
    QString sCity = city.toLower(), sStartStopName = startStopName.toLower(),
	    sTargetStopName = targetStopName.toLower();
    if ( dataType == "arrivals" || dataType == "journeysArr" )
	sDataType = "arr";
    else if ( dataType == "departures" || dataType == "journeysDep" )
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

    sRawUrl = sRawUrl.replace( "{time}", sTime )
		     .replace( "{maxDeps}", QString("%1").arg(maxDeps) )
		     .replace( "{startStop}", sStartStopName )
		     .replace( "{targetStop}", sTargetStopName )
		     .replace( "{dataType}", sDataType );

    QRegExp rx = QRegExp("\\{date:([^\\}]*)\\}", Qt::CaseInsensitive);
    if ( rx.indexIn(sRawUrl) != -1 )
	sRawUrl.replace( rx, dateTime.date().toString(rx.cap(1)) );
    
    rx = QRegExp("\\{dep=([^\\|]*)\\|arr=([^\\}]*)\\}", Qt::CaseInsensitive);
    if ( rx.indexIn(sRawUrl) != -1 ) {
	if ( sDataType == "arr" )
	    sRawUrl.replace( rx, rx.cap(2) );
	else
	    sRawUrl.replace( rx, rx.cap(1) );
    }
    
    return KUrl( sRawUrl );
}

QString TimetableAccessor::gethex( ushort decimal ) {
    QString hexchars = "0123456789ABCDEFabcdef";
    return "%" + hexchars[decimal >> 4] + hexchars[decimal & 0xF];
}

QString TimetableAccessor::toPercentEncoding( QString str, QByteArray charset ) {
    QString unreserved = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~";
//     QString reserved = "!*'();:@&=+$,/?%#[]";
    QString encoded;

    QByteArray ba = QTextCodec::codecForName(charset)->fromUnicode(str);
    for ( int i = 0; i < ba.length(); ++i ) {
	char ch = ba[i];
	if ( unreserved.indexOf(ch) != -1 ) {
	    encoded += ch;
// 	    qDebug() << "  Unreserved char" << encoded;
	} else if ( ch < 0 ) {
	    encoded += gethex(256 + ch);
// 	    qDebug() << "  Encoding char < 0" << encoded;
	} else {
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

bool TimetableAccessor::parseDocumentPossibleStops( QStringList *stops,
				QHash<QString,QString> *stopToStopId ) {
    Q_UNUSED( stops );
    Q_UNUSED( stopToStopId );
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

TimetableAccessorInfo TimetableAccessor::timetableAccessorInfo() const {
    return m_info;
}




