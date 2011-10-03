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

#include <KLocalizedString>
#include <KLocale>
#include <KDebug>
#include <QFile>
#include <QFileInfo>
#include <KGlobal>

bool AccessorInfoXmlWriter::write( QIODevice* device, const TimetableAccessor& accessor ) {
    Q_ASSERT( device );
    
    bool closeAfterRead; // Only close after reading if it wasn't open before
    if ( (closeAfterRead = !device->isOpen()) && !device->open(QIODevice::WriteOnly) ) {
		return false;
	}
    setDevice( device );
    setAutoFormatting( true );

    writeStartDocument();
    writeStartElement( "accessorInfo" );
    writeAttribute( "fileVersion", accessor.fileVersion );
    writeAttribute( "version", accessor.version );
    writeAttribute( "type", accessor.type == XmlAccessor ? "XML" : "HTML" );

    bool enUsed = false;
    for ( QHash<QString, QString>::const_iterator it = accessor.name.constBegin();
	  it != accessor.name.constEnd(); ++it )
    {
		QString lang = it.key() == "en_US" ? "en" : it.key();
		if ( lang == "en" ) {
			if ( enUsed ) {
				continue;
			}
			enUsed = true;
		}
		
		writeStartElement( "name" );
		writeAttribute( "lang", lang );
		writeCharacters( it.value() );
		writeEndElement();
    }

    enUsed = false;
    for ( QHash<QString, QString>::const_iterator it = accessor.description.constBegin();
	  it != accessor.description.constEnd(); ++it )
    {
		QString lang = it.key() == "en_US" ? "en" : it.key();
		if ( lang == "en" ) {
			if ( enUsed ) {
				continue;
			}
			enUsed = true;
		}
		
		writeStartElement( "description" );
		writeAttribute( "lang", lang );
		writeCharacters( it.value() );
		writeEndElement();
    }
    
    writeStartElement( "author" );
    writeTextElement( "fullname", accessor.author );
    writeTextElement( "short", accessor.shortAuthor );
    writeTextElement( "email", accessor.email );
    writeEndElement();

    if ( accessor.useCityValue )
		writeTextElement( "useSeperateCityValue", accessor.useCityValue ? "true" : "false" );
    if ( accessor.onlyUseCitiesInList )
		writeTextElement( "onlyUseCitiesInList", accessor.onlyUseCitiesInList ? "true" : "false" );
    if ( !accessor.url.isEmpty() )
		writeTextElement( "url", accessor.url );
    if ( !accessor.shortUrl.isEmpty() )
		writeTextElement( "shortUrl", accessor.shortUrl );
    if ( !accessor.credit.isEmpty() )
		writeTextElement( "credit", accessor.credit );
    if ( accessor.defaultVehicleType != "Unknown" )
		writeTextElement( "defaultVehicleType", accessor.defaultVehicleType );
    if ( accessor.minFetchWait > 2 )
		writeTextElement( "minFetchWait", QString::number(accessor.minFetchWait) );
    if ( !accessor.fallbackCharset.isEmpty() )
		writeTextElement( "fallbackCharset", accessor.fallbackCharset );
    if ( !accessor.charsetForUrlEncoding.isEmpty() )
		writeTextElement( "charsetForUrlEncoding", accessor.charsetForUrlEncoding );
    if ( !accessor.scriptFile.isEmpty() )
		writeTextElement( "script", accessor.scriptFile );
    if ( !accessor.cities.isEmpty() ) {
		writeStartElement( "cities" );
		foreach ( const QString &city, accessor.cities ) {
			writeStartElement( "city" );
			const QString lowerCity = city.toLower();
			if ( accessor.cityNameReplacements.contains(lowerCity) ) {
				writeAttribute( "replaceWith", accessor.cityNameReplacements[lowerCity] );
			}
			writeCharacters( city );
			writeEndElement(); // city
		}
		writeEndElement(); // cities
    }
    
    // Write raw urls
    writeStartElement( "rawUrls" );
    if ( !accessor.rawDepartureUrl.isEmpty() ) {
		writeStartElement( "departures" );
		writeCDATA( accessor.rawDepartureUrl );
		writeEndElement();
    }
    if ( !accessor.rawJourneyUrl.isEmpty() ) {
		writeStartElement( "journeys" );
		writeCDATA( accessor.rawJourneyUrl );
		writeEndElement();
    }
    if ( !accessor.rawStopSuggestionsUrl.isEmpty() ) {
		writeStartElement( "stopSuggestions" );
		writeCDATA( accessor.rawStopSuggestionsUrl );
		writeEndElement();
    }
    writeEndElement(); // rawUrls
	
	// Write changelog
	if ( !accessor.changelog.isEmpty() ) {
		writeStartElement( "changelog" );
		foreach ( const ChangelogEntry &entry, accessor.changelog ) {
			writeStartElement( "entry" );
			if ( !entry.author.isEmpty() && entry.author != accessor.shortAuthor ) {
				writeAttribute( "author", entry.author );
			}
			writeAttribute( "since", entry.version );
			if ( !entry.releasedWith.isEmpty() ) {
				writeAttribute( "releasedWith", entry.releasedWith );
			}
			writeCharacters( entry.description );
			writeEndElement(); // entry
		}
		writeEndElement(); // changelog
	}
    
    writeEndElement(); // accessorInfo
    writeEndDocument();

    return true;
}

