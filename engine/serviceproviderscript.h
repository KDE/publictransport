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
#include "serviceprovider.h" // Base class
#include "scripting.h"

namespace ThreadWeaver {
    class Job;
};
namespace Scripting {
    class Storage;
};
class QScriptEngine;
class QScriptProgram;
class ScriptThread;

/** @brief Stores information about a departure/arrival/journey/stop suggestion. */
typedef QHash<TimetableInformation, QVariant> TimetableData;

using namespace Scripting;

/**
 * @brief The base class for all scripted service providers.
 *
 * Scripts are executed in a separate thread and do network requests synchronously
 * from withing the script. Scripts are written in ECMAScript (QScript), but the "kross" extension
 * gets loaded automatically, so that you can also use other languages supported by Kross.
 *
 * To use eg. Python code in the script, the following code can be used:
 * @code
 *  var action = Kross.action( "MyPythonScript" ); // Create Kross action
 *  action.addQObject( action, "MyAction" ); // Propagate action to itself
 *  action.setInterpreter( "python" ); // Set the interpreter to use, eg. "python", "ruby"
 *  action.setCode("import MyAction ; print 'This is Python. name=>',MyAction.interpreter()");
 *  action.trigger(); // Run the script
 * @endcode
 */
class ServiceProviderScript : public ServiceProvider {
    Q_OBJECT

public:
    /** @brief States of the script, used for loading the script only when needed. */
    enum ScriptState {
        WaitingForScriptUsage = 0x00,
        ScriptLoaded = 0x01,
        ScriptHasErrors = 0x02
    };

public:
    void import( const QString &import, QScriptEngine *engine );

    /** @brief The name of the script function to get a list of used TimetableInformation's. */
    static const char *SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS;

    /** @brief The name of the script function to download and parse departures/arrivals. */
    static const char *SCRIPT_FUNCTION_GETTIMETABLE;

    /** @brief The name of the script function to download and parse journeys. */
    static const char *SCRIPT_FUNCTION_GETJOURNEYS;

    /** @brief The name of the script function to download and parse stop suggestions. */
    static const char *SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;

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

    static bool isTestResultUnchanged( const QString &providerId,
                                       const QSharedPointer<KConfig> &cache );

    virtual bool isTestResultUnchanged( const QSharedPointer<KConfig> &cache ) const;

    /** @brief Whether or not the script has been successfully loaded. */
    bool isScriptLoaded() const { return m_scriptState == ScriptLoaded; };

    /** @brief Whether or not the script has errors. */
    bool hasScriptErrors() const { return m_scriptState == ScriptHasErrors; };

    QString errorMessage() const { return m_errorMessage; };

    /** @brief Gets a list of features that this service provider supports through a script. */
    virtual QStringList features() const;

    /**
     * @brief Requests a list of departures.
     * When the departure/arrival list is completely received @ref departureListReceived is emitted.
     **/
    virtual void requestDepartures( const DepartureRequest &request );

    /**
     * @brief Requests a list of arrivals.
     * When the arrival list is completely received @ref departureListReceived is emitted.
     **/
    virtual void requestArrivals( const ArrivalRequest &request );

    virtual void requestJourneys( const JourneyRequest &request );

    virtual void requestStopSuggestions( const StopSuggestionRequest &request );

protected slots:
    void departuresReady( const QList<TimetableData> &data,
                          ResultObject::Features features, ResultObject::Hints hints,
                          const QString &url, const GlobalTimetableInfo &globalInfo,
                          const DepartureRequest &request, bool couldNeedForcedUpdate = false );

    inline void arrivalsReady( const QList<TimetableData> &data,
                               ResultObject::Features features, ResultObject::Hints hints,
                               const QString &url, const GlobalTimetableInfo &globalInfo,
                               const DepartureRequest &request, bool couldNeedForcedUpdate = false )
    {
        departuresReady( data, features, hints, url, globalInfo, request, couldNeedForcedUpdate );
    };

    void journeysReady( const QList<TimetableData> &data, ResultObject::Features features,
                        ResultObject::Hints hints, const QString &url,
                        const GlobalTimetableInfo &globalInfo,
                        const JourneyRequest &request, bool couldNeedForcedUpdate = false );

    void stopSuggestionsReady( const QList<TimetableData> &data, ResultObject::Features features,
                               ResultObject::Hints hints, const QString &url,
                               const GlobalTimetableInfo &globalInfo,
                               const StopSuggestionRequest &request,
                               bool couldNeedForcedUpdate = false );

    void jobStarted( ThreadWeaver::Job *job );
    void jobDone( ThreadWeaver::Job *job );
    void jobFailed( ThreadWeaver::Job *job );

protected:
    bool lazyLoadScript();
    QStringList readScriptFeatures( const QSharedPointer<KConfig> &cache );

    /** @brief Whether or not the source XML file should be usable to get timetable data. */
    virtual bool runTests( QString *errorMessage = 0 ) const;

private:
    ScriptState m_scriptState; // The state of the script
    QStringList m_scriptFeatures; // Caches the features the script provides
    ScriptThread *m_thread;
    QHash< QString, PublicTransportInfoList > m_publishedData;
    QScriptProgram *m_script;
    Storage *m_scriptStorage;
    QMutex *m_mutex;
    QString m_errorMessage;
};

#endif // Multiple inclusion guard
