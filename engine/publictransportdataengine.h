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

/** @file
* @brief This file contains the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

/** @mainpage Public Transport Data Engine
@section intro_sec Introduction
The public transport data engine is used to get timetable data of public transport.
It can get departure / arrival lists or journey lists.
There are different accessors used to download and parse documents from
different service providers.

@section install_sec Installation
To install this data engine type the following commands:<br>
  \> cd /path-to-extracted-engine-sources/build<br>
  \> cmake -DCMAKE_INSTALL_PREFIX=`kde4-config --prefix` ..<br>
  \> make<br>
  \> make install<br>
<br>
After installation do the following to use the data engine in your plasma desktop:
Restart plasma to load the data engine:<br>
  \> kquitapp plasma-desktop<br>
  \> plasma-desktop<br>
<br>
or test it with:<br>
  \> plasmaengineexplorer<br>
<br>
You might need to run kbuildsycoca4 in order to get the .desktop file recognized.

@section usage_sec Usage
To use this data engine in an applet you need to connect it to a data source
of the public transport data engine. There is one default data source which
provides information about the available service providers.

@subsection usage_serviceproviders_sec Data Source "ServiceProviders"
This data source is called "ServiceProviders", you can see it in the
plasmaengineexplorer. For each available service provider it contains a key
with the display name of the service provider. These keys point to the service
provider information, stored as a QMap with the following keys:<br>
"id" (int), "name" (QString), "shortUrl" (QString), "country" (QString), "cities"
(QStringList), "useSeperateCityValue" (bool), "onlyUseCitiesInList" (bool),
"features" (QStringList), "url" (QString), "author" (QString), "email" (QString),
"description" (QString), "version" (QString).<br><br>
Here is an example of how to get service provider information for all available
service providers:
@code
Plasma::DataEngine::Data data = dataEngine("publictransport")->query("ServiceProviders");
foreach( QString serviceProviderName, data.keys() )
{
    QMap<QString, QVariant> serviceProviderData = data.value(serviceProviderName).toMap();
    int id = serviceProviderData["id"].toInt();
    QString name = serviceProviderData["name"].toString(); // The name is already in serviceProviderName
    QString country = serviceProviderData["country"].toString();
    QStringList features = serviceProviderData["features"].toStringList();
    bool useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
}
@endcode

@subsection usage_departures_sec Data Source "Departures", "Arrivals"
To get a list of departures / arrivals you need to construct the name of the
data source. For departures it begins with "Departures", for arrivals it begins
with "Arrivals". Next comes a space (" "), then the ID of the service provider
to use, e.g. 1 for db.de, a service provider for germany ("Deutsche Bahn").
The following parameters are seperated by ":" and start with the parameter name.
The sorting of the additional parameters doesn't matter. The parameter "stop"
is needed and can be the stop name or the stop ID. If the service provider has
useSeperateCityValue set to true (see @ref usage_serviceproviders_sec),
the parameter "city" is also needed (otherwise it is ignored). The following
additional parameters are allowed: "maxDeps" (the maximum departure / arrival
count to get), "timeOffset" (the offset in minutes from now for the first
departure / arrival to get).<br><br>
<b>Examples:</b><br>
<b>"Departures 1:stopPappelstraße, Bremen"</b> gets departures for the stop
"Pappelstraße, Bremen" using the service provider db.de.<br>
<b>"Arrivals 1:stopLeipzig:timeOffset5:maxDeps99"</b> gets arrivals for the stop
"Leipzig" using db.de, the first possible arrival is in 5 minutes from now, the
maximum arrival count is 99.<br><br>
Once you have the data source name, you can connect your applet to that
data source from the data engine. Here is an example on how to do this:
@code
class Applet : public Plasma::Applet {
    public:
	Applet(QObject *parent, const QVariantList &args) : AppletWithState(parent, args) {
	    dataEngine("publictransport")->connectSource( "Departures 1:stopPappelstraße, Bremen", this, 60 * 1000 );
	};

    public slots:
	void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data ) {
	    if ( data.value("error").toBool() ) {
		// Handle errors
	    } else if ( data.value("receivedPossibleStopList").toBool() ) {
		// Possible stop list received, because the given stop name is ambigous
		// See section "Receiving Stop Lists"
	    } else {
		// Departures / arrivals received.
		int count = data["count"].toInt(); // The number of received departures / arrivals
		for (int i = 0; i < count; ++i) {
		    QMap<QString, QVariant> departureData = data.value( QString("%1").arg(i) ).toMap();
		    QString line = dataMap["line"].toString();
		    QString target = dataMap["target"].toString(); // For arrival lists this is the origin
		    QDateTime departure = dataMap["departure"].toDateTime();
		}
	    }
	};
};
@endcode
The data map received from the data engine always contains the keys "error"
(true, if an error occured while parsing), "receivedPossibleStopList" (true, if
the given stop name is ambigous and a list of possible stops was received),
"count" (the number of received departures / arrivals / stops or 0 if there was
an error), "receivedData" ("departures", "journeys", "stopList" or "nothing" if
there was an error), "updated" (a QDateTime object containing the date and time
when the data source was last updated).

@subsubsection usage_stopList_sec Receiving Stop Lists
First, you should check, if a stop list was received by checking the value of key
"receivedPossibleStopList" in the data object from the data engine. Then you
get the number of stops received, which is stored in the key "count". For each
received stop there is a key "stopName i", where i is the number of the stop,
beginning with 0, ending with count - 1. In these keys, there are QMaps with
two keys: "stopName" (containing the stop name) and "stopID" (<b>can</b>
contain the stop ID, if it was received, otherwise it is empty).
@code
void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data ) {
    if ( data.value("receivedPossibleStopList").toBool() ) {
	QStringList possibleStops;
	int count = data["count"].toInt();
	for( int i = 0; i < count; ++i ) {
	QMap<QString, QVariant> stopData = data.value( QString("stopName %1").arg(i) ).toMap();

	QString stopName = dataMap["stopName"].toString();
	QString stopID = dataMap["stopID"].toString();
	}
    }
}
@endcode
*/

