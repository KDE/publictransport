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
#include "timetabledatarequestjob.h"

// Own includes
#include "debuggeragent.h"

// Public Transport engine includes
#include <engine/script/serviceproviderscript.h>
#include <engine/global.h>
#include <engine/serviceprovider.h>
#include <engine/request.h>

// KDE includes
#include <KDebug>
#include <KLocalizedString>
#include <KGlobal>
#include <KLocale>
#include <KGlobalSettings>
#include <ThreadWeaver/Thread>

// Qt includes
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <QWaitCondition>
#include <QSemaphore>
#include <QApplication>

namespace Debugger {

CallScriptFunctionJob::CallScriptFunctionJob( const ScriptData &scriptData,
        const QString &functionName, const QVariantList &arguments, const QString &useCase,
        DebugFlags debugFlags, QObject *parent )
        : DebuggerJob(scriptData, useCase, parent), m_debugFlags(debugFlags),
          m_functionName(functionName), m_arguments(arguments)
{
}

CallScriptFunctionJob::CallScriptFunctionJob( const ScriptData &scriptData, const QString &useCase,
                                              DebugFlags debugFlags, QObject *parent )
        : DebuggerJob(scriptData, useCase, parent), m_debugFlags(debugFlags)
{
}

TestFeaturesJob::TestFeaturesJob( const ScriptData &scriptData, const QString &useCase,
                                  DebugFlags debugFlags, QObject *parent )
        : CallScriptFunctionJob(scriptData, ServiceProviderScript::SCRIPT_FUNCTION_FEATURES,
                                QVariantList(), useCase, debugFlags, parent)
{
}

TimetableDataRequestJob::TimetableDataRequestJob( const ScriptData &scriptData,
        const AbstractRequest *request, const QString &useCase, DebugFlags debugFlags,
        QObject *parent )
        : CallScriptFunctionJob(scriptData, useCase, debugFlags, parent), m_request(request->clone())
{
    QMutexLocker locker( m_mutex );
    switch ( request->parseMode() ) {
    case ParseForDepartures:
    case ParseForArrivals:
        m_functionName = ServiceProviderScript::SCRIPT_FUNCTION_GETTIMETABLE;
        break;
    case ParseForAdditionalData:
        m_functionName = ServiceProviderScript::SCRIPT_FUNCTION_GETADDITIONALDATA;
        break;
    case ParseForJourneysByDepartureTime:
    case ParseForJourneysByArrivalTime:
        m_functionName = ServiceProviderScript::SCRIPT_FUNCTION_GETJOURNEYS;
        break;
    case ParseForStopSuggestions:
        m_functionName = ServiceProviderScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;
        break;
    default:
        // This should never happen, therefore no i18n
        m_explanation = "Unknown parse mode";
        m_success = false;
        return;
    }
}

QScriptValueList CallScriptFunctionJob::createArgumentScriptValues( DebuggerAgent *debugger ) const
{
    // m_engineSemaphore is locked
    QMutexLocker locker( m_mutex );
    if ( !debugger ) {
        debugger = m_agent;
    }

    QScriptValueList args;
    QScriptEngine *engine = debugger->engine();
    foreach ( const QVariant &argument, m_arguments ) {
        args << engine->toScriptValue( argument );
    }
    return args;
}

QScriptValueList TimetableDataRequestJob::createArgumentScriptValues( DebuggerAgent *debugger ) const
{
    // m_engineSemaphore is locked
    QMutexLocker locker( m_mutex );
    if ( !debugger ) {
        debugger = m_agent;
    }
    return QScriptValueList() << m_request->toScriptValue( debugger->engine() );
}

void TimetableDataRequestJob::finish( const QList<TimetableData> &data )
{
    QMutexLocker locker( m_mutex );
    m_timetableData = data;
}

CallScriptFunctionJob::~CallScriptFunctionJob()
{
}

TimetableDataRequestJob::~TimetableDataRequestJob()
{
    QMutexLocker locker( m_mutex );
    delete m_request;
}

QVariant CallScriptFunctionJob::returnValue() const
{
    QMutexLocker locker( m_mutex );
    return m_returnValue;
}

QList< Enums::ProviderFeature > TestFeaturesJob::features() const
{
    QMutexLocker locker( m_mutex );
    return m_features;
}

QList<TimetableData> TimetableDataRequestJob::timetableData() const
{
    QMutexLocker locker( m_mutex );
    return m_timetableData;
}

QSharedPointer< AbstractRequest > TimetableDataRequestJob::request() const
{
    QMutexLocker locker( m_mutex );
    return QSharedPointer< AbstractRequest >( m_request->clone() );
}

QList< TimetableDataRequestMessage > CallScriptFunctionJob::additionalMessages() const
{
    QMutexLocker locker( m_mutex );
    return m_additionalMessages;
}

QString CallScriptFunctionJob::functionName() const
{
    QMutexLocker locker( m_mutex );
    return m_functionName;
}

DebugFlags CallScriptFunctionJob::debugFlags() const
{
    QMutexLocker locker( m_mutex );
    return m_debugFlags;
}

QString CallScriptFunctionJob::toString() const
{
    QMutexLocker locker( m_mutex );
    if ( m_returnValue.isValid() ) {
        QString valueString = m_returnValue.toString();
        if ( valueString.length() > 100 ) {
            valueString = valueString.left(100) + "...";
        } else if ( valueString.isEmpty() ) {
            valueString = "undefined";
        }
        return QString("%1 (%2() = %3)").arg( DebuggerJob::toString() )
                .arg( m_functionName ).arg( valueString );
    } else {
        return QString("%1 (%2())").arg( DebuggerJob::toString() ).arg( m_functionName );
    }
}

QString CallScriptFunctionJob::defaultUseCase() const
{
    QMutexLocker locker( m_mutex );
    return i18nc("@info", "Call function <icode>%1()</icode>", m_functionName);
}

QString TimetableDataRequestJob::defaultUseCase() const
{
    QMutexLocker locker( m_mutex );
    return i18nc("@info", "Call function <icode>%1( %2 )</icode>",
                 m_functionName, m_request->argumentsString());
}

bool TestFeaturesJob::testResults()
{
    QMutexLocker locker( m_mutex );
    if ( !m_returnValue.isValid() ) {
        return false;
    }

    const QVariantList items = m_returnValue.toList();
    if ( items.isEmpty() ) {
        m_additionalMessages << TimetableDataRequestMessage(
                i18nc("@info/plain", "No provider features returned"),
                TimetableDataRequestMessage::Warning );
    } else {
        int i = 1;
        foreach ( const QVariant &item, items ) {
            const Enums::ProviderFeature feature =
                    static_cast< Enums::ProviderFeature >( item.toInt() );
            if ( feature == Enums::InvalidProviderFeature ) {
                m_additionalMessages << TimetableDataRequestMessage(
                        i18nc("@info/plain", "Invalid ProviderFeature: '%1'",
                              item.toString()),
                        TimetableDataRequestMessage::Error );
            } else {
                m_features << feature;
                m_additionalMessages << TimetableDataRequestMessage(
                        QString("%1: %2").arg(i).arg(Enums::toString(feature)),
                        TimetableDataRequestMessage::Information );
            }
            ++i;
        }
    }

    return true;
}

void CallScriptFunctionJob::connectScriptObjects( bool doConnect )
{
    DebuggerJob::connectScriptObjects( doConnect );

    QMutexLocker locker( m_mutex );
    if ( doConnect ) {
        // Connect to request finished signals to store the time spent for network requests
        connect( m_objects.network.data(), SIGNAL(requestFinished(NetworkRequest::Ptr,QByteArray,bool,QString,QDateTime,int,int)),
                 this, SLOT(requestFinished(NetworkRequest::Ptr,QByteArray,bool,QString,QDateTime,int,int)) );
        connect( m_objects.network.data(), SIGNAL(synchronousRequestFinished(QString,QByteArray,bool,int,int,int)),
                 this, SLOT(synchronousRequestFinished(QString,QByteArray,bool,int,int,int)) );

        // Connect to the messageReceived() signal of the "helper" script object to collect
        // messages sent to "helper.error()", "helper.warning()" or "helper.information()"
        connect( m_objects.helper.data(), SIGNAL(messageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)),
                 this, SLOT(slotScriptMessageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)) );

