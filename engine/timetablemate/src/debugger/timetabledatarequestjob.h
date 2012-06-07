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

class QEventLoop;
namespace Scripting
{
    class ResultObject;
    class NetworkRequest;
}

class RequestInfo;
struct JourneyRequestInfo;
struct StopSuggestionRequestInfo;
struct DepartureRequestInfo;

using namespace Scripting;

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
    virtual Type type() const { return TimetableDataRequest; };

    virtual QString toString() const;

    /** @brief Gets the return value of the called script function, when the job has finished. */
    QScriptValue returnValue() const;

    /** @brief Get a list of additional messages, if any. */
    QList< TimetableDataRequestMessage > additionalMessages() const;

    QString functionName() const;

protected slots:
    /**
     * @brief Collect messages sent to Helper::error() as additional messages.
     * This slot gets connected to the errorReceived() signal of the scripts Helper object.
     **/
    void scriptErrorReceived( const QString &message, const QScriptContextInfo &context,
                              const QString &failedParseText );

    /**
     * @brief Collect error messages generated in ResultObject::addData() as additional messages.
     * This slot gets connected to the invalidDataReceived() signal of the scripts ResultObject
     * object.
     **/
    void invalidDataReceived( TimetableInformation information, const QString &message,
                              const QScriptContextInfo &context,
                              int index, const QVariantMap& map );

    void requestFinished( NetworkRequest *request );

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
     * @param info The TimetableAccessorInfo object containing information about the current accessor.
     * @param engineMutex A pointer to the global mutex to protect the QScriptEngine.
     * @param functionName The name of the script function to call.
     * @param arguments A list of arguments for the script function to call.
     * @param debugFlags Flags for the debugger.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit CallScriptFunctionJob( DebuggerAgent *debugger,
            const TimetableAccessorInfo &info, QMutex *engineMutex, const QString &functionName,
            const QVariantList &arguments, DebugFlags debugFlags = DefaultDebugFlags,
            QObject* parent = 0 );

    explicit CallScriptFunctionJob( DebuggerAgent *debugger,
            const TimetableAccessorInfo &info, QMutex *engineMutex,
            DebugFlags debugFlags = DefaultDebugFlags, QObject* parent = 0 );

    virtual void debuggerRun();

    virtual void requestAbort();

    /**
     * @brief Create a list of arguments for the function to call.
     * The engine mutex is locked when this function gets called.
     **/
    virtual QScriptValueList createArgumentScriptValues() const;

    virtual void finish( Scripting::ResultObject *result ) { Q_UNUSED(result); };

    /**
     * @brief Test the results, ie. the return value of the called function.
     * The engine mutex is locked when this function gets called.
     **/
    virtual bool testResults() { return true; };

    enum MessageType {
        NotSameNumberOfItems,
        NotOneItemLessThan
    };

    TimetableDataRequestMessage message( MessageType messageType, TimetableInformation info1,
            TimetableInformation info2, int count1, int count2,
            TimetableDataRequestMessage::Type type = TimetableDataRequestMessage::Warning,
            int lineNumber = -1 );

    const DebugFlags m_debugFlags;
    QString m_functionName;
    QVariantList m_arguments;
    QScriptValue m_returnValue;
    QList< TimetableDataRequestMessage > m_additionalMessages;
    QEventLoop *m_currentLoop;
};

/**
 * @brief Runs the usedTimetableInformation() script function and tests the return value.
 **/
class TestUsedTimetableInformationsJob : public CallScriptFunctionJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Get the type of this debugger job. */
    virtual Type type() const { return TestUsedTimetableInformations; };

    /** @brief Returns the list of valid TimetableInformation elements found in the return value. */
    QList< TimetableInformation > timetableInformations() const;

protected:
    /**
     * @brief Creates a new job that calls the usedTimetableInformation() script function.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param info The TimetableAccessorInfo object containing information about the current accessor.
     * @param engineMutex A pointer to the global mutex to protect the QScriptEngine.
     * @param debugFlags Flags for the debugger.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit TestUsedTimetableInformationsJob( DebuggerAgent *debugger,
            const TimetableAccessorInfo &info, QMutex *engineMutex,
            DebugFlags debugFlags = DefaultDebugFlags, QObject* parent = 0 );

    // Generate "additional messages" for invalid datasets
    virtual bool testResults();

private:
    QList< TimetableInformation > m_timetableInformations;
};

/**
 * @brief Runs a function in the script according to the used RequestInfo.
 **/
class TimetableDataRequestJob : public CallScriptFunctionJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Destructor. */
    virtual ~TimetableDataRequestJob();

    /** @brief Get the type of this debugger job. */
    virtual Type type() const { return TimetableDataRequest; };

    /** @brief Gets the resulting list of timetable data objects, when the job has finished. */
    QList< TimetableData > timetableData() const;

    /** @brief Get a clone of the RequestInfo object. */
    QSharedPointer< RequestInfo > request() const;

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
     * @param info The TimetableAccessorInfo object containing information about the current accessor.
     * @param engineMutex A pointer to the global mutex to protect the QScriptEngine.
     * @param request A RequestInfo object containing information about the request. This object
     *   gets cloned and the original object can be deleted. Should be of type DepartureRequestInfo,
     *   JourneyRequestInfo or StopSuggestionRequestInfo.
     * @param debugFlags Flags for the debugger.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit TimetableDataRequestJob( DebuggerAgent *debugger,
            const TimetableAccessorInfo &info, QMutex *engineMutex, const RequestInfo *request,
            DebugFlags debugFlags = DefaultDebugFlags, QObject* parent = 0 );

    virtual void finish( Scripting::ResultObject *result );

    /**
     * @brief Create a list of arguments for the function to call, using RequestInfo::toScriptValue().
     * The engine mutex is locked when this function gets called.
     **/
    virtual QScriptValueList createArgumentScriptValues() const;

    // Generate "additional messages" for invalid datasets
    virtual bool testResults();

private:
    bool testDepartureData( const DepartureRequestInfo *request );
    bool testStopSuggestionData( const StopSuggestionRequestInfo *request );
    bool testJourneyData( const JourneyRequestInfo *request );

    const RequestInfo *m_request;
    QList< TimetableData > m_timetableData;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
