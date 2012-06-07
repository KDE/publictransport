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
#include <engine/global.h>
#include <engine/script_thread.h>

// KDE includes
#include <KLocalizedString>
#include <KDebug>
#include <ThreadWeaver/DependencyPolicy>
#include <ThreadWeaver/Thread>
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
    if ( !isFinished() ) {
        requestAbort();
    }
    delete m_mutex;
}

bool DebuggerJob::success() const
{
    QMutexLocker locker( m_mutex );
    return m_success;
}

void DebuggerJob::requestAbort()
{
    kDebug() << "Abort requested";
    ThreadWeaver::debug( 0, "Abort requested\n" );
    m_debugger->abortDebugger();
    if ( m_engineMutex->tryLock() ) {
        m_debugger->engine()->abortEvaluation();
        m_engineMutex->unlockInline();
    }
}

QString DebuggerJob::typeToString( DebuggerJob::Type type )
{
    switch ( type ) {
    case LoadScript:
        return "LoadScriptJob";
    case EvaluateInContext:
        return "EvaluateInContextJob";
    case ExecuteConsoleCommand:
        return "ExecuteConsoleCommandJob";
    case CallScriptFunction:
        return "CallScriptFunctionJob";
    case TestUsedTimetableInformations:
        return "TestUsedTimetableInformationsJob";
    case TimetableDataRequest:
        return "TimetableDataRequestJob";
    default:
        return QString("Unknown job type %1").arg(type);
    }
}

QString EvaluateInContextJob::toString() const
{
    QMutexLocker locker( m_mutex );
    QString programString = m_program;
    if ( programString.length() > 100 ) {
        programString = programString.left(100) + "...";
    }
    return QString("%1 (%2)").arg( DebuggerJob::toString() ).arg( programString );
}

QString ExecuteConsoleCommandJob::toString() const
{
    QMutexLocker locker( m_mutex );
    return QString("%1 (%2)").arg( DebuggerJob::toString() ).arg( m_command.toString() );
}

void DebuggerJob::run()
{
#ifdef DEBUG_JOB_START_END
    qDebug() << "\n****** Start" << toString().replace( '\n', ' ' ).toUtf8().constData();
    ThreadWeaver::debug( 0, "\nStart %s\n", toString().replace( '\n', ' ' ).toUtf8().constData() );
#endif

    debuggerRun();
    debugJobEnd();
}

void DebuggerJob::debugJobEnd() const
{
#ifdef DEBUG_JOB_START_END
    const QString result = m_success ? "Success" : "Fail";
    qDebug() << "******" << result.toUtf8().constData() << toString().replace( '\n', ' ' ).toUtf8().constData();
    ThreadWeaver::debug( 0, "%s %s\n", result.toUtf8().constData(), toString().replace( '\n', ' ' ).toUtf8().constData() );
#endif
}

