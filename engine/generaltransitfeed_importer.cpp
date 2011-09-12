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

#include "generaltransitfeed_importer.h"

#include <KZip>
#include <KStandardDirs>
#include <KDebug>

#include <QDir>
#include <QUrl>
#include <QColor>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlField>
#include <QSqlRecord>
#include <QSqlDriver>

GeneralTransitFeedImporter::GeneralTransitFeedImporter( const QString &providerName )
{
    initDatabase( providerName );
    if ( !createDatabaseTables() ) {
        kDebug() << "Error initializing tables in the database!";
    }
}

QString GeneralTransitFeedImporter::databasePath( const QString &providerName )
{
    const QString dir = KGlobal::dirs()->saveLocation("data", "plasma_engine_publictransport/gtfs/");
    return dir + providerName + ".sqlite";
}

// The GTFS (GeneralTransitFeedSpecification) specification defines the following files:
//-----------------------------------------------------------------------------------------------
// agency.txt - Required. This file contains information about one or more transit agencies that
//          provide the data in this feed.
// stops.txt - Required. This file contains information about individual locations where vehicles
//          pick up or drop off passengers.
// routes.txt - Required. This file contains information about a transit organization's routes.
//          A route is a group of trips that are displayed to riders as a single service.
// trips.txt - Required. This file lists all trips and their routes. A trip is a sequence of two
//          or more stops that occurs at specific time.
// stop_times.txt - Required. This file lists the times that a vehicle arrives at and departs
//          from individual stops for each trip.
// calendar.txt - Required. This file defines dates for service IDs using a weekly schedule.
//          Specify when service starts and ends, as well as days of the week where service
//          is available.
// calendar_dates.txt - Optional. This file lists exceptions for the service IDs defined in the
//          calendar.txt file. If calendar_dates.txt includes ALL dates of service, this file may
//          be specified instead of calendar.txt.
// fare_attributes.txt - Optional. This file defines fare information for a transit
//          organization's routes.
// fare_rules.txt - Optional. This file defines the rules for applying fare information for a
//          transit organization's routes.
// shapes.txt - Optional. This file defines the rules for drawing lines on a map to represent
//          a transit organization's routes.
// frequencies.txt - Optional. This file defines the headway (time between trips) for routes with
//          variable frequency of service.
// transfers.txt - Optional. This file defines the rules for making connections at transfer
//          points between routes.
//
// (see http://code.google.com/intl/de-DE/transit/spec/transit_feed_specification.html#transitFeedFiles)
bool GeneralTransitFeedImporter::writeGtfsDataToDatabase( const QString &fileName )
{
    const QString tmpGtfsDir = KGlobal::dirs()->saveLocation( "tmp", fileName + '/' );
    KZip gtfsZipFile( fileName );
    if ( !gtfsZipFile.open(QIODevice::ReadOnly) ) {
        kDebug() << "Can not open file" << fileName << gtfsZipFile.device()->errorString();
        return false;
    }
    gtfsZipFile.directory()->copyTo( tmpGtfsDir );
    gtfsZipFile.close();

    bool errors = false;
    QFileInfoList fileInfos = QDir( tmpGtfsDir ).entryInfoList( QDir::Files | QDir::NoDotAndDotDot );
    if ( fileInfos.isEmpty() ) {
        kDebug() << "Empty GTFS feed" << fileName << "extracted to" << tmpGtfsDir;
    }
    foreach ( const QFileInfo &fileInfo, fileInfos ) {
        QStringList requiredFields;
        if ( fileInfo.fileName() == "agency.txt" ) {
            requiredFields << "agency_name" << "agency_url" << "agency_timezone";
        } else if ( fileInfo.fileName() == "stops.txt" ) {
            requiredFields << "stop_id" << "stop_name" << "stop_lat" << "stop_lon";
        } else if ( fileInfo.fileName() == "routes.txt" ) {
            requiredFields << "route_id" << "route_short_name" << "route_long_name" << "route_type";
        } else if ( fileInfo.fileName() == "trips.txt" ) {
            requiredFields << "trip_id" << "route_id" << "service_id";
        } else if ( fileInfo.fileName() == "stop_times.txt" ) {
            requiredFields << "trip_id" << "arrival_time" << "departure_time" << "stop_id"
                           << "stop_sequence";
        } else if ( fileInfo.fileName() == "calendar.txt" ) {
            requiredFields << "service_id" << "monday" << "tuesday" << "wednesday" << "thursday"
                           << "friday" << "saturday" << "sunday" << "start_date" << "end_date";
        } else if ( fileInfo.fileName() == "calendar_dates.txt" ) {
            requiredFields << "service_id" << "date" << "exception_type";
        } else if ( fileInfo.fileName() == "fare_attributes.txt" ) {
            requiredFields << "fare_id" << "price" << "currency_type" << "payment_method"
                           << "transfers";
        } else if ( fileInfo.fileName() == "fare_rules.txt" ) {
            requiredFields << "fare_id";
        } else if ( fileInfo.fileName() == "shapes.txt" ) {
            kDebug() << "Skipping 'shapes.txt', data is unused";
//             requiredFields << "shape_id" << "shape_pt_lat" << "shape_pt_lon" << "shape_pt_sequence";
            continue;
        } else if ( fileInfo.fileName() == "frequencies.txt" ) {
            requiredFields << "trip_id" << "start_time" << "end_time" << "headway_secs";
        } else if ( fileInfo.fileName() == "transfers.txt" ) {
            requiredFields << "from_stop_id" << "to_stop_id" << "transfer_type";
        } else {
            kDebug() << "Filename unexpected:" << fileInfo.fileName();
            continue;
        }

        if ( !writeGtfsDataToDatabase(fileInfo.filePath(), requiredFields) ) {
            errors = true;
            continue;
        }
    }

    return !errors;
}

