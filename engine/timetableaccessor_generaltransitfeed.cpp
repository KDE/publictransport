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

#include "timetableaccessor_generaltransitfeed.h"

#include "generaltransitfeed_importer.h"

#include <KTimeZone>
#include <KStandardDirs>
#include <KDebug>
#include <KTemporaryFile>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QFileInfo>
#include <KLocalizedString>

TimetableAccessorGeneralTransitFeed::TimetableAccessorGeneralTransitFeed(
        TimetableAccessorInfo *info ) : TimetableAccessor(info)
{
    m_state = Initializing;
    GeneralTransitFeedImporter::initDatabase( info->serviceProvider() );
    QFileInfo fi( GeneralTransitFeedImporter::databasePath(info->serviceProvider()) );
    if ( !fi.exists() || fi.size() < 50000 ) {
        // If the database does not exist or is too small, (re)create it
        // TODO Do this in a thread and report import progress
        GeneralTransitFeedImporter::createDatabaseTables();
        downloadFeed();
    } else {
        loadAgencyInformation();
        m_state = Ready;
    }
}

TimetableAccessorGeneralTransitFeed::~TimetableAccessorGeneralTransitFeed()
{
    // Free all agency objects
    qDeleteAll( m_agencyCache );
}

void TimetableAccessorGeneralTransitFeed::downloadFeed()
{
    KTemporaryFile tmpFile;
    if ( tmpFile.open() ) {
        kDebug() << "Downloading GTFS feed from" << m_info->feedUrl() << "to" << tmpFile.fileName();
        m_state = DownloadingFeed;
        KIO::FileCopyJob *job = KIO::file_copy( m_info->feedUrl(), KUrl(tmpFile.fileName()), -1,
                                                KIO::Overwrite | KIO::HideProgressInfo );
        connect( job, SIGNAL(result(KJob*)), this, SLOT(feedReceived(KJob*)) );
    } else {
        kDebug() << "Could not create a temporary file to download the GTFS feed";
    }
}

void TimetableAccessorGeneralTransitFeed::feedReceived( KJob *job )
{
    kDebug() << "--------------------------------------------------------------------------------";
    if ( job->error() != 0 ) {
        kDebug() << "Error downloading GTFS feed:" << job->errorString();
        m_state = ErrorDownloadingFeed;
        return;
    }

    KIO::FileCopyJob *fileCopyJob = qobject_cast<KIO::FileCopyJob*>( job );
    QString tmpFilePath = fileCopyJob->destUrl().path();
    kDebug() << "GTFS feed received at" << tmpFilePath;

    // Read feed and write data into the DB
    m_state = ReadingFeed;
    if ( GeneralTransitFeedImporter::writeGtfsDataToDatabase(tmpFilePath) ) {
        loadAgencyInformation();
        m_state = Ready;
    } else {
        kDebug() << "There was an error writing the GTFS data to the database";
        m_state = ErrorReadingFeed;
    }

    // Remove temporary file
    if ( !QFile::remove(tmpFilePath) ) {
        kDebug() << "Could not remove the temporary GTFS feed file";
    }
}

void TimetableAccessorGeneralTransitFeed::loadAgencyInformation()
{
    if ( m_state != Ready ) {
        return;
    }

    QSqlQuery query;
    if ( !query.exec("SELECT * FROM agency") ) {
        kDebug() << "Could not load agency information from DB:" << query.lastError();
        return;
    }

    // Clear previously loaded agency data
    qDeleteAll( m_agencyCache );
    m_agencyCache.clear();

    // Read agency records from the database
    QSqlRecord record = query.record();
    const int agencyIdColumn = record.indexOf( "agency_id" );
    const int agencyNameColumn = record.indexOf( "agency_name" );
    const int agencyUrlColumn = record.indexOf( "agency_url" );
    const int agencyTimezoneColumn = record.indexOf( "agency_timezone" );
    const int agencyLanguageColumn = record.indexOf( "agency_lang" );
    const int agencyPhoneColumn = record.indexOf( "agency_phone" );
    while ( query.next() ) {
        AgencyInformation *agency = new AgencyInformation;
        agency->name = query.value( agencyNameColumn ).toString();
        agency->url = query.value( agencyUrlColumn ).toString();
        agency->timezone = new KTimeZone( query.value(agencyTimezoneColumn).toString() );
        agency->language = query.value( agencyLanguageColumn ).toString();
        agency->phone = query.value( agencyPhoneColumn ).toString();

        const uint id = query.value( agencyIdColumn ).toUInt();
        m_agencyCache.insert( id, agency );
    }
}

int TimetableAccessorGeneralTransitFeed::AgencyInformation::timeZoneOffset() const
{
    return timezone && timezone->isValid() ? timezone->currentOffset( Qt::LocalTime ) : 0;
}

