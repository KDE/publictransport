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
          m_data(data ? data : new ServiceProviderData(Enums::InvalidProvider, QString(), this))
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

ServiceProvider *ServiceProvider::createInvalidProvider( QObject *parent )
{
    return new ServiceProvider( 0, parent );
}

bool ServiceProvider::isSourceFileModified( const QSharedPointer<KConfig> &cache ) const
{
    return ServiceProviderGlobal::isSourceFileModified( m_data->id(), cache );
}

QStringList ServiceProvider::featuresLocalized() const
{
    return ServiceProviderGlobal::localizeFeatures( features() );
}

void ServiceProvider::request( AbstractRequest *request )
{
    StopSuggestionRequest *stopSuggestionRequest = dynamic_cast< StopSuggestionRequest* >( request );
    if ( stopSuggestionRequest ) {
        requestStopSuggestions( *stopSuggestionRequest );
        return;
    }

    DepartureRequest *departureRequest = dynamic_cast< DepartureRequest* >( request );
    if ( departureRequest ) {
        requestDepartures( *departureRequest );
        return;
    }

    ArrivalRequest *arrivalRequest = dynamic_cast< ArrivalRequest* >( request );
    if ( arrivalRequest ) {
        requestArrivals( *arrivalRequest );
        return;
    }

    JourneyRequest *journeyRequest = dynamic_cast< JourneyRequest* >( request );
    if ( journeyRequest ) {
        requestJourneys( *journeyRequest );
        return;
    }
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

Enums::ServiceProviderType ServiceProvider::type() const
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
