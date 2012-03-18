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

// Header
#include "debuggerjobs.h"

// Own includes
#include "debugger.h"

// PublicTransport engine includes
#include <engine/timetableaccessor_script.h>

// KDE includes
#include <KLocalizedString>
#include <KDebug>
#include <ThreadWeaver/DependencyPolicy>
#include <threadweaver/DebuggingAids.h>

// Qt includes
#include <QMutex>
#include <QEventLoop>
#include <QTimer>

namespace Debugger {

DebuggerJob::DebuggerJob( DebuggerAgent *debugger, const TimetableAccessorInfo &info,
                          QMutex *engineMutex, QObject *parent )
        : ThreadWeaver::Job(parent), m_debugger(debugger), m_info(info), m_success(true),
          m_mutex(new QMutex()), m_engineMutex(engineMutex)
{
}

DebuggerJob::~DebuggerJob()
{
    delete m_mutex;
}

void DebuggerJob::requestAbort()
{
    ThreadWeaver::debug( 0, "ABORT REQUESTED\n" );
    m_debugger->abortDebugger();
    if ( m_engineMutex->tryLock() ) {
        m_debugger->engine()->abortEvaluation();
        m_engineMutex->unlockInline();
    }
}

ProcessTimetableDataRequestJob::~ProcessTimetableDataRequestJob()
{
    delete m_request;
}

void LoadScriptJob::run()
{
    ThreadWeaver::debug( 0, QString("LOAD SCRIPT JOB RUN %1\n").arg(QTime::currentTime().toString()).toUtf8() );

    m_mutex->lockInline();
    DebuggerAgent *debugger = m_debugger;
    QScriptEngine *engine = m_debugger->engine();
//     const TimetableAccessorInfo info = m_info;
    QScriptProgram script = *m_script;
    m_mutex->unlockInline();

    // Initialize the script
//     kDebug() << "Reload script text" << info.scriptFileName();
//     m_script = new QScriptProgram( program, info.scriptFileName() );

    debugger->setExecutionControlType( ExecuteRun );

    ThreadWeaver::debug( 0, "--> Load script now: engine->evaluate()\n" );
    m_engineMutex->lockInline();
    QScriptValue returnValue = engine->evaluate( script );
    m_engineMutex->unlockInline();
    ThreadWeaver::debug( 0, "--> Load script done\n" );

    kDebug() << "Script evaluated";

    const QString functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE;
    QScriptValue function = engine->globalObject().property( functionName );
    if ( !function.isFunction() ) {
        ThreadWeaver::debug( 0, QString("--> Load script ERROR: Did not find function " +
                                        functionName + '\n').toUtf8() );
        kDebug() << "Did not find" << functionName << "function in the script!";
        m_errorString = i18nc("@info/plain", "Did not find a '%1' function in the script.",
                              functionName);
        m_success = false;
    }

    m_engineMutex->lockInline();
    if ( engine->hasUncaughtException() ) {
        const QString uncaughtException = engine->uncaughtException().toString();
        ThreadWeaver::debug( 0, QString("--> Load script ERROR: Uncaught exception: " +
                                        uncaughtException + '\n').toUtf8() );
        kDebug() << "Error in the script" << engine->uncaughtExceptionLineNumber()
                 << uncaughtException;
        kDebug() << "Backtrace:" << engine->uncaughtExceptionBacktrace().join("\n");
        m_engineMutex->unlockInline();

        QMutexLocker locker( m_mutex );
        m_errorString = i18nc("@info/plain", "Error in the script: "
                              "<message>%1</message>.", uncaughtException);
        m_success = false;
    } else {
        m_engineMutex->unlockInline();
        QMutexLocker locker( m_mutex );
        kDebug() << "Script successfully loaded";
        ThreadWeaver::debug( 0, "--> Load script SUCCESS\n" );
        m_success = true;
    }
}

void ExecuteConsoleCommandJob::run()
{
    ThreadWeaver::debug( 0, QString("EXECUTE CONSOLE COMMAND JOB RUN %1\n").arg(QTime::currentTime().toString()).toUtf8() );

    m_mutex->lockInline();
    DebuggerAgent *debugger = m_debugger;
    ConsoleCommand command = m_command;
    m_mutex->unlockInline();

    // Execute the command
    QString returnValue;
    bool success = debugger->executeCommand( command, &returnValue );

    m_mutex->lockInline();
    m_success = success;
    m_returnValue = returnValue;
    m_mutex->unlockInline();
}

void ProcessTimetableDataRequestJob::run()
{
    ThreadWeaver::debug( 0, QString("PROCESS TIMETABLE DATA REQUEST JOB RUN %1\n").arg(QTime::currentTime().toString()).toUtf8() );

    m_mutex->lockInline();
    DebuggerAgent *debugger = m_debugger;
    QScriptEngine *engine = m_debugger->engine();
    const RequestInfo *request = m_request;
    const DebugMode debugMode = m_debugMode;
    Network *scriptNetwork = new Network( m_info.fallbackCharset() );
//     kDebug() << "Create Network in" << thread();

//     TODO Clear "result" script object
    ResultObject *scriptResult = qobject_cast<ResultObject*>(
            engine->globalObject().property("result").toQObject() );
    Q_ASSERT( scriptResult );
    scriptResult->clear();

    m_mutex->unlockInline();

    QString functionName;
    switch ( request->parseMode ) {
    case ParseForDeparturesArrivals:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE;
        break;
    case ParseForJourneys:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETJOURNEYS;
        break;
    case ParseForStopSuggestions:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;
        break;
    default:
        kDebug() << "Parse mode unsupported:" << request->parseMode;
        delete scriptNetwork;
        return;
    }

    if ( functionName.isEmpty() ) {
        // This should never happen, therefore no i18n
        QMutexLocker locker( m_mutex );
        m_errorString = "Unknown parse mode";
        m_success = false;
        delete scriptNetwork;
        return;
    }

    ThreadWeaver::debug( 0, "--> Run script initialize\n" );
    m_engineMutex->lockInline();
    engine->abortEvaluation();
    engine->globalObject().setProperty( "network", engine->newQObject(scriptNetwork) );
    kDebug() << "Run script job";
    kDebug() << "Values:" << request->stop << request->dateTime;

    QScriptValueList arguments = QScriptValueList() << request->toScriptValue( engine );
    QScriptValue function = engine->globalObject().property( functionName );
    if ( !function.isFunction() ) {
        ThreadWeaver::debug( 0, QString("--> Run script ERROR: Did not find function " +
                                        functionName + '\n').toUtf8() );
        kDebug() << "Did not find" << functionName << "function in the script!";
        engine->globalObject().setProperty( "network", QScriptValue() );
        m_engineMutex->unlockInline();
        delete scriptNetwork;

        QMutexLocker locker( m_mutex );
        m_errorString = i18nc("@info/plain", "Did not find '%1' function in the script.",
                              functionName);
        m_success = false;
        return;
    }

    // Call script function
    ThreadWeaver::debug( 0, "--> Run script now: function.call()\n" );
    kDebug() << "Call script function";
    if ( debugMode == InterruptAtStart ) {
        debugger->debugInterrupt();
    }
    QScriptValue returnValue = function.call( QScriptValue(), arguments );
    m_engineMutex->unlockInline();
    ThreadWeaver::debug( 0, "--> Run script function.call() returned\n" );

    const int finishWaitTime = 500;
    int finishWaitCounter = 0;
    while ( (!debugger->checkHasExited() || scriptNetwork->hasRunningRequests()) && // || engine->isEvaluating()) &&
            finishWaitCounter < 20 ) // Wait maximally 10 seconds, ie. 20 * 500ms
    {
        if ( m_engineMutex->tryLock(100) ) {
            if ( engine->hasUncaughtException() ) {
                engine->globalObject().setProperty( "network", QScriptValue() );
                m_engineMutex->unlockInline();

                ThreadWeaver::debug( 0, QString("--> Run script ERROR: In function " +
                        functionName + ": " + engine->uncaughtException().toString() + '\n').toUtf8() );
                handleError( engine, i18nc("@info/plain", "Error in the script when calling function '%1': "
                            "<message>%2</message>.", functionName, engine->uncaughtException().toString()) );
                delete scriptNetwork;
                return;
            }
            m_engineMutex->unlockInline();
        }

        ThreadWeaver::debug( 0, "--> Run script wait for execution to finish\n" );
        kDebug() << "Wait for the script to finish execution";
        QEventLoop loop;
        connect( m_debugger, SIGNAL(stopped()), &loop, SLOT(quit()) );
        connect( m_debugger, SIGNAL(destroyed(QObject*)), &loop, SLOT(quit()) );
        QTimer::singleShot( finishWaitTime, &loop, SLOT(quit()) );
        loop.exec();
        ++finishWaitCounter;
        kDebug() << "Finished?" << (20 - finishWaitCounter)
                 << scriptNetwork->hasRunningRequests() << engine->isEvaluating();
    }
    ThreadWeaver::debug( 0, "--> Run script execution has finished\n" );
    kDebug() << "calling done.." << finishWaitCounter;

//     GlobalTimetableInfo globalInfo;
//     globalInfo.requestDate = QDate::currentDate();
//     globalInfo.delayInfoAvailable =
//             !m_scriptResult->isHintGiven( ResultObject::NoDelaysForStop );

    if ( finishWaitCounter < 20 ) {
        m_engineMutex->lockInline();
    } else {
        // Script not finished
        m_engineMutex->tryLock();
        engine->abortEvaluation();
    }
    engine->globalObject().setProperty( "network", QScriptValue() ); // TODO
    delete scriptNetwork;
    if ( engine->hasUncaughtException() ) {
        m_engineMutex->unlockInline();
        ThreadWeaver::debug( 0, QString("--> Run script ERROR: In function " +
                functionName + ": " + engine->uncaughtException().toString() + '\n').toUtf8() );
        handleError( engine, i18nc("@info/plain", "Error in the script when calling function '%1': "
                     "<message>%2</message>.", functionName, engine->uncaughtException().toString()) );
        return;
    }
    m_engineMutex->unlockInline();

    QMutexLocker locker( m_mutex );
    m_returnValue = returnValue;
    m_timetableData = scriptResult->data();
    if ( finishWaitCounter < 20 ) {
        ThreadWeaver::debug( 0, "--> Run script SUCCESS\n" );
        m_success = true; // If finishWaitCounter >= 10, the script did not finish in time
    } else {
        ThreadWeaver::debug( 0, "--> Run script ERROR: Did not finish in time\n" );
        m_errorString = i18nc("@info", "The script did not finish in time");
        m_success = false;
    }
}

void EvaluateInContextJob::run()
{
    ThreadWeaver::debug( 0, QString("EVALUATE IN CONTEXT JOB RUN %1\n").arg(QTime::currentTime().toString()).toUtf8() );
    ThreadWeaver::DependencyPolicy::instance().dumpJobDependencies();

    m_mutex->lockInline();
    DebuggerAgent *debugger = m_debugger;
    QScriptEngine *engine = m_debugger->engine();
    const QString program = m_program;
    const QString context = m_context;
//     Network *scriptNetwork = new Network( m_info.fallbackCharset() );
//     kDebug() << "Create Network in" << thread();
    m_result.error = false;
    m_mutex->unlockInline();

//     engine->abortEvaluation();

//     engine->globalObject().setProperty( "network", engine->newQObject(scriptNetwork) );
    kDebug() << "Evaluate in context" << context << program;
    ThreadWeaver::debug( 0, QString("--> Evaluate script: '%1' '%2'\n")
            .arg(context).arg(program).toUtf8() );

    // Store start time of the script
//     QTime time;
//     time.start();

    QScriptSyntaxCheckResult syntax = QScriptEngine::checkSyntax( program );
    if ( syntax.state() == QScriptSyntaxCheckResult::Error ) {
        ThreadWeaver::debug( 0, QString("--> Evaluate script SYNTAX ERROR: Line %1, '%2'\n")
                .arg(syntax.errorLineNumber()).arg(syntax.errorMessage()).toUtf8() );
        kDebug() << "Error in script code:" << syntax.errorLineNumber() << syntax.errorMessage();

        QMutexLocker locker( m_mutex );
        m_errorString = syntax.errorMessage().isEmpty() ? i18nc("@info", "Syntax error")
                : i18nc("@info", "Syntax error: <message>%1</message>.", syntax.errorMessage());
        m_success = false;
        m_result.error = true;
        m_result.errorLineNumber = syntax.errorLineNumber();
        m_result.errorMessage = m_errorString;
        return;
    }

    // Evaluate script code
    ThreadWeaver::debug( 0, "--> Evaluate script now\n" );
    EvaluationResult result;
    result.returnValue = debugger->evaluateInContext( program, context,
            &(result.error), &(result.errorLineNumber),
            &(result.errorMessage), &(result.backtrace) );
    ThreadWeaver::debug( 0, QString("--> Evaluate script Debugger::evaluateInContext() returned " +
            result.returnValue.toString() + '\n').toUtf8() );
//     emit evaluationResult( result ); TODO

    const int finishWaitTime = 250;
    int finishWaitCounter = 0;
    while ( debugger->isRunning() && !debugger->checkHasExited() && finishWaitCounter < 10 ) { // Wait maximally 2.5 seconds, ie. 10 * 250ms
        m_engineMutex->lockInline();
        if ( engine->hasUncaughtException() ) {
            m_engineMutex->unlockInline();
            ThreadWeaver::debug( 0, QString("--> Evaluate script ERROR: Line %1, '%2'\n")
                    .arg(engine->uncaughtExceptionLineNumber())
                    .arg(engine->uncaughtException().toString()).toUtf8() );
            handleError( engine, i18nc("@info/plain", "Error in the script when evaluating '%1' "
                                       "with code <icode>%2</icode>: <message>%3</message>",
                                       context, program, engine->uncaughtException().toString()),
                         &result );
//             engine->globalObject().setProperty( "network", QScriptValue() );
//             delete scriptNetwork;
            return;
        }
        m_engineMutex->unlockInline();

        ThreadWeaver::debug( 0, "--> Evaluate script: Wait for script execution to finish\n" );
        kDebug() << "Wait for the script to finish execution";
        QEventLoop loop;
        connect( m_debugger, SIGNAL(stopped()), &loop, SLOT(quit()) );
        connect( m_debugger, SIGNAL(destroyed(QObject*)), &loop, SLOT(quit()) );
        QTimer::singleShot( finishWaitTime, &loop, SLOT(quit()) );
        loop.exec();
        ++finishWaitCounter;
        kDebug() << "Finished?" << finishWaitCounter << /*scriptNetwork->hasRunningRequests() <<*/ engine->isEvaluating();
    }
    kDebug() << "calling done.." << finishWaitCounter;
        ThreadWeaver::debug( 0, "--> Evaluate script: Script execution has finished\n" );

//     GlobalTimetableInfo globalInfo;
//     globalInfo.requestDate = QDate::currentDate();
//     globalInfo.delayInfoAvailable =
//             !m_scriptResult->isHintGiven( ResultObject::NoDelaysForStop );

    m_engineMutex->lockInline();
    if ( engine->hasUncaughtException() ) {
        m_engineMutex->unlockInline();
        ThreadWeaver::debug( 0, QString("--> Evaluate script ERROR: Line %1, '%2'\n")
                .arg(engine->uncaughtExceptionLineNumber())
                .arg(engine->uncaughtException().toString()).toUtf8() );
        handleError( engine, i18nc("@info/plain", "Error in the script when evaluating '%1' "
                                   "with code <icode>%2</icode>: <message>%3</message>",
                                   context, program, engine->uncaughtException().toString()),
                     &result );
//         engine->globalObject().setProperty( "network", QScriptValue() );
//         delete scriptNetwork;
        return;
    }
    m_engineMutex->unlockInline();

    QMutexLocker locker( m_mutex );
    m_result = result;
    if ( finishWaitCounter < 10 ) {
        ThreadWeaver::debug( 0, "--> Evaluate script SUCCESS\n" );
        m_success = true; // If finishWaitCounter >= 10, the script did not finish in time
    } else {
        ThreadWeaver::debug( 0, QString("--> Evaluate script ERROR: Did not finish in time\n")
                .arg(engine->uncaughtExceptionLineNumber())
                .arg(engine->uncaughtException().toString()).toUtf8() );
        // TODO If the script was interrupted, it will not exit after the evaluation in context has finished
        // TODO signal function exit of injected code
        m_errorString = i18nc("@info", "The script did not finish in time");
        m_success = false;
        m_result.error = true;
        m_result.errorMessage = m_errorString;
    }
//     engine->globalObject().setProperty( "network", QScriptValue() );
//     delete scriptNetwork;
}

void DebuggerJob::handleError( QScriptEngine *engine, const QString &message,
                               EvaluationResult *result )
{
    m_engineMutex->lockInline();
    if ( result ) {
        result->error = true;
        result->errorMessage = engine->uncaughtException().toString();
        result->errorLineNumber = engine->uncaughtExceptionLineNumber();
        result->backtrace = engine->uncaughtExceptionBacktrace();
    }
    kDebug() << (!message.isEmpty() ? message : "Script error")
             << engine->uncaughtExceptionLineNumber() << engine->uncaughtException().toString();
    kDebug() << "Backtrace:" << engine->uncaughtExceptionBacktrace().join("\n");
    m_engineMutex->unlockInline();

    QMutexLocker locker( m_mutex );
    m_errorString = !message.isEmpty() ? message
            : i18nc("@info/plain", "Error in the script: <message>%1</message>.",
                    engine->uncaughtException().toString());
    m_success = false;

}

bool EvaluateInContextJob::canBeExecuted()
{
    const bool can = (m_debugger->isInterrupted() || !m_debugger->isRunning())
            ? ThreadWeaver::Job::canBeExecuted() : false;
    kDebug() << "ThreadWeaver::Job::canBeExecuted() =" << ThreadWeaver::Job::canBeExecuted();
    ThreadWeaver::debug( 0, can ? "EvaluateInContextJob::canBeExecuted() = true\n"
                                : "EvaluateInContextJob::canBeExecuted() = false\n" );
    return /*(m_debugger->isInterrupted() || !m_debugger->isRunning())
            ?*/ ThreadWeaver::Job::canBeExecuted() /*: false*/;
}

QScriptValue ProcessTimetableDataRequestJob::returnValue() const
{
    QMutexLocker locker( m_mutex );
    return m_returnValue;
}

QList<TimetableData> ProcessTimetableDataRequestJob::timetableData() const
{
    QMutexLocker locker( m_mutex );
    return m_timetableData;
}

bool DebuggerJob::success() const
{
    QMutexLocker locker( m_mutex );
    return m_success;
}

ConsoleCommand ExecuteConsoleCommandJob::command() const
{
    QMutexLocker locker( m_mutex );
    return m_command;
}

QString ExecuteConsoleCommandJob::returnValue() const
{
    QMutexLocker locker( m_mutex );
    return m_returnValue;
}

EvaluationResult EvaluateInContextJob::result() const
{
    QMutexLocker locker( m_mutex );
    return m_result;
}

}; // namespace Debugger
