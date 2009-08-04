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
@section index_sec Content
<ol>
<li>@ref intro_sec </li>
<li>@ref install_sec </li>
<li>@ref usage_sec
<ul>
    <li>@ref usage_serviceproviders_sec </li>
    <li>@ref usage_departures_sec </li>
    <li>@ref usage_journeys_sec </li>
    <li>@ref usage_stopList_sec </li>
</ul></li>
</ol>

<br>
@section intro_sec Introduction
The public transport data engine is used to get timetable data of public transport.
It can get departure / arrival lists or journey lists.
There are different accessors used to download and parse documents from
different service providers.

<br>
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

<br>
@section usage_sec Usage
To use this data engine in an applet you need to connect it to a data source
of the public transport data engine. There is one default data source which
provides information about the available service providers
(@ref usage_serviceproviders_sec). Another data source contains departures or
arrivals (@ref usage_departures_sec) and one contains journeys
(@ref usage_journeys_sec).<br>
<br>
This enumeration can be used in your applet to ease the usage of the data engine.
Don't change the numbers, as they need to match the ones in the data engine,
which uses a similiar enumeration.
@code
// The type of the vehicle used for a public transport line.
// The numbers here must match the ones in the data engine!
enum VehicleType {
    Unknown = 0, // The vehicle type is unknown
    Tram = 1, // The vehicle is a tram
    Bus = 2, // The vehicle is a bus
    Subway = 3, // The vehicle is a subway
    TrainInterurban = 4, // The vehicle is an interurban train

    TrainRegional = 10, // The vehicle is a regional train
    TrainRegionalExpress = 11, // The vehicle is a region express
    TrainInterregio = 12, // The vehicle is an interregional train
    TrainIntercityEurocity = 13, // The vehicle is a intercity / eurocity train
    TrainIntercityExpress = 14 // The vehicle is an intercity express (ICE, TGV?, ...?)
};
@endcode

<br>
@subsection usage_serviceproviders_sec Receiving a List of Available Service Providers
You can view this data source in the plasmaengineexplorer, it's name is "ServiceProviders".
For each available service provider it contains a key
with the display name of the service provider. These keys point to the service
provider information, stored as a QHash with the following keys:<br>
<table>
<tr><td><i>id</i></td> <td>QString</td> <td>The ID of the service provider.</td></tr>
<tr><td><i>fileName</i></td> <td>QString</td> <td>The file name of the XML file that was parsed to get the
accessor information.</td> </tr>
<tr><td><i>name</i></td> <td>QString</td> <td>The name of the accessor.</td></tr>
<tr><td><i>url</i></td> <td>QString</td> <td>The url to the home page of the service provider.</td></tr>
<tr><td><i>shortUrl</i></td> <td>QString</td> <td>A short version of the url to the home page of the service provider. This can be used to display short links, while using "url" as the url of that link.</td></tr>
<tr><td><i>country</i></td> <td>QString</td> <td>The country the service provider is (mainly) designed for.</td></tr>
<tr><td><i>cities</i></td> <td>QStringList</td> <td>A list of cities the service provider supports.</td></tr>
<tr><td><i>useSeperateCityValue</i></td> <td>bool</td> <td>Wheather or not the service provider needs a seperate city value. If this is true, you need to specify a "city" parameter in data source names for @ref usage_departures_sec and @ref usage_journeys_sec .</td></tr>
<tr><td><i>onlyUseCitiesInList</i></td> <td>bool</td> <td>Wheather or not the service provider only accepts cities that are in the "cities" list.</td></tr>
<tr><td><i>features</i></td> <td>QStringList</td> <td>A list of strings describing which features the accessor has.</td></tr>
<tr><td><i>author</i></td> <td>QString</td> <td>The author of the accessor.</td></tr>
<tr><td><i>email</i></td> <td>QString</td> <td>The email address of the author of the accessor.</td></tr>
<tr><td><i>description</i></td> <td>QString</td> <td>A description of the accessor.</td></tr>
<tr><td><i>version</i></td> <td>QString</td> <td>The version of the accessor.</td></tr>
</table>
<br>
Here is an example of how to get service provider information for all available
service providers:
@code
Plasma::DataEngine::Data data = dataEngine("publictransport")->query("ServiceProviders");
foreach( QString serviceProviderName, data.keys() )
{
    QHash<QString, QVariant> serviceProviderData = data.value(serviceProviderName).toHash();
    int id = serviceProviderData["id"].toInt();
    QString name = serviceProviderData["name"].toString(); // The name is already in serviceProviderName
    QString country = serviceProviderData["country"].toString();
    QStringList features = serviceProviderData["features"].toStringList();
    bool useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
}
@endcode

