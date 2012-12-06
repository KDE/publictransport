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

/**
 * @file
 * @brief Contains the TimetableDataRequestJob class to run/debug script functions.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de>
 **/

#ifndef TIMETABLEDATAREQUESTJOB_H
#define TIMETABLEDATAREQUESTJOB_H

#include "debuggerjobs.h"

// Own includes
#include "../testmodel.h" // For TestModel::TimetableDataRequestMessage

// Includes form the PublicTransport engine
#include <engine/script/scriptapi.h>

class QEventLoop;
namespace ScriptApi
{
    class ResultObject;
    class NetworkRequest;
}

class AbstractRequest;
struct JourneyRequest;
struct StopSuggestionRequest;
struct DepartureRequest;
struct AdditionalDataRequest;

using namespace ScriptApi;

namespace Debugger {

/**
 * @brief Runs a function in the script.
 **/
class CallScriptFunctionJob : public DebuggerJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Destructor. */
    virtual ~CallScriptFunctionJob();

    /** @brief Get the type of this debugger job. */
    virtual JobType type() const { return TimetableDataRequest; };

    virtual QString toString() const;
    virtual QString defaultUseCase() const;

    /** @brief Gets the return value of the called script function, when the job has finished. */
    virtual QVariant returnValue() const;

    /** @brief Get a list of additional messages, if any. */
    QList< TimetableDataRequestMessage > additionalMessages() const;

    QString functionName() const;

    DebugFlags debugFlags() const;

    virtual void connectScriptObjects( bool doConnect = true );

signals:
    void asynchronousRequestWaitFinished( const QDateTime &timestamp,
                                          int statusCode = 200, int size = 0 );
    void synchronousRequestWaitFinished( int statusCode = 200, int waitingTime = 0, int size = 0 );

protected slots:
    /**
     * @brief Collect messages from the script as additional messages.
     * This slot gets connected to the messageReceived() signal of the scripts Helper object.
     **/
    void slotScriptMessageReceived( const QString &message, const QScriptContextInfo &context,
                                    const QString &failedParseText, Helper::ErrorSeverity severity );

    /**
     * @brief Collect error messages generated in ResultObject::addData() as additional messages.
     * This slot gets connected to the invalidDataReceived() signal of the scripts ResultObject
     * object.
     **/
    void invalidDataReceived( Enums::TimetableInformation information, const QString &message,
                              const QScriptContextInfo &context,
                              int index, const QVariantMap& map );

    void requestFinished( const NetworkRequest::Ptr &request, const QByteArray &data = QByteArray(),
                          bool error = false, const QString &errorString = QString(),
                          const QDateTime &timestamp = QDateTime(),
                          int statusCode = 200, int size = 0 );
    void synchronousRequestFinished( const QString &url, const QByteArray &data = QByteArray(),
                                     bool cancelled = false, int statusCode = 200,
                                     int waitingTime = 0, int size = 0 );

    void scriptStopped( const QDateTime &timestamp, bool aborted,
                        bool hasRunningRequests = false, int uncaughtExceptionLineNumber = -1,
                        const QString &uncaughtException = QString(),
                        const QStringList &backtrace = QStringList() );

protected:
    /**
     * @brief Creates a new job that calls a script function to get timetable data.
     *
     * Which script function gets called depends on the parseMode in @p request, ie. to get
     * departure/arrival data, journey data or stop suggestion data.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param data The ServiceProviderData object containing information about the current
     *   service provider.
     * @param engineSemaphore A pointer to the global mutex to protect the QScriptEngine.
     * @param functionName The name of the script function to call.
     * @param arguments A list of arguments for the script function to call.
     * @param debugFlags Flags for the debugger.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit CallScriptFunctionJob( const ScriptData &scriptData, const QString &functionName,
                                    const QVariantList &arguments, const QString &useCase,
                                    DebugFlags debugFlags = DefaultDebugFlags,
                                    QObject* parent = 0 );

    explicit CallScriptFunctionJob( const ScriptData &scriptData, const QString &useCase,
                                    DebugFlags debugFlags = DefaultDebugFlags,
                                    QObject* parent = 0 );

    virtual void debuggerRun();

    /**
     * @brief Create a list of arguments for the function to call.
     * The engine mutex is locked when this function gets called.
     **/
    virtual QScriptValueList createArgumentScriptValues( DebuggerAgent *debugger ) const;