void LoadScriptJob::debuggerRun()
{
    m_mutex->lockInline();
    QScriptSyntaxCheckResult syntax = QScriptEngine::checkSyntax( m_script->sourceCode() );
    if ( syntax.state() == QScriptSyntaxCheckResult::Error ) {
        kDebug() << "Syntax error at" << syntax.errorLineNumber() << syntax.errorColumnNumber()
                 << syntax.errorMessage();
        if ( syntax.errorColumnNumber() < 0 ) {
            m_explanation = i18nc("@info/plain", "Syntax error at line %1: <message>%2</message>",
                                  syntax.errorLineNumber(), syntax.errorMessage());
        } else {
            m_explanation = i18nc("@info/plain", "Syntax error at line %1, column %2: <message>%3</message>",
                                  syntax.errorLineNumber(), syntax.errorColumnNumber(),
                                  syntax.errorMessage());
        }
        m_success = false;
        m_mutex->unlockInline();
        return;
    }

    DebuggerAgent *debugger = m_debugger;
    QScriptEngine *engine = m_debugger->engine();
//     const TimetableAccessorInfo info = m_info;
    QScriptProgram script = *m_script;
    m_mutex->unlockInline();

    // Initialize the script
//     kDebug() << "Reload script text" << info.scriptFileName();
//     m_script = new QScriptProgram( program, info.scriptFileName() );
    if ( !loadScriptObjects() ) {
        return;
    }

    debugger->setExecutionControlType( ExecuteRun );

    m_engineMutex->lockInline();
    // Evaluate the script
    QScriptValue returnValue = engine->evaluate( script );
    const QString functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE;
    QScriptValue function = engine->globalObject().property( functionName );
    m_engineMutex->unlockInline();


    if ( !function.isFunction() ) {
        ThreadWeaver::debug( 0, " - Load script ERROR: Did not find function %s\n",
                             functionName.toUtf8().constData() );
        kDebug() << "Did not find" << functionName << "function in the script!";
        m_explanation = i18nc("@info/plain", "Did not find a '%1' function in the script.",
                              functionName);
        m_success = false;
    }

    m_engineMutex->lockInline();
    if ( engine->hasUncaughtException() ) {
        const QString uncaughtException = engine->uncaughtException().toString();
        ThreadWeaver::debug( 0, " - Load script ERROR: Uncaught exception: %s",
                             uncaughtException.toUtf8().constData() );
        kDebug() << "Error in the script" << engine->uncaughtExceptionLineNumber()
                 << uncaughtException;
        kDebug() << "Backtrace:" << engine->uncaughtExceptionBacktrace().join("\n");
        m_engineMutex->unlockInline();

        QMutexLocker locker( m_mutex );
        m_explanation = i18nc("@info/plain", "Error in the script: "
                              "<message>%1</message>.", uncaughtException);
        m_success = false;
        m_debugger->debugInterrupt(); // TODO
    } else {
        m_engineMutex->unlockInline();

        QMutexLocker locker( m_mutex );
        kDebug() << "Script successfully loaded";
        m_success = true;
    }
}

bool LoadScriptJob::loadScriptObjects()
{
    m_mutex->lockInline();
    QScriptEngine *engine = m_debugger->engine();
    const TimetableAccessorInfo info = m_info;
    m_mutex->unlockInline();

    m_engineMutex->lockInline();
    // Register NetworkRequest class for use in the script
    qScriptRegisterMetaType< NetworkRequestPtr >( engine,
            networkRequestToScript, networkRequestFromScript );

    // Expose objects to the script engine
    if ( !engine->globalObject().property("accessor").isValid() ) {
        engine->globalObject().setProperty( "accessor", engine->newQObject(
                new TimetableAccessorInfo(m_info)) ); // info has only readonly properties
    }
    if ( !engine->globalObject().property("helper").isValid() ) {
        engine->globalObject().setProperty( "helper", engine->newQObject(m_scriptHelper) );
    }
    if ( !engine->globalObject().property("network").isValid() ) {
        engine->globalObject().setProperty( "network", engine->newQObject(m_scriptNetwork) );
    }
    if ( !engine->globalObject().property("storage").isValid() ) {
        engine->globalObject().setProperty( "storage", engine->newQObject(m_scriptStorage) );
    }
    if ( !engine->globalObject().property("result").isValid() ) {
        engine->globalObject().setProperty( "result", engine->newQObject(m_scriptResult) );
    }
    if ( !engine->globalObject().property("enum").isValid() ) {
        engine->globalObject().setProperty( "enum",
                engine->newQMetaObject(&ResultObject::staticMetaObject) );
    }

    // Import extensions (from XML file, <script extensions="...">)
    foreach ( const QString &extension, info.scriptExtensions() ) {
        if ( !importExtension(engine, extension) ) {
            m_mutex->lockInline();
            m_explanation = i18nc("@info/plain", "Could not import extension %1", extension);
            m_success = false;
            m_mutex->unlockInline();
            m_engineMutex->unlockInline();
            return false;
        }
    }
    m_engineMutex->unlockInline();
    return true;
}

