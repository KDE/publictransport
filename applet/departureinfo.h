/*
*   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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

#include <QString>
#include <QTime>
#include <QRegExp>
#include <KLocalizedString>

#include "enums.h"

Q_DECLARE_FLAGS( LineServices, LineService );

class DepartureInfo {
    public:
	int lineNumber;
	QString target, lineString, platform, delayReason, journeyNews;
	int delay; // in minutes
	QDateTime departure;
	VehicleType vehicleType;
	LineServices lineServices;

	DepartureInfo() {
	};

	DepartureInfo( const QString &line, const QString &target, const QDateTime &departure, VehicleType lineType ) {
	    init( line, target, departure, lineType );
	};

	DepartureInfo( const QString &line, const QString &target, const QDateTime &departure, VehicleType lineType, bool nightLine, bool expressLine, const QString &platform = "", int delay = -1, const QString &delayReason = "", const QString &journeyNews = "" ) {
	    LineServices lineServices = NoLineService;
	    if ( nightLine )
		lineServices |= NightLine;
	    if ( expressLine )
		lineServices |= ExpressLine;
	    init( line, target, departure, lineType, lineServices, platform, delay, delayReason, journeyNews );
	};

	DepartureInfo( const QString &line, const QString &target, const QDateTime &departure, VehicleType lineType, LineServices lineServices, const QString &platform = "", int delay = -1, const QString &delayReason = "", const QString &journeyNews = "" ) {
	    init( line, target, departure, lineType, lineServices, platform, delay, delayReason, journeyNews );
	};


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
	// Gets the "real" departure time, which is the departure time from the timetable plus the delay
	QDateTime predictedDeparture() const { return delayType() == Delayed ? departure.addSecs(delay * 60) : departure; };
	bool isLineNumberValid() const {
	    return lineNumber > 0 &&
	    vehicleType != Unknown && // May not been parsed correctly
	    vehicleType < 10; // Isn't a train (bus | tram | subway | interurbantrain)
	};

	bool isLineNumberInRange( int min, int max ) const {
	    return lineNumber >= min && lineNumber <= max;
	};

	// Used to sort a container of DepartureInfos by departure time
	static bool departureLessThan( const DepartureInfo &di1, const DepartureInfo &di2 ) {
	    return di1.predictedDeparture() < di2.predictedDeparture();
	};

    private:
	void init( const QString &line, const QString &target, const QDateTime &departure, VehicleType lineType, LineServices lineServices = NoLineService, const QString &platform = "", int delay = -1, const QString &delayReason = "", const QString &journeyNews = "" );
};

bool operator < ( const DepartureInfo &di1, const DepartureInfo &di2 );

#endif // DEPARTUREINFO_HEADER
