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

/** @file
* @brief This file contains the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef PUBLICTRANSPORTDATAENGINE_HEADER
#define PUBLICTRANSPORTDATAENGINE_HEADER

// Own includes
#include "config.h"
#include "enums.h"
#include "departureinfo.h"

// Plasma includes
#include <Plasma/DataEngine>

class AbstractRequest;
class AbstractTimetableItemRequest;
class StopSuggestionRequest;
class DepartureRequest;
class ArrivalRequest;
class JourneyRequest;
class AdditionalDataRequest;

class DataSource;
class TimetableDataSource;
class ProvidersDataSource;
class ServiceProvider;
class ServiceProviderData;
class DepartureInfo;
class JourneyInfo;
class StopInfo;

class KJob;

class QTimer;
class QFileSystemWatcher;

/**
 * @brief This engine provides departure/arrival times and journeys for public transport.
 *
 * See @ref pageUsage.
 */
class PublicTransportEngine : public Plasma::DataEngine {
    Q_OBJECT

public:
    /** @brief A shared pointer to a ServiceProvider. */
    typedef QSharedPointer< ServiceProvider > ProviderPointer;

    /**
     * @brief The available types of sources of this data engine.
     *
     * Source names can be associated with these types by their first words using
     * souceTypeFromName().
     * @see sourceTypeKeyword()
     * @see sourceTypeFromName()
     **/
    enum SourceType {
        InvalidSourceName = 0, /**< Returned by @ref sourceTypeFromName,
                * if the source name is invalid. */

        // Data sources providing information about the engine, available providers
        ServiceProviderSource = 1, /**< The source contains information about a specific
                * service provider, identified by it's ID or a country code. If a country code
                * is given, the default provider for that country gets used, if any.
                * See also @ref usage_serviceproviders_sec. */
        ServiceProvidersSource = 2, /**< The source contains information about all available
                * service providers. See also @ref usage_serviceproviders_sec. */
        ErroneousServiceProvidersSource = 3, /**< The source contains a list of erroneous
                * service providers. */
        LocationsSource = 4, /**< The source contains information about locations
                * for which supported service providers exist. */
        VehicleTypesSource = 5, /**< The source contains information about all available
                * vehicle types. They are stored by integer keys, matching the values of
                * the Enums::VehicleType (engine) and PublicTransport::VehicleType
                * (libpublictransporthelper) enumerations. The information stored in this
                * data source can also be retrieved from PublicTransport::VehicleType
                * using libpublictransporthelper. See also @ref usage_vehicletypes_sec.  */

        // Data sources providing timetable data
        DeparturesSource = 10, /**< The source contains timetable data for departures.
                * See also @ref usage_departures_sec. */
        ArrivalsSource, /**< The source contains timetable data for arrivals.
                * See also @ref usage_departures_sec. */
        StopsSource, /**< The source contains a list of stop suggestions.
                * See also @ref usage_stopList_sec. */
        JourneysSource, /**< The source contains information about journeys.
                * See also @ref usage_journeys_sec. */
        JourneysDepSource, /**< The source contains information about journeys,
                * that depart at the given date and time. See also @ref usage_journeys_sec. */
        JourneysArrSource /**< The source contains information about journeys,
                * that arrive at the given date and time. See also @ref usage_journeys_sec. */
    };

    /** @brief Every data engine needs a constructor with these arguments. */
    PublicTransportEngine( QObject *parent, const QVariantList &args );

    /** @brief Destructor. */
    ~PublicTransportEngine();

    /** @brief Contains data read from a data source name. */
    struct SourceRequestData {
        /** @brief Reads the given data source @p name and fills member variables. */
        SourceRequestData( const QString &name );

        /** @brief Deletes the request object if it was created. */
        ~SourceRequestData();

        /** @brief Whether or not the source name is valid. */
        bool isValid() const;

        QString name; /**< The complete data source name. */
        QString defaultParameter; /**< A parameter without name read from the source name.
                * This can be a provider ID or a location code. */
        SourceType type; /**< The type of the data source. */
        ParseDocumentMode parseMode; /**< Parse mode for requesting data sources. */
        AbstractTimetableItemRequest *request; /**< A request object created for requesting
                * data sources. */
    };

