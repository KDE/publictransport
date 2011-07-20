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

/** @file
* @brief This file contains classes, which hold information about public transport journeys/departures/arrivals.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef DEPARTUREINFO_HEADER
#define DEPARTUREINFO_HEADER

// Qt includes
#include <QString>
#include <QTime>
#include <QRegExp>
#include <QSet>
#include <QVariant>
#include <QDebug>

// KDE includes
#include <KLocalizedString>

// Own includes
#include "global.h"

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

Q_DECLARE_FLAGS( LineServices, LineService )

/** @class PublicTransportInfo
 * @brief Base class for DepartureInfo and JourneyInfo.
 *
 * Use @ref PublicTransportInfo::hash to get an unsigned integer value unique for this
 * departure/arrival/journey.
 *
 * @ingroup models
 **/
class PUBLICTRANSPORTHELPER_EXPORT PublicTransportInfo {
public:
    PublicTransportInfo() { };

    /**
     * @brief Gets an unsigned integer value unique for this departure/arrival/journey.
     *
     * The value returned by this function will be equal for items (departures/arrivals/journeys)
     * that are equal. Two items are considered equal, also if they aren't exactly equal,
     * eg. the delay may be different. That is important to be able to find items from the data
     * engine in the model of the applet after an update. For example a departure which delay has
     * changed is still the same departure and therefore it returns the same hash value.
     *
     * @return uint The hash value for this item.
     **/
    uint hash() const { return m_hash; };

protected:
    uint m_hash;
};

/** @class JourneyInfo
 * @brief Stores information about journeys.
 *
 * @ingroup models
 **/
class PUBLICTRANSPORTHELPER_EXPORT JourneyInfo : public PublicTransportInfo {
public:
    JourneyInfo() : PublicTransportInfo() {
        m_duration = -1;
    };

    JourneyInfo( const QString &operatorName, const QVariantList &vehicleTypesVariant,
            const QDateTime &departure, const QDateTime &arrival, const QString &pricing,
            const QString &startStopName, const QString &targetStopName, int duration, int changes,
            const QString &journeyNews = QString(), const QStringList &routeStops = QStringList(),
            const QStringList &routeTransportLines = QStringList(),
            const QStringList &routePlatformsDeparture = QStringList(),
            const QStringList &routePlatformsArrival = QStringList(),
            const QVariantList &routeVehicleTypes = QVariantList(),
            const QList<QTime> &routeTimesDeparture = QList<QTime>(),
            const QList<QTime> &routeTimesArrival = QList<QTime>(),
            const QList<int> &routeTimesDepartureDelay = QList<int>(),
            const QList<int> &routeTimesArrivalDelay = QList<int>(),
            int routeExactStops = 0 );

    JourneyInfo( const QString &operatorName, const QSet<VehicleType> &vehicleTypes,
            const QDateTime &departure, const QDateTime &arrival, const QString &pricing,
            const QString &startStopName, const QString &targetStopName, int duration, int changes,
            const QString &journeyNews = QString(), const QStringList &routeStops = QStringList(),
            const QStringList &routeTransportLines = QStringList(),
            const QStringList &routePlatformsDeparture = QStringList(),
            const QStringList &routePlatformsArrival = QStringList(),
            const QList<VehicleType> &routeVehicleTypes = QList<VehicleType>(),
            const QList<QTime> &routeTimesDeparture = QList<QTime>(),
            const QList<QTime> &routeTimesArrival = QList<QTime>(),
            const QList<int> &routeTimesDepartureDelay = QList<int>(),
            const QList<int> &routeTimesArrivalDelay = QList<int>(),
            int routeExactStops = 0  ) : PublicTransportInfo()
    {
        init( operatorName, vehicleTypes, departure, arrival, pricing,
            startStopName, targetStopName, duration, changes, journeyNews,
            routeStops, routeTransportLines, routePlatformsDeparture,
            routePlatformsArrival, routeVehicleTypes, routeTimesDeparture,
            routeTimesArrival, routeTimesDepartureDelay, routeTimesArrivalDelay,
            routeExactStops );
    };

    /** @brief Whether or not this journey information is valid. */
    bool isValid() const { return m_duration >= 0; };