        connect( m_objects.result.data(), SIGNAL(invalidDataReceived(Enums::TimetableInformation,QString,QScriptContextInfo,int,QVariantMap)),
                 this, SLOT(invalidDataReceived(Enums::TimetableInformation,QString,QScriptContextInfo,int,QVariantMap)) );

        // Connect to the stopped() signal of the agent to get notified when execution gets aborted
        connect( m_agent, SIGNAL(stopped(QDateTime,bool,bool,int,QString,QStringList)),
                 this, SLOT(scriptStopped(QDateTime,bool,bool,int,QString,QStringList)) );
    } else {
        disconnect( m_objects.network.data(), SIGNAL(requestFinished(NetworkRequest::Ptr,QByteArray,bool,QString,QDateTime,int,int)),
                    this, SLOT(requestFinished(NetworkRequest::Ptr,QByteArray,bool,QString,QDateTime,int,int)) );
        disconnect( m_objects.network.data(), SIGNAL(synchronousRequestFinished(QString,QByteArray,bool,int,int,int)),
                    this, SLOT(synchronousRequestFinished(QString,QByteArray,bool,int,int,int)) );
        disconnect( m_objects.helper.data(), SIGNAL(messageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)),
                    this, SLOT(slotScriptMessageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)) );
        disconnect( m_objects.result.data(), SIGNAL(invalidDataReceived(Enums::TimetableInformation,QString,QScriptContextInfo,int,QVariantMap)),
                    this, SLOT(invalidDataReceived(Enums::TimetableInformation,QString,QScriptContextInfo,int,QVariantMap)) );
        disconnect( m_agent, SIGNAL(stopped(QDateTime,bool,bool,int,QString,QStringList)),
                    this, SLOT(scriptStopped(QDateTime,bool,bool,int,QString,QStringList)) );
    }
}

