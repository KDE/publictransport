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

/** @file
* @brief This file contains the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef PUBLICTRANSPORTDATAENGINE_HEADER
#define PUBLICTRANSPORTDATAENGINE_HEADER

// Plasma includes
#include <Plasma/DataEngine>

// Own includes
#include "enums.h"

class QFileSystemWatcher;
class TimetableAccessor;
class DepartureInfo;
class JourneyInfo;

/** @class PublicTransportEngine
 @brief This engine provides departure/arrival times and journeys for public transport.
 @see @ref usage_sec (how to use this data engine in an applet?)
 */
class PublicTransportEngine : public Plasma::DataEngine {
    Q_OBJECT

    public:
	/** The available types of sources of this data engine. They all have
	* an associated keyword with that data source names start.
	* @see sourceTypeKeyword
	* @see sourceTypeFromName */
	enum SourceType {
	    InvalidSourceName = 0, /**< Returned by @ref sourceTypeFromName, if
				      * the source name is invalid. */
	    
	    ServiceProvider = 1, /**< The source contains information about available
				    * service providers for a given country. */
	    ServiceProviders = 2, /**< The source contains information about available
				     * service providers. */
	    ErrornousServiceProviders = 3, /**< The source contains a list of errornous
				  	      * service provider accessors. */
	    Locations = 4, /**< The source contains information about locations
			      * for which accessors to service providers exist. */
	    
	    Departures = 10, /**< The source contains timetable data for departures. */
	    Arrivals, /**< The source contains timetable data for arrivals. */
	    Stops, /**< The source contains a list of stop suggestions. */
	    Journeys, /**< The source contains information about journeys. */
	    JourneysDep, /**< The source contains information about journeys,
			    * that depart at the given date and time. */
	    JourneysArr /**< The source contains information about journeys,
			   * that arrive at the given date and time. */
	};
	
        /** Every data engine needs a constructor with these arguments. */
        PublicTransportEngine( QObject* parent, const QVariantList& args );
	~PublicTransportEngine();

	static const QString sourceTypeKeyword( SourceType sourceType );

	SourceType sourceTypeFromName( const QString &sourceName ) const;
	bool isDataRequestingSourceType( SourceType sourceType ) const {
	    return static_cast< int >( sourceType ) >= 10; };

	/** Minimum timeout in seconds to request new data. Before the timeout 
	* is over, old stored data from previous requests is used. */
	static const int MIN_UPDATE_TIMEOUT;
	
	/** Maximum timeout in seconds to request new data, if delays are avaiable. 
	* Before the timeout is over, old stored data from previous requests is used. */
	static const int MAX_UPDATE_TIMEOUT_DELAY;

	/** The default time offset from now for the first departure / arrival / journey
	* in the list. This is used if it wasn't specified in the source name. */
	static const int DEFAULT_TIME_OFFSET;

    protected:
        /** This virtual function is called when a new source is requested.
        * @param name The name of the requested data source. */
        bool sourceRequestEvent( const QString& name );

        /** This virtual function is called when an automatic update is triggered 
        * for an existing source (ie: when a valid update interval is set
	* when requesting a source).
	* @param name The name of the data source to be updated. */
        bool updateSourceEvent( const QString& name );

	bool updateServiceProviderForCountrySource( const QString &name );
	bool updateServiceProviderSource();
	void updateErrornousServiceProviderSource( const QString &name );
	bool updateLocationSource();
	bool updateDepartureOrJourneySource( const QString &name );

	/** Returns wheather or not the given source is up to date.
	* @param name The name of the source to be checked. */
	bool isSourceUpToDate( const QString& name );

    public slots:
	/** A list of departures / arrivals was received.
	* @param accessor The accessor that was used to download and parse the
	* departures / arrivals.
	* @param requestUrl The url used to request the information.
	* @param departures A list of departures/arrivals that were received.
	* @param globalInfo Global information that affects all departures/arrivals.
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source for which the departures /
	* arrivals have been downloaded and parsed.
	* @param city The city the stop is in. May be empty if the service provider
	* doesn't need a separate city value.
	* @param stop The stop name for which the departures / arrivals have been received.
	* @param dataType "departures" or "arrivals".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeparateCityValue() */
	void departureListReceived( TimetableAccessor *accessor,
				    const QUrl &requestUrl,
				    const QList<DepartureInfo*> &departures,
				    const GlobalTimetableInfo &globalInfo,
				    const QString &serviceProvider,
				    const QString &sourceName,
				    const QString &city, const QString &stop,
				    const QString &dataType,
				    ParseDocumentMode parseDocumentMode );

