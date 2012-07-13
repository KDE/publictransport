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
#include <threadweaver/DebuggingAids.h>
#include <KDebug>
#include <KLocalizedString>
#include <KGlobal>
#include <KLocale>

// Qt includes
#include <QEventLoop>
#include <QTimer>

namespace Debugger {

CallScriptFunctionJob::CallScriptFunctionJob( DebuggerAgent *debugger,
        const ServiceProviderData &data, QMutex *engineMutex, const QString &functionName,
        const QVariantList &arguments, DebugFlags debugFlags, QObject *parent )
        : DebuggerJob( debugger, data, engineMutex, parent ),
          m_debugFlags(debugFlags), m_functionName(functionName), m_arguments(arguments),
          m_currentLoop(0)
{
}

CallScriptFunctionJob::CallScriptFunctionJob( DebuggerAgent *debugger,
        const ServiceProviderData &data, QMutex *engineMutex, DebugFlags debugFlags,
        QObject *parent )
        : DebuggerJob( debugger, data, engineMutex, parent ),
          m_debugFlags(debugFlags), m_currentLoop(0)
{
}

TestFeaturesJob::TestFeaturesJob( DebuggerAgent *debugger,
        const ServiceProviderData &data, QMutex *engineMutex, DebugFlags debugFlags,
        QObject *parent )
        : CallScriptFunctionJob(debugger, data, engineMutex,
                                ServiceProviderScript::SCRIPT_FUNCTION_FEATURES,
                                QVariantList(), debugFlags, parent)
{
}

TimetableDataRequestJob::TimetableDataRequestJob( DebuggerAgent *debugger,
        const ServiceProviderData &data, QMutex *engineMutex, const AbstractRequest *request,
        DebugFlags debugFlags, QObject *parent )
        : CallScriptFunctionJob(debugger, data, engineMutex, debugFlags, parent),
          m_request(request->clone())
//         : DebuggerJob(debugger, info, engineMutex, parent),
//           m_request(request->clone()), m_debugMode(debugMode)
{
    switch ( request->parseMode ) {
    case ParseForDepartures:
    case ParseForArrivals:
        m_functionName = ServiceProviderScript::SCRIPT_FUNCTION_GETTIMETABLE;
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
        QMutexLocker locker( m_mutex );
        m_explanation = "Unknown parse mode";
        m_success = false;
        return;
    }
}

void CallScriptFunctionJob::requestAbort()
{
    DebuggerJob::requestAbort();

    QMutexLocker locker( m_mutex );
    if ( m_currentLoop ) {
        QEventLoop *loop = m_currentLoop;
        m_currentLoop = 0;
        loop->quit();
    }
}

QScriptValueList CallScriptFunctionJob::createArgumentScriptValues() const
{
    m_mutex->lockInline();
    QVariantList arguments = m_arguments;
    m_mutex->unlockInline();

    QScriptValueList args;
    QScriptEngine *engine = m_debugger->engine();
    foreach ( const QVariant &argument, arguments ) {
        args << engine->toScriptValue( argument );
    }
    return args;
}

QScriptValueList TimetableDataRequestJob::createArgumentScriptValues() const
{
    return QScriptValueList() << m_request->toScriptValue( m_debugger->engine() );
}

void TimetableDataRequestJob::finish( Scripting::ResultObject *result )
{
    m_timetableData = result->data();
}

CallScriptFunctionJob::~CallScriptFunctionJob()
{
}

TimetableDataRequestJob::~TimetableDataRequestJob()
{
    delete m_request;
}

QScriptValue CallScriptFunctionJob::returnValue() const
{
    QMutexLocker locker( m_mutex );
    return m_returnValue;
}

QList< Enums::ProviderFeature > TestFeaturesJob::features() const
{
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

QString CallScriptFunctionJob::toString() const
{
    QMutexLocker locker( m_mutex );
    if ( m_returnValue.isValid() ) {
        QString valueString = m_returnValue.toString();
        if ( valueString.length() > 100 ) {
            valueString = valueString.left(100) + "...";
        }
        return QString("%1 (%2() = %3)").arg( DebuggerJob::toString() )
                .arg( m_functionName ).arg( valueString );
    } else {
        return QString("%1 (%2())").arg( DebuggerJob::toString() ).arg( m_functionName );
    }
}

bool TestFeaturesJob::testResults()
{
    const QVariant variant = m_returnValue.toVariant();
    if ( !variant.isValid() ) {
        return false;
    }

    const QVariantList items = variant.toList();
    if ( items.isEmpty() ) {
        m_additionalMessages << TimetableDataRequestMessage(
                i18nc("@info/plain", "No TimetableInformation string returned"),
                TimetableDataRequestMessage::Warning );
    } else {
        int i = 1;
        foreach ( const QVariant &item, items ) {
//             const bool stringEqualsArrivals =
//                     string.compare(QLatin1String("arrivals"), Qt::CaseInsensitive) == 0;
            const Enums::ProviderFeature feature =
                    static_cast< Enums::ProviderFeature >( item.toInt() );
//             if ( info == Enums::Nothing ) { //&& !stringEqualsArrivals ) {
//                 m_additionalMessages << TimetableDataRequestMessage(
//                         i18nc("@info/plain", "Invalid TimetableInformation string returned: '%1'",
//                               string),
//                         TimetableDataRequestMessage::Error );
//             } else {
            m_features << feature;
            m_additionalMessages << TimetableDataRequestMessage(
                    QString("%1: %2").arg(i).arg(Enums::toString(feature)),
                    TimetableDataRequestMessage::Information );
            ++i;
//             }
        }
    }

    return true;
}

void CallScriptFunctionJob::debuggerRun()
{
    m_mutex->lockInline();
    if ( !m_success ) {
        // Job already marked as not successful, a derived class may have set it to false
        m_mutex->unlockInline();
        return;
    }

    DebuggerAgent *debugger = m_debugger;
    QScriptEngine *engine = m_debugger->engine();
    const QString functionName = m_functionName;
    const DebugFlags debugFlags = m_debugFlags;
    m_mutex->unlockInline();

    m_engineMutex->lockInline();
    engine->clearExceptions();

    ResultObject *scriptResult = qobject_cast< ResultObject* >(
            engine->globalObject().property("result").toQObject() );
    Q_ASSERT( scriptResult );

    // Clear "result" script object
    scriptResult->clear();

    QScriptValue function = engine->globalObject().property( functionName );
    if ( !function.isFunction() ) {
        ThreadWeaver::debug( 0, " - Run script ERROR: Did not find function %s\n",
                             functionName.toUtf8().constData() );
        m_engineMutex->unlockInline();

        QMutexLocker locker( m_mutex );
        m_explanation = i18nc("@info/plain", "Did not find a '%1' function in the script.",
                               functionName);
        m_success = false;
        return;
    }

    // Create new Network object in this thread, restore old object at the end
    const QScriptValue previousScriptNetwork = engine->globalObject().property("network");
    Network *scriptNetwork = new Network( m_data.fallbackCharset() );
    connect( scriptNetwork, SIGNAL(requestFinished(NetworkRequest*,QString,int)),
             this, SLOT(requestFinished(NetworkRequest*,QString,int)) );
    connect( scriptNetwork, SIGNAL(synchronousRequestFinished(QString,QString,bool,int,int)),
             this, SLOT(synchronousRequestFinished(QString,QString,bool,int,int)) );
    engine->globalObject().setProperty( "network", engine->newQObject(scriptNetwork) );

    // Get the helper object and connect to the errorReceived() signal to collect
    // messages sent to "helper.error()" from a script
    Helper *scriptHelper = qobject_cast< Helper* >(
            engine->globalObject().property("helper").toQObject() );
    connect( scriptHelper, SIGNAL(errorReceived(QString,QScriptContextInfo,QString)),
             this, SLOT(scriptErrorReceived(QString,QScriptContextInfo,QString)) );

    connect( scriptResult, SIGNAL(invalidDataReceived(Enums::TimetableInformation,QString,QScriptContextInfo,int,QVariantMap)),
             this, SLOT(invalidDataReceived(Enums::TimetableInformation,QString,QScriptContextInfo,int,QVariantMap)) );

    // Call script function
    if ( debugFlags.testFlag(InterruptAtStart) ) {
        debugger->setExecutionControlType( ExecuteInterrupt );
    } else if ( debugFlags.testFlag(InterruptOnExceptions) ) {
        debugger->setExecutionControlType( ExecuteContinue );
    }
    const QScriptValueList arguments = createArgumentScriptValues();
    QScriptValue returnValue = function.call( QScriptValue(), arguments );
    m_engineMutex->unlockInline();

    // The called function returned, but asynchronous network requests may have been started.
    // Wait for all network requests to finish, because slots in the script may get called
    const int finishWaitTime = 500;
    int finishWaitCounter = 0;
    while ( !debugger->wasLastRunAborted() && scriptNetwork->hasRunningRequests() &&
            finishWaitCounter < 20 )
    {
        QEventLoop loop;
        connect( scriptNetwork, SIGNAL(allRequestsFinished()), &loop, SLOT(quit()) );
//         connect( this, SIGNAL(destroyed(QObject*)), &loop, SLOT(quit()) );
        QTimer::singleShot( finishWaitTime, &loop, SLOT(quit()) );

        m_currentLoop = &loop;
        loop.exec();
        if ( !m_currentLoop ) {
            setFinished( true );
            return;
        } else {
            m_currentLoop = 0;
        }

        ++finishWaitCounter;
    }
    const bool allNetworkRequestsFinished = !scriptNetwork->hasRunningRequests();

    // Waiting for script execution to finish
    finishWaitCounter = 0;
    while ( !debugger->wasLastRunAborted() && m_debugger->isRunning() && finishWaitCounter < 20 ) {
        QEventLoop loop;
        connect( m_debugger, SIGNAL(stopped()), &loop, SLOT(quit()) );
//         connect( this, SIGNAL(destroyed(QObject*)), &loop, SLOT(quit()) );
        QTimer::singleShot( finishWaitTime, &loop, SLOT(quit()) );

        m_currentLoop = &loop;
        loop.exec();
        if ( !m_currentLoop ) {
            setFinished( true );
            return;
        } else {
            m_currentLoop = 0;
        }

        ++finishWaitCounter;
    }
    const bool finishedSuccessfully = !debugger->wasLastRunAborted() && finishWaitCounter < 20;

//     GlobalTimetableInfo globalInfo;
//     globalInfo.requestDate = QDate::currentDate();
//     globalInfo.delayInfoAvailable =
//             !m_scriptResult->isHintGiven( ResultObject::NoDelaysForStop );

    if ( finishedSuccessfully || debugger->wasLastRunAborted() ) {
        // Script finished or was aborted
        m_engineMutex->lockInline();
    } else {
        // Script not finished, engine mutex is most probably still locked
        kDebug() << "Script not finished, abort";
        m_engineMutex->tryLock();
        engine->abortEvaluation();
    }

    disconnect( scriptResult, SIGNAL(invalidDataReceived(Enums::TimetableInformation,QString,QScriptContextInfo,int,QVariantMap)),
                this, SLOT(invalidDataReceived(Enums::TimetableInformation,QString,QScriptContextInfo,int,QVariantMap)) );

    // Check for exceptions
    if ( finishedSuccessfully && engine->hasUncaughtException() ) {
        m_engineMutex->unlockInline();
        ThreadWeaver::debug( 0, " - Run script ERROR: In function %s: %s\n",
                            functionName.toUtf8().constData(),
                            engine->uncaughtException().toString().toUtf8().constData() );
        handleError( engine, i18nc("@info/plain", "Error in the script at line %1 in function "
                                "'%2': <message>%3</message>.",
                                engine->uncaughtExceptionLineNumber(), functionName,
                                engine->uncaughtException().toString()) );
        m_debugger->debugInterrupt(); // TEST is this needed?
        return;
    }
//     engine->clearExceptions();
    m_engineMutex->unlockInline();

    m_mutex->lockInline();
    m_returnValue = returnValue;
    finish( scriptResult );
    m_mutex->unlockInline();

    // Mark job as done for Debugger to know that the script is not waiting for a signal
    setFinished( true );

    // Use the previous Network object in the engine again and delete the one for this thread
    // This needs to be called after setFinished(true), otherwise the Debugger thinks there are
    // still running jobs, ie. waiting for a signal
    m_engineMutex->lockInline();
    engine->globalObject().setProperty( "network", previousScriptNetwork );
    m_engineMutex->unlockInline();
    delete scriptNetwork;

    QMutexLocker locker( m_mutex );
    if ( allNetworkRequestsFinished && finishedSuccessfully ) {
        // No uncaught exceptions, all network requests finished
        if ( debugger->wasLastRunAborted() ) {
            m_success = false;
            m_explanation = i18nc("@info/plain", "Execution was aborted");
        } else {
            m_success = testResults();
        }
    } else if ( finishedSuccessfully ) {
        // The script finish successfully, but not all network requests finished
        m_explanation = i18nc("@info/plain", "Not all network requests were finished in time");
        m_success = false;
    } else if ( debugger->wasLastRunAborted() ) {
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

void CallScriptFunctionJob::scriptErrorReceived( const QString &message,
                                                 const QScriptContextInfo &context,
                                                 const QString &failedParseText )
{
    Q_UNUSED( failedParseText );
    m_additionalMessages << TimetableDataRequestMessage(
            i18nc("@info/plain", "Error in line %1: <message>%2</message>",
                  context.lineNumber(), message),
            TimetableDataRequestMessage::Warning, context.lineNumber() );
}

void CallScriptFunctionJob::invalidDataReceived( Enums::TimetableInformation information,
                                                 const QString &message,
                                                 const QScriptContextInfo &context,
                                                 int index, const QVariantMap& map )
{
    Q_UNUSED( information );
    Q_UNUSED( map );
    m_additionalMessages << TimetableDataRequestMessage(
            i18nc("@info/plain", "Invalid data in result %1, line %2: <message>%3</message>",
                  index + 1, context.lineNumber(), message),
            TimetableDataRequestMessage::Error, context.lineNumber() );
}

void CallScriptFunctionJob::requestFinished( NetworkRequest *request, const QString &data, int size )
{
    m_additionalMessages << TimetableDataRequestMessage(
            i18nc("@info/plain", "Download finished: %1, <link>%2</link>",
                  KGlobal::locale()->formatByteSize(size), request->url()),
            TimetableDataRequestMessage::Information, -1, TimetableDataRequestMessage::OpenLink,
            request->url() );
    emit asynchronousRequestWaitFinished( size );
}

void CallScriptFunctionJob::synchronousRequestFinished( const QString &url, const QString &data,
                                                        bool cancelled, int waitingTime, int size )
{
    if ( cancelled ) {
        m_additionalMessages << TimetableDataRequestMessage(
                i18nc("@info/plain", "Download cancelled/failed: <link>%1</link>", url),
                TimetableDataRequestMessage::Warning, -1, TimetableDataRequestMessage::OpenLink,
                url );
    } else {
        m_additionalMessages << TimetableDataRequestMessage(
                i18nc("@info/plain", "Download finished: %1, <link>%2</link>",
                      KGlobal::locale()->formatByteSize(size), url),
                TimetableDataRequestMessage::Information, -1, TimetableDataRequestMessage::OpenLink,
                url );
    }
    emit synchronousRequestWaitFinished( waitingTime, size );
}

bool TimetableDataRequestJob::testResults()
{
    const DepartureRequest *departureRequest =
            dynamic_cast< const DepartureRequest* >( m_request );
    if ( departureRequest ) {
        return testDepartureData( departureRequest );
    }

    const StopSuggestionRequest *stopSuggestionRequest =
            dynamic_cast< const StopSuggestionRequest* >( m_request );
    if ( stopSuggestionRequest ) {
        return testStopSuggestionData( stopSuggestionRequest );
    }

    const JourneyRequest *journeyRequest =
            dynamic_cast< const JourneyRequest* >( m_request );
    if ( journeyRequest ) {
        return testJourneyData( journeyRequest );
    }

    return false;
}

TimetableDataRequestMessage CallScriptFunctionJob::message( MessageType messageType,
        Enums::TimetableInformation info1, Enums::TimetableInformation info2, int count1, int count2,
        TimetableDataRequestMessage::Type type, int lineNumber )
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

    return TimetableDataRequestMessage( msg, type, lineNumber );
}

bool TimetableDataRequestJob::testDepartureData( const DepartureRequest *request )
{
    if ( m_timetableData.isEmpty() ) {
        if ( request->parseMode == ParseForArrivals ) {
            m_explanation = i18nc("@info/plain", "No arrivals found");
        } else {
            m_explanation = i18nc("@info/plain", "No departures found");
        }
        return false;
    }

    // Get global information
    QStringList globalInfos;
    if ( m_returnValue.isValid() ) {
        globalInfos = m_returnValue.toVariant().toStringList();
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
    if ( request->parseMode == ParseForArrivals ) {
        m_explanation = i18ncp("@info/plain", "Got %1 arrival",
                                              "Got %1 arrivals", m_timetableData.count());
    } else {
        m_explanation = i18ncp("@info/plain", "Got %1 departure",
                                              "Got %1 departures", m_timetableData.count());
    }

    if ( countInvalid > 0 ) {
        if ( request->parseMode == ParseForArrivals ) {
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
    if ( m_timetableData.isEmpty() ) {
        m_explanation = i18nc("@info/plain", "No stop suggestions found");
        return false;
    }

    // Get global information
    QStringList globalInfos;
    if ( m_returnValue.isValid() ) {
        globalInfos = m_returnValue.toVariant().toStringList();
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
    if ( m_timetableData.isEmpty() ) {
        m_explanation = i18nc("@info/plain", "No journeys found");
        return false;
    }

    // Get global information
    QStringList globalInfos;
    if ( m_returnValue.isValid() ) {
        globalInfos = m_returnValue.toVariant().toStringList();
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