TimetableAccessorGeneralTransitFeed::AgencyInformation::~AgencyInformation()
{
    delete timezone;
}

QTime TimetableAccessorGeneralTransitFeed::timeFromSecondsSinceMidnight(
        int secondsSinceMidnight, QDate *date ) const
{
    const int secondsInOneDay = 60 * 60 * 24;
    while ( secondsSinceMidnight >= secondsInOneDay ) {
        secondsSinceMidnight -= secondsInOneDay;
        if ( date ) {
            date->addDays( 1 );
        }
    }
    return QTime( secondsSinceMidnight / (60 * 60),
                  (secondsSinceMidnight / 60) % 60,
                  secondsSinceMidnight % 60 );
}

void TimetableAccessorGeneralTransitFeed::requestDepartures( const QString &sourceName,
        const QString &city, const QString &stop, int maxCount, const QDateTime &dateTime,
        const QString &dataType, bool useDifferentUrl )
{
    if ( m_state != Ready ) {
        switch ( m_state ) {
        case ErrorDownloadingFeed:
            emit errorParsing( this, ErrorDownloadFailed, i18nc("@info TODO",
                    "Failed to download the GTFS feed from <resource>%1</resource>",
                    m_info->feedUrl()), m_info->feedUrl(), m_info->serviceProvider(),
                    sourceName, city, stop, dataType, ParseForDeparturesArrivals );
            break;
        case ErrorReadingFeed:
            emit errorParsing( this, ErrorParsingFailed, i18nc("@info TODO",
                    "Failed to read the GTFS feed from <resource>%1</resource>",
                    m_info->feedUrl()), m_info->feedUrl(), m_info->serviceProvider(),
                    sourceName, city, stop, dataType, ParseForDeparturesArrivals );
            break;
        case DownloadingFeed:
        case ReadingFeed:
        default:
//             TODO
            emit errorParsing( this, ErrorParsingFailed, i18nc("@info TODO",
                    "Currently downloading and importing GTFS feed into database, please wait.",
                    m_info->feedUrl()), m_info->feedUrl(), m_info->serviceProvider(),
                    sourceName, city, stop, dataType, ParseForDeparturesArrivals );
            break;
        }
        return;
    }

    QSqlQuery query;

    // TODO If it is known that [stop] contains a stop ID testing for it's ID in stop_times is not necessary!
    // Try to get the ID for the given stop (fails, if it already is a stop ID). Only select
    // stops, no stations (with one or more sub stops) by requiring 'location_type=0',
    // location_type 1 is for stations.
    // It's fast, because 'stop_name' is part of a compound index in the database.
    uint stopId;
    QString stopValue = stop;
    stopValue.replace( '\'', "\'\'" );
    if ( !query.exec("SELECT stops.stop_id FROM stops "
                     "WHERE stop_name='" + stopValue + "' AND location_type=0") )
    {
        kDebug() << query.lastError();
        kDebug() << query.executedQuery();
        return;
    }

    QSqlRecord stopRecord = query.record();
    if ( query.next() ) {
        stopId = query.value( query.record().indexOf("stop_id") ).toUInt();
    } else {
        bool ok;
        stopId = stop.toUInt( &ok );
        if ( !ok ) {
            kDebug() << "No stop with the given name or id found (needs the exact name):" << stop;
            emit errorParsing( this, ErrorParsingFailed /*TODO*/,
                    "No stop with the given name or id found (needs the exact name): " + stop,
                    QUrl(), serviceProvider(), sourceName, city, stop, dataType,
                    ParseForDeparturesArrivals );
            return;
        }
    }

    // Query the needed departure info from the database.
    // It's fast, because all JOINs are done using INTEGER PRIMARY KEYs and
    // because 'stop_id' and 'departure_time' are part of a compound index in the database.
    // Sorting by 'arrival_time' may be a bit slower because is has no index in the database,
    // but if arrival_time values do not differ too much from the deaprture_time values, they
    // are also already sorted.
    // TODO Use 'calendar' and 'calendar_dates' tables, merge weekday fields in calendar into one...
    const QTime time = dateTime.time();
    const QString queryString = QString(
            "SELECT times.departure_time, times.arrival_time, times.stop_headsign, "
                   "routes.route_type, routes.route_short_name, routes.route_long_name, "
                   "trips.trip_headsign, routes.agency_id, "
                   "( SELECT group_concat(route_stop.stop_name) AS route_stops "
                     "FROM stop_times AS route_times INNER JOIN stops AS route_stop USING (stop_id) "
                     "WHERE route_times.trip_id=times.trip_id AND route_times.stop_sequence >= times.stop_sequence "
                     "ORDER BY departure_time ) AS route_stops, "
                   "( SELECT group_concat(route_times.departure_time) AS route_times "
                     "FROM stop_times AS route_times "
                     "WHERE route_times.trip_id=times.trip_id AND route_times.stop_sequence >= times.stop_sequence "
                     "ORDER BY departure_time ) AS route_times "
            "FROM stops INNER JOIN stop_times AS times USING (stop_id) "
                       "INNER JOIN trips USING (trip_id) "
                       "INNER JOIN routes USING (route_id) "
//                        "LEFT JOIN calendar USING (service_id) "
//                        "LEFT JOIN calendar_dates USING (service_id) "
            "WHERE stop_id=%1 AND departure_time>%3 "
//              "AND (NOT EXISTS (SELECT * FROM calendar_dates) OR calendar_dates.exception_type!=2) "
//             Test calendar table with another feed (one which actually uses that table, not TriMet)
//              "AND (NOT EXISTS (SELECT * FROM calendar) OR calendar.weekdays & strftime('%w') > 0) " // TODO CASE strftime('%w') WHEN 0 THEN calendar.sunday WHEN 1 THEN ... END
            "ORDER BY departure_time "
            "LIMIT %4" )
            .arg( stopId )
            .arg( time.hour() * 60 * 60 + time.minute() * 60 + time.second() )
            .arg( maxCount );
    query.setForwardOnly( true ); // Don't cache records
    if ( !query.prepare(queryString) || !query.exec() ) {
        kDebug() << "Error while querying for departures:" << query.lastError();
        kDebug() << query.executedQuery();
        return;
    }
    if ( query.size() == 0 ) {
        kDebug() << "Got an empty record";
        return;
    }
    kDebug() << "Query executed";
    kDebug() << query.executedQuery();

    QSqlRecord record = query.record();
    const int agencyIdColumn = record.indexOf( "agency_id" );
    const int arrivalTimeColumn = record.indexOf( "arrival_time" );
    const int departureTimeColumn = record.indexOf( "departure_time" );
    const int routeShortNameColumn = record.indexOf( "route_short_name" );
    const int routeLongNameColumn = record.indexOf( "route_long_name" );
    const int routeTypeColumn = record.indexOf( "route_type" );
    const int tripHeadsignColumn = record.indexOf( "trip_headsign" );
    const int stopHeadsignColumn = record.indexOf( "stop_headsign" );
    const int routeStopsColumn = record.indexOf( "route_stops" );
    const int routeTimesColumn = record.indexOf( "route_times" );

    // Prepare agency information, if only one is given, it is used for all records
    AgencyInformation *agency = 0;
    if ( m_agencyCache.count() == 1 ) {
        agency = m_agencyCache.values().first();
    }

    QList<DepartureInfo*> departures;
    while ( query.next() ) {
        QDate arrivalDate = dateTime.date();
        QDate departureDate = dateTime.date();

        // Load agency information from cache
        const QVariant agencyIdValue = query.value( agencyIdColumn );
        if ( m_agencyCache.count() > 1 ) {
            Q_ASSERT( agencyIdValue.isValid() ); // GTFS says, that agency_id can only be null, if there is only one agency
            agency = m_agencyCache[ agencyIdValue.toUInt() ];
        }

        // Time values are stored as seconds since midnight of the associated date
        int arrivalTimeValue = query.value(arrivalTimeColumn).toInt();
        int departureTimeValue = query.value(departureTimeColumn).toInt();

        QDateTime arrivalTime( arrivalDate,
                               timeFromSecondsSinceMidnight(arrivalTimeValue, &arrivalDate) );
        QDateTime departureTime( departureDate,
                                 timeFromSecondsSinceMidnight(departureTimeValue, &departureDate) );

        // Apply timezone offset
        int offsetSeconds = agency ? agency->timeZoneOffset() : 0;
        if ( offsetSeconds != 0 ) {
            arrivalTime.addSecs( offsetSeconds );
            departureTime.addSecs( offsetSeconds );
        }

        QHash<TimetableInformation, QVariant> data;
        if ( dataType == "arrivals" ) {
            data[ DepartureDate ] = arrivalTime.date();
            data[ DepartureTime ] = arrivalTime.time();
        } else {
            data[ DepartureDate ] = departureTime.date();
            data[ DepartureTime ] = departureTime.time();
        }
        data[ TypeOfVehicle ] = vehicleTypeFromGtfsRouteType( query.value(routeTypeColumn).toInt() );
        data[ Operator ] = agency ? agency->name : QString();

        const QString transportLine = query.value(routeShortNameColumn).toString();
        data[ TransportLine ] = !transportLine.isEmpty() ? transportLine
                         : query.value(routeLongNameColumn).toString();

        const QString tripHeadsign = query.value(tripHeadsignColumn).toString();
        data[ Target ] = !tripHeadsign.isEmpty() ? tripHeadsign
                         : query.value(stopHeadsignColumn).toString();

        const QStringList routeStops = query.value(routeStopsColumn).toString().split( ',' );
        data[ RouteStops ] = routeStops;
        data[ RouteExactStops ] = routeStops.count();

        const QStringList routeTimeValues = query.value(routeTimesColumn).toString().split( ',' );
        QVariantList routeTimes;
        foreach ( const QString routeTimeValue, routeTimeValues ) {
            routeTimes << timeFromSecondsSinceMidnight( routeTimeValue.toInt(), &arrivalDate );
        }
        data[ RouteTimes ] = routeTimes;

        departures << new DepartureInfo( data );
    }

    emit departureListReceived( this, QUrl(), departures, GlobalTimetableInfo(), serviceProvider(),
                                sourceName, city, stop, dataType, ParseForDeparturesArrivals );
}

