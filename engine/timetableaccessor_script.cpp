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
#include "timetableaccessor_script.h"

// Own includes
#include "scripting.h"
#include "script_thread.h"
#include "timetableaccessor_info.h"
#include "departureinfo.h"
#include "request.h"

// KDE includes
#include <KLocalizedString>
#include <KConfig>
#include <KConfigGroup>
#include <KStandardDirs>
#include <ThreadWeaver/Weaver>
#include <KDebug>

// Qt includes
#include <QTextCodec>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QMutex>

const char *TimetableAccessorScript::SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS = "usedTimetableInformations";
const char *TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE = "getTimetable";
const char *TimetableAccessorScript::SCRIPT_FUNCTION_GETJOURNEYS = "getJourneys";
const char *TimetableAccessorScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS = "getStopSuggestions";

TimetableAccessorScript::TimetableAccessorScript( TimetableAccessorInfo *info, QObject *parent )
        : TimetableAccessor(info, parent), m_thread(0), m_script(0), m_scriptStorage(0),
          m_mutex(new QMutex)
{
    m_scriptState = WaitingForScriptUsage;
    m_scriptFeatures = readScriptFeatures();

    qRegisterMetaType< QList<TimetableData> >( "QList<TimetableData>" );
    qRegisterMetaType< GlobalTimetableInfo >( "GlobalTimetableInfo" );
    qRegisterMetaType< ParseDocumentMode >( "ParseDocumentMode" );
    qRegisterMetaType< ResultObject::Features >( "ResultObject::Features" );
    qRegisterMetaType< ResultObject::Hints >( "ResultObject::Hints" );

    // FIXME: Unfortunately this crashes with multiple QScriptEngine's importing the same extension
    // in different threads, ie. at the same time...
    // Maybe it helps to protect the importExtension() function
//     ThreadWeaver::Weaver::instance()->setMaximumNumberOfThreads( 1 );
}

TimetableAccessorScript::~TimetableAccessorScript()
{
    delete m_script;
    delete m_mutex;
}

QStringList TimetableAccessorScript::allowedExtensions()
{
    return QStringList() << "kross" << "qt" << "qt.core" << "qt.xml";
}

bool TimetableAccessorScript::lazyLoadScript()
{
    if ( m_script ) {
        return true;
    }

    // Read script
    QFile scriptFile( m_info->scriptFileName() );
    if ( !scriptFile.open(QIODevice::ReadOnly) ) {
        kDebug() << "Script could not be opened for reading"
                 << m_info->scriptFileName() << scriptFile.errorString();
        return false;
    }
    QTextStream stream( &scriptFile );
    QString scriptContents = stream.readAll();
    scriptFile.close();

    // Initialize the script
    m_script = new QScriptProgram( scriptContents, m_info->scriptFileName() );
    m_scriptStorage = new Storage( m_info->serviceProvider(), this );

    return true;
}