void CallScriptFunctionJob::debuggerRun()
{
    m_mutex->lock();
    if ( !m_success ) {
        // Job already marked as not successful, a derived class may have set it to false
        m_mutex->unlock();
        return;
    }
    m_mutex->unlock();

    // Create new engine and agent
    m_engineSemaphore->acquire();
    DebuggerAgent *agent = createAgent();
    if ( !agent ) {
        m_engineSemaphore->release();
        return;
    }
    QScriptEngine *engine = agent->engine();
    m_mutex->lock();
    ScriptObjects objects = m_objects;
    const ScriptData data = m_data;
    const QString functionName = m_functionName;
    const DebugFlags debugFlags = m_debugFlags;
    m_mutex->unlock();

    // Load script
    engine->evaluate( data.program );
    Q_ASSERT_X( !objects.network->hasRunningRequests() || !agent->engine()->isEvaluating(),
                "LoadScriptJob::debuggerRun()",
                "Evaluating the script should not start any asynchronous requests, bad script" );

    QScriptValue function = engine->globalObject().property( functionName );
    if ( !function.isFunction() ) {
        QMutexLocker locker( m_mutex );
        destroyAgent();
        m_engineSemaphore->release();

        m_explanation = i18nc("@info/plain", "Did not find a '%1' function in the script.",
                               functionName);
        m_success = false;
        return;
    }

    connectScriptObjects();
    agent->setExecutionControlType( debugFlags.testFlag(InterruptAtStart)
                                       ? ExecuteInterrupt : ExecuteRun );
    agent->setDebugFlags( debugFlags );

    // Make this job responsive while running the script
    engine->setProcessEventsInterval( 50 );

    // Call script function while the engine mutex is still locked
    const QScriptValueList arguments = createArgumentScriptValues( agent );
    const QVariant returnValue = function.call( QScriptValue(), arguments ).toVariant();

    // The called function returned, but asynchronous network requests may have been started.
    // Wait for all network requests to finish, because slots in the script may get called
    if ( !waitFor(objects.network.data(), SIGNAL(allRequestsFinished()), WaitForNetwork) ) {
        m_engineSemaphore->release();
        return;
    }

    // Wait for script execution to finish
    if ( !waitFor(this, SIGNAL(stopped(QDateTime,ScriptStoppedFlags,int,QString,QStringList)),
                  WaitForScriptFinish) )
    {
        kWarning() << "Stopped signal not received";
        m_engineSemaphore->release();
        return;
    }
    qApp->processEvents();

    const bool allNetworkRequestsFinished = !objects.network->hasRunningRequests();
    const bool finishedSuccessfully = !agent->wasLastRunAborted();

//     GlobalTimetableInfo globalInfo;
//     globalInfo.requestDate = QDate::currentDate();
//     globalInfo.delayInfoAvailable =
//             !m_scriptResult->isHintGiven( ResultObject::NoDelaysForStop );

    // Check for exceptions
    if ( finishedSuccessfully && agent->hasUncaughtException() ) {
        const QString uncaughtException = agent->uncaughtException().toString();
        handleError( engine, i18nc("@info/plain", "Error in the script at line %1"
                     "in function '%2': <message>%3</message>.",
                     agent->uncaughtExceptionLineNumber(), functionName, uncaughtException) );
        m_mutex->lockInline();
        destroyAgent();
        m_mutex->unlockInline();
        m_engineSemaphore->release();
        return;
    }

    // Unlock engine mutex after execution was finished
    m_engineSemaphore->release();

    m_mutex->lockInline();
    m_returnValue = returnValue;
    m_aborted = agent->wasLastRunAborted();
    bool quit = m_quit;
    m_mutex->unlockInline();

    if ( !quit ) {
        if ( functionName == ServiceProviderScript::SCRIPT_FUNCTION_GETADDITIONALDATA ) {
            const QVariantMap map = returnValue.toMap();
            TimetableData data;
            for ( QVariantMap::ConstIterator it = map.constBegin(); it != map.constEnd(); ++it ) {
                data[ Global::timetableInformationFromString(it.key()) ] = it.value();
            }
            if ( !data.isEmpty() ) {
                finish( QList<TimetableData>() << data );
            }
        } else {
            finish( objects.result->data() );
        }
    }

    // Process signals from the debugger before it gets deleted
    qApp->processEvents();

    QMutexLocker locker( m_mutex );
    m_engineSemaphore->acquire();
    destroyAgent();
    m_engineSemaphore->release();

    if ( !m_success || m_quit ) {
        m_success = false;
    } else if ( allNetworkRequestsFinished && finishedSuccessfully ) {
        // No uncaught exceptions, all network requests finished
        if ( m_aborted ) {
            m_success = false;
            m_explanation = i18nc("@info/plain", "Execution was aborted");
        } else {
            m_success = testResults();
        }
    } else if ( finishedSuccessfully ) {
        // The script finish successfully, but not all network requests finished
        m_explanation = i18nc("@info/plain", "Not all network requests were finished in time");
        m_success = false;
    } else if ( m_aborted ) {
        // Script was aborted
        m_explanation = i18nc("@info/plain", "Aborted");
        m_success = false;
    } else {
        // The script did not finish successfully,
        // ignore if not all network requests were finished here
        m_explanation = i18nc("@info/plain", "The script did not finish in time, "
                               "there may be an infinite loop.");
        m_success = false;
    }

}

void CallScriptFunctionJob::scriptStopped( const QDateTime &timestamp, bool aborted,
                                           bool hasRunningRequests, int uncaughtExceptionLineNumber,
                                           const QString &uncaughtException,
                                           const QStringList &backtrace )
{
    Q_UNUSED( timestamp );
    Q_UNUSED( hasRunningRequests );
    if ( aborted ) {
        handleError( uncaughtExceptionLineNumber, uncaughtException, backtrace,
                     i18nc("@info/plain", "Aborted") );
    }
}

void CallScriptFunctionJob::slotScriptMessageReceived( const QString &message,
                                                     const QScriptContextInfo &context,
                                                     const QString &failedParseText,
                                                     Helper::ErrorSeverity severity )
{
    Q_UNUSED( failedParseText );
    TimetableDataRequestMessage::Type type;
    switch ( severity ) {
    case Helper::Warning:
        type = TimetableDataRequestMessage::Warning;
        break;
    case Helper::Fatal:
        type = TimetableDataRequestMessage::Error;
        break;
    default:
    case Helper::Information:
        type = TimetableDataRequestMessage::Information;
        break;
    }

    QMutexLocker locker( m_mutex );
    const TimetableDataRequestMessage newMessage(
            i18nc("@info/plain", "Error in file <filename>%1</filename>, line %2: "
                  "<message>%3</message>",
                  QFileInfo(context.fileName()).fileName(), context.lineNumber(), message),
            type, context.fileName(), context.lineNumber() );
    if ( !m_additionalMessages.isEmpty() && m_additionalMessages.last() == newMessage ) {
        ++m_additionalMessages.last().repetitions;
    } else {
        m_additionalMessages << newMessage;
    }
}