    /** @brief Get the keyword used in source names associated with the given @p sourceType. */
    static const QLatin1String sourceTypeKeyword( SourceType sourceType );

    /** @brief Get ParseDocumentMode associated with the given @p sourceType. */
    static ParseDocumentMode parseModeFromSourceType( SourceType sourceType );

    /**
     * @brief Get the SourceType associated with the given @p sourceName.
     * This matches case sensitive, otherwise there can be multiple data sources with the
     * same data but only different case in their names.
     **/
    static SourceType sourceTypeFromName( const QString &sourceName );

    /** @brief Whether or not data sources of @p sourceType request data from a server. */
    static bool isDataRequestingSourceType( SourceType sourceType ) {
        return static_cast< int >( sourceType ) >= 10; };

    /** @brief Get the ID of the provider used for the source with the given @p sourceName. */
    static QString providerIdFromSourceName( const QString &sourceName );

    /** @brief Remove words from the beginning/end of stop names that occur in most names. */
    static QStringList removeCityNameFromStops( const QStringList &stopNames );

    /** @brief Reimplemented to add some always visible default sources. */
    virtual QStringList sources() const;

    /**
     * @brief Gets the service for the data source with the given @p name.
     *
     * The returned service can be used to start operations on the timetable data source.
     * For example it has an operation to import GTFS feeds into a local database or to update
     * or delete that database.
     * @return A pointer to the created Plasma::Service or 0 if no service is available for @p name.
     * @see PublicTransportService
     **/
    virtual Plasma::Service* serviceForSource( const QString &name );

    /**
     * @brief Get the number of seconds until the next automatic update of @p sourceName.
     *
     * Timetable data sources update their data periodically, based on the needs of the data source
     * (is realtime data available?, until when are departures already available?, etc.). To update
     * a timetable data source before the automatic update, use requestUpdate().
     * @param sourceName The name of the data source to query for the next automatic update time.
     * @param errorMessage If not 0, this gets set to a message describing the error if -1
     *   gets returned.
     * @returns The number of seconds until the next automatic update of @p sourceName or -1 if
     *   no data source with @p sourceName is available or is not a timetable data source.
     * @see requestUpdate()
     **/
    int secsUntilUpdate( const QString &sourceName, QString *errorMessage = 0 );

    /**
     * @brief Get the minimum number of seconds until the next (manual) update of @p sourceName.
     *
     * Timetable data sources can be updated before the next automatic update using requestUpdate().
     * But a minimum number of seconds needs to have passed before another update request will
     * be accepted.
     * @param sourceName The name of the data source to query for the minimal next update time.
     * @param errorMessage If not 0, this gets set to a message describing the error if -1
     *   gets returned.
     * @returns The minumal number of seconds until the next update request of @p sourceName
     *   will be accepted or -1 if no data source with @p sourceName is available or is not a
     *   timetable data source.
     * @see requestUpdate()
     **/
    int minSecsUntilUpdate( const QString &sourceName, QString *errorMessage = 0 );

    /**
     * @brief Requests an update for @p sourceName.
     *
     * Can be used to update a timetable data source before the next calculated automatic update
     * time. This allows for more updates in the same time, but not unlimited. The waiting time
     * between two updates is lower when using requestUpdate(), compared to automatic updates.
     * @note This is marked with Q_INVOKABLE so that it can be invoked using QMetaObject from the
     *   timetable service for that source.
     * @param sourceName The name of the data source to request an update for.
     * @param errorMessage If not 0, this gets set to a message describing the error if @c false
     *   gets returned.
     * @returns @c True, if the update gets processed, @c false otherwise, eg. if not enough time
     *   has passed since the last update.
     * @see minSecsUntilUpdate()
     * @see secsUntilUpdate()
     **/
    Q_INVOKABLE bool requestUpdate( const QString &sourceName );

    /** @brief Request more items in @p direction for @p sourceName */
    Q_INVOKABLE bool requestMoreItems( const QString &sourceName,
                                       Enums::MoreItemsDirection direction = Enums::LaterItems );

