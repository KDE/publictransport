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

#include "accessorinfoxmlreader.h"

#include "timetableaccessor.h"

#include <KLocalizedString>
#include <KLocale>
#include <KDebug>
#include <QFile>
#include <QFileInfo>
#include "timetableaccessor_html.h"
#include "timetableaccessor_html_script.h"
#include "timetableaccessor_xml.h"


TimetableAccessor* AccessorInfoXmlReader::read( QIODevice* device,
						const QString &serviceProvider,
						const QString &fileName,
						const QString &country ) {
    Q_ASSERT( device );

    bool closeAfterRead; // Only close after reading if it wasn't open before
    if ( (closeAfterRead = !device->isOpen()) && !device->open(QIODevice::ReadOnly) )
	return NULL;
    setDevice( device );
    
    TimetableAccessor *ret = NULL;
    while ( !atEnd() ) {
	readNext();
	
	if ( isStartElement() ) {
	    if ( name().compare("accessorInfo", Qt::CaseInsensitive) == 0
			&& attributes().value("fileVersion") == "1.0" ) {
		ret = readAccessorInfo( serviceProvider, fileName, country );
		break;
	    } else
		raiseError( "The file is not a public transport accessor info "
			    "version 1.0 file." );
	}
    }
    
    if ( closeAfterRead )
	device->close();
    
    if ( error() )
	return NULL;
    else
	return ret;
}

void AccessorInfoXmlReader::readUnknownElement() {
    Q_ASSERT( isStartElement() );
    
    while ( !atEnd() ) {
	readNext();
	
	if ( isEndElement() )
	    break;
	
	if ( isStartElement() )
	    readUnknownElement();
    }
}

