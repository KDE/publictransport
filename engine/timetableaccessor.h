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

#ifndef TIMETABLEACCESSOR_HEADER
#define TIMETABLEACCESSOR_HEADER

#include <KUrl>
#include <QMap>
#include <QTime>
#include "kio/scheduler.h"
#include "kio/jobclasses.h"
#include <QDebug>
#include "departureinfo.h"

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
    DVB = 5, // Dresden
    NASA = 6, // Sachsen-Anhalt
    DB = 7, // (ganz Deutschland?)

    SBB = 10, // (ganze Schweiz?)

    // Slovakia (1000 .. ?)
    IMHD = 1000 // Bratislava
};

enum TimetableInformation
{
    Nothing = 0,
    DepartureHour = 1,
    DepartureMinute = 2,
    TypeOfVehicle = 3,
    TransportLine = 4,
    Direction = 5
};

// Gets timetable information for public transport from different service providers.
// To implement support for a new service provider create a new class based on
// TimetableAccessor and overwrite
//	- serviceProvider()  (you also need to add an enum value to ServiceProvider)
// 	- country(), cities()
// 	- rawUrl(), parseDocument()
class TimetableAccessor : public QObject
{
    Q_OBJECT
    
    public:
	TimetableAccessor();
        ~TimetableAccessor();

	// Gets a timetable accessor that is able to parse results from the given service provider
	static TimetableAccessor *getSpecificAccessor( ServiceProvider serviceProvider );

	// The service provider the accessor is designed for
	virtual ServiceProvider serviceProvider() { return NoServiceProvider; };

	// The country for which the accessor returns results
	virtual QString country() const { return ""; };

	// A list of cities for which the accessor returns results
	virtual QStringList cities() const { return QStringList(); };

	// Requests a list of departures
	// When the departure list is received journeyListReceived() is emitted
	KIO::TransferJob *requestJourneys( QString city, QString stop );

	// deprecated ;)
	QList< DepartureInfo > getJourneys( QString city, QString stop );

    protected:
	// Stores the downloaded parts of a reply
	QByteArray m_document;
	QString m_curCity;

	// Parses the contents of a document that was requested using requestJourneys()
	virtual QList<DepartureInfo> parseDocument();
	
	// Gets the "raw" url with placeholders for the city ("%1") and the stop ("%2") 
	// or only for the stop ("%1") if putCityIntoUrl() == false
	virtual QString rawUrl();

	// Constructs an url by combining the "raw" url with the needed information
	KUrl getUrl( QString city, QString stop );

	// Wheather or not the city should be put into the "raw" url
	virtual bool putCityIntoUrl() const { return true; };

    private:
	// Stores information about currently running download jobs
	QMap<KJob*, QStringList> m_jobInfos;

    signals:
	// Emitted when a new journey list has been received and parsed
	void journeyListReceived( QList< DepartureInfo > journeys, ServiceProvider serviceProvider, QString city, QString stop );

    public slots:
	// Some data has been received
	void dataReceived( KIO::Job *job, const QByteArray &data );

	// All data of a journey list has been received
	void finished( KJob* job );
};

#endif // TIMETABLEACCESSOR_HEADER