	/** A list of journey was received.
	* @param accessor The accessor that was used to download and parse the journeys.
	* @param requestUrl The url used to request the information.
	* @param journeys A list of journeys that were received.
	* @param globalInfo Global information that affects all journeys.
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source for which the journeys have
	* been downloaded and parsed.
	* @param city The city the stop is in. May be empty if the service provider
	* doesn't need a separate city value.
	* @param stop The stop name for which the departures / arrivals have been received.
	* @param dataType "journeys".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeparateCityValue() */
	void journeyListReceived( TimetableAccessor *accessor,
				  const QUrl &requestUrl,
				  const QList<JourneyInfo*> &journeys,
				  const GlobalTimetableInfo &globalInfo,
				  const QString &serviceProvider,
				  const QString &sourceName,
				  const QString &city, const QString &stop,
				  const QString &dataType,
				  ParseDocumentMode parseDocumentMode );

	/** A list of stops was received.
	* @param accessor The accessor that was used to download and parse the stops.
	* @param requestUrl The url used to request the information.
	* @param stops A string list containing the received stop names.
	* @param stopToStopId A QHash containing the received stop names as keys and the stop
	* IDs as values (stop IDs may be empty).
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source for which the stops have been
	* downloaded and parsed.
	* @param city The city the (ambiguous) stop is in. May be empty if the service provider
	* doesn't need a separate city value.
	* @param stop The (ambiguous) stop name for which the stop list has been received.
	* @param dataType "stopList".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeparateCityValue() */
	void stopListReceived( TimetableAccessor *accessor,
			       const QUrl &requestUrl,
			       const QStringList &stops,
			       const QHash<QString, QString> &stopToStopId,
			       const QHash<QString, int> &stopToStopWeight,
			       const QString &serviceProvider,
			       const QString &sourceName, const QString &city,
			       const QString &stop, const QString &dataType,
			       ParseDocumentMode parseDocumentMode );

	/** An error was received.
	* @param accessor The accessor that was used to download and parse information
	* from the service provider.
	* @param errorType The type of error or NoError if there was no error.
	* @param errorString If @p errorType isn't NoError this contains a
	* description of the error.
	* @param requestUrl The url used to request the information.
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source.
	* @param city The city the stop is in. May be empty if the service provider
	* doesn't need a separate city value.
	* @param stop The stop name for which the error occurred.
	* @param dataType "nothing".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeparateCityValue() */
	void errorParsing( TimetableAccessor *accessor, ErrorType errorType,
			   const QString &errorString, const QUrl &requestUrl,
			   const QString &serviceProvider,
			   const QString &sourceName, const QString &city,
			   const QString &stop, const QString &dataType,
			   ParseDocumentMode parseDocumentMode );

	/** A directory with accessor info xmls was changed. */
	void accessorInfoDirChanged( QString path );

    private:
	/** Gets a map with information about an accessor.
	* @param accessor The accessor to get information about. */
	QHash< QString, QVariant > serviceProviderInfo( const TimetableAccessor *&accessor );
	QHash< QString, QVariant > locations();

	QString stripDateAndTimeValues( const QString &sourceName );

// 	struct AccessorCheck {
// 	    QDateTime checkTime;
// 	    bool result;
// 	    QString error;
// 	};
	
	QHash< QString, TimetableAccessor* > m_accessors; // List of already loaded accessors
	QHash< QString, QVariant > m_dataSources; // List of already used data sources
	QStringList m_errornousAccessors; // List of errornous accessors
	QFileSystemWatcher *m_fileSystemWatcher; // watch the accessor directory
	int m_lastStopNameCount, m_lastJourneyCount;
	
	// The next times at which new downloads will have sufficient changes
	// (enough departures in past or maybe changed delays, estimated),
	// for each data source name.
	QHash< QString, QDateTime > m_nextDownloadTimeProposals;
	
// 	QHash< QString, AccessorCheck > m_checkedAccessors;
};

/** @mainpage Public Transport Data Engine
@section intro_dataengine_sec Introduction
The public transport data engine is used to get timetable data of public transport.
It can get departure / arrival lists or journey lists.
There are different accessors used to download and parse documents from
different service providers. Currently there are two classes of accessors, one
for parsing html pages and one for parsing xml files. Both are using information
from TimetableAccessorInfo, which reads information data from xml files to
support different service providers.

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

<br>
@section index_sec Other Pages
See also
<ul>
<li>@ref pageUsage </li>
<li>@ref pageAccessorInfos </li>
<li>@ref pageClassDiagram </li>
</ul>
*/

