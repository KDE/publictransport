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

#include "datasource.h"

#include <KDebug>
#include <QTimer>

DataSource::DataSource( const QString &dataSource, const QVariantHash &data )
        : name(dataSource), data(data)
{
}

DataSource::~DataSource()
{
}

TimetableDataSource::TimetableDataSource( const QString &dataSource, const QVariantHash &data )
        : DataSource(dataSource, data), m_updateTimer(0), m_updateAdditionalDataDelayTimer(0)
{
}

TimetableDataSource::~TimetableDataSource()
{
    delete m_updateTimer;
    delete m_updateAdditionalDataDelayTimer;
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
    return data.contains("departures") ? "departures"
            : (data.contains("arrivals") ? "arrivals"
               : (data.contains("journeys") ? "journeys" : "stops"));
}

QVariantList TimetableDataSource::timetableItems() const
{
    return data[ timetableItemKey() ].toList();
}

void TimetableDataSource::setTimetableItems( const QVariantList &items )
{
    data[ timetableItemKey() ] = items;
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
