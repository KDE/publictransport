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
#include "enums.h"
#include "timetableaccessor_htmlinfo.h"

// Gets timetable information for public transport from different service providers.
// The easiest way to implement support for a new service provider is to derive a new class
// from TimetableAccessorInfo and use it with TimetableAccessorHtml.
// To implement support for a new class service providers create a new class based on
// TimetableAccessor or it's derivates and overwrite
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
	virtual ServiceProvider serviceProvider() const { return m_info.serviceProvider; };

	virtual QStringList features() const;

	// The country for which the accessor returns results
	virtual QString country() const { return m_info.country; };

	// A list of cities for which the accessor returns results
	virtual QStringList cities() const { return m_info.cities; };
	// Requests a list of departures
	// When the departure list is received journeyListReceived() is emitted
	KIO::TransferJob *requestJourneys( const QString &sourceName, const QString &city, const QString &stop, int maxDeps, int timeOffset, const QString &dataType = "departures", bool useDifferentUrl = false );

	TimetableAccessorInfo timetableAccessorInfo() const;

	// Wheather or not the city should be put into the "raw" url
	virtual bool useSeperateCityValue() const { return m_info.useSeperateCityValue; };
	virtual bool useDifferentUrlForPossibleStops() const { return false; };

	static QString toPercentEncoding( QString str, QByteArray charset );

    protected:
	// Parses the contents of a document that was requested using requestJourneys()
	virtual bool parseDocument( QList<DepartureInfo> *journeys );

	// Parses the contents of a document for a list of stop names
	virtual bool parseDocumentPossibleStops( QMap<QString,QString> *stops ) const;

	// Gets the "raw" url with placeholders for the city ("%1") and the stop ("%2")
	// or only for the stop ("%1") if putCityIntoUrl() == false
	virtual QString rawUrl() const;
	virtual QString differentRawUrl() const;
	virtual QByteArray charsetForUrlEncoding() const;

	// Constructs an url by combining the "raw" url with the needed information
	KUrl getUrl( const QString &city, const QString &stop, int maxDeps, int timeOffset, const QString &dataType = "departures", bool useDifferentUrl = false ) const;

	// Stores the downloaded parts of a reply
	QByteArray m_document;
	QString m_curCity;

	// Stores service provider specific information that is used to parse the html pages
	TimetableAccessorInfo m_info;

    private:
	static QString gethex ( ushort decimal );

	// Stores information about currently running download jobs
	QMap<KJob*, QVariantList> m_jobInfos;

    signals:
	// Emitted when a new journey list has been received and parsed
	void journeyListReceived( TimetableAccessor *accessor, QList< DepartureInfo > journeys, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType );
	// Emitted when a list of stop names has been received and parsed
	void stopListReceived( TimetableAccessor *accessor, QMap< QString, QString > stops, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType );
	// Emitted when an error occured while parsing
	void errorParsing( TimetableAccessor *accessor, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType );

    public slots:
	// Some data has been received
	void dataReceived( KIO::Job *job, const QByteArray &data );

	// All data of a journey list has been received
	void finished( KJob* job );
};

#endif // TIMETABLEACCESSOR_HEADER