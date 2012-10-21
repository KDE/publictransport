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
#include "../testmodel.h" // For TestModel::TimetableDataRequestMessage

// PublicTransport engine includes
#include <engine/serviceproviderdata.h>
#include <engine/script/scriptobjects.h>
#include <engine/request.h>

// KDE includes
#include <ThreadWeaver/Job>
#include <QPointer>

// Uncomment to activate more debug messages
#define DEBUG_JOBS(msg) // kDebug() << msg

// If this is defined debug output gets generated when a job starts or ends
// #define DEBUG_JOB_START_END

class QEventLoop;
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
class QSemaphore;

namespace Debugger {

class BreakpointChange;
class BacktraceChange;
class VariableChange;

class DebuggerAgent;
class Debugger;

struct DebuggerJobResult {
    DebuggerJobResult( bool success = true, bool aborted = false,
                       const QVariant returnValue = QVariant(),
                       const QString &explanation = QString(),
                       const QList< TimetableData > &resultData = QList< TimetableData >(),
                       const QList< TimetableDataRequestMessage > &messages =
                            QList< TimetableDataRequestMessage >(),
                       AbstractRequest *request = 0 )
            : success(success), aborted(aborted), returnValue(returnValue),
              explanation(explanation), resultData(resultData), messages(messages),
              request(QSharedPointer<AbstractRequest>(request)) {};

    bool success;
    bool aborted;
    QVariant returnValue;
    QString explanation;
    QList< TimetableData > resultData;
    QList< TimetableDataRequestMessage > messages;
    QSharedPointer< AbstractRequest > request;
};

/**
 * @brief Abstract base for debugger jobs.
 **/
class DebuggerJob : public ThreadWeaver::Job {
    Q_OBJECT
    friend class DebuggerAgent;

public:
    explicit DebuggerJob( const ScriptData &scriptData, const QString &useCase = QString(),
                          QObject* parent = 0 );

    /** @brief Constructor. */
    DebuggerJob( DebuggerAgent *debugger, QSemaphore *engineSemaphore, const ScriptData &scriptData,
                 const ScriptObjects &objects, const QString &useCase = QString(),
                 QObject* parent = 0 );

    /** @brief Destructor. */
    virtual ~DebuggerJob();

    /** @brief Get the type of this debugger job. */
    virtual JobType type() const = 0;

    static QString typeToString( JobType type );

    virtual QString defaultUseCase() const { return QString(); };
    void setUseCase( const QString &useCase );
    QString useCase() const;

    /** @brief Get a string with information about this job. */
    virtual QString toString() const;

    virtual void requestAbort();

    DebuggerAgent *debuggerAgent() const;
    QSemaphore *engineSemaphore() const;

    bool wasAborted() const;

    /** @brief Overwritten from ThreadWeaver::Job to return whether or not the job was successful. */
    virtual bool success() const;

    ScriptObjects objects() const;

    /**
     * @brief Get information about the result of the job as QString.
     * If success() returns false, this contains an error message.
     **/
    QString explanation() const;

    virtual QVariant returnValue() const { return QVariant(); };

    /** @brief Get the ServiceProviderData object used by the job. */
    const ServiceProviderData providerData() const;

#ifdef DEBUG_JOB_START_END
    /**
     * @brief Only for debugging, prints to output that the job has ended (and how).
     * @note This method gets called automatically, when debuggerRun() returns. Only call this
     *   manually, when the thread will be killed before returning from debuggerRun().
     **/
    void debugJobEnd() const;
#endif


    /** @brief Gets the current state of the debugger. */
    DebuggerState state() const;

    /** @brief Whether or not script execution is currently interrupted. */
    bool isInterrupted() const;

    /** @brief Whether or not the script currently gets executed or is interrupted. */
    bool isRunning() const;

    /** @brief Whether or not the script currently gets aborted. */
    bool isAborting() const;

    bool isEvaluating() const;

    /** @brief Whether or not @p program can be evaluated. */
    bool canEvaluate( const QString &program ) const;

    /**
     * @brief Blocks until the debugger has been completely shut down.
     *
     * If the debugger is not running, this function returns immediately.
     * This function should be called before starting another execution to ensure that the debugger
     * state stays clean. Otherwise there may be crashes and unexpected behaviour.
     **/
    void finish() const;

