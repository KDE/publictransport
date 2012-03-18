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
 * @brief Contains ThreadWeaver::Job classes to perform debugger jobs.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de>
 **/

#ifndef DEBUGGERJOBS_H
#define DEBUGGERJOBS_H

// KDE includes
#include <ThreadWeaver/Job>

// Own includes
#include "debuggeragent.h"

// PublicTransport engine includes
#include <engine/timetableaccessor_info.h>
#include <engine/timetableaccessor.h>

class QScriptProgram;
class QScriptEngine;
class QMutex;

namespace Debugger {

class DebuggerAgent;
class Debugger;

/**
 * @brief Abstract base for debugger jobs.
 **/
class DebuggerJob : public ThreadWeaver::Job {
    Q_OBJECT

public:
    explicit DebuggerJob( DebuggerAgent *debugger, const TimetableAccessorInfo &info,
                          QMutex *engineMutex, QObject* parent = 0 );
    virtual ~DebuggerJob();
    virtual void run() = 0;
    virtual void requestAbort();

    /** @brief Overwritten from ThreadWeaver::Job to return whether or not the job was successful. */
    virtual bool success() const;

protected:
    // Expects m_engineMutex and m_mutex to be unlocked
    void handleError( QScriptEngine *engine, const QString &message = QString(),
                      EvaluationResult *result = 0 );

    DebuggerAgent *m_debugger;
//     Storage *m_scriptStorage;
//     QSharedPointer<Network> m_scriptNetwork;
//     QSharedPointer<ResultObject> m_scriptResult;
    const TimetableAccessorInfo m_info;
    bool m_success;
    QString m_errorString;
    QMutex *m_mutex;
    QMutex *m_engineMutex;
};

/**
 * @brief Loads the script, ie. tests it for runtime errors.
 **/
class LoadScriptJob : public DebuggerJob {
    Q_OBJECT
    friend class Debugger;

protected:
    /**
     * @brief Creates a new job that leads a script.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param info The TimetableAccessorInfo object containing information about the current accessor.
     * @param engineMutex A pointer to the global mutex to protect the QScriptEngine.
     * @param script A QScriptProgram object containing the new script code.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit LoadScriptJob( DebuggerAgent *debugger, const TimetableAccessorInfo &info,
                            QMutex *engineMutex, QScriptProgram *script, QObject* parent = 0 )
            : DebuggerJob(debugger, info, engineMutex, parent), m_script(script) {};

    virtual void run();

private:
    QScriptProgram *m_script;
};

/**
 * @brief Runs a function in the script according to the used RequestInfo.
 **/
class ProcessTimetableDataRequestJob : public DebuggerJob {
    Q_OBJECT
    friend class Debugger;

public:
    virtual ~ProcessTimetableDataRequestJob();

    /** @brief Gets the result value of the evaluation, when the job has finished. */
    QScriptValue returnValue() const;

    /** @brief Gets the resulting list of timetable data objects, when the job has finished. */
    QList< TimetableData > timetableData() const;

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
     * @param debugMode Whether or not execution should be interrupted at start.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit ProcessTimetableDataRequestJob( DebuggerAgent *debugger, const TimetableAccessorInfo &info,
            QMutex *engineMutex, const RequestInfo *request,
            DebugMode debugMode = InterruptOnExceptions, QObject* parent = 0 )
            : DebuggerJob(debugger, info, engineMutex, parent),
              m_request(request->clone()), m_debugMode(debugMode) {};

    virtual void run();

private:
    const RequestInfo *m_request;
    const DebugMode m_debugMode;
    QScriptValue m_returnValue;
    QList< TimetableData > m_timetableData;
};

/**
 * @brief Runs script code in the context where the script is currently interrupted.
 **/
class EvaluateInContextJob : public DebuggerJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Gets the result of the evaluation, when the job has finished. */
    EvaluationResult result() const;

    /** @brief Overwritten to only be executed, when the debugger is not running. */
    virtual bool canBeExecuted();

protected:
    /**
     * @brief Creates a new job that evaluates script code in the current engines context.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param info The TimetableAccessorInfo object containing information about the current accessor.
     * @param engineMutex A pointer to the global mutex to protect the QScriptEngine.
     * @param program The script code to be evaluated in this job.
     * @param context A name for the evaluation context, shown in backtraces.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit EvaluateInContextJob( DebuggerAgent *debugger, const TimetableAccessorInfo &info,
                                   QMutex *engineMutex, const QString &program,
                                   const QString &context, QObject* parent = 0 )
            : DebuggerJob(debugger, info, engineMutex, parent),
              m_program(program), m_context(context) {};

    virtual void run();

private:
    const QString m_program;
    const QString m_context;
//     const bool m_interrupt; TODO
    EvaluationResult m_result;
};

/**
 * @brief Executes a console command.
 **/
class ExecuteConsoleCommandJob : public DebuggerJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Gets the ConsoleCommand that gets run in this job. */
    ConsoleCommand command() const;

    /**
     * @brief Gets the "answer string" of the console command, when the job has finished.
     *
     * The "answer string" or return value can contain HTML and may be empty.
     **/
    QString returnValue() const;

protected:
    /**
     * @brief Creates a new job that executes a console @p command.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param info The TimetableAccessorInfo object containing information about the current accessor.
     * @param engineMutex A pointer to the global mutex to protect the QScriptEngine.
     * @param command The console command to execute in this job.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit ExecuteConsoleCommandJob( DebuggerAgent *debugger, const TimetableAccessorInfo &info,
                                       QMutex *engineMutex, const ConsoleCommand &command,
                                       QObject* parent = 0 )
            : DebuggerJob(debugger, info, engineMutex, parent), m_command(command) {};

    virtual void run();

private:
    ConsoleCommand m_command;
    QString m_returnValue;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