void TimetableAccessorGeneralTransitFeed::requestStopSuggestions( const QString &sourceName,
        const QString &city, const QString &stop, ParseDocumentMode parseMode, int maxCount,
        const QDateTime &dateTime, const QString &dataType, bool usedDifferentUrl )
{
    QSqlQuery query;
    QString stopValue = stop;
    stopValue.replace( '\'', "\'\'" );
    if ( !query.prepare(QString("SELECT * FROM stops WHERE stop_name LIKE '%%1%' LIMIT %2")
                        .arg(stopValue).arg(STOP_SUGGESTION_LIMIT))
         || !query.exec() )
    {
        kDebug() << query.lastError();
        kDebug() << query.executedQuery();
        return;
    }

    QSqlRecord record = query.record();
    const int stopIdColumn = record.indexOf( "stop_id" );
    const int stopNameColumn = record.indexOf( "stop_name" );
//     int stopLatitudeColumn = record.indexOf( "stop_lat" ); TODO
//     int stopLongitudeColumn = record.indexOf( "stop_lon" );

    QList<StopInfo*> stops;
    while ( query.next() ) {
        const QString stopName = query.value(stopNameColumn).toString();

        // Compute a weight value for the found stop name.
        // The less different the found stop name is compared to the search string, the higher
        // it's weight gets. If the found name equals the search string, the weight becomes 100.
        // Use 84 as maximal starting weight value (if stopName doesn't equal the search string),
        // because maximally 15 bonus points are added which makes 99, less than total equality 100.
        int weight = stopName == stop ? 100
                : 84 - qMin( 84, qAbs(stopName.length() - stop.length()) );

        if ( weight < 100 && stopName.startsWith(stop) ) {
            // 15 weight points bonus if the found stop name starts with the search string
            weight = qMin( 100, weight + 15 );
        }
        if ( weight < 100 ) {
            // Test if the search string is the start of a new word in stopName
            // Start at 2, because startsWith is already tested above and at least a space must 
            // follow to start a new word
            int pos = stopName.indexOf( stop, 2, Qt::CaseInsensitive );

            if ( pos != -1 && stopName[pos - 1].isSpace() ) {
                // 10 weight points bonus if a word in the found stop name 
                // starts with the search string
                weight = qMin( 100, weight + 10 );
            }
        }
        stops << new StopInfo( stopName, query.value(stopIdColumn).toString(), weight, city );
    }

    emit stopListReceived( this, QUrl(), stops, serviceProvider(), sourceName, city, stop,
                           dataType, parseMode );
}

VehicleType TimetableAccessorGeneralTransitFeed::vehicleTypeFromGtfsRouteType(
        int gtfsRouteType ) const
{
    switch ( gtfsRouteType ) {
    case 0: // Tram, Streetcar, Light rail. Any light rail or street level system within a metropolitan area.
        return Tram;
    case 1: // Subway, Metro. Any underground rail system within a metropolitan area.
        return Subway;
    case 2: // Rail. Used for intercity or long-distance travel.
        return TrainIntercityEurocity;
    case 3: // Bus. Used for short- and long-distance bus routes.
        return Bus;
    case 4: // Ferry. Used for short- and long-distance boat service.
        return Ferry;
    case 5: // Cable car. Used for street-level cable cars where the cable runs beneath the car.
        return TrolleyBus;
    case 6: // Gondola, Suspended cable car. Typically used for aerial cable cars where the car is suspended from the cable.
        return Unknown; // TODO Add new type to VehicleType: eg. Gondola
    case 7: // Funicular. Any rail system designed for steep inclines.
        return Unknown; // TODO Add new type to VehicleType: eg. Funicular
    default:
        return Unknown;
    }
}
