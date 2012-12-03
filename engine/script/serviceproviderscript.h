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
* @brief This file contains the base class for service providers using script files.
*
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SERVICEPROVIDERSCRIPT_HEADER
#define SERVICEPROVIDERSCRIPT_HEADER

// Own includes
#include "../serviceprovider.h" // Base class
#include "scriptapi.h"
#include "scriptobjects.h"

class ScriptJob;
namespace ScriptApi {
    class Storage;
};
namespace ThreadWeaver {
    class Job;
};
class QScriptEngine;
class QScriptProgram;
class QFileInfo;

/** @brief Stores information about a departure/arrival/journey/stop suggestion. */
typedef QHash<Enums::TimetableInformation, QVariant> TimetableData;

using namespace ScriptApi;

/**
 * @brief The base class for all scripted service providers.
 *
 * Scripts are executed in separate threads and can start synchronous/asynchronous network requests.
 * Scripts are written in QtScript, ie. ECMAScript/JavaScript. See @ref scriptApi for more
 * information.
 *
 * @note Other script languages supported by Kross can be used, the "kross" extension is then
 *   needed. To use eg. Python code in the script, the following code can be used:
 *   @code
 *    var action = Kross.action( "MyPythonScript" ); // Create Kross action
 *    action.addQObject( action, "MyAction" ); // Propagate action to itself
 *    action.setInterpreter( "python" ); // Set the interpreter to use, eg. "python", "ruby"
 *    action.setCode("import MyAction ; print 'This is Python. name=>',MyAction.interpreter()");
 *    action.trigger(); // Run the script
 *   @endcode
 */
class ServiceProviderScript : public ServiceProvider {
    Q_OBJECT

public:
    /** @brief States of the script, used for loading the script only when needed. */
    enum ScriptState {
        WaitingForScriptUsage = 0x00, /**< The script was not loaded,
                * because it was not needed yet. */
        ScriptLoaded = 0x01, /**< The script has been loaded. */
        ScriptHasErrors = 0x02 /**< The script has errors. */
    };

public:
    void import( const QString &import, QScriptEngine *engine );

    /** @brief The name of the script function to get a list of used TimetableInformation's. */
    static const char *SCRIPT_FUNCTION_FEATURES;

    /** @brief The name of the script function to download and parse departures/arrivals. */
    static const char *SCRIPT_FUNCTION_GETTIMETABLE;

    /** @brief The name of the script function to download and parse journeys. */
    static const char *SCRIPT_FUNCTION_GETJOURNEYS;

    /** @brief The name of the script function to download and parse stop suggestions. */
    static const char *SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;

    /** @brief The name of the script function to download and parse additional timetable data. */
    static const char *SCRIPT_FUNCTION_GETADDITIONALDATA;

    /** @brief Gets a list of extensions that are allowed to be imported by scripts. */
    static QStringList allowedExtensions();

    /**
     * @brief Creates a new ServiceProviderScript object with the given information.
     *
     * @param data Information about how to download and parse the documents of a service provider.
     *   If this is 0 a default ServiceProviderData instance gets created.
     *
     * @note Can be used if you have a custom ServiceProviderData object.
     **/
    ServiceProviderScript( const ServiceProviderData *data = 0, QObject *parent = 0,
                           const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>(0) );

    /** @brief Destructor. */
    virtual ~ServiceProviderScript();

    /**
     * @brief Whether or not the cached test result for @p providerId is unchanged.
     *
     * This function tests if the script file or included script files have been modified.
     * @param providerId The provider to check.
     * @param cache A shared pointer to the cache.
     * @see runTests()
     **/
    static bool isTestResultUnchanged( const QString &providerId,
                                       const QSharedPointer<KConfig> &cache );

    /**
     * @brief Whether or not the cached test result is unchanged.
     *
     * This function tests if the script file or included script files have been modified.
     * @param cache A shared pointer to the cache.
     * @see runTests()
     **/
    virtual bool isTestResultUnchanged( const QSharedPointer<KConfig> &cache ) const;

    /** @brief Whether or not the script has been successfully loaded. */
    bool isScriptLoaded() const { return m_scriptState == ScriptLoaded; };

    /** @brief Whether or not the script has errors. */
    bool hasScriptErrors() const { return m_scriptState == ScriptHasErrors; };

    /** @brief Gets a list of features that this service provider supports through a script. */
    virtual QList<Enums::ProviderFeature> features() const;

    /**
     * @brief Request departures as described in @p request.
     * When the departures are completely received departuresReceived() gets emitted.
     **/
    virtual void requestDepartures( const DepartureRequest &request );

    /**
     * @brief Request arrivals as described in @p request.
     * When the arrivals are completely received arrivalsReceived() gets emitted.
     **/
    virtual void requestArrivals( const ArrivalRequest &request );

