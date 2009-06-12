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

#include <QTime>

// The type of the vehicle used for a public transport line.
enum LineType
{
    Unknown = 0,
    Tram = 1,
    Bus = 2,
    Subway = 3
};

// The type of services for a public transport line.
enum LineService
{
    NoLineService = 0,

    NightLine = 1,
    ExpressLine = 2
};
Q_DECLARE_FLAGS( LineServices, LineService );

// List of implemented service providers with IDs.
// If you implemented support for a new one, add a value here.
enum ServiceProvider
{
    NoServiceProvider = -1,

    // Germany (0 .. 50?)
    Fahrplaner = 0, // Niedersachsen/Bremen
    RMV = 1, // Rhein-Main
    VVS = 2, // Stuttgart
    VRN = 3, // Rhein-Neckar
    BVG = 4, // Berlin

    // Slovakia (1000 .. ?)
    IMHD = 1000 // Bratislava
};

class DepartureInfo
{
    public:
	DepartureInfo() { m_line = -1; };

	DepartureInfo( const QString &line, LineType lineType, QString direction, const QTime &departure )
	{
	    m_line = line;
	    m_lineType = lineType;
	    m_direction = direction;
	    m_departure = departure;
	    m_lineServices = NoLineService;
	}

	DepartureInfo( const QString &line, LineType lineType, QString direction, const QTime &departure, bool nightLine )
	{
	    m_line = line;
	    m_lineType = lineType;
	    m_direction = direction;
	    m_departure = departure;
	    m_lineServices = nightLine ? NightLine : NoLineService;
	}

	bool isValid() const { return m_line >= 0; }

	QString direction() const { return m_direction; };
	QString line() const { return m_line; };
	QTime departure() const { return m_departure; };
	LineType lineType() const { return m_lineType; };
	bool isNightLine() const { return m_lineServices.testFlag( NightLine ); };
	bool isExpressLine() const { return m_lineServices.testFlag( ExpressLine ); };

    private:
	QString m_direction, m_line;
	QTime m_departure;
	LineType m_lineType;
	LineServices m_lineServices;
};

#endif // DEPARTUREINFO_HEADER