bool GeneralTransitFeedImporter::writeGtfsDataToDatabase( const QString &fileName,
                                                          const QStringList &requiredFields )
{
    // Open the file
    QFile file( fileName );
    if ( !file.open(QIODevice::ReadOnly) ) {
        kDebug() << "Cannot open file" << fileName;
        return false;
    }

    // Create a text stream
    QTextStream stream( &file );
    if ( stream.atEnd() ) {
        kDebug() << "Empty file" << fileName;
        return false;
    }

    // Open the database
    QSqlDatabase db = QSqlDatabase::database();
    if ( !db.isValid() ) {
        kDebug() << "Can not open database";
        return false;
    }

    const QString tableName = QFileInfo(file).baseName();
    kDebug() << "Read GTFS data for table" << tableName;

    // Read first line from file (header with used field names)
    QStringList fieldNames;
    if ( stream.atEnd() || !readHeader(stream.readLine(), &fieldNames, requiredFields) ) {
        return false; // Error in header
    }
    // Get types of the fields
    QList<QVariant::Type> fieldTypes;
    foreach ( const QString &fieldName, fieldNames ) {
        fieldTypes << typeOfField( fieldName );
    }

    QStringList availableFields;
    QSqlRecord table = db.record( tableName );
    QStringList unavailableFieldNames; // Field names not used in the database
    QList<int> unavailableFieldIndices;
    for ( int i = fieldNames.count() - 1; i >= 0; --i ) {
        if ( !table.contains(fieldNames[i]) ) {
            // The current field name is not available in the database, skip it's value in each row
            unavailableFieldNames << fieldNames[i];
            unavailableFieldIndices << i;
            fieldNames.removeAt( i );
        }
    }
    if ( !unavailableFieldNames.isEmpty() ) {
        kDebug() << "Not all used fields are available in the database:" << unavailableFieldNames;
    }

    // Performance optimization
    QSqlQuery query;
    if( !query.exec("PRAGMA synchronous=OFF;") ) {
        qDebug() << query.lastError();
    }
    if( !query.exec("PRAGMA journal_mode=WAL;") ) {
        qDebug() << query.lastError();
    }

    // Begin transaction
    if ( !db.driver()->beginTransaction() ) {
        qDebug() << db.lastError();
    }

    QStringList dbFieldNames = fieldNames;
    if ( tableName == "calendar" ) {
        dbFieldNames.removeOne( "monday" );
        dbFieldNames.removeOne( "tuesday" );
        dbFieldNames.removeOne( "wednesday" );
        dbFieldNames.removeOne( "thursday" );
        dbFieldNames.removeOne( "friday" );
        dbFieldNames.removeOne( "saturday" );
        dbFieldNames.removeOne( "sunday" );
        dbFieldNames.append( "weekdays" );
    }
    QString placeholder( '?' );
    for ( int i = 1; i < dbFieldNames.count(); ++i ) {
        placeholder += ",?";
    }

    // Prepare an INSERT query to be used for each dataset to be inserted
    query.prepare( QString("INSERT OR REPLACE INTO %1 (%2) VALUES (%3)")
                   .arg(tableName, dbFieldNames.join(","), placeholder) );

    int counter = 0;
    while ( !stream.atEnd() ) {
        QString line = stream.readLine();
        QVariantList fieldValues;
        if ( readFields(line, &fieldValues, fieldTypes, fieldNames.count()) ) {
            // Remove values for fields that do not exist in the database
            for ( int i = unavailableFieldIndices.count() - 1; i >= 0; --i ) {
                fieldValues.removeAt( unavailableFieldIndices[i] );
            }

//             if ( counter < 10 ) // Print out the first 10 rows
//                 qDebug() << fieldValues;

            // Add current field values to the prepared query and execute it
            if ( tableName == "calendar" ) {
                QString weekdays = "0000000";
                for ( int i = 0; i < fieldNames.count(); ++i ) {
                    if ( fieldValues[i].toInt() > 0 ) {
                        const QString fieldName = fieldNames[i];
                        if ( fieldName == "monday" ) {
                            weekdays[0] = '1';
                        } else if ( fieldName == "tuesday" ) {
                            weekdays[1] = '1';
                        } else if ( fieldName == "wednesday" ) {
                            weekdays[2] = '1';
                        } else if ( fieldName == "thursday" ) {
                            weekdays[3] = '1';
                        } else if ( fieldName == "friday" ) {
                            weekdays[4] = '1';
                        } else if ( fieldName == "saturday" ) {
                            weekdays[5] = '1';
                        } else if ( fieldName == "sunday" ) {
                            weekdays[6] = '1';
                        } else {
                            query.addBindValue( fieldValues[i] );
                        }
                    } else {
                        query.addBindValue( fieldValues[i] );
                    }
                }
                query.addBindValue( weekdays );
            } else {
                foreach ( const QVariant &fieldValue, fieldValues ) {
                    query.addBindValue( fieldValue );
                }
            }
            if ( !query.exec() ) {
                kDebug() << query.lastError();
                kDebug() << "With this query:" << query.executedQuery();
                continue;
            }

            // New row has been inserted into the DB successfully
            ++counter;
            if ( counter > 0 && counter % 50000 == 0 ) {
                // Start a new transaction after 10000 INSERTs
                if ( !db.driver()->commitTransaction() ) {
                    qDebug() << db.lastError();
                }
                if ( !db.driver()->beginTransaction() ) {
                    qDebug() << db.lastError();
                }

                qDebug() << "Done:" << (qreal(100 * file.pos()) / qreal(file.size())) << '%';
            }
        } else {
            continue;
        }
    }
    file.close();

    // End transaction, restore synchronous=FULL
    if ( !db.driver()->commitTransaction() ) {
        qDebug() << db.lastError();
    }
    if( !query.exec("PRAGMA synchronous=FULL;") ) {
        qDebug() << query.lastError();
    }

    // Return true (success) if at least one stop has been read
    return counter > 0;
}

