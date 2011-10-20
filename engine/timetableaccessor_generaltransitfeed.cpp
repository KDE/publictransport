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
#include "timetableaccessor_info.h"
#include "departureinfo.h"
#include "publictransportservice.h"
#include "generaltransitfeed_realtime.h"

#include <Plasma/Service>
#include <KTimeZone>
#include <KDateTime>
#include <KStandardDirs>
#include <KDebug>
#include <KTemporaryFile>
#include <KLocalizedString>
#include <KLocale>
#include <KCurrencyCode>
#include <KConfigGroup>
#include <KIO/Job>

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QFileInfo>

TimetableAccessorGeneralTransitFeed::TimetableAccessorGeneralTransitFeed(
        TimetableAccessorInfo *info ) : TimetableAccessor(info),
        m_tripUpdates(0), m_alerts(0), m_service(0)
{
    m_state = Initializing;

    QString errorText;
    if ( !GeneralTransitFeedDatabase::initDatabase(info->serviceProvider(), &errorText) ) {
        kDebug() << "Error initializing the database" << errorText;
        m_state = ErrorInDatabase;
        return;
    }

    // Read accessor information cache
    const QString fileName = accessorCacheFileName();
    KConfig cfg( fileName, KConfig::SimpleConfig );
    KConfigGroup grp = cfg.group( m_info->serviceProvider() );
    bool importFinished = grp.readEntry( "feedImportFinished", false );

    QFileInfo fi( GeneralTransitFeedDatabase::databasePath(info->serviceProvider()) );
    if ( importFinished ) {
        if ( fi.exists() && fi.size() > 10000 ) {
            // Load agency information from database and request GTFS-realtime data
            loadAgencyInformation();
            updateRealtimeData();
            m_state = Ready;
        } else {
            // Accessor cache says the import has been finished, but the database file does not
            // exist or is empty
            grp.writeEntry( "feedImportFinished", false );
        }
    }

    // Update database to the current version of the GTFS feed
    updateGtfsData();
}

TimetableAccessorGeneralTransitFeed::~TimetableAccessorGeneralTransitFeed()
{
    // Free all agency objects
    qDeleteAll( m_agencyCache );

    // Free waiting request objects
    for ( QHash<QString,RequestInfo*>::ConstIterator it = m_waitingRequests.constBegin();
            it != m_waitingRequests.constEnd(); ++it )
    {
        delete *it;
    }
    m_waitingRequests.clear();

    delete m_tripUpdates;
    delete m_alerts;
}

void TimetableAccessorGeneralTransitFeed::updateGtfsData()
{
    if ( m_service ) {
        kDebug() << "Is already updating, please wait";
        return;
    }

    // Set state to UpdateGtfsFeed, if state was Ready, ie. if the database was already imported
    // and only gets updated now
    if ( m_state != Ready ) {
        m_state = UpdatingGtfsFeed;
        kDebug() << "Updating GTFS database for" << m_info->serviceProvider() << " please wait";
    } else {
        kDebug() << "Stays ready, updates GTFS database in background";
    }
    m_progress = 0.0;
    m_service = new PublicTransportService( QString(), this );
    KConfigGroup op = m_service->operationDescription("updateGtfsFeed");
    op.writeEntry( "serviceProviderId", m_info->serviceProvider() );
    Plasma::ServiceJob *job = m_service->startOperationCall( op );
    connect( job, SIGNAL(result(KJob*)), this, SLOT(importFinished(KJob*)) );
    connect( job, SIGNAL(percent(KJob*,ulong)), this, SLOT(importProgress(KJob*,ulong)) );
}

void TimetableAccessorGeneralTransitFeed::importProgress( KJob *job, ulong percent )
{
    Q_UNUSED( job );
    kDebug() << percent << m_info->serviceProvider() << m_state;
    m_progress = percent / 100.0;
    for ( QHash<QString,RequestInfo*>::ConstIterator it = m_waitingRequests.constBegin();
          it != m_waitingRequests.constEnd(); ++it )
    {
        emit progress( this, m_progress, i18nc("@info/plain TODO", "Importing GTFS feed"),
                       m_info->feedUrl(), *it );
    }
}

