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
* @brief This file contains the base class for all service provider plugins
*   used by the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SERVICEPROVIDER_HEADER
#define SERVICEPROVIDER_HEADER

// Own includes
#include "enums.h"
#include "departureinfo.h"

// KDE includes
#include <KUrl>

// Qt includes
#include <QHash>
#include <QStringList>

class QScriptEngine;
class KConfig;
class KConfigGroup;
class KJob;

class AbstractRequest;
class DepartureRequest;
class ArrivalRequest;
class StopSuggestionRequest;
class StopsByGeoPositionRequest;
class AdditionalDataRequest;
class JourneyRequest;
class MoreItemsRequest;

class StopInfo;
class DepartureInfo;
class JourneyInfo;

class ServiceProviderTestData;
class ServiceProviderData;
class ChangelogEntry;

/**
 * @brief Get timetable information for public transport from different service providers.
 *
 * This class can be instantiated to create an invalid service provider. To create a valid
 * provider instantiate one of the derivates of this class.
 * The easiest way to implement support for a new service provider is to add an XML file describing
 * the service provider and a script to parse timetable documents.
 * If that's not enough a new class can be derived from ServiceProvider or it's derivates.
 * These functions should then be overwritten:
 *   @li requestDepartures()
 *   @li requestArrivals()
 *   @li requestStopSuggestions()
 *   @li requestJourneys()
 * If one of these functions isn't overridden, the associated timetable data can not be accessed
 * from the service provider.
 **/
class ServiceProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY( QString id READ id )
    Q_PROPERTY( QList<Enums::ProviderFeature> features READ features )
    Q_PROPERTY( QString country READ country )
    Q_PROPERTY( QStringList cities READ cities )
    Q_PROPERTY( QString credit READ credit )
    Q_PROPERTY( int minFetchWait READ minFetchWait )
    Q_PROPERTY( bool useSeparateCityValue READ useSeparateCityValue )
    Q_PROPERTY( bool onlyUseCitiesInList READ onlyUseCitiesInList )