void CallScriptFunctionJob::invalidDataReceived( Enums::TimetableInformation information,
                                                 const QString &message,
                                                 const QScriptContextInfo &context,
                                                 int index, const QVariantMap& map )
{
    Q_UNUSED( information );
    Q_UNUSED( map );

    QMutexLocker locker( m_mutex );
    m_additionalMessages << TimetableDataRequestMessage(
            i18nc("@info/plain", "Invalid data in result %1, line %2: <message>%3</message>",
                  index + 1, context.lineNumber(), message),
            TimetableDataRequestMessage::Error, context.fileName(), context.lineNumber() );
}

void CallScriptFunctionJob::requestFinished( const NetworkRequest::Ptr &request,
                                             const QByteArray &data,
                                             bool error, const QString &errorString,
                                             const QDateTime &timestamp, int statusCode, int size )
{
    Q_UNUSED( data );
    emit asynchronousRequestWaitFinished( timestamp, statusCode, size );

    QMutexLocker locker( m_mutex );
    if ( error ) {
        m_additionalMessages << TimetableDataRequestMessage(
                i18nc("@info/plain", "Download failed (<message>%1</message>): <link>%2</link>",
                      errorString, request->url()),
                TimetableDataRequestMessage::Information, QString(), -1,
                TimetableDataRequestMessage::OpenLink, request->url() );
    } else {
        m_additionalMessages << TimetableDataRequestMessage(
                i18nc("@info/plain", "Download finished (status %1): %2, <link>%3</link>",
                      statusCode, KGlobal::locale()->formatByteSize(size), request->url()),
                TimetableDataRequestMessage::Information, QString(), -1,
                TimetableDataRequestMessage::OpenLink, request->url() );
    }
}

void CallScriptFunctionJob::synchronousRequestFinished( const QString &url, const QByteArray &data,
                                                        bool cancelled, int statusCode,
                                                        int waitingTime, int size )
{
    Q_UNUSED(data);
    emit synchronousRequestWaitFinished( statusCode, waitingTime, size );

    QMutexLocker locker( m_mutex );
    if ( cancelled ) {
        m_additionalMessages << TimetableDataRequestMessage(
                i18nc("@info/plain", "Download cancelled/failed (status %1): <link>%2</link>",
                      statusCode, url),
                TimetableDataRequestMessage::Warning, QString(), -1,
                TimetableDataRequestMessage::OpenLink, url );
    } else {
        m_additionalMessages << TimetableDataRequestMessage(
                i18nc("@info/plain", "Download finished (status %1): %2, <link>%3</link>",
                      statusCode, KGlobal::locale()->formatByteSize(size), url),
                TimetableDataRequestMessage::Information, QString(), -1,
                TimetableDataRequestMessage::OpenLink, url );
    }
}

bool TimetableDataRequestJob::testResults()
{
    QMutexLocker locker( m_mutex );
    const AbstractRequest *request = m_request;
    const DepartureRequest *departureRequest =
            dynamic_cast< const DepartureRequest* >( request );
    if ( departureRequest ) {
        return testDepartureData( departureRequest );
    }

    const StopSuggestionRequest *stopSuggestionRequest =
            dynamic_cast< const StopSuggestionRequest* >( request );
    if ( stopSuggestionRequest ) {
        return testStopSuggestionData( stopSuggestionRequest );
    }

    const StopsByGeoPositionRequest *stopsByGeoPositionRequest =
            dynamic_cast< const StopsByGeoPositionRequest* >( request );
    if ( stopsByGeoPositionRequest ) {
        return testStopSuggestionData( stopsByGeoPositionRequest );
    }

    const JourneyRequest *journeyRequest =
            dynamic_cast< const JourneyRequest* >( request );
    if ( journeyRequest ) {
        return testJourneyData( journeyRequest );
    }

    const AdditionalDataRequest *additinalDataRequest =
            dynamic_cast< const AdditionalDataRequest* >( request );
    if ( additinalDataRequest ) {
        return testAdditionalData( additinalDataRequest );
    }

    return false;
}

TimetableDataRequestMessage CallScriptFunctionJob::message( MessageType messageType,
        Enums::TimetableInformation info1, Enums::TimetableInformation info2, int count1, int count2,
        TimetableDataRequestMessage::Type type, const QString &fileName, int lineNumber )
{
    QString msg;
    switch ( messageType ) {
    case NotSameNumberOfItems:
        msg = i18nc("@info/plain", "'%1' should contain the same number of elements like '%3'. "
                    "Found %2 values for '%1' and %4 values for '%3'",
                    Global::timetableInformationToString(info1), count1,
                    Global::timetableInformationToString(info2), count2);
        break;
    case NotOneItemLessThan:
        msg = i18nc("@info/plain", "'%1' should contain one element less than '%3'. "
                    "Found %2 values for '%1' and %4 values for '%3'",
                    Global::timetableInformationToString(info1), count1,
                    Global::timetableInformationToString(info2), count2);
        break;
    default:
        kWarning() << "Unknown message type" << messageType;
        break;
    }

    return TimetableDataRequestMessage( msg, type, fileName, lineNumber );
}

