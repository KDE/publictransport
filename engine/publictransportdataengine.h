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

#ifndef PUBLICTRANSPORTDATAENGINE_HEADER
#define PUBLICTRANSPORTDATAENGINE_HEADER

class QSizeF;

// Plasma includes
#include <Plasma/DataEngine>

// Own includes
#include "timetableaccessor.h"

/**
 * This engine provides departure times for public transport.
 */
class PublicTransportEngine : public Plasma::DataEngine
{
    Q_OBJECT

    public:
        // every engine needs a constructor with these arguments
        PublicTransportEngine( QObject* parent, const QVariantList& args );

	static const int UPDATE_TIMEOUT = 120; // Timeout to request new data

	static const int DEFAULT_MAXIMUM_DEPARTURES = 20;
	static const int ADDITIONAL_MAXIMUM_DEPARTURES = UPDATE_TIMEOUT / 20; // will be added to a given maximum departures value (otherwise the data couldn't provide enough departures until the update-timeout)

	static const int DEFAULT_TIME_OFFSET = 0;

    protected:
        // this virtual function is called when a new source is requested
        bool sourceRequestEvent( const QString& name );

        // this virtual function is called when an automatic update
        // is triggered for an existing source (ie: when a valid update
        // interval is set when requesting a source)
        bool updateSourceEvent( const QString& name );

	// Returns wheather or not the given source is up to date
	bool isSourceUpToDate( const QString& name );

    private:
	QMap<QString, QVariant> serviceProviderInfo( const TimetableAccessor *&accessor );

	QMap<ServiceProvider, TimetableAccessor *> m_accessors;

	// m_dataSources[ dataSource ][ key ] => QVariant-data
	QMap<QString, QVariant> m_dataSources;

    public slots:
	void journeyListReceived( TimetableAccessor *accessor, QList<DepartureInfo> journeys, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType );
	void stopListReceived( TimetableAccessor *accessor, QMap<QString, QString> stops, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType );
	void errorParsing( TimetableAccessor *accessor, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType );
};

#endif