TimetableAccessor AccessorInfoXmlReader::read( QIODevice* device ) {
    Q_ASSERT( device );
    TimetableAccessor ret;

    bool closeAfterRead; // Only close after reading if it wasn't open before
    if ( (closeAfterRead = !device->isOpen()) && !device->open(QIODevice::ReadOnly) ) {
		raiseError( "Couldn't open the file read only" );
		return ret;
    }
    setDevice( device );
    
    while ( !atEnd() ) {
		readNext();

		if ( isStartElement() ) {
			if ( name().compare("accessorInfo", Qt::CaseInsensitive) == 0 ) {
				QString fileVersion = attributes().value( "fileVersion" ).toString();
				if ( fileVersion != "1.0" ) {
					// File won't be read by the data engine, because of a wrong
					// file version
					kDebug() << "The file is not a public transport accessor info "
						"version 1.0 file.";
				}
				ret = readAccessorInfo();
				ret.fileVersion = fileVersion;
				break;
			} else {
				readUnknownElement();
			}
		}
    }

    if ( closeAfterRead ) {
		device->close();
	}

    if ( error() ) {
		ret.type = InvalidAccessor;
	}
    return ret;
}

void AccessorInfoXmlReader::readUnknownElement() {
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

TimetableAccessor AccessorInfoXmlReader::readAccessorInfo() {
    TimetableAccessor ret;
    QString langRead;
    QHash<QString,QString> l10nNames, l10nDescriptions;

    if ( attributes().hasAttribute("version") ) {
		ret.version = attributes().value( "version" ).toString();
	} else {
		ret.version = "1.0";
	}

    if ( attributes().hasAttribute("type") ) {
		ret.type = attributes().value("type").toString()
				.compare("XML", Qt::CaseInsensitive) == 0 ? XmlAccessor : ScriptedAccessor;
		if ( ret.type == InvalidAccessor ) { // TODO
			kDebug() << "The type" << attributes().value("type").toString()
				<< "is invalid. Currently there are two values allowed: "
				"HTML and XML.";
			return ret;
		}
    } else {
		ret.type = ScriptedAccessor;
	}

    while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare("accessorInfo", Qt::CaseInsensitive) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare("name", Qt::CaseInsensitive) == 0 ) {
				QString nameRead = readLocalizedTextElement( &langRead );
				l10nNames.insert( langRead, nameRead );
			} else if ( name().compare("description", Qt::CaseInsensitive) == 0 ) {
				QString descriptionRead = readLocalizedTextElement( &langRead );
				l10nDescriptions.insert( langRead, descriptionRead );
			} else if ( name().compare("author", Qt::CaseInsensitive) == 0 ) {
				readAuthor( &ret.author, &ret.shortAuthor, &ret.email );
			} else if ( name().compare("cities", Qt::CaseInsensitive) == 0 ) {
				readCities( &ret.cities, &ret.cityNameReplacements );
			} else if ( name().compare("useSeperateCityValue", Qt::CaseInsensitive) == 0 ) {
				ret.useCityValue = readBooleanElement();
			} else if ( name().compare("onlyUseCitiesInList", Qt::CaseInsensitive) == 0 ) {
				ret.onlyUseCitiesInList = readBooleanElement();
			} else if ( name().compare("defaultVehicleType", Qt::CaseInsensitive) == 0 ) {
				ret.defaultVehicleType = readElementText();
			} else if ( name().compare("url", Qt::CaseInsensitive) == 0 ) {
				ret.url = readElementText();
			} else if ( name().compare("shortUrl", Qt::CaseInsensitive) == 0 ) {
				ret.shortUrl = readElementText();
			} else if ( name().compare("minFetchWait", Qt::CaseInsensitive) == 0 ) {
				ret.minFetchWait = readElementText().toInt();
			} else if ( name().compare("charsetForUrlEncoding", Qt::CaseInsensitive) == 0 ) {
				ret.charsetForUrlEncoding = readElementText();
			} else if ( name().compare("fallbackCharset", Qt::CaseInsensitive) == 0 ) {
				ret.fallbackCharset = readElementText(); // TODO Implement as attributes in the url tags
			} else if ( name().compare("rawUrls", Qt::CaseInsensitive) == 0 ) {
				readRawUrls( &ret.rawDepartureUrl, &ret.rawStopSuggestionsUrl, &ret.rawJourneyUrl );
			} else if ( name().compare("script", Qt::CaseInsensitive) == 0 ) {
				ret.scriptFile = readElementText();
			} else if ( name().compare("credit", Qt::CaseInsensitive) == 0 ) {
				ret.credit = readElementText();
			} else if ( name().compare("changelog", Qt::CaseInsensitive) == 0 ) {
				ret.changelog = readChangelog();
			} else {
				readUnknownElement();
			}
		}
    }

    if ( ret.shortUrl.isEmpty() ) {
		ret.shortUrl = ret.url;
	}

    ret.name = l10nNames;
    ret.description = l10nDescriptions;
    return ret;
}