bool TimetableDataRequestJob::testAdditionalData( const AdditionalDataRequest *request )
{
    Q_UNUSED(request);

    QMutexLocker locker( m_mutex );
    if ( m_timetableData.isEmpty() ) {
        m_explanation = i18nc("@info/plain", "No additional data found");
        return false;
    }

    const TimetableData timetableData = m_timetableData.first();
    for ( TimetableData::ConstIterator it = timetableData.constBegin();
          it != timetableData.constEnd(); ++it )
    {
        const bool isValid = Global::checkTimetableInformation( it.key(), it.value() );
        QString info;
        if ( it->canConvert(QVariant::List) ) {
            const QVariantList list = it->toList();
            QStringList elements;
            foreach ( const QVariant &item, list ) {
                elements << item.toString();
            }
            info = "[" + elements.join(", ") + "]";
        } else {
            info = it->toString();
        }
        m_additionalMessages << TimetableDataRequestMessage(
                QString("%1%2: %3")
                .arg(Global::timetableInformationToString(it.key()))
                .arg(isValid ? QString() : (' ' + i18nc("@info/plain", "(invalid)")))
                .arg(info),
                isValid ? TimetableDataRequestMessage::Information
                        : TimetableDataRequestMessage::Warning );
    }
    return true;
}

bool TimetableDataRequestJob::testDepartureData( const DepartureRequest *request )
{
    QMutexLocker locker( m_mutex );
    if ( m_timetableData.isEmpty() ) {
        if ( request->parseMode() == ParseForArrivals ) {
            m_explanation = i18nc("@info/plain", "No arrivals found");
        } else {
            m_explanation = i18nc("@info/plain", "No departures found");
        }
        return false;
    }

    // Get global information
    QStringList globalInfos;
    if ( m_returnValue.isValid() ) {
        globalInfos = m_returnValue.toStringList();
    }

    // Get result set
    int countInvalid = 0;
    QDate curDate;
    QTime lastTime;
    for ( int i = 0; i < m_timetableData.count(); ++ i ) {
        TimetableData timetableData = m_timetableData[ i ];
        QDateTime departureDateTime = timetableData.value( Enums::DepartureDateTime ).toDateTime();
        if ( !departureDateTime.isValid() ) {
            QDate date = timetableData.value( Enums::DepartureDate ).toDate();
            QTime departureTime;
            if ( timetableData.values().contains(departureTime) ) {
                QVariant timeValue = timetableData.value( Enums::DepartureTime );
                if ( timeValue.canConvert(QVariant::Time) ) {
                    departureTime = timeValue.toTime();
                } else {
                    departureTime = QTime::fromString( timeValue.toString(), "hh:mm:ss" );
                    if ( !departureTime.isValid() ) {
                        departureTime = QTime::fromString( timeValue.toString(), "hh:mm" );
                    }
                }
            }
            if ( !date.isValid() ) {
                if ( curDate.isNull() ) {
                    // First departure
                    if ( QTime::currentTime().hour() < 3 && departureTime.hour() > 21 )
                        date = QDate::currentDate().addDays( -1 );
                    else if ( QTime::currentTime().hour() > 21 && departureTime.hour() < 3 )
                        date = QDate::currentDate().addDays( 1 );
                    else
                        date = QDate::currentDate();
                } else if ( lastTime.secsTo(departureTime) < -5 * 60 ) {
                    // Time too much ealier than last time, estimate it's tomorrow
                    date = curDate.addDays( 1 );
                } else {
                    date = curDate;
                }
            }

            departureDateTime = QDateTime( date, departureTime );
            timetableData.insert( Enums::DepartureDateTime, departureDateTime );
        }

        curDate = departureDateTime.date();
        lastTime = departureDateTime.time();

        const bool isValid = timetableData.contains(Enums::TransportLine) &&
                             timetableData.contains(Enums::Target) &&
                             timetableData.contains(Enums::DepartureDateTime);
        if ( !isValid ) {
            ++countInvalid;
            m_additionalMessages << TimetableDataRequestMessage(
                    i18nc("@info/plain", "Data missing in result %1, required are TransportLine, "
                          "Target and at least a departure time (better also a date)", i),
                    TimetableDataRequestMessage::Error );
        }

        m_additionalMessages << TimetableDataRequestMessage(
                QString("%1: %2 (%3, %4), %5").arg(i + 1)
                .arg(timetableData[Enums::TransportLine].toString())
                .arg(Enums::toString(static_cast<Enums::VehicleType>(timetableData[Enums::TypeOfVehicle].toInt())))
                .arg(timetableData[Enums::DepartureDateTime].toDateTime().time().toString(Qt::DefaultLocaleShortDate))
                .arg(timetableData[Enums::Target].toString()),
                isValid ? TimetableDataRequestMessage::Information
                        : TimetableDataRequestMessage::Warning );
    }

    bool success = true;
    for ( int i = 0; i < m_timetableData.count(); ++i ) {
        TimetableData values = m_timetableData[i];

        // If RouteStops data is available test it and associated TimetableInformation values
        if ( values.contains(Enums::RouteStops) && !values[Enums::RouteStops].toStringList().isEmpty() ) {
            // Check if RouteTimesDeparture has one element less than RouteStops
            // and if RouteTimesDepartureDelay has the same number of elements as RouteStops (if set)
            const QStringList routeStops = values[Enums::RouteStops].toStringList();
            if ( values.contains(Enums::RouteTimes) && !values[Enums::RouteTimes].toStringList().isEmpty() ) {
                const QStringList routeTimes = values[Enums::RouteTimes].toStringList();
                if ( routeTimes.count() != routeStops.count() ) {
                    m_additionalMessages << message( NotSameNumberOfItems, Enums::RouteTimes, Enums::RouteStops,
                                                     routeTimes.count(), routeStops.count() );
                    success = false;
                }

                if ( values.contains(Enums::RouteTimesDepartureDelay) &&
                     !values[Enums::RouteTimesDepartureDelay].toStringList().isEmpty() )
                {
                    const QStringList routeTimesDepartureDelay =
                        values[Enums::RouteTimesDepartureDelay].toStringList();
                    if ( routeTimesDepartureDelay.count() != routeTimes.count() ) {
                        m_additionalMessages << message( NotSameNumberOfItems,
                                Enums::RouteTimesDepartureDelay, Enums::RouteTimes,
                                routeTimesDepartureDelay.count(), routeTimes.count() );
                        success = false;
                    }
                }
            }

            if ( values.contains(Enums::RouteTypesOfVehicles) &&
                 !values[Enums::RouteTypesOfVehicles].toList().isEmpty() )
            {
                const QVariantList routeTypesOfVehicles = values[Enums::RouteTypesOfVehicles].toList();
                if ( routeTypesOfVehicles.count() != routeStops.count() - 1 ) {
                    m_additionalMessages << message( NotOneItemLessThan, Enums::RouteTypesOfVehicles,
                            Enums::RouteStops, routeTypesOfVehicles.count(), routeStops.count() );
                    success = false;
                }
            }

            // Check if RoutePlatformsDeparture has one element less than RouteStops
            if ( values.contains(Enums::RoutePlatformsDeparture) &&
                 !values[Enums::RoutePlatformsDeparture].toStringList().isEmpty() )
            {
                const QStringList routePlatformsDeparture =
                        values[Enums::RoutePlatformsDeparture].toStringList();
                if ( routePlatformsDeparture.count() != routeStops.count() - 1 ) {
                    m_additionalMessages << message( NotOneItemLessThan, Enums::RoutePlatformsDeparture,
                            Enums::RouteStops, routePlatformsDeparture.count(), routeStops.count() );
                    success = false;
                }
            }

            // Check if RoutePlatformsArrival has one element less than RouteStops
            if ( values.contains(Enums::RoutePlatformsArrival) &&
                 !values[Enums::RoutePlatformsArrival].toStringList().isEmpty() )
            {
                const QStringList routePlatformsArrival =
                        values[Enums::RoutePlatformsArrival].toStringList();
                if ( routePlatformsArrival.count() != routeStops.count() - 1 ) {
                    m_additionalMessages << message( NotOneItemLessThan, Enums::RoutePlatformsArrival,
                            Enums::RouteStops, routePlatformsArrival.count(), routeStops.count() );
                    success = false;
                }
            }
        } else { // values does not contain RouteStops
            QList< Enums::TimetableInformation > infos;
            infos << Enums::RouteTimes << Enums::RoutePlatformsDeparture << Enums::RoutePlatformsArrival
                  << Enums::RouteExactStops << Enums::RouteStopsShortened << Enums::RouteTypesOfVehicles
                  << Enums::RouteTransportLines;
            foreach ( Enums::TimetableInformation info, infos ) {
                if ( values.contains(info) ) {
                    m_additionalMessages << TimetableDataRequestMessage( i18nc("@info/plain",
                            "'%1' data ignored, because data for 'RouteStops' is missing.",
                            Global::timetableInformationToString(info)),
                            TimetableDataRequestMessage::Warning );
                    success = false;
                }
            }
        }
    }

    // Show results
    if ( request->parseMode() == ParseForArrivals ) {
        m_explanation = i18ncp("@info/plain", "Got %1 arrival",
                                              "Got %1 arrivals", m_timetableData.count());
    } else {
        m_explanation = i18ncp("@info/plain", "Got %1 departure",
                                              "Got %1 departures", m_timetableData.count());
    }

    if ( countInvalid > 0 ) {
        if ( request->parseMode() == ParseForArrivals ) {
            m_explanation += ", " +
                    i18ncp("@info/plain", "<warning>%1 arrival is invalid</warning>",
                           "<warning>%1 arrivals are invalid</warning>",
                           countInvalid);
        } else {
            m_explanation += ", " +
                    i18ncp("@info/plain", "<warning>%1 departure is invalid</warning>",
                           "<warning>%1 departures are invalid</warning>",
                           countInvalid);
        }
        return false;
    }
    if ( globalInfos.contains("no delays", Qt::CaseInsensitive) ) {
        // No delay information available for the given stop
        m_explanation += ", " +
                i18nc("@info/plain", "Got the information from the script that there "
                      "is no delay information available for the given stop.");
    }

    return success;
}