bool GeneralTransitFeedImporter::readHeader( const QString &header, QStringList *fieldNames,
                                                 const QStringList &requiredFields )
{
    *fieldNames = header.split(',');

    if ( fieldNames->isEmpty() ) {
        kDebug() << "No field names found in header:" << header;
        return false;
    }

    // Only allow alphanumerical characters as field names (and prevent SQL injection).
    foreach ( const QString &fieldName, *fieldNames ) {
        if ( fieldName.contains(QRegExp("[^A-Z0-9_]", Qt::CaseInsensitive)) ) {
            kDebug() << "Field name contains disallowed characters:" << fieldName;
            return false;
        }
    }

    // Check required fields
    foreach ( const QString &requiredField, requiredFields ) {
        if ( !fieldNames->contains(requiredField) ) {
            kDebug() << "Required field missing:" << requiredField;
            kDebug() << "in this header line:" << header;
            return false;
        }
    }

    return true;
}

bool GeneralTransitFeedImporter::readFields( const QString& line, QVariantList *fieldValues,
                                             const QList<QVariant::Type> &fieldTypes,
                                             int expectedFieldCount )
{
    int pos = 0;
    QList<QVariant::Type>::ConstIterator fieldType = fieldTypes.constBegin();
    while ( pos < line.length() && fieldType != fieldTypes.constEnd() ) {
        int endPos = pos;
        QString newField;
        if ( line[pos] == '"' ) {
            // A field with a quotation mark in it must start and end with a quotation mark,
            // all other quotation marks must be preceded with another quotation mark
            ++endPos;
            while ( endPos < line.length() ) {
                if ( line[endPos] == '"' ) {
                    if ( endPos + 1 >= line.length() || line[endPos + 1] == ',' ) {
                        break; // At the end of the field / line
                    }

                    if ( line[endPos + 1] == '"' ) {
                        ++endPos; // Two quotation marks read, skip them
                    }
                }
                ++endPos;
            }
            if ( endPos >= line.length() || line[endPos] != '"' ) {
                return false; // Didn't find field end, wrong file format
            }

            // Add field value without the quotation marks around it
            // and doubled quotation marks replaced with single ones
            newField = line.mid( pos + 1, endPos - pos - 1 )
                    .replace( QLatin1String("\"\""), QLatin1String("\"") );
            pos = endPos + 2;
        } else if ( line[pos] == ',' ) {
            // Empty field, newField stays empty
            ++pos;
        } else {
            // Field without quotation marks, read until the next ','
            endPos = line.indexOf( ',', pos );
            if ( endPos < 0 ) {
                endPos = line.length();
            }

            // Add field value
            newField = line.mid( pos, endPos - pos );
            pos = endPos + 1;
        }

        // Append the new field value
        fieldValues->append( convertFieldValue(newField, *fieldType) );
        ++fieldType;
    }

    if ( fieldValues->count() < expectedFieldCount ) {
        kDebug() << "Header contains" << expectedFieldCount << "fields, but a line was read with only"
                 << fieldValues->count() << "field values, skipping:";
        kDebug() << "Values: " << *fieldValues;
        return false;
    }

    return true;
}

