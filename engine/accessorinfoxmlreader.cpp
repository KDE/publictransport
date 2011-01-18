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
#include "timetableaccessor_html.h"
#include "timetableaccessor_html_script.h"
#include "timetableaccessor_xml.h"

#include <KLocalizedString>
#include <KLocale>
#include <KDebug>
#include <QFile>
#include <QFileInfo>

TimetableAccessor* AccessorInfoXmlReader::read( QIODevice* device,
		const QString &serviceProvider, const QString &fileName, const QString &country )
{
	Q_ASSERT( device );

	bool closeAfterRead; // Only close after reading if it wasn't open before
	if ( (closeAfterRead = !device->isOpen()) && !device->open(QIODevice::ReadOnly) ) {
		raiseError( "Couldn't read the file \"" + fileName + "\"." );
		return NULL;
	}
	setDevice( device );

	TimetableAccessor *ret = NULL;
	while ( !atEnd() ) {
		readNext();

		if ( isStartElement() ) {
			if ( name().compare( "accessorInfo", Qt::CaseInsensitive ) == 0
						&& attributes().value( "fileVersion" ) == "1.0" ) {
				ret = readAccessorInfo( serviceProvider, fileName, country );
				break;
			} else {
				raiseError( "The file is not a public transport accessor info "
							"version 1.0 file." );
			}
		}
	}

	if ( closeAfterRead ) {
		device->close();
	}

	return error() ? NULL : ret;
}

void AccessorInfoXmlReader::readUnknownElement()
{
	Q_ASSERT( isStartElement() );

	while ( !atEnd() ) {
		readNext();

		if ( isEndElement() ) {
			break;
		}

		if ( isStartElement() ) {
			readUnknownElement();
		}
	}
}