/** @page pageUsage Usage
<ol>
<li>@ref usage_serviceproviders_sec </li>
<li>@ref usage_departures_sec </li>
<li>@ref usage_journeys_sec </li>
<li>@ref usage_stopList_sec </li>
</ol><br>

To use this data engine in an applet you need to connect it to a data source
of the public transport data engine. There is one default data source which
provides information about the available service providers
(@ref usage_serviceproviders_sec). Another data source contains departures or
arrivals (@ref usage_departures_sec) and one contains journeys
(@ref usage_journeys_sec).<br>
<br>
This enumeration can be used in your applet to ease the usage of the data engine.
Don't change the numbers, as they need to match the ones in the data engine,
which uses a similar enumeration.
@code
// The type of the vehicle used for a public transport line.
// The numbers here must match the ones in the data engine!
enum VehicleType {
    Unknown = 0, // The vehicle type is unknown

    Tram = 1, // The vehicle is a tram
    Bus = 2, // The vehicle is a bus
    Subway = 3, // The vehicle is a subway
    TrainInterurban = 4, // The vehicle is an interurban train
    Metro = 5, // The vehicle is a metro
    TrolleyBus = 6, // The vehicle is an electric bus

    TrainRegional = 10, // The vehicle is a regional train
    TrainRegionalExpress = 11, // The vehicle is a region express
    TrainInterregio = 12, // The vehicle is an interregional train
    TrainIntercityEurocity = 13, // The vehicle is a intercity / eurocity train
    TrainIntercityExpress = 14, // The vehicle is an intercity express (ICE, TGV?, ...?)

    Ferry = 100, // The vehicle is a ferry

    Plane = 200 // The vehicle is an aeroplane
};
@endcode

<br>
@section usage_serviceproviders_sec Receiving a List of Available Service Providers
You can view this data source in the plasmaengineexplorer, it's name is "ServiceProviders".
For each available service provider it contains a key
with the display name of the service provider. These keys point to the service
provider information, stored as a QHash with the following keys:<br>
<table>
<tr><td><i>id</i></td> <td>QString</td> <td>The ID of the service provider.</td></tr>
<tr><td><i>fileName</i></td> <td>QString</td> <td>The file name of the XML file containing the accessor information.</td> </tr>
<tr><td><i>scriptFileName</i></td> <td>QString</td> <td>The file name of the script file used by the accessor for parsing, if any.</td> </tr>
<tr><td><i>name</i></td> <td>QString</td> <td>The name of the accessor.</td></tr>
<tr><td><i>url</i></td> <td>QString</td> <td>The url to the home page of the service provider.</td></tr>
<tr><td><i>shortUrl</i></td> <td>QString</td> <td>A short version of the url to the home page of the service provider. This can be used to display short links, while using "url" as the url of that link.</td></tr>
<tr><td><i>country</i></td> <td>QString</td> <td>The country the service provider is (mainly) designed for.</td></tr>
<tr><td><i>cities</i></td> <td>QStringList</td> <td>A list of cities the service provider supports.</td></tr>
<tr><td><i>credit</i></td> <td>QString</td> <td>The ones who run the service provider (companies).</td></tr>
<tr><td><i>useSeparateCityValue</i></td> <td>bool</td> <td>Wheather or not the service provider needs a separate city value. If this is true, you need to specify a "city" parameter in data source names for @ref usage_departures_sec and @ref usage_journeys_sec .</td></tr>
<tr><td><i>onlyUseCitiesInList</i></td> <td>bool</td> <td>Wheather or not the service provider only accepts cities that are in the "cities" list.</td></tr>
<tr><td><i>features</i></td> <td>QStringList</td> <td>A list of strings, each string stands for a featureof the accessor.</td></tr>
<tr><td><i>featuresLocalized</i></td> <td>QStringList</td> <td>A list of localized strings describing which features the accessor has.</td></tr>
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
    bool useSeparateCityValue = serviceProviderData["useSeparateCityValue"].toBool();
}
@endcode

<br>
@section usage_departures_sec Receiving Departures or Arrivals
To get a list of departures / arrivals you need to construct the name of the
data source. For departures it begins with "Departures", for arrivals it begins
with "Arrivals". Next comes a space (" "), then the ID of the service provider
to use, e.g. "de_db" for db.de, a service provider for germany ("Deutsche Bahn").
The following parameters are separated by "|" and start with the parameter name
followed by "=". The sorting of the additional parameters doesn't matter. The
parameter <i>stop</i> is needed and can be the stop name or the stop ID. If the
service provider has useSeparateCityValue set to true (see
@ref usage_serviceproviders_sec), the parameter <i>city</i> is also needed
(otherwise it is ignored).<br>
The following parameters are allowed:<br>
<table>
<tr><td><i>stop</i></td> <td>The name or ID of the stop to get departures / arrivals for.</td></tr>
<tr><td><i>city</i></td> <td>The city to get departures/arrivals for, if needed.</td></tr>
<tr><td><i>maxCount</i></td> <td>The maximum departure/arrival count to get.</td></tr>
<tr><td><i>timeOffset</i></td> <td>The offset in minutes from now for the first departure /
arrival to get.</td></tr>
<tr><td><i>time</i></td> <td>The time of the first departure/arrival to get ("hh:mm"). This uses the current date. To use another date use 'datetime'.</td></tr>
<tr><td><i>datetime</i></td> <td>The date and time of the first departure/arrival to get (use QDateTime::toString()).</td></tr>
</table>
<br>

<b>Examples:</b><br>
<b>"Departures de_db|stop=Pappelstraße, Bremen"</b><br>
Gets departures for the stop "Pappelstraße, Bremen" using the service provider db.de.<br><br>

<b>"Arrivals de_db|stop=Leipzig|timeOffset=5|maxCount=99"</b><br>
Gets arrivals for the stop "Leipzig" using db.de, the first possible arrival is in five minutes from now, the
maximum arrival count is 99.<br><br>

<b>"Departures de_rmv|stop=Frankfurt (Main) Speyerer Straße|time=08:00"</b><br>
Gets departures for the stop "Frankfurt (Main) Speyerer Straße" using rmv.de, the first possible departure is at eight o'clock.<br><br>

<b>"Departures de_rmv|stop=3000019|maxCount=20|timeOffset=1"</b><br>
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
		// Possible stop list received, because the given stop name is ambiguous
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
<tr><td><i>error</i></td> <td>bool</td> <td>True, if an error occurred while parsing.</td></tr>
<tr><td><i>receivedPossibleStopList</i></td> <td>bool</td> <td>True, if the given stop name is ambiguous and
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
<tr><td><i>routeStops</i></td> <td>QStringList</td> <td>A list of stops of the departure/arrival to it's destination stop or a list of stops of the journey from it's start to it's destination stop. If 'routeStops' and 'routeTimes' are both set, they contain the same number of elements. And elements with equal indices are associated (the times at which the vehicle is at the stops).</td></tr>
<tr><td><i>routeTimes</i></td> <td>QList< QTime > (stored as QVariantList)</td> <td>A list of times of the departure/arrival to it's destination stop. If 'routeStops' and 'routeTimes' are both set, they contain the same number of elements. And elements with equal indices are associated (the times at which the vehicle is at the stops).</td></tr>
<tr><td><i>routeExactStops</i></td> <td>int</td> <td>The number of exact route stops. The route stop list isn't complete from the last exact route stop.</td></tr>

</table>

<br>
@section usage_journeys_sec Receiving Journeys from A to B
To get a list of journeys from one stop to antoher you need to construct the
name of the data source (much like the data source for departures / arrivals).
The data source name begins with "Journeys". Next comes a space (" "), then
the ID of the service provider to use, e.g. "de_db" for db.de, a service
provider for germany ("Deutsche Bahn").
The following parameters are separated by "|" and start with the parameter name
followed by "=". The sorting of the additional parameters doesn't matter. The
parameters <i>originStop</i> and <i>targetStop</i> are needed and can be the
stop names or the stop IDs. If the service provider has useSeparateCityValue
set to true (see @ref usage_serviceproviders_sec), the parameter <i>city</i> is
also needed (otherwise it is ignored).<br>
The following parameters are allowed:<br>
<table>
<tr><td><i>originStop</i></td> <td>The name or ID of the origin stop.</td></tr>
<tr><td><i>targetStop</i></td> <td>The name or ID of the target stop.</td></tr>
<tr><td><i>city</i></td> <td>The city to get journeys for, if needed.</td></tr>
<tr><td><i>maxCount</i></td> <td>The maximum journey count to get.</td></tr>
<tr><td><i>timeOffset</i></td> <td>The offset in minutes from now for the first journey to get.</td></tr>
<tr><td><i>time</i></td> <td>The time for the first journey to get (in format "hh:mm").</td></tr>
<tr><td><i>datetime</i></td> <td>The date and time for the first journey to get (QDateTime::fromString() is used with default parameters to parse the date).</td></tr>
</table>
<br>

<b>Examples:</b><br>
<b>"Journeys de_db|originStop=Pappelstraße, Bremen|targetStop=Kirchweg, Bremen"</b><br>
Gets journeys from stop "Pappelstraße, Bremen" to stop "Kirchweg, Bremen"
using the service provider db.de.<br><br>

<b>"Journeys de_db|originStop=Leipzig|targetStop=Hannover|timeOffset=5|maxCount=99"</b><br>
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
		// Possible stop list received, because the given stop name is ambiguous
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
<tr><td><i>error</i></td> <td>bool</td> <td>True, if an error occurred while parsing.</td></tr>
<tr><td><i>receivedPossibleStopList</i></td> <td>bool</td> <td>True, if the given stop name is ambiguous and
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
<tr><td><i>routeStops</i></td> <td>QStringList</td> <td>A list of stops of the journey from it's start to it's destination stop. If 'routeStops' and 'routeTimes' are both set, they contain the same number of elements. And elements with equal indices are associated (the times at which the vehicle is at the stops).</td></tr>
<tr><td><i>routeTimesDeparture</i></td> <td>QList< QTime > (stored as QVariantList)</td> <td>A list of departure times of the journey to it's destination stop. If 'routeStops' and 'routeTimesDeparture' are both set, the latter contains one element less (because the last stop has no departure, only an arrival time). Elements with equal indices are associated (the times at which the vehicle departs from the stops).</td></tr>
<tr><td><i>routeTimesArrival</i></td> <td>QList< QTime > (stored as QVariantList)</td> <td>A list of arrival times of the journey to it's destination stop. If 'routeStops' and 'routeTimesArrival' are both set, the latter contains one element less (because the last stop has no departure, only an arrival time). Elements with equal indices are associated (the times at which the vehicle departs from the stops).</td></tr>
<tr><td><i>routeExactStops</i></td> <td>int</td> <td>The number of exact route stops. The route stop list isn't complete from the last exact route stop.</td></tr>
<tr><td><i>routeVehicleTypes</i></td> <td>QList< int > (stored as QVariantList)</td> <td>A list of vehicle types used for each "sub-journey" in the journey. The vehicle types are described by integers (see @ref pageUsage).</td></tr>
<tr><td><i>routeTransportLines</i></td> <td>QStringList</td> <td>A list of transport lines used for each "sub-journey" in the journey.</td></tr>
<tr><td><i>routePlatformsDeparture</i></td> <td>QStringList</td> <td>A list of platforms of the departure used for each stop in the journey.</td></tr>
<tr><td><i>routePlatformsArrival</i></td> <td>QStringList</td> <td>A list of platforms of the arrival used for each stop in the journey.</td></tr>
<tr><td><i>routeTimesDepartureDelay</i></td> <td>QList< int > (stored as QVariantList)</td> <td>A list of delays in minutes of the departures at each stop in the journey. A value of 0 means, that the vehicle is on schedule, -1 means, that there's no information about delays.</td></tr>
<tr><td><i>routeTimesArrivalDelay</i></td> <td>QList< int > (stored as QVariantList)</td> <td>A list of delays in minutes of the arrivals at each stop in the journey. A value of 0 means, that the vehicle is on schedule, -1 means, that there's no information about delays.</td></tr>
</table>

<br>
@section usage_stopList_sec Receiving Stop Lists
When you have requested departures, arrivals or journeys from the data engine,
it may return a list of stops, if the given stop name is ambiguous.
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

/** @page pageAccessorInfos Add Support for new Service Providers
To add support for a new service provider you need to create an "accessor info xml". These xml files describe where to download the data and how to parse it. The filename starts with the country code or "international" or "unknown" followed by "_" and a short name of the service provider, e.g. "de_db", "ch_sbb", "sk_atlas", "international_flightstats".<br>
Here is an overview of the allowed tags:
<table>
<tr style="background-color: #bbbbbb; font-weight: bold;"><td>Tag</td> <td>Child of</td> <td>Optional?</td> <td>Description</td></tr>

<tr><td><b>\<accessorInfo\></b></td>
<td>Root</td> <td>Required</td> <td>This is the root item.</td></tr>

<tr><td><b>\<xml_file_version type="publictransport" version="1.0" /\></b></td>
<td>\<accessorInfo\></td> <td>Not used</td>
<td>This is currently not used by the data engine. But to be sure that the xml is parsed correctly you should add this tag. The version is the version of the xml file structure, current version is 1.0.</td></tr>

<tr><td><b>\<name\></b></td>
<td>\<accessorInfo\></td> <td>Required</td>
<td>The name of the accessor. If it provides data for international stops it should begin with "International", if it's specific for a country or city it should begin with the name of that country or city. That should be followed by a short url to the service provider.</td></tr>

<tr><td style="color:#888800"><b>\<author\></b></td>
<td>\<accessorInfo\></td> <td>Required</td>
<td>Contains information about the author of this accessor info xml.</td></tr>

<tr><td style="color:#666600"><b>\<fullname\></b></td>
<td style="color:#888800">\<author\></td> <td>Required</td>
<td>The full name of the author of this accessor info xml.</td></tr>

<tr><td style="color:#666600"><b>\<email\></b></td>
<td style="color:#888800">\<author\></td> <td>Optional</td> <td>
The email address of the author of this accessor info xml.</td></tr>

<tr><td><b>\<version\></b></td>
<td>\<accessorInfo\></td> <td>Required</td>
<td>The version of this accessor info xml, should start with "1.0".</td></tr>

<tr><td><b>\<type\></b></td>
<td>\<accessorInfo\></td> <td>Required</td>
<td>Can be either HTML or XML.</td></tr>

<tr><td>Deprecated (TODO: remove) \<country\></td>
<td>\<accessorInfo\></td> <td>Required</td>
<td>The country the service provider has most data for. Can also be "international" or "unknown".</td></tr>

<tr><td style="color:#00bb00"><b>\<cities\></b></td>
<td>\<accessorInfo\></td> <td>Optional</td>
<td>A list of cities the service provider has data for (with surrounding \<city\>-tags).</td></tr>

<tr><td style="color:#007700"><b>\<city\></b></td>
<td style="color:#00bb00">\<cities\></td> <td>Optional</td>
<td>A city in the list of cities (\<cities\>). Can have an attribute "replaceWith", to replace city names with values used by the service provider.</td></tr>

<tr><td><b>\<description\></b></td>
<td>\<accessorInfo\></td> <td>Required</td>
<td>A description of the service provider / accessor. You don't need to list the features supported by the accessor here, the feature list is generated automatically.</td></tr>

<tr><td><b>\<url\></b></td>
<td>\<accessorInfo\></td> <td>Required</td>
<td>An url to the service provider home page.</td></tr>

<tr><td><b>\<shortUrl\></b></td>
<td>\<accessorInfo\></td> <td>Required</td>
<td>A short version of the url, used as link text.</td></tr>

<tr><td style="color:#bb0000;"><b>\<rawUrls\></b></td>
<td>\<accessorInfo\></td> <td>Required</td>
<td>Contains the used "raw urls". A raw url is a string with placeholders that are replaced with values to get a real url.</td></tr>

<tr><td style="color:#770000;"><b>\<departures\></b></td>
<td style="color:#bb0000;">\<rawUrls\></td> <td>Required</td>
<td>A raw url (in a CDATA tag) to a page containing a departure / arrival list. The following substrings are replaced by current values: <b>{stop}</b> (the stop name), <b>{type}</b> (arr or dep for arrivals or departures), <b>{time}</b> (the time of the first departure / arrival), <b>{maxCount}</b> (maximal number of departures / arrivals).</td></tr>

<tr><td style="color:#770000;"><b>\<journeys\></b></td>
<td style="color:#bb0000;">\<rawUrls\></td> <td>Optional</td>
<td>A raw url (in a CDATA tag) to a page containing a journey list. The following substrings are replaced by current values: <b>{startStop}</b> (the name of the stop where the journey starts), <b>{targetStop}</b> (the name of the stop where the journey ends), <b>{time}</b> (the time of the first journey), <b>{maxCount}</b> (maximal number of journeys).</td></tr>

<tr><td style="color:#770000;"><b>\<stopSuggestions\></b></td>
<td style="color:#bb0000;">\<rawUrls\></td> <td>Optional</td>
<td>A raw url (in a CDATA tag) to a page containing a list of stop suggestions. Normally this tag isn't needed, because the url is the same as the url to the departure list. When the stop name is ambiguous the service provider can show a page containing a list of stop suggestions. You may want to use this tag if you want to parse XML files for departure lists and get the stop suggestions from an HTML page or if there is a special url only for stop suggestions.</td></tr>

<tr><td style="color:#0000bb;"><b>\<regExps\></b></td>
<td>\<accessorInfo\></td> <td>Required (Optional for XML)</td>
<td>Contains regular expressions used to parse the documents from the service provider.</td></tr>

<tr><td style="color:#000077;"><b>\<departures\></b></td>
<td style="color:#0000bb;">\<regExps\></td> <td>Required (Optional for XML)</td>
<td>Contains a regular expression used to parse documents with departure / arrival lists.</td></tr>

<tr><td style="color:#000077;"><b>\<journeys\></b></td>
<td style="color:#0000bb;">\<regExps\></td> <td>Optional</td>
<td>Contains a regular expression used to parse documents with journey lists.</td></tr>

<tr><td style="color:#000077;"><b>\<journeyNews\></b></td>
<td style="color:#0000bb;">\<regExps\></td> <td>Optional</td>
<td>Contains a list of regular expression used to parse strings matched with the \<info\> "JourneyNews".</td></tr>

<tr><td style="color:#000077;"><b>\<possibleStops\></b></td>
<td style="color:#0000bb;">\<regExps\></td> <td>Optional</td>
<td>Contains a list of regular expression used to parse documents with stop suggestions. This is used if there's a special raw url for stop suggestions or if no departures / arrivals / journeys could be found in a document.</td></tr>

<tr><td style="color:#000033;"><b>\<regExp\></b></td>
<td style="color:#000077;">\<departures\>, \<journeys\>, \<item\></td> <td>Required</td>
<td>A regular expression (in a CDATA tag).</td></tr>

<tr><td style="color:#000033;"><b>\<infos\></b></td>
<td style="color:#000077;">\<departures\>, \<journeys\>, \<item\></td> <td>Required</td>
<td>A list of \<info\>-tags. For each string matched by the regular expression there must be an \<info\>-tag with the meaning of that match.</td></tr>

<tr><td><b>\<info\></b></td>
<td style="color:#000033;">\<infos\></td> <td>Required</td>
<td>Information about a match of a regular expression. Allowed values are: Nothing, DepartureDate, DepartureHour, DepartureMinute, TypeOfVehicle, TransportLine, FlightNumber, Target, Platform, Delay, DelayReason, JourneyNews, JourneyNewsOther, JourneyNewsLink, DepartureHourPrognosis, DepartureMinutePrognosis, Operator, DepartureAMorPM, DepartureAMorPMPrognosis, ArrivalAMorPM, Status, Duration, StartStopName, StartStopID, TargetStopName, TargetStopID, ArrivalDate, ArrivalHour, ArrivalMinute, Changes, TypesOfVehicleInJourney, Pricing, NoMatchOnSchedule, StopName, StopID.</td></tr>

<tr><td style="color:#000033;"><b>\<item\></b></td>
<td style="color:#000077;">\<jorneyNews\>, \<possibleStops\></td> <td>Required</td>
<td>Contains \<regExp\> and \<infos\> tags. There can be more than one \<item\> tag to use different regular expressions.</td></tr>
</table>

Here is an example of a simple accessor info xml (de_bvg.xml):
@verbinclude de_bvg.xml
*/

