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

// Header
#include "accessorinfoxmlreader.h"

// Own includes
#include "timetableaccessor.h"
#include "timetableaccessor_info.h"
#include "timetableaccessor_script.h"

// KDE includes
#include <KLocalizedString>
#include <KGlobal>
#include <KLocale>
#include <KDebug>

// Qt includes
#include <QFile>
#include <QFileInfo>

TimetableAccessor* AccessorInfoXmlReader::read( QIODevice* device,
        const QString &serviceProvider, const QString &fileName, const QString &country )
{
    Q_ASSERT( device );

    bool closeAfterRead; // Only close after reading if it wasn't open before
    if ( (closeAfterRead = !device->isOpen()) && !device->open(QIODevice::ReadOnly) ) {
        raiseError( "Couldn't read the file \"" + fileName + "\"." );
        return 0;
    }
    setDevice( device );

    TimetableAccessor *ret = 0;
    while ( !atEnd() ) {
        readNext();

        if ( isStartElement() ) {
            if ( name().compare("accessorInfo", Qt::CaseInsensitive) == 0 &&
                 attributes().value("fileVersion") == "1.0" )
            {
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

    return error() ? 0 : ret;
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
    QString nameLocal, nameEn, descriptionLocal, descriptionEn;
    AccessorType accessorType = HTML;
    if ( attributes().hasAttribute(QLatin1String("type")) ) {
        accessorType = TimetableAccessor::accessorTypeFromString(
                attributes().value(QLatin1String("type")).toString() );
        if ( accessorType == NoAccessor ) {
            raiseError( QString("The accessor type %1 is invalid. Currently there are two "
                                "values allowed: HTML and XML.")
                        .arg(attributes().value("type").toString()) );
            return 0;
        }
    }

    TimetableAccessorInfo *accessorInfo = new TimetableAccessorInfo();
    accessorInfo->setType( accessorType );
    accessorInfo->setServiceProvider( serviceProvider );
    accessorInfo->setFileName( fileName );
    accessorInfo->setCountry( country );

    if ( attributes().hasAttribute(QLatin1String("version")) ) {
        accessorInfo->setVersion( attributes().value(QLatin1String("version")).toString() );
    } else {
        accessorInfo->setVersion( "1.0" );
    }

    while ( !atEnd() ) {
        readNext();

        if ( isEndElement() && name().compare(QLatin1String("accessorInfo"), Qt::CaseInsensitive) == 0 ) {
            break;
        }

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("name"), Qt::CaseInsensitive) == 0 ) {
                QString nameRead = readLocalizedTextElement( &langRead );
                if ( langRead == lang ) {
                    nameLocal = nameRead;
                } else {
                    nameEn = nameRead;
                }
            } else if ( name().compare(QLatin1String("description"), Qt::CaseInsensitive) == 0 ) {
                QString descriptionRead = readLocalizedTextElement( &langRead );
                if ( langRead == lang ) {
                    descriptionLocal = descriptionRead;
                } else {
                    descriptionEn = descriptionRead;
                }
            } else if ( name().compare(QLatin1String("author"), Qt::CaseInsensitive) == 0 ) {
                QString authorName, shortName, authorEmail;
                readAuthor( &authorName, &shortName, &authorEmail );
                accessorInfo->setAuthor( authorName, shortName, authorEmail );
            } else if ( name().compare(QLatin1String("cities"), Qt::CaseInsensitive) == 0 ) {
                QStringList cities;
                QHash<QString, QString> cityNameReplacements;
                readCities( &cities, &cityNameReplacements );
                accessorInfo->setCities( cities );
                accessorInfo->setCityNameToValueReplacementHash( cityNameReplacements );
            } else if ( name().compare(QLatin1String("useSeperateCityValue"), Qt::CaseInsensitive) == 0 ) {
                accessorInfo->setUseSeparateCityValue( readBooleanElement() );
            } else if ( name().compare(QLatin1String("onlyUseCitiesInList"), Qt::CaseInsensitive) == 0 ) {
                accessorInfo->setOnlyUseCitiesInList( readBooleanElement() );
            } else if ( name().compare(QLatin1String("defaultVehicleType"), Qt::CaseInsensitive) == 0 ) {
                accessorInfo->setDefaultVehicleType(
                        TimetableAccessor::vehicleTypeFromString(readElementText()) );
            } else if ( name().compare(QLatin1String("url"), Qt::CaseInsensitive) == 0 ) {
                accessorInfo->setUrl( readElementText() );
            } else if ( name().compare(QLatin1String("shortUrl"), Qt::CaseInsensitive) == 0 ) {
                accessorInfo->setShortUrl( readElementText() );
            } else if ( name().compare(QLatin1String("minFetchWait"), Qt::CaseInsensitive) == 0 ) {
                accessorInfo->setMinFetchWait( readElementText().toInt() );
            } else if ( name().compare(QLatin1String("charsetForUrlEncoding"), Qt::CaseInsensitive) == 0 ) {
                accessorInfo->setCharsetForUrlEncoding( readElementText().toAscii() );
            } else if ( name().compare(QLatin1String("fallbackCharset"), Qt::CaseInsensitive) == 0 ) {
                accessorInfo->setFallbackCharset( readElementText().toAscii() ); // TODO Implement as attributes in the url tags?
            } else if ( name().compare(QLatin1String("changelog"), Qt::CaseInsensitive) == 0 ) {
                accessorInfo->setChangelog( readChangelog() );
            } else if ( name().compare(QLatin1String("credit"), Qt::CaseInsensitive) == 0 ) {
                accessorInfo->setCredit( readElementText() );
            } else if ( accessorType == HTML &&
                        name().compare(QLatin1String("script"), Qt::CaseInsensitive) == 0 )
            {
                const QStringList extensions = attributes().value( QLatin1String("extensions") )
                        .toString().split( ',', QString::SkipEmptyParts );
                const QString scriptFile = QFileInfo( fileName ).path() + '/' + readElementText();
                if ( !QFile::exists(scriptFile) ) {
                    raiseError( QString("The script file %1 referenced by the service provider "
                                        "information XML named %2 wasn't found")
                                .arg(scriptFile).arg(nameEn) );
                    delete accessorInfo;
                    return 0;
                }
                accessorInfo->setScriptFile( scriptFile, extensions );
            } else {
                readUnknownElement();
            }
        }
    }

    if ( accessorInfo->url().isEmpty() ) {
        delete accessorInfo;
        raiseError( "No <url> tag in accessor info XML" );
        return 0;
    }
//     if ( accessorInfo->accessorType() == HTML && accessorInfo->departureRawUrl().isEmpty() ) {
//         delete accessorInfo;
//         raiseError( "No raw url for departures in accessor info XML, mandatory for HTML types" );
//         return 0;

// TODO
//         kDebug() << "No raw url for deaprtures in accessor info XML, will use 'getTimetable' "
//                     "function to request timetable document from within the script using "
//                     "netAccess.download(url).";
//     }

    accessorInfo->setName( nameLocal.isEmpty() ? nameEn : nameLocal );
    accessorInfo->setDescription( descriptionLocal.isEmpty() ? descriptionEn : descriptionLocal );
    accessorInfo->finish();

    // Create the accessor
    if ( accessorInfo->accessorType() == HTML ) {
        // Ensure a script is specified
        if ( accessorInfo->scriptFileName().isEmpty() ) {
            delete accessorInfo;
            raiseError( "HTML accessors need a script for parsing" );
            return 0;
        }

        // Create the accessor and check for script errors
        TimetableAccessorScript *scriptAccessor = new TimetableAccessorScript( accessorInfo );
        if ( !scriptAccessor->hasScriptErrors() ) {
            return scriptAccessor;
        } else {
            delete scriptAccessor;
            raiseError( "Couldn't correctly load the script (bad script)" );
            return 0;
        }
    }

    delete accessorInfo;
    raiseError( QString("Accessor type %1 not supported").arg(accessorInfo->accessorType()) );
    return 0;
}

