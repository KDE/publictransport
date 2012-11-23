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
    typedef QSharedPointer< ServiceProvider > ProviderPointer;

    /**
     * @brief The available types of sources of this data engine.
     *
     * Source names can be associated with these types by the first word using souceTypeFromName.
     * @see sourceTypeKeyword
     * @see sourceTypeFromName
     **/
    enum SourceType {
        InvalidSourceName = 0, /**< Returned by @ref sourceTypeFromName,
                * if the source name is invalid. */

        // Data sources providing information about the engine, available providers
        ServiceProviderSource = 1, /**< The source contains information about available
                * service providers for a given country. See also @ref usage_serviceproviders_sec. */
        ServiceProvidersSource = 2, /**< The source contains information about available
                * service providers. See also @ref usage_serviceproviders_sec. */
        ErroneousServiceProvidersSource = 3, /**< The source contains a list of erroneous
                * service providers. */
        LocationsSource = 4, /**< The source contains information about locations
                * for which supported service providers exist. */
        VehicleTypesSource = 5, /**< The source contains information about all available
                * vehicle types. They are stored by integer keys, matching the values of
                * the Enums::VehicleType (engine) and PublicTransport::VehicleType
                * (libpublictransporthelper) enumerations. The information stored stored in
                * this data source can also be retrieved from PublicTransport::VehicleType
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
    PublicTransportEngine( QObject* parent, const QVariantList& args );

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

    /** @brief Get the SourceType associated with the given @p sourceName. */
    static SourceType sourceTypeFromName( const QString &sourceName );

    /** @brief Whether or not data sources of @p sourceType request data from a server. */
    static bool isDataRequestingSourceType( SourceType sourceType ) {
        return static_cast< int >( sourceType ) >= 10; };

    static QString providerIdFromSourceName( const QString &sourceName );

    /** @brief Reimplemented to add some always visible default sources. */
    virtual QStringList sources() const;

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
    Q_INVOKABLE bool requestUpdate( const QString &sourceName, QString *errorMessage = 0 );

    /** @brief Request more items in @p direction for @p sourceName */
    Q_INVOKABLE bool requestMoreItems( const QString &sourceName,
                                       Enums::MoreItemsDirection direction = Enums::LaterItems,
                                       QString *errorMessage = 0 );

    /**
     * @brief Get a hash with information about the current GTFS feed import state.
     **/
    QString updateProviderState( const QString &providerId, QVariantHash *stateData,
                                 const QString &providerType, bool readFromCache = true );

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
     * @c dateTime or @c timeOffset.
     **/
    static const int DEFAULT_TIME_OFFSET;

signals:
    /**
     * @brief Emitted when a request for additional data has been finished.
     *
     * This signal gets used by the "timetable" service to get notified when a job has finsihed
     * and whether it was successful or not.
     *
     * @param newData The new contents (including additional data) in the data source for the item.
     * @param success Whether or not the additional data job was successful.
     * @param errorMessage Contains an error message if @p success is @c false.
     **/
    void additionalDataRequestFinished( const QVariantHash &newData, bool success = true,
                                        const QString &errorMessage = QString() );

public slots:
    /** @brief Request additional timetable data for item @p updateItem in @p sourceName. */
    void requestAdditionalData( const QString &sourceName, int updateItem );

protected slots:
    void slotSourceRemoved( const QString &name );
    void networkStateChanged( uint state );

    void forceUpdate();
    void updateTimeout();

#ifdef BUILD_PROVIDER_TYPE_GTFS
    /** @brief A @p job of the GTFS service has finished. */
    void gtfsServiceJobFinished( Plasma::ServiceJob *job );

    /** @brief Received a message from a @p job of the GTFS service. */
    void gtfsImportJobInfoMessage( KJob *job, const QString &plain, const QString &rich );

    /** @brief A @p job of the GTFS service notifies it's progress. */
    void gtfsImportJobPercent( KJob *job, ulong percent );
#endif // BUILD_PROVIDER_TYPE_GTFS

    /**
     * @brief A list of departures was received.
     *
     * @param provider The provider that was used to download and parse the departures.
     * @param requestUrl The url used to request the information.
     * @param departures A list of departures that were received.
     * @param globalInfo Global information that affects all departures.
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
     * @brief A list of arrivals was received.
     *
     * @param provider The provider that was used to download and parse the arrivals.
     * @param requestUrl The url used to request the information.
     * @param arrivals A list of arrivals that were received.
     * @param globalInfo Global information that affects all arrivals.
     * @param request Information about the request for the here received @p arrivals.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void arrivalListReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const ArrivalInfoList &arrivals,
            const GlobalTimetableInfo &globalInfo,
            const ArrivalRequest &request,
            bool deleteDepartureInfos = true );

    /**
     * @brief A list of journeys was received.
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
     * @brief A list of stop suggestions was received.
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

    /** TODO
     * @brief A list of stop suggestions was received.
     *
     * @param provider The service provider that was used to download and parse the stops.
     * @param requestUrl The url used to request the information.
     * @param stops A pointer to a list of @ref StopInfo objects. TODO
     * @param request Information about the request for the here received @p stops.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void additionalDataReceived( ServiceProvider *provider,
            const QUrl &requestUrl, const TimetableData &data,
            const AdditionalDataRequest &request );

    void updateDataSourcesWithNewAdditionData();

    /**
     * @brief An error was received.
     *
     * @param provider The provider that was used to download and parse information
     *   from the service provider.
     * @param errorCode The error code or NoError if there was no error.
     * @param errorString If @p errorCode isn't NoError this contains a description of the error.
     * @param requestUrl The url used to request the information.
     * @param request Information about the request that failed with @p errorType.
     *
     * @see ServiceProvider::useSeparateCityValue()
     **/
    void errorParsing( ServiceProvider *provider, ErrorCode errorCode, const QString &errorMessage,
            const QUrl &requestUrl, const AbstractRequest *request );

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

    bool requestOrUpdateSourceEvent( const QString &name, bool update = false );

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

    int getSecsUntilUpdate( const QString &sourceName, QString *errorMessage = 0,
                            UpdateFlags updateFlags = DefaultUpdateFlags );

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

    bool testServiceProvider( const QString &providerId, QVariantHash *providerData,
                              QString *errorMessage,
                              const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>() );

    bool request( const SourceRequestData &data );
    inline bool request( const QString &sourceName ) {
        return request( SourceRequestData(sourceName) );
    };

private:
    QDateTime sourceUpdateTime( TimetableDataSource *dataSource,
                                UpdateFlags updateFlags = DefaultUpdateFlags );

    /** Handler for departureListReceived() (@p isDepartureData @c true) and arrivalListReceived() */
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

    QVariantHash serviceProviderData( const ServiceProvider *provider );

    /**
     * @brief Gets a hash with information about available locations.
     *
     * Locations are here countries plus some special locations like "international".
     **/
    QVariantHash locations();

    ProviderPointer providerFromId( const QString &providerId, bool *newlyCreated = 0 );

    void deleteProvider( const QString &providerId );

    /** @brief Publish the data of @p dataSource under it's data source name. */
    void publishData( DataSource *dataSource, const QString &changedProviderId = QString() );

    /**
     * @brief Get a pointer to the ProvidersDataSource object of service provider data source(s).
     * The ProvidersDataSource objects provides data for the "ServiceProviders" and
     * "ServiceProvider [providerId]" data source(s).
     **/
    ProvidersDataSource *providersDataSource() const;

    /** @brief Checks if the provider with the given ID is used by a connected source. */
    bool isProviderUsed( const QString &serviceProviderId );

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
     * @brief Remove all ambiguous parts from @p sourceName and make it lower case.
     *
     * TODO
     *
     * Ambiguous parts are date and time parameters. Timetable items should be shared between
     * data sources for the same stop, also if their date and/or time values differ.
     * Without removing these parameters eg. departure data sources with a little different time
     * values will not share it's departures and download them twice (at least the overlapping ones).
     **/
    static QString disambiguateSourceName( const QString &sourceName );

    inline static uint hashForDeparture( const QVariantHash &departure ) {
        return hashForDeparture( departure[Enums::toString(Enums::DepartureDateTime)].toDateTime(),
                static_cast<Enums::VehicleType>(departure[Enums::toString(Enums::TypeOfVehicle)].toInt()),
                departure[Enums::toString(Enums::TransportLine)].toString(),
                departure[Enums::toString(Enums::Target)].toString() );
    };

    inline static uint hashForDeparture( const TimetableData &departure ) {
        return hashForDeparture( departure[Enums::DepartureDateTime].toDateTime(),
                static_cast<Enums::VehicleType>(departure[Enums::TypeOfVehicle].toInt()),
                departure[Enums::TransportLine].toString(),
                departure[Enums::Target].toString() );
    };

    static uint hashForDeparture( const QDateTime &departure, Enums::VehicleType vehicleType,
                                  const QString &lineString, const QString &target )
    {
        return qHash( QString("%1%2%3%4").arg(departure.toString("dMMyyhhmmss"))
                      .arg(static_cast<int>(vehicleType))
                      .arg(lineString)
                      .arg(target.trimmed().toLower()) );
    };

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

    TimetableDataSource *dataSourceFromAdditionDataTimer( QTimer *timer ) const;
    bool enoughDataAvailable( DataSource *dataSource, const SourceRequestData &sourceData ) const;

    QHash< QString, ProviderPointer > m_providers; // Already loaded service providers by ID
    QVariantHash m_erroneousProviders; // List of erroneous service provider IDs as keys
                                       // and error messages as values
    QHash< QString, DataSource* > m_dataSources; // Data objects for data sources, stored by
                                                 // unambiguous data source name
    QFileSystemWatcher *m_fileSystemWatcher; // Watch the service provider directory

    QTimer *m_providerUpdateDelayTimer;
    QStringList m_runningSources; // Sources which are currently being processed
};

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
*/

