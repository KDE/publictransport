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

#include "stopfinder.h"

StopSuggester::StopSuggester( Plasma::DataEngine* publicTransportEngine,
			      QObject* parent) : QObject(parent),
			      m_publicTransportEngine(publicTransportEngine) {
}

void StopSuggester::requestSuggestions( const QString &serviceProviderID,
			const QString &stopSubstring, const QString &city,
			RunningRequestOptions runningRequestOptions ) {
    if ( runningRequestOptions == AbortRunningRequests ) {
	foreach ( const QString &sourceName, m_sourceNames )
	    m_publicTransportEngine->disconnectSource( sourceName, this );
	m_sourceNames.clear();
    }
    
    if ( !city.isEmpty() ) { // m_useSeparateCityValue ) {
	m_sourceNames << QString( "Stops %1|stop=%2|city=%3" )
		.arg( serviceProviderID, stopSubstring, city );
    } else {
	m_sourceNames << QString( "Stops %1|stop=%2" )
		.arg( serviceProviderID, stopSubstring );
    }
    m_publicTransportEngine->connectSource( m_sourceNames.last(), this );
}

void StopSuggester::dataUpdated( const QString& sourceName,
				 const Plasma::DataEngine::Data& data ) {
    if ( sourceName.startsWith(QLatin1String("Stops"), Qt::CaseInsensitive) ) {
	m_publicTransportEngine->disconnectSource( sourceName, this );
	if ( !m_sourceNames.removeOne(sourceName) ) {
	    kDebug() << "Source" << sourceName << "was aborted";
	    return;
	}
	
	QStringList stops;
	QVariantHash stopToStopID;
	QHash< QString, int > stopToStopWeight;
	int count = data["count"].toInt();
	for ( int i = 0; i < count; ++i ) {
	    QVariant stopData = data.value( QString("stopName %1").arg(i) );
	    if ( stopData.isValid() ) {
		QVariantHash stopHash = stopData.toHash();
		QString stop = stopHash["stopName"].toString();
		QString stopID = stopHash["stopID"].toString();
		int stopWeight = stopHash["stopWeight"].toInt();
		if ( stopWeight <= 0 )
		    stopWeight = 0;
// 		else
// 		    hasAtLeastOneWeight = true;
	    }
	}
	if ( !stops.isEmpty() ) {
	    emit stopSuggestionsReceived( stops, stopToStopID, stopToStopWeight );
	} else {
	    kDebug() << "nothing found";
	}
    }
}



StopFinder::StopFinder( StopFinder::Mode mode,
		Plasma::DataEngine* publicTransportEngine,
		Plasma::DataEngine* osmEngine, Plasma::DataEngine* geolocationEngine,
		int resultLimit, DeletionPolicy deletionPolicy, QObject* parent )
		: QObject(parent), m_mode(mode), m_deletionPolicy(deletionPolicy),
		m_publicTransportEngine(publicTransportEngine),
		m_osmEngine(osmEngine), m_geolocationEngine(geolocationEngine) {
    m_osmFinished = false;
    m_resultLimit = resultLimit;
}

void StopFinder::start() {
    m_geolocationEngine->connectSource( "location", this );
}

void StopFinder::dataUpdated( const QString& sourceName,
			      const Plasma::DataEngine::Data& data ) {
    if ( sourceName.startsWith(QLatin1String("Stops"), Qt::CaseInsensitive) ) {
	m_publicTransportEngine->disconnectSource( sourceName, this );
	processPublicTransportData( data );
    } else if ( sourceName == "location" ) {
	m_geolocationEngine->disconnectSource( sourceName, this );
	processGeolocationData( data );
    } else if ( sourceName.contains("publictransportstops") ) {
	bool finished = processOpenStreetMapData( data );
	if ( finished || (m_foundStops.count() + m_stopsToBeChecked.count())
		    >= m_resultLimit ) {
	    m_osmEngine->disconnectSource( sourceName, this );
	}
    }
}

void StopFinder::processPublicTransportData( const Plasma::DataEngine::Data& data ) {
    QString stop, stopID;
    int count = data["count"].toInt();
    for ( int i = 0; i < count; ++i ) {
	QVariant stopData = data.value( QString("stopName %1").arg(i) );
	if ( stopData.isValid() ) {
	    QHash< QString, QVariant > stopHash = stopData.toHash();
	    stop = stopHash["stopName"].toString();
	    stopID = stopHash["stopID"].toString();
	    break;
	}
    }
    if ( !stop.isEmpty() ) {
	m_foundStops << stop;
	m_foundStopIDs << stopID;
	
	emit stopsFound( QStringList() << stop, QStringList() << stopID,
			 m_serviceProviderID );
    } else {
	kDebug() << "nothing found";
    }

    if ( !validateNextStop() && m_osmFinished ) {
	kDebug() << "Last stop validated and OSM engine is finished."
		 << m_foundStops.count() << "stops found.";
// 	emit stopsFound( m_foundStops, m_foundStopIDs, m_serviceProviderID );
	emit finished();
	if ( m_deletionPolicy == DeleteWhenFinished )
	    deleteLater();
    }
}

