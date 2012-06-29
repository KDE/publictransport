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

#ifndef SERVICEPROVIDERGTFS_HEADER
#define SERVICEPROVIDERGTFS_HEADER

#include "serviceprovider.h"
#include "generaltransitfeed_importer.h"
#include "generaltransitfeed_realtime.h"

class PublicTransportService;
class QNetworkReply;
class KTimeZone;

/**
 * @brief This class uses a database similiar to the GTFS structure to access public transport data.
 *
 * To fill the GTFS database with data from a GeneralTransitFeedSpecification feed (zip file)
 * GeneralTransitFeedImporter is used by PublicTransportService. This service has an operation
 * "UpdateGtfsFeed", which gets called by this class. That operation only updates already imported
 * GTFS feeds if there is a new version. To import a new GTFS feed for the first time the operation
 * "ImportGtfsFeed" should be used. That operation does @em not get called by this class. This is
 * because importing GTFS feeds can require quite a lot disk space and importing can take some
 * time. The user should be asked to import a new GTFS feed.
 *
 * This class immediately emits the ...Received() signal in the associated request...() functions,
 * because making timetable data available is very fast using the GTFS database. Requesting data
 * from the database is very fast even for big databases (eg. 300MB).
 *
 * To add support for a new service provider using this accessor type you need to write an accessor
 * XML file for the service provider.
 * See @ref pageAccessorInfos.
 **/
class ServiceProviderGtfs : public ServiceProvider
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

    /** @brief The maximum number of stop suggestions to return. */
    static const int STOP_SUGGESTION_LIMIT = 100;

    /**
     * @brief Constructs a new ServiceProviderGtfs object.
     *
     * You should use createProvider() to get an accessor for a given service provider ID.
     *
     * @param info An object containing information about the service provider.
     * @param parent The parent QObject.
     **/
    explicit ServiceProviderGtfs( const ServiceProviderData *data, QObject *parent = 0,
                                  const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>(0) );

    /** @brief Destructor. */
    virtual ~ServiceProviderGtfs();

    static bool isTestResultUnchanged( const QString &providerId,
                                       const QSharedPointer<KConfig> &cache );

    virtual bool isTestResultUnchanged( const QSharedPointer<KConfig> &cache ) const;

    /** @brief Returns the type of this provider, ie. GtfsProvider. */
    virtual ServiceProviderType type() const { return GtfsProvider; };

    /** @brief Checks if there was an error. */
    bool hasErrors( QString *errorMessage = 0 ) const;

    /** @brief Gets a list of features that this provider supports. */
    virtual QStringList features() const;

    /** @brief Returns true, if there is a GTFS-realtime source available. */
    bool isRealtimeDataAvailable() const;

    /** @brief Gets the size in bytes of the database containing the GTFS data. */
    qint64 databaseSize() const;

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
     * @brief Requests a list of departures/arrivals from the GTFS database.
     *
     * @param requestInfo Information about the departure/arrival request.
     **/
    virtual void requestDepartures( const DepartureRequest &requestInfo );

    /**
     * @brief Requests a list of stop suggestions from the GTFS database.
     *
     * @param requestInfo Information about the stop suggestion request.
     **/
    virtual void requestStopSuggestions( const StopSuggestionRequest &requestInfo );

    /** @brief Run script provider specific tests. */
    virtual bool runTests( QString *errorMessage = 0 ) const;

    /** @brief Updates the GTFS feed data using @ref PublicTransportService. */
    void updateGtfsData();

    /** @brief Updates the GTFS-realtime data, ie. delays and journey news. */
    void updateRealtimeData();

    /**
     * @brief Returns true if the GTFS feed has been initially imported.
     *
     * This provider can only be used, if this function returns true. The GTFS feed needs to be
     * completely imported once. Afterwards updates are done automatically in the background to
     * update to new GTFS feed versions.
     **/
    bool isGtfsFeedImportFinished();

    bool checkState( const AbstractRequest *requestInfo );
    bool checkForDiskIoErrorInDatabase( const QSqlError &error, const AbstractRequest *requestInfo );

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
    static const qreal PROGRESS_PART_FOR_FEED_DOWNLOAD;

    /**
     * @brief Converts a GTFS route_type value to a matching VehicleType.
     *
     * @see http://code.google.com/intl/en-US/transit/spec/transit_feed_specification.html#routes_txt___Field_Definitions
     **/
    VehicleType vehicleTypeFromGtfsRouteType( int gtfsRouteType ) const;

    QTime timeFromSecondsSinceMidnight( int secondsSinceMidnight, QDate *date = 0 ) const;

    // errorState >= 10
    QString errorMessageForErrorState( State errorState ) const;

    void loadAgencyInformation();

    State m_state; // Current state
    AgencyInformations m_agencyCache; // Cache contents of the "agency" DB table, usally small, eg. only one agency
    GtfsRealtimeTripUpdates *m_tripUpdates;
    GtfsRealtimeAlerts *m_alerts;
    QHash< QString, AbstractRequest* > m_waitingRequests; // Sources waiting for import to finish
    PublicTransportService *m_service;
    qreal m_progress;
};

#endif // Multiple inclusion guard
