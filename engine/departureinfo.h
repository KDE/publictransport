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

// Qt includes
#include <QTime>
#include <QDebug>

// Own includes
#include "enums.h"

Q_DECLARE_FLAGS( LineServices, LineService );
class DepartureInfo
{
    public:
	DepartureInfo();

	DepartureInfo( const QString &line, const VehicleType &typeOfVehicle, const QString &target, const QDateTime &departure, bool nightLine = false, bool expressLine = false, const QString &platform = "", int delay = -1, const QString &delayReason = "", const QString &sJourneyNews = "" );

	DepartureInfo( const QString &line, const VehicleType &typeOfVehicle, const QString &target, const QTime &requestTime, const QTime &departureTime, bool nightLine = false, bool expressLine = false, const QString &platform = "", int delay = -1, const QString &delayReason = "", const QString &sJourneyNews = "" );

	void init( const QString &line, const VehicleType &typeOfVehicle, const QString &target, const QDateTime &departure, bool nightLine = false, bool expressLine = false, const QString &platform = "", int delay = -1, const QString &delayReason = "", const QString &sJourneyNews = "" );

	bool isValid() const { return !m_line.isEmpty(); };

	QString target() const { return m_target; };
	QString line() const { return m_line; };
	QDateTime departure() const { return m_departure; };
	VehicleType vehicleType() const { return m_vehicleType; };
	bool isNightLine() const { return m_lineServices.testFlag( NightLine ); };
	bool isExpressLine() const { return m_lineServices.testFlag( ExpressLine ); };
	QString platform() const { return m_platform; };
	int delay() const { return m_delay; };
	QString delayReason() const { return m_delayReason; };
	QString journeyNews() const { return m_journeyNews; };

	static VehicleType getVehicleTypeFromString( const QString &sLineType );

    private:
	QString m_target, m_line, m_platform, m_delayReason, m_journeyNews;
	int m_delay;
	QDateTime m_departure;
	VehicleType m_vehicleType;
	LineServices m_lineServices;
};

#endif // DEPARTUREINFO_HEADER