bool TimetableDataRequestJob::testStopSuggestionData(
        const StopSuggestionRequest *request )
{
    Q_UNUSED( request );
    QMutexLocker locker( m_mutex );
    if ( m_timetableData.isEmpty() ) {
        m_explanation = i18nc("@info/plain", "No stop suggestions found");
        return false;
    }

    // Get global information
    QStringList globalInfos;
    if ( m_returnValue.isValid() ) {
        globalInfos = m_returnValue.toStringList();
    }

    // Test timetable data
    int countInvalid = 0;
    int i = 1;
    foreach ( const TimetableData &timetableData, m_timetableData ) {
        const bool isValid = !timetableData[Enums::StopName].toString().isEmpty();
        if ( !isValid ) {
            ++countInvalid;
        }

        QString info = timetableData[Enums::StopName].toString();
        if ( info.length() > 50 ) {
            info = info.left(50) + "...";
        }
        if ( timetableData.contains(Enums::StopID) ) {
            info += ", ID: " + timetableData[Enums::StopID].toString();
        }
        if ( timetableData.contains(Enums::StopWeight) ) {
            info += ", weight: " + timetableData[Enums::StopWeight].toString();
        }
        if ( timetableData.contains(Enums::StopCity) ) {
            info += ", city: " + timetableData[Enums::StopCity].toString();
        }
        if ( timetableData.contains(Enums::StopLongitude) ) {
            info += ", longitude: " + timetableData[Enums::StopLongitude].toString();
        }
        if ( timetableData.contains(Enums::StopLatitude) ) {
            info += ", latitude: " + timetableData[Enums::StopLatitude].toString();
        }
        m_additionalMessages << TimetableDataRequestMessage(
                QString("%1%2: %3").arg(i)
                .arg(isValid ? QString() : (' ' + i18nc("@info/plain", "(invalid)")))
                .arg(info),
                isValid ? TimetableDataRequestMessage::Information
                        : TimetableDataRequestMessage::Warning );
        ++i;
    }

    // Show results
    m_explanation = i18ncp("@info/plain", "Got %1 stop suggestion",
                                          "Got %1 stop suggestions", m_timetableData.count());
    if ( countInvalid > 0 ) {
        m_explanation += ", " +
                i18ncp("@info/plain", "<warning>%1 stop suggestion is invalid</warning>",
                                      "<warning>%1 stop suggestions are invalid</warning>",
                                      countInvalid);
        return false;
    }

    return true;
}