TimetableAccessor* AccessorInfoXmlReader::readAccessorInfo( const QString &serviceProvider,
		const QString &fileName, const QString &country )
{
	QString lang = KGlobal::locale()->country();
	QString langRead;

	AccessorType type;
	QString version, nameLocal, nameEn, descriptionLocal, descriptionEn,
			authorName, authorEmail, defaultVehicleType, url, shortUrl,
			charsetForUrlEncoding, fallbackCharset, scriptFile, credit;
	QString rawUrlDepartures, rawUrlStopSuggestions, rawUrlJourneys;
	QString sessionKeyUrl, sessionKeyData;
	SessionKeyPlace sessionKeyPlace;
	QHash<QString, QString> attributesForDepartures, attributesForStopSuggestions, attributesForJourneys;
	QStringList cities;
	QHash<QString, QString> cityNameReplacements;
	bool useSeperateCityValue = false, onlyUseCitiesInList = false;
	int minFetchWait = 0;

	QString regExpDepartureGroupTitles, regExpDepartures, regExpDeparturesPre, regExpJourneys;
	QList<TimetableInformation> infosDepartureGroupTitles, infosDepartures, infosJourneys;
	QList< QList<TimetableInformation> > infosListPossibleStops, infosListJourneyNews;
	QStringList regExpListPossibleStopsRange, regExpListPossibleStops, regExpListJourneyNews;
	TimetableInformation infoPreKey = Nothing, infoPreValue = Nothing;


	if ( attributes().hasAttribute( "version" ) ) {
		version = attributes().value( "version" ).toString();
	} else {
		version = "1.0";
	}

	if ( attributes().hasAttribute( "type" ) ) {
		type = TimetableAccessor::accessorTypeFromString( attributes().value( "type" ).toString() );
		if ( type == NoAccessor ) {
			kDebug() << "The type" << attributes().value( "type" ).toString()
					 << "is invalid. Currently there are two values allowed: HTML and XML.";
			return NULL;
		}
	} else {
		type = HTML;
	}

	while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare( "accessorInfo", Qt::CaseInsensitive ) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare( "name", Qt::CaseInsensitive ) == 0 ) {
				QString nameRead = readLocalizedTextElement( &langRead );
				if ( langRead == lang ) {
					nameLocal = nameRead;
				} else {
					nameEn = nameRead;
				}
			} else if ( name().compare( "description", Qt::CaseInsensitive ) == 0 ) {
				QString descriptionRead = readLocalizedTextElement( &langRead );
				if ( langRead == lang ) {
					descriptionLocal = descriptionRead;
				} else {
					descriptionEn = descriptionRead;
				}
			} else if ( name().compare( "author", Qt::CaseInsensitive ) == 0 ) {
				readAuthor( &authorName, &authorEmail );
			} else if ( name().compare( "cities", Qt::CaseInsensitive ) == 0 ) {
				readCities( &cities, &cityNameReplacements );
			} else if ( name().compare( "useSeperateCityValue", Qt::CaseInsensitive ) == 0 ) {
				useSeperateCityValue = readBooleanElement();
			} else if ( name().compare( "onlyUseCitiesInList", Qt::CaseInsensitive ) == 0 ) {
				onlyUseCitiesInList = readBooleanElement();
			} else if ( name().compare( "defaultVehicleType", Qt::CaseInsensitive ) == 0 ) {
				defaultVehicleType = readElementText();
			} else if ( name().compare( "url", Qt::CaseInsensitive ) == 0 ) {
				url = readElementText();
			} else if ( name().compare( "shortUrl", Qt::CaseInsensitive ) == 0 ) {
				shortUrl = readElementText();
			} else if ( name().compare( "minFetchWait", Qt::CaseInsensitive ) == 0 ) {
				minFetchWait = readElementText().toInt();
			} else if ( name().compare( "charsetForUrlEncoding", Qt::CaseInsensitive ) == 0 ) {
				charsetForUrlEncoding = readElementText();
			} else if ( name().compare( "fallbackCharset", Qt::CaseInsensitive ) == 0 ) {
				fallbackCharset = readElementText(); // TODO Implement as attributes in the url tags
			} else if ( name().compare( "rawUrls", Qt::CaseInsensitive ) == 0 ) {
				readRawUrls( &rawUrlDepartures, &rawUrlStopSuggestions, &rawUrlJourneys,
						&attributesForDepartures, &attributesForStopSuggestions, &attributesForJourneys );
			} else if ( name().compare( "sessionKey", Qt::CaseInsensitive ) == 0 ) {
				readSessionKey( &sessionKeyUrl, &sessionKeyPlace, &sessionKeyData );
			} else if ( name().compare( "regExps", Qt::CaseInsensitive ) == 0 ) {
				bool ok = readRegExps( &regExpDepartures, &infosDepartures,
									   &regExpDeparturesPre, &infoPreKey, &infoPreValue,
									   &regExpJourneys, &infosJourneys,
									   &regExpDepartureGroupTitles, &infosDepartureGroupTitles,
									   &regExpListPossibleStopsRange, &regExpListPossibleStops,
									   &infosListPossibleStops,
									   &regExpListJourneyNews, &infosListJourneyNews );
				if ( !ok ) {
					return NULL;
				}
			} else if ( name().compare( "script", Qt::CaseInsensitive ) == 0 ) {
				scriptFile = QFileInfo( fileName ).path() + '/' + readElementText();
				if ( !QFile::exists( scriptFile ) ) {
					kDebug() << "The script file " << scriptFile << "referenced by "
							 << "the service provider information XML named"
							 << nameEn << "wasn't found";
					return NULL;
				}
			} else if ( name().compare( "credit", Qt::CaseInsensitive ) == 0 ) {
				credit = readElementText();
			} else {
				readUnknownElement();
			}
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

	if ( nameLocal.isEmpty() ) {
		nameLocal = nameEn;
	}
	if ( descriptionLocal.isEmpty() ) {
		descriptionLocal = descriptionEn;
	}
	if ( shortUrl.isEmpty() ) {
		shortUrl = url;
	}

	TimetableAccessorInfo *accessorInfo = new TimetableAccessorInfo( 
			nameLocal, shortUrl, authorName, authorEmail, version, serviceProvider, type );
	accessorInfo->setFileName( fileName );
	accessorInfo->setCountry( country );
	accessorInfo->setCities( cities );
	accessorInfo->setCredit( credit );
	accessorInfo->setCityNameToValueReplacementHash( cityNameReplacements );
	accessorInfo->setUseSeparateCityValue( useSeperateCityValue );
	accessorInfo->setOnlyUseCitiesInList( onlyUseCitiesInList );
	accessorInfo->setDescription( descriptionLocal );
	accessorInfo->setDefaultVehicleType( TimetableAccessor::vehicleTypeFromString( defaultVehicleType ) );
	accessorInfo->setUrl( url );
	accessorInfo->setShortUrl( shortUrl );
	accessorInfo->setMinFetchWait( minFetchWait );
	accessorInfo->setDepartureRawUrl( rawUrlDepartures );
	accessorInfo->setStopSuggestionsRawUrl( rawUrlStopSuggestions );
	accessorInfo->setJourneyRawUrl( rawUrlJourneys );
	accessorInfo->setAttributesForDepatures( attributesForDepartures );
	accessorInfo->setAttributesForStopSuggestions( attributesForStopSuggestions );
	accessorInfo->setAttributesForJourneys( attributesForJourneys );
	accessorInfo->setFallbackCharset( fallbackCharset.toAscii() );
	accessorInfo->setCharsetForUrlEncoding( charsetForUrlEncoding.toAscii() );
	accessorInfo->setSessionKeyData( sessionKeyUrl, sessionKeyPlace, sessionKeyData );

	if ( scriptFile.isEmpty() ) {
		// Set regular expressions, if no script file was specified
		TimetableAccessorInfoRegExp *accessorInfoRegExp = new TimetableAccessorInfoRegExp( *accessorInfo );
		delete accessorInfo;
		accessorInfo = accessorInfoRegExp;
		
		accessorInfoRegExp->setRegExpDepartures( regExpDepartures, infosDepartures,
												regExpDeparturesPre, infoPreKey, infoPreValue );
		if ( !regExpDepartureGroupTitles.isEmpty() ) {
			accessorInfoRegExp->setRegExpDepartureGroupTitles( regExpDepartureGroupTitles, 
															infosDepartureGroupTitles );
		}
		if ( !regExpJourneys.isEmpty() ) {
			accessorInfoRegExp->setRegExpJourneys( regExpJourneys, infosJourneys );
		}
		for ( int i = 0; i < regExpListJourneyNews.length(); ++i ) {
			QString regExpJourneyNews = regExpListJourneyNews[i];
			QList<TimetableInformation> infosJourneyNews = infosListJourneyNews[i];
			accessorInfoRegExp->addRegExpJouneyNews( regExpJourneyNews, infosJourneyNews );
		}
		for ( int i = 0; i < regExpListPossibleStopsRange.length(); ++i ) {
			QString regExpPossibleStopsRange = regExpListPossibleStopsRange[i];
			QString regExpPossibleStops = regExpListPossibleStops[i];
			QList<TimetableInformation> infosPossibleStops = infosListPossibleStops[i];
			accessorInfoRegExp->addRegExpPossibleStops( regExpPossibleStopsRange,
												regExpPossibleStops, infosPossibleStops );
		}
		
		if ( type == HTML ) {
			return new TimetableAccessorHtml( accessorInfoRegExp );
		} else if ( type == XML ) {
			return new TimetableAccessorXml( accessorInfoRegExp );
		}
	} else if ( type == HTML ) {
		accessorInfo->setScriptFile( scriptFile );
		TimetableAccessorHtmlScript *jsAccessor = new TimetableAccessorHtmlScript( accessorInfo );
		if ( !jsAccessor->hasScriptErrors() ) {
			return jsAccessor;
		} else {
			return NULL; // Couldn't correctly load the script (bad script)
		}
	} else {
		kDebug() << "XML accessors currently don't support scripts";
		return NULL;
	}
	
	kDebug() << "Accessor type not supported" << type;
	return NULL;
}

