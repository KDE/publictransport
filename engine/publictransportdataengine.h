/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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

// Own includes
#include "enums.h"
#include "departureinfo.h"

// Plasma includes
#include <Plasma/DataEngine>

struct AbstractRequest;
struct StopSuggestionRequest;
struct DepartureRequest;
struct JourneyRequest;

class ServiceProvider;
class DepartureInfo;
class JourneyInfo;
class StopInfo;

class QTimer;
class QFileSystemWatcher;


/**
 * @brief This engine provides departure/arrival times and journeys for public transport.
 *
 * @see @ref pageUsage (how to use this data engine in an applet?)
 */
class PublicTransportEngine : public Plasma::DataEngine {
    Q_OBJECT

public:
    /**
     * @brief The available types of sources of this data engine.
     *
     * They all have an associated keyword with that data source names start.
     * @see sourceTypeKeyword
     * @see sourceTypeFromName
     **/
    enum SourceType {
        InvalidSourceName = 0, /**< Returned by @ref sourceTypeFromName, if
                * the source name is invalid. */

        ServiceProviderSource = 1, /**< The source contains information about available
                * service providers for a given country. */
        ServiceProvidersSource = 2, /**< The source contains information about available
                * service providers. */
        ErroneousServiceProvidersSource = 3, /**< The source contains a list of erroneous
                * service providers. */
        LocationsSource = 4, /**< The source contains information about locations
                * for which supported service providers exist. */

        DeparturesSource = 10, /**< The source contains timetable data for departures. */
        ArrivalsSource, /**< The source contains timetable data for arrivals. */
        StopsSource, /**< The source contains a list of stop suggestions. */
        JourneysSource, /**< The source contains information about journeys. */
        JourneysDepSource, /**< The source contains information about journeys,
                * that depart at the given date and time. */
        JourneysArrSource /**< The source contains information about journeys,
                * that arrive at the given date and time. */
    };

    /** @brief Every data engine needs a constructor with these arguments. */
    PublicTransportEngine( QObject* parent, const QVariantList& args );

    /** @brief Destructor. */
    ~PublicTransportEngine();

    /** @returns the keyword of the given @p sourceType used in source names. */
    static const QString sourceTypeKeyword( SourceType sourceType );

    /** @returns the type of the given @p sourceName. */
    SourceType sourceTypeFromName( const QString &sourceName ) const;

    /** @brief Reimplemented to add some always visible default sources. */
    virtual QStringList sources() const;

    /**
     * @returns true, if a data source of the given @p sourceType requests
     * data from a web server.
     **/
    bool isDataRequestingSourceType( SourceType sourceType ) const {
        return static_cast< int >( sourceType ) >= 10; };

    /**
     * @brief Minimum timeout in seconds to request new data. Before the timeout
     * is over, old stored data from previous requests is used. */
    static const int MIN_UPDATE_TIMEOUT;

    /**
     * @brief Maximum timeout in seconds to request new data, if delays are avaiable.
     * Before the timeout is over, old stored data from previous requests is used.
     **/
    static const int MAX_UPDATE_TIMEOUT_DELAY;

    /**
     * @brief The default time offset from now for the first departure/arrival/journey
     * in the list. This is used if it wasn't specified in the source name.
     **/
    static const int DEFAULT_TIME_OFFSET;

protected:
    /**
     * @brief This virtual function is called when a new source is requested.
     *
     * @param name The name of the requested data source.
     **/
    bool sourceRequestEvent( const QString& name );

    /**
     * @brief This virtual function is called when an automatic update is triggered for an
     * existing source (ie: when a valid update interval is set when requesting a source).
     *
     * @param name The name of the data source to be updated.
     **/
    bool updateSourceEvent( const QString& name );

    bool updateServiceProviderForCountrySource( const QString &name );
    bool updateServiceProviderSource();
    void updateErroneousServiceProviderSource( const QString &name );
    bool updateLocationSource();
    bool updateTimetableDataSource( const QString &name );

    /**
     * @brief Returns wheather or not the given source is up to date.
     *
     * @param name The name of the source to be checked.
     **/
    bool isSourceUpToDate( const QString& name );

public slots:
    void slotSourceRemoved( const QString &name );

    void forceUpdate();

