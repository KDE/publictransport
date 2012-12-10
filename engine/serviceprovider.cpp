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

    qRegisterMetaType< StopInfoList >( "StopInfoList" );
    qRegisterMetaType< ArrivalInfoList >( "ArrivalInfoList" );
    qRegisterMetaType< DepartureInfoList >( "DepartureInfoList" );
    qRegisterMetaType< JourneyInfoList >( "JourneyInfoList" );
}

ServiceProvider::~ServiceProvider()
{
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

void ServiceProvider::request( AbstractRequest *request )
{
    StopsByGeoPositionRequest *stopsByGeoPositionRequest =
            dynamic_cast< StopsByGeoPositionRequest* >( request );
    if ( stopsByGeoPositionRequest ) {
        requestStopsByGeoPosition( *stopsByGeoPositionRequest );
        return;
    }

    StopSuggestionRequest *stopSuggestionRequest = dynamic_cast< StopSuggestionRequest* >( request );
    if ( stopSuggestionRequest ) {
        requestStopSuggestions( *stopSuggestionRequest );
        return;
    }

    // ArrivalRequest is a derivate of DepartureRequest, therefore this test needs to be done before
    ArrivalRequest *arrivalRequest = dynamic_cast< ArrivalRequest* >( request );
    if ( arrivalRequest ) {
        requestArrivals( *arrivalRequest );
        return;
    }

    DepartureRequest *departureRequest = dynamic_cast< DepartureRequest* >( request );
    if ( departureRequest ) {
        requestDepartures( *departureRequest );
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

void ServiceProvider::requestStopsByGeoPosition(
        const StopsByGeoPositionRequest &request )
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

void ServiceProvider::requestAdditionalData( const AdditionalDataRequest &request )
{
    Q_UNUSED( request );
    kDebug() << "Not implemented";
    return;
}

void ServiceProvider::requestMoreItems( const MoreItemsRequest &moreItemsRequest )
{
    Q_UNUSED( moreItemsRequest );
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

int ServiceProvider::minFetchWait( UpdateFlags updateFlags ) const
{
    Q_UNUSED( updateFlags )
    return qMax( 60, m_data->minFetchWait() );
}

QDateTime ServiceProvider::nextUpdateTime( UpdateFlags updateFlags, const QDateTime &lastUpdate,
                                           const QDateTime &latestForSufficientChanges,
                                           const QVariantHash &data ) const
{
    if ( !lastUpdate.isValid() ) {
        return QDateTime::currentDateTime();
    }

    if ( updateFlags.testFlag(UpdateWasRequestedManually) ) {
        return lastUpdate.addSecs( minFetchWait(updateFlags) );
    }

    // If the requested time is constant, wait until next midnight
    const QDateTime dateTime = QDateTime::currentDateTime();
    QDateTime _latestForSufficientChanges = updateFlags.testFlag(SourceHasConstantTime)
            ? QDateTime(dateTime.date().addDays(1)) : latestForSufficientChanges;

    if ( isRealtimeDataAvailable(data) ) {
        // Wait maximally 30 minutes until an update if realtime data is available,
        // for more updates the timetable service must be used to request an update manually
        return _latestForSufficientChanges.isValid()
                ? qBound( lastUpdate.addSecs(minFetchWait(updateFlags)), _latestForSufficientChanges,
                          lastUpdate.addSecs(30 * 60) )
                : lastUpdate.addSecs(minFetchWait(updateFlags));
    } else {
        // No realtime data, no need to update existing timetable items,
        // only update to have enough valid items for the data source.
        // With constant time update only at midnight for dynamic date.
        // With dynamic time (eg. the current time) update to have enough items available
        // while old ones get removed as time passes by.
        return _latestForSufficientChanges.isValid()
                ? qMax( lastUpdate.addSecs(minFetchWait()), _latestForSufficientChanges )
                : lastUpdate.addSecs(minFetchWait());
    }
}

bool ServiceProvider::isRealtimeDataAvailable( const QVariantHash &data ) const
{
    return features().contains(Enums::ProvidesDelays) &&
           data.contains("delayInfoAvailable") ? data["delayInfoAvailable"].toBool() : false;
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
