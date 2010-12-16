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

#ifndef STOPFINDER_HEADER
#define STOPFINDER_HEADER

#include <QObject>
#include <QQueue>
#include <Plasma/DataEngine>

// TODO: Finish and use this
class StopSuggester : public QObject {
	Q_OBJECT

public:
	enum RunningRequestOptions {
		AbortRunningRequests,
		KeepRunningRequests
	};

	explicit StopSuggester( Plasma::DataEngine *publicTransportEngine, QObject* parent = 0 );

	void requestSuggestions( const QString &serviceProviderID, const QString &stopSubstring,
							 const QString &city = QString(),
							 RunningRequestOptions runningRequestOptions = AbortRunningRequests );
	bool isRunning() const { return !m_sourceNames.isEmpty(); };

signals:
	void stopSuggestionsReceived( const QStringList &stopSuggestions,
								  const QVariantHash &stopToStopID,
								  const QHash< QString, int > &stopToStopWeight );

protected slots:
	/** The data from the data engine was updated. */
	void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

private:
	Plasma::DataEngine *m_publicTransportEngine;
	QStringList m_sourceNames;
};

class StopFinder : public QObject {
	Q_OBJECT

public:
	enum Mode {
		StopNamesFromOSM, /**< Get stop names for stops near the current position from OpenStreetMap */
		ValidatedStopNamesFromOSM /**< Get first suggested stop names from publicTransport engine
				* for stop names (for stops near the current position) from OpenStreetMap */
	};

	enum Error {
		NoStopsFound,
		NoServiceProviderForCurrentCountry,
		OpenStreetMapDataEngineNotAvailable
	};

	enum DeletionPolicy {
		DeleteWhenFinished,
		KeepWhenFinished
	};

	StopFinder( Mode mode, Plasma::DataEngine *publicTransportEngine,
				Plasma::DataEngine *osmEngine, Plasma::DataEngine *geolocationEngine,
				int resultLimit = 25, DeletionPolicy deletionPolicy = DeleteWhenFinished,
				QObject* parent = 0 );

	Mode mode() const { return m_mode; };
	void start();

signals:
	void finished();
	void error( StopFinder::Error error, const QString &errorMessage );
	void stopsFound( const QStringList &stops, const QStringList &stopIDs,
					 const QString &serviceProviderID );
	void geolocationData( const QString &countryCode, const QString &city,
						  qreal latitude, qreal longitude, int accuracy );

protected slots:
	/** The data from the data engine was updated. */
	void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

private:
	bool validateNextStop();
	void processGeolocationData( const Plasma::DataEngine::Data &data );
	bool processOpenStreetMapData( const Plasma::DataEngine::Data &data );
	void processPublicTransportData( const Plasma::DataEngine::Data &data );

	Mode m_mode;
	DeletionPolicy m_deletionPolicy;
	Plasma::DataEngine *m_publicTransportEngine, *m_osmEngine, *m_geolocationEngine;

	QStringList m_foundStops, m_foundStopIDs;
	QQueue< QString > m_stopsToBeChecked;

	int m_resultLimit;
	bool m_osmFinished;
	QString m_countryCode;
	QString m_city;
	QString m_serviceProviderID;
	int m_accuracy;
};

#endif // Multiple inclusion guard
