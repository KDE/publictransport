/*
*   Copyright 2013 Friedrich Pülz <fpuelz@gmx.de>
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

/** @mainpage Public Transport Data Engine
@section intro_dataengine_sec Introduction
The public transport data engine provides timetable data for public transport, trains, ships,
ferries and planes. It can get departures/arrivals, journeys and stop suggestions.
There are different plugins (eg. scripts) used to get timetable data from the different
service providers. Currently there are two classes of service providers: One uses scripts to
do the work, the other one uses GTFS (GeneralTransitFeedSpecification). All are using information
from ServiceProviderData, which reads information data from "*.pts" files (XML files with mime type
"application-x-publictransport-serviceprovider").

<br />
@section install_sec Installation
To install this data engine type the following commands:<br />
@verbatim
> cd /path-to-extracted-engine-sources/build
> cmake -DCMAKE_INSTALL_PREFIX=`kde4-config --prefix` ..
> make
> make install
@endverbatim
@note Do not forget the ".." at the end of the second line!

After installation do the following to use the data engine in your plasma desktop:
Restart plasma to load the data engine:<br />
@verbatim
> kquitapp plasma-desktop
> plasma-desktop
@endverbatim
<br />
or test it with:<br />
@verbatim
> plasmaengineexplorer --engine publictransport
@endverbatim
<br />
You might need to run kbuildsycoca4 in order to get the .desktop file recognized.
<br />

<br />

@section index_sec Other Pages
@par
    @li @ref pageUsage
    @li @ref pageServiceProviders
    @li @ref pageClassDiagram
    @li @ref pageGlossary
*/