    QString departureText( bool htmlFormatted, bool displayTimeBold, bool showRemainingMinutes,
                        bool showDepartureTime, int linesPerRow ) const;
    QString arrivalText( bool htmlFormatted, bool displayTimeBold, bool showRemainingMinutes,
                        bool showDepartureTime, int linesPerRow ) const;

    QList<QVariant> vehicleTypesVariant() const;
    QString durationToDepartureString( bool toArrival = false ) const;

    /** @returns the departure date and time of this journey. */
    QDateTime departure() const { return m_departure; };

    /** @returns the arrival date and time of this journey. */
    QDateTime arrival() const { return m_arrival; };

    /** @returns the operator for this journey. */
    QString operatorName() const { return m_operator; };

    /** @returns pricing information for this journey. */
    QString pricing() const { return m_pricing; };

    /** @returns the name of the start stop of this journey. */
    QString startStopName() const { return m_startStopName; };

    /** @returns the name of the target stop of this journey. */
    QString targetStopName() const { return m_targetStopName; };

    /** @returns an information string with news for this journey. */
    QString journeyNews() const { return m_journeyNews; };

    /** @returns a list of vehicle types used by this journey. Each vehicle type is contained only once. */
    QSet<VehicleType> vehicleTypes() const { return m_vehicleTypes; };

    /** @returns the duration of this journey in minutes. */
    int duration() const { return m_duration; };

    /** @returns the needed changes for this journey. */
    int changes() const { return m_changes; };

    /** @returns the number of exact stops in @ref routeStops. Some of the following stops
     * have been omitted. */
    int routeExactStops() const { return m_routeExactStops; };

    /** @returns a list of intermediate stop names. */
    QStringList routeStops() const { return m_routeStops; };

    QStringList routeTransportLines() const { return m_routeTransportLines; };
    QStringList routePlatformsDeparture() const { return m_routePlatformsDeparture; };
    QStringList routePlatformsArrival() const { return m_routePlatformsArrival; };
    QList<QTime> routeTimesDeparture() const { return m_routeTimesDeparture; };
    QList<QTime> routeTimesArrival() const { return m_routeTimesArrival; };
    QList<int> routeTimesDepartureDelay() const { return m_routeTimesDepartureDelay; };
    QList<int> routeTimesArrivalDelay() const { return m_routeTimesArrivalDelay; };

    /** @returns a list of vehicle types used by this journey in chronological order. */
    QList<VehicleType> routeVehicleTypes() const { return m_routeVehicleTypes; };

    QList< int > matchedAlarms() const { return m_matchedAlarms; };
    QList< int > &matchedAlarms() { return m_matchedAlarms; };

private:
    void init( const QString &operatorName, const QSet<VehicleType> &vehicleTypes,
               const QDateTime &departure, const QDateTime &arrival,
               const QString &pricing, const QString &startStopName,
               const QString &targetStopName, int duration, int changes,
               const QString &journeyNews = QString(),
               const QStringList &routeStops = QStringList(),
               const QStringList &routeTransportLines = QStringList(),
               const QStringList &routePlatformsDeparture = QStringList(),
               const QStringList &routePlatformsArrival = QStringList(),
               const QList<VehicleType> &routeVehicleTypes = QList<VehicleType>(),
               const QList<QTime> &routeTimesDeparture = QList<QTime>(),
               const QList<QTime> &routeTimesArrival = QList<QTime>(),
               const QList<int> &routeTimesDepartureDelay = QList<int>(),
               const QList<int> &routeTimesArrivalDelay = QList<int>(),
               int routeExactStops = 0 );

    void generateHash();


    QDateTime m_departure, m_arrival;
    QString m_operator, m_pricing, m_startStopName, m_targetStopName, m_journeyNews;
    QSet<VehicleType> m_vehicleTypes;
    QList<VehicleType> m_routeVehicleTypes;
    int m_duration, m_changes, m_routeExactStops;
    QStringList m_routeStops, m_routeTransportLines,
        m_routePlatformsDeparture, m_routePlatformsArrival;
    QList<QTime> m_routeTimesDeparture, m_routeTimesArrival;
    QList<int> m_routeTimesDepartureDelay, m_routeTimesArrivalDelay;
    QList<int> m_matchedAlarms;
};

