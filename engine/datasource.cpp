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

// Own includes
#include "datasource.h"
#include "serviceprovider.h"

// KDE includes
#include <KDebug>
#include <KLocalizedString>

// Qt includes
#include <QTimer>

DataSource::DataSource( const QString &dataSource ) : m_name(dataSource)
{
}

DataSource::~DataSource()
{
}

ProvidersDataSource::ProvidersDataSource( const QString &dataSource,
                                          const QHash<QString, ProviderData> &providerData )
        : DataSource(dataSource), m_providerDirectoryWasChanged(false), m_providerData(providerData)
{
}

SimpleDataSource::SimpleDataSource( const QString &dataSource, const QVariantHash &data )
        : DataSource(dataSource), m_data(data)
{
}

TimetableDataSource::TimetableDataSource( const QString &dataSource, const QVariantHash &data )
        : SimpleDataSource(dataSource, data), m_updateTimer(0), m_updateAdditionalDataDelayTimer(0)
{
}

TimetableDataSource::~TimetableDataSource()
{
    delete m_updateTimer;
    delete m_updateAdditionalDataDelayTimer;
}

QString ProvidersDataSource::toStaticState( const QString &dynamicStateId )
{
    if ( dynamicStateId == QLatin1String("importing_gtfs_feed") ) {
        return "gtfs_feed_import_pending";
    } else {
        return dynamicStateId;
    }
}

ProvidersDataSource::ProviderData::ProviderData( const QVariantHash &data, const QString &_stateId,
                                                 const QVariantHash &_stateData )
        : dataWithoutState(data), stateId(ProvidersDataSource::toStaticState(_stateId)),
          stateData(_stateData)
{
}

QVariantHash ProvidersDataSource::ProviderData::data() const
{
    // Combine all provider data with state ID and state data
    QVariantHash data = dataWithoutState;
    data[ "state" ] = stateId;
    data[ "stateData" ] = stateData;
    return data;
}

QVariantHash ProvidersDataSource::data() const
{
    // Combine all provider data QVariantHash's into one
    QVariantHash data;
    for ( QHash<QString, ProviderData>::ConstIterator it = m_providerData.constBegin();
          it != m_providerData.constEnd(); ++it )
    {
        data.insert( it.key(), it.value().data() );
    }
    return data;
}

void TimetableDataSource::addUsingDataSource( const QSharedPointer< AbstractRequest > &request,
                                              const QString &sourceName, const QDateTime &dateTime,
                                              int maxCount )
{
    m_dataSources[ sourceName ] = SourceData( request, dateTime, maxCount );
}

void TimetableDataSource::removeUsingDataSource( const QString &sourceName )
{
    m_dataSources.remove( sourceName );
}

bool TimetableDataSource::enoughDataAvailable( const QDateTime &dateTime, int count )
{
    bool foundTime = false;
    int foundCount = 0;
    const QVariantList items = timetableItems();
    for ( int i = 0; i < items.count(); ++i ) {
        const QVariantHash data = items[i].toHash();
        const QDateTime itemDateTime = data["DepartureDateTime"].toDateTime();
        if ( itemDateTime < dateTime ) {
            foundTime = true;
        } else if ( itemDateTime >= dateTime ) {
            if ( dateTime.secsTo(itemDateTime) < 2 * 60 ) {
                foundTime = true;
            }
            if ( !foundTime ) {
                break;
            }

            foundCount = items.count() - i;
            break;
        }
    }

    return foundTime && foundCount > qMax(1, int(count * 0.8));
}

QString TimetableDataSource::timetableItemKey() const
{
    return m_data.contains("departures") ? "departures"
            : (m_data.contains("arrivals") ? "arrivals"
               : (m_data.contains("journeys") ? "journeys" : "stops"));
}

void TimetableDataSource::setTimetableItems( const QVariantList &items )
{
    m_data[ timetableItemKey() ] = items;
}

UpdateFlags TimetableDataSource::updateFlags() const
{
    UpdateFlags flags = NoUpdateFlags;
    if ( hasConstantTime() ) {
        flags |= SourceHasConstantTime;
    }
    return flags;
}

void TimetableDataSource::setUpdateTimer( QTimer *timer )
{
    // Delete old timer (if any) and replace with the new timer
    delete m_updateTimer;
    m_updateTimer = timer;
}

void TimetableDataSource::stopUpdateTimer()
{
    if ( m_updateTimer ) {
        m_updateTimer->stop();
    }
}

void TimetableDataSource::setUpdateAdditionalDataDelayTimer( QTimer *timer )
{
    // Delete old timer (if any) and replace with the new timer
    delete m_updateAdditionalDataDelayTimer;
    m_updateAdditionalDataDelayTimer = timer;
}

QSharedPointer< AbstractRequest > TimetableDataSource::request( const QString &sourceName ) const
{
    return m_dataSources[ sourceName ].request;
}

void ProvidersDataSource::addProvider( const QString &providerId,
                                       const ProvidersDataSource::ProviderData &providerData )
{
    if ( mayProviderBeNewlyChanged(providerId) ) {
        // Provider data available and not marked as changed alraedy
        if ( m_providerData[providerId].data() != providerData.data() ) {
            // The provider data has changed
            m_changedProviders << providerId;
        }
    }

    m_providerData.insert( providerId, providerData );
}

void ProvidersDataSource::removeProvider( const QString &providerId )
{
    m_changedProviders << providerId;
    m_providerData.remove( providerId );
}

QVariantHash ProvidersDataSource::providerData( const QString &providerId ) const
{
    return m_providerData.value( providerId ).data();
}

QString ProvidersDataSource::providerState( const QString &providerId ) const
{
    return m_providerData.value( providerId ).stateId;
}

QVariantHash ProvidersDataSource::providerStateData( const QString &providerId ) const
{
    return m_providerData.value( providerId ).stateData;
}

void ProvidersDataSource::setProviderState( const QString &providerId, const QString &stateId )
{
    if ( mayProviderBeNewlyChanged(providerId) ) {
        // Provider data available and not marked as changed alraedy
        ProviderData providerData = m_providerData[ providerId ];
        if ( providerData.stateId != stateId ) {
            m_changedProviders << providerId;
        }
    }

    m_providerData[ providerId ].stateId = stateId;
}

void ProvidersDataSource::setProviderStateData( const QString &providerId, const QVariantHash &stateData )
{
    if ( mayProviderBeNewlyChanged(providerId) ) {
        // Provider data available and not marked as changed alraedy
        ProviderData providerData = m_providerData[ providerId ];
        if ( providerData.stateData != stateData ) {
            m_changedProviders << providerId;
        }
    }

    m_providerData[ providerId ].stateData = stateData;
}
