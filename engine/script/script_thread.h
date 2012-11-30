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
* @brief This file contains a thread which executes service provider plugin scripts.
*
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SCRIPTTHREAD_HEADER
#define SCRIPTTHREAD_HEADER

// Own includes
#include "enums.h"
#include "scripting.h"
#include "scriptobjects.h"
#include "serviceproviderdata.h"

// KDE includes
#include <ThreadWeaver/Job> // Base class

// Qt includes
#include <QScriptEngineAgent> // Base class
#include <QPointer>

class QMutex;
class AbstractRequest;
class DepartureRequest;
class ArrivalRequest;
class JourneyRequest;
class StopSuggestionRequest;
class StopsByGeoPositionRequest;
class AdditionalDataRequest;
class MoreItemsRequest;

class ServiceProviderData;
namespace Scripting {
    class Storage;
    class Network;
    class Helper;
    class ResultObject;
};

class QScriptProgram;
class QScriptEngine;
class QEventLoop;

using namespace Scripting;

/** @brief Stores information about a departure/arrival/journey/stop suggestion. */
typedef QHash<Enums::TimetableInformation, QVariant> TimetableData;

typedef NetworkRequest* NetworkRequestPtr;
QScriptValue networkRequestToScript( QScriptEngine *engine, const NetworkRequestPtr &request );
void networkRequestFromScript( const QScriptValue &object, NetworkRequestPtr &request );
bool importExtension( QScriptEngine *engine, const QString &extension );

/**
 * @brief Script function to include external script files.
 *
 * Calls to this function need to be the first statements in the global context of the script file,
 * otherwise an exception gets thrown. It expects one argument, the name of the file to be included,
 * without it's path. The file needs to be in the same directory as the main script.
 * If the file is already included this function does nothing. A list of included files gets stored
 * in the engine's global object, in the "includedFiles" property, as QStringList.
 * @see maxIncludeLine()
 **/
QScriptValue include( QScriptContext *context, QScriptEngine *engine );

/** @brief Get the maximum line number for valid include() calls in @p program. */
quint16 maxIncludeLine( const QString &program );

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
 **/
class ScriptJob : public ThreadWeaver::Job {
    Q_OBJECT

public:
    /**
     * @brief Creates a new ScriptJob.
     *
     * @param script The script to executes.
     * @param data Information about the service provider.
     * @param scriptStorage The shared Storage object.
     **/
    explicit ScriptJob( const ScriptData &data, const QSharedPointer< Storage > &scriptStorage,
                        QObject* parent = 0 );

    /** @brief Destructor. */
    virtual ~ScriptJob();

    /** @brief Return a pointer to the object containing inforamtion about the request of this job. */
    virtual const AbstractRequest* request() const = 0;

    /** @brief Overwritten from ThreadWeaver::Job to return whether or not the job was successful. */
    virtual bool success() const;

    /** @brief Return the number of items which are already published. */
    int publishedItems() const;

    /** @brief Return a string describing the error, if success() returns false. */
    QString errorString() const;

    /** @brief Return the URL of the last finished request. */
    QString lastDownloadUrl() const;

    /** @brief Return an URL for the last finished request that should be shown to users. */
    QString lastUserUrl() const;

signals:
    /** @brief Signals ready TimetableData items. */
    void departuresReady( const QList<TimetableData> &departures,
                          ResultObject::Features features, ResultObject::Hints hints,
                          const QString &url, const GlobalTimetableInfo &globalInfo,
                          const DepartureRequest &request, bool couldNeedForcedUpdate = false );

    /** @brief Signals ready TimetableData items. */
    void arrivalsReady( const QList<TimetableData> &arrivals,
                        ResultObject::Features features, ResultObject::Hints hints,
                        const QString &url, const GlobalTimetableInfo &globalInfo,
                        const ArrivalRequest &request, bool couldNeedForcedUpdate = false );

    /** @brief Signals ready TimetableData items. */
    void journeysReady( const QList<TimetableData> &journeys,
                        ResultObject::Features features, ResultObject::Hints hints,
                        const QString &url, const GlobalTimetableInfo &globalInfo,
                        const JourneyRequest &request, bool couldNeedForcedUpdate = false );

    /** @brief Signals ready TimetableData items. */
    void stopSuggestionsReady( const QList<TimetableData> &stops,
                               ResultObject::Features features, ResultObject::Hints hints,
                               const QString &url, const GlobalTimetableInfo &globalInfo,
                               const StopSuggestionRequest &request,
                               bool couldNeedForcedUpdate = false );

