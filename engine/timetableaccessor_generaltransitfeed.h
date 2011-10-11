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

/** @file
* @brief This file contains a class to access data from GTFS feeds.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_GENERALRANSITFEED_HEADER
#define TIMETABLEACCESSOR_GENERALRANSITFEED_HEADER

#include "timetableaccessor.h"
#include "generaltransitfeed_importer.h"
#include "generaltransitfeed_realtime.h"

class PublicTransportService;
class QNetworkReply;
class KTimeZone;

/** @class TimetableAccessorGeneralTransitFeed
* @brief This class uses a database similiar to the GTFS structure to access public transport data.
*
* To fill the database with data from a GeneralTransitFeedSpecification feed (zip file)
* @ref GeneralTransitFeedImporter is used by @ref PublicTransportService. This service has an
* operation "UpdateGtfsFeed", which gets called by this class. That operation only updates already
* imported GTFS feeds if there is a new version. To import a new GTFS feed for the first time
* the operation "ImportGtfsFeed" should be used. That operation does @em not get called by this
* class. This is because importing GTFS feeds can require quite a lot disk space and importing
* can take some time. The user should be asked to import a new GTFS feed.
*
* This class does not use the default functions in @ref TimetableAccessor to request data. Instead
* these functions are overwritten to query a database for results and directly emitting a received
* signal. Requesting data from the database is very fast even for big databases (eg. 300MB).
*/
class TimetableAccessorGeneralTransitFeed : public TimetableAccessor
{
    Q_OBJECT
public:
    /**
     * @brief Holds information about a public transport agency.
     *
     * All agencies used in the GTFS feed are stored in the accessor for fast access.
     * For most GTFS feeds, there is only one agency. For others, there are only a few agencies.
     **/
    struct AgencyInformation {
        AgencyInformation() : timezone(0) {};
        virtual ~AgencyInformation();

        /** @brief Gets the offset in seconds for the agency's timezone. */
        int timeZoneOffset() const;

        QString name;
        QString phone;
        QString language;
        KUrl url;
        KTimeZone *timezone;
    };

    /** @brief Typedef to store agency information of all agencies in the GTFS feed by ID. */
    typedef QHash<uint, AgencyInformation*> AgencyInformations;

    /** @brief The maximum of stop suggestion to return. */
    static const int STOP_SUGGESTION_LIMIT = 100;

    explicit TimetableAccessorGeneralTransitFeed(
            TimetableAccessorInfo *info = new TimetableAccessorInfo() );
    virtual ~TimetableAccessorGeneralTransitFeed();

    /** @brief Returns the type of this accessor, ie. GtfsAccessor. */
    virtual AccessorType type() const { return GtfsAccessor; };

    /** @brief Checks if there was an error. */
    bool hasErrors() const { return m_state >= 10; }

    /** @brief Gets a list of features that this accessor supports. */
    virtual QStringList features() const;

    /** @brief Returns true, if there is a GTFS-realtime source available. */
    bool isRealtimeDataAvailable() const;

protected slots:
    /**
     * @brief GTFS-realtime TripUpdates data received.
     *
     * TripUpdates are realtime updates to departure/arrival times, ie. delays.
     **/
    void realtimeTripUpdatesReceived( KJob *job );

    /**
     * @brief GTFS-realtime Alerts data received.
     *
     * Alerts contain journey information for specific departures/arrivals.
     **/
    void realtimeAlertsReceived( KJob *job );

    void importFinished( KJob *job );
    void importProgress( KJob *job, ulong percent );

protected:
    /**
     * @brief Requests a list of departures from the GTFS database.
     *
     * @param sourceName The source name in the data engine.
     * @param city The city in which the @p stop is, or an empty string if not required by the
     *   service provider. Most providers do not require a city value.
     * @param stop The name or ID of the stop to get departures for.
     * @param maxCount The maximum number of departures/arrivals to get.
     * @param dateTime The date and time of the first departure to get.
     * @param dataType This should be left as "departures".
     * @param usedDifferentUrl Is not used by this class. Default is false.
     **/
    virtual void requestDepartures( const QString &sourceName, const QString &city,
            const QString &stop, int maxCount, const QDateTime &dateTime,
            const QString &dataType = "departures", bool useDifferentUrl = false );

    /**
     * @brief Requests a list of stops from the GTFS database.
     *
     * @param sourceName The source name in the data engine.
     * @param city The city in which the @p stop is, or an empty string if not required by the
     *   service provider. Most providers do not require a city value.
     * @param stop The name or ID of the stop to get departures for.
     * @param parseMode The parse mode can be @ref ParseForStopSuggestions or
     *   @ref ParseForDepartureArrivals, if departures/arrivals were requested but the stop name
     *   was ambiguous.
     * @param maxCount The maximum number of stops to get.
     * @param dateTime The date and time of the request.
     * @param dataType This should be left empty
     * @param usedDifferentUrl Is not used by this class. Default is false.
     **/
    virtual void requestStopSuggestions( const QString &sourceName, const QString &city,
            const QString &stop, ParseDocumentMode parseMode = ParseForStopSuggestions,
            int maxCount = 1, const QDateTime& dateTime = QDateTime::currentDateTime(),
            const QString &dataType = QString(), bool usedDifferentUrl = false );

    /** @brief Updates the GTFS feed data using @ref PublicTransportService. */
    void updateGtfsData();

    /** @brief Updates the GTFS-realtime data, ie. delays and journey news. */
    void updateRealtimeData();

    /**
     * @brief Returns true if the GTFS feed has been initially imported.
     *
     * This accessor can only be used, if this function returns true. The GTFS feed needs to be
     * completely imported once. Afterwards updates are done automatically in the background to
     * update to new GTFS feed versions.
     **/
    bool isGtfsFeedImportFinished();

    bool checkState( const QString &sourceName,
            const QString &city, const QString &stop, int maxCount, const QDateTime &dateTime,
            const QString &dataType, bool useDifferentUrl, ParseDocumentMode parseMode );
    bool checkForDiskIoErrorInDatabase( const QSqlError &error, const QString &sourceName,
            const QString &city, const QString &stop, int maxCount, const QDateTime &dateTime,
            const QString &dataType, bool useDifferentUrl, ParseDocumentMode parseMode );

private:
    enum State {
        Initializing = 0,
        UpdatingGtfsFeed,
        Ready,

        ErrorDownloadingFeed = 10,
        ErrorReadingFeed,
        ErrorInDatabase,
        ErrorNeedsFeedImport
    };

    /** A value between 0.0 and 1.0 indicating the amount of the total progress for downloading. */
    static const qreal PROGRESS_PART_FOR_FEED_DOWNLOAD = 0.1;

    /**
     * @brief Converts a GTFS route_type value to a matching VehicleType.
     *
     * @see http://code.google.com/intl/en-US/transit/spec/transit_feed_specification.html#routes_txt___Field_Definitions
     **/
    VehicleType vehicleTypeFromGtfsRouteType( int gtfsRouteType ) const;

    QTime timeFromSecondsSinceMidnight( int secondsSinceMidnight, QDate *date = 0 ) const;

    void loadAgencyInformation();

    State m_state; // Current state
    AgencyInformations m_agencyCache; // Cache contents of the "agency" DB table, usally small, eg. only one agency
    GtfsRealtimeTripUpdates *m_tripUpdates;
    GtfsRealtimeAlerts *m_alerts;
    QHash< QString, JobInfos > m_waitingSources; // Sources waiting for import to finish
    PublicTransportService *m_service;
    qreal m_progress;
};

#endif // Multiple inclusion guard