bool PUBLICTRANSPORTHELPER_EXPORT operator <( const JourneyInfo &ji1, const JourneyInfo &ji2 );

/** @class DepartureInfo
 * @brief Stores information about departures / arrivals.
 * 
 * @ingroup models */
class PUBLICTRANSPORTHELPER_EXPORT DepartureInfo : public PublicTransportInfo {
public:
    /** Creates an invalid DepartureInfo object. */
    DepartureInfo() : PublicTransportInfo() {
    };

    DepartureInfo( const QString &operatorName, const QString &line, const QString &target,
                   const QDateTime &departure, VehicleType lineType,
                   const QStringList &routeStops = QStringList(),
                   const QList<QTime> &routeTimes = QList<QTime>(),
                   int routeExactStops = 0 ) : PublicTransportInfo()
    {
        init( operatorName, line, target, departure, lineType, NoLineService,
              QString(), -1, QString(), QString(), routeStops, routeTimes, routeExactStops );
    };

    DepartureInfo( const QString &operatorName, const QString &line, const QString &target,
                   const QDateTime &departure, VehicleType lineType, bool nightLine,
                   bool expressLine, const QString &platform = QString(), int delay = -1,
                   const QString &delayReason = QString(), const QString &journeyNews = QString(),
                   const QStringList &routeStops = QStringList(),
                   const QList<QTime> &routeTimes = QList<QTime>(),
                   int routeExactStops = 0 ) : PublicTransportInfo() {
        LineServices lineServices = NoLineService;
        if ( nightLine ) {
            lineServices |= NightLine;
        }
        if ( expressLine ) {
            lineServices |= ExpressLine;
        }
        init( operatorName, line, target, departure, lineType, lineServices, platform, delay,
              delayReason, journeyNews, routeStops, routeTimes, routeExactStops );
    };

    DepartureInfo( const QString &operatorName, const QString &line, const QString &target,
                   const QDateTime &departure, VehicleType lineType, LineServices lineServices,
                   const QString &platform = QString(), int delay = -1,
                   const QString &delayReason = QString(), const QString &journeyNews = QString(),
                   const QStringList &routeStops = QStringList(),
                   const QList<QTime> &routeTimes = QList<QTime>(),
                   int routeExactStops = 0 ) : PublicTransportInfo() {
        init( operatorName, line, target, departure, lineType, lineServices, platform, delay,
              delayReason, journeyNews, routeStops, routeTimes, routeExactStops );
    };

    static QString formatDateFancyFuture( const QDate& date );

    bool operator ==( const DepartureInfo &other ) const;

    bool isFilteredOut() const { return m_filteredOut; };
    void setFilteredOut( bool filteredOut = false ) { m_filteredOut = filteredOut; };

    /**
     * @brief Whether or not this DepartureInfo object is valid.
     *
     * It currently checks validity by checking if the lineString is empty.
     **/
    bool isValid() const { return !m_lineString.isEmpty(); };

    /** @brief Gets the text to be displayed in the item for delay information. */
    QString delayText() const;

    QString departureText( bool htmlFormatted, bool displayTimeBold, bool showRemainingMinutes,
                           bool showDepartureTime, int linesPerRow ) const;

    /** @brief Whether or not the line number of this departure / arrival is valid. */
    bool isLineNumberValid() const {
        return m_lineNumber > 0 &&
                m_vehicleType != Unknown && // May not been parsed correctly
                m_vehicleType < 10; // Isn't a train (bus | tram | subway | interurbantrain | metro | trolleybus). Line numbers are only valid for those. TODO: Change this?
    };

    /** @brief The line number. @see isLineNumberValid */
    int lineNumber() const { return m_lineNumber; };

    QString durationString( bool showDelay = true ) const;
    QString delayString( bool htmlFormatted = false ) const;

    bool isNightLine() const { return m_lineServices.testFlag(NightLine); };
    bool isExpressLine() const { return m_lineServices.testFlag(ExpressLine); };
    DelayType delayType() const {
        if ( m_delay < 0 ) {
            return DelayUnknown;
        } else if ( m_delay == 0 ) {
            return OnSchedule;
        } else {
            return Delayed;
        }
    };

