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
	QDateTime departure, arrival;
	QString operatorName, pricing, startStopName, targetStopName, journeyNews;
	QList<VehicleType> vehicleTypes, routeVehicleTypes;
	int duration, changes, routeExactStops;
	QStringList routeStops, routeTransportLines,
		    routePlatformsDeparture, routePlatformsArrival;
	QList<QTime> routeTimesDeparture, routeTimesArrival;
	QList<int> routeTimesDepartureDelay, routeTimesArrivalDelay;

	JourneyInfo() {
	    duration = -1;
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

	bool isValid() const { return duration >= 0; };

	QList<QVariant> vehicleTypesVariant() const;
	QString durationToDepartureString( bool toArrival = false ) const;

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
};

bool operator < ( const JourneyInfo &ji1, const JourneyInfo &ji2 );

/** @class DepartureInfo
*
* @brief Stores information about departures / arrivals. */
class DepartureInfo {
    public:
	int lineNumber; /**< The line number. @see isLineNumberValid */
	QString operatorName, target, lineString, platform, delayReason, journeyNews;
	QDateTime departure;
	int delay; /**< The delay in minutes. */
	VehicleType vehicleType;
	LineServices lineServices;
	QStringList routeStops;
	QList<QTime> routeTimes;
	int routeExactStops;

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

	/** Wheather or not this DepartureInfo object is valid. It currently checks
	* validity by checking if the lineString is empty. */
	bool isValid() const { return !lineString.isEmpty(); };

	QString durationString() const;

	bool isNightLine() const { return lineServices.testFlag(NightLine); };
	bool isExpressLine() const { return lineServices.testFlag(ExpressLine); };
	DelayType delayType() const {
	    if ( delay < 0 )
		return DelayUnknown;
	    else if ( delay == 0 )
		return OnSchedule;
	    else
		return Delayed;
	};

	/** Gets the "real" departure time, which is the departure time from the
	* timetable plus the delay. */
	QDateTime predictedDeparture() const {
	    return delayType() == Delayed ? departure.addSecs(delay * 60) : departure;
	};

	/** Wheather or not the line number of this departure / arrival is valid. */
	bool isLineNumberValid() const {
	    return lineNumber > 0 &&
		vehicleType != Unknown && // May not been parsed correctly
		vehicleType < 10; // Isn't a train (bus | tram | subway | interurbantrain | metro | trolleybus). Line numbers are only valid for those.
	};

	/** Wheather or not the line number is in the specified range.
	* @return true, if the line number is in the specified range or if it is
	* greater than 999.
	* @return false, if the line number is not in the specified range.
	* @see isLineNumberValid()
	* @see lineNumber */
	bool isLineNumberInRange( int min, int max ) const {
	    return (lineNumber >= min && lineNumber <= max) || lineNumber >= 1000;
	};

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
};

bool operator < ( const DepartureInfo &di1, const DepartureInfo &di2 );

#endif // DEPARTUREINFO_HEADER