    /**
     * @brief Get a hash with information about the current GTFS feed import state.
     **/
    QString updateProviderState( const QString &providerId, QVariantHash *stateData,
                                 const QString &providerType, const QString &feedUrl = QString(),
                                 bool readFromCache = true );

#ifdef BUILD_PROVIDER_TYPE_GTFS
    /**
     * @brief Try to start a GTFS service @p job that accesses the GTFS database.
     * @return @c True, if the @p job can be started, @c false otherwise. If there already is a
     *   running GTFS service @p job that accesses the GTFS database no other such jobs can be
     *   started and this function returns @c false.
     **/
    Q_INVOKABLE bool tryToStartGtfsFeedImportJob( Plasma::ServiceJob *job );
#endif // BUILD_PROVIDER_TYPE_GTFS

    /**
     * @brief The default time offset from now for the first departure/arrival/journey in results.
     *
     * This is used if no time was specified in the source name with the parameter @c time,
     * @c datetime or @c timeoffset.
     **/
    static const int DEFAULT_TIME_OFFSET;

    /**
     * @brief The (minimal) time in milliseconds for unused providers to stay cached.
     *
     * When a provider is not used any longer, it is not destroyed immediately, but gets cached.
     * After this timeout still unused cached providers get destroyed. If an unused provider
     * gets used again before this timeout it will not get destroyed.
     *
     * This timeout can be some seconds long, only the ServiceProvider instance gets cached.
     * Can prevent too many recreations of providers.
     **/
    static const int PROVIDER_CLEANUP_TIMEOUT;

signals:
    /**
     * @brief Emitted when a request for additional data has been finished.
     *
     * This signal gets used by the timetable service to get notified when a job has finsihed
     * and whether it was successful or not.
     * @param sourceName The name of the data source for which an additional data request finished.
     * @param item The index of the timetable item in @p sourceName for which the additional data
     *   request finished or -1 if the request failed for all items.
     * @param success Whether or not the additional data job was successful.
     * @param errorMessage Contains an error message if @p success is @c false.
     *
     * @note The "additionalDataError" field of the target data source is not necessarily updated
     *   when this signal gets emitted. This only happens when the request could actually be
     *   started, ie. if the target data source exists, is a timetable data source and the provider
     *   is valid and supports the Enums::ProvidesAdditionalData feature.
     *
     * @see usage_service_sec
     **/
    void additionalDataRequestFinished( const QString &sourceName, int item, bool success = true,
                                        const QString &errorMessage = QString() );

    /** @brief Emitted when a manual update request (requestUpdate()) has finished. */
    void updateRequestFinished( const QString &sourceName,
                                bool success = true, const QString &errorMessage = QString() );

    /** @brief Emitted when a request for more items (requestMoreItems()) has finished. */
    void moreItemsRequestFinished( const QString &sourceName, Enums::MoreItemsDirection direction,
                                   bool success = true, const QString &errorMessage = QString() );

public slots:
    /**
     * @brief Request additional timetable data in @p sourceName.
     *
     * @param sourceName The name of the timetable data source for which additional data should
     *   be requested.
     * @param updateItem The index of the (first) timetable item to request additional data for.
     * @param count The number of timetable items beginning at @p updateItem to request additional
     *   data for. The default is 1.
     **/
    void requestAdditionalData( const QString &sourceName, int updateItem, int count = 1 );

protected slots:
    /**
     * @brief Free resources for the data source with the given @p name where possible.
     *
     * Destroys the DataSource object associated with the data source, if the source is not also
     * connected under other ambiguous names (for timetable data sources). If a ServiceProvider
     * was used for the data source but not for any other source, it gets destroyed, too.
     **/
    void slotSourceRemoved( const QString &name );

    /**
     * @brief The network state has changed, eg. was connected or disconnected.
     *
     * This slot is connected with the @c networkstatus module of @c kded through DBus.
     * @p state The new network state. See Solid::Networking::Status.
     **/
    void networkStateChanged( uint state );

    /** @brief The update timeout for a timetable data source was reached. */
    void updateTimeout();

    /** @brief The cleanup timeout for a timetable data source was reached. */
    void cleanupTimeout();

    /** @brief Do some cleanup, eg. deleting unused cached providers after a while. */
    void cleanup();

#ifdef BUILD_PROVIDER_TYPE_GTFS
    /** @brief A @p job of the GTFS service has finished. */
    void gtfsServiceJobFinished( KJob *job );