TimetableAccessor* AccessorInfoXmlReader::readAccessorInfo( const QString &serviceProvider,
							    const QString &fileName,
							    const QString &country ) {
    QString lang = KGlobal::locale()->country();
    QString langRead;

    AccessorType type;
    QString version, nameLocal, nameEn, descriptionLocal, descriptionEn,
	    authorName, authorEmail, defaultVehicleType, url, shortUrl,
	    charsetForUrlEncoding, fallbackCharset, scriptFile, credit,
	    rawUrlDepartures, rawUrlStopSuggestions, rawUrlJourneys;
    QStringList cities;
    QHash<QString, QString> cityNameReplacements;
    bool useSeperateCityValue = false, onlyUseCitiesInList = false;
    int minFetchWait = 0;
    
    QString regExpDepartureGroupTitles, regExpDepartures, regExpDeparturesPre, regExpJourneys;
    QList<TimetableInformation> infosDepartureGroupTitles, infosDepartures, infosJourneys;
    QList< QList<TimetableInformation> > infosListPossibleStops, infosListJourneyNews;
    QStringList regExpListPossibleStopsRange, regExpListPossibleStops, regExpListJourneyNews;
    TimetableInformation infoPreKey = Nothing, infoPreValue = Nothing;
    
    
    if ( attributes().hasAttribute("version") )
	version = attributes().value( "version" ).toString();
    else
	version = "1.0";
    
    if ( attributes().hasAttribute("type") ) {
	type = TimetableAccessor::accessorTypeFromString( attributes().value("type").toString() );
	if ( type == NoAccessor ) {
	    kDebug() << "The type" << attributes().value("type").toString()
		     << "is invalid. Currently there are two values allowed: "
			"HTML and XML.";
	    return NULL;
	}
    } else
	type = HTML;
    
    while ( !atEnd() ) {
	readNext();
	
	if ( isEndElement() && name().compare("accessorInfo", Qt::CaseInsensitive) == 0 )
	    break;
	
	if ( isStartElement() ) {
	    if ( name().compare("name", Qt::CaseInsensitive) == 0 ) {
		QString nameRead = readLocalizedTextElement( &langRead );
		if ( langRead == lang )
		    nameLocal = nameRead;
		else
		    nameEn = nameRead;
	    } else if ( name().compare("description", Qt::CaseInsensitive) == 0 ) {
		QString descriptionRead = readLocalizedTextElement( &langRead );
		if ( langRead == lang )
		    descriptionLocal = descriptionRead;
		else
		    descriptionEn = descriptionRead;
	    } else if ( name().compare("author", Qt::CaseInsensitive) == 0 ) {
		readAuthor( &authorName, &authorEmail );
	    } else if ( name().compare("cities", Qt::CaseInsensitive) == 0 ) {
		readCities( &cities, &cityNameReplacements );
	    } else if ( name().compare("useSeperateCityValue", Qt::CaseInsensitive) == 0 ) {
		useSeperateCityValue = readBooleanElement();
	    } else if ( name().compare("onlyUseCitiesInList", Qt::CaseInsensitive) == 0 ) {
		onlyUseCitiesInList = readBooleanElement();
	    } else if ( name().compare("defaultVehicleType", Qt::CaseInsensitive) == 0 ) {
		defaultVehicleType = readElementText();
	    } else if ( name().compare("url", Qt::CaseInsensitive) == 0 ) {
		url = readElementText();
	    } else if ( name().compare("shortUrl", Qt::CaseInsensitive) == 0 ) {
		shortUrl = readElementText();
	    } else if ( name().compare("minFetchWait", Qt::CaseInsensitive) == 0 ) {
		minFetchWait = readElementText().toInt();
	    } else if ( name().compare("charsetForUrlEncoding", Qt::CaseInsensitive) == 0 ) {
		charsetForUrlEncoding = readElementText();
	    } else if ( name().compare("fallbackCharset", Qt::CaseInsensitive) == 0 ) {
		fallbackCharset = readElementText(); // TODO Implement as attributes in the url tags
	    } else if ( name().compare("rawUrls", Qt::CaseInsensitive) == 0 ) {
		readRawUrls( &rawUrlDepartures, &rawUrlStopSuggestions, &rawUrlJourneys );
	    } else if ( name().compare("regExps", Qt::CaseInsensitive) == 0 ) {
		bool ok = readRegExps( &regExpDepartures, &infosDepartures,
			     &regExpDeparturesPre, &infoPreKey, &infoPreValue,
			     &regExpJourneys, &infosJourneys,
			     &regExpDepartureGroupTitles, &infosDepartureGroupTitles,
			     &regExpListPossibleStopsRange, &regExpListPossibleStops,
			     &infosListPossibleStops,
			     &regExpListJourneyNews, &infosListJourneyNews );
		if ( !ok )
		    return NULL;
	    } else if ( name().compare("script", Qt::CaseInsensitive) == 0 ) {
		scriptFile = QFileInfo( fileName ).path()  + "/" + readElementText();
		if ( !QFile::exists(scriptFile) ) {
		    kDebug() << "The script file " << scriptFile << "referenced by "
			     << "the service provider information XML named"
			     << nameEn << "wasn't found";
		    return NULL;
		}
	    } else if ( name().compare("credit", Qt::CaseInsensitive) == 0 ) {
		credit = readElementText();
	    } else
		readUnknownElement();
	}
    }

    if ( url.isEmpty() ) {
	raiseError( "No <url> tag in accessor info XML" );
	return NULL;
    }
    if ( type == HTML && rawUrlDepartures.isEmpty() ) {
	raiseError( "No raw url for departures in accessor info XML, mandatory for HTML types" );
	return NULL;
    }
    
    if ( nameLocal.isEmpty() )
	nameLocal = nameEn;
    if ( descriptionLocal.isEmpty() )
	descriptionLocal = descriptionEn;
    if ( shortUrl.isEmpty() )
	shortUrl = url;
    
    TimetableAccessorInfo accessorInfos( nameLocal, shortUrl, authorName, authorEmail,
					 version, serviceProvider, type );
    accessorInfos.setFileName( fileName );
    accessorInfos.setCountry( country );
    accessorInfos.setCities( cities );
    accessorInfos.setCredit( credit );
    accessorInfos.setCityNameToValueReplacementHash( cityNameReplacements );
    accessorInfos.setUseSeperateCityValue( useSeperateCityValue );
    accessorInfos.setOnlyUseCitiesInList( onlyUseCitiesInList );
    accessorInfos.setDescription( descriptionLocal );
    accessorInfos.setDefaultVehicleType( TimetableAccessor::vehicleTypeFromString(defaultVehicleType) );
    accessorInfos.setUrl( url );
    accessorInfos.setShortUrl( shortUrl );
    accessorInfos.setMinFetchWait( minFetchWait );
    accessorInfos.setDepartureRawUrl( rawUrlDepartures );
    accessorInfos.setStopSuggestionsRawUrl( rawUrlStopSuggestions );
    accessorInfos.setFallbackCharset( fallbackCharset.toAscii() );
    accessorInfos.setCharsetForUrlEncoding( charsetForUrlEncoding.toAscii() );
    accessorInfos.setJourneyRawUrl( rawUrlJourneys );

    // Set regular expressions, if no script file was specified
    if ( scriptFile.isEmpty() ) {
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
	    accessorInfos.addRegExpPossibleStops( regExpPossibleStopsRange,
					regExpPossibleStops, infosPossibleStops );
	}
    } else {
	accessorInfos.setScriptFile( scriptFile );
    }

    if ( type == HTML ) {
	if ( scriptFile.isEmpty() )
	    return new TimetableAccessorHtml( accessorInfos );
	else {
	    TimetableAccessorHtmlScript *jsAccessor = new TimetableAccessorHtmlScript( accessorInfos );
	    if ( jsAccessor->isScriptLoaded() )
		return jsAccessor;
	    else
		return NULL; // Couldn't correctly load the script (bad script)
	}
    } else if ( type == XML ) {
	return new TimetableAccessorXml( accessorInfos );
    } else {
	kDebug() << "Accessor type not supported" << type;
	return NULL;
    }
}