void StopFinder::processGeolocationData( const Plasma::DataEngine::Data& data ) {
    m_countryCode = data["country code"].toString().toLower();
    m_city = data["city"].toString();
    qreal latitude = data["latitude"].toDouble();
    qreal longitude = data["longitude"].toDouble();
    m_accuracy = data["accuracy"].toInt();
    emit geolocationData( m_countryCode, m_city, latitude, longitude, m_accuracy );

    // Check if a service provider is available for the given country
    Plasma::DataEngine::Data dataProvider =
	    m_publicTransportEngine->query("ServiceProvider " + m_countryCode);
    if ( dataProvider.isEmpty() ) {
	QString errorMessage = i18nc( "@info", "There's no supported "
		"service provider for the country you're currently in (%1).\n"
		"You can try service providers for other countries, as some of "
		"them also provide data for adjacent countries.",
		KGlobal::locale()->countryCodeToName(m_countryCode) );
	kDebug() << "No service provider found for country" << m_countryCode;
	emit error( NoServiceProviderForCurrentCountry, errorMessage );
	emit finished();
	if ( m_deletionPolicy == DeleteWhenFinished )
	    deleteLater();
    } else {
	m_serviceProviderID = dataProvider["id"].toString();
	if ( m_osmEngine->isValid() ) {
	    // Get stop list near the user from the OpenStreetMap data engine.
	    // If the OSM engine isn't available, the city is used as stop name.
	    double areaSize = m_accuracy > 10000 ? 0.5 : 0.02;
	    QString sourceName = QString( "%1,%2 %3 publictransportstops" )
		    .arg( latitude ).arg( longitude ).arg( areaSize );
	    m_osmEngine->connectSource( sourceName, this );
	} else {
	    kDebug() << "OSM engine not available";
	    emit error( OpenStreetMapDataEngineNotAvailable,
			i18nc("@info", "OpenStreetMap data engine not available") );
	    emit finished();
	    if ( m_deletionPolicy == DeleteWhenFinished )
		deleteLater();
	}
    }
}

bool StopFinder::processOpenStreetMapData( const Plasma::DataEngine::Data& data ) {
    QStringList stops;
    Plasma::DataEngine::Data::const_iterator it = data.constBegin();
// 	it += m_nearStopsDialog->listView()->model()->rowCount(); // Don't readd already added stops
    for ( ; it != data.constEnd(); ++it ) {
	QHash< QString, QVariant > item = it.value().toHash();
	if ( item.contains("name") )
	    stops << item[ "name" ].toString();
    }
    stops.removeDuplicates();

    if ( m_mode == ValidatedStopNamesFromOSM ) {
	m_stopsToBeChecked.append( stops );
	validateNextStop();
    }
    
    if ( m_mode == StopNamesFromOSM && !stops.isEmpty() )
	emit stopsFound( stops, QStringList(), m_serviceProviderID );

    if ( data.contains("finished") && data["finished"].toBool() ) {
	m_osmFinished = true;

	if ( m_mode == StopNamesFromOSM ) {
	    if ( stops.isEmpty() ) {
		kDebug() << "No stops found by OSM for the given position";
		emit error( NoStopsFound, i18nc("@info", "No stops found by OpenStreetMap "
			"for the given position") );
	    }
	    emit finished();
	    if ( m_deletionPolicy == DeleteWhenFinished )
		deleteLater();
	} //else ( m_mode == ValidatedStopNamesFromOSM && m_s
    }

    return m_osmFinished;
}

bool StopFinder::validateNextStop() {
    if ( m_stopsToBeChecked.isEmpty() || m_foundStops.count() >= m_resultLimit ) {
	kDebug() << "No more stops to be checked in the queue or limit reached.";
	return false;
    }

    QString stop = m_stopsToBeChecked.dequeue();
    kDebug() << "Validate stop" << stop;
    if ( !m_city.isEmpty() ) { // m_useSeparateCityValue ) {
	m_publicTransportEngine->connectSource(
		QString( "Stops %1|stop=%2|city=%3" )
		.arg( m_serviceProviderID, stop, m_city ), this );
    } else {
	m_publicTransportEngine->connectSource(
		QString("Stops %1|stop=%2").arg(m_serviceProviderID, stop), this );
    }

    return true;
}