<br>
@subsection usage_departures_sec Receiving Departures or Arrivals
To get a list of departures / arrivals you need to construct the name of the
data source. For departures it begins with "Departures", for arrivals it begins
with "Arrivals". Next comes a space (" "), then the ID of the service provider
to use, e.g. "de_db" for db.de, a service provider for germany ("Deutsche Bahn").
The following parameters are seperated by "|" and start with the parameter name
followed by "=". The sorting of the additional parameters doesn't matter. The
parameter <i>stop</i> is needed and can be the stop name or the stop ID. If the
service provider has useSeperateCityValue set to true (see
@ref usage_serviceproviders_sec), the parameter <i>city</i> is also needed
(otherwise it is ignored).<br>
The following parameters are allowed:<br>
<table>
<tr><td><i>stop</i></td> <td>The name or ID of the stop to get departures / arrivals for.</td></tr>
<tr><td><i>city</i></td> <td>The city to get departures / arrivals for, if needed.</td></tr>
<tr><td><i>maxDeps</i></td> <td>The maximum departure / arrival count to get.</td></tr>
<tr><td><i>timeOffset</i></td> <td>The offset in minutes from now for the first departure /
arrival to get.</td></tr>
<tr><td><i>time</i></td> <td>The time for the first departure / arrival to get ("hh:mm").</td></tr>
</table>
<br>

<b>Examples:</b><br>
<b>"Departures de_db|stop=Pappelstraße, Bremen"</b><br>
Gets departures for the stop "Pappelstraße, Bremen" using the service provider db.de.<br><br>

<b>"Arrivals de_db|stop=Leipzig|timeOffset=5|maxDeps=99"</b><br>
Gets arrivals for the stop "Leipzig" using db.de, the first possible arrival is in five minutes from now, the
maximum arrival count is 99.<br><br>

<b>"Departures de_rmv|stop=Frankfurt (Main) Speyerer Straße|time=08:00"</b><br>
Gets departures for the stop "Frankfurt (Main) Speyerer Straße" using rmv.de, the first possible departure is at eight o'clock.<br><br>

<b>"Departures de_rmv|stop=3000019|maxDeps=20|timeOffset=1"</b><br>
Gets departures for the stop with the ID "3000019", the first possible departure is in one minute from now, the maximum departure count is 20.<br><br>

Once you have the data source name, you can connect your applet to that
data source from the data engine. Here is an example of how to do this:
@code
class Applet : public Plasma::Applet {
    public:
	Applet(QObject *parent, const QVariantList &args) : AppletWithState(parent, args) {
	    dataEngine("publictransport")->connectSource( "Departures de_db|stop=Pappelstraße, Bremen", this, 60 * 1000 );
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
		    QHash<QString, QVariant> departureData = data.value( QString("%1").arg(i) ).toHash();
		    QString line = departureData["line"].toString();
		    QString target = departureData["target"].toString(); // For arrival lists this is the origin
		    QDateTime departure = departureData["departure"].toDateTime();
		}
	    }
	};
};
@endcode
<br>
The data received from the data engine always contains these keys:<br>
<table>
<tr><td><i>error</i></td> <td>bool</td> <td>True, if an error occured while parsing.</td></tr>
<tr><td><i>receivedPossibleStopList</i></td> <td>bool</td> <td>True, if the given stop name is ambigous and
a list of possible stops was received, see @ref usage_stopList_sec .</td></tr>
<tr><td><i>count</i></td> <td>int</td> <td>The number of received departures / arrivals / stops or 0 if there was
an error.</td></tr>
<tr><td><i>receivedData</i></td> <td>QString</td> <td>"departures", "journeys", "stopList" or "nothing" if
there was an error.</td></tr>
<tr><td><i>updated</i></td> <td>QDateTime</td> <td>The date and time when the data source was last updated.</td></tr>
</table>
<br>
Each departure / arrival in the data received from the data engine (departureData in the code
example) has the following keys:<br>
<table>
<tr><td><i>line</i></td> <td>QString</td> <td>The name of the public transport line, e.g. "S1", "6", "1E", "RB 24155".</td></tr>
<tr><td><i>target</i></td> <td>QString</td> <td>The name of the target / origin of the public transport line.</td></tr>
<tr><td><i>departure</i></td> <td>QDateTime</td> <td>The date and time of the departure / arrival.</td></tr>
<tr><td><i>vehicleType</i></td> <td>int</td> <td>An int containing the ID of the vehicle type used for the
departure / arrival. You can cast the ID to VehicleType using "static_cast<VehicleType>( iVehicleType )".</td></tr>
<tr><td><i>nightline</i></td> <td>bool</td> <td>Wheather or not the public transport line is a night line.</td></tr>
<tr><td><i>expressline</i></td> <td>bool</td> <td>Wheather or not the public transport line is an express line.</td></tr>
<tr><td><i>platform</i></td> <td>QString</td> <td>The platform from/at which the vehicle departs/arrives.</td></tr>
<tr><td><i>delay</i></td> <td>int</td> <td>The delay in minutes, 0 means 'on schedule', -1 means 'no delay information available'.</td></tr>
<tr><td><i>delayReason</i></td> <td>QString</td> <td>The reason of a delay.</td></tr>
<tr><td><i>journeyNews</i></td> <td>QString</td> <td>News for the journey.</td></tr>
<tr><td><i>operator</i></td> <td>QString</td> <td>The company that is responsible for the journey.</td></tr>
</table>