QString AccessorInfoXmlReader::readLocalizedTextElement( QString *lang )
{
	if ( attributes().hasAttribute( "lang" ) ) {
		*lang = attributes().value( "lang" ).toString();
	} else {
		*lang = "en";
	}

	return readElementText();
}

bool AccessorInfoXmlReader::readBooleanElement()
{
	QString content = readElementText().trimmed();
	if ( content.compare( "true", Qt::CaseInsensitive ) == 0 || content == "1" ) {
		return true;
	} else {
		return false;
	}
}

void AccessorInfoXmlReader::readAuthor( QString *fullname, QString *email )
{
	while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare( "author", Qt::CaseInsensitive ) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare( "fullName", Qt::CaseInsensitive ) == 0 ) {
				*fullname = readElementText();
			} else if ( name().compare( "email", Qt::CaseInsensitive ) == 0 ) {
				*email = readElementText();
			} else {
				readUnknownElement();
			}
		}
	}
}

void AccessorInfoXmlReader::readCities( QStringList *cities,
										QHash< QString, QString > *cityNameReplacements )
{
	while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare( "cities", Qt::CaseInsensitive ) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare( "city", Qt::CaseInsensitive ) == 0 ) {
				if ( attributes().hasAttribute("replaceWith") ) {
					QString replacement = attributes().value( "replaceWith" ).toString().toLower();
					QString city = readElementText();
					cityNameReplacements->insert( city.toLower(), replacement );
					cities->append( city );
				} else {
					QString city = readElementText();
					cities->append( city );
				}
			} else {
				readUnknownElement();
			}
		}
	}
}