    /** @brief Received a message from a @p job of the GTFS service. */
    void gtfsImportJobInfoMessage( KJob *job, const QString &plain, const QString &rich );

    /** @brief A @p job of the GTFS service notifies it's progress in @p percent. */
    void gtfsImportJobPercent( KJob *job, ulong percent );
#endif // BUILD_PROVIDER_TYPE_GTFS

    /**
     * @brief Departures were received.
     *
     * @param provider The provider that was used to get the departures.
     * @param requestUrl The url used to request the information.
     * @param departures The departures that were received.
     * @param globalInfo Global information that affects all departures.
     * @param request Information about the request for the here received @p departures.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void departuresReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const DepartureInfoList &departures,
            const GlobalTimetableInfo &globalInfo,
            const DepartureRequest &request,
            bool deleteDepartureInfos = true );

    /**
     * @brief Arrivals were received.
     *
     * @param provider The provider that was used to get the arrivals.
     * @param requestUrl The url used to request the information.
     * @param arrivals The arrivals that were received.
     * @param globalInfo Global information that affects all arrivals.
     * @param request Information about the request for the here received @p arrivals.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void arrivalsReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const ArrivalInfoList &arrivals,
            const GlobalTimetableInfo &globalInfo,
            const ArrivalRequest &request,
            bool deleteDepartureInfos = true );

    /**
     * @brief Journeys were received.
     *
     * @param provider The provider that was used to get the journeys.
     * @param requestUrl The url used to request the information.
     * @param journeys The journeys that were received.
     * @param globalInfo Global information that affects all journeys.
     * @param request Information about the request for the here received @p journeys.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void journeysReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const JourneyInfoList &journeys,
            const GlobalTimetableInfo &globalInfo,
            const JourneyRequest &request,
            bool deleteJourneyInfos = true );

    /**
     * @brief Stop suggestions were received.
     *
     * @param provider The service provider that was used to get the stops.
     * @param requestUrl The url used to request the information.
     * @param stops The stops that were received.
     * @param request Information about the request for the here received @p stops.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void stopsReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const StopInfoList &stops,
            const StopSuggestionRequest &request,
            bool deleteStopInfos = true );

    /**
     * @brief Additional data was received.
     *
     * @param provider The service provider that was used to get additional data.
     * @param requestUrl The url used to request the information.
     * @param data The additional data that was receceived.
     * @param request Information about the request for the here received @p data.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void additionalDataReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const TimetableData &data,
            const AdditionalDataRequest &request );

    /**
     * @brief A request has failed.
     *
     * @param provider The provider that was used to get data from the service provider.
     * @param errorCode The error code or NoError if there was no error.
     * @param errorString If @p errorCode isn't NoError this contains a description of the error.
     * @param requestUrl The url used to request the information.
     * @param request Information about the request that failed with @p errorType.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void requestFailed( ServiceProvider *provider, ErrorCode errorCode, const QString &errorMessage,
            const QUrl &requestUrl, const AbstractRequest *request );

    /**
     * @brief Update data sources which have new additional data available.
     *
     * This slot gets used to combine too many data source updates in short time
     * into a single update.
     **/
    void updateDataSourcesWithNewAdditionData();

    /**
     * @brief A global or local directory with service provider XML files was changed.
     *
     * This slot uses reloadChangedProviders() to actually update the loaded service providers.
     * Because a change in multiple files in one or more directories causes a call to this slot for
     * each file, this would cause all providers to be reloaded for each changed file. Especially
     * when installing a new version, while running an old one, this can take some time.
     * Therefore this slot uses a short timeout. If a new call to this slot happens within that
     * timeout, the timeout gets simply restarted. Otherwise after the timeout
     * reloadChangedProviders() gets called.
     *
     * @param path The changed directory.
     * @see reloadChangedProviders()
     */
    void serviceProviderDirChanged( const QString &path );

