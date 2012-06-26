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
#include "serviceprovider.h"

// Own includes
#include "serviceproviderdata.h"
#include "serviceprovidertestdata.h"
#include "serviceproviderglobal.h"
#include "serviceproviderdatareader.h"
#include "serviceproviderscript.h"
#include "serviceprovidergtfs.h"
#include "departureinfo.h"
#include "request.h"

// KDE includes
#include <KIO/NetAccess>
#include <KStandardDirs>
#include <KLocale>
#include <KDebug>
#include <KMimeType>
#include <KConfigGroup>
#include <kio/job.h>

// Qt includes
#include <QTextCodec>
#include <QFile>
#include <QTimer>
#include <QFileInfo>
#include <QScriptEngine>

ServiceProvider::ServiceProvider( const ServiceProviderData *data, QObject *parent,
                                  const QSharedPointer< KConfig > &cache )
        : QObject(parent),
          m_data(data ? data : new ServiceProviderData(InvalidProvider, QString(), this))
{
    Q_UNUSED( cache );
    const_cast<ServiceProviderData*>(m_data)->setParent( this );
    m_idAlreadyRequested = false;
}

ServiceProvider::~ServiceProvider()
{
    if ( !m_jobInfos.isEmpty() ) {
        // There are still pending requests, data haven't been completely received.
        // The result is, that the data engine won't be able to fill the data source with values.
        // A possible cause is calling Plasma::DataEngineManager::unloadEngine() more often
        // than Plasma::DataEngineManager::loadEngine(), which deletes the whole data engine
        // with all it's currently loaded service provider plugins even if one other visualization
        // is connected to the data engine. The dataUpdated() slot of this other visualization
        // won't be called.
        kDebug() << "Service provider with" << m_jobInfos.count() << "pending requests deleted";
        if ( m_data ) {
            kDebug() << m_data->id() << m_jobInfos.count();
        }
    }
}

ServiceProviderTestData ServiceProvider::runSubTypeTest( const ServiceProviderTestData &oldTestData,
                                                         const QSharedPointer< KConfig > cache ) const
{
    if ( oldTestData.isSubTypeTestPending() || !isTestResultUnchanged(cache) ) {
        // Run subclass tests
        QString errorMessage;
        const bool testResult = runTests( &errorMessage );

        // Write test result
        ServiceProviderTestData newTestData = oldTestData;
        newTestData.setSubTypeTestStatus( testResult ? ServiceProviderTestData::Passed
                : ServiceProviderTestData::Failed, errorMessage );
        newTestData.write( id(), cache );
        return newTestData;
    } else {
        return oldTestData;
    }
}

QString ServiceProvider::typeName( ServiceProviderType type )
{
    switch ( type ) {
    case ScriptedProvider:
        return i18nc("@info/plain Name of a service provider plugin type", "Scripted");
    case GtfsProvider:
        return i18nc("@info/plain Name of a service provider plugin type", "GTFS");
    case InvalidProvider:
    default:
        kWarning() << "Invalid provider type" << type;
        return i18nc("@info/plain Name of a service provider plugin type", "Invalid");
    }
}

ServiceProvider *ServiceProvider::createInvalidProvider( QObject *parent )
{
    return new ServiceProvider( 0, parent );
}

ServiceProvider* ServiceProvider::createProvider( const QString &serviceProviderId,
        QObject *parent, const QSharedPointer<KConfig> &cache )
{
    ServiceProviderData *data = readProviderData( serviceProviderId );
    return data ? createProviderForData(data, parent, cache) : 0;
}

ServiceProvider *ServiceProvider::createProviderForData( const ServiceProviderData *data,
        QObject *parent, const QSharedPointer<KConfig> &cache )
{
    switch ( data->type() ) {
    case ScriptedProvider:
        return new ServiceProviderScript( data, parent, cache );
    case GtfsProvider:
        return new ServiceProviderGtfs( data, parent, cache );
    case InvalidProvider:
    default:
        kWarning() << "Invalid/unknown provider type" << data->type();
        return 0;
    }
}

bool ServiceProvider::isSourceFileModified( const QSharedPointer<KConfig> &cache ) const
{
    return ServiceProviderGlobal::isSourceFileModified( m_data->id(), cache );
}

