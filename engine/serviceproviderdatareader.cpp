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
#include "serviceprovider.h"
#include "serviceproviderdata.h"
#include "serviceproviderglobal.h"
#include "serviceproviderscript.h"
#include "global.h"

// KDE includes
#include <KLocalizedString>
#include <KGlobal>
#include <KLocale>
#include <KDebug>

// Qt includes
#include <QFile>
#include <QFileInfo>

ServiceProvider *ServiceProviderDataReader::read( QIODevice *device, const QString &fileName,
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

ServiceProvider* ServiceProviderDataReader::read( QIODevice* device,
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

    ServiceProvider *ret = 0;
    while ( !atEnd() ) {
        readNext();

        if ( isStartElement() ) {
            if ( name().compare("serviceProvider", Qt::CaseInsensitive) == 0 &&
                 attributes().value("fileVersion") == QLatin1String("1.0") )
            {
                ret = readServiceProvider( serviceProvider, fileName, country, errorAcceptance, parent );
                break;
            } else {
                raiseError( "The file is not a public transport service provider plugin "
                            "version 1.0 file." );
            }
        }
    }

    if ( closeAfterRead ) {
        device->close();
    }

    if ( error() != NoError ) {
        kDebug() << "     ERROR   " << errorString();
    }
    return error() == NoError ? ret : 0;
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

ServiceProvider* ServiceProviderDataReader::readServiceProvider( const QString &serviceProviderId,
        const QString &fileName, const QString &country, ErrorAcceptance errorAcceptance,
        QObject *parent )
{
    const QString lang = KGlobal::locale()->country();
    QString langRead, url, shortUrl;
    QHash<QString, QString> names, descriptions;
    ServiceProviderType serviceProviderType = ScriptedServiceProvider;
    const QString fileVersion = attributes().value("fileVersion").toString();

    if ( attributes().hasAttribute(QLatin1String("type")) ) {
        serviceProviderType = ServiceProviderGlobal::typeFromString(
                attributes().value(QLatin1String("type")).toString() );
        if ( serviceProviderType == InvalidServiceProvider && errorAcceptance == OnlyReadCorrectFiles ) {
            raiseError( QString("The service provider type %1 is invalid. Currently there is only "
                                "one value allowed: Script. You can use qt.xml to read XML.")
                        .arg(attributes().value("type").toString()) );
            return 0;
        }
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
            } else if ( serviceProviderType == ScriptedServiceProvider &&
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
                    return 0;
                }
                serviceProviderData->setScriptFile( scriptFile, extensions );
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
//         delete serviceProviderData;
//         raiseError( "No <url> tag in service provider plugin XML" );
//         return 0;
    }

    serviceProviderData->setNames( names );
    serviceProviderData->setDescriptions( descriptions );
    serviceProviderData->setUrl( url, shortUrl );
    serviceProviderData->finish();

    // Create the service provider
    if ( serviceProviderData->type() == ScriptedServiceProvider ) {
        // Ensure a script is specified
        if ( serviceProviderData->scriptFileName().isEmpty() && errorAcceptance == OnlyReadCorrectFiles ) {
            delete serviceProviderData;
            raiseError( "Scripted service provider plugins need a script for parsing" );
            return 0;
        }

        // Create the service provider and check for script errors
        ServiceProviderScript *scriptServiceProvider =
                new ServiceProviderScript( serviceProviderData, parent );
        if ( scriptServiceProvider->hasScriptErrors() && errorAcceptance == OnlyReadCorrectFiles ) {
            delete scriptServiceProvider;
            raiseError( "Couldn't correctly load the script (bad script)" );
            return 0;
        } else {
            return scriptServiceProvider;
        }
    }

    delete serviceProviderData;
    raiseError( QString("Service provider type %1 not supported").arg(serviceProviderData->type()) );
    return 0;
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