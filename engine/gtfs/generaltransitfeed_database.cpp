/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

#include "generaltransitfeed_database.h"

#include <KDebug>
#include <KGlobal>
#include <KStandardDirs>

#include <QDate>
#include <QColor>
#include <QUrl>
#include <QVariant>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

QString GeneralTransitFeedDatabase::databasePath( const QString &providerName )
{
    const QString dir = KGlobal::dirs()->saveLocation("data", "plasma_engine_publictransport/gtfs/");
    return dir + providerName + ".sqlite";
}

bool GeneralTransitFeedDatabase::initDatabase( const QString &providerName, QString *errorText )
{
    QSqlDatabase db = QSqlDatabase::database( providerName );
    if ( !db.isValid() ) {
        db = QSqlDatabase::addDatabase( "QSQLITE", providerName );
        if ( !db.isValid() ) {
            kDebug() << "Error adding a QSQLITE database" << db.lastError();
            *errorText = "Error adding a QSQLITE database " + db.lastError().text();
            return false;
        }

        db.setDatabaseName( databasePath(providerName) );
        if ( !db.open() ) {
            kDebug() << "Error opening the database connection" << db.lastError();
            *errorText = "Error opening the database connection " + db.lastError().text();
            return false;
        }
    }

    return true;
}

bool GeneralTransitFeedDatabase::createDatabaseTables( QString *errorText, QSqlDatabase database )
{
    QSqlQuery query( database );
    kDebug() << "Create tables";

    // Create table for "agency.txt" TODO agency_id only referenced from routes => merge tables?
    query.prepare( "CREATE TABLE IF NOT EXISTS agency ("
                   "agency_id INTEGER UNIQUE PRIMARY KEY, " // (optional for gtfs with a single agency)
                   "agency_name VARCHAR(256) NOT NULL, " // (required) The name of the agency
                   "agency_url VARCHAR(512) NOT NULL, " // (required) URL of the transit agency
                   "agency_timezone VARCHAR(256), " // (required, if NULL, the default timezone from the accesor XML is used, from <timeZone>-tag) Timezone name, see http://en.wikipedia.org/wiki/List_of_tz_zones
                   "agency_lang VARCHAR(2), " // (optional) A two-letter ISO 639-1 code for the primary language used by this transit agency
                   "agency_phone VARCHAR(64)" // (optional) A single voice telephone number for the agency (can contain punctuation marks)
//                    "agency_fare_url VARCHAR(512)" // (optional) URL of a website about fares of the transit agency (found in TriMet's GTFS data)
                   ")" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'agency' table:" << query.lastError();
        *errorText = "Error creating 'agency' table: " + query.lastError().text();
        return false;
    }

    // Create table for "routes.txt"
    // Values for the "route_type" field:
    //     0 - Tram, Streetcar, Light rail. Any light rail or street level system within a metropolitan area.
    //     1 - Subway, Metro. Any underground rail system within a metropolitan area.
    //     2 - Rail. Used for intercity or long-distance travel.
    //     3 - Bus. Used for short- and long-distance bus routes.
    //     4 - Ferry. Used for short- and long-distance boat service.
    //     5 - Cable car. Used for street-level cable cars where the cable runs beneath the car.
    //     6 - Gondola, Suspended cable car. Typically used for aerial cable cars where the car is suspended from the cable.
    //     7 - Funicular. Any rail system designed for steep inclines.
    query.prepare( "CREATE TABLE IF NOT EXISTS routes ("
                   "route_id INTEGER UNIQUE PRIMARY KEY NOT NULL, " // (required)
                   "agency_id INTEGER, " // (optional) Defines an agency for the route
                   "route_short_name VARCHAR(128), " // (required) The short name of a route (can be an empty (NULL in DB) string, then route_long_name is used)
                   "route_long_name VARCHAR(256), " // (required) The long name of a route (can be an empty (NULL in DB) string, then route_short_name is used)
                   "route_desc VARCHAR(256), " // (optional) Additional information
                   "route_type INTEGER NOT NULL, " // (required) The type of transportation used on a route (see above)
                   "route_url VARCHAR(512), " // (optional) URL of a web page about a particular route
                   "route_color VARCHAR(6), " // (optional) The (background) color for a route as a six-character hexadecimal number, eg. 00FFFF, default is white
                   "route_text_color VARCHAR(6), " // (optional) The text color for a route as a six-character hexadecimal number, eg. 00FFFF, default is black
                   "FOREIGN KEY(agency_id) REFERENCES agency(agency_id)"
                   ")" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'routes' table:" << query.lastError();
        *errorText = "Error creating 'routes' table: " + query.lastError().text();
        return false;
    }

    // Create table for "stops.txt"
    query.prepare( "CREATE TABLE IF NOT EXISTS stops ("
                   "stop_id INTEGER UNIQUE PRIMARY KEY NOT NULL, " // (required)
                   "stop_code VARCHAR(30), " // (optional) Makes stops uniquely identifyable by passengers
                   "stop_name VARCHAR(256) NOT NULL, " // (required) The name of the stop
                   "stop_desc VARCHAR(256), " // (optional) Additional information
                   "stop_lat REAL NOT NULL, " // (required) The WGS 84 latitude of the stop
                   "stop_lon REAL NOT NULL, " // (required) The WGS 84 longitude of the stop from -180 to 180
                   "zone_id INTEGER, " // (optional) The fare zone ID for a stop => fare_rules.txt
                   "stop_url VARCHAR(512), " // (optional) URL of a web page about a particular stop
                   "location_type TINYINT, " // (optional) "1": Station (with one or more stops), "0" or NULL: Stop
                   "direction VARCHAR(30), " // (optional)
                   "position VARCHAR(30), " // TODO
                   "parent_station INTEGER, " // (optional) stop_id of a parent station (with location_type == 1)
                   "min_fare_id INTEGER, " // TODO
                   "max_fare_id INTEGER"
                   ");" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'stops' table:" << query.lastError() << query.lastQuery();
        *errorText = "Error creating 'stops' table: " + query.lastError().text();
        return false;
    }
    query.prepare( "CREATE INDEX IF NOT EXISTS stops_stop_name_id ON stops(stop_id, stop_name);" );
    if( !query.exec() ) {
        kDebug() << "Error creating index for 'stop_name' in 'stops' table:" << query.lastError() << query.lastQuery();
        *errorText = "Error creating index for 'stop_name' in 'stops' table: " + query.lastError().text();
        return false;
    }

// Not used
//     // Create table for "shapes.txt"
//     query.prepare( "CREATE TABLE IF NOT EXISTS shapes ("
//                    "shape_id INTEGER UNIQUE PRIMARY KEY NOT NULL, " // (required)
//                    "shape_pt_lat REAL NOT NULL, " // (required) The WGS 84 latitude of a point in a shape
//                    "shape_pt_lon REAL NOT NULL, " // (required) The WGS 84 longitude of a point in a shape from -180 to 180
//                    "shape_pt_sequence INTEGER NOT NULL, " // (required) Associates the latitude and longitude of a shape point with its sequence order along the shape
//                    "shape_dist_traveled REAL" // (optional) If used, positions a shape point as a distance traveled along a shape from the first shape point
//                    ")" );
//     if( !query.exec() ) {
//         kDebug() << "Error creating 'shapes' table:" << query.lastError();
//         return false;
//     }

    // Create table for "trips.txt"
    query.prepare( "CREATE TABLE IF NOT EXISTS trips ("
                   "trip_id INTEGER UNIQUE PRIMARY KEY NOT NULL, " // (required) TODO trip_id only referenced from (small) frequencies and (big) stop_times => merge?
                   "route_id INTEGER NOT NULL, " // (required) Uniquely identifies a route (routes.txt)
                   "service_id INTEGER NOT NULL, " // (required) Uniquely identifies a set of dates when service is available for one or more routes (in "calendar" or "calendar_dates")
                   "trip_headsign VARCHAR(256), " // (optional) The text that appears on a sign that identifies the trip's destination to passengers
                   "trip_short_name VARCHAR(256), " // (optional) The text that appears in schedules and sign boards to identify the trip to passengers
                   "direction_id TINYINT, " // (optional) A binary value to distinguish between bi-directional trips with the same route_id
                   "block_id INTEGER, " // (optional) Identifies the block to which the trip belongs. A block consists of two or more sequential trips made using the same vehicle, where a passenger can transfer from one trip to the next just by staying in the vehicle.
                   "shape_id INTEGER, " // (optional) Uniquely identifies a shape (shapes.txt)
                   "FOREIGN KEY(route_id) REFERENCES routes(route_id), "
                   "FOREIGN KEY(shape_id) REFERENCES shapes(shape_id)"
                   ")" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'trips' table:" << query.lastError();
        *errorText = "Error creating 'trips' table: " + query.lastError().text();
        return false;
    }

    // Create table for "stop_times.txt"
    query.prepare( "CREATE TABLE IF NOT EXISTS stop_times ("
                   "trip_id INTEGER NOT NULL, " // (required) Uniquely identifies a trip (trips.txt)
                   "arrival_time INTEGER NOT NULL, " // (required) Specifies the arrival time at a specific stop for a specific trip on a route, HH:MM:SS or H:MM:SS, can be > 23:59:59 for times on the next day, eg. for trips that span with multiple dates
                   "departure_time INTEGER NOT NULL, " // (required) Specifies the departure time from a specific stop for a specific trip on a route, HH:MM:SS or H:MM:SS, can be > 23:59:59 for times on the next day, eg. for trips that span with multiple dates
                   "stop_id INTEGER NOT NULL, " // (required) Uniquely identifies a stop (with location_type == 0, if used)
                   "stop_sequence INTEGER NOT NULL, " // (required) Identifies the order of the stops for a particular trip
                   "stop_headsign VARCHAR(256), " // (optional) The text that appears on a sign that identifies the trip's destination to passengers. Used to override the default trip_headsign when the headsign changes between stops.
                   "pickup_type TINYINT, " // (optional) Indicates whether passengers are picked up at a stop as part of the normal schedule or whether a pickup at the stop is not available, 0: Regularly scheduled pickup, 1: No pickup available, 2: Must phone agency to arrange pickup, 3: Must coordinate with driver to arrange pickup, default is 0
                   "drop_off_type TINYINT, " // (optional) Indicates whether passengers are dropped off at a stop as part of the normal schedule or whether a drop off at the stop is not available, 0: Regularly scheduled drop off, 1: No drop off available, 2: Must phone agency to arrange drop off, 3: Must coordinate with driver to arrange drop off, default is 0
                   "shape_dist_traveled TINYINT, " // (optional) If used, positions a stop as a distance from the first shape point (same unit as in shapes.txt)
                   "FOREIGN KEY(trip_id) REFERENCES trips(trip_id), "
                   "FOREIGN KEY(stop_id) REFERENCES stops(stop_id), "
                   "PRIMARY KEY(stop_id, departure_time, trip_id)" // makes inserts slow..
                   ");" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'stop_times' table:" << query.lastError();
        *errorText = "Error creating 'stop_times' table: " + query.lastError().text();
        return false;
    }
    // Create an index to quickly access trip information sorted by stop_sequence,
    // eg. for route stop lists for departures
    query.prepare( "CREATE INDEX IF NOT EXISTS stop_times_trip ON stop_times(trip_id, stop_sequence, stop_id);" );
    if( !query.exec() ) {
        kDebug() << "Error creating index for 'trip_id' in 'stop_times' table:" << query.lastError();
        *errorText = "Error creating index for 'trip_id' in 'stop_times' table: " + query.lastError().text();
        return false;
    }

    // Create table for "calendar.txt" (exceptions in "calendar_dates.txt")
    query.prepare( "CREATE TABLE IF NOT EXISTS calendar ("
                   "service_id INTEGER UNIQUE PRIMARY KEY NOT NULL, " // (required) Uiquely identifies a set of dates when service is available for one or more routes
                   "weekdays VARCHAR(7) NOT NULL, " // (required) Combines GTFS fields monday-sunday into a string of '1' (available at that weekday) and '0' (not available)
                   "start_date VARCHAR(8) NOT NULL, " // (required) Contains the start date for the service, in yyyyMMdd format
                   "end_date VARCHAR(8) NOT NULL" // (required) Contains the end date for the service, in yyyyMMdd format
                   ")" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'calendar' table:" << query.lastError();
        *errorText = "Error creating 'calendar' table: " + query.lastError().text();
        return false;
    }

    // Create table for "calendar_dates.txt"
    query.prepare( "CREATE TABLE IF NOT EXISTS calendar_dates ("
                   "service_id INTEGER NOT NULL, " // (required) Uiquely identifies a set of dates when a service exception is available for one or more routes, Each (service_id, date) pair can only appear once in "calendar_dates", if the a service_id value appears in both "calendar" and "calendar_dates", the information in "calendar_dates" modifies the service information specified in "calendar", referenced by "trips"
                   "date VARCHAR(8) NOT NULL, " // (required) Specifies a particular date when service availability is different than the norm, in yyyyMMdd format
                   "exception_type TINYINT  NOT NULL, " // (required) Indicates whether service is available on the date specified in the date field (1: The service has been added for the date, 2: The service has been removed)
                   "PRIMARY KEY(service_id, date)"
                   ")" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'calendar_dates' table:" << query.lastError();
        *errorText = "Error creating 'calendar_dates' table: " + query.lastError().text();
        return false;
    }

    // Create table for "fare_attributes.txt"
    query.prepare( "CREATE TABLE IF NOT EXISTS fare_attributes ("
                   "fare_id INTEGER UNIQUE PRIMARY KEY NOT NULL, " // (required) Uniquely identifies a fare class
                   "price DECIMAL(5,2) NOT NULL, " // (required) The fare price, in the unit specified by currency_type
                   "currency_type VARCHAR(3) NOT NULL, " // (required) Defines the currency used to pay the fare, ISO 4217 alphabetical currency code, see http://www.iso.org/iso/en/prods-services/popstds/currencycodeslist.html
                   "payment_method TINYINT NOT NULL, " // (required) Indicates when the fare must be paid (0: paid on board, 1: must be paid before boarding)
                   "transfers TINYINT, " // (required in fare_attributes.txt, but may be empty => use NULL here) Specifies the number of transfers permitted on this fare (0: no transfers permitted on this fare, 1: passenger may transfer once, 2: passenger may transfer twice, (empty): Unlimited transfers are permitted)
                   "transfer_duration INTEGER" // (optional) Specifies the length of time in seconds before a transfer expires
                   ")" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'fare_attributes' table:" << query.lastError();
        *errorText = "Error creating 'fare_attributes' table: " + query.lastError().text();
        return false;
    }

    // Create table for "fare_rules.txt"
    query.prepare( "CREATE TABLE IF NOT EXISTS fare_rules ("
                   "fare_id INTEGER NOT NULL, " // (required) Uniquely identifies a fare class
                   "route_id INTEGER, " // (optional) Associates the fare ID with a route
                   "origin_id INTEGER, " // (optional) Associates the fare ID with an origin zone ID
                   "destination_id INTEGER, " // (optional) Associates the fare ID with a destination zone ID
                   "contains_id INTEGER, " // (optional) Associates the fare ID with a zone ID (intermediate zone)
                   "FOREIGN KEY(fare_id) REFERENCES fare_attributes(fare_id), "
                   "FOREIGN KEY(route_id) REFERENCES routes(route_id), "
                   "FOREIGN KEY(origin_id) REFERENCES stops(zone_id), "
                   "FOREIGN KEY(destination_id) REFERENCES stops(zone_id), "
                   "FOREIGN KEY(contains_id) REFERENCES stops(zone_id)"
                   ")" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'fare_rules' table:" << query.lastError();
        *errorText = "Error creating 'fare_rules' table: " + query.lastError().text();
        return false;
    }

    // Create table for "frequencies.txt"
    query.prepare( "CREATE TABLE IF NOT EXISTS frequencies ("
                   "trip_id INTEGER PRIMARY KEY NOT NULL, " // (required) Identifies a trip on which the specified frequency of service applies
                   "start_time INTEGER NOT NULL, " // (required) Specifies the time at which service begins with the specified frequency, HH:MM:SS or H:MM:SS, can be > 23:59:59 for times on the next day, eg. for trips that span with multiple dates
                   "end_time INTEGER NOT NULL, " // (required) Indicates the time at which service changes to a different frequency (or ceases) at the first stop in the trip, HH:MM:SS or H:MM:SS, can be > 23:59:59 for times on the next day, eg. for trips that span with multiple dates
                   "headway_secs INTEGER NOT NULL, " // (required) Indicates the time between departures from the same stop (headway) for this trip type, during the time interval specified by start_time and end_time, in seconds
                   "FOREIGN KEY(trip_id) REFERENCES trips(trip_id)"
                   ")" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'frequencies' table:" << query.lastError();
        *errorText = "Error creating 'frequencies' table: " + query.lastError().text();
        return false;
    }

    // Create table for "transfers.txt"
    query.prepare( "CREATE TABLE IF NOT EXISTS transfers ("
                   "from_stop_id INTEGER NOT NULL, " // (required) Identifies a stop or station where a connection between routes begins
                   "to_stop_id INTEGER NOT NULL, " // (required) Identifies a stop or station where a connection between routes ends
                   "transfer_type INTEGER NOT NULL, " // (required) Specifies the type of connection for the specified (from_stop_id, to_stop_id) pair (0 or empty: This is a recommended transfer point between two routes, 1: This is a timed transfer point between two routes. The departing vehicle is expected to wait for the arriving one, with sufficient time for a passenger to transfer between routes, 2: This transfer requires a minimum amount of time between arrival and departure to ensure a connection. The time required to transfer is specified by min_transfer_time, 3: Transfers are not possible between routes at this location)
                   "min_transfer_time INTEGER, " // (optional) When a connection between routes requires an amount of time between arrival and departure (transfer_type=2), the min_transfer_time field defines the amount of time that must be available in an itinerary to permit a transfer between routes at these stops. The min_transfer_time must be sufficient to permit a typical rider to move between the two stops, including buffer time to allow for schedule variance on each route, in seconds
                   "FOREIGN KEY(from_stop_id) REFERENCES stops(stop_id), "
                   "FOREIGN KEY(to_stop_id) REFERENCES stops(stop_id)"
                   ")" );
    if( !query.exec() ) {
        kDebug() << "Error creating 'transfers' table:" << query.lastError();
        *errorText = "Error creating 'transfers' table: " + query.lastError().text();
        return false;
    }

    return true;
}