<br>
@subsection usage_journeys_sec Receiving Journeys from A to B
To get a list of journeys from one stop to antoher you need to construct the
name of the data source (much like the data source for departures / arrivals).
The data source name begins with "Journeys". Next comes a space (" "), then
the ID of the service provider to use, e.g. "de_db" for db.de, a service
provider for germany ("Deutsche Bahn").
The following parameters are seperated by "|" and start with the parameter name
followed by "=". The sorting of the additional parameters doesn't matter. The
parameters <i>originStop</i> and <i>targetStop</i> are needed and can be the
stop names or the stop IDs. If the service provider has useSeperateCityValue
set to true (see @ref usage_serviceproviders_sec), the parameter <i>city</i> is
also needed (otherwise it is ignored).<br>
The following parameters are allowed:<br>
<table>
<tr><td><i>originStop</i></td> <td>The name or ID of the origin stop.</td></tr>
<tr><td><i>targetStop</i></td> <td>The name or ID of the target stop.</td></tr>
<tr><td><i>city</i></td> <td>The city to get journeys for, if needed.</td></tr>
<tr><td><i>maxDeps</i></td> <td>The maximum journey count to get.</td></tr>
<tr><td><i>timeOffset</i></td> <td>The offset in minutes from now for the first journey to get.</td></tr>
<tr><td><i>time</i></td> <td>The time for the first journey to get (in format "hh:mm").</td></tr>
</table>
<br>

<b>Examples:</b><br>
<b>"Journeys de_db|originStop=Pappelstraße, Bremen|targetStop=Kirchweg, Bremen"</b><br>
Gets journeys from stop "Pappelstraße, Bremen" to stop "Kirchweg, Bremen"
using the service provider db.de.<br><br>

<b>"Journeys de_db|originStop=Leipzig|targetStop=Hannover|timeOffset=5|maxDeps=99"</b><br>
Gets journeys from stop "Leipzig" to stop "Hannover" using db.de, the first
possible journey departs in five minutes from now, the maximum journey count is 99.<br><br>

Once you have the data source name, you can connect your applet to that
data source from the data engine. Here is an example of how to do this:
@code
class Applet : public Plasma::Applet {
   public:
       Applet(QObject *parent, const QVariantList &args) : AppletWithState(parent, args) {
	    dataEngine("publictransport")->connectSource(
		"Journeys de_db|originStop=Pappelstraße, Bremen|targetStop=Kirchweg, Bremen", this, 60 * 1000 );
	};

   public slots:
       void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data ) {
	    if ( data.value("error").toBool() ) {
		// Handle errors
	    } else if ( data.value("receivedPossibleStopList").toBool() ) {
		// Possible stop list received, because the given stop name is ambigous
		// See section "Receiving Stop Lists"
	    } else {
		// Journeys received.
		int count = data["count"].toInt(); // The number of received journeys
		for (int i = 0; i < count; ++i) {
		    QHash<QString, QVariant> journeyData = data.value( QString("%1").arg(i) ).toHash();

		    // Get vehicle type list
		    QVariantList = journeyData["vehicleTypes"].toList();
		    QList<VehicleType> vehicleTypes;
		    foreach( QVariant vehicleType, vehicleTypesVariant )
			vehicleTypes.append( static_cast<VehicleType>(vehicleType.toInt()) );

		    QString target = journeyData["startStopName"].toString();
		    QDateTime departure = journeyData["departure"].toDateTime();
		    int duration = journeyData["duration"].toInt(); // Duration in minutes
		}
	    }
	};
};
@endcode
<br>
The data received from the data engine always contains these keys:<br>
<table>
<tr><td><i>error</i></td> <td>bool</td> <td>True, if an error occured while parsing.</td></tr>
<tr><td><i>receivedPossibleStopList</i></td> <td>bool</td> <td>True, if the given stop name is ambigous and
a list of possible stops was received, see @ref usage_stopList_sec .</td></tr>
<tr><td><i>count</i></td> <td>int</td> <td>The number of received journeys / stops or 0 if there was
an error.</td></tr>
<tr><td><i>receivedData</i></td> <td>QString</td> <td>"departures", "journeys", "stopList" or "nothing" if
there was an error.</td></tr>
<tr><td><i>updated</i></td> <td>QDateTime</td> <td>The date and time when the data source was last updated.</td></tr>
</table>
<br>
Each journey in the data received from the data engine (journeyData in the code
example) has the following keys:<br>
<i>vehicleTypes</i>: A QVariantList containing a list of vehicle types used in the journey. You can cast the list to QList<VehicleType> as seen in the code example (QVariantList).<br>
<table>
<tr><td><i>arrival</i></td> <td>QDateTime</td> <td>The date and time of the arrival at the target stop.</td></tr>
<tr><td><i>departure</i></td> <td>QDateTime</td> <td>The date and time of the departure from the origin stop.</td></tr>
<tr><td><i>duration</i></td> <td>int</td> <td>The duration in minutes of the journey.</td></tr>
<tr><td><i>changes</i></td> <td>int</td> <td>The changes between vehicles needed for the journey.</td></tr>
<tr><td><i>pricing</i></td> <td>QString</td> <td>Information about the pricing of the journey.</td></tr>
<tr><td><i>journeyNews</i></td> <td>QString</td> <td>News for the journey.</td></tr>
<tr><td><i>startStopName</i></td> <td>QString</td> <td>The name or ID of the origin stop.</td></tr>