/** @page pageUsage Usage of Data Sources and Services

@par Sections
<ul><li>@ref usage_introduction_sec </li>
    <li>@ref usage_serviceproviders_sec </li>
    <li>@ref usage_vehicletypes_sec </li>
    <li>@ref usage_departures_sec </li>
        <ul><li>@ref usage_departures_datastructure_sec </li></ul>
    <li>@ref usage_journeys_sec </li>
        <ul><li>@ref usage_journeys_datastructure_sec </li></ul>
    <li>@ref usage_stopList_sec </li>
        <ul><li>@ref usage_stopList_datastructure_sec </li></ul>
    <li>@ref usage_service_sec </li>
        <ul><li>@ref usage_service_manual_update </li>
            <li>@ref usage_service_additional_data </li>
            <li>@ref usage_service_later_items </li>
        </ul>
    <li>@ref usage_gtfs_service_sec </li>
        <ul><li>@ref usage_gtfs_service_import </li>
            <li>@ref usage_gtfs_service_update </li>
            <li>@ref usage_gtfs_service_delete </li>
            <li>@ref usage_gtfs_service_info </li>
        </ul>
</ul>
<br />

@section usage_introduction_sec Introduction
To use this data engine in an applet you need to connect it to a data source of the public
transport data engine. There are data sources which provide information about the available
service providers (@ref usage_serviceproviders_sec) or supported countries. Other data sources
contain departures/arrivals (@ref usage_departures_sec), journeys (@ref usage_journeys_sec) or
stop suggestions (@ref usage_stopList_sec).<br />
The engine provides services for all timetable data sources (departures, journeys, ...). It offers
operations to manually request updates, request earlier/later timetable items or to request
additional data for specific timetable items (@ref usage_service_sec).<br />
@note Since version 0.11 the engine will only match data source names with correct case, ie.
"serviceproViders" will not work any longer, but "ServiceProviders" will. All parameter names in
data source names need to be completely lower case. This is to prevent ambiguities, each variant
would get it's own data source object in the data engine, duplicating the data. To update a data
source all connected source name variants would need to be updated. Only accepting source names
case sensitive makes it much easier for the engine. The only thing left that can make two identic
data sources ambiguous is their parameter order, which gets handled using disambiguateSourceName().
<br />
The following enumeration can be used in your applet if you don't want to use
libpublictransporthelper which exports this enumaration as @ref PublicTransport::VehicleType.
Don't change the numbers, as they need to match the ones in the data engine, which uses a similar
enumeration.
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
    TrolleyBus = 6, // A trolleybus (also known as trolley bus, trolley coach, trackless trolley,
            // trackless tram or trolley) is an electric bus that draws its electricity from
            // overhead wires (generally suspended from roadside posts) using spring-loaded
            // trolley poles

    RegionalTrain = 10, // The vehicle is a regional train
    RegionalExpressTrain = 11, // The vehicle is a region express
    InterregionalTrain = 12, // The vehicle is an interregional train
    IntercityTrain = 13, // The vehicle is a intercity / eurocity train
    HighspeedTrain = 14, // The vehicle is an intercity express (ICE, TGV?, ...?)

    Ferry = 100, // The vehicle is a ferry
    Ship = 101, // The vehicle is a ship

    Plane = 200 // The vehicle is an aeroplane
};
@endcode

<br />
@section usage_serviceproviders_sec Receiving a List of Available Service Providers
You can view this data source in @em plasmaengineexplorer, it's name is
@em "ServiceProviders", but you can also use <em>"ServiceProvider [providerId]"</em> with the ID
of a service provider to get information only for that service provider.
For each available service provider the data source contains a key with the display name of the
service provider. These keys point to the service provider information, stored as a QHash with
the following keys:
<br />
<table>
<tr><td><i>id</i></td> <td>QString</td> <td>The ID of the service provider plugin.</td></tr>
<tr><td><i>fileName</i></td> <td>QString</td> <td>The file name of the XML file containing the
service provider information.</td> </tr>
<tr><td><i>name</i></td> <td>QString</td> <td>The name of the service provider.</td></tr>
<tr><td><i>type</i></td> <td>QString</td> <td>The type of the provider plugin, may currently be
"GTFS", "Scripted" or "Invalid".</td></tr>
<tr><td><i>feedUrl</i></td> <td>QString</td> <td><em>(only for type "GTFS")</em> The url to the
(latest) GTFS feed.</td></tr>
<tr><td><i>scriptFileName</i></td> <td>QString</td> <td><em>(only for type "Scripted")</em>
The file name of the script used to parse documents from the service provider, if any.</td></tr>
<tr><td><i>url</i></td> <td>QString</td>
<td>The url to the home page of the service provider.</td></tr>
<tr><td><i>shortUrl</i></td> <td>QString</td> <td>A short version of the url to the home page
of the service provider. This can be used to display short links, while using "url" as the url
of that link.</td></tr>
<tr><td><i>country</i></td> <td>QString</td> <td>The country the service provider is (mainly)
designed for.</td></tr>
<tr><td><i>cities</i></td> <td>QStringList</td>
<td>A list of cities the service provider supports.</td></tr>
<tr><td><i>credit</i></td> <td>QString</td>
<td>The ones who run the service provider (companies).</td></tr>
<tr><td><i>useSeparateCityValue</i></td> <td>bool</td> <td>Wheather or not the service provider
needs a separate city value. If this is @c true, you need to specify a "city" parameter in data
source names for @ref usage_departures_sec and @ref usage_journeys_sec .</td></tr>
<tr><td><i>onlyUseCitiesInList</i></td> <td>bool</td> <td>Wheather or not the service provider
only accepts cities that are in the "cities" list.</td></tr>
<tr><td><i>features</i></td> <td>QStringList</td> <td>A list of strings, each string stands
for a feature of the service provider.</td></tr>
<tr><td><i>featureNames</i></td> <td>QStringList</td> <td>A list of localized names describing
which features the service provider offers.</td></tr>
<tr><td><i>author</i></td> <td>QString</td> <td>The author of the service provider plugin.</td></tr>
<tr><td><i>email</i></td> <td>QString</td> <td>The email address of the author of the
service provider plugin.</td></tr>
<tr><td><i>description</i></td> <td>QString</td>
<td>A description of the service provider.</td></tr>
<tr><td><i>version</i></td> <td>QString</td>
<td>The version of the service provider plugin.</td></tr>

<tr><td><i>error</i></td> <td>bool</td> <td>Whether or not the provider plugin has errors.
If this is @c true, the other fields except @em id may not be available (invalid provider .pts
file), but a field @em errorMessage is available explaining the error. If this if @c false, the
provider did not encounter any errors. But the provider may still not be ready to use, if the
@em state field contains a state string other than @em "ready". If no @em state field is available,
the provider can also be considered to be ready.</td></tr>

<tr><td><i>errorMessage</i></td> <td>QString</td> <td>A string explaining the error,
only available if @em error is @c true. </td></tr>

<tr><td><i>state</i></td> <td>QString</td>
<td>A string to identify different provider states. Currently these states are available:
@em "ready" (the provider is ready to use), @em "gtfs_import_pending" (a GTFS
provider is waiting for the GTFS feed to get imported), @em "importing_gtfs_feed" (a GTFS
provider currently downloads and imports it's GTFS feed).<br />
A provider can only be used to query for departures/arrivals, etc. if it's state is @em "ready"
and @em error is @c false.
</td></tr>

<tr><td><i>stateData</i></td> <td>QVariantHash</td> <td>Contains more information about the
current provider state in a QVariantHash. At least a @em statusMessage field is contained,
with a human readable string explaining the current state. There may also be a
@em statusMessageRich field with a formatted version of @em statusMessage.
Depending on the @em state additional fields may be available.
<br />
For example with the @em "importing_gtfs_feed" state a field @em progress is available.
GTFS providers of any state offer these fields as state data:
@em gtfsFeedImported (whether or not the GTFS feed was successfully imported, see
@ref usage_gtfs_service_import), @em updatable (whether or not an updated version of the GTFS feed
is available, see @ref usage_gtfs_service_update), @em gtfsFeedSize (the size in bytes of the GTFS
feed) and @em gtfsFeedModifiedTime (the last modified time of the GTFS feed as Qt::IsoDate string).
Note that if @em gtfsFeedSize is -1 this information needs to be requested first using the GTFS
service, see @ref usage_gtfs_service_info.
GTFS providers that are @em "ready" also offer these fields as state data:
@em gtfsDatabasePath (the path to the GTFS database file), @em gtfsDatabaseSize (the size in bytes
of the GTFS database) and @em gtfsDatabaseModifiedTime (the last modified time of the GTFS
database as Qt::IsoDate string).</td></tr>
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
    // The name is already available in serviceProviderName
    QString name = serviceProviderData["name"].toString();
    QString country = serviceProviderData["country"].toString();
    QStringList features = serviceProviderData["features"].toStringList();
    bool useSeparateCityValue = serviceProviderData["useSeparateCityValue"].toBool();
    QString state = serviceProviderData["state"].toString();
}
@endcode

There is also data source named <em>"ServiceProvider \<country-code|service-provider-id\>"</em>
to get information about the default service provider for the given country or about the given
provider with the given ID.

<br />
@section usage_vehicletypes_sec Receiving Information About Available Vehicle Types
Information about all available vehicle types can be retrieved from the data source
@em "VehicleTypes". It stores vehicle type information by vehicle type ID, which matches
the values of the Enums::VehicleType (engine) and PublicTransport::VehicleType
(libpublictransporthelper) enumerations. The information stored in this data source can also be
retrieved from PublicTransport::VehicleType using libpublictransporthelper, ie. the static
functions of PublicTransport::Global.
For each vehicle type there are the following key/value pairs:
<br />
<table>
<tr><td><i>id</i></td> <td>QString</td> <td>An untranslated unique string identifying the vehicle
type. These strings match the names of the Enums::VehicleType enumerables.</td> </tr>
<tr><td><i>name</i></td> <td>QString</td> <td>The translated name of the vehicle type.</td></tr>
<tr><td><i>namePlural</i></td> <td>QString</td> <td>Like <i>name</i> but in plural form.</td></tr>
<tr><td><i>iconName</i></td> <td>QString</td> <td>The name of the icon associated with the
vehicle type. This name can be used as argument for the QIcon contructor.</td></tr>
</table>
<br />
Here is an example of how to get more information for a specific vehicle type by it's (integer) ID:
@code
// This value can be retrieved from eg. a departures data source
const int vehicleType;

// Query the data engine for information about all available vehicle types
Plasma::DataEngine::Data allVehicleTypes = dataEngine("publictransport")->query("VehicleTypes");

// Extract the information
const QVariantHash vehicleData = allVehicleTypes[ QString::number(vehicleType) ].toHash();
QString id = vehicleData["id"].toString();
QString name = vehicleData["name"].toString();
QString namePlural = vehicleData["namePlural"].toString();
KIcon icon = KIcon( vehicleData["iconName"].toString() );
@endcode

<br />
@section usage_departures_sec Receiving Departures or Arrivals
To get a list of departures/arrivals you need to construct the name of the data source. For
departures it begins with "Departures", for arrivals it begins with "Arrivals". Next comes a
space (" "), then the ID of the service provider to use, e.g. "de_db" for a service provider
for germany ("Deutsche Bahn"). The following parameters are separated by "|" and start with the
parameter name followed by "=" and the value. The sorting of the additional parameters doesn't
matter. One of the parameters @em stopid and @em stop is required. If the service provider has
useSeparateCityValue set to @c true (see @ref usage_serviceproviders_sec), the parameter @em city
is also required (otherwise it is ignored). You can leave the service provider ID away, the data
engine then uses the default service provider for the users country.<br />
@note All parameter names need to be completely lower case.
The following parameters are allowed:<br />
<table>
<tr><td>@em stopid </td> <td>The ID of the stop to get departures/arrivals for. This is preferred
over @em stop. You can retrieve the stop ID for a stop name (part) using the stop suggestions data
source, see @ref usage_stopList_sec. </td></tr>
<tr><td>@em stop </td> <td>The name of the stop to get departures/arrivals for. Use this only if
no ID is available.</td></tr>
<tr><td>@em city </td> <td>The city to get departures/arrivals for, if needed.</td></tr>
<tr><td>@em count </td> <td>The number of departures/arrivals to get.
 @note This is just a hint for the provider.</td></tr>
<tr><td>@em timeoffset </td> <td>The offset in minutes from now for the first departure /
arrival to get.</td></tr>
<tr><td>@em time </td> <td>The time of the first departure/arrival to get ("hh:mm"). This uses
the current date. To use another date use 'datetime'.</td></tr>
<tr><td>@em datetime </td> <td>The date and time of the first departure/arrival to get (use
QDateTime::toString()).</td></tr>
</table>
<br />

<b>Examples:</b><br />
<b>"Departures de_db|stopid=000776455"</b><br />
Gets departures for the stop with the ID "000776455" using the service provider de_db.<br /><br />

<b>"Departures de_db|stop=Pappelstraße, Bremen"</b><br />
Gets departures for the stop "Pappelstraße, Bremen" using the service provider de_db.
If possible use a stop ID instead of a stop name (@em stopid parameter, like in the previous
example).<br /><br />

<b>"Arrivals de_db|stop=Leipzig|timeoffset=5|count=99"</b><br />
Gets arrivals for the stop "Leipzig" using de_db, the first possible arrival is in five minutes
from now, the number of arrivals to request is 99.<br /><br />

<b>"Departures de_rmv|stop=Frankfurt (Main) Speyerer Straße|time=08:00"</b><br />
Gets departures for the stop "Frankfurt (Main) Speyerer Straße" using de_rmv, the first possible
departure is at eight o'clock.<br /><br />

<b>"Departures de_rmv|stopid=3000019|count=20|timeoffset=1"</b><br />
Gets departures for the stop with the ID "3000019", the first possible departure is in one minute
from now, the number of departures to request is 20.<br /><br />

<b>"Departures stop=Hauptbahnhof"</b><br />
Gets departures for the stop "Hauptbahnhof" using the default service provider for the users
country, if there is one. Using a stop ID without specifying a provider is not recommended,
because IDs are provider specific.<br /><br />

Once you have the data source name, you can connect your applet to that data source from the data
engine. Here is an example of how to do this:
@code
class Applet : public Plasma::Applet {
public:
    Applet(QObject *parent, const QVariantList &args) : AppletWithState(parent, args) {
        dataEngine("publictransport")->connectSource( "Departures de_db|stop=Köln, Hauptbahnhof",
                                                      this, 60 * 1000 );
    };

public slots:
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data ) {
        if ( data.value("error").toBool() ) {
            // Handle errors
        } else if ( data.contains("stops") ) {
            // Possible stop list received, because the given stop name is ambiguous
            // See section "Receiving Stop Lists"
        } else {
            // Departures / arrivals received.
            QVariantList departures = data.contains("departures")
                    ? data["departures"].toList() : data["arrivals"].toList();

            foreach ( const QVariant &departureData, departures ) {
                QHash<QString, QVariant> departure = departureData.toHash();
                QString line = departure["TransportLine"].toString();
                // For arrival lists this is the origin
                QString target = departure["Target"].toString();
                QDateTime departure = departure["DepartureDateTime"].toDateTime();
            }
        }
    };
};
@endcode
<br />
@subsection usage_departures_datastructure_sec Departure Data Source Structure
The data received from the data engine always contains these keys:<br />
<table>
<tr><td><i>error</i></td> <td>bool</td> <td>True, if an error occurred while parsing.</td></tr>
<tr><td><i>errorMessage</i></td> <td>QString</td> <td>(only if @em error is @c true),
an error message string.</td></tr>
<tr><td><i>errorCode</i></td> <td>int</td> <td>(only if @em error is @c true), an error code.
    Error code 1 means, that there was a problem downloading a source file.
    Error code 2 means, that parsing a source file failed.
    Error code 3 means that a GTFS feed needs to be imorted into the database before using it.
    See @ref usage_gtfs_service_import to start and monitor the import.</td></tr>
<tr><td><i>receivedData</i></td> <td>QString</td> <td>"departures", "journeys", "stopList" or
"nothing" if there was an error.</td></tr>
<tr><td><i>updated</i></td> <td>QDateTime</td> <td>The date and time when the data source was
last updated.</td></tr>
<tr><td><i>nextAutomaticUpdate</i></td> <td>QDateTime</td> <td>The date and time of the next
automatic update of the data source.</td></tr>
<tr><td><i>minManualUpdateTime</i></td> <td>QDateTime</td> <td>The minimal date and time to request
an update using the "requestUpdate" operation of the timetable service.</td></tr>
<tr><td><i>departures</i> or <i>arrivals</i></td> <td>QVariantList</td>
<td>A list of all found departures/arrivals.</td></tr>
</table>
<br />

Each departure/arrival in the data received from the data engine (departureData in the code
example) has the following keys:<br />
<table>
<tr><td><i>additionalDataState</i></td> <td>QString</td>
<td>A string to identify different states of additional data for this item. Currently these states
are available: @em "notsupported" (the provider does not support any additional data),
@em "notrequested" (additional data was not yet requested, but the provider supports it),
@em "included" (additional data is included for this item, ie. in the following fields),
@em "busy" (additional data was requested but is not yet ready), @em "error" (there was an error
when getting additional data for this item, see @em additionalDataError. </td></tr>
<tr><td><i>additionalDataError</i></td> <td>QString</td> <td>An error message if
@em additionalDataState is @em "error" , otherwise an empty string. </td></tr>
</table>

The actual data of the timetable items is stored in fields named after the enumerables in
Enums::TimetableInformation. A departure/arrival at least has a @em "DepartureDateTime",
a @em "VehicleType" and a @em "Target" field.

@note The service provider may not load all data by default. To load missing data
  ("additional data"), use the timetable service's operation "requestAdditionalData",
  @ref usage_service_sec.

<br />
@section usage_journeys_sec Receiving Journeys from A to B
To get a list of journeys from one stop to antoher you need to construct the name of the data
source (much like the data source for departures / arrivals). The data source name begins with
"Journeys". Next comes a space (" "), then the ID of the service provider to use, e.g. "de_db"
for a service provider for germany ("Deutsche Bahn").
The following parameters are separated by "|" and start with the parameter name followed by "=".
The sorting of the additional parameters doesn't matter. The parameters @em originstopid or
@em originstop and @em targetstopid or @em targetstop are required. If the service provider
has useSeparateCityValue set to @c true (see @ref usage_serviceproviders_sec), the parameter
@em city is also needed (otherwise it is ignored).<br />
@note All parameter names need to be completely lower case.
The following parameters are allowed:<br />
<table>
<tr><td>@em originstopid </td> <td>The ID of the origin stop. This is preferred over
@em originstop. You can retrieve the stop ID for a stop name (part) using the stop suggestions
data source, see @ref usage_stopList_sec. </td></tr>
<tr><td>@em originstop </td> <td>The name of the origin stop.</td></tr>
<tr><td>@em targetstopId </td> <td>The ID of the target stop. This is preferred over
@em targetstop. </td></tr>
<tr><td>@em targetstop </td> <td>The name of the target stop.</td></tr>
<tr><td>@em city </td> <td>The city to get journeys for, if needed.</td></tr>
<tr><td>@em count </td> <td>The number of journeys to get.
 @note This is just a hint for the provider</td></tr>
<tr><td>@em timeoffset </td>
<td>The offset in minutes from now for the first journey to get.</td></tr>
<tr><td>@em time </td> <td>The time for the first journey to get (in format "hh:mm").</td></tr>
<tr><td>@em datetime </td> <td>The date and time for the first journey to get
(QDateTime::fromString() is used with default parameters to parse the date).</td></tr>
</table>
<br />

<b>Examples:</b><br />
<b>"Journeys de_db|originstop=000776455|targetstop=000776465"</b><br />
Gets journeys from the stop with the ID "000776455" to the stop with the ID "000776465"
using the service provider de_db.<br /><br />

<b>"Journeys de_db|originstop=Pappelstraße, Bremen|targetstop=Kirchweg, Bremen"</b><br />
Gets journeys from stop "Pappelstraße, Bremen" to stop "Kirchweg, Bremen"
using the service provider de_db.
If possible use stop IDs instead of stop names (like in the previous example).<br /><br />

<b>"Journeys de_db|originstop=Leipzig|targetstop=Hannover|timeoffset=5|count=99"</b><br />
Gets journeys from stop "Leipzig" to stop "Hannover" using de_db, the first
possible journey departs in five minutes from now, the maximum journey count is 99.<br /><br />

Once you have the data source name, you can connect your applet to that
data source from the data engine. Here is an example of how to do this:
@code
class Applet : public Plasma::Applet {
public:
    Applet(QObject *parent, const QVariantList &args) : AppletWithState(parent, args) {
        dataEngine("publictransport")->connectSource(
                "Journeys de_db|originstop=Pappelstraße, Bremen|targetstop=Kirchweg, Bremen",
                this, 60 * 1000 );
    };

public slots:
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data ) {
        if ( data.value("error").toBool() ) {
            // Handle errors
        } else if ( data.contains("stops") ) {
            // Possible stop list received, because the given stop name is ambiguous
            // See section "Receiving Stop Lists"
        } else {
            // Journeys received.
            QVariantList journeysData = data["journeys"].toList();
            foreach ( const QVariant &journeyData, journeysData ) {
                QHash<QString, QVariant> journey = journeyData.toHash();

                // Get vehicle type list
                QVariantList = journey["TypesOfVehicleInJourney"].toList();
                QList< PublicTransport::VehicleType > vehicleTypes;
                foreach( QVariant vehicleType, vehicleTypesVariant ) {
                    vehicleTypes.append(
                            static_cast< PublicTransport::VehicleType >( vehicleType.toInt() ) );
                }

                QString target = journey["StartStopName"].toString();
                QDateTime departure = journey["DepartureDateTime"].toDateTime();
                int duration = journey["Duration"].toInt(); // Duration in minutes
            }
        }
    };
};
@endcode
<br />
@subsection usage_journeys_datastructure_sec Journey Data Source Structure
The data received from the data engine always contains these keys:<br />
<table>
<tr><td><i>error</i></td> <td>bool</td> <td>True, if an error occurred while parsing.</td></tr>
<tr><td><i>errorMessage</i></td> <td>QString</td> <td>(only if @em error is @c true),
an error message string.</td></tr>
<tr><td><i>errorCode</i></td> <td>int</td> <td>(only if @em error is @c true), an error code.
    Error code 1 means, that there was a problem downloading a source file.
    Error code 2 means, that parsing a source file failed.
    Error code 3 means that a GTFS feed needs to be imorted into the database before using it.
    Use the @ref PublicTransportService to start and monitor the import.</td></tr>
<tr><td><i>updated</i></td> <td>QDateTime</td> <td>The date and time when the data source was
last updated.</td></tr>
<tr><td><i>journeys</i></td> <td>QVariantList</td> <td>A list of all found journeys.</td></tr>
</table>
<br />
Each journey in the data received from the data engine (journeyData in the code
example) has the following keys:<br />
<i>vehicleTypes</i>: A QVariantList containing a list of vehicle type ID's (integers) of vehicles
used in the journey. You can cast the list to QList<PublicTransport::VehicleType> as seen in the
code example above (QVariantList), if you use libpublictransporthelper. Alternatively the
"VehicleTypes" data source can be used to get more information about the vehicle types.
See also @ref usage_vehicletypes_sec. <br />
<table>
<tr><td><i>ArrivalDateTime</i></td> <td>QDateTime</td> <td>The date and time of the arrival
at the target stop.</td></tr>
<tr><td><i>DepartureDateTime</i></td> <td>QDateTime</td> <td>The date and time of the departure
from the origin stop.</td></tr>
<tr><td><i>Duration</i></td> <td>int</td> <td>The duration in minutes of the journey.</td></tr>
<tr><td><i>Changes</i></td> <td>int</td>
<td>The changes between vehicles needed for the journey.</td></tr>
<tr><td><i>Pricing</i></td> <td>QString</td>
<td>Information about the pricing of the journey.</td></tr>
<tr><td><i>JourneyNews</i></td> <td>QString</td> <td>News for the journey.</td></tr>
<tr><td><i>StartStopName</i></td> <td>QString</td> <td>The name or ID of the origin stop.</td></tr>
<tr><td><i>TargetStopName</i></td> <td>QString</td>
<td>The name or ID of the target stop (QString).</td></tr>
<tr><td><i>Operator</i></td> <td>QString</td>
<td>The company that is responsible for the journey.</td></tr>
<tr><td><i>RouteStops</i></td> <td>QStringList</td> <td>A list of stops of the journey
from it's start to it's destination stop. If 'routeStops' and 'routeTimes' are both set,
they contain the same number of elements. And elements with equal indices are associated
(the times at which the vehicle is at the stops).</td></tr>
<tr><td><i>RouteNews</i></td> <td>QStringList</td> <td>A list of news/comments for sub-journeys.
If 'RouteStops' and 'RouteNews' are both set, the latter contains one element less
(one news/comment string for each sub-journey between two stop from 'RouteStops').</td></tr>
<tr><td><i>RouteTimesDeparture</i></td> <td>QList< QTime > (stored as QVariantList)</td>
<td>A list of departure times of the journey to it's destination stop. If 'routeStops' and
'routeTimesDeparture' are both set, the latter contains one element less (because the last stop
has no departure, only an arrival time). Elements with equal indices are associated
(the times at which the vehicle departs from the stops).</td></tr>
<tr><td><i>RouteTimesArrival</i></td> <td>QList< QTime > (stored as QVariantList)</td>
<td>A list of arrival times of the journey to it's destination stop. If 'routeStops' and
'routeTimesArrival' are both set, the latter contains one element less (because the last stop
has no departure, only an arrival time). Elements with equal indices are associated
(the times at which the vehicle departs from the stops).</td></tr>
<tr><td><i>RouteExactStops</i></td> <td>int</td> <td>The number of exact route stops.
The route stop list isn't complete from the last exact route stop.</td></tr>
<tr><td><i>RouteTypesOfVehicles</i></td> <td>QList< int > (stored as QVariantList)</td>
<td>A list of vehicle type ID's (integers) of vehicles used for each "sub-journey" in the journey.
See the <i>vehicleTypes</i> field or @ref usage_vehicletypes_sec for more information. </td></tr>
<tr><td><i>RouteTransportLines</i></td> <td>QStringList</td>
<td>A list of transport lines used for each "sub-journey" in the journey.</td></tr>
<tr><td><i>RoutePlatformsDeparture</i></td> <td>QStringList</td>
<td>A list of platforms of the departure used for each stop in the journey.</td></tr>
<tr><td><i>RoutePlatformsArrival</i></td> <td>QStringList</td>
<td>A list of platforms of the arrival used for each stop in the journey.</td></tr>
<tr><td><i>RouteTimesDepartureDelay</i></td> <td>QList< int > (stored as QVariantList)</td>
<td>A list of delays in minutes of the departures at each stop in the journey. A value of 0 means,
that the vehicle is on schedule, -1 means, that there's no information about delays.</td></tr>
<tr><td><i>RouteTimesArrivalDelay</i></td> <td>QList< int > (stored as QVariantList)</td>
<td>A list of delays in minutes of the arrivals at each stop in the journey. A value of 0 means,
that the vehicle is on schedule, -1 means, that there's no information about delays.</td></tr>
<tr><td><i>RouteSubJourneys</i></td> <td>QList< QVariantMap ></td> <td>A list of data maps for
all sub-journeys between two connecting stops. If 'RouteStops' and 'RouteSubJourneys' are both set,
the latter contains one element less (one sub-journey between two stops from 'RouteStops').
Each map in the list contains route data for the sub journey. These TimetableInformation values
can be used inside this map: RouteStops, RouteNews, RouteTimesDeparture, RouteTimesArrival,
RouteTimesDepartureDelay, RouteTimesArrivalDelay, RoutePlatformsDeparture and RoutePlatformsArrival.
Each list should contain the same number of elements here (no origin or target included here,
only intermediate stops).</td></tr>
</table>

<br />
@section usage_stopList_sec Receiving Stop Lists
To get a list of stop suggestions use the data source
@verbatim "Stops <service-provider-id>|stop=<stop-name-part>" @endverbatim
If the provider supports the @em ProvidesStopsByGeoPosition feature, the following parameters can
be used to get stops at a specific geo position:
@verbatim "Stops <service-provider-id>|latitude=<decimal-latitude>|longitude=<decimal-longitude>" @endverbatim

In your dataUpdated-slot you should first check, if a stop list was received by checking if a
key "stops" exists in the data object from the data engine. Then you get the stop data, which is
stored in the key "stops" and contains a list of data sets, one for each stop. They have at least
a <i>StopName</i> key (containing the stop name). They <b>may</b> additionally contain a
<i>StopID</i> (a non-ambiguous ID for the stop, if available, otherwise it is empty),
<i>StopWeight</i> (the weight of the suggestion), a <i>StopCity</i> (the city the stop is in)
and a <i>StopCountryCode</i> (the code of the country in with the stop is). If the provider
supports the ProvidesStopGeoPosition feature they also contain <i>StopLatitude</i> and
<i>StopLongitude</i>.

@code
void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data ) {
    if ( data.contains("stops").toBool() ) {
        QStringList possibleStops;
        QVariantList stops = data["stops"].toList();
        foreach ( const QVariant &stopData, stops ) {
            QVariantHash stop = stopData.toHash();

            // Get the name
            QString stopName = stop["StopName"].toString();

            // Get other values
            if ( stopData.contains("StopID") ) {
                QString stopID = stop["StopID"].toString();
            }
            QString stopCity = stop["StopCity"].toString();
            QString stopCityCode = stop["StopCountryCode"].toString();
        }
    }
}
@endcode

@subsection usage_stopList_datastructure_sec Stop Data Source Structure
The data received from the data engine contains these keys:<br />
<table>
<tr><td><i>error</i></td> <td>bool</td> <td>True, if an error occurred while parsing.</td></tr>
<tr><td><i>errorMessage</i></td> <td>QString</td> <td>(only if @em error is @c true),
an error message string.</td></tr>
<tr><td><i>errorCode</i></td> <td>int</td> <td>(only if @em error is @c true), an error code.
    Error code 1 means, that there was a problem downloading a source file.
    Error code 2 means, that parsing a source file failed.
    Error code 3 means that a GTFS feed needs to be imorted into the database before using it.
    Use the @ref PublicTransportService to start and monitor the import.</td></tr>
<tr><td><i>updated</i></td> <td>QDateTime</td> <td>The date and time when the data source was
last updated.</td></tr>
<tr><td><i>stops</i></td> <td>QVariantList</td> <td>A list of all found stops.</td></tr>
</table>
<br />
Each stop in the data received from the data engine has the following keys:<br />
<table>
<tr><td><i>StopName</i></td> <td>QString</td> <td>The name of the stop.</td></tr>
<tr><td><i>StopID</i></td> <td>QString</td> <td>A unique ID for the stop, if available.</td></tr>
<tr><td><i>StopWeight</i></td> <td>int</td>
<td>The weight of the stop as a suggestion, if available.</td></tr>
<tr><td><i>StopCity</i></td> <td>QString</td>
<td>The name of the city the stop is in, if available.</td></tr>
<tr><td><i>StopCountryCode</i></td> <td>QString</td> <td>The code of the country in with
the stop is, if available.</td></tr>
<tr><td><i>StopLatitude</i></td> <td>qreal</td> <td>The decimal latitude of the stop.
Only available if the provider supports the ProvidesStopGeoPosition feature.</td></tr>
<tr><td><i>StopLongitude</i></td> <td>qreal</td> <td>The decimal longitude of the stop.
Only available if the provider supports the ProvidesStopGeoPosition feature.</td></tr>
</table>

<br />
@section usage_service_sec Using the Timetable Service
This service is available for all timetable data sources, ie. departure, arrival and journey
data sources. It can be retrieved using DataEngine::serviceForSource() with the name of the
timetable data source. The service offers some operations on timetable data sources and allows
to change it's contents, ie. update or extend it with new data.
@see TimetableService

@subsection usage_service_manual_update Manual updates
Manual updates can be requested for timetable data sources using the @b requestUpdate operation.
They may be rejected if the last update was not long enough ago (see the @em minManualUpdateTime
field of the data source). Manual updates are allowed more often than automatic updates.
Does not need any parameters.
The following code example shows how to use the service to request a manual update:
@code
// Get a pointer to the service for the used data source,
// use Plasma::Applet::dataEngine() to get a pointer to the engine,
// alternatively Plasma::DataEngineManager can be used
Plasma::Service *service = dataEngine("publictransport")->serviceForSource( sourceName );

// Start the "requestUpdate" operation (no parameters)
KConfigGroup op = service->operationDescription("requestUpdate");
Plasma::ServiceJob *updateJob = service->startOperationCall( op );

// Connect to the finished() slot if needed
connect( updateJob, SIGNAL(finished(KJob*)), this, SLOT(updateRequestFinished(KJob*)) );
@endcode

@subsection usage_service_additional_data Request additional data
Additional data (eg. route data) can be requested for specific timetable items. There are two
operations @b "requestAdditionalData" and @b "requestAdditionalDataRange", the latter one should
be used if additional data gets requested for multiple items at once to save data source updates
in the engine. Uses an "itemnumber" or "itemnumberbegin"/"itemnumberend" parameters to identify
the timetable item(s) to get additional data for.
@code
// Get a pointer to the service for the used data source, like in example 1
Plasma::Service *service = dataEngine("publictransport")->serviceForSource( sourceName );

// Start the "requestAdditionalData" operation
// with an "itemnumber" parameter, 0 to get additional data for the first item
KConfigGroup op = service->operationDescription("requestAdditionalData");
op.writeEntry( "itemnumber", 0 );
Plasma::ServiceJob *additionalDataJob = service->startOperationCall( op );
@endcode

The associated timetable data source item (eg. a departure) gets extended with the additional data.
When a timetable item in the data source already includes additional data, it has the field
@em "additionalDataState" set to @em "included". If additional data was requested but is not yet
ready the state is @em "busy" (you may want to show a busy widget while this state is set).
Otherwise the additional data state is @em "error", @em "notrequested" or @em "notsupported"
(see @ref usage_departures_datastructure_sec).

What data actually gets added as additional data is up to the provider plugin. But it can also
be determined with the other supported features sometimes. If a provider eg. supports the
Enums::ProvidesAdditionalData @em and Enums::ProvidesRouteInformation features and a departure
does not include any route data (without additional data), it can be expected that route data will
be added as additional data.

@note The used provider must support the Enums::ProvidesAdditionalData feature, otherwise all
  requests for additional data will fail. For script providers this feature gets automatically set
  if the getAdditionalData() function is implemented in the script.

@subsection usage_service_later_items Request earlier/later items
Use the operations @em "requestEarlierItems" and @em "requestLaterItems" to get more timetable items
for a data source. This is currently only used for journeys. The difference between these operation
and simply requesting more journeys with an earlier/later time is that the provider may benefit
from data stored for the request at the provider's server (if any) when using this operation.
Another difference is that the data source will contain both the old and the earlier/later
journeys after using this operation.

These operations need the used service provider to support the @em ProvidesMoreJourneys feature.
Does not need any parameters.
@code
// Get a pointer to the service for the used data source, like in example 1
Plasma::Service *service = dataEngine("publictransport")->serviceForSource( sourceName );

// Start the "requestLaterItems" operation (no parameters)
KConfigGroup op = service->operationDescription("requestLaterItems");
Plasma::ServiceJob *laterItemsJob = service->startOperationCall( op );
@endcode

@n
@section usage_gtfs_service_sec Using the GTFS Service
This service is available under the name "GTFS". It can be retrieved using
DataEngine::serviceForSource() with "GTFS" as source name. The service offers some operations to
import GTFS feeds or to update/delete an already imported GTFS database.
@see GtfsService

@subsection usage_gtfs_service_import Import GTFS feeds
Use the operation @em "importGtfsFeed" to import the GTFS feed of an installed GTFS provider plugin
to its local GTFS database. When this operation succeeds, timetable data will be available without
network connection for the GTFS provider plugin. The operation expects one parameter
@em "serviceProviderId", which is the ID of the GTFS provider plugin which GTFS feed should get
imported.@n

While the import is running the service provider data source will set the @em "state" field for
the importing provider to @em "importing_gtfs_feed". The @em "stateData" field gets a field
@em "progress" for monitoring the import progress, @em "statusMessage" contains more information
about what is currently done in the import job.

@note Importing a GTFS feed can take some time and the resulting GTFS database can take quite some
  disk space, eg. around 300MB depending on the GTFS feed. The @em "stateData" field of the
  provider data source contains some informative fields about the feed, eg. @em "gtfsFeedSize"
  contains the size in bytes of the GTFS feed (can be used to estimate how big the database will be).
  If it is not available, ie. @em "gtfsFeedSize" is -1, use the @em "updateGtfsFeedInfo" operation
  to update this information.

@subsection usage_gtfs_service_update Update GTFS database
GTFS providers automatically update their GTFS databases when they get created and the GTFS feed
was modified. To manually request an update of the GTFS database use the operation
@em "updateGtfsDatabase", which expects one parameter @em "serviceProviderId", which is the ID
of the GTFS provider plugin which GTFS database should be updated.

@subsection usage_gtfs_service_delete Delete GTFS database
To delete an imported GTFS database use the @em "deleteGtfsDatabase" operation, which expects
one parameter @em "serviceProviderId", which is the ID of the GTFS provider plugin which GTFS
database should be deleted.

@subsection usage_gtfs_service_info Get information about GTFS feeds
The service provider data source contains informative fields about the GTFS feed for GTFS providers
in their @em "stateData" fields. If this information is not available it can be requested using
the @em "updateGtfsFeedInfo" operation, which expects one parameter @em "serviceProviderId", which
is the ID of the GTFS provider plugin for which information about the GTFS feed should be updated.
The data updated by this operation is @em "gtfsFeedSize" and @em "gtfsFeedModifiedTime". The size
of the feed is given in bytes and can be used to estimate how big the database will be after an
import. The last modified time of the GTFS feed is stored as Qt::IsoDate string. The size will be
-1 and the modified time will be invalid if this information is not available.
*/