/** @page pageClassDiagram Class Diagram
\dot
digraph publicTransportDataEngine {
    ratio="compress";
    size="10,100";
    // concentrate="true";
    // rankdir="LR";
     clusterrank=local;

    node [
       shape=record
       fontname=Helvetica, fontsize=10
       style=filled
       fillcolor="#eeeeee"
       ];

    engine [
       label="{PublicTransportEngine|The main class of the public transport data engine.\l}"
       URL="\ref PublicTransportEngine"
       style=filled
       fillcolor="#ffdddd"
       ];

    accessorHtml [
    label="{TimetableAccessorHtml|Parses HTML documents.\l|# parseDocument() : bool\l# parseDocumentPre() : bool\l# parseDocumentPossibleStops() : bool\l# parseJourneyNews() : bool\l}"
    URL="\ref TimetableAccessorHtml"
    ];

    accessor [
    label="{TimetableAccessor|Loads and parses documents from service providers.\lIt uses TimetableAccessorInfo to get needed information.\l|+ static getSpecificAccessor( serviceProvider ) : TimetableAccessor*\l+ requestDepartures() : KIO:TransferJob\l+ requestJourneys() : KIO:TransferJob\l+ timetableAccessorInfo() : TimetableAccessorInfo\l|# parseDocument() : bool\l# parseDocumentPossibleStops() : bool\l# getUrl() : KUrl\l# getJourneyUrl() : KUrl\l}"
    URL="\ref TimetableAccessor"
    ];

    accessorXml [
    label="{TimetableAccessorXml|Parses XML documents.\l|# parseDocument() : bool\l# parseDocumentPossibleStops() : bool\l}"
    URL="\ref TimetableAccessorXml"
    ];

    subgraph cluster_subDataTypes {
	label="Data Types";
	style="rounded, filled";
	color="#ccccff";
	node [ fillcolor="#dfdfff" ];

	pubTransInfo [
	label="{PublicTransportInfo|Used to store information about a received data unit.\l|+ departure() : QDateTime\l+ operatorName() : QString\l}"
	URL="\ref PublicTransportInfo"
	];

	depInfo [
	label="{DepartureInfo|Stores departure / arrival information.\l|+ target() : QString\l+ line() : QString\l+ vehicleType() : VehicleType\l+ isNightLine() : bool\l+ isExpressLine() : bool\l+ platform() : QString\l+ delay() : int\l+ delayReason() : QString\l+ journeyNews() : QString\l}"
	URL="\ref DepartureInfo"
	];

	journeyInfo [
	label="{JourneyInfo|Stores journey information.\l|+ pricing() : QString\l+ journeyNews() : QString\l+ startStopName() : QString\l+ targetStopName() : QString\l+ arrival() : QDateTime\l+ duration() : int\l+ vehicleTypes() : QList\<VehicleType\>\l+ changes() : int\l}"
	URL="\ref JourneyInfo"
	];
    }

     subgraph cluster_subAccessorInfos {
	label="Accessor Information";
	style="rounded, filled";
	color="#ccffcc";

	node [ fillcolor="#dfffdf" ];

//	label="{TimetableAccessorInfo|Information about where to download the documents \land how to parse them.\l|+ name() : QString\l+ charsetForUrlEncoding() : QByteArray\l+ stopSuggestionsRawUrl() : QString\l+ description() : QString\l+ author() : QString\l+ email() : QString\l+ version() : QString\l+ url() : QString\l+ shortUrl() : QString\l+ departureRawUrl() : QString\l+ journeyRawUrl() : QString\l+ serviceProvider() : QString\l+ country() : QString\l+ cities() : QStringList\l+ defaultVehicleType() : VehicleType\l+ searchDepartures() : TimetableRegExpSearch\l+ searchJourneys() : TimetableRegExpSearch\l+ searchDeparturesPre() : TimetableRegExpSearch*\l+ regExpSearchPossibleStopsRanges() : QStringList\l+ searchPossibleStops() : QList\<TimetableRegExpSearch\>\l+ searchJourneyNews() : QList\<TimetableRegExpSearch\>\l+ useSeparateCityValue() : bool\l+ onlyUseCitiesInList() : bool\l+ mapCityNameToValue(cityName) : QString\l+ fileName() : QString\l+ features() : QStringList\l+ supportsStopAutocompletion() : bool\l+ supportsTimetableAccessorInfo(info) : bool\l }"

	accessorInfo [
	label="{TimetableAccessorInfo|Information about where to download the documents \land how to parse them.\l|+ name() : QString\l+ stopSuggestionsRawUrl() : QString\l+ departureRawUrl() : QString\l+ journeyRawUrl() : QString\l+ searchDepartures() : TimetableRegExpSearch\l+ searchJourneys() : TimetableRegExpSearch\l+ searchDeparturesPre() : TimetableRegExpSearch*\l+ regExpSearchPossibleStopsRanges() : QStringList\l+ searchPossibleStops() : QList\<TimetableRegExpSearch\>\l+ searchJourneyNews() : QList\<TimetableRegExpSearch\>\l+ features() : QStringList\l... }"
	URL="\ref TimetableAccessorInfo"
	];

	regExpSearch [
	label="{TimetableRegExpSearch|Stores a regular expression with information\labout the meanings of the matches.\l|+ isValid() : bool\l+ regExp() : QRegExp\l+ infos() : QList\<TimetableInformation\>\l}"
	URL="\ref TimetableRegExpSearch"
	];
     }

   // { rank=same; accessor pubTransInfo accessorHtml accessorXml accessorInfo regExpSearch depInfo journeyInfo }
   { rank=same; accessor accessorHtml accessorXml }

    edge [ arrowhead="empty", arrowtail="none", style="solid" ];
    accessorHtml -> accessor;

    edge [ dir=back, arrowhead="none", arrowtail="empty", style="solid" ];
    accessor -> accessorXml;
    pubTransInfo -> depInfo;
    pubTransInfo -> journeyInfo;

    edge [ dir=forward, arrowhead="none", arrowtail="normal", style="dashed", fontcolor="gray", taillabel="0..*" ];
    accessorHtml -> accessorXml [ label="uses" ];

    edge [ dir=back, arrowhead="normal", arrowtail="none", style="dashed", fontcolor="gray", taillabel="", headlabel="0..*" ];
    engine -> accessor [ label=""uses ];
    accessor -> pubTransInfo [ label="uses" ];
    accessor -> accessorInfo [ label="uses" ];
    accessorInfo -> regExpSearch [ label="uses" ];

    edge [ dir=both, arrowhead="normal", arrowtail="normal", color="gray", fontcolor="gray", style="dashed", headlabel="" ];
    accessorInfo -> accessor [ label="friend" ];
}
@enddot
*/

#endif