void TimetableAccessorGeneralTransitFeed::importFinished( KJob *job )
{
    m_progress = 1.0;
    if ( job->error() != 0 ) {
        // Error while importing
        kDebug() << "ERROR" << m_info->serviceProvider() << job->errorString();
        ErrorCode errorCode = job->error() == -7 ? ErrorNeedsImport : ErrorDownloadFailed;
        m_state = job->error() == -7 ? ErrorNeedsFeedImport : ErrorReadingFeed;
        for ( QHash<QString,RequestInfo*>::ConstIterator it = m_waitingRequests.constBegin();
              it != m_waitingRequests.constEnd(); ++it )
        {
            // TODO Less arguments to errorParsing and similiar functions...
            emit errorParsing( this, errorCode, /*i18nc("@info/plain TODO",
                    "Failed to import GTFS feed from <resource>%1</resource>: <message>%2</message>",
                    m_info->feedUrl(), */job->errorString()/*)*/,
                    m_info->feedUrl(), *it );
            delete *it;
        }
    } else {
        // Succesfully updated GTFS database
        kDebug() << "GTFS feed updated successfully" << m_info->serviceProvider();
        m_state = Ready;
        for ( QHash<QString,RequestInfo*>::ConstIterator it = m_waitingRequests.constBegin();
              it != m_waitingRequests.constEnd(); ++it )
        {
            if ( (*it)->parseMode == ParseForDeparturesArrivals ) {
                DepartureRequestInfo *requestInfo = static_cast<DepartureRequestInfo*>( *it );
                Q_ASSERT( requestInfo );
                requestDepartures( *requestInfo );
//                             DepartureRequestInfo(it.key(), (*it)->stop, (*it)->maxCount,
//                                    requestInfo->dateTime, (*it)->dataType, (*it)->useDifferentUrl,
//                                    (*it)->city) );
            } else if ( (*it)->parseMode == ParseForStopSuggestions ) {
                StopSuggestionRequestInfo *requestInfo =
                        static_cast<StopSuggestionRequestInfo*>( *it );
                Q_ASSERT( requestInfo );
                requestStopSuggestions( *requestInfo );
//                 requestStopSuggestions( it.key(), (*it)->city, (*it)->stop, ParseForStopSuggestions,
//                                         (*it)->maxCount, QDateTime()/* TODO (*it)->dateTime*/, (*it)->dataType,
//                                         (*it)->useDifferentUrl );
            } else {
                kDebug() << "Finished updating GTFS database, but unknown parse mode in a "
                            "waiting source" << (*it)->parseMode;
            }
            delete *it;
        }
    }

    m_waitingRequests.clear();
    m_service->deleteLater();
    m_service = 0;
}

bool TimetableAccessorGeneralTransitFeed::isRealtimeDataAvailable() const
{
    return !m_info->realtimeTripUpdateUrl().isEmpty() || !m_info->realtimeAlertsUrl().isEmpty();
}

void TimetableAccessorGeneralTransitFeed::updateRealtimeData()
{
//         m_state = DownloadingFeed;
    if ( !m_info->realtimeTripUpdateUrl().isEmpty() ) {
        KIO::StoredTransferJob *tripUpdatesJob = KIO::storedGet( m_info->realtimeTripUpdateUrl(),
                KIO::Reload, KIO::Overwrite | KIO::HideProgressInfo );
        connect( tripUpdatesJob, SIGNAL(result(KJob*)), this, SLOT(realtimeTripUpdatesReceived(KJob*)) );
        kDebug() << "Updating GTFS-realtime trip update data" << m_info->realtimeTripUpdateUrl();
    }

    if ( !m_info->realtimeAlertsUrl().isEmpty() ) {
        KIO::StoredTransferJob *alertsJob = KIO::storedGet( m_info->realtimeAlertsUrl(),
                KIO::Reload, KIO::Overwrite | KIO::HideProgressInfo );
        connect( alertsJob, SIGNAL(result(KJob*)), this, SLOT(realtimeAlertsReceived(KJob*)) );
        kDebug() << "Updating GTFS-realtime alerts data" << m_info->realtimeAlertsUrl();
    }

    if ( m_info->realtimeTripUpdateUrl().isEmpty() && m_info->realtimeAlertsUrl().isEmpty() ) {
        m_state = Ready;
    }
}