void ExecuteConsoleCommandJob::debuggerRun()
{
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

void EvaluateInContextJob::debuggerRun()
{
    m_mutex->lockInline();
    DebuggerAgent *debugger = m_debugger;
    QScriptEngine *engine = m_debugger->engine();
    const QString program = m_program;
    const QString context = m_context;
    m_result.error = false;
    m_success = true;
    m_mutex->unlockInline();

    kDebug() << "Evaluate in context" << context << program;
    ThreadWeaver::debug( 0, " - Evaluate script: '%s' '%s'\n",
                         context.toUtf8().constData(), program.toUtf8().constData() );

    QScriptSyntaxCheckResult syntax = QScriptEngine::checkSyntax( program );
    if ( syntax.state() == QScriptSyntaxCheckResult::Error ) {
        ThreadWeaver::debug( 0, " - Evaluate script SYNTAX ERROR: Line %i, '%s'\n",
                             syntax.errorLineNumber(), syntax.errorMessage().toUtf8().constData() );
        kDebug() << "Error in script code:" << syntax.errorLineNumber() << syntax.errorMessage();

        QMutexLocker locker( m_mutex );
        m_explanation = syntax.errorMessage().isEmpty() ? i18nc("@info/plain", "Syntax error")
                : i18nc("@info/plain", "Syntax error: <message>%1</message>.", syntax.errorMessage());
        m_success = false;
        m_result.error = true;
        m_result.errorLineNumber = syntax.errorLineNumber();
        m_result.errorMessage = m_explanation;
        return;
    }

    connect( debugger, SIGNAL(evaluationInContextAborted(QString)),
             this, SLOT(evaluationAborted(QString)) );

    // Evaluate script code
    EvaluationResult result;
    result.returnValue = debugger->evaluateInContext( /*'{' + */program/* + '}'*/, context,
            &(result.error), &(result.errorLineNumber),
            &(result.errorMessage), &(result.backtrace), InterruptOnExceptions ).toVariant();
    ThreadWeaver::debug( 0, " - Evaluate script Debugger::evaluateInContext() returned %s\n",
                         result.returnValue.toString().toUtf8().constData() );
    kDebug() << "Evaluate in context result:" << result.returnValue.toString();

    disconnect( debugger, SIGNAL(evaluationInContextAborted(QString)),
                this, SLOT(evaluationAborted(QString)) );

    m_engineMutex->lockInline();
    if ( engine->hasUncaughtException() ) {
        m_engineMutex->unlockInline();
        ThreadWeaver::debug( 0, " - Evaluate script ERROR: Line %i, '%s'\n",
                             engine->uncaughtExceptionLineNumber(),
                             engine->uncaughtException().toString().toUtf8().constData() );
        handleError( engine, i18nc("@info/plain", "Error in the script when evaluating '%1' "
                                   "with code <icode>%2</icode>: <message>%3</message>",
                                   context, program, engine->uncaughtException().toString()),
                     &result );
        m_debugger->debugInterrupt(); // TODO
        return;
    }
    m_engineMutex->unlockInline();

    QMutexLocker locker( m_mutex );
    if ( m_success ) {
        m_result = result;
        ThreadWeaver::debug( 0, " - Evaluate script SUCCESS\n" );
    } else {
        ThreadWeaver::debug( 0, " - Evaluate script ERROR: Did not finish in time\n" );
    }
}

void EvaluateInContextJob::evaluationAborted( const QString &errorMessage )
{
    m_mutex->lockInline();
    m_explanation = errorMessage;
    m_success = false;
    m_result.error = true;
    m_result.errorMessage = m_explanation;
    m_mutex->unlockInline();

    m_debugger->abortDebugger();
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

    // Clear exceptions in the engine
//     engine->clearExceptions();
    m_engineMutex->unlockInline();

    QMutexLocker locker( m_mutex );
    m_explanation = !message.isEmpty() ? message
            : i18nc("@info/plain", "Error in the script: <message>%1</message>.",
                    engine->uncaughtException().toString());
    m_success = false;

}

const ConsoleCommand ExecuteConsoleCommandJob::command() const
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