bool GeneralTransitFeedImporter::initDatabase( const QString &providerName )
{
    QSqlDatabase db = QSqlDatabase::addDatabase( "QSQLITE" );
    if ( !db.isValid() ) {
        kDebug() << "Error adding a QSQLITE database" << db.lastError();
        return false;
    }

    db.setDatabaseName( databasePath(providerName) );
    if ( !db.open() ) {
        kDebug() << "Error opening the database connection" << db.lastError();
        return false;
    }

    return true;
}

bool GeneralTransitFeedImporter::createDatabaseTables()
{
    QSqlQuery query;
    kDebug() << "Create tables";

    // Create table for "agency.txt" TODO agency_id only referenced from routes => merge tables?
    query.prepare( "CREATE TABLE IF NOT EXISTS agency ("
                   "agency_id INTEGER UNIQUE PRIMARY KEY, " // (optional for gtfs with a single agency)
                   "agency_name VARCHAR(256) NOT NULL, " // (required) The name of the agency
                   "agency_url VARCHAR(512) NOT NULL, " // (required) URL of the transit agency
                   "agency_timezone VARCHAR(256) NOT NULL, " // (required) Timezone name, see http://en.wikipedia.org/wiki/List_of_tz_zones
                   "agency_lang VARCHAR(2), " // (optional) A two-letter ISO 639-1 code for the primary language used by this transit agency
                   "agency_phone VARCHAR(64)" // (optional) A single voice telephone number for the agency (can contain punctuation marks)
//                    "agency_fare_url VARCHAR(512)" // (optional) URL of a website about fares of the transit agency (found in TriMet's GTFS data)
                   ")" );
    if( !query.exec() ) {
        qDebug() << "Error creating 'agency' table:" << query.lastError();
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
        qDebug() << "Error creating 'routes' table:" << query.lastError();
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
                   "position VARCHAR(30), "
                   "parent_station INTEGER" // (optional) stop_id of a parent station (with location_type == 1)
                   ");" );
    if( !query.exec() ) {
        qDebug() << "Error creating 'stops' table:" << query.lastError();
        return false;
    }
    query.prepare( "CREATE INDEX stops_stop_name_id ON stops(stop_id, stop_name);" );
    if( !query.exec() ) {
        qDebug() << "Error creating index for 'stop_name' in 'stops' table:" << query.lastError();
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
//         qDebug() << "Error creating 'shapes' table:" << query.lastError();
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
        qDebug() << "Error creating 'trips' table:" << query.lastError();
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
                   "PRIMARY KEY(stop_id, departure_time, trip_id)" //, stop_id)" // makes inserts slow..
                   ");" ); // CREATE INDEX id ON stop_times(stop_id, departure_time);" );
    if( !query.exec() ) {
        qDebug() << "Error creating 'stop_times' table:" << query.lastError();
        return false;
    }
//     query.prepare( "CREATE INDEX stop_times_trip_id ON stop_times(trip_id, stop_id, departure_time);" );
//     if( !query.exec() ) {
//         qDebug() << "Error creating index for 'trip_id' in 'stop_times' table:" << query.lastError();
//         return false;
//     }
    // Create an index to quickly access trip information sorted by stop_sequence,
    // eg. for route stop lists for departures
    query.prepare( "CREATE INDEX stop_times_trip ON stop_times(trip_id, stop_sequence, stop_id);" );
    if( !query.exec() ) {
        qDebug() << "Error creating index for 'trip_id' in 'stop_times' table:" << query.lastError();
        return false;
    }
//     query.prepare( "CREATE INDEX stop_times_trip_id3 ON stop_times(departure_time, trip_id, stop_id);" );
//     if( !query.exec() ) {
//         qDebug() << "Error creating index for 'trip_id' in 'stop_times' table:" << query.lastError();
//         return false;
//     }

    // Create table for "calendar.txt" (exceptions in "calendar_dates.txt")
    query.prepare( "CREATE TABLE IF NOT EXISTS calendar ("
                   "service_id INTEGER UNIQUE PRIMARY KEY NOT NULL, " // (required) Uiquely identifies a set of dates when service is available for one or more routes
                   "weekdays VARCHAR(7) NOT NULL, "
//                    "monday TINYINT  NOT NULL, " // (required) A binary value that indicates whether the service is valid for all Mondays (1: available on Mondays, 0: not available)
//                    "tuesday TINYINT NOT NULL, " // (required) A binary value that indicates whether the service is valid for all Tuesdays
//                    "wednesday TINYINT NOT NULL, " // (required) A binary value that indicates whether the service is valid for all Wednesdays
//                    "thursday TINYINT NOT NULL, " // (required) A binary value that indicates whether the service is valid for all Thursdays
//                    "friday TINYINT NOT NULL, " // (required) A binary value that indicates whether the service is valid for all Fridays
//                    "saturday TINYINT NOT NULL, " // (required) A binary value that indicates whether the service is valid for all Saturdays
//                    "sunday TINYINT NOT NULL, " // (required) A binary value that indicates whether the service is valid for all Sundays
                   "start_date VARCHAR(8) NOT NULL, " // (required) Contains the start date for the service, in YYYYMMDD format
                   "end_date VARCHAR(8) NOT NULL" // (required) Contains the end date for the service, in YYYYMMDD format
                   ")" );
    if( !query.exec() ) {
        qDebug() << "Error creating 'calendar' table:" << query.lastError();
        return false;
    }

    // Create table for "calendar_dates.txt"
    query.prepare( "CREATE TABLE IF NOT EXISTS calendar_dates ("
                   "service_id INTEGER PRIMARY KEY NOT NULL, " // (required) Uiquely identifies a set of dates when a service exception is available for one or more routes, Each (service_id, date) pair can only appear once in "calendar_dates", if the a service_id value appears in both "calendar" and "calendar_dates", the information in "calendar_dates" modifies the service information specified in "calendar", referenced by "trips"
                   "date VARCHAR(8) NOT NULL, " // (required) Specifies a particular date when service availability is different than the norm, in YYYYMMDD format
                   "exception_type TINYINT  NOT NULL" // (required) Indicates whether service is available on the date specified in the date field (1: The service has been added for the date, 2: The service has been removed)
                   ")" );
    if( !query.exec() ) {
        qDebug() << "Error creating 'calendar_dates' table:" << query.lastError();
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
        qDebug() << query.lastError();
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
                   "FOREIGN KEY(origin_id) REFERENCES stops(stop_id), "
                   "FOREIGN KEY(destination_id) REFERENCES stops(stop_id), "
                   "FOREIGN KEY(contains_id) REFERENCES stops(stop_id)"
                   ")" );
    if( !query.exec() ) {
        qDebug() << "Error creating 'fare_rules' table:" << query.lastError();
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
        qDebug() << "Error creating 'frequencies' table:" << query.lastError();
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
        qDebug() << "Error creating 'transfers' table:" << query.lastError();
        return false;
    }

    return true;
}