QString AccessorInfoXmlReader::readLocalizedTextElement( QString *lang )
{
    if ( attributes().hasAttribute(QLatin1String("lang")) ) {
        *lang = attributes().value(QLatin1String("lang")).toString();
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

void AccessorInfoXmlReader::readAuthor( QString *fullname, QString *shortName, QString *email )
{
    while ( !atEnd() ) {
        readNext();

        if ( isEndElement() && name().compare( "author", Qt::CaseInsensitive ) == 0 ) {
            break;
        }

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("fullName"), Qt::CaseInsensitive) == 0 ) {
                *fullname = readElementText().trimmed();
            } else if ( name().compare(QLatin1String("short"), Qt::CaseInsensitive) == 0 ) {
                *shortName = readElementText().trimmed();
            } else if ( name().compare(QLatin1String("email"), Qt::CaseInsensitive) == 0 ) {
                *email = readElementText().trimmed();
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

        if ( isEndElement() && name().compare(QLatin1String("cities"), Qt::CaseInsensitive) == 0 ) {
            break;
        }

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("city"), Qt::CaseInsensitive ) == 0 ) {
                if ( attributes().hasAttribute(QLatin1String("replaceWith")) ) {
                    QString replacement = attributes().value(QLatin1String("replaceWith")).toString().toLower();
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

        if ( isEndElement() && name().compare(QLatin1String("rawUrls"), Qt::CaseInsensitive) == 0 ) {
            break;
        }

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("departures"), Qt::CaseInsensitive) == 0 ) {
                foreach ( const QXmlStreamAttribute &attribute, attributes().toList() ) {
                    attributesForDepartures->insert( attribute.name().toString(),
                                                     attribute.value().toString() );
                }

                while ( !atEnd() ) {
                    if ( isStartElement() ) {
                        if ( name().compare(QLatin1String("url"), Qt::CaseInsensitive) == 0 ) {
                            *rawUrlDepartures = readElementText();
                        } else if ( name().compare(QLatin1String("data"), Qt::CaseInsensitive) == 0 ) {
                            attributesForDepartures->insert( QLatin1String("data"), readElementText() );
                        }
                    } else if ( rawUrlDepartures->isEmpty() && (isCDATA() || isCharacters()) ) {
                        *rawUrlDepartures = text().toString();
                    } else if ( isEndElement() && name().compare(QLatin1String("departures"), Qt::CaseInsensitive) == 0 ) {
                        // End of <departures> tag
                        break;
                    }

                    if ( atEnd() ) {
                        break;
                    }
                    readNext();
                }
            } else if ( name().compare(QLatin1String("stopSuggestions"), Qt::CaseInsensitive) == 0 ) {
                foreach ( const QXmlStreamAttribute &attribute, attributes().toList() ) {
                    attributesForStopSuggestions->insert( attribute.name().toString(),
                                                          attribute.value().toString() );
                }
                *rawUrlStopSuggestions = readElementText();
            } else if ( name().compare(QLatin1String("journeys"), Qt::CaseInsensitive) == 0 ) {
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

        if ( isEndElement() && name().compare(QLatin1String("sessionKey"), Qt::CaseInsensitive) == 0 ) {
            break;
        }

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("url"), Qt::CaseInsensitive) == 0 ) {
                *sessionKeyUrl = readElementText();
            } else if ( name().compare(QLatin1String("putInto"), Qt::CaseInsensitive) == 0 ) {
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
                    currentEntry.since_version = attributes().value( QLatin1String("since") ).toString();
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