void TimetableAccessorGeneralTransitFeed::realtimeTripUpdatesReceived( KJob *job )
{
    KIO::StoredTransferJob *transferJob = qobject_cast<KIO::StoredTransferJob*>( job );
    if ( job->error() != 0 ) {
        kDebug() << "Error downloading GTFS-realtime trip updates:" << job->errorString();
//         m_state = ErrorDownloadingFeed;

//         for ( QHash<QString, JobInfos>::ConstIterator it = m_jobInfos.constBegin();
//               it != m_jobInfos.constEnd(); ++it )
//         {
//             emit errorParsing( this, ErrorDownloadFailed, i18nc("@info/plain TODO",
//                     "Failed to download GTFS feed from <resource>%1</resource>: <message>%2</message>",
//                     m_info->feedUrl(), job->errorString()),
//                     m_info->feedUrl(), m_info->serviceProvider(),
//                     it->sourceName, it->city, it->stop, it->dataType, it->parseDocumentMode );
//         }
        return;
    }

    delete m_tripUpdates;
    m_tripUpdates = GtfsRealtimeTripUpdate::fromProtocolBuffer( transferJob->data() );

    if ( m_alerts || m_info->realtimeAlertsUrl().isEmpty() ) {
        m_state = Ready;
    }
}

void TimetableAccessorGeneralTransitFeed::realtimeAlertsReceived( KJob *job )
{
    KIO::StoredTransferJob *transferJob = qobject_cast<KIO::StoredTransferJob*>( job );
    if ( job->error() != 0 ) {
        kDebug() << "Error downloading GTFS-realtime alerts:" << job->errorString();
//         m_state = ErrorDownloadingFeed;

//         for ( QHash<QString, JobInfos>::ConstIterator it = m_jobInfos.constBegin();
//               it != m_jobInfos.constEnd(); ++it )
//         {
//             emit errorParsing( this, ErrorDownloadFailed, i18nc("@info/plain TODO",
//                     "Failed to download GTFS feed from <resource>%1</resource>: <message>%2</message>",
//                     m_info->feedUrl(), job->errorString()),
//                     m_info->feedUrl(), m_info->serviceProvider(),
//                     it->sourceName, it->city, it->stop, it->dataType, it->parseDocumentMode );
//         }
        return;
    }

    delete m_alerts;
    m_alerts = GtfsRealtimeAlert::fromProtocolBuffer( transferJob->data() );

    if ( m_tripUpdates || m_info->realtimeTripUpdateUrl().isEmpty() ) {
        m_state = Ready;
    }
}

void TimetableAccessorGeneralTransitFeed::loadAgencyInformation()
{
    if ( m_state != Ready ) {
        return;
    }

    QSqlQuery query( QSqlDatabase::database(m_info->serviceProvider()) );
    if ( !query.exec("SELECT * FROM agency") ) {
        kDebug() << "Could not load agency information from database:" << query.lastError();
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
        agency->language = query.value( agencyLanguageColumn ).toString();
        agency->phone = query.value( agencyPhoneColumn ).toString();

        const QString timeZone = query.value(agencyTimezoneColumn).toString();
        agency->timezone = new KTimeZone( timeZone.isEmpty() ? m_info->timeZone() : timeZone );

        const uint id = query.value( agencyIdColumn ).toUInt();
        m_agencyCache.insert( id, agency );
    }
}