/** @page pageUsage Usage

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
If this is @c true, the other fields are not available, instead a field @em errorMessage
is available explaining the error. If this if @c false, the provider did not encounter any errors.
But the provider may still not be ready to use, if the @em state field contains a state string
other than @em "ready". If no @em state field is available, the provider can also be considered
to be ready.</td></tr>

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
Depending on the @em state additional fields may be available. For example with the
@em "importing_gtfs_feed" state a field @em progress is available.
GTFS providers that are @em "ready" also offer these fields as state data:
@em gtfsDatabasePath (the path to the GTFS database file), @em gtfsDatabaseSize
(the size in bytes of the GTFS database).</td></tr>
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
space (" "), then the ID of the service provider to use, e.g. "de_db" for db.de, a service provider
for germany ("Deutsche Bahn"). The following parameters are separated by "|" and start with the
parameter name followed by "=" and the value. The sorting of the additional parameters doesn't
matter. The parameter <i>stop</i> is needed and can be the stop name or the stop ID. If the service
provider has useSeparateCityValue set to @c true (see @ref usage_serviceproviders_sec), the
parameter <i>city</i> is also needed (otherwise it is ignored).
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
    Use the @ref PublicTransportService to start and monitor the import.</td></tr>
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
<tr><td><i>TransportLine</i></td> <td>QString</td> <td>The name of the public transport line,
e.g. "S1", "6", "1E", "RB 24155".</td></tr>
<tr><td><i>Target</i></td> <td>QString</td> <td>The name of the target / origin of the public
transport line.</td></tr>
<tr><td><i>DepartureDateTime</i></td> <td>QDateTime</td> <td>The date and time of the
departure / arrival.</td></tr>
<tr><td><i>TypeOfVehicle</i></td> <td>int</td> <td>An integer containing the ID of the vehicle type
used for the departure/arrival. When you are using libpublictransporthelper, you can cast this ID
to PublicTransport::VehicleType and get more information about the vehicle type using the static
functions of PublicTransport::Global. Alternatively you can use the "VehicleTypes" data source,
it stores vehicle type information in a hash with vehicle type ID's as keys.
See also @ref usage_vehicletypes_sec. </td></tr>
<tr><td><i>Nightline</i></td> <td>bool</td> <td>Wheather or not the public transport line
is a night line.</td></tr>
<tr><td><i>Expressline</i></td> <td>bool</td> <td>Wheather or not the public transport line
is an express line.</td></tr>
<tr><td><i>Platform</i></td> <td>QString</td> <td>The platform from/at which the vehicle
departs/arrives.</td></tr>
<tr><td><i>Delay</i></td> <td>int</td> <td>The delay in minutes, 0 means 'on schedule',
-1 means 'no delay information available'.</td></tr>
<tr><td><i>DelayReason</i></td> <td>QString</td> <td>The reason of a delay.</td></tr>
<tr><td><i>Status</i></td> <td>QString</td> <td>The status of the departure, if available.</td></tr>
<tr><td><i>JourneyNews</i></td> <td>QString</td> <td>News for the journey.</td></tr>
<tr><td><i>Operator</i></td> <td>QString</td>
<td>The company that is responsible for the journey.</td></tr>
<tr><td><i>RouteStops</i></td> <td>QStringList</td> <td>A list of stops of the departure/arrival
to it's destination stop or a list of stops of the journey from it's start to it's destination stop.
If 'routeStops' and 'routeTimes' are both set, they contain the same number of elements.
And elements with equal indices are associated (the times at which the vehicle is at the stops).
</td></tr>
<tr><td><i>RouteTimes</i></td> <td>QList< QTime > (stored as QVariantList)</td>
<td>A list of times of the departure/arrival to it's destination stop. If 'routeStops' and
'routeTimes' are both set, they contain the same number of elements. And elements with
equal indices are associated (the times at which the vehicle is at the stops).</td></tr>
<tr><td><i>RouteExactStops</i></td> <td>int</td> <td>The number of exact route stops.
The route stop list is not complete from the last exact route stop.</td></tr>
</table>

@note The service provider may not load all data by default. To load missing data
  ("additional data"), use the timetable service's operation "requestAdditionalData",
  @ref usage_service_sec.

<br />
@section usage_journeys_sec Receiving Journeys from A to B
To get a list of journeys from one stop to antoher you need to construct the name of the data
source (much like the data source for departures / arrivals). The data source name begins with
"Journeys". Next comes a space (" "), then the ID of the service provider to use, e.g. "de_db"
for db.de, a service provider for germany ("Deutsche Bahn").
The following parameters are separated by "|" and start with the parameter name followed by "=".
The sorting of the additional parameters doesn't matter. The parameters <i>originStop</i> and
<i>targetStop</i> are needed and can be the stop names or the stop IDs. If the service provider
has useSeparateCityValue set to @c true (see @ref usage_serviceproviders_sec), the parameter
<i>city</i> is also needed (otherwise it is ignored).<br />
The following parameters are allowed:<br />
<table>
<tr><td><i>originStop</i></td> <td>The name or ID of the origin stop.</td></tr>
<tr><td><i>targetStop</i></td> <td>The name or ID of the target stop.</td></tr>
<tr><td><i>city</i></td> <td>The city to get journeys for, if needed.</td></tr>
<tr><td><i>maxCount</i></td> <td>The maximum journey count to get.</td></tr>
<tr><td><i>timeOffset</i></td>
<td>The offset in minutes from now for the first journey to get.</td></tr>
<tr><td><i>time</i></td> <td>The time for the first journey to get (in format "hh:mm").</td></tr>
<tr><td><i>datetime</i></td> <td>The date and time for the first journey to get
(QDateTime::fromString() is used with default parameters to parse the date).</td></tr>
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
                "Journeys de_db|originStop=Pappelstraße, Bremen|targetStop=Kirchweg, Bremen",
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

@subsection usage_service_later_items Request earlier/later items
Use the operations @b "requestEarlierItems" and @b "requestLaterItems" to get more timetable items
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
*/