void AccessorInfoXmlReader::readRawUrls( QString* rawUrlDepartures, QString* rawUrlStopSuggestions,
										 QString* rawUrlJourneys, 
										 QHash<QString, QString> *attributesForDepartures, 
										 QHash<QString, QString> *attributesForStopSuggestions, 
										 QHash<QString, QString> *attributesForJourneys )
{
	while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare("rawUrls", Qt::CaseInsensitive) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare("departures", Qt::CaseInsensitive) == 0 ) {
				foreach ( const QXmlStreamAttribute &attribute, attributes().toList() ) {
					attributesForDepartures->insert( attribute.name().toString(), 
													 attribute.value().toString() );
				}
				
				while ( !atEnd() ) {
					if ( isStartElement() ) {
						if ( name().compare("url", Qt::CaseInsensitive) == 0 ) {
							*rawUrlDepartures = readElementText();
						} else if ( name().compare("data", Qt::CaseInsensitive) == 0 ) {
							attributesForDepartures->insert( "data", readElementText() );
						}
					} else if ( rawUrlDepartures->isEmpty() && (isCDATA() || isCharacters()) ) {
						*rawUrlDepartures = text().toString();
					} else if ( isEndElement() && name().compare("departures", Qt::CaseInsensitive) == 0 ) {
						// End of <departures> tag
						break;
					}
					
					if ( atEnd() ) {
						break;
					}
					readNext();
				}
			} else if ( name().compare("stopSuggestions", Qt::CaseInsensitive) == 0 ) {
				foreach ( const QXmlStreamAttribute &attribute, attributes().toList() ) {
					attributesForStopSuggestions->insert( attribute.name().toString(), 
														  attribute.value().toString() );
				}
				*rawUrlStopSuggestions = readElementText();
			} else if ( name().compare("journeys", Qt::CaseInsensitive) == 0 ) {
				foreach ( const QXmlStreamAttribute &attribute, attributes().toList() ) {
					attributesForJourneys->insert( attribute.name().toString(), 
												   attribute.value().toString() );
				}
				*rawUrlJourneys = readElementText();
			} else {
				readUnknownElement();
			}
		}
	}
}

void AccessorInfoXmlReader::readSessionKey(QString* sessionKeyUrl, SessionKeyPlace* sessionKeyPlace,
										   QString* data )
{
	while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare("sessionKey", Qt::CaseInsensitive) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare("url", Qt::CaseInsensitive) == 0 ) {
				*sessionKeyUrl = readElementText();
			} else if ( name().compare("putInto", Qt::CaseInsensitive) == 0 ) {
				if ( attributes().hasAttribute(QLatin1String("data")) ) {
					*data = attributes().value( QLatin1String("data") ).toString();
				}
				
				QString putInto = readElementText();
				if ( putInto.compare(QLatin1String("CustomHeader"), Qt::CaseInsensitive) == 0 ) {
					*sessionKeyPlace = PutIntoCustomHeader;
				} else {
					*sessionKeyPlace = PutNowhere;
				}
			} else {
				readUnknownElement();
			}
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
		QList< QList< TimetableInformation > >* infosListJourneyNews )
{
	while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare( "regExps", Qt::CaseInsensitive ) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare( "departures", Qt::CaseInsensitive ) == 0 ) {
				if ( !readRegExp( regExpDepartures, infosDepartures,
				                  regExpDeparturesPre, infoPreKey, infoPreValue ) ) {
					return false;
				}
			} else if ( name().compare( "journeys", Qt::CaseInsensitive ) == 0 ) {
				if ( !readRegExp( regExpJourneys, infosJourneys ) ) {
					return false;
				}
			} else if ( name().compare( "journeyNews", Qt::CaseInsensitive ) == 0 ) {
				if ( !readRegExpItems( regExpListJourneyNews, infosListJourneyNews ) ) {
					return false;
				}
			} else if ( name().compare( "possibleStops", Qt::CaseInsensitive ) == 0 ) {
				if ( !readRegExpItems( regExpListPossibleStops, infosListPossibleStops,
				                       regExpListPossibleStopsRange ) ) {
					return false;
				}
			} else if ( name().compare( "departureGroupTitles", Qt::CaseInsensitive ) == 0 ) {
				if ( !readRegExp( regExpDepartureGroupTitles, infosDepartureGroupTitles ) ) {
					return false;
				}
			} else {
				readUnknownElement();
			}
		}
	}

	return true;
}

