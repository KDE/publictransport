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

// Own includes
#include "debuggerstructures.h"

// PublicTransport engine includes
#include <engine/serviceproviderdata.h>

// KDE includes
#include <ThreadWeaver/Job>

// If this is defined debug output gets generated when a job starts or ends
#define DEBUG_JOB_START_END

namespace Scripting
{
    class Storage;
    class Network;
    class ResultObject;
    class Helper;
    class DataStreamPrototype;
}

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
    /** @brief Job types. */
    enum Type {
        LoadScript, /**< A job of type LoadScriptJob. */
        EvaluateInContext, /**< A job of type EvaluateInContextJob. */
        ExecuteConsoleCommand, /**< A job of type ExecuteConsoleCommandJob. */
        CallScriptFunction, /**< A job of type CallScriptFunctionJob. */
        TestFeatures, /**< A job of type TestFeaturesJob. */
        TimetableDataRequest /**< A job of type TimetableDataRequestJob. */
    };

    /** @brief Constructor. */
    explicit DebuggerJob( DebuggerAgent *debugger, const ServiceProviderData &data,
                          QMutex *engineMutex, QObject* parent = 0 );

    /** @brief Destructor. */
    virtual ~DebuggerJob();

    /** @brief Get the type of this debugger job. */
    virtual Type type() const = 0;

    static QString typeToString( Type type );

    /** @brief Get a string with information about this job. */
    virtual QString toString() const { return typeToString(type()); };

    virtual void requestAbort();

    DebuggerAgent *debugger() const { return m_debugger; };

    /** @brief Overwritten from ThreadWeaver::Job to return whether or not the job was successful. */
    virtual bool success() const;

    /**
     * @brief Get information about the result of the job as QString.
     * If success() returns false, this contains an error message.
     **/
    inline QString explanation() const { return m_explanation; };

    /** @brief Get the ServiceProviderData object used by the job. */
    inline const ServiceProviderData providerData() const { return m_data; };

    /**
     * @brief Only for debugging, prints to output that the job has ended (and how).
     * @note This method gets called automatically, when debuggerRun() returns. Only call this
     *   manually, when the thread will be killed before returning from debuggerRun().
     **/
    void debugJobEnd() const;

protected:
    // Expects m_engineMutex and m_mutex to be unlocked
    void handleError( QScriptEngine *engine, const QString &message = QString(),
                      EvaluationResult *result = 0 );

    /** @brief Override this method instead of run(). */
    virtual void debuggerRun() = 0;

    /** @brief Sends output to the debugger when a job starts/ends. */
    void run();

    DebuggerAgent *const m_debugger;
//     Storage *m_scriptStorage;
//     QSharedPointer<Network> m_scriptNetwork;
//     QSharedPointer<ResultObject> m_scriptResult;
    const ServiceProviderData m_data;
    bool m_success;
    QString m_explanation;
    QMutex *m_mutex;
    QMutex *m_engineMutex;
};

/**
 * @brief Loads the script, ie. tests it for runtime errors.
 **/
class LoadScriptJob : public DebuggerJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Get the type of this debugger job. */
    virtual Type type() const { return LoadScript; };

protected:
    /**
     * @brief Creates a new job that leads a script.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param data The ServiceProviderData object of the used service provider plugin.
     * @param engineMutex A pointer to the global mutex to protect the QScriptEngine.
     * @param script A QScriptProgram object containing the new script code.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit LoadScriptJob( DebuggerAgent *debugger, const ServiceProviderData &data,
                            QMutex *engineMutex, QScriptProgram *script,
                            Scripting::Helper *scriptHelper, Scripting::ResultObject *scriptResult,
                            Scripting::Network *scriptNetwork, Scripting::Storage *scriptStorage,
                            Scripting::DataStreamPrototype *dataStreamPrototype,
                            const QMetaObject &enums, QObject* parent = 0 )
            : DebuggerJob(debugger, data, engineMutex, parent),
              m_script(script), m_scriptHelper(scriptHelper), m_scriptResult(scriptResult),
              m_scriptNetwork(scriptNetwork), m_scriptStorage(scriptStorage),
              m_dataStreamPrototype(dataStreamPrototype), m_enums(enums) {};

    virtual void debuggerRun();
    bool loadScriptObjects();
    QStringList globalFunctions() const { return m_globalFunctions; };

private:
    QScriptProgram *m_script;
    Scripting::Helper *const m_scriptHelper;
    Scripting::ResultObject *const m_scriptResult;
    Scripting::Network *const m_scriptNetwork;
    Scripting::Storage *const m_scriptStorage;
    Scripting::DataStreamPrototype *const m_dataStreamPrototype;
    const QMetaObject m_enums;
    QStringList m_globalFunctions;
};

/**
 * @brief Runs script code in the context where the script is currently interrupted.
 **/
class EvaluateInContextJob : public DebuggerJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Get the type of this debugger job. */
    virtual Type type() const { return EvaluateInContext; };

    /** @brief Gets the result of the evaluation, when the job has finished. */
    EvaluationResult result() const;

    virtual QString toString() const;

protected slots:
    void evaluationAborted( const QString &errorMessage );

protected:
    /**
     * @brief Creates a new job that evaluates script code in the current engines context.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param data The ServiceProviderData object of the used service provider plugin.
     * @param engineMutex A pointer to the global mutex to protect the QScriptEngine.
     * @param program The script code to be evaluated in this job.
     * @param context A name for the evaluation context, shown in backtraces.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit EvaluateInContextJob( DebuggerAgent *debugger, const ServiceProviderData &data,
                                   QMutex *engineMutex, const QString &program,
                                   const QString &context, QObject* parent = 0 )
            : DebuggerJob(debugger, data, engineMutex, parent),
              m_program(program), m_context(QString(context).replace( '\n', ' ' )) {};

    virtual void debuggerRun();

private:
    const QString m_program;
    const QString m_context;
    EvaluationResult m_result;
};

/**
 * @brief Executes a console command.
 **/
class ExecuteConsoleCommandJob : public DebuggerJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Get the type of this debugger job. */
    virtual Type type() const { return ExecuteConsoleCommand; };

    /** @brief Gets the ConsoleCommand that gets run in this job. */
    const ConsoleCommand command() const;

    /**
     * @brief Gets the "answer string" of the console command, when the job has finished.
     *
     * The "answer string" or return value can contain HTML and may be empty.
     **/
    QString returnValue() const;

    virtual QString toString() const;

protected:
    /**
     * @brief Creates a new job that executes a console @p command.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param data The ServiceProviderData object of the used service provider plugin.
     * @param engineMutex A pointer to the global mutex to protect the QScriptEngine.
     * @param command The console command to execute in this job.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit ExecuteConsoleCommandJob( DebuggerAgent *debugger, const ServiceProviderData &data,
                                       QMutex *engineMutex, const ConsoleCommand &command,
                                       QObject* parent = 0 )
            : DebuggerJob(debugger, data, engineMutex, parent), m_command(command) {};

    virtual void debuggerRun();

private:
    const ConsoleCommand m_command;
    QString m_returnValue;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