int TimetableAccessorGeneralTransitFeed::AgencyInformation::timeZoneOffset() const
{
    return timezone && timezone->isValid() ? timezone->currentOffset( Qt::LocalTime ) : 0;
}

qint64 TimetableAccessorGeneralTransitFeed::databaseSize() const
{
    QFileInfo fi( GeneralTransitFeedDatabase::databasePath(m_info->serviceProvider()) );
    return fi.size();
}

TimetableAccessorGeneralTransitFeed::AgencyInformation::~AgencyInformation()
{
    delete timezone;
}

QStringList TimetableAccessorGeneralTransitFeed::features() const
{
    QStringList list;
    list << "Autocompletion" << "TypeOfVehicle" << "Operator" << "StopID" << "RouteStops"
         << "RouteTimes" << "Arrivals";
    if ( !m_info->realtimeAlertsUrl().isEmpty() ) {
        list << "JourneyNews";
    }
    if ( !m_info->realtimeTripUpdateUrl().isEmpty() ) {
        list << "Delay";
    }
    return list;
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

bool TimetableAccessorGeneralTransitFeed::isGtfsFeedImportFinished()
{
    // Try to load accessor information from a cache file
    const QString fileName = accessorCacheFileName();
    const bool cacheExists = QFile::exists( fileName );
    KConfig cfg( fileName, KConfig::SimpleConfig );
    KConfigGroup grp = cfg.group( m_info->serviceProvider() );
// TODO
    kDebug() << "NOT YET IMPLEMENTED";
    if ( cacheExists ) {
        // Check if the GTFS feed file was modified since the cache was last updated
//         KDateTime gtfsModifiedTime = KDateTime::fromString( grp.readEntry("feedLastModified", QString()) );
// //         QDateTime gtfsModifiedTime = grp.readEntry("feedLastModified", QDateTime());
//         if ( QFileInfo(m_info->feedUrl()).lastModified() /*TODO*/ == gtfsModifiedTime ) {
//             // Return feature list stored in the cache
//             return grp.readEntry("feedImportFinished", false);
//         }
    }

    // No actual cached information about the service provider
    kDebug() << "No up-to-date cache information for service provider" << m_info->serviceProvider();

    return false;
}

bool TimetableAccessorGeneralTransitFeed::checkState( const RequestInfo *requestInfo )
{
    if ( m_state == Ready ) {
        return true;
    } else {
        switch ( m_state ) {
        case ErrorDownloadingFeed:
            emit errorParsing( this, ErrorDownloadFailed, i18nc("@info/plain",
                    "Failed to download the GTFS feed from <resource>%1</resource>",
                    m_info->feedUrl()), m_info->feedUrl(), requestInfo );
            break;
        case ErrorReadingFeed:
            emit errorParsing( this, ErrorParsingFailed, i18nc("@info/plain",
                    "Failed to read the GTFS feed from <resource>%1</resource>",
                    m_info->feedUrl()), m_info->feedUrl(), requestInfo );
            break;
        case ErrorNeedsFeedImport: {
            // Check, if the feed is imported now
            // Read accessor information cache
            // TODO function for this (also used in constructor)
            const QString fileName = accessorCacheFileName();
            KConfig cfg( fileName, KConfig::SimpleConfig );
            KConfigGroup grp = cfg.group( m_info->serviceProvider() );
            bool importFinished = grp.readEntry( "feedImportFinished", false );
            QFileInfo fi( GeneralTransitFeedDatabase::databasePath(m_info->serviceProvider()) );
            if ( importFinished && fi.exists() && fi.size() > 30000 ) {
                // Load agency information from database and request GTFS-realtime data
                loadAgencyInformation();
                updateRealtimeData();
                m_state = Ready;
                return true;
            } else {
                emit errorParsing( this, ErrorNeedsImport, i18nc("@info/plain",
                        "GTFS feed not imported from <resource>%1</resource>",
                        m_info->feedUrl()), m_info->feedUrl(), requestInfo );
            }
            break;
        }
        case Initializing:
            emit progress( this, 0.0, i18nc("@info/plain",
                    "Initializing GTFS feed database.",
                    m_info->feedUrl()), m_info->feedUrl(), requestInfo );
            break;
        case UpdatingGtfsFeed:
            emit progress( this, m_progress, i18nc("@info/plain",
                    "Updating GTFS feed database.",
                    m_info->feedUrl()), m_info->feedUrl(), requestInfo );
            break;
        default:
            emit errorParsing( this, ErrorParsingFailed, i18nc("@info/plain", "Busy, please wait.",
                    m_info->feedUrl()), m_info->feedUrl(), requestInfo );
            break;
        }

        kDebug() << "State" << m_state;
        if ( m_state == ErrorDownloadingFeed || m_state == ErrorReadingFeed ) {
            // Update database to the current version of the GTFS feed or import it for the first time
            kDebug() << "Restart update";
            updateGtfsData();
        }

        // Store information about the request to report import progress to
        if ( !m_waitingRequests.contains(requestInfo->sourceName) ) {
            m_waitingRequests[ requestInfo->sourceName ] = requestInfo->clone();
        }
        kDebug() << "Wait for GTFS feed download and import";
        return false;
    }
}

void TimetableAccessorGeneralTransitFeed::requestDepartures( const DepartureRequestInfo &requestInfo )
{
    if ( !checkState(&requestInfo) ) {
        return;
    }

    QSqlQuery query( QSqlDatabase::database(m_info->serviceProvider()) );
    query.setForwardOnly( true ); // Don't cache records

    // TODO If it is known that [stop] contains a stop ID testing for it's ID in stop_times is not necessary!
    // Try to get the ID for the given stop (fails, if it already is a stop ID). Only select
    // stops, no stations (with one or more sub stops) by requiring 'location_type=0',
    // location_type 1 is for stations.
    // It's fast, because 'stop_name' is part of a compound index in the database.
    uint stopId;
    QString stopValue = requestInfo.stop;
    stopValue.replace( '\'', "\'\'" );
    if ( !query.exec("SELECT stops.stop_id FROM stops "
                     "WHERE stop_name='" + stopValue + "' "
                     "AND (location_type IS NULL OR location_type=0)") )
    {
        // Check of the error is a "disk I/O error", ie. the database file may have been deleted
        checkForDiskIoErrorInDatabase( query.lastError(), &requestInfo );

        kDebug() << query.lastError();
        kDebug() << query.executedQuery();
        return;
    }

    QSqlRecord stopRecord = query.record();
    if ( query.next() ) {
        stopId = query.value( query.record().indexOf("stop_id") ).toUInt();
    } else {
        bool ok;
        stopId = requestInfo.stop.toUInt( &ok );
        if ( !ok ) {
            kDebug() << "No stop with the given name or id found (needs the exact name):"
                     << requestInfo.stop;
            emit errorParsing( this, ErrorParsingFailed /*TODO*/,
                    "No stop with the given name or id found (needs the exact name): "
                    + requestInfo.stop,
                    QUrl(), &requestInfo );
            return;
        }
    }

// This creates a temporary table to calculate min/max fares for departures.
// These values should be added into the db while importing, doing it here takes too long
//     const QString createJoinedFareTable = "CREATE TEMPORARY TABLE IF NOT EXISTS tmp_fares AS "
//             "SELECT * FROM fare_rules JOIN fare_attributes USING (fare_id);";
//     if ( !query.prepare(createJoinedFareTable) || !query.exec() ) {
//         kDebug() << "Error while creating a temporary table fore min/max fare calculation:"
//                  << query.lastError();
//         kDebug() << query.executedQuery();
//         return;
//     }

    // Query the needed departure info from the database.
    // It's fast, because all JOINs are done using INTEGER PRIMARY KEYs and
    // because 'stop_id' and 'departure_time' are part of a compound index in the database.
    // Sorting by 'arrival_time' may be a bit slower because is has no index in the database,
    // but if arrival_time values do not differ too much from the deaprture_time values, they
    // are also already sorted.
    // The tables 'calendar' and 'calendar_dates' are also fully implemented by the query below.
    // TODO: Create a new (temporary) table for each connected departure/arrival source and use
    //       that (much smaller) table here for performance reasons
    const QString routeSeparator = "||";
    const QTime time = requestInfo.dateTime.time();
    const QString queryString = QString(
            "SELECT times.departure_time, times.arrival_time, times.stop_headsign, "
                   "routes.route_type, routes.route_short_name, routes.route_long_name, "
                   "trips.trip_headsign, routes.agency_id, stops.stop_id, trips.trip_id, "
                   "routes.route_id, times.stop_sequence, "
                   "( SELECT group_concat(route_stop.stop_name, '%5') AS route_stops "
                     "FROM stop_times AS route_times INNER JOIN stops AS route_stop USING (stop_id) "
                     "WHERE route_times.trip_id=times.trip_id AND route_times.stop_sequence %4= times.stop_sequence "
                     "ORDER BY departure_time ) AS route_stops, "
                   "( SELECT group_concat(route_times.departure_time, '%5') AS route_times "
                     "FROM stop_times AS route_times "
                     "WHERE route_times.trip_id=times.trip_id AND route_times.stop_sequence %4= times.stop_sequence "
                     "ORDER BY departure_time ) AS route_times "
//                    "( SELECT min(price) FROM tmp_fares WHERE origin_id=stops.zone_id AND price>0 ) AS min_price, "
//                    "( SELECT max(price) FROM tmp_fares WHERE origin_id=stops.zone_id ) AS max_price, "
//                    "( SELECT currency_type FROM tmp_fares WHERE origin_id=stops.zone_id LIMIT 1 ) AS currency_type "
            "FROM stops INNER JOIN stop_times AS times USING (stop_id) "
                       "INNER JOIN trips USING (trip_id) "
                       "INNER JOIN routes USING (route_id) "
                       "LEFT JOIN calendar USING (service_id) "
                       "LEFT JOIN calendar_dates ON (trips.service_id=calendar_dates.service_id "
                                                    "AND strftime('%Y%m%d')=calendar_dates.date) "
            "WHERE stop_id=%1 AND departure_time>%2 "
                  "AND (calendar_dates.date IS NULL " // No matching record in calendar_dates table for today
                       "OR NOT (calendar_dates.exception_type=2)) " // Journey is not removed today
                  "AND (calendar.weekdays IS NULL " // No matching record in calendar table => always available
                       "OR (strftime('%Y%m%d') BETWEEN calendar.start_date " // Current date is in the range...
                                              "AND calendar.end_date " // ...where the service is available...
                           "AND substr(calendar.weekdays, strftime('%w') + 1, 1)='1') " // ...and it's available at the current weekday
                       "OR (calendar_dates.date IS NOT NULL " // Or there is a matching record in calendar_dates for today...
                           "AND calendar_dates.exception_type=1)) " // ...and this record adds availability of the service for today
            "ORDER BY departure_time "
            "LIMIT %3" )
            .arg( stopId )
            .arg( time.hour() * 60 * 60 + time.minute() * 60 + time.second() )
            .arg( requestInfo.maxCount )
            .arg( requestInfo.dataType == "arrivals" ? '<' : '>' ) // For arrivals route_stops/route_times need stops before the home stop
            .arg( routeSeparator );
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
    const int tripIdColumn = record.indexOf( "trip_id" );
    const int routeIdColumn = record.indexOf( "route_id" );
    const int stopIdColumn = record.indexOf( "stop_id" );
    const int arrivalTimeColumn = record.indexOf( "arrival_time" );
    const int departureTimeColumn = record.indexOf( "departure_time" );
    const int routeShortNameColumn = record.indexOf( "route_short_name" );
    const int routeLongNameColumn = record.indexOf( "route_long_name" );
    const int routeTypeColumn = record.indexOf( "route_type" );
    const int tripHeadsignColumn = record.indexOf( "trip_headsign" );
    const int stopSequenceColumn = record.indexOf( "stop_sequence" );
    const int stopHeadsignColumn = record.indexOf( "stop_headsign" );
    const int routeStopsColumn = record.indexOf( "route_stops" );
    const int routeTimesColumn = record.indexOf( "route_times" );
//     const int fareMinPriceColumn = record.indexOf( "min_price" );
//     const int fareMaxPriceColumn = record.indexOf( "max_price" );
//     const int fareCurrencyColumn  = record.indexOf( "currency_type" );

    // Prepare agency information, if only one is given, it is used for all records
    AgencyInformation *agency = 0;
    if ( m_agencyCache.count() == 1 ) {
        agency = m_agencyCache.values().first();
    }

    // Create a list of DepartureInfo objects from the query result
    QList<DepartureInfo*> departures;
    while ( query.next() ) {
        QDate arrivalDate = requestInfo.dateTime.date();
        QDate departureDate = requestInfo.dateTime.date();

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
        if ( requestInfo.dataType == "arrivals" ) {
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

        const QStringList routeStops = query.value(routeStopsColumn).toString().split( routeSeparator );
        if ( routeStops.isEmpty() ) {
            // This happens, if the current departure is actually no departure, but an arrival at
            // the target station and vice versa for arrivals.
            continue;
        }
        data[ RouteStops ] = routeStops;
        data[ RouteExactStops ] = routeStops.count();

        const QStringList routeTimeValues = query.value(routeTimesColumn).toString().split( routeSeparator );
        QVariantList routeTimes;
        foreach ( const QString routeTimeValue, routeTimeValues ) {
            routeTimes << timeFromSecondsSinceMidnight( routeTimeValue.toInt(), &arrivalDate );
        }
        data[ RouteTimes ] = routeTimes;

//         const QString symbol = KCurrencyCode( query.value(fareCurrencyColumn).toString() ).defaultSymbol();
//         data[ Pricing ] = KGlobal::locale()->formatMoney(
//                 query.value(fareMinPriceColumn).toDouble(), symbol ) + " - " +
//                 KGlobal::locale()->formatMoney( query.value(fareMaxPriceColumn).toDouble(), symbol );

        if ( m_alerts ) {
            QStringList journeyNews;
            QString journeyNewsLink;
            foreach ( const GtfsRealtimeAlert &alert, *m_alerts ) {
                if ( alert.isActiveAt(QDateTime::currentDateTime()) ) {
                    journeyNews << alert.description;
                    journeyNewsLink = alert.url;
                }
            }
            if ( !journeyNews.isEmpty() ) {
                data[ JourneyNews ] = journeyNews.join( ", " );
                data[ JourneyNewsLink ] = journeyNewsLink;
            }
        }

        if ( m_tripUpdates ) {
            uint tripId = query.value(tripIdColumn).toUInt();
            uint routeId = query.value(routeIdColumn).toUInt();
            uint stopId = query.value(stopIdColumn).toUInt();
            uint stopSequence = query.value(stopSequenceColumn).toUInt();
            foreach ( const GtfsRealtimeTripUpdate &tripUpdate, *m_tripUpdates ) {
                if ( (tripUpdate.tripId > 0 && tripId == tripUpdate.tripId) ||
                     (tripUpdate.routeId > 0 && routeId == tripUpdate.routeId) ||
                     (tripUpdate.tripId <= 0 && tripUpdate.routeId <= 0) )
                {
                    kDebug() << "tripId or routeId matched or not queried!";
                    foreach ( const GtfsRealtimeStopTimeUpdate &stopTimeUpdate,
                              tripUpdate.stopTimeUpdates )
                    {
                        if ( (stopTimeUpdate.stopId > 0 && stopId == stopTimeUpdate.stopId) ||
                             (stopTimeUpdate.stopSequence > 0 &&
                              stopSequence == stopTimeUpdate.stopSequence) ||
                             (stopTimeUpdate.stopId <= 0 && stopTimeUpdate.stopSequence <= 0) )
                        {
                            kDebug() << "stopId matched or stopsequence matched or not queried!";
                            // Found a matching stop time update
                            kDebug() << "Delays:" << stopTimeUpdate.arrivalDelay << stopTimeUpdate.departureDelay;
                        }
                    }
                }
            }
        }

        departures << new DepartureInfo( data );
    }

    // TODO Do not use a list of pointers here, maybe use data sharing for PublicTransportInfo/StopInfo?
    // The objects in departures are deleted in a connected slot in the data engine...
    emit departureListReceived( this, QUrl(), departures, GlobalTimetableInfo(), &requestInfo );
}

void TimetableAccessorGeneralTransitFeed::requestStopSuggestions(
        const StopSuggestionRequestInfo &requestInfo )
{
    if ( !checkState(&requestInfo) ) {
        return;
    }

    QSqlQuery query( QSqlDatabase::database(m_info->serviceProvider()) );
    query.setForwardOnly( true );
    QString stopValue = requestInfo.stop;
    stopValue.replace( '\'', "\'\'" );
    if ( !query.prepare(QString("SELECT * FROM stops WHERE stop_name LIKE '%%1%' LIMIT %2")
                        .arg(stopValue).arg(STOP_SUGGESTION_LIMIT))
         || !query.exec() )
    {
        // Check of the error is a "disk I/O error", ie. the database file may have been deleted
        checkForDiskIoErrorInDatabase( query.lastError(), &requestInfo );
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
        int weight = stopName == requestInfo.stop ? 100
                : 84 - qMin( 84, qAbs(stopName.length() - requestInfo.stop.length()) );

        if ( weight < 100 && stopName.startsWith(requestInfo.stop) ) {
            // 15 weight points bonus if the found stop name starts with the search string
            weight = qMin( 100, weight + 15 );
        }
        if ( weight < 100 ) {
            // Test if the search string is the start of a new word in stopName
            // Start at 2, because startsWith is already tested above and at least a space must
            // follow to start a new word
            int pos = stopName.indexOf( requestInfo.stop, 2, Qt::CaseInsensitive );

            if ( pos != -1 && stopName[pos - 1].isSpace() ) {
                // 10 weight points bonus if a word in the found stop name
                // starts with the search string
                weight = qMin( 100, weight + 10 );
            }
        }
        stops << new StopInfo( stopName, query.value(stopIdColumn).toString(), weight,
                               requestInfo.city );
    }

    if ( stops.isEmpty() ) {
        kDebug() << "No stop names found";
    }
    emit stopListReceived( this, QUrl(), stops, &requestInfo );

    // Cleanup
    foreach ( StopInfo *stop, stops ) {
        delete stop;
    }
}

bool TimetableAccessorGeneralTransitFeed::checkForDiskIoErrorInDatabase( const QSqlError &error,
        const RequestInfo *requestInfo )
{
    Q_UNUSED( requestInfo );

    // Check of the error is a "disk I/O" error or a "no such table" error, ie. the database file
    // may have been deleted.
    // The error numbers (1, 10) is database dependend and works with SQLITE
    if ( error.number() == 10 || error.number() == 1 ) {
        kDebug() << "Disk I/O error reported from database, recreate the database";

        m_state = Initializing;
        QString errorText;
        if ( !GeneralTransitFeedDatabase::initDatabase(m_info->serviceProvider(), &errorText) ) {
            kDebug() << "Error initializing the database" << errorText;
            m_state = ErrorInDatabase;
            return true;
        }

        QFileInfo fi( GeneralTransitFeedDatabase::databasePath(m_info->serviceProvider()) );
        if ( !fi.exists() || fi.size() < 50000 ) {
            // If the database does not exist or is too small, get information about the GTFS feed,
            // download it, create the database and import the feed into it
//             statFeed();
        } else {
            loadAgencyInformation();
            updateRealtimeData();
        }
        return true;
    } else {
        return false;
    }
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