bool TimetableDataRequestJob::testJourneyData( const JourneyRequest *request )
{
    Q_UNUSED( request );
    QMutexLocker locker( m_mutex );
    if ( m_timetableData.isEmpty() ) {
        m_explanation = i18nc("@info/plain", "No journeys found");
        return false;
    }

    // Get global information
    QStringList globalInfos;
    if ( m_returnValue.isValid() ) {
        globalInfos = m_returnValue.toStringList();
    }

    m_explanation = i18ncp("@info/plain", "Got %1 journey",
                                          "Got %1 journeys", m_timetableData.count());

    // Get result set
    int countInvalid = 0;
    QDate curDate;
    QTime lastTime;
    for ( int i = 0; i < m_timetableData.count(); ++ i ) {
        TimetableData timetableData = m_timetableData[ i ];
        QDateTime departureDateTime = timetableData.value( Enums::DepartureDateTime ).toDateTime();
        if ( !departureDateTime.isValid() ) {
            QDate date = timetableData.value( Enums::DepartureDate ).toDate();
            QTime departureTime;
            if ( timetableData.values().contains(Enums::DepartureTime) ) {
                QVariant timeValue = timetableData[ Enums::DepartureTime ];
                if ( timeValue.canConvert(QVariant::Time) ) {
                    departureTime = timeValue.toTime();
                } else {
                    departureTime = QTime::fromString( timeValue.toString(), "hh:mm:ss" );
                    if ( !departureTime.isValid() ) {
                        departureTime = QTime::fromString( timeValue.toString(), "hh:mm" );
                    }
                }
            }
            if ( !date.isValid() ) {
                if ( curDate.isNull() ) {
                    // First departure
                    if ( QTime::currentTime().hour() < 3 && departureTime.hour() > 21 )
                        date = QDate::currentDate().addDays( -1 );
                    else if ( QTime::currentTime().hour() > 21 && departureTime.hour() < 3 )
                        date = QDate::currentDate().addDays( 1 );
                    else
                        date = QDate::currentDate();
                } else if ( lastTime.secsTo(departureTime) < -5 * 60 ) {
                    // Time too much ealier than last time, estimate it's tomorrow
                    date = curDate.addDays( 1 );
                } else {
                    date = curDate;
                }
            }

            departureDateTime = QDateTime( date, departureTime );
            timetableData.insert( Enums::DepartureDateTime, departureDateTime );
        }

        QDateTime arrivalDateTime = timetableData[ Enums::ArrivalDateTime ].toDateTime();
        if ( !departureDateTime.isValid() ) {
            QDate date = timetableData[ Enums::ArrivalDate ].toDate();
            QTime arrivalTime;
            if ( timetableData.contains(Enums::ArrivalTime) ) {
                QVariant timeValue = timetableData[ Enums::ArrivalTime ];
                if ( timeValue.canConvert(QVariant::Time) ) {
                    arrivalTime = timeValue.toTime();
                } else {
                    arrivalTime = QTime::fromString( timeValue.toString(), "hh:mm:ss" );
                    if ( !arrivalTime.isValid() ) {
                        arrivalTime = QTime::fromString( timeValue.toString(), "hh:mm" );
                    }
                }
            }
            if ( !date.isValid() ) {
                date = departureDateTime.date();
            }

            arrivalDateTime = QDateTime( date, arrivalTime );
            if ( arrivalDateTime < departureDateTime ) {
                arrivalDateTime = arrivalDateTime.addDays( 1 );
            }
            timetableData.insert( Enums::ArrivalDateTime, arrivalDateTime );
        }

        curDate = departureDateTime.date();
        lastTime = departureDateTime.time();

        bool isValid = timetableData.contains(Enums::StartStopName) &&
                       timetableData.contains(Enums::TargetStopName) &&
                       timetableData.contains(Enums::DepartureDateTime) &&
                       timetableData.contains(Enums::ArrivalDateTime);
        if ( !isValid ) {
            ++countInvalid;
        }

        m_additionalMessages << TimetableDataRequestMessage(
                QString("%1: %2 (%3) - %4 (%5), %6 route stops").arg(i + 1)
                .arg(timetableData[Enums::StartStopName].toString())
                .arg(timetableData[Enums::DepartureDateTime].toDateTime().time().toString(Qt::DefaultLocaleShortDate))
                .arg(timetableData[Enums::TargetStopName].toString())
                .arg(timetableData[Enums::ArrivalDateTime].toDateTime().time().toString(Qt::DefaultLocaleShortDate))
                .arg(timetableData[Enums::RouteStops].toStringList().count()),
                isValid ? TimetableDataRequestMessage::Information
                        : TimetableDataRequestMessage::Error );
    }

    bool success = true;
    for ( int i = 0; i < m_timetableData.count(); ++i ) {
        TimetableData values = m_timetableData[i];
        if ( values.contains(Enums::RouteStops) && !values[Enums::RouteStops].toStringList().isEmpty() ) {
            // Check if RouteTimesDeparture has one element less than RouteStops
            // and if RouteTimesDepartureDelay has the same number of elements as RouteStops (if set)
            const QStringList routeStops = values[Enums::RouteStops].toStringList();
            if ( values.contains(Enums::RouteTimesDeparture) &&
                 !values[Enums::RouteTimesDeparture].toStringList().isEmpty() )
            {
                const QStringList routeTimesDeparture = values[Enums::RouteTimesDeparture].toStringList();
                if ( routeTimesDeparture.count() != routeStops.count() - 1 ) {
                    m_additionalMessages << message( NotOneItemLessThan, Enums::RouteTimesDeparture,
                            Enums::RouteStops, routeTimesDeparture.count(), routeStops.count() );
                    success = false;
                }

                if ( values.contains(Enums::RouteTimesDepartureDelay) &&
                     !values[Enums::RouteTimesDepartureDelay].toStringList().isEmpty() )
                {
                    const QStringList routeTimesDepartureDelay =
                        values[Enums::RouteTimesDepartureDelay].toStringList();
                    if ( routeTimesDepartureDelay.count() != routeTimesDeparture.count() ) {
                        m_additionalMessages << message( NotSameNumberOfItems,
                                Enums::RouteTimesDepartureDelay, Enums::RouteTimesDeparture,
                                routeTimesDepartureDelay.count(), routeTimesDeparture.count() );
                        success = false;
                    }
                }
            }

            // Check if RoutePlatformsDeparture has one element less than RouteStops
            if ( values.contains(Enums::RoutePlatformsDeparture)
                    && !values[Enums::RoutePlatformsDeparture].toStringList().isEmpty() )
            {
                const QStringList routePlatformsArrival =
                        values[Enums::RoutePlatformsArrival].toStringList();
                if ( routePlatformsArrival.count() != routeStops.count() - 1 ) {
                    m_additionalMessages << message( NotOneItemLessThan, Enums::RoutePlatformsArrival,
                            Enums::RouteStops, routePlatformsArrival.count(), routeStops.count() );
                    success = false;
                }
            }

            // Check if RouteTimesArrival has one element less than RouteStops
            // and if RouteTimesArrivalDelay has the same number of elements as RouteStops (if set)
            if ( values.contains(Enums::RouteTimesArrival)
                    && !values[Enums::RouteTimesArrival].toStringList().isEmpty() )
            {
                const QStringList routeTimesArrival = values[Enums::RouteTimesArrival].toStringList();
                if ( routeTimesArrival.count() != routeStops.count() - 1 ) {
                    m_additionalMessages << message( NotOneItemLessThan, Enums::RouteTimesArrival,
                            Enums::RouteStops, routeTimesArrival.count(), routeStops.count() );
                    success = false;
                }

                if ( values.contains(Enums::RouteTimesArrivalDelay)
                        && !values[Enums::RouteTimesArrivalDelay].toStringList().isEmpty() )
                {
                    const QStringList routeTimesArrivalDelay =
                        values[Enums::RouteTimesArrivalDelay].toStringList();
                    if ( routeTimesArrivalDelay.count() != routeTimesArrival.count() ) {
                        m_additionalMessages << message( NotSameNumberOfItems,
                                Enums::RouteTimesArrivalDelay, Enums::RouteTimesArrival,
                                routeTimesArrivalDelay.count(), routeTimesArrival.count() );
                        success = false;
                    }
                }
            }

            // Check if RoutePlatformsArrival has one element less than RouteStops
            if ( values.contains(Enums::RoutePlatformsArrival)
                    && !values[Enums::RoutePlatformsArrival].toStringList().isEmpty() )
            {
                const QStringList routePlatformsArrival =
                        values[Enums::RoutePlatformsArrival].toStringList();
                if ( routePlatformsArrival.count() != routeStops.count() - 1 ) {
                    m_additionalMessages << message( NotOneItemLessThan, Enums::RoutePlatformsArrival,
                            Enums::RouteStops, routePlatformsArrival.count(), routeStops.count() );
                    success = false;
                }
            }
        } else { // values does not contain RouteStops else { // values does not contain RouteStops
            QList< Enums::TimetableInformation > infos;
            infos << Enums::RouteTimes << Enums::RoutePlatformsDeparture << Enums::RoutePlatformsArrival
                  << Enums::RouteTimesArrival << Enums::RouteTimesArrivalDelay << Enums::RouteTimesDeparture
                  << Enums::RouteTimesDepartureDelay << Enums::RouteStopsShortened << Enums::RouteTypesOfVehicles
                  << Enums::RouteTransportLines;
            foreach ( Enums::TimetableInformation info, infos ) {
                if ( values.contains(info) ) {
                    m_additionalMessages << TimetableDataRequestMessage( i18nc("@info/plain",
                            "'%1' data ignored, because data for 'RouteStops' is missing.",
                            Global::timetableInformationToString(info)),
                            TimetableDataRequestMessage::Warning  );
                    success = false;
                }
            }
        }
    }

    return success;
}

}; // namespace Debugger
