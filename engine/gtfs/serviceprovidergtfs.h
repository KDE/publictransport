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
* @brief This file contains a class to access data from GTFS feeds.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SERVICEPROVIDERGTFS_HEADER
#define SERVICEPROVIDERGTFS_HEADER

#include "config.h"
#include "serviceprovider.h"
#include "gtfsimporter.h"
#ifdef BUILD_GTFS_REALTIME
    #include "gtfsrealtime.h"
#endif

namespace Plasma {
    class Service;
}

class GtfsService;
class QNetworkReply;
class KTimeZone;

/**
 * @brief This class uses a database similiar to the GTFS structure to access public transport data.
 *
 * To fill the GTFS database with data from a GeneralTransitFeedSpecification feed (zip file)
 * GtfsImporter is used by the GTFS service. This service has an operation "updateGtfsDatabase",
 * which gets called by this class. That operation only updates already imported GTFS feeds if
 * there is a new version. To import a new GTFS feed for the first time the operation
 * "importGtfsFeed" should be used. That operation does @em not get called by this class.
 * This is because importing GTFS feeds can require quite a lot disk space and importing can take
 * some time. The user should be asked to import a new GTFS feed.
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
    /** @brief States of this class. */
    enum State {
        Initializing = 0, /**< Only used in the constructor as long as the state is unknown. */
        Ready, /**< The provider is ready to use, ie. the GTFS feed was successfully imported. */
        Error /**< There was an error that prevents the provider from being used, eg. the GTFS
                * database does not exist or was deleted/corrupted. */
    };

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
     * @brief Update the GTFS database state for @p providerId in the cache and return the result.
     *
     * This function checks and updates the 'feedImportFinished' field in the 'gtfs' config group
     * in the cache.
     *
     * @param providerId The ID of the GTFS provider for which the database state should be updated.
     * @param cache A shared pointer to the provider cache. If an invalid pointer gets passed it
     *   gets received using ServiceProviderGlobal::cache().
     * @param stateData State data gets inserted here, at least a 'statusMessage'.
     * @return The state of the provider. If the GTFS feed was successfully imported and the
     *   database is valid the returned state is "ready", otherwise "gtfs_feed_import_pending".
     **/
    static QString updateGtfsDatabaseState( const QString &providerId,
            const QSharedPointer< KConfig > &cache = QSharedPointer<KConfig>(),
            QVariantHash *stateData = 0 );

    /** @brief Checks if an updated GTFS feed is available for @p providerId. */
    static bool isUpdateAvailable( const QString &providerId,
            const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>() );

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

    /**
     * @brief Whether or not the cached test result for @p providerId is unchanged.
     *
     * This function tests if an updated GTFS feed is available.
     * @param providerId The provider to check.
     * @param cache A shared pointer to the cache.
     * @see isUpdateAvailable()
     * @see runTests()
     **/
    static bool isTestResultUnchanged( const QString &providerId,
                                       const QSharedPointer<KConfig> &cache );

    /**
     * @brief Whether or not the cached test result is unchanged.
     *
     * This function tests if an updated GTFS feed is available.
     * @param cache A shared pointer to the cache.
     * @see isUpdateAvailable()
     * @see runTests()
     **/
    virtual bool isTestResultUnchanged( const QSharedPointer<KConfig> &cache ) const;

    /** @brief Returns the type of this provider, ie. GtfsProvider. */
    virtual Enums::ServiceProviderType type() const { return Enums::GtfsProvider; };

    /** @brief Gets a list of features that this provider supports. */
    virtual QList<Enums::ProviderFeature> features() const;

    /**
     * @brief Get the minimum seconds to wait between two data-fetches from the service provider.
     * Limits the result to minimally one minute.
     **/
    virtual int minFetchWait( UpdateFlags updateFlags = DefaultUpdateFlags ) const;

#ifdef BUILD_GTFS_REALTIME
    /** @brief Returns true, if there is a GTFS-realtime source available. */
    bool isRealtimeDataAvailable() const;
#else
    bool isRealtimeDataAvailable() const { return false; }; // Dummy function
#endif

    /** @brief Gets the size in bytes of the database containing the GTFS data. */
    qint64 databaseSize() const;

protected slots:
#ifdef BUILD_GTFS_REALTIME
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
#endif // BUILD_GTFS_REALTIME

protected:
    void requestDeparturesOrArrivals( const DepartureRequest *request );

    /**
     * @brief Requests a list of departures from the GTFS database.
     * @param request Information about the departure request.
     **/
    virtual void requestDepartures( const DepartureRequest &request );

    /**
     * @brief Requests a list of arrivals from the GTFS database.
     * @param request Information about the arrival request.
     **/
    virtual void requestArrivals( const ArrivalRequest &request );

    /**
     * @brief Requests a list of stop suggestions from the GTFS database.
     * @param request Information about the stop suggestion request.
     **/
    virtual void requestStopSuggestions( const StopSuggestionRequest &request );

    virtual void requestStopsByGeoPosition( const StopsByGeoPositionRequest &request );

    /** @brief Run script provider specific tests. */
    virtual bool runTests( QString *errorMessage = 0 ) const;

    /** @brief Updates the GTFS database using the GTFS service. */
    void updateGtfsDatabase();

#ifdef BUILD_GTFS_REALTIME
    /** @brief Updates the GTFS-realtime data, ie. delays and journey news. */
    void updateRealtimeData();
#endif

    /** @brief Check @p error for IO errors, emit requestFailed() on failure. */
    bool checkForDiskIoError( const QSqlError &error, const AbstractRequest *request );

    /** @brief Get a list of stops from a successfully executed @p query. */
    StopInfoList stopsFromQuery( QSqlQuery *query, const StopSuggestionRequest *request = 0 ) const;

    /**
     * @brief Whether or not realtime data is available in the @p data of a timetable data source.
     */
    virtual bool isRealtimeDataAvailable( const QVariantHash &data ) const {
        Q_UNUSED( data );
        return isRealtimeDataAvailable();
    };

private:
    /** A value between 0.0 and 1.0 indicating the amount of the total progress for downloading. */
    static const qreal PROGRESS_PART_FOR_FEED_DOWNLOAD;

    /**
     * @brief Converts a GTFS route_type value to a matching VehicleType.
     *
     * @see http://code.google.com/intl/en-US/transit/spec/transit_feed_specification.html#routes_txt___Field_Definitions
     **/
    static Enums::VehicleType vehicleTypeFromGtfsRouteType( int gtfsRouteType );

    QTime timeFromSecondsSinceMidnight( int secondsSinceMidnight, QDate *date = 0 ) const;

    void loadAgencyInformation();

    State m_state; // Current state
    AgencyInformations m_agencyCache; // Cache contents of the "agency" DB table, usally small, eg. only one agency
    Plasma::Service *m_service;
#ifdef BUILD_GTFS_REALTIME
    GtfsRealtimeTripUpdates *m_tripUpdates;
    GtfsRealtimeAlerts *m_alerts;
#endif
};

#endif // Multiple inclusion guard