QString AccessorInfoXmlReader::readLocalizedTextElement( QString *lang ) {
    if ( attributes().hasAttribute("lang") )
	*lang = attributes().value( "lang" ).toString();
    else
	*lang = "en";

    return readElementText();
}

bool AccessorInfoXmlReader::readBooleanElement() {
    QString content = readElementText().trimmed();
    if ( content.compare("true", Qt::CaseInsensitive) == 0 || content == "1" )
	return true;
    else
	return false;
}

void AccessorInfoXmlReader::readAuthor( QString *fullname, QString *email ) {
    while ( !atEnd() ) {
	readNext();
	
	if ( isEndElement() && name().compare("author", Qt::CaseInsensitive) == 0 )
	    break;

	if ( isStartElement() ) {
	    if ( name().compare("fullName", Qt::CaseInsensitive) == 0 )
		*fullname = readElementText();
	    else if ( name().compare("email", Qt::CaseInsensitive) == 0 )
		*email = readElementText();
	    else
		readUnknownElement();
	}
    }
}

void AccessorInfoXmlReader::readCities( QStringList *cities,
					QHash< QString, QString > *cityNameReplacements ) {
    while ( !atEnd() ) {
	readNext();
	
	if ( isEndElement() && name().compare("cities", Qt::CaseInsensitive) == 0 )
	    break;
	
	if ( isStartElement() ) {
	    if ( name().compare("city", Qt::CaseInsensitive) == 0 ) {
		if ( attributes().hasAttribute("replaceWith") ) {
		    QString replacement = attributes().value("replaceWith").toString().toLower();
		    QString city = readElementText();
		    cityNameReplacements->insert( city.toLower(), replacement );
		    cities->append( city );
		} else {
		    QString city = readElementText();
		    cities->append( city );
		}
	    } else
		readUnknownElement();
	}
    }
}

void AccessorInfoXmlReader::readRawUrls( QString* rawUrlDepartures,
					 QString* rawUrlStopSuggestions,
					 QString* rawUrlJourneys ) {
    while ( !atEnd() ) {
	readNext();
	
	if ( isEndElement() && name().compare("rawUrls", Qt::CaseInsensitive) == 0 )
	    break;
	
	if ( isStartElement() ) {
	    if ( name().compare("departures", Qt::CaseInsensitive) == 0 ) {
		*rawUrlDepartures = readElementText();
	    } else if ( name().compare("stopSuggestions", Qt::CaseInsensitive) == 0 ) {
		*rawUrlStopSuggestions = readElementText();
	    } else if ( name().compare("journeys", Qt::CaseInsensitive) == 0 ) {
		*rawUrlJourneys = readElementText();
	    } else
		readUnknownElement();
	}
    }
}

bool AccessorInfoXmlReader::readRegExps( QString* regExpDepartures,
		QList< TimetableInformation >* infosDepartures,
		QString* regExpDeparturesPre, TimetableInformation* infoPreKey,
		TimetableInformation* infoPreValue, QString* regExpJourneys,
		QList< TimetableInformation >* infosJourneys,
		QString* regExpDepartureGroupTitles,
		QList< TimetableInformation >* infosDepartureGroupTitles,
		QStringList* regExpListPossibleStopsRange,
		QStringList* regExpListPossibleStops,
		QList< QList< TimetableInformation > >* infosListPossibleStops,
		QStringList* regExpListJourneyNews,
		QList< QList< TimetableInformation > >* infosListJourneyNews ) {
    while ( !atEnd() ) {
	readNext();
	
	if ( isEndElement() && name().compare("regExps", Qt::CaseInsensitive) == 0 )
	    break;
	
	if ( isStartElement() ) {
	    if ( name().compare("departures", Qt::CaseInsensitive) == 0 ) {
		if ( !readRegExp(regExpDepartures, infosDepartures,
				 regExpDeparturesPre, infoPreKey, infoPreValue) )
		    return false;
	    } else if ( name().compare("journeys", Qt::CaseInsensitive) == 0 ) {
		if ( !readRegExp(regExpJourneys, infosJourneys) )
		    return false;
	    } else if ( name().compare("journeyNews", Qt::CaseInsensitive) == 0 ) {
		if ( !readRegExpItems(regExpListJourneyNews, infosListJourneyNews) )
		    return false;
	    } else if ( name().compare("possibleStops", Qt::CaseInsensitive) == 0 ) {
		if ( !readRegExpItems(regExpListPossibleStops, infosListPossibleStops,
				      regExpListPossibleStopsRange) )
		    return false;
	    } else if ( name().compare("departureGroupTitles", Qt::CaseInsensitive) == 0 ) {
		if ( !readRegExp(regExpDepartureGroupTitles, infosDepartureGroupTitles) )
		    return false;
	    } else
		readUnknownElement();
	}
    }
    
    return true;
}