public:
    /**
     * @brief Constructs a new ServiceProvider object.
     *
     * You should use getSpecificProvider() to get an service provider that can download and
     * parse documents from the given service provider ID.
     *
     * @param data The data eg. read from a service provider plugin XML file, to construct a
     *   ServiceProvider for. If this is 0 an invalid ServiceProvider gets created.
     *   ServiceProvider takes ownership for @p data.
     * @param parent The parent QObject.
     **/
    explicit ServiceProvider( const ServiceProviderData *data = 0, QObject *parent = 0,
                              const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>(0) );

    /** @brief Destructor. */
    virtual ~ServiceProvider();

    /** @brief Create a new provider of the given @p type. */
    static ServiceProvider *createProvider( Enums::ServiceProviderType type, QObject *parent = 0 );

    /** @brief Create an invalid provider. */
    static inline ServiceProvider *createInvalidProvider( QObject *parent = 0 ) {
        return createProvider(Enums::InvalidProvider, parent); };

    /**
     * @brief Whether or not the cached test result is unchanged.
     *
     * Derived classes should override this function to indicate when test results might have
     * changed, eg. because an additionally needed file has been modified. If true gets returned
     * this prevents that runTests() gets called again. If false gets returned runTests() will
     * be called again the next time the ServiceProvider object gets created.
     *
     * @param cache A shared pointer to the cache.
     * @see runTests()
     **/
    virtual bool isTestResultUnchanged( const QSharedPointer<KConfig> &cache ) const {
        Q_UNUSED( cache );
        return true;
    };

    ServiceProviderTestData runSubTypeTest( const ServiceProviderTestData &oldTestData,
                                            const QSharedPointer< KConfig > cache ) const;

    /** @brief Get the ID of this service provider. */
    QString id() const;

    /** @brief Get the type of this provider. */
    Enums::ServiceProviderType type() const;

    /** @brief Whether or not the source XML file was modified since the cache was last updated. */
    bool isSourceFileModified( const QSharedPointer<KConfig> &cache ) const;

    /**
     * @brief Get the minimum seconds to wait between two data-fetches from the service provider.
     *
     * The default implementation simply takes the value from ServiceProviderData::minFetchWait(),
     * but minimally 60 seconds to not produce too many updates.
     * @param updateFlags Flags to take into consideration when calculating the result, eg. whether
     *   or not the result gets used for a manual data source update.
     **/
    virtual int minFetchWait( UpdateFlags updateFlags = DefaultUpdateFlags ) const;

    /**
     * @brief Get the date and time when new data should be fetched from the service provider.
     *
     * @param updateFlags Flags to take into consideration when calculating the result, eg. whether
     *   or not the result gets used for a manual data source update.
     * @param lastUpdate The date and time of the last update of the data. Must be valid, there
     *   cannot be a 'next' update without a previous one.
     * @param latestForSufficientChanges The latest date and time at which an update should be made
     *   to not run out of timetable items over time (passed items get removed). This is not used
     *   if @p updateFlags has the SourceHasConstantTime flag set. Nevertheless it must be valid.
     * @see minFetchWait()
     **/
    virtual QDateTime nextUpdateTime( UpdateFlags updateFlags, const QDateTime &lastUpdate,
                                      const QDateTime &latestForSufficientChanges,
                                      const QVariantHash &data = QVariantHash() ) const;

    /**
     * @brief Get a list of features that this provider supports.
     * The default implementation returns an empty list.
     **/
    virtual QList<Enums::ProviderFeature> features() const { return QList<Enums::ProviderFeature>(); };

    /** @brief The country for which the provider returns results. */
    QString country() const;

    /** @brief A list of cities for which the service providers returns results. */
    QStringList cities() const;

    /** @brief Get a credit string to be shown with the timetable data. */
    QString credit() const;

    /** @brief Get a pointer to the ServiceProviderData object for this provider. */
    const ServiceProviderData *data() const { return m_data; };

    /** @brief Get the number of currently running requests. */
    virtual int runningRequests() const { return 0; };

    /** @brief Abort all currently running requests. */
    virtual void abortAllRequests() {};

    /**
     * @brief Checks the type of @p request and calls the associated request function.
     *
     * Calls requestDepartures() if @p request is of type DepartureRequest, requestArrivals()
     * if @p request is of type ArrivalRequest and so on. The request object being pointed to
     * by @p request can be deleted after calling this function.
     **/
    void request( AbstractRequest *request );

    /**
     * @brief Request departures as described in @p request.
     *
     * When the departures are completely received departuresReceived() gets emitted.
     * The default implementation does nothing.
     * @param request Information about the departure request.
     **/
    virtual void requestDepartures( const DepartureRequest &request );

    /**
     * @brief Request arrivals as described in @p request.
     *
     * When the arrivals are completely received arrivalsReceived() gets emitted.
     * The default implementation does nothing.
     * @param request Information about the arrival request.
     **/
    virtual void requestArrivals( const ArrivalRequest &request );

    /**
     * @brief Request journeys as described in @p request.
     *
     * When the journeys are completely received journeysReceived() gets emitted.
     * The default implementation does nothing.
     * @param request Information about the journey request.
     **/
    virtual void requestJourneys( const JourneyRequest &request );

    /**
     * @brief Request stop suggestions as described in @p request.
     *
     * When the stop suggestions are completely received stopsReceived() gets emitted.
     * The default implementation does nothing.
     * @param request Information about the stop suggestion request.
     **/
    virtual void requestStopSuggestions( const StopSuggestionRequest &request );

    /**
     * @brief Request stops by geo position as described in @p request.
     *
     * When the stops are completely received stopsReceived() gets emitted.
     * The default implementation does nothing.
     * @param request Information about the stops by geo position request.
     **/
    virtual void requestStopsByGeoPosition( const StopsByGeoPositionRequest &request );

    /**
     * @brief Request additional data for a valid timetable item in the engine.
     *
     * When the additional data is completely received additionalDataReceived() gets emitted.
     * The default implementation does nothing.
     * @param request Information about the additional data request.
     **/
    virtual void requestAdditionalData( const AdditionalDataRequest &request );

    /** @brief Request more items for a data source. */
    virtual void requestMoreItems( const MoreItemsRequest &moreItemsRequest );

    /** @brief Whether or not the city should be put into the "raw" url. */
    virtual bool useSeparateCityValue() const;

    /**
     * @brief Whether or not cities may be chosen freely.
     *
     * @return @c True if only cities in the list returned by cities() are valid.
     * @return @c False (default) if cities may be chosen freely, but may be invalid.
     **/
    virtual bool onlyUseCitiesInList() const;

    /**
     * @brief Encodes the url in @p str using the charset in @p charset. Then it is percent encoded.
     *
     * @see charsetForUrlEncoding()
     **/
    static QString toPercentEncoding( const QString &str, const QByteArray &charset );