//  TODO Replace QVariant::Type with an own Enum
QVariant GeneralTransitFeedImporter::convertFieldValue( const QString &fieldValue,
                                                        QVariant::Type type )
{
    switch ( type ) {
    case QVariant::UInt:
        return qHash( fieldValue ); // Use the hash to convert string IDs
    case QVariant::Int:
        return fieldValue.toInt();
    case QVariant::Time: {
        // May contain hour values >= 24 (for times the next day), which is no valid QTime
        // Convert valid time format 'h:mm:ss' to 'hh:mm:ss'
        const QString timeString = fieldValue.length() == 7 ? '0' + fieldValue : fieldValue;
        return timeString.left(2).toInt() * 60 * 60 + timeString.mid(3, 2).toInt() * 60
                + timeString.right(2).toInt();
    } case QVariant::Date:
        return QDate::fromString( fieldValue, "yyyyMMdd" );
    case QVariant::Double:
        return fieldValue.toDouble();
    case QVariant::Url:
        return QUrl( fieldValue );
    case QVariant::Color:
        return QColor( '#' + fieldValue );
    case QVariant::String:
    default:
        return fieldValue;
    }
}

QVariant::Type GeneralTransitFeedImporter::typeOfField( const QString &fieldName )
{
    if ( fieldName == "min_transfer_time" || fieldName == "transfer_type" ||
         fieldName == "headway_secs" || fieldName == "transfer_duration" ||
         fieldName == "transfers" || fieldName == "payment_method" ||
         fieldName == "exception_type" || fieldName == "monday" || fieldName == "tuesday" ||
         fieldName == "thursday" || fieldName == "friday" || fieldName == "saturday" ||
         fieldName == "sunday" || fieldName == "shape_dist_traveled" ||
         fieldName == "drop_off_type" || fieldName == "pickup_type" ||
         fieldName == "stop_sequence" || fieldName == "shape_pt_sequence" ||
         fieldName == "parent_station" || fieldName == "location_type" ||
         fieldName == "route_type" )
    {
        return QVariant::Int;
    } else if ( fieldName.endsWith("_id") ) {
        return QVariant::UInt;
    } else if ( fieldName == "start_time" || fieldName == "end_time" ||
         fieldName == "arrival_time" || fieldName == "departure_time" )
    {
        return QVariant::Time; // A time stored as INTEGER
    } else if ( fieldName == "date" || fieldName == "startDate" || fieldName == "endDate" ) {
        return QVariant::Date;
    } else if ( fieldName.endsWith("_lat") || fieldName.endsWith("_lon") || fieldName == "price" ) {
        return QVariant::Double;
    } else if ( fieldName.endsWith("_url") ) {
        return QVariant::Url;
    } else if ( fieldName.endsWith("_color") ) {
        return QVariant::Color;
    } else {
        return QVariant::String;
    }
}