    /**
     * @brief Deletes all currently created providers and associated data and recreates them.
     *
     * This slot gets called by serviceProviderDirChanged() if a global or local directory
     * containing service provider XML files has changed. It does not call this function on every
     * directory change, but only if there are no further changes in a short timespan.
     * The provider cache (ServiceProviderGlobal::cache()) gets updated for all changed provider
     * plugins.
     * @note Deleted data sources only get filled again when new requests are made,
     *   they are not updated.
     *
     * @see serviceProviderDirChanged()
     **/
    void reloadChangedProviders();

protected:
    /**
     * @brief This function gets called when a new data source gets requested.
     *
     * @param name The name of the requested data source.
     * @return @c True, if the data source could be updated successfully. @c False, otherwise.
     **/
    bool sourceRequestEvent( const QString &name );

    /**
     * @brief This function gets called when a data source gets requested or needs an update.
     *
     * It checks the source type of the data source with the given @p name using sourceTypeFromName
     * and then calls the corresponding update...() function for the source type.
     * All departure/arrival/journey/stop suggestion data sources are updated using
     * updateTimetableSource(). Other data sources are updated using updateServiceProviderSource(),
     * updateServiceProviderForCountrySource(), updateErroneousServiceProviderSource(),
     * updateLocationSource().
     *
     * @param name The name of the data source to be updated.
     * @return @c True, if the data source could be updated successfully. @c False, otherwise.
     **/
    bool updateSourceEvent( const QString &name );

    /**
     * @brief Implementation of sourceRequestEvent() and updateSourceEvent() (@p update @c true).
     *
     * Requesting and updating a source is done in (almost) the same way.
     * @param data A SourceRequestData for the data source to be requested/updated.
     * @param update @c True, if the data source should be updated. @c False, otherwise.
     **/
    bool requestOrUpdateSourceEvent( const SourceRequestData &data, bool update = false );

    /**
     * @brief Updates the ServiceProviders data source.
     *
     * The data source contains information about all available service providers.
     * @return @c True, if the data source could be updated successfully. @c False, otherwise.
     **/
    bool updateServiceProviderSource();

    /**
     * @brief Updates the data source for the service provider mentioned in @p name.
     *
     * If a service provider ID is given in @p name, information about that service provider is
     * returned. If a location is given (ie. "international" or a two letter country code)
     * information about the default service provider for that location is returned.
     *
     * @param name The name of the data source. It starts with the ServiceProvider keyword and is
     *   followed by a service provider ID or a location, ie "ServiceProvider de" or
     *   "ServiceProvider de_db".
     * @return @c True, if the data source could be updated successfully. @c False, otherwise.
     **/
    bool updateServiceProviderForCountrySource( const SourceRequestData &sourceData );

    /**
     * @brief Updates the ErrornousServiceProviders data source.
     *
     * The data source contains information about all erroneous service providers.
     *
     * @return @c True, if the data source could be updated successfully. @c False, otherwise.
     **/
    bool updateErroneousServiceProviderSource();

    /**
     * @brief Updates the "Locations" data source.
     *
     * Locations with available service providers are determined by checking the list of valid
     * accessors for the countries they support.
     *
     * @return @c True, if the data source could be updated successfully. @c False, otherwise.
     **/
    bool updateLocationSource();

    /**
     * @brief Updates the timetable data source with the given @p data.
     *
     * Timetable data may be one of these here: Departure, arrivals, journeys (to or from the home
     * stop) or stop suggestions.
     * First it checks if the data source is up to date using isSourceUpToDate. If it's not, new
     * data gets requested using the associated accessor. The accessor gets retrieved using
     * getSpecificAccessor, which creates a new accessor if there is no cached value.
     *
     * Data may arrive asynchronously depending on the used accessor.
     *
     * @return @c True, if the data source could be updated successfully. @c False, otherwise.
     **/
    bool updateTimetableDataSource( const SourceRequestData &data );

    /** @brief Fill the VehicleTypes data source. */
    void initVehicleTypesSource();

    /**
     * @brief Wheather or not the data source with the given @p name is up to date.
     *
     * @param nonAmbiguousName The (non ambiguous) name of the source to be checked.
     * @return @c True, if the data source is up to date. @c False, otherwise.
     * @see disambiguateSourceName()
     **/
    bool isSourceUpToDate( const QString &nonAmbiguousName,
                           UpdateFlags updateFlags = DefaultUpdateFlags );