    /** @brief The name of the currently executed source file. */
    QString currentSourceFile() const;

    /** @brief Get the current execution line number. */
    int lineNumber() const;

    /** @brief Get the current execution column number. */
    int columnNumber() const;

    /** @brief Whether or not there was an uncaught exception. */
    bool hasUncaughtException() const;

    /** @brief The line number of an uncaught exception, if any. */
    int uncaughtExceptionLineNumber() const;

    /** @brief The QScriptValue of an uncaught exception, if any. */
    QScriptValue uncaughtException() const;

    /**
     * @brief Checks whether script execution can be interrupted at @p lineNumber.
     *
     * Empty lines or lines with '//' at the beginning are not executable and script execution
     * can not be interrupted there, for example.
     *
     * If the line at @p lineNumber is not evaluatable, the line and the following line are tested
     * together. Up to 25 following lines are currently used to test if there is an evaluatable
     * multiline statement starting at @p lineNumber.
     *
     * @warning This does not always work. The breakpoint may always be skipped although this
     *   function says it could break there.
     **/
    NextEvaluatableLineHint canBreakAt( const QString &fileName, int lineNumber ) const;

    /**
     * @brief Get the first executable line number bigger than or equal to @p lineNumber.
     *
     * This function uses canBreakAt() to check whether or not script execution can be interrupted.
     * If not, the line number gets increased and again checked, etc.
     * If no such line number could be found -1 gets returned.
     **/
    int getNextBreakableLineNumber( const QString &fileName, int lineNumber ) const;

    void attachDebugger( Debugger *debugger );

    void moveScriptObjectsToThread( QThread *thread );
    void gotScriptObjectsBack();

    bool waitFor( QObject *sender, const char *signal, WaitForType type = WaitForNothing );

    virtual void connectScriptObjects( bool doConnect = true );

signals:
    /** @brief Script execution just started. */
    void started( const QDateTime &timestamp );

    /** @brief The script finished and is no longer running */
    void stopped( const QDateTime &timestamp,
                  ScriptStoppedFlags scriptStoppedFlags = ScriptStopped,
                  int uncaughtExceptionLineNumber = -1,
                  const QString &uncaughtException = QString(),
                  const QStringList &backtrace = QStringList() );

    /** @see isInterrupted() */
    void positionChanged( int lineNumber, int columnNumber, int oldLineNumber, int oldColumnNumber );

    /** @brief The state of the debugger has changed from @p oldState to @p newState. */
    void stateChanged( DebuggerState newState, DebuggerState oldState );

    /** @brief Reached @p breakpoint and increased it's hit count. */
    void breakpointReached( const Breakpoint &breakpoint );

    /** @brief An uncaught exception occured at @p lineNumber. */
    void exception( int lineNumber, const QString &errorMessage,
                    const QString &fileName = QString() );

    /** @brief Script execution was just interrupted. */
    void interrupted( int lineNumber, const QString &fileName, const QDateTime &timestamp );

    /** @brief Script execution was just aborted. */
    void aborted();

    /** @brief Script execution was just continued after begin interrupted. */
    void continued( const QDateTime &timestamp, bool willInterruptAfterNextStatement = false );

    /** @brief The script send a @p debugString using the print() function. */
    void output( const QString &debugString, const QScriptContextInfo &contextInfo );

    void informationMessage( const QString &message );
    void errorMessage( const QString &message );

    /**
     * @brief Evaluation of script code in the context of a running script has finished.
     * This signal gets emitted after calling evaluateInContext().
     *
     * @p returnValue The value that was returned from the code that was evaluated.
     **/
    void evaluationInContextFinished( const QScriptValue &returnValue = QScriptValue() );

    void evaluationInContextAborted( const QString &errorMessage );

    /** @brief Variables have changed according to @p change. */
    void variablesChanged( const VariableChange &change );

    /** @brief The backtrace has changed according to @p change. */
    void backtraceChanged( const BacktraceChange &change );

    /** @brief Breakpoints have changed according to @p change. */
    void breakpointsChanged( const BreakpointChange &change );