QStringList TimetableAccessorScript::readScriptFeatures()
{
    // Try to load script features from a cache file
    QString fileName = KGlobal::dirs()->saveLocation("data",
            "plasma_engine_publictransport/").append( QLatin1String("datacache"));
    bool cacheExists = QFile::exists( fileName );
    KConfig cfg( fileName, KConfig::SimpleConfig );
    KConfigGroup grp = cfg.group( m_info->serviceProvider() );

    if ( cacheExists ) {
        // Check if the script file was modified since the cache was last updated
        QDateTime scriptModifiedTime = grp.readEntry("scriptModifiedTime", QDateTime());
        if ( QFileInfo(m_info->scriptFileName()).lastModified() == scriptModifiedTime ) {
            // Return feature list stored in the cache
            return grp.readEntry("scriptFeatures", QStringList());
        }
    }

    // No actual cached information about the service provider
    kDebug() << "No up-to-date cache information for service provider" << m_info->serviceProvider();
    QStringList features;
    bool ok = lazyLoadScript();
    if ( ok ) {
        // Create script engine
        QScriptEngine engine;
        foreach ( const QString &import, m_info->scriptExtensions() ) {
            if ( !importExtension(&engine, import) ) {
                ok = false;
            }
        }
        if ( ok ) {
            engine.evaluate( *m_script );
            if ( engine.hasUncaughtException() ) {
                kDebug() << "Error in the script" << engine.uncaughtExceptionLineNumber()
                        << engine.uncaughtException().toString();
                kDebug() << "Backtrace:" << engine.uncaughtExceptionBacktrace().join("\n");
                ok = false;
            } else {
                // Test if specific functions exist in the script
                if ( engine.globalObject().property(SCRIPT_FUNCTION_GETSTOPSUGGESTIONS).isValid() ) {
                    features << "Autocompletion";
                }
                if ( engine.globalObject().property(SCRIPT_FUNCTION_GETJOURNEYS).isValid() ) {
                    features << "JourneySearch";
                }

                // Test if usedTimetableInformations() script function is available
                if ( !engine.globalObject().property(SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS).isValid() ) {
                    kDebug() << "The script has no" << SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS << "function";
                } else {
                    // Use values returned by usedTimetableInformations() script functions
                    // to get additional features of the accessor
                    QVariantList result = engine.globalObject().property(
                            SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS ).call().toVariant().toList();
                    QStringList usedTimetableInformations;
                    foreach ( const QVariant &value, result ) {
                        usedTimetableInformations << value.toString();
                    }

                    if ( usedTimetableInformations.contains(QLatin1String("Arrivals"), Qt::CaseInsensitive) ) {
                        features << "Arrivals";
                    }
                    if ( usedTimetableInformations.contains(QLatin1String("Delay"), Qt::CaseInsensitive) ) {
                        features << "Delay";
                    }
                    if ( usedTimetableInformations.contains(QLatin1String("DelayReason"), Qt::CaseInsensitive) ) {
                        features << "DelayReason";
                    }
                    if ( usedTimetableInformations.contains(QLatin1String("Platform"), Qt::CaseInsensitive) ) {
                        features << "Platform";
                    }
                    if ( usedTimetableInformations.contains(QLatin1String("JourneyNews"), Qt::CaseInsensitive)
                      || usedTimetableInformations.contains(QLatin1String("JourneyNewsOther"), Qt::CaseInsensitive)
                      || usedTimetableInformations.contains(QLatin1String("JourneyNewsLink"), Qt::CaseInsensitive) )
                    {
                        features << "JourneyNews";
                    }
                    if ( usedTimetableInformations.contains(QLatin1String("TypeOfVehicle"), Qt::CaseInsensitive) ) {
                        features << "TypeOfVehicle";
                    }
                    if ( usedTimetableInformations.contains(QLatin1String("Status"), Qt::CaseInsensitive) ) {
                        features << "Status";
                    }
                    if ( usedTimetableInformations.contains(QLatin1String("Operator"), Qt::CaseInsensitive) ) {
                        features << "Operator";
                    }
                    if ( usedTimetableInformations.contains(QLatin1String("StopID"), Qt::CaseInsensitive) ) {
                        features << "StopID";
                    }
                }
            }
        }
    }

    // Store script features in a cache file
    grp.writeEntry( "scriptModifiedTime", QFileInfo(m_info->scriptFileName()).lastModified() );
    grp.writeEntry( "hasErrors", !ok );
    grp.writeEntry( "scriptFeatures", features );

    return features;
}

QStringList TimetableAccessorScript::scriptFeatures() const
{
    return m_scriptFeatures;
}

void TimetableAccessorScript::departuresReady( const QList<TimetableData> &data,
        ResultObject::Features features, ResultObject::Hints hints, const QString &url,
        const GlobalTimetableInfo &globalInfo, const DepartureRequest &request,
        bool couldNeedForcedUpdate )
{
//     TODO use hints
    if ( data.isEmpty() ) {
        kDebug() << "The script didn't find anything" << request.sourceName;
        emit errorParsing( this, ErrorParsingFailed,
                           i18n("Error while parsing the stop suggestions document."),
                           url, &request ); // TODO emit request pointer?
    } else {
        // Create PublicTransportInfo objects for new data and combine with already published data
        PublicTransportInfoList newResults;
        ResultObject::dataList( data, &newResults, request.parseMode,
                                m_info->defaultVehicleType(), &globalInfo, features, hints );
        PublicTransportInfoList results =
                (m_publishedData[request.sourceName] << newResults);
        DepartureInfoList departures;
        foreach( const PublicTransportInfoPtr &info, results ) {
//             departures << dynamic_cast< DepartureInfo* >( info.data() );
            departures << info.dynamicCast<DepartureInfo>();
        }

        emit departureListReceived( this, url, departures, globalInfo, request );
        if ( couldNeedForcedUpdate ) {
            emit forceUpdate();
        }
    }
}