    /**
     * @brief A list of departures / arrivals was received.
     *
     * @param provider The provider that was used to download and parse the
     *   departures/arrivals.
     * @param requestUrl The url used to request the information.
     * @param departures A list of departures/arrivals that were received.
     * @param globalInfo Global information that affects all departures/arrivals.
     * @param request Information about the request for the here received @p departures.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void departureListReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const DepartureInfoList &departures,
            const GlobalTimetableInfo &globalInfo,
            const DepartureRequest &request,
            bool deleteDepartureInfos = true );

    /**
     * @brief A list of journey was received.
     *
     * @param provider The provider that was used to download and parse the journeys.
     * @param requestUrl The url used to request the information.
     * @param journeys A list of journeys that were received.
     * @param globalInfo Global information that affects all journeys.
     * @param request Information about the request for the here received @p journeys.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void journeyListReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const JourneyInfoList &journeys,
            const GlobalTimetableInfo &globalInfo,
            const JourneyRequest &request,
            bool deleteJourneyInfos = true );

    /**
     * @brief A list of stops was received.
     *
     * @param provider The service provider that was used to download and parse the stops.
     * @param requestUrl The url used to request the information.
     * @param stops A pointer to a list of @ref StopInfo objects.
     * @param request Information about the request for the here received @p stops.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void stopListReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const StopInfoList &stops,
            const StopSuggestionRequest &request,
            bool deleteStopInfos = true );

    /**
     * @brief An error was received.
     *
     * @param provider The provider that was used to download and parse information
     *   from the service provider.
     * @param errorType The type of error or NoError if there was no error.
     * @param errorString If @p errorType isn't NoError this contains a description of the error.
     * @param requestUrl The url used to request the information.
     * @param request Information about the request that failed with @p errorType.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void errorParsing( ServiceProvider *provider, ErrorCode errorType, const QString &errorString,
            const QUrl &requestUrl, const AbstractRequest *request );

    /**
     * @brief A global or local directory with service provider XML files was changed.
     *
     * This slot uses reloadAllProviders() to actually update the loaded service providers. Because
     * a change in multiple files in one or more directories causes a call to this slot for each
     * file, this would cause all providers to be reloaded for each changed file. Especially when
     * installing a new version, while running an old one, this can take some time.
     * Therefore this slot uses a short timeout. If a new call to this slot happens within that
     * timeout, the timeout gets simply restarted. Otherwise after the timeout reloadAllProviders()
     * gets called.
     *
     * @param path The changed directory.
     * @see reloadAllProviders()
     */
    void serviceProviderDirChanged( const QString &path );

    /**
     * @brief Deletes all currently created providers and associated data and recreates them.
     *
     * Deleted data only gets filled by new requests again.
     * This slot gets called by serviceProviderDirChanged() if a global or local directory
     * containing service provider XML files has changed. It does not call this function on every
     * directory change, but only if there are no further changes in a short timespan.
     *
     * @see serviceProviderDirChanged()
     **/
    void reloadAllProviders();

private:
    /**
     * @brief Gets a hash with information about a given service @p provider.
     *
     * @param provider The provider to get information about.
     **/
    QHash< QString, QVariant > serviceProviderData( const ServiceProvider *&provider );

    /**
     * @brief Gets a hash with information about available locations.
     *
     * Locations are here countries plus some special locations like "international".
     **/
    QHash< QString, QVariant > locations();

    QString stripDateAndTimeValues( const QString &sourceName );

    QHash< QString, ServiceProvider* > m_providers; // List of already loaded service providers
    QHash< QString, QVariant > m_dataSources; // List of already used data sources
    QStringList m_erroneousProviders; // List of erroneous service providers
    QFileSystemWatcher *m_fileSystemWatcher; // Watch the service provider directory
    int m_lastStopNameCount, m_lastJourneyCount;

    // The next times at which new downloads will have sufficient changes
    // (enough departures in past or maybe changed delays, estimated),
    // for each data source name.
    QHash< QString, QDateTime > m_nextDownloadTimeProposals;

    QTimer *m_providerUpdateDelayTimer;
    QTimer *m_sourceUpdateTimer;
    QStringList m_runningSources; // Sources which are currently being processed
};

/** @mainpage Public Transport Data Engine
@section intro_dataengine_sec Introduction
The public transport data engine provides timetable data for public transport, trains, ships,
ferries and planes. It can get departure/arrival lists or journey lists.
There are different service providers used to download and parse documents from
different service providers. Currently there is only one class of service providers to parse
documents using scripts. A GTFS class is in preparation in a GIT branch. All are using information
from ServiceProviderData, which reads information data from xml files to support different
service providers.

<br />
@section install_sec Installation
To install this data engine type the following commands:<br />
\> cd /path-to-extracted-engine-sources/build<br />
\> cmake -DCMAKE_INSTALL_PREFIX=`kde4-config --prefix` ..<br />
\> make<br />
\> make install<br />
<br />
After installation do the following to use the data engine in your plasma desktop:
Restart plasma to load the data engine:<br />
\> kquitapp plasma-desktop<br />
\> plasma-desktop<br />
<br />
or test it with:<br />
\> plasmaengineexplorer --engine publictransport<br />
<br />
You might need to run kbuildsycoca4 in order to get the .desktop file recognized.
<br />

<br />

@section index_sec Other Pages
@par
    @li @ref pageUsage
    @li @ref pageServiceProviders
    @li @ref pageClassDiagram
*/