    void scriptMessageReceived( const QString &errorMessage, const QScriptContextInfo &contextInfo,
                                const QString &failedParseText = QString(),
                                Helper::ErrorSeverity severity = Helper::Information );

    void movedScriptObjectsToThread( QThread *thread );

public slots:
    /**
     * @brief Continue script execution until the next statement.
     * @param repeat The number of repetitions for step into. 0 for only one repetition, the default.
     **/
    virtual void debugStepInto( int repeat = 0 );

    /**
     * @brief Continue script execution until the next statement in the same context.
     * @param repeat The number of repetitions for step over. 0 for only one repetition, the default.
     **/
    virtual void debugStepOver( int repeat = 0 );

    /**
     * @brief Continue script execution until the current function gets left.
     * @param repeat The number of repetitions for step out. 0 for only one repetition, the default.
     **/
    virtual void debugStepOut( int repeat = 0 );

    /** @brief Continue script execution until @p lineNumber is reached. */
    virtual void debugRunUntilLineNumber( const QString &fileName, int lineNumber );

    /** @brief Interrupt script execution. */
    virtual void debugInterrupt();

    /** @brief Continue script execution, only interrupt on breakpoints or uncaught exceptions. */
    virtual void debugContinue();

    /** @brief Abort script execution. */
    virtual void abortDebugger();

    virtual void addBreakpoint( const Breakpoint &breakpoint );
    virtual void updateBreakpoint( const Breakpoint &breakpoint ) { addBreakpoint(breakpoint); };
    virtual void removeBreakpoint( const Breakpoint &breakpoint );

    virtual void slotOutput( const QString &outputString, const QScriptContextInfo &contextInfo );

    virtual void checkExecution();

protected slots:
    void setDefaultUseCase();

    /** @brief The script finished and is no longer running */
    void slotStopped( const QDateTime &timestamp, bool aborted = false,
                      bool hasRunningRequests = false, int uncaughtExceptionLineNumber = -1,
                      const QString &uncaughtException = QString(),
                      const QStringList &backtrace = QStringList() );

    void doSomething();

    void evaluationAborted( const QString &errorMessage );

protected:
    // Expects m_engineSemaphore and m_mutex to be unlocked
    void handleError( QScriptEngine *engine, const QString &message = QString(),
                      EvaluationResult *result = 0 );
    void handleError( int uncaughtExceptionLineNumber = -1,
                      const QString &uncaughtException = QString(),
                      const QStringList &backtrace = QStringList(),
                      const QString &message = QString(), EvaluationResult *result = 0 );

    /** @brief Override this method instead of run(). */
    virtual void debuggerRun() = 0;

    /** @brief Sends output to the debugger when a job starts/ends. */
    void run();

    void disconnectScriptObjects();

    DebuggerAgent *createAgent();
    void destroyAgent();
    void attachAgent( DebuggerAgent *agent );
    void connectAgent( DebuggerAgent *agent, bool doConnect = true, bool connectModels = true );

    virtual void _evaluationAborted( const QString &errorMessage ) { Q_UNUSED(errorMessage); };

    QPointer< DebuggerAgent > m_agent;
    ScriptData m_data;
    ScriptObjects m_objects;
    Debugger *m_debugger;
    bool m_success;
    bool m_aborted;
    QString m_explanation;
    QMutex *const m_mutex;
    QSemaphore *m_engineSemaphore;
    QString m_useCase;
    bool m_quit;
    QThread *m_scriptObjectTargetThread;
    QEventLoop *m_currentLoop;
};

/**
 * @brief Loads the script, ie. tests it for runtime errors.
 **/
class LoadScriptJob : public DebuggerJob {
    Q_OBJECT
    friend class Debugger;

public:
    /** @brief Get the type of this debugger job. */
    virtual JobType type() const { return LoadScript; };