QVariant GeneralTransitFeedDatabase::convertFieldValue( const QString &fieldValue, FieldType type )
{
    if ( fieldValue.isEmpty() ) {
        return QVariant();
    }

    switch ( type ) {
    case HashId:
        return qHash( fieldValue ); // Use the hash to convert string IDs
    case Integer:
        return fieldValue.toInt();
    case SecondsSinceMidnight: {
        // May contain hour values >= 24 (for times the next day), which is no valid QTime
        // Convert valid time format 'h:mm:ss' to 'hh:mm:ss'
        const QString timeString = fieldValue.length() == 7 ? '0' + fieldValue : fieldValue;
        return timeString.left(2).toInt() * 60 * 60 + timeString.mid(3, 2).toInt() * 60
                + timeString.right(2).toInt();
    } case Date:
        return /*QDate::fromString(*/ fieldValue/*, "yyyyMMdd" )*/;
    case Double:
        return fieldValue.toDouble();
    case Url:
        return QUrl( fieldValue );
    case Color: {
        return fieldValue.trimmed().isEmpty() ? Qt::transparent
                                              : QColor( '#' + fieldValue.trimmed() );
    } case String:
    default:
        // TODO Make camel case if everything is upper case?
        return fieldValue;
    }
}

GeneralTransitFeedDatabase::FieldType GeneralTransitFeedDatabase::typeOfField(
        const QString &fieldName )
{
    if ( fieldName == "min_transfer_time" || fieldName == "transfer_type" ||
         fieldName == "headway_secs" || fieldName == "transfer_duration" ||
         fieldName == "transfers" || fieldName == "payment_method" ||
         fieldName == "exception_type" || fieldName == "shape_dist_traveled" ||
         fieldName == "drop_off_type" || fieldName == "pickup_type" ||
         fieldName == "stop_sequence" || fieldName == "shape_pt_sequence" ||
         fieldName == "parent_station" || fieldName == "location_type" ||
         fieldName == "route_type" )
    {
        return Integer;
    } else if ( fieldName.endsWith("_id") ) {
        return HashId;
    } else if ( fieldName == "start_time" || fieldName == "end_time" ||
         fieldName == "arrival_time" || fieldName == "departure_time" )
    {
        return SecondsSinceMidnight; // A time stored as INTEGER
    } else if ( fieldName == "date" || fieldName == "startDate" || fieldName == "endDate" ) {
        return Date;
    } else if ( fieldName.endsWith("_lat") || fieldName.endsWith("_lon") || fieldName == "price" ) {
        return Double;
    } else if ( fieldName.endsWith("_url") ) {
        return Url;
    } else if ( fieldName.endsWith("_color") ) {
        return Color;
    } else {
        return String;
    }
}