/** @page pageServiceProviders Add Support for new Service Providers
@par Sections
<ul>
    <li>@ref provider_plugin_format_version </li>
    <li>@ref provider_plugin_types </li>
    <li>@ref provider_plugin_pts </li>
    <li>@ref provider_plugin_script </li>
    <li>@ref examples </li>
    <ul>
        <li>@ref examples_xml_gtfs </li>
        <li>@ref examples_xml_script </li>
        <li>@ref examples_script </li>
    </ul>
</ul>
@n

@n
@section provider_plugin_format_version Provider Plugin Format Version
New versions of the data engine may require provider plugins to use a new version of the provider
plugin format. The format version to use gets specified in the @em \<serviceProvider\>
XML tag of the .pts file, see @ref provider_plugin_pts. Version 1.0 is no longer supported,
because the new script API requires the scripts to be updated and GTFS providers were not supported
in that version. Later updates should be backwards compatible.

New plugins should use the newest version, currently 0.11. An older version can be used to also
support older versions of the data engine, if the update only affects a not used provider type.

<table>
    <tr><td><b>PublicTransport<br/> Engine Version</b></td>
        <td><b>Required Provider Plugin<br /> Format Version</b></td>
        <td><b>Changes</b></td></tr>
    <tr><td>until 0.10</td><td>1.0</td><td>(first version)</td></tr>
    <tr><td>0.11</td><td>1.1</td><td>New provider type @em "gtfs",<br />
        New script API (see @ref scriptApi),<br />
        New XML tags: \<samples\>, \<notes\>, \<feedUrl\>, \<realtimeTripUpdateUrl\>,
        \<realtimeAlertsUrl\>, \<timeZone\>
        </td>
    </tr>
</table>
@n

@section provider_plugin_types Provider Plugin Types
Currently two provider plugin types are supported: @em "script" and @em "gtfs".

GTFS providers are very easy, all you need is an URL to the GTFS feed zip file and general
information about the provider and the plugin. The GTFS feed then can be imported into a local
database using the GTFS service of the data engine. GTFS-realtime is also supported, but not
used widely among the currently supported GTFS providers. See @ref gtfsProviders for more
information.

Scripted providers need a script file to execute requests. There is an API for such scripts,
see @ref scriptApi. For providers using the HAFAS API this is quite easy, as most script logic
is already implemented in a base script. A base script for the EFA API does not exist yet.
See @ref scriptProviders for more information.
@n

@n
@section provider_plugin_pts Provider Plugin .pts File Structure
To add support for a new service provider you need to create a service provider plugin for the
PublicTransport engine, which is essentially an XML file with information about the service
provider. This XML file contains a name, description, changelog, etc. for the service provider
plugin and uses the mime type "application-x-publictransport-serviceprovider" (*.pts).
It can also contain a reference to a script to parse documents from the provider to process
requests from the data engine. There are many helper functions available for scripts to parse HTML
documents, QtXML is available to parse XML documents (as extension).
The filename of the XML file starts with the country code or "international"/"unknown", followed
by "_" and a short name for the service provider, e.g. "de_db.pts", "ch_sbb.pts", "sk_atlas.pts",
"international_flightstats.pts". The base file name (without extension) is the service provider ID.
<br />
There is also a nice tool called @em TimetableMate. It's a little IDE to create service
provider plugins for the PublicTransport data engine. The GUI is similiar to the GUI of KDevelop,
it also has docks for projects, breakpoints, backtraces, variables, a console, script output and
so on. TimetableMate also shows a nice dashboard for the service provider plugin projects. It
features script editing, syntax checking, code-completion for the engine's script API, automatic
tests, web page viewer, network request/reply viewer with some filters, a Plasma preview etc.<br />

Here is an overview of the allowed tags in the XML file (required child tags of the serviceProvider
tag are <span style="background-color: #ffdddd;">highlighted</span>):
<table>
<tr style="background-color: #bbbbbb; font-weight: bold;"><td>Tag</td> <td>Parent Tag</td>
<td>Optional?</td> <td>Description</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<?xml version="1.0" encoding="UTF-8"?\></b></td>
<td>Root</td> <td>Required</td><td>XML declaration.</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<serviceProvider fileVersion="1.1" version=@em "plugin-version" type=@em "provider-type" \> </b></td>
<td>Root</td> <td>Required</td> <td>This is the root item. The only currently supported provider
plugin format version is 1.1 and gets written as 'fileVersion' attribute. The 'version' attribute
contains the version of the plugin itself and the 'type' attribute specifies the type of the
plugin, which can currently be either @em "script" or @em "gtfs".
@see @ref provider_plugin_format_version, @ref provider_plugin_types
</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<name\> </b></td><td>\<serviceProvider\></td> <td>Required</td>
<td>The name of the service provider (plugin). If it provides data for international stops it
should begin with "International", if it's specific for a country or city it should begin with the
name of that country or city. That should be followed by a short url to the service provider.
</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<description\> </b></td><td>\<serviceProvider\> </td> <td>Required</td>
<td>A description of the service provider (plugin). You don't need to list the features
supported by the service provider here, the feature list is generated automatically.</td></tr>

<tr style="background-color: #ffdddd;"><td style="color:#888800">
<b>\<author\> </b></td><td>\<serviceProvider\></td> <td>Required</td>
<td>Contains information about the author of the service provider plugin.</td></tr>

<tr style="background-color: #ffdddd;"><td style="color:#666600">
<b>\<fullname\> </b></td><td style="color:#888800">\<author\></td> <td>Required</td>
<td>The full name of the author of the service provider plugin.</td></tr>

<tr><td style="color:#666600">
<b>\<short\> </b></td><td style="color:#888800">\<author\></td> <td>(Optional)</td>
<td>A short name for the author of the service provider plugin (eg. the initials).</td></tr>

<tr><td style="color:#666600">
<b>\<email\> </b></td><td style="color:#888800">\<author\></td> <td>(Optional)</td> <td>
The email address of the author of the service provider plugin.</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<version\> </b></td><td>\<serviceProvider\> </td> <td>Required</td>
<td>The version of the service provider plugin, should start with "1.0".</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<url\> </b></td><td>\<serviceProvider\></td> <td>Required</td>
<td>An url to the service provider home page.</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<shortUrl\> </b></td><td>\<serviceProvider\></td> <td>Required</td>
<td>A short version of the url, used as link text.</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<script\> </b></td><td>\<serviceProvider\></td>
<td>(Required only with "script" @em type)</td>
<td>Contains the filename of the script to be used to parse timetable documents.
The script must be in the same directory as the XML file. Always use "Script" as type when
using a script. Can have an "extensions" attribute with a comma separated list of QtScript
extensions to load when executing the script.</td></tr>

<tr><td style="color:#00bb00">
<b>\<cities\> </b></td><td>\<serviceProvider\></td> <td>(Optional)</td>
<td>A list of cities the service provider has data for (with surrounding \<city\>-tags).</td></tr>

<tr><td style="color:#007700">
<b>\<city\> </b></td><td style="color:#00bb00">\<cities\></td> <td>(Optional)</td>
<td>A city in the list of cities (\<cities\>). Can have an attribute "replaceWith",
to replace city names with values used by the service provider.</td></tr>

<tr><td><b>\<notes> </b></td><td>\<serviceProvider\> </td> <td>(Optional)</td>
<td>Custom notes for the service provider plugin. Can be a to do list.</td></tr>

<tr><td><b>\<fallbackCharset\> </b></td><td>\<serviceProvider\> </td> <td>(Optional)</td>
<td>The charset of documents to be downloaded. Depending on the used service provider this might
be needed or not. Scripts can use this value.</td></tr>

<tr><td><b>\<credit\> </b></td><td>\<serviceProvider\> </td> <td>(Optional)</td>
<td>A courtesy string that is required to be shown to the user when showing the timetable data
of the GTFS feed. If this tag is not given, a short default string is used,
eg. "data by: www.provider.com" or only the link (depending on available space).
Please check the license agreement for using the GTFS feed for such a string and include it here.
</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<feedUrl\> </b></td><td>\<serviceProvider\> </td>
<td>(Required only with "gtfs" @em type)</td>
<td>An URL to the GTFS feed to use. Use an URL to the latest available feed.</td></tr>

<tr><td><b>\<realtimeTripUpdateUrl\> </b></td><td>\<serviceProvider\> </td>
<td>(Optional, only used with "GTFS" type)</td>
<td>An URL to a GTFS-realtime data source with trip updates. If this tag is not present delay
information will not be available.</td></tr>

<tr><td><b>\<realtimeAlertsUrl\> </b></td><td>\<serviceProvider\> </td>
<td>(Optional, only used with "GTFS" type)</td>
<td>An URL to a GTFS-realtime data source with alerts. If this tag is not present journey news
will not be available.</td></tr>

<tr><td><b>\<timeZone> </b></td><td>\<serviceProvider\> </td>
<td>(Optional)</td>
<td>The name of the timezone of times from the service provider, eg. "America/Los_Angeles"
(Pacific Time). GTFS providers use this to calculate local time values.
@see KSystemTimeZones::zones() for a list of available time zones.
</td></tr>

<tr><td style="color:#00bb00;">
<b>\<changelog\> </b></td><td>\<serviceProvider\> </td> <td>(Optional)</td>
<td>Contains changelog entries for this service provider plugin.</td></tr>

<tr><td style="color:#007700;">
<b>\<entry\> </b></td><td style="color:#00bb00;">\<changelog\> </td> <td>(Optional)</td>
<td>Contains a changelog entry for this service provider plugin. The entry description is read
from the contents of the \<entry\> tag. Attributes @em version (the plugin version where
this change was applied) and @em engineVersion (the version of the publictransport
data engine this plugin was first released with) can be added.</td></tr>

<tr><td style="color:#880088;">
<b>\<samples\> </b></td><td>\<serviceProvider\> </td> <td>(Optional)</td>
<td>Contains child tags \<stop\> and \<city\> with sample stop/city names. These samples are used
eg. in TimetableMate for automatic tests.</td></tr>

<tr><td style="color:#660066;">
<b>\<stop\> </b></td><td style="color:#880088;">\<samples\> </td> <td>(Optional)</td>
<td>A sample stop name.</td></tr>

<tr><td style="color:#660066;">
<b>\<city\> </b></td><td style="color:#880088;">\<samples\> </td> <td>(Optional)</td>
<td>A sample city name.</td></tr>
</table>
@n

@section provider_plugin_script Script File Structure
Scripts are executed using QtScript (JavaScript), which can make use of Kross if other script
languages should be used, eg. Python or Ruby. JavaScript is tested, the other languages may also
work. There are functions with special names that get called by the data engine when needed:
@n
features(), getTimetable(), getStopSuggestions(), getJourneys() and getAdditionalData()
@n

@n
@section examples Service Provider Plugin Examples
@n
@subsection examples_xml_script A Simple Script Provider Plugin
Here is an example of a simple service provider plugin which uses a script to parse data from
the service provider:
@verbinclude ch_sbb.pts

@subsection examples_xml_gtfs A Simple GTFS Provider Plugin
The simplest provider XML can be written when using a GTFS feed. This example also contains tags
for GTFS-realtime support, which is optional.
@verbinclude us_bart.pts

@n
@subsection examples_script A Simple Parsing-Script
This is an example of a script used to parse data from the service provider. The script uses the
base script class for HAFAS providers, which already has quite flexible implementations
for the script.
@include ch_sbb.js
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
        label="{ServiceProviderScript|Parses timetable documents (eg. HTML) using scripts.}"
        URL="\ref ServiceProviderScript"
        fillcolor="#dfdfff"
    ];

    providerGtfs [
        label="{ServiceProviderGtfs|Imports GTFS feeds into a local database.}"
        URL="\ref ServiceProviderGtfs"
        fillcolor="#dfdfff"
    ];

    provider [
        label="{ServiceProvider|Loads timetable data from service providers.\lIt uses ServiceProviderData to get needed information.\l|# requestDepartures()\l# requestJourneys()\l# requestStopSuggestions()\l# requestStopsByGeoPosition()\l# requestAdditionData()\l# requestMoreItems()\l+ data() : ServiceProviderData\l}"
        URL="\ref ServiceProvider"
        fillcolor="#dfdfff"
    ];

    timetableData [
        label="{TimetableData|Typedef for QHash with TimetableInformation keys and QVariant values.\lCan be used to describe a departure, arrival, journey or stop.}"
        URL="\ref TimetableData"
    ];

    serviceProviderData [
        label="{ServiceProviderData|Information about the service provider.\l|+ name() : QString\l+ description() : QString\l+ features() : QStringList\l... }"
        URL="\ref ServiceProviderData"
    ];

    { rank=same; provider providerScript providerGtfs }

    edge [ arrowhead="empty", arrowtail="none", style="solid" ];
    providerScript -> provider;
    providerGtfs -> provider;

    edge [ dir=back, arrowhead="normal", arrowtail="none", style="dashed", fontcolor="gray", taillabel="", headlabel="0..*" ];
    engine -> provider [ label=""uses ];
    provider -> timetableData [ label="uses" ];
    provider -> serviceProviderData [ label="uses" ];

    edge [ dir=both, arrowhead="normal", arrowtail="normal", color="gray", fontcolor="gray", style="dashed", headlabel="" ];
    serviceProviderData -> provider [ label="friend" ];
}
@enddot
*/

