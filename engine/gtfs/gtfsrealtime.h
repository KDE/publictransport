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

/** @file
* @brief This file contains a class to access data from GTFS realtime data.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef GTFSREALTIME_HEADER
#define GTFSREALTIME_HEADER

#include <QDateTime>

struct GtfsRealtimeStopTimeUpdate {
    // The relation between this StopTime and the static schedule.
    enum ScheduleRelationship {
        // The vehicle is proceeding in accordance with its static schedule of
        // stops, although not necessarily according to the times of the schedule.
        // At least one of arrival and departure must be provided. If the schedule
        // for this stop contains both arrival and departure times then so must
        // this update. An update with only an arrival, say, where the schedule
        // has both, indicates that the trip is terminating early at this stop.
        Scheduled = 0,

        // The stop is skipped, i.e., the vehicle will not stop at this stop.
        // Arrival and departure are optional.
        Skipped = 1,

        // No data is given for this stop. The main intention for this value is to
        // give the predictions only for part of a trip, i.e., if the last update
        // for a trip has a NO_DATA specifier, then StopTimes for the rest of the
        // stops in the trip are considered to be unspecified as well.
        // Neither arrival nor departure should be supplied.
        NoData = 2
    };

    uint stopId;
    uint stopSequence;

    int arrivalDelay;
    int departureDelay;
    QDateTime departureTime;
    QDateTime arrivalTime;

    int arrivalUncertainty;
    int departureUncertainty;

    ScheduleRelationship scheduleRelationship;
};
typedef QList<GtfsRealtimeStopTimeUpdate> GtfsRealtimeStopTimeUpdates;

struct GtfsRealtimeTripUpdate {
    enum TripScheduleRelationship {
        Scheduled = 0,
        Added = 1,
        Unscheduled = 2,
        Canceled = 3,
        Replacement = 5
    };

    static QList<GtfsRealtimeTripUpdate> *fromProtocolBuffer( const QByteArray &data );

    uint tripId;
    uint routeId;
    QDateTime startDateTime;
    TripScheduleRelationship tripScheduleRelationship;
    GtfsRealtimeStopTimeUpdates stopTimeUpdates;
};
typedef QList<GtfsRealtimeTripUpdate> GtfsRealtimeTripUpdates;

struct GtfsRealtimeTimeSpan {
    bool isInRange( const QDateTime &dateTime ) const;

    QDateTime start;
    QDateTime end;
};
typedef QList<GtfsRealtimeTimeSpan> GtfsRealtimeTimeSpans;

struct GtfsRealtimeAlert {
    enum Cause {
        UnknownCause = 1,
        OtherCause = 2,        // Not machine-representable.
        TechnicalProblem = 3,
        Strike = 4,             // Public transit agency employees stopped working.
        Demonstration = 5,      // People are blocking the streets.
        Accident = 6,
        Holiday = 7,
        Weather = 8,
        Maintenance = 9,
        Construction = 10,
        PoliceActivity = 11,
        MedicalEmergency = 12
    };

    // What is the effect of this problem on the affected entity
    enum Effect {
        NoService = 1,
        ReducedService = 2,

        // We don't care about INsignificant delays: they are hard to detect, have
        // little impact on the user, and would clutter the results as they are too
        // frequent.
        SignificantDelays = 3,

        Detour = 4,
        AdditionalService = 5,
        ModifiedService = 6,
        OtherEffect = 7,
        UnknownEffect = 8,
        StopMoved = 9
    };

    static QList<GtfsRealtimeAlert> *fromProtocolBuffer( const QByteArray &data );

    bool isActiveAt( const QDateTime &dateTime ) const;

    QString summary;
    QString description;
    QString url;
    Cause cause;
    Effect effect;
    GtfsRealtimeTimeSpans activePeriods;
};
typedef QList<GtfsRealtimeAlert> GtfsRealtimeAlerts;

#endif // Multiple inclusion guard
