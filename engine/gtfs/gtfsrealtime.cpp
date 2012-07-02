/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

#include "gtfsrealtime.h"

#include "gtfs-realtime.pb.h"
#include <KDebug>

QList< GtfsRealtimeTripUpdate >* GtfsRealtimeTripUpdate::fromProtocolBuffer( const QByteArray &data )
{
    kDebug() << "GTFS-realtime trip updates received" << data.size();
    GtfsRealtimeTripUpdates *tripUpdates = new GtfsRealtimeTripUpdates();

    transit_realtime::FeedMessage feedMessage;
    if ( !feedMessage.ParsePartialFromArray(data.constData(), data.size()) ) {
        kDebug() << "Wrong protocol buffer format" << data.constData();
        return tripUpdates;
    }
    if ( feedMessage.has_header() && feedMessage.header().gtfs_realtime_version() != "1"
                                  && feedMessage.header().gtfs_realtime_version() != "1.0" )
    {
        kDebug() << "Unsupported GTFS-realtime version:"
                 << feedMessage.header().gtfs_realtime_version().data();
        return tripUpdates;
    }

    kDebug() << "entityCount:" << feedMessage.entity_size();
    tripUpdates->reserve( feedMessage.entity_size() );
    for ( int i = 0; i < feedMessage.entity_size(); ++i ) {
        GtfsRealtimeTripUpdate tripUpdate;
        const transit_realtime::TripUpdate newTripUpdate = feedMessage.entity( i ).trip_update();
        const transit_realtime::TripDescriptor newTripDescriptor = newTripUpdate.trip();

        tripUpdate.routeId = qHash( QString(newTripDescriptor.route_id().data()) );
        tripUpdate.tripId = qHash( QString(newTripDescriptor.trip_id().data()) );
        QDate startDate = QDate::fromString( newTripDescriptor.start_date().data() );
        tripUpdate.startDateTime = QDateTime( startDate,
                QTime::fromString(newTripDescriptor.start_time().data()) );
        tripUpdate.tripScheduleRelationship = static_cast<TripScheduleRelationship>(
                newTripDescriptor.schedule_relationship() );

        for ( int n = 0; n < newTripUpdate.stop_time_update_size(); ++n ) {
            GtfsRealtimeStopTimeUpdate stopTimeUpdate;
            const transit_realtime::TripUpdate::StopTimeUpdate newStopTimeUpdate =
                    newTripUpdate.stop_time_update( n );
            stopTimeUpdate.stopId = qHash( QString(newStopTimeUpdate.stop_id().data()) );
            stopTimeUpdate.stopSequence = newStopTimeUpdate.stop_sequence();

            stopTimeUpdate.arrivalDelay = newStopTimeUpdate.arrival().has_delay()
                    ? qint32(newStopTimeUpdate.arrival().delay()) : -1;
            stopTimeUpdate.arrivalTime = newStopTimeUpdate.arrival().has_time()
                    ? QDateTime::fromTime_t(newStopTimeUpdate.arrival().time()) : QDateTime();
            stopTimeUpdate.arrivalUncertainty = newStopTimeUpdate.arrival().uncertainty();

            stopTimeUpdate.departureDelay = newStopTimeUpdate.departure().has_delay()
                    ? qint32(newStopTimeUpdate.departure().delay()) : -1;
            stopTimeUpdate.departureTime = newStopTimeUpdate.departure().has_time()
                    ? QDateTime::fromTime_t(newStopTimeUpdate.departure().time()) : QDateTime();
            stopTimeUpdate.departureUncertainty = newStopTimeUpdate.departure().uncertainty();

            stopTimeUpdate.scheduleRelationship =
                    static_cast<GtfsRealtimeStopTimeUpdate::ScheduleRelationship>(
                    newStopTimeUpdate.schedule_relationship() );

            tripUpdate.stopTimeUpdates << stopTimeUpdate;
        }

        tripUpdates->append( tripUpdate );
    }

    return tripUpdates;
}

QList< GtfsRealtimeAlert >* GtfsRealtimeAlert::fromProtocolBuffer( const QByteArray &data )
{
    kDebug() << "GTFS-realtime alerts received" << data.size();
    GtfsRealtimeAlerts *alerts = new GtfsRealtimeAlerts();

    transit_realtime::FeedMessage feedMessage;
    if ( !feedMessage.ParsePartialFromArray(data.constData(), data.size()) ) {
        kDebug() << "Wrong protocol buffer format";
        return alerts;
    }
    if ( feedMessage.has_header() && feedMessage.header().gtfs_realtime_version() != "1"
                                  && feedMessage.header().gtfs_realtime_version() != "1.0" )
    {
        kDebug() << "Unsupported GTFS-realtime version:"
                 << feedMessage.header().gtfs_realtime_version().data();
        return alerts;
    }

    kDebug() << "entityCount:" << feedMessage.entity_size();
    alerts->reserve( feedMessage.entity_size() );
    for ( int i = 0; i < feedMessage.entity_size(); ++i ) {
        GtfsRealtimeAlert alert;
        const transit_realtime::Alert newAlert = feedMessage.entity( i ).alert();
        for ( int n = 0; n < newAlert.active_period_size(); ++n ) {
            GtfsRealtimeTimeSpan activePeriod;
            const transit_realtime::TimeRange newActivePeriod = newAlert.active_period( n );
            if ( newActivePeriod.has_start() ) {
                activePeriod.start = QDateTime::fromTime_t( newActivePeriod.start() );
            }
            if ( newActivePeriod.has_end() ) {
                activePeriod.end = QDateTime::fromTime_t( newActivePeriod.end() );
            }
            alert.activePeriods << activePeriod;
        }
        alert.summary = newAlert.header_text().translation_size() == 0 ? QString()
                : newAlert.header_text().translation(0).text().data(); // TODO Choose local language
        alert.description = newAlert.description_text().translation_size() == 0 ? QString()
                : newAlert.description_text().translation(0).text().data(); // TODO Choose local language
        alert.url = newAlert.url().translation_size() == 0 ? QString()
                : newAlert.url().translation(0).text().data();
        alert.cause = static_cast<GtfsRealtimeAlert::Cause>( newAlert.cause() );
        alert.effect = static_cast<GtfsRealtimeAlert::Effect>( newAlert.effect() );

        alerts->append( alert );
    }

    return alerts;
}

// TODO timezone...
bool GtfsRealtimeTimeSpan::isInRange( const QDateTime &dateTime ) const
{
    return dateTime >= start && dateTime <= end;
}

bool GtfsRealtimeAlert::isActiveAt( const QDateTime &dateTime ) const
{
    foreach ( const GtfsRealtimeTimeSpan &timeSpan, activePeriods ) {
        if ( timeSpan.isInRange(dateTime) ) {
            return true;
        }
    }

    return false;
}