    /**
     * @brief Request journeys as described in @p request.
     * When the journeys are completely received journeysReceived() gets emitted.
     **/
    virtual void requestJourneys( const JourneyRequest &request );

    /**
     * @brief Request stop suggestions as described in @p request.
     * When the stop suggestions are completely received stopsReceived() gets emitted.
     **/
    virtual void requestStopSuggestions( const StopSuggestionRequest &request );

    /**
     * @brief Request stops by geo position as described in @p request.
     * When the stops are completely received stopsReceived() gets emitted.
     **/
    virtual void requestStopsByGeoPosition(
            const StopsByGeoPositionRequest &request );

    /**
     * @brief Requests additional data as described in @p request.
     * When the additional data is completely received additionDataReceived() gets emitted.
     **/
    virtual void requestAdditionalData( const AdditionalDataRequest &request );

    /**
     * @brief Request more items for a data source as described in @p moreItemsRequest.
     **/
    virtual void requestMoreItems( const MoreItemsRequest &moreItemsRequest );

    /**
     * @brief Get the minimum seconds to wait between two data-fetches from the service provider.
     *
     * For manual updates the result is minimally one minute, for automatic updates 15 minutes.
     * @param updateFlags Flags to take into consideration when calculating the result, eg. whether
     *   or not the result gets used for a manual data source update.
     **/
    virtual int minFetchWait( UpdateFlags updateFlags = DefaultUpdateFlags ) const;

protected slots:
    /** @brief Departure @p data is ready, emits departuresReceived(). */
    void departuresReady( const QList<TimetableData> &data,
                          ResultObject::Features features, ResultObject::Hints hints,
                          const QString &url, const GlobalTimetableInfo &globalInfo,
                          const DepartureRequest &request, bool couldNeedForcedUpdate = false );

    /** @brief Arrival @p data is ready, emits arrivalsReceived(). */
    void arrivalsReady( const QList<TimetableData> &data,
                        ResultObject::Features features, ResultObject::Hints hints,
                        const QString &url, const GlobalTimetableInfo &globalInfo,
                        const ArrivalRequest &request, bool couldNeedForcedUpdate = false );

    /** @brief Journey @p data is ready, emits journeysReceived(). */
    void journeysReady( const QList<TimetableData> &data, ResultObject::Features features,
                        ResultObject::Hints hints, const QString &url,
                        const GlobalTimetableInfo &globalInfo,
                        const JourneyRequest &request, bool couldNeedForcedUpdate = false );

    /** @brief Stop suggestion @p data is ready, emits stopsReceived(). */
    void stopSuggestionsReady( const QList<TimetableData> &data, ResultObject::Features features,
                               ResultObject::Hints hints, const QString &url,
                               const GlobalTimetableInfo &globalInfo,
                               const StopSuggestionRequest &request,
                               bool couldNeedForcedUpdate = false );

    /** @brief Additional @p data is ready, emits additionalDataReceived(). */
    void additionDataReady( const TimetableData &data,
                            ResultObject::Features features, ResultObject::Hints hints,
                            const QString &url, const GlobalTimetableInfo &globalInfo,
                            const AdditionalDataRequest &request,
                            bool couldNeedForcedUpdate = false );

    /** @brief A @p job was started. */
    void jobStarted( ThreadWeaver::Job *job );

    /** @brief A @p job was done. */
    void jobDone( ThreadWeaver::Job *job );

    /** @brief A @p job failed. */
    void jobFailed( ThreadWeaver::Job *job );

protected:
    /** @brief Load the script file. */
    bool lazyLoadScript();

    /**
     * @brief Get a list of features supported by this provider.
     *
     * Uses @p cache to store the result. If the cached result is still valid, ie. the used script
     * file(s) haven't changed, the feature list from the @p cache gets returned as is.
     **/
    QList<Enums::ProviderFeature> readScriptFeatures( const QSharedPointer<KConfig> &cache );

    /** @brief Run script provider specific tests. */
    virtual bool runTests( QString *errorMessage = 0 ) const;

    /** @brief Enqueue @p job in the job queue. */
    void enqueue( ScriptJob *job );

private:
    static bool checkIncludedFiles( const QSharedPointer<KConfig> &cache,
                                    const QString &providerId = QString() );
    static bool checkIncludedFile( const QSharedPointer<KConfig> &cache, const QFileInfo &fileInfo,
                                   const QString &providerId = QString() );

    ScriptState m_scriptState; // The state of the script
    QList<Enums::ProviderFeature> m_scriptFeatures; // Caches the features the script provides
    QHash< QString, PublicTransportInfoList > m_publishedData;

    ScriptData m_scriptData;
    QSharedPointer< Storage > m_scriptStorage;
    QList< ScriptJob* > m_runningJobs;
};

#endif // Multiple inclusion guard