void TimetableAccessorScript::journeysReady( const QList<TimetableData> &data,
        ResultObject::Features features, ResultObject::Hints hints, const QString &url,
        const GlobalTimetableInfo &globalInfo, const JourneyRequest &request,
        bool couldNeedForcedUpdate )
{
    Q_UNUSED( couldNeedForcedUpdate );
//     TODO use hints
    if ( data.isEmpty() ) {
        kDebug() << "The script didn't find anything" << request.sourceName;
        emit errorParsing( this, ErrorParsingFailed,
                           i18n("Error while parsing the stop suggestions document."),
                           url, &request ); // TODO request pointer?
    } else {
        // Create PublicTransportInfo objects for new data and combine with already published data
        PublicTransportInfoList newResults;
        ResultObject::dataList( data, &newResults, request.parseMode,
                                m_info->defaultVehicleType(), &globalInfo, features, hints );
        PublicTransportInfoList results =
                (m_publishedData[request.sourceName] << newResults);
//         Q_ASSERT( request.parseMode == ParseForJourneys );
        JourneyInfoList journeys;
        foreach( const PublicTransportInfoPtr &info, results ) {
//             journeys << dynamic_cast< JourneyInfo* >( info.data() );
            journeys << info.dynamicCast<JourneyInfo>();
        }

        emit journeyListReceived( this, url, journeys, globalInfo, request );
    }
}

void TimetableAccessorScript::stopSuggestionsReady( const QList<TimetableData> &data,
        ResultObject::Features features, ResultObject::Hints hints, const QString &url,
        const GlobalTimetableInfo &globalInfo, const StopSuggestionRequest &request,
        bool couldNeedForcedUpdate )
{
    Q_UNUSED( couldNeedForcedUpdate );
//     TODO use hints
    kDebug() << "***** Received" << data.count() << "items";
    if ( data.isEmpty() ) {
        kDebug() << "The script didn't find anything" << request.sourceName;
        emit errorParsing( this, ErrorParsingFailed,
                           i18n("Error while parsing the stop suggestions document."),
                           url, &request ); // TODO request pointer?
    } else {
        // Create PublicTransportInfo objects for new data and combine with already published data
        PublicTransportInfoList newResults;
        ResultObject::dataList( data, &newResults, request.parseMode,
                                m_info->defaultVehicleType(), &globalInfo, features, hints );
        PublicTransportInfoList results =
                (m_publishedData[request.sourceName] << newResults);
        kDebug() << "RESULTS:" << results;

        StopInfoList stops;
        foreach( const PublicTransportInfoPtr &info, results ) {
//             stops << dynamic_cast< StopInfo* >( info.data() );
            stops << info.dynamicCast<StopInfo>();
        }

        emit stopListReceived( this, url, stops, request );
    }
}

void TimetableAccessorScript::jobStarted( ThreadWeaver::Job* job )
{
    ScriptJob *scriptJob = qobject_cast< ScriptJob* >( job );
    Q_ASSERT( scriptJob );

    const QString sourceName = scriptJob->request()->sourceName;
    Q_ASSERT ( !m_publishedData.contains(sourceName) ); // TODO
//     {
//         qDebug() << "------------------------------------------------------------------------";
//         qDebug() << "------------------------------------------------------------------------";
//         kWarning() << "The source" << sourceName << "gets filled with data from multiple threads";
//         qDebug() << "------------------------------------------------------------------------";
//         qDebug() << "------------------------------------------------------------------------";
//     }
    m_publishedData[ sourceName ].clear();
}

void TimetableAccessorScript::jobDone( ThreadWeaver::Job* job )
{
    ScriptJob *scriptJob = qobject_cast< ScriptJob* >( job );
    Q_ASSERT( scriptJob );

    const QString sourceName = scriptJob->request()->sourceName;
    PublicTransportInfoList results = m_publishedData.take( sourceName );
    kDebug() << "***** (DO NOT => QSharedPointer) Delete" << results.count() << "items";
    kDebug() << "m_publishedData contains" << m_publishedData.count() << "items";
//     qDeleteAll( results );
//     foreach ( PublicTransportInfoPtr ptr, results ) {
//         ptr.clear();
//         kDebug() << "DELETED?" << ptr.isNull();
//     }
    delete scriptJob;
}

void TimetableAccessorScript::jobFailed( ThreadWeaver::Job* job )
{
    ScriptJob *scriptJob = qobject_cast< ScriptJob* >( job );
    Q_ASSERT( scriptJob );

    emit errorParsing( this, ErrorParsingFailed, scriptJob->errorString(),
                       /*TODO: failing url */ QString(),
//                            TODO: no new... serviceProvider(),
                       new DepartureRequest(scriptJob->request()->sourceName,
                           /*stop*/QString(), /*dateTime*/QDateTime(), /*maxCount*/0,
                            /*dataType*/QString(), /*city*/QString(), /*parseMode*/ParseForDeparturesArrivals) );
}