/** @page pageGlossary PublicTransport Glossary
<table border="0" cellspacing="10">
<tr><td style="text-align:right;"><b>(Service) Provider</b></td>
    <td>A provider offers timetable data, eg. using a GTFS feed, a website or a (public) web API.
    Also called "agency".</td>
</tr>
<tr><td style="text-align:right;"><b>(Service) Provider Plugin</b></td>
    <td>A plugin for the PublicTransport data engine to add support for a service provider.
    For simplicity provider plugins are called provider sometimes.</td>
</tr>
<tr><td style="text-align:right;"><b>Stop</b></td>
    <td>A stop is where transportation vehicles stop. For trains this is a station,
    for flights it is an airport, for ships it is a harbor, etc.</td>
</tr>
<tr><td style="text-align:right;"><b>Journey</b></td>
    <td>A journey is a connection from one stop to another. It can contain sub journeys with
    information about the route of each vehicle used in the journey.
    Other projects call journeys "trips", "connections" or "routes".</td>
</tr>
<tr><td style="text-align:right;"><b>Route</b></td>
    <td>For departures/arrivals the route is the list of stops passed by the departing/arriving
    vehicle. For journeys it is the list of intermediate stops including origin and target.
    Sub journeys contain routes between two intermediate stops of a journey.</td>
</tr>
<tr><td style="text-align:right;"><b>Transport Line</b></td>
    <td>The name of a public transport line or train/flight number. Information about the vehicle
    type should not be included, except for preventing ambiguities.
    @em Examples: "3", "S4", "U5", "RB 885610".</td>
</tr>
<tr><td style="text-align:right;"><b>Vehicle Type</b></td>
    <td>The vehicle type can be a tram, bus, subway, train, ferry, etc.</td>
</tr>
<tr><td style="text-align:right;"><b>Additional Data</b></td>
    <td>This can be any timetable data that can be loaded later for existing timetable data
    sources. For example the HAFAS base script allows to load route data later, because it is
    not directly available.</td>
</tr>
<tr><td style="text-align:right;"><b>GTFS</b></td>
    <td>The <a href="https://developers.google.com/transit/gtfs/reference">GeneralTransitFeedSpecification</a>
    gets used by many service providers for standardized feeds with timetable data.</td>
</tr></table>
 */