QString AccessorInfoXmlReader::readLocalizedTextElement( QString *lang ) {
    if ( attributes().hasAttribute("lang") ) {
		*lang = attributes().value( "lang" ).toString();
	} else {
		*lang = "en";
	}

    return readElementText();
}

bool AccessorInfoXmlReader::readBooleanElement() {
    QString content = readElementText().trimmed();
    if ( content.compare("true", Qt::CaseInsensitive) == 0 || content == "1" ) {
		return true;
	} else {
		return false;
	}
}

void AccessorInfoXmlReader::readAuthor( QString *fullname, QString *shortName, QString *email ) {
    while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare("author", Qt::CaseInsensitive) == 0 )
			break;

		if ( isStartElement() ) {
			if ( name().compare("fullName", Qt::CaseInsensitive) == 0 ) {
				*fullname = readElementText();
			} else if ( name().compare("short", Qt::CaseInsensitive) == 0 ) {
				*shortName = readElementText();
			} else if ( name().compare("email", Qt::CaseInsensitive) == 0 ) {
				*email = readElementText();
			} else {
				readUnknownElement();
			}
		}
    }
}

void AccessorInfoXmlReader::readCities( QStringList *cities,
					QHash< QString, QString > *cityNameReplacements ) {
    while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare("cities", Qt::CaseInsensitive) == 0 ) {
			break;
		}

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
			} else {
				readUnknownElement();
			}
		}
    }
}

void AccessorInfoXmlReader::readRawUrls( QString* rawUrlDepartures,
					 QString* rawUrlStopSuggestions,
					 QString* rawUrlJourneys ) {
    while ( !atEnd() ) {
		readNext();

		if ( isEndElement() && name().compare("rawUrls", Qt::CaseInsensitive) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare("departures", Qt::CaseInsensitive) == 0 ) {
				*rawUrlDepartures = readElementText();
			} else if ( name().compare("stopSuggestions", Qt::CaseInsensitive) == 0 ) {
				*rawUrlStopSuggestions = readElementText();
			} else if ( name().compare("journeys", Qt::CaseInsensitive) == 0 ) {
				*rawUrlJourneys = readElementText();
			} else {
				readUnknownElement();
			}
		}
    }
}

QList<ChangelogEntry> AccessorInfoXmlReader::readChangelog()
{
	QList<ChangelogEntry> changelog;
	while ( !atEnd() ) {
		readNext();
		if ( isEndElement() && name().compare("changelog", Qt::CaseInsensitive) == 0 ) {
			break;
		}

		if ( isStartElement() ) {
			if ( name().compare("entry", Qt::CaseInsensitive) == 0 ) {
				ChangelogEntry currentEntry;
				if ( attributes().hasAttribute(QLatin1String("since")) ) {
					currentEntry.version = attributes().value( QLatin1String("since") ).toString();
				}
				if ( attributes().hasAttribute(QLatin1String("releasedWith")) ) {
					currentEntry.releasedWith = attributes().value( QLatin1String("releasedWith") ).toString();
				}
				if ( attributes().hasAttribute(QLatin1String("author")) ) {
					currentEntry.author = attributes().value( QLatin1String("author") ).toString();
				}
				currentEntry.description = readElementText();
				changelog.append( currentEntry );
			} else {
				readUnknownElement();
			}
		}
	}
	return changelog;
}