void TimetableAccessorScript::requestDepartures( const DepartureRequest &request )
{
    if ( !lazyLoadScript() ) {
        kDebug() << "Failed to load script!";
        return;
    }

    DepartureJob *job = new DepartureJob( m_script, m_info, m_scriptStorage, request, this );
    connect( job, SIGNAL(started(ThreadWeaver::Job*)), this, SLOT(jobStarted(ThreadWeaver::Job*)) );
    connect( job, SIGNAL(done(ThreadWeaver::Job*)), this, SLOT(jobDone(ThreadWeaver::Job*)) );
    connect( job, SIGNAL(failed(ThreadWeaver::Job*)), this, SLOT(jobFailed(ThreadWeaver::Job*)) );
    connect( job, SIGNAL(departuresReady(QList<TimetableData>,ResultObject::Features,ResultObject::Hints,QString,GlobalTimetableInfo,DepartureRequest,bool)),
             this, SLOT(departuresReady(QList<TimetableData>,ResultObject::Features,ResultObject::Hints,QString,GlobalTimetableInfo,DepartureRequest,bool)) );
    ThreadWeaver::Weaver::instance()->enqueue( job );
    return;
}

void TimetableAccessorScript::requestArrivals( const ArrivalRequest &request )
{
    if ( !lazyLoadScript() ) {
        kDebug() << "Failed to load script!";
        return;
    }

    DepartureJob *job = new DepartureJob( m_script, m_info, m_scriptStorage, request, this );
    connect( job, SIGNAL(started(ThreadWeaver::Job*)), this, SLOT(jobStarted(ThreadWeaver::Job*)) );
    connect( job, SIGNAL(done(ThreadWeaver::Job*)), this, SLOT(jobDone(ThreadWeaver::Job*)) );
    connect( job, SIGNAL(failed(ThreadWeaver::Job*)), this, SLOT(jobFailed(ThreadWeaver::Job*)) );
    connect( job, SIGNAL(departuresReady(QList<TimetableData>,ResultObject::Features,ResultObject::Hints,QString,GlobalTimetableInfo,DepartureRequest,bool)),
             this, SLOT(arrivalsReady(QList<TimetableData>,ResultObject::Features,ResultObject::Hints,QString,GlobalTimetableInfo,DepartureRequest,bool)) );
    ThreadWeaver::Weaver::instance()->enqueue( job );
    return;
}

void TimetableAccessorScript::requestJourneys( const JourneyRequest &request )
{
    if ( !lazyLoadScript() ) {
        kDebug() << "Failed to load script!";
        return;
    }

    JourneyJob *job = new JourneyJob( m_script, m_info, m_scriptStorage, request, this );
    connect( job, SIGNAL(done(ThreadWeaver::Job*)), this, SLOT(jobDone(ThreadWeaver::Job*)) );
    connect( job, SIGNAL(journeysReady(QList<TimetableData>,ResultObject::Features,ResultObject::Hints,QString,GlobalTimetableInfo,JourneyRequest,bool)),
             this, SLOT(journeysReady(QList<TimetableData>,ResultObject::Features,ResultObject::Hints,QString,GlobalTimetableInfo,JourneyRequest,bool)) );
    ThreadWeaver::Weaver::instance()->enqueue( job );
    return;
}

void TimetableAccessorScript::requestStopSuggestions( const StopSuggestionRequest &request )
{
    if ( !lazyLoadScript() ) {
        kDebug() << "Failed to load script!";
        return;
    }

    StopSuggestionsJob *job = new StopSuggestionsJob( m_script, m_info, m_scriptStorage,
                                                      request, this );
    connect( job, SIGNAL(done(ThreadWeaver::Job*)), this, SLOT(jobDone(ThreadWeaver::Job*)) );
    connect( job, SIGNAL(stopSuggestionsReady(QList<TimetableData>,ResultObject::Features,ResultObject::Hints,QString,GlobalTimetableInfo,StopSuggestionRequest,bool)),
             this, SLOT(stopSuggestionsReady(QList<TimetableData>,ResultObject::Features,ResultObject::Hints,QString,GlobalTimetableInfo,StopSuggestionRequest,bool)) );
    ThreadWeaver::Weaver::instance()->enqueue( job );
    return;
}

void TimetableAccessorScript::import( const QString &import, QScriptEngine *engine )
{
    QMutexLocker locker( m_mutex );
    engine->importExtension( import );
}