/** @page pageUsage Usage

@par Sections
    @li @ref usage_serviceproviders_sec
    @li @ref usage_departures_sec
    @li @ref usage_journeys_sec
    @li @ref usage_stopList_sec

<br />

@section usage_introduction_sec Introduction
To use this data engine in an applet you need to connect it to a data source
of the public transport data engine. There are data sources which provide information about the
available service providers (@ref usage_serviceproviders_sec) or supported countries. Another data
source contains departures or arrivals (@ref usage_departures_sec) and one contains journeys
(@ref usage_journeys_sec).<br />
<br />
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
    InterurbanTrain = 4, // The vehicle is an interurban train
    Metro = 5, // The vehicle is a metro
    TrolleyBus = 6, // The vehicle is an electric bus

    RegionalTrain = 10, // The vehicle is a regional train
    RegionalExpressTrain = 11, // The vehicle is a region express
    InterregionalTrain = 12, // The vehicle is an interregional train
    IntercityTrain = 13, // The vehicle is a intercity / eurocity train
    HighspeedTrain = 14, // The vehicle is an intercity express (ICE, TGV?, ...?)

    Ferry = 100, // The vehicle is a ferry

    Plane = 200 // The vehicle is an aeroplane
};
@endcode

<br />
@section usage_serviceproviders_sec Receiving a List of Available Service Providers
You can view this data source in the plasmaengineexplorer, it's name is <em>"ServiceProviders"</em>.
For each available service provider it contains a key with the display name of the service provider.
These keys point to the service provider information, stored as a QHash with the following keys:
<br />
<table>
<tr><td><i>id</i></td> <td>QString</td> <td>The ID of the service provider plugin.</td></tr>
<tr><td><i>fileName</i></td> <td>QString</td> <td>The file name of the XML file containing the service provider information.</td> </tr>
<tr><td><i>scriptFileName</i></td> <td>QString</td> <td>The file name of the script used to parse documents from the service provider, if any.</td> </tr>
<tr><td><i>name</i></td> <td>QString</td> <td>The name of the service provider.</td></tr>
<tr><td><i>url</i></td> <td>QString</td> <td>The url to the home page of the service provider.</td></tr>
<tr><td><i>shortUrl</i></td> <td>QString</td> <td>A short version of the url to the home page of the service provider. This can be used to display short links, while using "url" as the url of that link.</td></tr>
<tr><td><i>country</i></td> <td>QString</td> <td>The country the service provider is (mainly) designed for.</td></tr>
<tr><td><i>cities</i></td> <td>QStringList</td> <td>A list of cities the service provider supports.</td></tr>
<tr><td><i>credit</i></td> <td>QString</td> <td>The ones who run the service provider (companies).</td></tr>
<tr><td><i>useSeparateCityValue</i></td> <td>bool</td> <td>Wheather or not the service provider needs a separate city value. If this is true, you need to specify a "city" parameter in data source names for @ref usage_departures_sec and @ref usage_journeys_sec .</td></tr>
<tr><td><i>onlyUseCitiesInList</i></td> <td>bool</td> <td>Wheather or not the service provider only accepts cities that are in the "cities" list.</td></tr>
<tr><td><i>features</i></td> <td>QStringList</td> <td>A list of strings, each string stands for a feature of the service provider.</td></tr>
<tr><td><i>featuresLocalized</i></td> <td>QStringList</td> <td>A list of localized strings describing which features the service provider offers.</td></tr>
<tr><td><i>author</i></td> <td>QString</td> <td>The author of the service provider plugin.</td></tr>
<tr><td><i>email</i></td> <td>QString</td> <td>The email address of the author of the service provider plugin.</td></tr>
<tr><td><i>description</i></td> <td>QString</td> <td>A description of the service provider.</td></tr>
<tr><td><i>version</i></td> <td>QString</td> <td>The version of the service provider plugin.</td></tr>
</table>
<br />
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

There is also data source named <em>"ServiceProvider \<country-code|service-provider-id\>"</em>
to get information about the default service provider for the given country or about the given
provider with the given ID.

<br />
@section usage_departures_sec Receiving Departures or Arrivals
To get a list of departures/arrivals you need to construct the name of the data source. For
departures it begins with "Departures", for arrivals it begins with "Arrivals". Next comes a
space (" "), then the ID of the service provider to use, e.g. "de_db" for db.de, a service provider
for germany ("Deutsche Bahn"). The following parameters are separated by "|" and start with the
parameter name followed by "=" and the value. The sorting of the additional parameters doesn't
matter. The parameter <i>stop</i> is needed and can be the stop name or the stop ID. If the service
provider has useSeparateCityValue set to true (see @ref usage_serviceproviders_sec), the parameter
<i>city</i> is also needed (otherwise it is ignored).
You can leave the service provider ID away, the data engine then uses the default service provider
for the users country.<br />
The following parameters are allowed:<br />
<table>
<tr><td><i>stop</i></td> <td>The name or ID of the stop to get departures/arrivals for.</td></tr>
<tr><td><i>city</i></td> <td>The city to get departures/arrivals for, if needed.</td></tr>
<tr><td><i>maxCount</i></td> <td>The maximum departure/arrival count to get.</td></tr>
<tr><td><i>timeOffset</i></td> <td>The offset in minutes from now for the first departure /
arrival to get.</td></tr>
<tr><td><i>time</i></td> <td>The time of the first departure/arrival to get ("hh:mm"). This uses
the current date. To use another date use 'datetime'.</td></tr>
<tr><td><i>datetime</i></td> <td>The date and time of the first departure/arrival to get (use
QDateTime::toString()).</td></tr>
</table>
<br />

<b>Examples:</b><br />
<b>"Departures de_db|stop=Pappelstraße, Bremen"</b><br />
Gets departures for the stop "Pappelstraße, Bremen" using the service provider db.de.<br /><br />

<b>"Arrivals de_db|stop=Leipzig|timeOffset=5|maxCount=99"</b><br />
Gets arrivals for the stop "Leipzig" using db.de, the first possible arrival is in five minutes
from now, the maximum arrival count is 99.<br /><br />

<b>"Departures de_rmv|stop=Frankfurt (Main) Speyerer Straße|time=08:00"</b><br />
Gets departures for the stop "Frankfurt (Main) Speyerer Straße" using rmv.de, the first possible
departure is at eight o'clock.<br /><br />

<b>"Departures de_rmv|stop=3000019|maxCount=20|timeOffset=1"</b><br />
Gets departures for the stop with the ID "3000019", the first possible departure is in one minute
from now, the maximum departure count is 20.<br /><br />

<b>"Departures stop=Hauptbahnhof"</b><br />
Gets departures for the stop "Hauptbahnhof" using the default service provider for the users
country, if there is one.<br /><br />

Once you have the data source name, you can connect your applet to that data source from the data
engine. Here is an example of how to do this:
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
<br />
The data received from the data engine always contains these keys:<br />
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
<br />
Each departure/arrival in the data received from the data engine (departureData in the code
example) has the following keys:<br />
<table>
<tr><td><i>line</i></td> <td>QString</td> <td>The name of the public transport line, e.g. "S1", "6", "1E", "RB 24155".</td></tr>
<tr><td><i>target</i></td> <td>QString</td> <td>The name of the target / origin of the public transport line.</td></tr>
<tr><td><i>departure</i></td> <td>QDateTime</td> <td>The date and time of the departure / arrival.</td></tr>
<tr><td><i>vehicleType</i></td> <td>int</td> <td>An int containing the ID of the vehicle type used for the
departure/arrival. You can cast the ID to VehicleType using "static_cast<VehicleType>( iVehicleType )".</td></tr>
<tr><td><i>vehicleName</i></td> <td>QString</td> <td>A translated name for the vehicle type used for the
departure/arrival.</td></tr>
<tr><td><i>vehicleNamePlural</i></td> <td>QString</td> <td>A translated name for the vehicle type used for the
departure/arrival, plural version.</td></tr>
<tr><td><i>vehicleIconName</i></td> <td>QString</td> <td>The name of the icon for the vehicle type used for the
departure/arrival. Can be used as argument to the KIcon constructor.</td></tr>
<tr><td><i>nightline</i></td> <td>bool</td> <td>Wheather or not the public transport line is a night line.</td></tr>
<tr><td><i>expressline</i></td> <td>bool</td> <td>Wheather or not the public transport line is an express line.</td></tr>
<tr><td><i>platform</i></td> <td>QString</td> <td>The platform from/at which the vehicle departs/arrives.</td></tr>
<tr><td><i>delay</i></td> <td>int</td> <td>The delay in minutes, 0 means 'on schedule', -1 means 'no delay information available'.</td></tr>
<tr><td><i>delayReason</i></td> <td>QString</td> <td>The reason of a delay.</td></tr>
<tr><td><i>status</i></td> <td>QString</td> <td>The status of the departure, if available.</td></tr>
<tr><td><i>journeyNews</i></td> <td>QString</td> <td>News for the journey.</td></tr>
<tr><td><i>operator</i></td> <td>QString</td> <td>The company that is responsible for the journey.</td></tr>
<tr><td><i>routeStops</i></td> <td>QStringList</td> <td>A list of stops of the departure/arrival to it's destination stop or a list of stops of the journey from it's start to it's destination stop. If 'routeStops' and 'routeTimes' are both set, they contain the same number of elements. And elements with equal indices are associated (the times at which the vehicle is at the stops).</td></tr>
<tr><td><i>routeTimes</i></td> <td>QList< QTime > (stored as QVariantList)</td> <td>A list of times of the departure/arrival to it's destination stop. If 'routeStops' and 'routeTimes' are both set, they contain the same number of elements. And elements with equal indices are associated (the times at which the vehicle is at the stops).</td></tr>
<tr><td><i>routeExactStops</i></td> <td>int</td> <td>The number of exact route stops. The route stop list isn't complete from the last exact route stop.</td></tr>

</table>

<br />
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
also needed (otherwise it is ignored).<br />
The following parameters are allowed:<br />
<table>
<tr><td><i>originStop</i></td> <td>The name or ID of the origin stop.</td></tr>
<tr><td><i>targetStop</i></td> <td>The name or ID of the target stop.</td></tr>
<tr><td><i>city</i></td> <td>The city to get journeys for, if needed.</td></tr>
<tr><td><i>maxCount</i></td> <td>The maximum journey count to get.</td></tr>
<tr><td><i>timeOffset</i></td> <td>The offset in minutes from now for the first journey to get.</td></tr>
<tr><td><i>time</i></td> <td>The time for the first journey to get (in format "hh:mm").</td></tr>
<tr><td><i>datetime</i></td> <td>The date and time for the first journey to get (QDateTime::fromString() is used with default parameters to parse the date).</td></tr>
</table>
<br />

<b>Examples:</b><br />
<b>"Journeys de_db|originStop=Pappelstraße, Bremen|targetStop=Kirchweg, Bremen"</b><br />
Gets journeys from stop "Pappelstraße, Bremen" to stop "Kirchweg, Bremen"
using the service provider db.de.<br /><br />

<b>"Journeys de_db|originStop=Leipzig|targetStop=Hannover|timeOffset=5|maxCount=99"</b><br />
Gets journeys from stop "Leipzig" to stop "Hannover" using db.de, the first
possible journey departs in five minutes from now, the maximum journey count is 99.<br /><br />

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
<br />
The data received from the data engine always contains these keys:<br />
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
<br />
Each journey in the data received from the data engine (journeyData in the code
example) has the following keys:<br />
<i>vehicleTypes</i>: A QVariantList containing a list of vehicle types used in the journey. You can cast the list to QList<VehicleType> as seen in the code example (QVariantList).<br />
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

<br />
@section usage_stopList_sec Receiving Stop Lists
When you have requested departures, arrivals or journeys from the data engine, it may return a
list of stops, if the given stop name is ambiguous. To force getting a list of stop suggestions
use the data source <em>"Stops \<service-provider-id\>|stop=\<stop-name-part\>"</em>.

In your dataUpdated-slot you should first check, if a stop list was received by checking the value
of the key "receivedPossibleStopList" in the data object from the data engine. Then you get the
number of stops received, which is stored in the key "count". For each received stop there is a
key "stopName \<i\>", where i is the number of the stop, beginning with 0, ending with count - 1.
In these keys, there are QHash's with at least a <i>stopName</i> key (containing the stop name). It
<b>may</b> additionally contain a <i>stopID</i> (a non-ambiguous ID for the stop, if available,
otherwise it is empty), a <i>stopCity</i> (the city the stop is in) and a <i>stopCountryCode</i>
(the code of the country in with the stop is).

@code
void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data ) {
    if ( data.value("receivedPossibleStopList").toBool() ) {
    QStringList possibleStops;
    int count = data["count"].toInt();
    for( int i = 0; i < count; ++i ) {
        QHash<QString, QVariant> stopData = data.value( QString("stopName %1").arg(i) ).toHash();

        // Get the name
        QString stopName = dataMap["stopName"].toString();

        // Get other values
        if ( dataMap.contains("stopID") ) {
            QString stopID = dataMap["stopID"].toString();
        }
        QString stopCity = dataMap["stopCity"].toString();
        QString stopCityCode = dataMap["stopCountryCode"].toString();
    }
    }
}
@endcode
*/

