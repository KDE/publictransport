/*
*   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef DEPARTUREINFO_HEADER
#define DEPARTUREINFO_HEADER

// Qt includes
#include <QString>
#include <QTime>
#include <QRegExp>
#include <QVariant>

// KDE includes
#include <KLocalizedString>

// Own includes
#include "global.h"


Q_DECLARE_FLAGS( LineServices, LineService )

/** @class JourneyInfo
*
* @brief Stores information about journeys. */
class JourneyInfo {
    public:
	JourneyInfo() {
	    m_duration = -1;
	};

	JourneyInfo( const QString &operatorName,
		     const QVariantList &vehicleTypesVariant,
		     const QDateTime &departure, const QDateTime &arrival,
		     const QString &pricing, const QString &startStopName,
		     const QString &targetStopName, int duration, int changes,
		     const QString &journeyNews = QString(),
		     const QStringList &routeStops = QStringList(),
		     const QStringList &routeTransportLines = QStringList(),
		     const QStringList &routePlatformsDeparture = QStringList(),
		     const QStringList &routePlatformsArrival = QStringList(),
		     const QVariantList &routeVehicleTypes = QVariantList(),
		     const QList<QTime> &routeTimesDeparture = QList<QTime>(),
		     const QList<QTime> &routeTimesArrival = QList<QTime>(),
		     const QList<int> &routeTimesDepartureDelay = QList<int>(),
		     const QList<int> &routeTimesArrivalDelay = QList<int>(),
		     int routeExactStops = 0 );

	JourneyInfo( const QString &operatorName, const QList<VehicleType> &vehicleTypes,
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
		     int routeExactStops = 0  ) {
	    init( operatorName, vehicleTypes, departure, arrival, pricing,
		  startStopName, targetStopName, duration, changes, journeyNews,
		  routeStops, routeTransportLines, routePlatformsDeparture,
		  routePlatformsArrival, routeVehicleTypes, routeTimesDeparture,
		  routeTimesArrival, routeTimesDepartureDelay, routeTimesArrivalDelay,
		  routeExactStops );
	};

	bool isValid() const { return m_duration >= 0; };

	QList<QVariant> vehicleTypesVariant() const;
	QString durationToDepartureString( bool toArrival = false ) const;
	
	QDateTime departure() const { return m_departure; };
	QDateTime arrival() const { return m_arrival; };
	QString operatorName() const { return m_operator; };
	QString pricing() const { return m_pricing; };
	QString startStopName() const { return m_startStopName; };
	QString targetStopName() const { return m_targetStopName; };
	QString journeyNews() const { return m_journeyNews; };
	QList<VehicleType> vehicleTypes() const { return m_vehicleTypes; };
	QList<VehicleType> routeVehicleTypes() const { return m_routeVehicleTypes; };
	int duration() const { return m_duration; };
	int changes() const { return m_changes; };
	int routeExactStops() const { return m_routeExactStops; };
	QStringList routeStops() const { return m_routeStops; };
	QStringList routeTransportLines() const { return m_routeTransportLines; };
	QStringList routePlatformsDeparture() const { return m_routePlatformsDeparture; };
	QStringList routePlatformsArrival() const { return m_routePlatformsArrival; };
	QList<QTime> routeTimesDeparture() const { return m_routeTimesDeparture; };
	QList<QTime> routeTimesArrival() const { return m_routeTimesArrival; };
	QList<int> routeTimesDepartureDelay() const { return m_routeTimesDepartureDelay; };
	QList<int> routeTimesArrivalDelay() const { return m_routeTimesArrivalDelay; };
	QString hash() const { return m_hash; };
	
    private:
	void init( const QString &operatorName, const QList<VehicleType> &vehicleTypes,
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
	QList<VehicleType> m_vehicleTypes, m_routeVehicleTypes;
	int m_duration, m_changes, m_routeExactStops;
	QStringList m_routeStops, m_routeTransportLines,
		m_routePlatformsDeparture, m_routePlatformsArrival;
	QList<QTime> m_routeTimesDeparture, m_routeTimesArrival;
	QList<int> m_routeTimesDepartureDelay, m_routeTimesArrivalDelay;
	QString m_hash;
};

bool operator < ( const JourneyInfo &ji1, const JourneyInfo &ji2 );

/** @class DepartureInfo
*
* @brief Stores information about departures / arrivals. */
class DepartureInfo {
    public:
	/** Creates an invalid DepartureInfo object. */
	DepartureInfo() {
	};

	DepartureInfo( const QString &operatorName, const QString &line,
		       const QString &target, const QDateTime &departure,
		       VehicleType lineType,
		       const QStringList &routeStops = QStringList(),
		       const QList<QTime> &routeTimes = QList<QTime>(),
		       int routeExactStops = 0 ) {
	    init( operatorName, line, target, departure, lineType, NoLineService,
		  QString(), -1, QString(), QString(), routeStops, routeTimes,
		  routeExactStops );
	};