bool AccessorInfoXmlReader::readRegExp( QString* regExp,
					QList< TimetableInformation >* infos,
					QString* regExpPreOrRanges,
					TimetableInformation* infoPreKey,
					TimetableInformation* infoPreValue ) {
    QStringRef tagName = name();
    while ( !atEnd() ) {
	readNext();
	
	if ( isEndElement() && name().compare(tagName, Qt::CaseInsensitive) == 0 )
	    break;
	
	if ( isStartElement() ) {
	    if ( name().compare("regExp", Qt::CaseInsensitive) == 0 ) {
		*regExp = readElementText();
		if ( !QRegExp(*regExp).isValid() ) {
		    raiseError( QString("The regular expression in <%1> is invalid: %2.")
				.arg(tagName.toString(), QRegExp(*regExp).errorString()) );
		    return false;
		}
	    } else if ( name().compare("infos", Qt::CaseInsensitive) == 0 ) {
		readRegExpInfos( infos );
		// 		regExpPreOrRanges regExpRange
	    } else if ( regExpPreOrRanges && !infoPreKey && !infoPreValue
			&& name().compare("regExpRange", Qt::CaseInsensitive) == 0 ) {
		*regExpPreOrRanges = readElementText();
		if ( !QRegExp(*regExpPreOrRanges).isValid() ) {
		    raiseError( QString("The regular expression (range) in <%1> is invalid: %2.")
				.arg(tagName.toString(), QRegExp(*regExpPreOrRanges).errorString()) );
		    return false;
		}
	    } else if ( regExpPreOrRanges && infoPreKey && infoPreValue
			&& name().compare("regExpPre", Qt::CaseInsensitive) == 0 ) {
		if ( !readRegExpPre(regExpPreOrRanges, infoPreKey, infoPreValue) )
		    return false;
	    } else
		readUnknownElement();
	}
    }
    
    return true;
}

void AccessorInfoXmlReader::readRegExpInfos( QList< TimetableInformation >* infos ) {
    while ( !atEnd() ) {
	readNext();
	
	if ( isEndElement() && name().compare("infos", Qt::CaseInsensitive) == 0 )
	    break;
	
	if ( isStartElement() ) {
	    if ( name().compare("info", Qt::CaseInsensitive) == 0 ) {
		TimetableInformation info =
			TimetableAccessor::timetableInformationFromString( readElementText() );
		infos->append( info );
	    } else
		readUnknownElement();
	}
    }
}

bool AccessorInfoXmlReader::readRegExpPre( QString* regExpPre,
					   TimetableInformation* infoPreKey,
					   TimetableInformation* infoPreValue ) {
    if ( !attributes().hasAttribute("key") || !attributes().hasAttribute("value") ) {
	raiseError( "Missing attributes in <RegExpPre> tag (key and value are needed)" );
	return false;
    }

    *infoPreKey = TimetableAccessor::timetableInformationFromString(
		    attributes().value("key").toString() );
    *infoPreValue = TimetableAccessor::timetableInformationFromString(
		    attributes().value("value").toString() );
    *regExpPre = readElementText();
    
    return true;
}

bool AccessorInfoXmlReader::readRegExpItems( QStringList* regExps,
					     QList< QList< TimetableInformation > >* infosList,
					     QStringList *regExpsRanges ) {
    QStringRef tagName = name();
    while ( !atEnd() ) {
	readNext();
	
	if ( isEndElement() && name().compare(tagName, Qt::CaseInsensitive) == 0 )
	    break;
	
	if ( isStartElement() ) {
	    if ( name().compare("item", Qt::CaseInsensitive) == 0 ) {
		QString regExp, regExpRange;
		QList< TimetableInformation > infos;
		if ( !readRegExp(&regExp, &infos, regExpsRanges ? &regExpRange : NULL) )
		    return false;

		regExps->append( regExp );
		infosList->append( infos );
		if ( regExpsRanges )
		    regExpsRanges->append( regExpRange );
	    } else
		readUnknownElement();
	}
    }

    return true;
}