/** @page pageServiceProviders Add Support for new Service Providers
@par Sections
    @li @ref provider_infos_xml
    @li @ref provider_infos_script
    @li @ref examples
    @li @ref examples_xml_gtfs
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
<td><b>\<serviceProvider\> </b></td>
<td>Root</td> <td>Required</td> <td>This is the root item.</td></tr>

<tr><td><b>\<xml_file_version type="publictransport" version="1.0" /\> </b></td>
<td>\<serviceProvider\> </td> <td>Not used</td>
<td>This is currently not used by the data engine. But to be sure that the xml is parsed correctly
you should add this tag. The version is the version of the xml file structure,
current version is 1.0.</td></tr>

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

<tr><td style="color:#666600">
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
<td><b>\<type\> </b></td><td>\<serviceProvider\></td> <td>Required</td>
<td>Can be either HTML or XML.</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<url\> </b></td><td>\<serviceProvider\></td> <td>Required</td>
<td>An url to the service provider home page.</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<shortUrl\> </b></td><td>\<serviceProvider\></td> <td>Required</td>
<td>A short version of the url, used as link text.</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<script\> </b></td><td>\<serviceProvider\></td>
<td>Required for script provider plugins</td>
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

<tr><td><b>\<fallbackCharset\> </b></td><td>\<serviceProvider\> </td> <td>Optional</td>
<td>The charset of documents to be downloaded. Depending on the used service provider this might
be needed or not. Scripts can use this value.</td></tr>

<tr><td><b>\<credit\> </b></td><td>\<serviceProvider\> </td> <td>(Optional)</td>
<td>A courtesy string that is required to be shown to the user when showing the timetable data
of the GTFS feed. If this tag is not given, a short default string is used,
eg. "data by: www.provider.com" or only the link (depending on available space).
Please check the license agreement for using the GTFS feed for such a string and include it here.
</td></tr>

<tr style="background-color: #ffdddd;">
<td><b>\<feedUrl\> </b></td><td>\<serviceProvider\> </td> <td>(Required only with "GTFS" type)</td>
<td>An URL to the GTFS feed to use. Use an URL to the latest available feed.</td></tr>

<tr><td><b>\<realtimeTripUpdateUrl\> </b></td><td>\<serviceProvider\> </td>
<td>(Optional, only used with "GTFS" type)</td>
<td>An URL to a GTFS-realtime data source with trip updates. If this tag is not present delay
information will not be available.</td></tr>

<tr><td><b>\<realtimeAlertsUrl\> </b></td><td>\<serviceProvider\> </td>
<td>(Optional, only used with "GTFS" type)</td>
<td>An URL to a GTFS-realtime data source with alerts. If this tag is not present journey news
will not be available.</td></tr>

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

@section provider_infos_script Script file structure
Scripts are executed using QtScript (JavaScript), which can make use of Kross if other script
languages should be used, eg. Python or Ruby. JavaScript is tested, the other languages may also
work. There are functions with special names that get called by the data engine when needed:
@n
getTimetable(), getStopSuggestions(), getJourneys() and features()
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

#endif

struct JourneyRequest;