bool AccessorInfoXmlReader::readRegExp( QString* regExp, QList< TimetableInformation >* info,
		QString* regExpPreOrRanges, TimetableInformation* infoPreKey, TimetableInformation* infoPreValue )
{
	QStringRef tagName = name();
	while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare( tagName, Qt::CaseInsensitive ) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare( "regExp", Qt::CaseInsensitive ) == 0 ) {
				*regExp = readElementText();
				if ( !QRegExp( *regExp ).isValid() ) {
					raiseError( QString( "The regular expression in <%1> is invalid: %2." )
								.arg( tagName.toString(), QRegExp( *regExp ).errorString() ) );
					return false;
				}
			} else if ( name().compare( "infos", Qt::CaseInsensitive ) == 0 ) { // krazy:exclude=spelling
				readRegExpInfos( info );
				// 		regExpPreOrRanges regExpRange
			} else if ( regExpPreOrRanges && !infoPreKey && !infoPreValue
						&& name().compare( "regExpRange", Qt::CaseInsensitive ) == 0 ) {
				*regExpPreOrRanges = readElementText();
				if ( !QRegExp( *regExpPreOrRanges ).isValid() ) {
					raiseError( QString( "The regular expression (range) in <%1> is invalid: %2." )
					            .arg( tagName.toString(), QRegExp( *regExpPreOrRanges ).errorString() ) );
					return false;
				}
			} else if ( regExpPreOrRanges && infoPreKey && infoPreValue
						&& name().compare( "regExpPre", Qt::CaseInsensitive ) == 0 ) {
				if ( !readRegExpPre( regExpPreOrRanges, infoPreKey, infoPreValue ) ) {
					return false;
				}
			} else
				readUnknownElement();
		}
	}

	return true;
}

void AccessorInfoXmlReader::readRegExpInfos( QList< TimetableInformation >* info )
{
	while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare( "infos", Qt::CaseInsensitive ) == 0 ) { // krazy:exclude=spelling
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare( "info", Qt::CaseInsensitive ) == 0 ) {
				TimetableInformation curInfo =
						TimetableAccessor::timetableInformationFromString( readElementText() );
				info->append( curInfo );
			} else {
				readUnknownElement();
			}
		}
	}
}

bool AccessorInfoXmlReader::readRegExpPre( QString* regExpPre,
		TimetableInformation* infoPreKey,
		TimetableInformation* infoPreValue )
{
	if ( !attributes().hasAttribute( "key" ) || !attributes().hasAttribute( "value" ) ) {
		raiseError( "Missing attributes in <RegExpPre> tag (key and value are needed)" );
		return false;
	}

	*infoPreKey = TimetableAccessor::timetableInformationFromString(
					attributes().value( "key" ).toString() );
	*infoPreValue = TimetableAccessor::timetableInformationFromString(
					attributes().value( "value" ).toString() );
	*regExpPre = readElementText();

	return true;
}

bool AccessorInfoXmlReader::readRegExpItems( QStringList* regExps,
		QList< QList< TimetableInformation > >* infosList,
		QStringList *regExpsRanges )
{
	QStringRef tagName = name();
	while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare( tagName, Qt::CaseInsensitive ) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare( "item", Qt::CaseInsensitive ) == 0 ) {
				QString regExp, regExpRange;
				QList< TimetableInformation > info;
				if ( !readRegExp( &regExp, &info, regExpsRanges ? &regExpRange : NULL ) ) {
					return false;
				}

				regExps->append( regExp );
				infosList->append( info );
				if ( regExpsRanges ) {
					regExpsRanges->append( regExpRange );
				}
			} else {
				readUnknownElement();
			}
		}
	}

	return true;
}
