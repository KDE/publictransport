/*
*   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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
#include "serviceproviderdatareader.h"

// Own includes
#include "config.h"
#include "serviceprovider.h"
#include "serviceproviderdata.h"
#include "serviceproviderglobal.h"
#include "script/serviceproviderscript.h"
#include "global.h"

// KDE includes
#include <KLocalizedString>
#include <KGlobal>
#include <KLocale>
#include <KDebug>
#include <KStandardDirs>

// Qt includes
#include <QFile>
#include <QFileInfo>

ServiceProviderData *ServiceProviderDataReader::read( const QString &providerId,
                                                      QString *errorMessage )
{
    QString filePath;
    QString country = "international";
    QString _serviceProviderId = providerId;
    if ( _serviceProviderId.isEmpty() ) {
        // No service provider ID given, use the default one for the users country
        country = KGlobal::locale()->country();

        // Try to find the XML filename of the default accessor for [country]
        filePath = ServiceProviderGlobal::defaultProviderForLocation( country );
        if ( filePath.isEmpty() ) {
            return 0;
        }

        // Extract service provider ID from filename
        _serviceProviderId = ServiceProviderGlobal::idFromFileName( filePath );
        kDebug() << "No service provider ID given, using the default one for country"
                 << country << "which is" << _serviceProviderId;
    } else {
        foreach ( const QString &extension, ServiceProviderGlobal::fileExtensions() ) {
            filePath = KGlobal::dirs()->findResource( "data",
                    ServiceProviderGlobal::installationSubDirectory() +
                    _serviceProviderId + '.' + extension );
            if ( !filePath.isEmpty() ) {
                break;
            }
        }
        if ( filePath.isEmpty() ) {
            kDebug() << "Could not find a service provider plugin XML named" << _serviceProviderId;
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "Could not find a service provider "
                                      "plugin with the ID %1", _serviceProviderId);
            }
            return 0;
        }

        // Get country code from filename
        QRegExp rx( "^([^_]+)" );
        if ( rx.indexIn(_serviceProviderId) != -1 &&
             KGlobal::locale()->allCountriesList().contains(rx.cap()) )
        {
            country = rx.cap();
        }
    }

    QFile file( filePath );
    ServiceProviderDataReader reader;
    ServiceProviderData *data = reader.read( &file, providerId, filePath, country,
                                             ServiceProviderDataReader::OnlyReadCorrectFiles );
    if ( !data && errorMessage ) {
        *errorMessage = i18nc("@info/plain", "Error in line %1: <message>%2</message>",
                              reader.lineNumber(), reader.errorString());
    }
    return data;
}

ServiceProviderData *ServiceProviderDataReader::read( QIODevice *device, const QString &fileName,
                                                      ErrorAcceptance errorAcceptance, QObject *parent )
{
    const QString serviceProvider = ServiceProviderGlobal::idFromFileName( fileName );

    // Get country code from filename
    QString country;
    QRegExp rx( "^([^_]+)" );
    if ( rx.indexIn(serviceProvider) != -1 &&
         KGlobal::locale()->allCountriesList().contains(rx.cap()) )
    {
        country = rx.cap();
    } else {
        country = "international";
    }

    return read( device, serviceProvider, fileName, country, errorAcceptance, parent );
}

ServiceProviderData* ServiceProviderDataReader::read( QIODevice* device,
        const QString &serviceProvider, const QString &fileName, const QString &country,
        ErrorAcceptance errorAcceptance, QObject *parent )
{
    Q_ASSERT( device );

    bool closeAfterRead; // Only close after reading if it wasn't open before
    if ( (closeAfterRead = !device->isOpen()) && !device->open(QIODevice::ReadOnly) ) {
        raiseError( "Couldn't read the file \"" + fileName + "\"." );
        return 0;
    }
    setDevice( device );

    ServiceProviderData *data = 0;
    while ( !atEnd() ) {
        readNext();

        if ( isStartElement() ) {
            if ( name().compare("serviceProvider", Qt::CaseInsensitive) != 0 ) {
                raiseError( QString("Wrong root element, should be <serviceProvider>, is <%1>.")
                            .arg(name().toString()) );
            } else if ( attributes().value("fileVersion") != QLatin1String("1.0") ) {
                raiseError( "The file is not a public transport service provider plugin "
                            "version 1.0 file." );
            } else {
                data = readProviderData( serviceProvider, fileName, country, errorAcceptance, parent );
                break;
            }
        }
    }

    if ( closeAfterRead ) {
        device->close();
    }

    if ( error() != NoError ) {
        kDebug() << "Error reading provider" << serviceProvider << errorString();
    }
    return error() == NoError && data ? data : 0;
}

void ServiceProviderDataReader::readUnknownElement()
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

ServiceProviderData *ServiceProviderDataReader::readProviderData( const QString &serviceProviderId,
        const QString &fileName, const QString &country, ErrorAcceptance errorAcceptance,
        QObject *parent )
{
    const QString lang = KGlobal::locale()->country();
    QString langRead, url, shortUrl;
    QHash<QString, QString> names, descriptions;
    ServiceProviderType serviceProviderType;
    QString serviceProviderTypeString;
    const QString fileVersion = attributes().value("fileVersion").toString();

    if ( attributes().hasAttribute(QLatin1String("type")) ) {
        serviceProviderTypeString = attributes().value( QLatin1String("type") ).toString();
        serviceProviderType = ServiceProviderGlobal::typeFromString( serviceProviderTypeString );
        if ( serviceProviderType == InvalidProvider && errorAcceptance == OnlyReadCorrectFiles ) {
            raiseError( QString("The service provider type %1 is invalid. Currently there is only "
                                "one value allowed: Script. You can use qt.xml to read XML.")
                        .arg(attributes().value("type").toString()) );
            return 0;
        }
    } else {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        serviceProviderType = ScriptedProvider;
        serviceProviderTypeString = ServiceProviderGlobal::typeToString( serviceProviderType );
#else
    #ifdef BUILD_PROVIDER_TYPE_GTFS
        serviceProviderType = GtfsProvider;
        serviceProviderTypeString = ServiceProviderGlobal::typeToString( serviceProviderType );
    #else
        kFatal() << "Internal error: No known default provider type, "
                    "tried ScriptedProvider and GtfsProvider";
    #endif
#endif
    }

    ServiceProviderData *serviceProviderData =
            new ServiceProviderData( serviceProviderType, serviceProviderId, parent );
    serviceProviderData->setFileName( fileName );
    serviceProviderData->setCountry( country );
    serviceProviderData->setFileFormatVersion( fileVersion );

    if ( attributes().hasAttribute(QLatin1String("version")) ) {
        serviceProviderData->setVersion( attributes().value(QLatin1String("version")).toString() );
    }

    while ( !atEnd() ) {
        readNext();

        if ( isEndElement() && name().compare(QLatin1String("serviceProvider"), Qt::CaseInsensitive) == 0 ) {
            break;
        }

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("name"), Qt::CaseInsensitive) == 0 ) {
                const QString nameRead = readLocalizedTextElement( &langRead );
                names[ langRead ] = nameRead;
            } else if ( name().compare(QLatin1String("description"), Qt::CaseInsensitive) == 0 ) {
                const QString descriptionRead = readLocalizedTextElement( &langRead );
                descriptions[ langRead ] = descriptionRead;
            } else if ( name().compare(QLatin1String("author"), Qt::CaseInsensitive) == 0 ) {
                QString authorName, shortName, authorEmail;
                readAuthor( &authorName, &shortName, &authorEmail );
                serviceProviderData->setAuthor( authorName, shortName, authorEmail );
            } else if ( name().compare(QLatin1String("cities"), Qt::CaseInsensitive) == 0 ) {
                QStringList cities;
                QHash<QString, QString> cityNameReplacements;
                readCities( &cities, &cityNameReplacements );
                serviceProviderData->setCities( cities );
                serviceProviderData->setCityNameToValueReplacementHash( cityNameReplacements );
            } else if ( name().compare(QLatin1String("useSeperateCityValue"), Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setUseSeparateCityValue( readBooleanElement() );
            } else if ( name().compare(QLatin1String("onlyUseCitiesInList"), Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setOnlyUseCitiesInList( readBooleanElement() );
            } else if ( name().compare(QLatin1String("defaultVehicleType"), Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setDefaultVehicleType(
                        Global::vehicleTypeFromString(readElementText()) );
            } else if ( name().compare(QLatin1String("url"), Qt::CaseInsensitive) == 0 ) {
                url = readElementText();
            } else if ( name().compare(QLatin1String("shortUrl"), Qt::CaseInsensitive) == 0 ) {
                shortUrl = readElementText();
            } else if ( name().compare(QLatin1String("minFetchWait"), Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setMinFetchWait( readElementText().toInt() );
            } else if ( name().compare(QLatin1String("charsetForUrlEncoding"), Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setCharsetForUrlEncoding( readElementText().toAscii() );
            } else if ( name().compare(QLatin1String("fallbackCharset"), Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setFallbackCharset( readElementText().toAscii() ); // TODO Implement as attributes in the url tags?
            } else if ( name().compare(QLatin1String("changelog"), Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setChangelog( readChangelog() );
            } else if ( name().compare(QLatin1String("credit"), Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setCredit( readElementText() );
#ifdef BUILD_PROVIDER_TYPE_GTFS
            } else if ( name().compare("feedUrl", Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setFeedUrl( readElementText() );
            } else if ( name().compare("realtimeTripUpdateUrl", Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setRealtimeTripUpdateUrl( readElementText() );
            } else if ( name().compare("realtimeAlertsUrl", Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setRealtimeAlertsUrl( readElementText() );
            } else if ( name().compare("timeZone", Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setTimeZone( readElementText() );
#endif
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
            } else if ( serviceProviderType == ScriptedProvider &&
                        name().compare(QLatin1String("script"), Qt::CaseInsensitive) == 0 )
            {
                const QStringList extensions = attributes().value( QLatin1String("extensions") )
                        .toString().split( ',', QString::SkipEmptyParts );
                const QString scriptFile = QFileInfo( fileName ).path() + '/' + readElementText();
                if ( !QFile::exists(scriptFile) && errorAcceptance == OnlyReadCorrectFiles ) {
                    raiseError( QString("The script file %1 referenced by the service provider "
                                        "information XML named %2 wasn't found")
                                .arg(scriptFile).arg(names["en"]) );
                    delete serviceProviderData;
                    return 0; // TODO
                }
                serviceProviderData->setScriptFile( scriptFile, extensions );
#endif
            } else if ( name().compare(QLatin1String("samples"), Qt::CaseInsensitive) == 0 ) {
                QStringList stops;
                QString city;
                readSamples( &stops, &city );
                serviceProviderData->setSampleStops( stops );
                serviceProviderData->setSampleCity( city );
            } else if ( name().compare(QLatin1String("notes"), Qt::CaseInsensitive) == 0 ) {
                serviceProviderData->setNotes( readElementText() );
            } else {
                readUnknownElement();
            }
        }
    }

    if ( url.isEmpty() ) {
        kWarning() << "No <url> tag in service provider plugin XML";
    }

    serviceProviderData->setNames( names );
    serviceProviderData->setDescriptions( descriptions );
    serviceProviderData->setUrl( url, shortUrl );
    serviceProviderData->finish();

    return serviceProviderData;
}

QString ServiceProviderDataReader::readLocalizedTextElement( QString *lang )
{
    if ( attributes().hasAttribute(QLatin1String("lang")) ) {
        *lang = attributes().value(QLatin1String("lang")).toString();
    } else {
        *lang = "en";
    }

    return readElementText();
}

bool ServiceProviderDataReader::readBooleanElement()
{
    const QString content = readElementText().trimmed();
    if ( content.compare( "true", Qt::CaseInsensitive ) == 0 || content == QLatin1String("1") ) {
        return true;
    } else {
        return false;
    }
}

void ServiceProviderDataReader::readAuthor( QString *fullname, QString *shortName, QString *email )
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

void ServiceProviderDataReader::readCities( QStringList *cities,
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

void ServiceProviderDataReader::readSamples( QStringList *stops, QString *city )
{
    while ( !atEnd() ) {
        readNext();

        if ( isEndElement() && name().compare(QLatin1String("samples"), Qt::CaseInsensitive) == 0 ) {
            break;
        }

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("stop"), Qt::CaseInsensitive ) == 0 ) {
                stops->append( readElementText() );
            } else if ( name().compare(QLatin1String("city"), Qt::CaseInsensitive ) == 0 ) {
                *city = readElementText();
            } else {
                readUnknownElement();
            }
        }
    }
}

QList<ChangelogEntry> ServiceProviderDataReader::readChangelog()
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
                if ( attributes().hasAttribute(QLatin1String("version")) ) {
                    currentEntry.version = attributes().value( QLatin1String("version") ).toString();
                } else if ( attributes().hasAttribute(QLatin1String("since")) ) { // DEPRECATED
                    currentEntry.version = attributes().value( QLatin1String("since") ).toString();
                }
                if( attributes().hasAttribute(QLatin1String("releasedWith")) ) {
                    currentEntry.engineVersion = attributes().value( QLatin1String("releasedWith") ).toString();
                } else if ( attributes().hasAttribute(QLatin1String("engineVersion")) ) { // DEPRECATED
                    currentEntry.engineVersion = attributes().value( QLatin1String("engineVersion") ).toString();
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