ServiceProviderData *ServiceProvider::readProviderData( const QString &providerId,
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

QStringList ServiceProvider::featuresLocalized() const
{
    return localizeFeatures( features() );
}

QStringList ServiceProvider::localizeFeatures( const QStringList &features )
{
    QStringList featuresl10n;
    if ( features.contains("Arrivals") ) {
        featuresl10n << i18nc( "Support for getting arrivals for a stop of public "
                               "transport. This string is used in a feature list, "
                               "should be short.", "Arrivals" );
    }
    if ( features.contains("Autocompletion") ) {
        featuresl10n << i18nc( "Autocompletion for names of public transport stops",
                               "Autocompletion" );
    }
    if ( features.contains("JourneySearch") ) {
        featuresl10n << i18nc( "Support for getting journeys from one stop to another. "
                               "This string is used in a feature list, should be short.",
                               "Journey search" );
    }
    if ( features.contains("Delay") ) {
        featuresl10n << i18nc( "Support for getting delay information. This string is "
                               "used in a feature list, should be short.", "Delay" );
    }
    if ( features.contains("DelayReason") ) {
        featuresl10n << i18nc( "Support for getting the reason of a delay. This string "
                               "is used in a feature list, should be short.",
                               "Delay reason" );
    }
    if ( features.contains("Platform") ) {
        featuresl10n << i18nc( "Support for getting the information from which platform "
                               "a public transport vehicle departs / at which it "
                               "arrives. This string is used in a feature list, "
                               "should be short.", "Platform" );
    }
    if ( features.contains("JourneyNews") ) {
        featuresl10n << i18nc( "Support for getting the news about a journey with public "
                               "transport, such as a platform change. This string is "
                               "used in a feature list, should be short.", "Journey news" );
    }
    if ( features.contains("TypeOfVehicle") ) {
        featuresl10n << i18nc( "Support for getting information about the type of "
                               "vehicle of a journey with public transport. This string "
                               "is used in a feature list, should be short.",
                               "Type of vehicle" );
    }
    if ( features.contains("Status") ) {
        featuresl10n << i18nc( "Support for getting information about the status of a "
                               "journey with public transport or an aeroplane. This "
                               "string is used in a feature list, should be short.",
                               "Status" );
    }
    if ( features.contains("Operator") ) {
        featuresl10n << i18nc( "Support for getting the operator of a journey with public "
                               "transport or an aeroplane. This string is used in a "
                               "feature list, should be short.", "Operator" );
    }
    if ( features.contains("StopID") ) {
        featuresl10n << i18nc( "Support for getting the id of a stop of public transport. "
                               "This string is used in a feature list, should be short.",
                               "Stop ID" );
    }
    return featuresl10n;
}

void ServiceProvider::requestDepartures( const DepartureRequest &request )
{
    Q_UNUSED( request );
    kDebug() << "Not implemented";
    return;
}

void ServiceProvider::requestArrivals( const ArrivalRequest &request )
{
    Q_UNUSED( request );
    kDebug() << "Not implemented";
    return;
}

void ServiceProvider::requestStopSuggestions( const StopSuggestionRequest &request )
{
    Q_UNUSED( request );
    kDebug() << "Not implemented";
    return;
}

void ServiceProvider::requestJourneys( const JourneyRequest &request )
{
    Q_UNUSED( request );
    kDebug() << "Not implemented";
    return;
}

QString ServiceProvider::gethex( ushort decimal )
{
    QString hexchars = "0123456789ABCDEFabcdef";
    return QChar( '%' ) + hexchars[decimal >> 4] + hexchars[decimal & 0xF];
}

QString ServiceProvider::toPercentEncoding( const QString &str, const QByteArray &charset )
{
    QString unreserved = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~";
    QString encoded;

    QByteArray ba = QTextCodec::codecForName( charset )->fromUnicode( str );
    for ( int i = 0; i < ba.length(); ++i ) {
        char ch = ba[i];
        if ( unreserved.indexOf(ch) != -1 ) {
            encoded += ch;
        } else if ( ch < 0 ) {
            encoded += gethex( 256 + ch );
        } else {
            encoded += gethex( ch );
        }
    }

    return encoded;
}

QByteArray ServiceProvider::charsetForUrlEncoding() const
{
    return m_data->charsetForUrlEncoding();
}

QString ServiceProvider::id() const
{
    return m_data->id();
}

ServiceProviderType ServiceProvider::type() const
{
    return m_data->type();
}

int ServiceProvider::minFetchWait() const
{
    return m_data->minFetchWait();
}

QString ServiceProvider::country() const
{
    return m_data->country();
}

QStringList ServiceProvider::cities() const
{
    return m_data->cities();
}

QString ServiceProvider::credit() const
{
    return m_data->credit();
}

bool ServiceProvider::useSeparateCityValue() const
{
    return m_data->useSeparateCityValue();
}

bool ServiceProvider::onlyUseCitiesInList() const
{
    return m_data->onlyUseCitiesInList();
}

ServiceProvider::JobInfos::~JobInfos()
{
    delete request;
}