    /**
     * @brief Get the minimum number of seconds until the next update of @p sourceName.
     *
     * Whether or not the time until the next automatic or manual update should be calculated
     * is defined by @p updateFlags.
     * This function gets used by secsUntilUpdate() and minSecsUntilUpdate().
     **/
    int getSecsUntilUpdate( const QString &sourceName, QString *errorMessage = 0,
                            UpdateFlags updateFlags = DefaultUpdateFlags );

    /**
     * @brief Test @p providerId for errors.
     *
     * @param providerId The ID of the provider to test.
     * @param providerData A pointer to a QVariantHash into which provider data gets inserted,
     *   ie. the data that gets used for "ServiceProvider [providerId]" data sources.
     * @param errorMessage A pointer to a QString which gets set to an error message
     *   if @c false gets returned.
     * @param cache A shared pointer to the provider cache. If an invalid pointer is given it
     *   gets created using ServiceProviderGlobal::cache().
     * @return @c True, if the service provider is valid, @c false otherwise.
     **/
    bool testServiceProvider( const QString &providerId, QVariantHash *providerData,
                              QString *errorMessage,
                              const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>() );

    /**
     * @brief Request timetable data after checking the provider and the request.
     *
     * The request fails, if the requested provider is invalid, if an unsupported feature
     * is needed to run the request or if a @c city parameter is missing but needed by the
     * provider.
     * @param data A SourceRequestData object for the data source name that triggered the request.
     * @return @c True, if the request could be started successfully, @c false otherwise.
     * @see ServiceProvider::request()
     **/
    bool request( const SourceRequestData &data );

private:
    /** @brief Get the date and time for the next update of @p dataSource. */
    QDateTime sourceUpdateTime( TimetableDataSource *dataSource,
                                UpdateFlags updateFlags = DefaultUpdateFlags );