#ifndef PUBLICTRANSPORTDATAENGINE_HEADER
#define PUBLICTRANSPORTDATAENGINE_HEADER

class QSizeF;

// Plasma includes
#include <Plasma/DataEngine>

// Own includes
#include "timetableaccessor.h"

/** @class PublicTransportEngine
 @brief This engine provides departure / arrival times and journeys for public transport.
 @see @ref usage_sec (how to use this data engine in an applet?)
 */
class PublicTransportEngine : public Plasma::DataEngine
{
    Q_OBJECT

    public:
        /** Every data engine needs a constructor with these arguments. */
        PublicTransportEngine( QObject* parent, const QVariantList& args );

	/** Timeout in seconds to request new data. Before the timeout is over, old
	* stored data from previous requests is used. */
	static const int UPDATE_TIMEOUT = 120; //

	/** The default number of maximum departures. This is used if it wasn't
	* specified in the source name. */
	static const int DEFAULT_MAXIMUM_DEPARTURES = 20;

	/** Will be added to a given maximum departures value (otherwise the data
	* couldn't provide enough departures until the update-timeout) */
	static const int ADDITIONAL_MAXIMUM_DEPARTURES = UPDATE_TIMEOUT / 20;

	/** The default time offset from now for the first departure / arrival / journey
	* in the list. This is used if it wasn't specified in the source name. */
	static const int DEFAULT_TIME_OFFSET = 0;

    protected:
        /** This virtual function is called when a new source is requested.
        * @param name The name of the requested data source. */
        bool sourceRequestEvent( const QString& name );

        /**  This virtual function is called when an automatic update is
	* triggered for an existing source (ie: when a valid update interval is set
	* when requesting a source).
	* @param name The name of the data source to be updated. */
        bool updateSourceEvent( const QString& name );

	/** Returns wheather or not the given source is up to date.
	* @param name The name of the source to be checked. */
	bool isSourceUpToDate( const QString& name );

    public slots:
	/** A list of departures / arrivals was received.
	* @param accessor The accessor that was used to download and parse the
	* departures / arrivals.
	* @param departures A list of departures / arrivals that were received.
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source for which the departures /
	* arrivals have been downloaded and parsed.
	* @param city The city the stop is in. May be empty if the service provider
	* doesn't need a seperate city value.
	* @param stop The stop name for which the departures / arrivals have been received.
	* @param dataType "departures" or "arrivals".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeperateCityValue() */
	void departureListReceived( TimetableAccessor *accessor, QList<DepartureInfo*> departures, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode );

	/** A list of journey was received.
	* @param accessor The accessor that was used to download and parse the journeys.
	* @param journeys A list of journeys that were received.
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source for which the journeys have
	* been downloaded and parsed.
	* @param city The city the stop is in. May be empty if the service provider
	* doesn't need a seperate city value.
	* @param stop The stop name for which the departures / arrivals have been received.
	* @param dataType "journeysTo" or "journeysFrom".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeperateCityValue() */
	void journeyListReceived( TimetableAccessor *accessor, QList<JourneyInfo*> journeys, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode );

	/** A list of stops was received.
	* @param accessor The accessor that was used to download and parse the stops.
	* @param stops A QMap containing the received stop names as keys and the stop
	* IDs as values (stop IDs may be empty).
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source for which the stops have been
	* downloaded and parsed.
	* @param city The city the (ambigous) stop is in. May be empty if the service provider
	* doesn't need a seperate city value.
	* @param stop The (ambigous) stop name for which the stop list has been received.
	* @param dataType "stopList".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeperateCityValue() */
	void stopListReceived( TimetableAccessor *accessor, QMap<QString, QString> stops, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode );

	/** An error was received.
	* @param accessor The accessor that was used to download and parse information
	* from the service provider.
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source.
	* @param city The city the stop is in. May be empty if the service provider
	* doesn't need a seperate city value.
	* @param stop The stop name for which the error occured.
	* @param dataType "nothing".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeperateCityValue() */
	void errorParsing( TimetableAccessor *accessor, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode );

    private:
	/** Gets a map with information about an accessor.
	* @param accessor The accessor to get information about. */
	QMap<QString, QVariant> serviceProviderInfo( const TimetableAccessor *&accessor );

	QMap<ServiceProvider, TimetableAccessor *> m_accessors;

	// m_dataSources[ dataSource ][ key ] => QVariant-data
	QMap<QString, QVariant> m_dataSources;
};

#endif