    virtual QString defaultUseCase() const;

protected:
    /**
     * @brief Creates a new job that leads a script.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param data The ServiceProviderData object of the used service provider plugin.
     * @param engineSemaphore A pointer to the global mutex to protect the QScriptEngine.
     * @param script A QScriptProgram object containing the new script code.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit LoadScriptJob( const ScriptData &scriptData, const QString &useCase = QString(),
                            DebugFlags debugFlags = DefaultDebugFlags, QObject* parent = 0 )
            : DebuggerJob(scriptData, useCase, parent), m_debugFlags(debugFlags) {};

    virtual ~LoadScriptJob();

    virtual void debuggerRun();
    QStringList globalFunctions() const;
    QStringList includedFiles() const;
    DebugFlags debugFlags() const;

private:
    QStringList m_globalFunctions;
    QStringList m_includedFiles;
    const DebugFlags m_debugFlags;
};

/**
 * @brief Base class for jobs that can run with the debugger agent of another job.
 **/
class ForeignAgentJob : public DebuggerJob {
    Q_OBJECT
    friend class Debugger;

public:
    virtual ~ForeignAgentJob() {};

    QPointer< DebuggerJob > parentJob() const { return m_parentJob; };

protected:
    explicit ForeignAgentJob( const ScriptData &scriptData, const QString &useCase = QString(),
                              DebuggerJob *parentJob = 0, QObject* parent = 0 );

    virtual void debuggerRun();

    virtual bool runInForeignAgent( DebuggerAgent *agent ) = 0;
    virtual void handleUncaughtException( DebuggerAgent *agent ) { Q_UNUSED(agent); };
    virtual bool waitForFinish( DebuggerAgent *agent, const ScriptObjects &objects );

    QPointer< DebuggerJob > m_parentJob;
};

/**
 * @brief Runs script code in the context where the script is currently interrupted.
 **/
class EvaluateInContextJob : public ForeignAgentJob {
    Q_OBJECT
    friend class Debugger;

public:
    virtual ~EvaluateInContextJob();

    /** @brief Get the type of this debugger job. */
    virtual JobType type() const { return EvaluateInContext; };

    /** @brief Gets the result of the evaluation, when the job has finished. */
    EvaluationResult result() const;

    virtual QVariant returnValue() const;

    virtual QString toString() const;
    virtual QString defaultUseCase() const;

protected:
    /**
     * @brief Creates a new job that evaluates script code in the current engines context.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param data The ServiceProviderData object of the used service provider plugin.
     * @param engineSemaphore A pointer to the global mutex to protect the QScriptEngine.
     * @param program The script code to be evaluated in this job.
     * @param context A name for the evaluation context, shown in backtraces.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit EvaluateInContextJob( const ScriptData &data, const QString &program,
                                   const QString &context, const QString &useCase,
                                   DebuggerJob *runningJob = 0, QObject* parent = 0 );

    virtual bool runInForeignAgent( DebuggerAgent *agent );
    virtual void handleUncaughtException( DebuggerAgent *agent );
    virtual void _evaluationAborted( const QString &errorMessage );

private:
    const QString m_program;
    const QString m_context;
    EvaluationResult m_result;
};

/**
 * @brief Executes a console command.
 **/
class ExecuteConsoleCommandJob : public ForeignAgentJob {
    Q_OBJECT
    friend class Debugger;

public:
    virtual ~ExecuteConsoleCommandJob();

    /** @brief Get the type of this debugger job. */
    virtual JobType type() const { return ExecuteConsoleCommand; };

    /** @brief Gets the ConsoleCommand that gets run in this job. */
    const ConsoleCommand command() const;

    /**
     * @brief Gets the "answer string" of the console command, when the job has finished.
     *
     * The "answer string" or return value can contain HTML and may be empty.
     **/
    virtual QVariant returnValue() const;

    virtual QString toString() const;
    virtual QString defaultUseCase() const;

protected:
    /**
     * @brief Creates a new job that executes a console @p command.
     *
     * @internal Used by Debugger.
     *
     * @param debugger A pointer to the DebuggerAgent used for debugging.
     * @param data The ServiceProviderData object of the used service provider plugin.
     * @param engineSemaphore A pointer to the global mutex to protect the QScriptEngine.
     * @param command The console command to execute in this job.
     * @param parent The parent QObject. Default is 0.
     **/
    explicit ExecuteConsoleCommandJob( const ScriptData &data, const ConsoleCommand &command,
                                       const QString &useCase = QString(),
                                       DebuggerJob *runningJob = 0, QObject* parent = 0 );

    virtual bool runInForeignAgent( DebuggerAgent *agent );

private:
    const ConsoleCommand m_command;
    QString m_returnValue;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