    /** @brief Signals ready additional data for a TimetableData item. */
    void additionalDataReady( const TimetableData &data,
                              ResultObject::Features features, ResultObject::Hints hints,
                              const QString &url, const GlobalTimetableInfo &globalInfo,
                              const AdditionalDataRequest &request,
                              bool couldNeedForcedUpdate = false );

protected slots:
    /** @brief Handle the ResultObject::publish() signal by emitting dataReady(). */
    void publish();

protected:
    /** @brief Perform the job. */
    virtual void run();

    /** @brief Load @p script into the engine and insert some objects/functions. */
    bool loadScript( QScriptProgram *script );

    bool waitFor( QObject *sender, const char *signal, WaitForType type );

    bool hasDataToBePublished() const;

    void handleError( const QString &errorMessage );

    QScriptEngine *m_engine;
    QMutex *m_mutex;
    ScriptData m_data;
    ScriptObjects m_objects;
    QEventLoop *m_eventLoop;
    int m_published;
    bool m_quit;
    bool m_success;
    QString m_errorString;
    QString m_lastUrl;
    QString m_lastUserUrl;
};

class DepartureJobPrivate;
class DepartureJob : public ScriptJob {
    Q_OBJECT

public:
    explicit DepartureJob( const ScriptData &data, const QSharedPointer< Storage > &scriptStorage,
                           const DepartureRequest& request, QObject* parent = 0);

    virtual ~DepartureJob();

    virtual const AbstractRequest* request() const;

private:
    const DepartureJobPrivate *d;
};

class ArrivalJobPrivate;
class ArrivalJob : public ScriptJob {
    Q_OBJECT

public:
    explicit ArrivalJob( const ScriptData &data, const QSharedPointer< Storage > &scriptStorage,
                         const ArrivalRequest& request, QObject* parent = 0);

    virtual ~ArrivalJob();

    virtual const AbstractRequest* request() const;

private:
    const ArrivalJobPrivate *d;
};

class JourneyJobPrivate;
class JourneyJob : public ScriptJob {
    Q_OBJECT

public:
    explicit JourneyJob( const ScriptData &data, const QSharedPointer< Storage > &scriptStorage,
                         const JourneyRequest& request, QObject* parent = 0);

    virtual ~JourneyJob();

    virtual const AbstractRequest* request() const;

private:
    const JourneyJobPrivate *d;
};

class StopSuggestionsJobPrivate;
class StopSuggestionsJob : public ScriptJob {
    Q_OBJECT

public:
    explicit StopSuggestionsJob( const ScriptData &data,
                                 const QSharedPointer< Storage > &scriptStorage,
                                 const StopSuggestionRequest& request,
                                 QObject* parent = 0);

    virtual ~StopSuggestionsJob();

    virtual const AbstractRequest* request() const;

private:
    const StopSuggestionsJobPrivate *d;
};

class StopsByGeoPositionJobPrivate;
class StopsByGeoPositionJob : public ScriptJob {
    Q_OBJECT

public:
    explicit StopsByGeoPositionJob( const ScriptData &data,
            const QSharedPointer< Storage > &scriptStorage,
            const StopsByGeoPositionRequest& request, QObject* parent = 0);

    virtual ~StopsByGeoPositionJob();

    virtual const AbstractRequest* request() const;

private:
    const StopsByGeoPositionJobPrivate *d;
};

class AdditionalDataJobPrivate;
class AdditionalDataJob : public ScriptJob {
    Q_OBJECT

public:
    explicit AdditionalDataJob( const ScriptData &data,
                                const QSharedPointer< Storage > &scriptStorage,
                                const AdditionalDataRequest& request,
                                QObject* parent = 0);
    virtual ~AdditionalDataJob();

    virtual const AbstractRequest* request() const;

private:
    const AdditionalDataJobPrivate *d;
};

class MoreItemsJobPrivate;
class MoreItemsJob : public ScriptJob {
    Q_OBJECT

public:
    explicit MoreItemsJob( const ScriptData &data,
                           const QSharedPointer< Storage > &scriptStorage,
                           const MoreItemsRequest& request,
                           QObject* parent = 0);
    virtual ~MoreItemsJob();

    virtual const AbstractRequest* request() const;

private:
    const MoreItemsJobPrivate *d;
};

#endif // Multiple inclusion guard