protected:
    /**
     * @brief Get the charset used to encode urls before percent-encoding them. Normally
     *   this charset is UTF-8. But that doesn't work for sites that require parameters
     *   in the url (..&param=x) to be encoded in that specific charset.
     *
     * @see ServiceProvider::toPercentEncoding()
     **/
    virtual QByteArray charsetForUrlEncoding() const;

    /**
     * @brief Run sub-type tests in derived classes.
     *
     * Derived classes should override this function to do additional tests, eg. test for
     * additionally needed files and their correctness. After this function has been called it's
     * result gets stored in the cache. The cached value gets used until isTestResultUnchanged()
     * returns false, the sub-type test gets marked as pending or the provider source XML file
     * gets modified.
     * The default implementation simply returns true.
     *
     * @param errorMessage If not 0, an error message gets stored in the QString being pointed to.
     * @return @c True, if all tests completed successfully, @c false otherwise.
     * @see isTestResultUnchanged()
     * @see ServiceProviderTestData::SubTypeTestPassed
     * @see ServiceProviderTestData::SubTypeTestFailed
     **/
    virtual bool runTests( QString *errorMessage = 0 ) const
    {
        Q_UNUSED( errorMessage );
        return true;
    };

    QString m_curCity; /**< @brief Stores the currently used city. */
    const ServiceProviderData *const m_data; /**< @brief Stores service provider data.
            * This ServiceProviderData object contains all static data needed by ServiceProvider.
            * ServiceProvider uses this object to request/receive the correct data and execute
            * the correct script for a specific service provider.
            * To get a non-const copy of this object, use ServiceProviderData::clone(). */

signals:
    /**
     * @brief Emitted when a new departure list has been received.
     *
     * @param provider The provider that was used to get the departures.
     * @param requestUrl The url used to request the information.
     * @param departures A list of departures that were received.
     * @param request Information about the request for the just received departure list.
     * @see ServiceProvider::useSeperateCityValue()
     **/
    void departuresReceived( ServiceProvider *provider, const QUrl &requestUrl,
            const DepartureInfoList &departures, const GlobalTimetableInfo &globalInfo,
            const DepartureRequest &request );

    /**
     * @brief Emitted when a new arrival list has been received.
     *
     * @param provider The provider that was used to get the arrivals.
     * @param requestUrl The url used to request the information.
     * @param arrivals A list of arrivals that were received.
     * @param request Information about the request for the just received arrival list.
     * @see ServiceProvider::useSeperateCityValue()
     **/
    void arrivalsReceived( ServiceProvider *provider, const QUrl &requestUrl,
            const ArrivalInfoList &arrivals, const GlobalTimetableInfo &globalInfo,
            const ArrivalRequest &request );

    /**
     * @brief Emitted when a new journey list has been received.
     *
     * @param provider The provider that was used to get the journeys.
     * @param requestUrl The url used to request the information.
     * @param journeys A list of journeys that were received.
     * @param request Information about the request for the just received journey list.
     * @see ServiceProvider::useSeperateCityValue()
     **/
    void journeysReceived( ServiceProvider *provider, const QUrl &requestUrl,
            const JourneyInfoList &journeys, const GlobalTimetableInfo &globalInfo,
            const JourneyRequest &request );

    /**
     * @brief Emitted when a list of stops has been received.
     *
     * @param provider The provider that was used to get the stops.
     * @param requestUrl The url used to request the information.
     * @param stops A pointer to a list of @ref StopInfo objects.
     * @param request Information about the request for the just received stop list.
     * @see ServiceProvider::useSeperateCityValue()
     **/
    void stopsReceived( ServiceProvider *provider, const QUrl &requestUrl,
            const StopInfoList &stops, const StopSuggestionRequest &request );

    /**
     * @brief Emitted when additional data has been received.
     *
     * @param provider The provider that was used to get additional data.
     * @param requestUrl The url used to request the information.
     * @param data A TimetableData object containing the additional data.
     * @param request Information about the request for the just received additional data.
     * @see ServiceProvider::useSeperateCityValue()
     **/
    void additionalDataReceived( ServiceProvider *provider, const QUrl &requestUrl,
                                 const TimetableData &data, const AdditionalDataRequest &request );

    /**
     * @brief Emitted when an error occurred while parsing.
     *
     * @param provider The provider that was used to download and parse information
     *   from the service provider.
     * @param errorCode The error code or NoError if there was no error.
     * @param errorString If @p errorCode isn't NoError this contains a
     *   description of the error.
     * @param requestUrl The url used to request the information.
     * @param request Information about the request that resulted in the error.
     * @see ServiceProvider::useSeperateCityValue()
     **/
    void requestFailed( ServiceProvider *provider, ErrorCode errorCode, const QString &errorString,
            const QUrl &requestUrl, const AbstractRequest *request );

    void forceUpdate();

protected:
    /**
     * @brief Whether or not realtime data is available in the @p data of a timetable data source.
     *
     * The default implementation checks for the provider feature @c Enums::ProvidesDelays and
     * checks @p data for a "delayInfoAvailable" key and returns it's boolean value if it exists,
     * otherwise it returns @c false.
     */
    virtual bool isRealtimeDataAvailable( const QVariantHash &data ) const;

private:
    static QString gethex( ushort decimal );

    bool m_idAlreadyRequested;
};

#endif // Multiple inclusion guard