    /**
     * @brief Gets the "real" departure time, which is the departure time from the timetable
     * plus the delay.
     **/
    QDateTime predictedDeparture() const {
        return delayType() == Delayed ? m_departure.addSecs(m_delay * 60) : m_departure;
    };

    /**
     * @brief Whether or not the line number is in the specified range.
     * 
     * @return true, if the line number is in the specified range or if it is
     *   greater than 999.
     * @return false, if the line number is not in the specified range.
     * 
     * @see isLineNumberValid()
     * @see lineNumber
     **/
    bool isLineNumberInRange( int min, int max ) const {
        return (m_lineNumber >= min && m_lineNumber <= max) || m_lineNumber >= 1000;
    };

    /** @returns the operator for this departure/arrival. */
    QString operatorName() const { return m_operator; };

    /** @returns the target/origin of this departure/arrival. */
    QString target() const { return m_target; };

    /** @returns the line string of this departure/arrival. */
    QString lineString() const { return m_lineString; };

    /** @returns the platform at which this departure departs or this arrival arrives. */
    QString platform() const { return m_platform; };

    /** @returns a string containing the reason of a delay if any. */
    QString delayReason() const { return m_delayReason; };

    QString journeyNews() const { return m_journeyNews; };

    /** @returns the departure/arrival time. */
    QDateTime departure() const { return m_departure; };

    /** The delay in minutes or -1 if there's no information about delays. */
    int delay() const { return m_delay; };

    /** @returns the vehicle type of this departure/arrival. */
    VehicleType vehicleType() const { return m_vehicleType; };

    LineServices lineServices() const { return m_lineServices; };
    
    /** @returns a list of intermediate stop names. */
    QStringList routeStops() const { return m_routeStops; };
    
    /** @returns a list of QTime objects. Each time corresponds to the stop
     * in @ref routeStops with the same index. */
    QList<QTime> routeTimes() const { return m_routeTimes; };

    /** @returns the number of exact stops in @ref routeStops. Some of the
     * following stops have been omitted. */
    int routeExactStops() const { return m_routeExactStops; };

    QList< int > matchedAlarms() const { return m_matchedAlarms; };
    QList< int > &matchedAlarms() { return m_matchedAlarms; };

private:
    void init( const QString &operatorName, const QString &line, const QString &target,
            const QDateTime &departure, VehicleType lineType,
            LineServices lineServices = NoLineService, const QString &platform = QString(),
            int delay = -1, const QString &delayReason = QString(),
            const QString &journeyNews = QString(),
            const QStringList &routeStops = QStringList(),
            const QList<QTime> &routeTimes = QList<QTime>(), int routeExactStops = 0 );

    void generateHash();

    int m_lineNumber;
    QString m_operator, m_target, m_lineString, m_platform, m_delayReason, m_journeyNews;
    QDateTime m_departure;
    int m_delay;
    VehicleType m_vehicleType;
    LineServices m_lineServices;
    QStringList m_routeStops;
    QList<QTime> m_routeTimes;
    int m_routeExactStops;
    bool m_filteredOut;
    QList< int > m_matchedAlarms;
};

uint PUBLICTRANSPORTHELPER_EXPORT qHash( const DepartureInfo &departureInfo );
bool PUBLICTRANSPORTHELPER_EXPORT operator <( const DepartureInfo &di1, const DepartureInfo &di2 );

inline QDebug& operator<<( QDebug debug, const DepartureInfo& departureInfo )
{
    return debug << QString( "(%1 %2 at %3)" )
            .arg( departureInfo.lineString() )
            .arg( departureInfo.target() )
            .arg( departureInfo.predictedDeparture().toString() );
}

inline QDebug& operator<<( QDebug debug, const JourneyInfo& journeyInfo )
{
    return debug << QString( "(from %1 to %2, %3, %4 changes at %5)" )
            .arg( journeyInfo.startStopName() )
            .arg( journeyInfo.targetStopName() )
            .arg( journeyInfo.durationToDepartureString() )
            .arg( journeyInfo.changes() )
            .arg( journeyInfo.departure().toString() );
}

}; // namespace Timetable

Q_DECLARE_METATYPE( Timetable::DepartureInfo );
Q_DECLARE_METATYPE( Timetable::JourneyInfo );

#endif // DEPARTUREINFO_HEADER