<tr><td><i>targetStopName</i></td> <td>QString</td> <td>The name or ID of the target stop (QString).</td></tr>
<tr><td><i>operator</i></td> <td>QString</td> <td>The company that is responsible for the journey.</td></tr>
</table>

<br>
@subsection usage_stopList_sec Receiving Stop Lists
When you have requested departures, arrivals or journeys from the data engine,
it may return a list of stops, if the given stop name is ambigous.
First, you should check, if a stop list was received by checking the value of key
"receivedPossibleStopList" in the data object from the data engine. Then you
get the number of stops received, which is stored in the key "count". For each
received stop there is a key "stopName i", where i is the number of the stop,
beginning with 0, ending with count - 1. In these keys, there are QHash's with
two keys: <i>stopName</i> (containing the stop name) and <i>stopID</i> (<b>may</b>
contain the stop ID, if it was received, otherwise it is empty).
@code
void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data ) {
   if ( data.value("receivedPossibleStopList").toBool() ) {
	QStringList possibleStops;
	int count = data["count"].toInt();
	for( int i = 0; i < count; ++i ) {
	    QHash<QString, QVariant> stopData = data.value( QString("stopName %1").arg(i) ).toHash();

	    QString stopName = dataMap["stopName"].toString();
	    QString stopID = dataMap["stopID"].toString();
	    }
	}
   }
   @endcode
*/

#ifndef PUBLICTRANSPORTDATAENGINE_HEADER
#define PUBLICTRANSPORTDATAENGINE_HEADER


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
	* couldn't provide enough departures until the update-timeout). */
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

	bool updateServiceProviderSource( const QString &name );
	void updateErrornousServiceProviderSource( const QString &name );
	void updateLocationSource( const QString &name );
	bool updateDepartureOrJourneySource( const QString &name );

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
	void departureListReceived( TimetableAccessor *accessor, QList<DepartureInfo*> departures, const QString &serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode );

	/** A list of journey was received.
	* @param accessor The accessor that was used to download and parse the journeys.
	* @param journeys A list of journeys that were received.
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source for which the journeys have
	* been downloaded and parsed.
	* @param city The city the stop is in. May be empty if the service provider
	* doesn't need a seperate city value.
	* @param stop The stop name for which the departures / arrivals have been received.
	* @param dataType "journeys".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeperateCityValue() */
	void journeyListReceived( TimetableAccessor *accessor, QList<JourneyInfo*> journeys, const QString &serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode );

	/** A list of stops was received.
	* @param accessor The accessor that was used to download and parse the stops.
	* @param stops A QHash containing the received stop names as keys and the stop
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
	void stopListReceived( TimetableAccessor *accessor, QHash<QString, QString> stops, const QString &serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode );

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
	void errorParsing( TimetableAccessor *accessor, const QString &serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode );

    private:
	/** Gets a map with information about an accessor.
	* @param accessor The accessor to get information about. */
	QHash<QString, QVariant> serviceProviderInfo( const TimetableAccessor *&accessor );
	QHash<QString, QVariant> locations();

	QHash<QString, TimetableAccessor *> m_accessors;
	QHash<QString, QVariant> m_dataSources;
	QStringList m_errornousAccessors;
	int m_lastStopNameCount, m_lastJourneyCount;
};

#endif