    /** @brief Handler for departuresReceived() (@p isDepartureData @c true) and arrivalsReceived() */
    void timetableDataReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const DepartureInfoList &items,
            const GlobalTimetableInfo &globalInfo,
            const DepartureRequest &request,
            bool deleteDepartureInfos = true, bool isDepartureData = true );

    /**
     * @brief Gets information about @p provider for a service provider data source.
     *
     * If @p provider is 0, the provider cache file ServiceProvider::cacheFileName() gets checked.
     * If it contains all needed information which is not available in @p data no ServiceProvider
     * object needs to be created to fill the QHash with the service provider information. If there
     * are no cached values for the provider it gets created to get and cache those values.
     *
     * @param provider The accessor to get information about. May be 0, see above.
     * @return A QHash with values keyed with strings.
     **/
    QVariantHash serviceProviderData( const ServiceProviderData &data,
                                      const ServiceProvider *provider = 0 );

    /** @brief Overload, use if @p provider is @em not 0. */
    QVariantHash serviceProviderData( const ServiceProvider *provider );

    /**
     * @brief Gets a hash with information about available locations.
     *
     * Locations are here countries plus some special locations like "international".
     **/
    QVariantHash locations();

    /**
     * @brief Get a pointer to the provider with the given @p providerId.
     *
     * If the provider to get is in use it does not need to be created.
     * @param providerId The ID of the provider to get.
     * @param newlyCreated If not 0 this gets set to @p true if the provider was newly created
     *   and @p false if it was already created.
     * @return A pointer to the provider with the given @p providerId or an invalid pointer
     *   if the provider could not be found or it's state is not "ready".
     **/
    ProviderPointer providerFromId( const QString &providerId, bool *newlyCreated = 0 );

    /**
     * @brief Delete the provider with the given @p providerId and clears all cached data.
     *
     * The provider to delete is considered invalid and will not be cached.
     * Data sources using the provider will be deleted if @p keepProviderDataSources is @c false,
     * which is @em not the default.
     **/
    void deleteProvider( const QString &providerId, bool keepProviderDataSources = true );

    /** @brief Publish the data of @p dataSource under it's data source name. */
    void publishData( DataSource *dataSource,
                      const QString &newlyRequestedProviderId = QString() );

    /**
     * @brief Get a pointer to the ProvidersDataSource object of service provider data source(s).
     * The ProvidersDataSource objects provides data for the "ServiceProviders" and
     * "ServiceProvider [providerId]" data source(s).
     **/
    ProvidersDataSource *providersDataSource() const;

    /** @brief Checks if the provider with the given @p providerId is used by a connected source. */
    bool isProviderUsed( const QString &providerId );

    /**
     * @brief Get an error code for the given @p stateId.
     *
     * The returned error code gets published in data sources if the provider state is not "ready"
     * and the provider cannot be used.
     **/
    static ErrorCode errorCodeFromState( const QString &stateId );

    /**
     * @brief Whether or not the state data with @p stateDataKey should be written to the cache.
     *
     * Dynamic state data gets invalid when the engine gets destroyed. Therefore they should not
     * be written to the provider cache. For example a 'progress' field is useless in the cache,
     * since the operation needs to be restarted if it was aborted, eg. because of a crash.
     **/
    static bool isStateDataCached( const QString &stateDataKey );

    /**
     * @brief Disambiguates the given @p sourceName.
     *
     * Date and time parameters get replaced by a @c datetime parameter, with the time rounded to
     * 15 minutes to use one data source object for all sources that are less than 15 minutes apart
     * from each other.
     * Parameters get reordered to a standard order, so that data source names with only a
     * different parameter order point to the same DataSource object. Parameter values get
     * converted to lower case (eg. stop names).
     *
     * Without doing this eg. departure data sources with only little different time values
     * will @em not share their departures and download them twice.
     **/
    static QString disambiguateSourceName( const QString &sourceName );

    /** @brief If @p providerId is empty return the default provider for the users country. */
    static QString fixProviderId( const QString &providerId );

    /**
     * @brief Get the service provider with the given @p serviceProviderId, if any.
     *
     * @param serviceProviderId The ID of the service provider to get.
     *   The ID starts with a country code, followed by an underscore and it's name.
     *   If it's empty, the default service provider for the users country will
     *   be used, if there is any.
     **/
    static ServiceProvider *createProvider( const QString &serviceProviderId = QString(),
            QObject *parent = 0, const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>(0) );

    /**
     * @brief Create a provider for the given @p data.
     * @param data The data object to create a provider for.
     * @param parent The parent for the new provider
     **/
    static ServiceProvider *createProviderForData( const ServiceProviderData *data,
            QObject *parent = 0, const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>(0) );

    /** @brief Get the timetable data source that uses the given @p timer. */
    TimetableDataSource *dataSourceFromTimer( QTimer *timer ) const;

    /**
     * @brief Update all provider data including the provider state.
     *
     * Uses updateProviderState() to update the state of the provider.
     **/
    bool updateProviderData( const QString &providerId,
                             const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>(0) );

    bool enoughDataAvailable( DataSource *dataSource, const SourceRequestData &sourceData ) const;

    void startCleanupLater();
    void startDataSourceCleanupLater( TimetableDataSource *dataSource );

    // Implementation for the requestAdditionalData() slot,
    // call testDataSourceForAdditionalDataRequests() before and publishData() afterwards.
    // Both functions only need to be called once for multiple calls to this function.
    bool requestAdditionalData( const QString &sourceName, int updateItem,
                                TimetableDataSource *dataSource );

    // Should be called once before calling requestAdditionalData(),
    // if 0 gets returned all additional data requests on sourceName will fail
    TimetableDataSource *testDataSourceForAdditionalDataRequests( const QString &sourceName );

    QVariantMap hashToMap(QVariantHash data);

    QHash< QString, ProviderPointer > m_providers; // Currently used providers by ID
    QHash< QString, ProviderPointer > m_cachedProviders; // Unused but still cached providers by ID
    QVariantHash m_erroneousProviders; // Error messages for erroneous providers by ID
    QHash< QString, DataSource* > m_dataSources; // Data objects for data sources, stored by
                                                 // unambiguous data source name
    QFileSystemWatcher *m_fileSystemWatcher; // Watches provider installation directories
    QTimer *m_providerUpdateDelayTimer; // Delays updates after changes in installation directories
    QTimer *m_cleanupTimer; // Cleanup cached providers after PROVIDER_CLEANUP_TIMEOUT
    QStringList m_runningSources; // Sources which are currently being processed
};

#endif // Multiple inclusion guard