	DepartureInfo( const QString &operatorName, const QString &line,
		       const QString &target, const QDateTime &departure,
		       VehicleType lineType, bool nightLine, bool expressLine,
		       const QString &platform = QString(), int delay = -1,
		       const QString &delayReason = QString(),
		       const QString &journeyNews = QString(),
		       const QStringList &routeStops = QStringList(),
		       const QList<QTime> &routeTimes = QList<QTime>(),
		       int routeExactStops = 0 ) {
	    LineServices lineServices = NoLineService;
	    if ( nightLine )
		lineServices |= NightLine;
	    if ( expressLine )
		lineServices |= ExpressLine;
	    init( operatorName, line, target, departure, lineType, lineServices,
		  platform, delay, delayReason, journeyNews, routeStops,
		  routeTimes, routeExactStops );
	};

	DepartureInfo( const QString &operatorName, const QString &line,
		       const QString &target, const QDateTime &departure,
		       VehicleType lineType, LineServices lineServices,
		       const QString &platform = QString(), int delay = -1,
		       const QString &delayReason = QString(),
		       const QString &journeyNews = QString(),
		       const QStringList &routeStops = QStringList(),
		       const QList<QTime> &routeTimes = QList<QTime>(),
		       int routeExactStops = 0 ) {
	    init( operatorName, line, target, departure, lineType, lineServices,
		  platform, delay, delayReason, journeyNews, routeStops,
		  routeTimes, routeExactStops );
	};

	bool isVisible() const { return m_visible; };
	void setVisible( bool visible = true ) { m_visible = visible; };

	/** Wheather or not this DepartureInfo object is valid. It currently checks
	* validity by checking if the lineString is empty. */
	bool isValid() const { return !m_lineString.isEmpty(); };
	
	/** Gets the text to be displayed in the item for delay information. */
	QString delayText() const;

	/** Wheather or not the line number of this departure / arrival is valid. */
	bool isLineNumberValid() const {
	    return m_lineNumber > 0 &&
		    m_vehicleType != Unknown && // May not been parsed correctly
		    m_vehicleType < 10; // Isn't a train (bus | tram | subway | interurbantrain | metro | trolleybus). Line numbers are only valid for those. TODO: Change this?
	};

	/**< The line number. @see isLineNumberValid */
	int lineNumber() const { return m_lineNumber; };
	
	QString durationString( bool showDelay = true ) const;
	QString delayString() const;

	bool isNightLine() const { return m_lineServices.testFlag(NightLine); };
	bool isExpressLine() const { return m_lineServices.testFlag(ExpressLine); };
	DelayType delayType() const {
	    if ( m_delay < 0 )
		return DelayUnknown;
	    else if ( m_delay == 0 )
		return OnSchedule;
	    else
		return Delayed;
	};

	/** Gets the "real" departure time, which is the departure time from the
	* timetable plus the delay. */
	QDateTime predictedDeparture() const {
	    return delayType() == Delayed ? m_departure.addSecs(m_delay * 60) : m_departure;
	};

	/** Wheather or not the line number is in the specified range.
	* @return true, if the line number is in the specified range or if it is
	* greater than 999.
	* @return false, if the line number is not in the specified range.
	* @see isLineNumberValid()
	* @see lineNumber */
	bool isLineNumberInRange( int min, int max ) const {
	    return (m_lineNumber >= min && m_lineNumber <= max) || m_lineNumber >= 1000;
	};
	
	QString operatorName() const { return m_operator; };
	QString target() const { return m_target; };
	QString lineString() const { return m_lineString; };
	QString platform() const { return m_platform; };
	QString delayReason() const { return m_delayReason; };
	QString journeyNews() const { return m_journeyNews; };
	QDateTime departure() const { return m_departure; };
	/** The delay in minutes or -1 if there's no information about delays. */
	int delay() const { return m_delay; };
	VehicleType vehicleType() const { return m_vehicleType; };
	LineServices lineServices() const { return m_lineServices; };
	QStringList routeStops() const { return m_routeStops; };
	QList<QTime> routeTimes() const { return m_routeTimes; };
	int routeExactStops() const { return m_routeExactStops; };
	QString hash() const { return m_hash; };
	
    private:
	void init( const QString &operatorName, const QString &line,
		   const QString &target, const QDateTime &departure,
		   VehicleType lineType, LineServices lineServices = NoLineService,
		   const QString &platform = QString(), int delay = -1,
		   const QString &delayReason = QString(),
		   const QString &journeyNews = QString(),
		   const QStringList &routeStops = QStringList(),
		   const QList<QTime> &routeTimes = QList<QTime>(),
		   int routeExactStops = 0 );

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
	QString m_hash;
	bool m_visible;
};

bool operator < ( const DepartureInfo &di1, const DepartureInfo &di2 );

#endif // DEPARTUREINFO_HEADER
