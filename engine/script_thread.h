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
* @brief This file contains a thread which executes timetable accessor scripts.
*
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SCRIPTTHREAD_HEADER
#define SCRIPTTHREAD_HEADER

// Own includes
#include "enums.h"
#include "scripting.h"
#include "timetableaccessor_info.h"

// KDE includes
#include <ThreadWeaver/Job> // Base class

// Qt includes
#include <QScriptEngineAgent> // Base class
#include <QPointer>

struct RequestInfo;
struct StopSuggestionRequestInfo;
struct DepartureRequestInfo;
struct JourneyRequestInfo;

class TimetableAccessorInfo;
namespace Scripting {
    class Storage;
    class Network;
    class Helper;
    class ResultObject;
};

class QScriptProgram;
class QScriptEngine;

using namespace Scripting;

/** @brief Stores information about a departure/arrival/journey/stop suggestion. */
typedef QHash<TimetableInformation, QVariant> TimetableData;

typedef NetworkRequest* NetworkRequestPtr;
QScriptValue networkRequestToScript( QScriptEngine *engine, const NetworkRequestPtr &request );
void networkRequestFromScript( const QScriptValue &object, NetworkRequestPtr &request );
bool importExtension( QScriptEngine *engine, const QString &extension );

/**
 * @brief A QScriptEngineAgent that signals when a script finishes.
 *
 * After a function exit the agent waits a little bit and checks if the script is still executing
 * using QScriptEngineAgent::isEvaluating().
 **/
class ScriptAgent : public QObject, public QScriptEngineAgent {
    Q_OBJECT

public:
    /** @brief Creates a new ScriptAgent instance. */
    ScriptAgent( QScriptEngine* engine = 0, QObject *parent = 0 );

    /** Overwritten to get noticed when a script might have finished. */
    virtual void functionExit( qint64 scriptId, const QScriptValue& returnValue );

signals:
    /** @brief Emitted, when the script is no longer running */
    void scriptFinished();

protected slots:
    void checkExecution();
};

/** @brief Implements the script function 'importExtension()'. */
bool importExtension( QScriptEngine *engine, const QString &extension );

/**
 * @brief Executes a script.
 *
 * @ingroup scripting
 **/
class ScriptJob : public ThreadWeaver::Job {
    Q_OBJECT

public:
    /**
     * @brief Creates a new ScriptJob.
     *
     * @param script The script to executes.
     * @param info Information about the accessor.
     * @param scriptStorage The shared Storage object.
     **/
    explicit ScriptJob( QScriptProgram *script, const TimetableAccessorInfo *info,
                        Storage *scriptStorage, QObject* parent = 0 );

    /** @brief Destructor. */
    virtual ~ScriptJob();

    /** @brief Return a pointer to the object containing inforamtion about the request of this job. */
    virtual const RequestInfo* requestInfo() const = 0;

    /** @brief Return the number of items which are already published. */
    int publishedItems() const { return m_published; };

    /** @brief Return a string describing the error, if success() returns false. */
    QString errorString() const { return m_errorString; };

signals:
    /** @brief Signals ready TimetableData items. */
    void departuresReady( const QList<TimetableData> &departures,
                    ResultObject::Features features, ResultObject::Hints hints,
                    const QString &url, const GlobalTimetableInfo &globalInfo,
                    const DepartureRequestInfo &info, bool couldNeedForcedUpdate = false );

    /** @brief Signals ready TimetableData items. */
    void journeysReady( const QList<TimetableData> &departures,
                        ResultObject::Features features, ResultObject::Hints hints,
                        const QString &url, const GlobalTimetableInfo &globalInfo,
                        const JourneyRequestInfo &info, bool couldNeedForcedUpdate = false );

    /** @brief Signals ready TimetableData items. */
    void stopSuggestionsReady( const QList<TimetableData> &departures,
                               ResultObject::Features features, ResultObject::Hints hints,
                               const QString &url, const GlobalTimetableInfo &globalInfo,
                               const StopSuggestionRequestInfo &info,
                               bool couldNeedForcedUpdate = false );

protected slots:
    /** @brief Handles the ResultObject::publish() signal by emitting dataReady(). */
    void publish();

protected:
    /** @brief Performs the job. */
    virtual void run();

    /** @brief Loads @p script into the engine and insert some objects/functions. */
    bool loadScript( QScriptProgram *script );

    /** @brief Overwritten from ThreadWeaver::Job to return whether or not the job was successful. */
    virtual bool success() const { return m_success; };

    QScriptEngine *m_engine;
    QScriptProgram *m_script;
    Storage *m_scriptStorage;
    QSharedPointer<Network> m_scriptNetwork;
    QSharedPointer<ResultObject> m_scriptResult;

    int m_published;
    bool m_success;
    QString m_errorString;

    TimetableAccessorInfo m_info;
};

class DepartureJobPrivate;
class DepartureJob : public ScriptJob {
    Q_OBJECT

public:
    explicit DepartureJob( QScriptProgram* script, const TimetableAccessorInfo* info,
                           Storage* scriptStorage, const DepartureRequestInfo& request,
                           QObject* parent = 0);

    virtual ~DepartureJob();

    virtual const RequestInfo* requestInfo() const;

private:
    const DepartureJobPrivate *d;
};

class JourneyJobPrivate;
class JourneyJob : public ScriptJob {
    Q_OBJECT

public:
    explicit JourneyJob( QScriptProgram* script, const TimetableAccessorInfo* info,
                         Storage* scriptStorage, const JourneyRequestInfo& request,
                         QObject* parent = 0);

    virtual ~JourneyJob();

    virtual const RequestInfo* requestInfo() const;

private:
    const JourneyJobPrivate *d;
};

class StopSuggestionsJobPrivate;
class StopSuggestionsJob : public ScriptJob {
    Q_OBJECT

public:
    explicit StopSuggestionsJob( QScriptProgram* script, const TimetableAccessorInfo* info,
                                 Storage* scriptStorage, const StopSuggestionRequestInfo& request,
                                 QObject* parent = 0);

    virtual ~StopSuggestionsJob();

    virtual const RequestInfo* requestInfo() const;

private:
    const StopSuggestionsJobPrivate *d;
};

#endif // Multiple inclusion guard