    virtual void finish( const QList<TimetableData> &data ) { Q_UNUSED(data); };

    /**
     * @brief Test the results, ie. the return value of the called function.
     * The engine mutex is locked when this function gets called.
     **/
    virtual bool testResults() { return true; };

    enum MessageType {
        NotSameNumberOfItems,
        NotOneItemLessThan
    };

    TimetableDataRequestMessage message( MessageType messageType, Enums::TimetableInformation info1,
            Enums::TimetableInformation info2, int count1, int count2,
            TimetableDataRequestMessage::Type type = TimetableDataRequestMessage::Warning,
            const QString &fileName = QString(), int lineNumber = -1 );

    const DebugFlags m_debugFlags;
    QString m_functionName;
    QVariantList m_arguments;
    QVariant m_returnValue;
    QList< TimetableDataRequestMessage > m_additionalMessages;
};

/**
 * @brief Runs the features() script function and tests the return value.
 **/
class TestFeaturesJob : public CallScriptFunctionJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Get the type of this debugger job. */
    virtual JobType type() const { return TestFeatures; };

    /** @brief Returns a list of provider features found in the return value. */
    QList< Enums::ProviderFeature > features() const;

protected:
    /**
     * @brief Creates a new job that calls the usedTimetableInformation() script function.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param data The ServiceProviderData object containing information about the current
     *   service provider.
     * @param engineSemaphore A pointer to the global mutex to protect the QScriptEngine.
     * @param debugFlags Flags for the debugger.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit TestFeaturesJob( const ScriptData &scriptData, const QString &useCase,
                              DebugFlags debugFlags = DefaultDebugFlags, QObject* parent = 0 );

    // Generate "additional messages" for invalid datasets
    virtual bool testResults();

private:
    QList< Enums::ProviderFeature > m_features;
};

/**
 * @brief Runs a function in the script according to the used request object.
 **/
class TimetableDataRequestJob : public CallScriptFunctionJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Destructor. */
    virtual ~TimetableDataRequestJob();

    /** @brief Get the type of this debugger job. */
    virtual JobType type() const { return TimetableDataRequest; };

    /** @brief Gets the resulting list of timetable data objects, when the job has finished. */
    QList< TimetableData > timetableData() const;

    /** @brief Get a clone of the request object. */
    QSharedPointer< AbstractRequest > request() const;

    virtual QString defaultUseCase() const;

protected:
    /**
     * @brief Creates a new job that calls a script function to get timetable data.
     *
     * Which script function gets called depends on the parseMode in @p request, ie. to get
     * departure/arrival data, journey data or stop suggestion data.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param data The ServiceProviderData object containing information about the current
     *   service provider.
     * @param engineSemaphore A pointer to the global mutex to protect the QScriptEngine.
     * @param request A request object containing information about the request. This object
     *   gets cloned and the original object can be deleted. Should be of type DepartureRequest,
     *   JourneyRequest or StopSuggestionRequest.
     * @param debugFlags Flags for the debugger.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit TimetableDataRequestJob( const ScriptData &scriptData, const AbstractRequest *request,
                                      const QString &useCase,
                                      DebugFlags debugFlags = DefaultDebugFlags,
                                      QObject* parent = 0 );

    virtual void finish( const QList<TimetableData> &data );

    /**
     * @brief Create a list of arguments for the function to call, using AbstractRequest::toScriptValue().
     * The engine mutex is locked when this function gets called.
     **/
    virtual QScriptValueList createArgumentScriptValues( DebuggerAgent *debugger ) const;

    // Generate "additional messages" for invalid datasets
    virtual bool testResults();

private:
    bool testDepartureData( const DepartureRequest *request );
    bool testStopSuggestionData( const StopSuggestionRequest *request );
    bool testJourneyData( const JourneyRequest *request );
    bool testAdditionalData( const AdditionalDataRequest *request );

    const AbstractRequest *m_request;
    QList< TimetableData > m_timetableData;
};

} // namespace Debugger

#endif // Multiple inclusion guard