/** @page pageServiceProviders Add Support for new Service Providers
@par Sections
    @li @ref provider_infos_xml
    @li @ref provider_infos_script
    @li @ref examples
    @li @ref examples_xml_script
    @li @ref examples_script
<br />

@section provider_infos_xml XML file structure
To add support for a new service provider you need to create a service provider plugin for the
PublicTransport engine, which is essentially an XML file with information about the service
provider. This XML file contains a name, description, changelog, etc. for the service provider
plugin.
It can also contain a reference to a script to parse documents from the provider to process
requests from the data engine. There are many helper functions available for scripts to parse HTML
documents, QtXML is available to parse XML documents (as extension).
The filename of the XML file starts with the country code or "international"/"unknown", followed
by "_" and a short name for the service provider, e.g. "de_db.pts", "ch_sbb.pts", "sk_atlas.pts",
"international_flightstats.pts". The base file name (without extension) is the service provider ID.
<br />
There is also a nice tool called <em>TimetableMate</em>. It's a little IDE to create service
provider plugins for the PublicTransport data engine. The GUI is similiar to the GUI of KDevelop,
it also has docks for projects, breakpoints, backtraces, variables, a console, script output and
so on. TimetableMate also shows a nice dashboard for the service provider plugin projects. It
features script editing, syntax checking, code-completion for the engine's script API, automatic
tests, web page viewer, network request/reply viewer with some filters, a Plasma preview etc.<br />

Here is an overview of the allowed tags in the XML file (required child tags of the serviceProvider
tag are <span style="background-color: #ffbbbb;">highlighted</span>):
<table>
<tr style="background-color: #bbbbbb; font-weight: bold;"><td>Tag</td> <td>Parent Tag</td> <td>Optional?</td> <td>Description</td></tr>

<tr style="background-color: #ffbbbb;"><td><b>\<serviceProvider\></b></td>
<td>Root</td> <td>Required</td> <td>This is the root item.</td></tr>

<tr><td><b>\<xml_file_version type="publictransport" version="1.0" /\></b></td>
<td>\<serviceProvider\></td> <td>Not used</td>
<td>This is currently not used by the data engine. But to be sure that the xml is parsed correctly you should add this tag. The version is the version of the xml file structure, current version is 1.0.</td></tr>

<tr style="background-color: #ffbbbb; border-top: 3px double black;"><td><b>\<name\></b></td>
<td>\<serviceProvider\></td> <td>Required</td>
<td>The name of the service provider (plugin). If it provides data for international stops it should begin with "International", if it's specific for a country or city it should begin with the name of that country or city. That should be followed by a short url to the service provider.</td></tr>

<tr style="background-color: #ffbbbb; border-top: 3px double black;"><td style="color:#888800"><b>\<author\></b></td>
<td>\<serviceProvider\></td> <td>Required</td>
<td>Contains information about the author of the service provider plugin.</td></tr>

<tr><td style="color:#666600"><b>\<fullname\></b></td>
<td style="color:#888800">\<author\></td> <td>Required</td>
<td>The full name of the author of the service provider plugin.</td></tr>

<tr><td style="color:#666600"><b>\<short\></b></td>
<td style="color:#888800">\<author\></td> <td>(Optional)</td>
<td>A short name for the author of the service provider plugin (eg. the initials).</td></tr>

<tr><td style="color:#666600"><b>\<email\></b></td>
<td style="color:#888800">\<author\></td> <td>(Optional)</td> <td>
The email address of the author of the service provider plugin.</td></tr>

<tr style="background-color: #ffbbbb; border-top: 3px double black;"><td><b>\<version\></b></td>
<td>\<serviceProvider\></td> <td>Required</td>
<td>The version of the service provider plugin, should start with "1.0".</td></tr>

<tr style="background-color: #ffbbbb; border-top: 3px double black;"><td><b>\<type\></b></td>
<td>\<serviceProvider\></td> <td>Required</td>
<td>Can be either HTML or XML.</td></tr>

<tr style="border-top: 3px double black;"><td style="color:#00bb00"><b>\<cities\></b></td>
<td>\<serviceProvider\></td> <td>(Optional)</td>
<td>A list of cities the service provider has data for (with surrounding \<city\>-tags).</td></tr>

<tr><td style="color:#007700"><b>\<city\></b></td>
<td style="color:#00bb00">\<cities\></td> <td>(Optional)</td>
<td>A city in the list of cities (\<cities\>). Can have an attribute "replaceWith", to replace city names with values used by the service provider.</td></tr>

<tr style="background-color: #ffbbbb; border-top: 3px double black;"><td><b>\<description\></b></td>
<td>\<serviceProvider\></td> <td>Required</td>
<td>A description of the service provider (plugin). You don't need to list the features supported by the service provider here, the feature list is generated automatically.</td></tr>

<tr style="background-color: #ffbbbb;"><td><b>\<url\></b></td>
<td>\<serviceProvider\></td> <td>Required</td>
<td>An url to the service provider home page.</td></tr>

<tr style="background-color: #ffbbbb;"><td><b>\<shortUrl\></b></td>
<td>\<serviceProvider\></td> <td>Required</td>
<td>A short version of the url, used as link text.</td></tr>

<tr style="background-color: #ffbbbb;"><td><b>\<fallbackCharset\></b></td>
<td>\<serviceProvider\></td> <td>Optional</td>
<td>The charset of documents to be downloaded (TODO do this from the script). Important if NetworkRequest::readyRead() gets used, because the correct codec can only be determined for completely downloaded HTML pages.</td></tr>

<tr style="background-color: #ffbbbb; border-top: 3px double black;"><td style="color:#bb0000;"><b>\<rawUrls\> DEPRECATED</b></td>
<td>\<serviceProvider\></td> <td>Required</td>
<td>Contains the used "raw urls". A raw url is a string with placeholders that are replaced with values to get a real url.</td></tr>

<tr><td style="color:#770000;"><b>\<departures\></b></td>
<td style="color:#bb0000;">\<rawUrls\> DEPRECATED</td> <td>Required</td>
<td>A raw url (in a CDATA tag) to a page containing a departure / arrival list. The following substrings are replaced by current values: <b>{stop}</b> (the stop name), <b>{type}</b> (arr or dep for arrivals or departures), <b>{time}</b> (the time of the first departure / arrival), <b>{maxCount}</b> (maximal number of departures / arrivals).</td></tr>

<tr><td style="color:#770000;"><b>\<journeys\></b></td>
<td style="color:#bb0000;">\<rawUrls\> DEPRECATED</td> <td>(Optional)</td>
<td>A raw url (in a CDATA tag) to a page containing a journey list. The following substrings are replaced by current values: <b>{startStop}</b> (the name of the stop where the journey starts), <b>{targetStop}</b> (the name of the stop where the journey ends), <b>{time}</b> (the time of the first journey), <b>{maxCount}</b> (maximal number of journeys).</td></tr>

<tr><td style="color:#770000;"><b>\<stopSuggestions\></b></td>
<td style="color:#bb0000;">\<rawUrls\> DEPRECATED</td> <td>(Optional)</td>
<td>A raw url (in a CDATA tag) to a page containing a list of stop suggestions. Normally this tag isn't needed, because the url is the same as the url to the departure list. When the stop name is ambiguous the service provider can show a page containing a list of stop suggestions. You may want to use this tag if you want to parse XML files for departure lists and get the stop suggestions from an HTML page or if there is a special url only for stop suggestions.</td></tr>

<tr style="background-color: #ffbbbb; border-top: 3px double black;"><td><b>\<script\></b></td>
<td>\<serviceProvider\></td> <td>Required, if no regExps are set</td>
<td>Contains the filename of the script to be used to parse timetable documents. The script must be in the same directory as the XML file. Always use "Script" as type when using a script.</td></tr>

<tr style="border-top: 3px double black;"><td style="color:#00bb00;"><b>\<changelog\></b></td>
<td>\<serviceProvider\></td> <td>(Optional)</td>
<td>Contains changelog entries for this service provider plugin.</td></tr>

<tr><td style="color:#007700;"><b>\<entry\></b></td>
<td style="color:#00bb00;">\<changelog\></td> <td>(Optional)</td>
<td>Contains a changelog entry for this service provider plugin. The entry description is read from the contents of the \<entry\> tag. Attributes <em>"version"</em> (the plugin version where this change was applied) and <em>"engineVersion"</em> (the version of the publictransport data engine this plugin was first released with) can be added.</td></tr>

<tr style="border-top: 3px double black;"><td style="color:#880088;"><b>\<sessionKey\></b></td>
<td>\<serviceProvider\></td> <td>(Optional)</td>
<td>Contains information about usage of session keys, if required by the service provider.</td></tr>

<tr><td style="color:#660066;"><b>\<url\></b></td>
<td style="color:#880088;">\<sessionKey\></td> <td>Required</td>
<td>Contains the URL to a document containing a session key.</td></tr>

<tr><td style="color:#660066;"><b>\<putInto\></b></td>
<td style="color:#880088;">\<sessionKey\></td> <td>(Optional)</td>
<td>The name of a place where to put the session key in requests (currently only "CustomHeader" is supported). May contain a <em>"data"</em> attribute, which is used for "CustomHeader" as name of the custom HTTP header.</td></tr>

</table>
@n @n

@section provider_infos_script Script file structure
Scripts are executed using Kross, which supports JavaScript, Python and Ruby. JavaScript is tested, the other languages may also work.
There are functions with special names that get called by the data engine when needed:
<ul>
    <li>
        <b>usedTimetableInformations():</b> Should return a list of strings representing supported TimetableInformations. The following strings are currently used to determine special features of the service provider: "Delay", "DelayReason", "Platform", "JourneyNews", "JourneyNewsOther", "JourneyNewsLink", "TypeOfVehicle", "Status", "Operator", "StopID". Example: <em>return [ 'TypeOfVehicle', 'Platform', 'Delay', 'StopID' ];</em>
    </li>

    <li>
        <b>parseTimetable( document ):</b> Used to parse departure/arrival documents. The content of the document is given as argument. Use the <em>timetableData</em> object to set values of departures/arrivals, eg. <em>timetableData.set( 'Target', targetString )</em>. Add a departure to the result set using <em>result.addData( timetableData )</em> (after setting all values). Before setting the values of the next departure/arrival you should call <em>timetableData.clear()</em>, otherwise the new departure will contain values of previous departures, that weren't overwritten with new values.
    </li>

    <li>
        <b>parsePossibleStops( document ):</b> Used to parse stop suggestion documents. The content of the document is given as argument. This works similiar to <em>parseTimetable</em>. You can set these values for stop suggestions: "StopName" (required), "StopID", "StopWeight", "StopCity" and "StopCountryCode". Example: <em>timetableData.set( 'StopName', 'Testname' );</em>
        </li>

    <li>
        <b>parseJourneys( document ):</b> Used to parse journey documents. The content of the document is given as argument. Use the <em>timetableData</em> object to set values of journeys, just like with <em>parseTimetable</em>. Add a journey to the result set using <em>result.addData( timetableData )</em> (after setting all values).
    </li>

    <li>
        <b>getUrlForLaterJourneyResults( document ):</b> Used to parse the URL to get later journeys from a given journey document. The returned URL is used to download another journey document and send it to <em>parseJourneys</em>.
    </li>

    <li>
        <b>parseSessionKey( document ):</b> Used to parse a session key from the given document, if the service provider requires it. Just return the session key string.
    </li>
</ul>

@n @n
@section examples Service Provider Plugin Examples
@n
@subsection examples_xml_script A Simple Service Provider Plugin
Here is an example of a simple service provider plugin which uses a script to parse data from the service provider:
@verbinclude fr_gares.pts

@n
@subsection examples_script A Simple Parsing-Script
This is an example of a script used to parse data from the service provider (actually the one used by the XML from the last section).
@verbinclude fr_gares.js
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

    providerScript [
    label="{ServiceProviderScript|Parses timetable documents, eg. HTML.\l|# requestDepartures() : bool\l# requestStopSuggestions() : bool\l}"
    URL="\ref ServiceProviderScript"
    ];

    provider [
    label="{ServiceProvider|Loads and parses documents from service providers.\lIt uses ServiceProviderData to get needed information.\l|+ static getSpecificProvider( serviceProvider ) : ServiceProvider*\l+ requestDepartures() : KIO:TransferJob\l+ requestJourneys() : KIO:TransferJob\l+ data() : ServiceProviderData\l}"
    URL="\ref ServiceProvider"
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

    subgraph cluster_subServiceProviderData {
    label="Service Provider Data";
    style="rounded, filled";
    color="#ccffcc";

    node [ fillcolor="#dfffdf" ];

    serviceProviderData [
    label="{ServiceProviderData|Information about where to download the documents \land how to parse them.\l|+ name() : QString\l+ stopSuggestionsRawUrl() : QString\l+ departureRawUrl() : QString\l+ journeyRawUrl() : QString\l+ searchDepartures() : TimetableRegExpSearch\l+ searchJourneys() : TimetableRegExpSearch\l+ searchDeparturesPre() : TimetableRegExpSearch*\l+ regExpSearchPossibleStopsRanges() : QStringList\l+ searchPossibleStops() : QList\<TimetableRegExpSearch\>\l+ searchJourneyNews() : QList\<TimetableRegExpSearch\>\l+ features() : QStringList\l... }"
    URL="\ref ServiceProviderData"
    ];
     }

   // { rank=same; provider pubTransInfo providerScript serviceProviderData regExpSearch depInfo journeyInfo }
   { rank=same; provider providerScript }

    edge [ arrowhead="empty", arrowtail="none", style="solid" ];
    providerScript -> provider;

    edge [ dir=back, arrowhead="none", arrowtail="empty", style="solid" ];
    pubTransInfo -> depInfo;
    pubTransInfo -> journeyInfo;

    edge [ dir=back, arrowhead="normal", arrowtail="none", style="dashed", fontcolor="gray", taillabel="", headlabel="0..*" ];
    engine -> provider [ label=""uses ];
    provider -> pubTransInfo [ label="uses" ];
    provider -> serviceProviderData [ label="uses" ];
    serviceProviderData -> regExpSearch [ label="uses" ];

    edge [ dir=both, arrowhead="normal", arrowtail="normal", color="gray", fontcolor="gray", style="dashed", headlabel="" ];
    serviceProviderData -> provider [ label="friend" ];
}
@enddot
*/

#endif

struct JourneyRequest;
