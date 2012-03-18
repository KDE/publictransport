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
* @brief This file contains the base class for accessors using script files
*   for parsing that are used by the public transport data engine.
*
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_SCRIPT_HEADER
#define TIMETABLEACCESSOR_SCRIPT_HEADER

// Own includes
#include "timetableaccessor.h" // Base class
#include "scripting.h"

namespace ThreadWeaver {
    class Job;
}
namespace Scripting {
    class Storage;
};
class QScriptProgram;
class ScriptThread;

/** @brief Stores information about a departure/arrival/journey/stop suggestion. */
typedef QHash<TimetableInformation, QVariant> TimetableData;

using namespace Scripting;

/**
 * @brief The base class for all scripted accessors.
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
class TimetableAccessorScript : public TimetableAccessor {
    Q_OBJECT

    // Because the XML accessor uses TimetableAccessorScript::parseDocumentPossibleStops().
    friend class TimetableAccessorXml;

public:
    /** @brief States of the script, used for loading the script only when needed. */
    enum ScriptState {
        WaitingForScriptUsage = 0x00,
        ScriptLoaded = 0x01,
        ScriptHasErrors = 0x02
    };

private:
    QMutex mutex; // TODO
public:
    void import( const QString &import, QScriptEngine *engine ) {
        mutex.lock();
        engine->importExtension(import);
        mutex.unlock();
    };

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
     * @brief Creates a new TimetableAccessorScript object with the given information.
     *
     * @param info Information about how to download and parse the documents of a service provider.
     *   If this is 0 a default TimetableAccessorInfo instance gets created.
     *
     * @note Can be used if you have a custom TimetableAccessorInfo object.
     *   TimetableAccessorXml uses this to create an HTML accessor for parsing of stop lists.
     **/
    TimetableAccessorScript( TimetableAccessorInfo *info = 0 );

    /** @brief Destructor. */
    virtual ~TimetableAccessorScript();

    /** @brief Whether or not the script has been successfully loaded. */
    bool isScriptLoaded() const { return m_scriptState == ScriptLoaded; };

    /** @brief Whether or not the script has errors. */
    bool hasScriptErrors() const { return m_scriptState == ScriptHasErrors; };

    /** @brief Gets a list of features that this accessor supports through a script. */
    virtual QStringList scriptFeatures() const;

    /**
     * @brief Requests a list of departures/arrivals. When the departure/arrival list
     *   is completely received @ref departureListReceived is emitted.
     **/
    virtual void requestDepartures( const DepartureRequestInfo &requestInfo );

    virtual void requestJourneys( const JourneyRequestInfo &requestInfo );

    virtual void requestStopSuggestions(
            const StopSuggestionRequestInfo &requestInfo );

protected slots:
    void departuresReady( const QList<TimetableData> &data,
                          ResultObject::Features features, ResultObject::Hints hints,
                          const QString &url, const GlobalTimetableInfo &globalInfo,
                          const DepartureRequestInfo &requestInfo, bool couldNeedForcedUpdate = false );

    void journeysReady( const QList<TimetableData> &data, ResultObject::Features features,
                        ResultObject::Hints hints, const QString &url,
                        const GlobalTimetableInfo &globalInfo,
                        const JourneyRequestInfo &requestInfo, bool couldNeedForcedUpdate = false );

    void stopSuggestionsReady( const QList<TimetableData> &data, ResultObject::Features features,
                               ResultObject::Hints hints, const QString &url,
                               const GlobalTimetableInfo &globalInfo,
                               const StopSuggestionRequestInfo &requestInfo,
                               bool couldNeedForcedUpdate = false );

    void jobStarted( ThreadWeaver::Job *job );
    void jobDone( ThreadWeaver::Job *job );
    void jobFailed( ThreadWeaver::Job *job );

protected:
    bool lazyLoadScript();
    QStringList readScriptFeatures();

private:
    ScriptState m_scriptState; // The state of the script
    QStringList m_scriptFeatures; // Caches the features the script provides
    ScriptThread *m_thread;
    QHash< QString, PublicTransportInfoList > m_publishedData;
    QScriptProgram *m_script;
    Storage *m_scriptStorage;
};

#endif // TIMETABLEACCESSOR_SCRIPT_HEADER
