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
#include "publictransportrunner.h"

// libpublictransporthelper includes
#include "marbleprocess.h"

// KDE includes
#include <KToolInvocation>
#include <KDebug>
#include <KIcon>

// Qt includes
#include <QThread>
#include <QCoreApplication>
#include <QWaitCondition>
#include <QTimer>
#include <QSemaphore>

using namespace PublicTransport;

PublicTransportRunner::PublicTransportRunner( QObject *parent, const QVariantList& args )
        : Plasma::AbstractRunner(parent, args),
          m_helper(new PublicTransportRunnerHelper(this)),
          m_semaphore(new QSemaphore(1)), m_marble(0)
{
    Q_UNUSED( args );
    setObjectName( QLatin1String( "PublicTransportRunner" ) );

    // The connection to the data engine must be created in it's own thread,
    // because it's not thread safe (using KIO, ...)
    m_helper->moveToThread( dataEngine( "publictransport" )->thread() );

    setIgnoredTypes( Plasma::RunnerContext::Directory | Plasma::RunnerContext::File |
                     Plasma::RunnerContext::NetworkLocation | Plasma::RunnerContext::Executable |
                     Plasma::RunnerContext::ShellCommand );

    reloadConfiguration();
}

PublicTransportRunner::~PublicTransportRunner()
{
    delete m_helper;
    delete m_semaphore;
    delete m_marble;
}

void PublicTransportRunner::init()
{
    // We are pretty slow,
    // requesting data from web servers, then parsing it in the data engine.
    // (But can be fast, if the data engine has cached values for the request)
    setSpeed( SlowSpeed );

    // We aren't very important, mostly because we are slow
    setPriority( NormalPriority );
}

void PublicTransportRunner::reloadConfiguration()
{
    KConfigGroup grp = config(); // Create config-object

    m_settings.location = grp.readEntry( CONFIG_LOCATION, KGlobal::locale()->country() );
    m_settings.serviceProviderID = grp.readEntry( CONFIG_SERVICE_PROVIDER_ID, QString() );
    m_settings.city = grp.readEntry( CONFIG_CITY, QString() );
    m_settings.keywordDeparture = grp.readEntry( CONFIG_KEYWORD_DEPARTURE,
            i18nc( "This is a runner keyword to search for departures", "departures" ) );
    m_settings.keywordArrival = grp.readEntry( CONFIG_KEYWORD_ARRIVAL,
            i18nc( "This is a runner keyword to search for arrivals", "arrivals" ) );
    m_settings.keywordJourney = grp.readEntry( CONFIG_KEYWORD_JOURNEY,
            i18nc( "This is a runner keyword to search for journeys", "journeys" ) );
    m_settings.keywordStop = grp.readEntry( CONFIG_KEYWORD_STOP,
            i18nc( "This is a runner keyword to search for stops", "stops" ) );
    m_settings.resultCount = grp.readEntry( CONFIG_RESULT_COUNT, 4 );

    Plasma::RunnerSyntax syntaxDepartures( m_settings.keywordDeparture + QLatin1String( " :q:" ),
            i18n( "Finds public transport departures from :q:." ) );
    syntaxDepartures.setSearchTermDescription(
            i18nc( "A description of the search term for the 'departures' keyword", "stop" ) );

    Plasma::RunnerSyntax syntaxArrivals( m_settings.keywordArrival + QLatin1String( " :q:" ),
            i18n( "Finds public transport arrivals at :q:." ) );
    syntaxArrivals.setSearchTermDescription(
            i18nc( "A description of the search term for the 'arrivals' keyword", "stop" ) );

    Plasma::RunnerSyntax syntaxJourneys( m_settings.keywordJourney + QLatin1String( " :q:" ),
                                         i18n( "Finds public transport journeys :q:." ) );
    syntaxJourneys.setSearchTermDescription(
            i18nc( "A description of the search term for the 'journeys' keyword", "origin stop to target stop" ) );

    Plasma::RunnerSyntax syntaxStops( m_settings.keywordStop + QLatin1String( " :q:" ),
                                      i18n( "Finds public transport stops like :q:." ) );
    syntaxStops.setSearchTermDescription(
            i18nc( "A description of the search term for the 'stops' keyword", "stop part" ) );

    setDefaultSyntax( syntaxDepartures );
    addSyntax( syntaxArrivals );
    addSyntax( syntaxJourneys );
    addSyntax( syntaxStops );
}

void PublicTransportRunner::match( Plasma::RunnerContext &context )
{
    // Limit matches running in parallel
    m_semaphore->acquire();

    // Used aseigo's change in the places runner to make it work with KIO's non-thread-safety
    Plasma::DataEngine *engine = dataEngine( "publictransport" );
    if ( QThread::currentThread() == QCoreApplication::instance()->thread() ) {
        // from the main thread
        m_helper->match( this, engine, &context );
    } else {
        // from the non-gui thread
        emit doMatch( this, engine, &context );

        // Wait for the matching to finish (the RunnerContext object needs to stay valid)
        QEventLoop loop;
        connect( m_helper, SIGNAL(matchFinished(FinishType)), &loop, SLOT(quit()) );
        loop.exec();
    }

    m_semaphore->release();
}

void PublicTransportRunnerHelper::match( PublicTransportRunner *runner,
        Plasma::DataEngine *engine, Plasma::RunnerContext* c )
{
    QPointer< Plasma::RunnerContext > context = c;
    if ( !context || !context->isValid() ) {
        emit matchFinished( Aborted );
        return;
    }

    // TODO Store the map in the PublicTransportRunner
    PublicTransportRunner::Settings settings = runner->settings();
    static QMap<QString, PublicTransportRunner::Keywords> keywordMap;
    keywordMap.insert( settings.keywordJourney, PublicTransportRunner::Journeys );
    keywordMap.insert( settings.keywordDeparture, PublicTransportRunner::Departures );
    keywordMap.insert( settings.keywordArrival, PublicTransportRunner::Arrivals );
    keywordMap.insert( settings.keywordStop, PublicTransportRunner::StopSuggestions );
    keywordMap.insert( i18nc( "This is a runner keyword to search for bus departures",
                              "buses from" ),
                       PublicTransportRunner::OnlyBuses | PublicTransportRunner::Departures );
    keywordMap.insert( i18nc( "This is a runner keyword to search for bus arrivals",
                              "buses to" ),
                       PublicTransportRunner::OnlyBuses | PublicTransportRunner::Arrivals );
    keywordMap.insert( i18nc( "This is a runner keyword to search for tram departures",
                              "trams from" ),
                       PublicTransportRunner::OnlyTrams | PublicTransportRunner::Departures );
    keywordMap.insert( i18nc( "This is a runner keyword to search for tram arrivals",
                              "trams to" ),
                       PublicTransportRunner::OnlyTrams | PublicTransportRunner::Arrivals );
    keywordMap.insert( i18nc( "This is a runner keyword to search for bus/tram/interurban "
                              "train/metro/trolley bus departures",
                              "public transport from" ),
                       PublicTransportRunner::OnlyPublicTransport | PublicTransportRunner::Departures );
    keywordMap.insert( i18nc( "This is a runner keyword to search for bus/tram/interurban "
                              "train/metro/trolley bus arrivals",
                              "public transport to" ),
                       PublicTransportRunner::OnlyPublicTransport | PublicTransportRunner::Arrivals );
    keywordMap.insert( i18nc( "This is a runner keyword to search for train departures",
                              "trains from" ),
                       PublicTransportRunner::OnlyTrains | PublicTransportRunner::Departures );
    keywordMap.insert( i18nc( "This is a runner keyword to search for train arrivals",
                              "trains to" ),
                       PublicTransportRunner::OnlyTrains | PublicTransportRunner::Arrivals );

    PublicTransportRunner::Keywords keywords = PublicTransportRunner::NoKeyword;

    QString term = context->query();
    if ( term.length() < 3 ) {
        emit matchFinished( FinishedWithErrors );
        return;
    }

    // Read and cut keywords
    for ( QMap<QString, PublicTransportRunner::Keywords>::const_iterator it = keywordMap.constBegin();
            it != keywordMap.constEnd(); ++it ) {
        if ( term.startsWith(it.key() + ' ', Qt::CaseInsensitive) ) {
            keywords = it.value();

            // Cut the keyword from the term
            term = term.mid( it.key().length() + 1 );
            break;
        }
    }

    if ( keywords == PublicTransportRunner::NoKeyword ) {
        if ( context->singleRunnerQueryMode() ) {
            // Single runner query mode doesn't need a keyword, assume Departures (used as default syntax)
            keywords = PublicTransportRunner::Departures;
        } else {
            // When not in single runner query mode a keyword is needed
            emit matchFinished( FinishedWithErrors );
            return;
        }
    }

    // Dont't allow too short terms after the first keyword
    QString stop = term.trimmed();
    if ( stop.length() < 3 ) {
        emit matchFinished( FinishedWithErrors );
        return;
    }

    PublicTransportRunner::QueryData data;
    data.keywords = keywords;

    QRegExp rx( i18nc("This is a regular expression, used for the runner query string, to give "
                      "the offset in minutes of the first result. '\\b' at the beginning and "
                      "at the end assures, that the found string is separated from other words "
                      "with whitespaces. The '(\\d+)' is used to match an integer and there must "
                      "be one such part in the regexp (there shouldn't be parenthesized "
                      "expressions before this one, but you can use non-matching parentheses "
                      "'(?:XX)'). 'min(?:utes)?' can match 'min' and 'minutes'. The string is "
                      "matched case insensitive.",
                      "\\bin\\s+(\\d+)\\s+min(?:utes)?\\b"), Qt::CaseInsensitive );
    if ( rx.indexIn(stop) != -1 && rx.captureCount() ) {
        data.minutesUntilFirstResult = rx.cap(1).toInt();

        // Cut the matched string from the query string
        stop = stop.remove( rx.pos(), rx.matchedLength() );
    } else {
        data.minutesUntilFirstResult = 2; // Default, "in 2 minutes"
    }

    kDebug() << "Keyword found, rest of the term is" << stop;
    QString stop2;
    if ( keywords.testFlag( PublicTransportRunner::Journeys ) ) {
        QString pattern = QString( "^(?:%1\\s+)?(.*)(?:\\s+%2\\s+)(.*)$" )
                .arg( i18nc("Used for journey searches before the origin stop", "from") )
                .arg( i18nc("Used for journey searches before the target stop", "to") );
        QRegExp rx( pattern, Qt::CaseInsensitive );
        if ( rx.indexIn(stop) == -1 ) {
            kDebug() << "Journey regexp pattern" << pattern << "not matched in" << stop;
            emit matchFinished( FinishedWithErrors );
            return;
        }

        stop = rx.cap(1);
        stop2 = rx.cap(2);
    }

    // Wait a little bit, we don't want to query on every keypress
    QMutex mutex;
    QWaitCondition waiter;
    mutex.lock();
    if ( keywords.testFlag( PublicTransportRunner::StopSuggestions ) ) {
        // Stop suggestion documents are usually smaller, no need to wait so much
        waiter.wait( &mutex, 50 );
    } else {
        waiter.wait( &mutex, 500 );
    }
    mutex.unlock();

    // Check if the context is still valid
    if ( !context || !context->isValid() ) {
        emit matchFinished( Aborted );
        return;
    }

    // TODO: put the asyncUpdater-code into the helper class as methods?
    AsyncDataEngineUpdater asyncUpdater( engine, context.data(), runner );

    QEventLoop loop;
    connect( &asyncUpdater, SIGNAL(finished(bool)), &loop, SLOT(quit()) );

    // Query results from the data engine
    asyncUpdater.query( engine, data, stop, stop2 );

    // Wait for the updater to finish
    loop.exec();

    // Check if the context is still valid
    if ( !context || !context->isValid() ) {
        emit matchFinished( Aborted );
        return;
    }

    // Create a match for each result
    const QList<Result> results = asyncUpdater.results();
    QList<Plasma::QueryMatch> matches;
    foreach( const Result &result, results ) {
        Plasma::QueryMatch m( runner );
        m.setType( Plasma::QueryMatch::HelperMatch );
        m.setIcon( result.icon );
        m.setText( result.text );
        m.setSubtext( result.subtext );
        m.setData( QVariant::fromValue<Result>(result) );
        m.setRelevance( result.relevance );
        matches << m;
    }

    context->addMatches( context->query(), matches );
    emit matchFinished( FinishedSuccessfully );
}

void PublicTransportRunner::run( const Plasma::RunnerContext &context,
                                 const Plasma::QueryMatch &match )
{
    Q_UNUSED( context )

    // Open the page containing the departure/arrival/journey in a web browser
    Result result = match.data().value< Result >();
    if ( result.data.contains("StopLongitude") && result.data.contains("StopLatitude") ) {
        // Use stop coordinates to show the stop in Marble
        const QString stopName = result.data["StopName"].toString();
        const qreal longitude = result.data["StopLongitude"].toReal();
        const qreal latitude = result.data["StopLatitude"].toReal();
        if ( m_marble ) {
            // Marble is already running
            m_marble->centerOnStop( stopName, longitude, latitude );
        } else {
            m_marble = new MarbleProcess( stopName, longitude, latitude, this );
//             connect( m_marble, SIGNAL(marbleError(QString)), this, SLOT(marbleError(QString)) );
            connect( m_marble, SIGNAL(finished(int)), this, SLOT(marbleFinished()) );
            m_marble->start();
        }
    } else if ( !result.url.isEmpty() ) {
        KToolInvocation::invokeBrowser( result.url.toString() );
    }
}

void PublicTransportRunner::marbleFinished()
{
    m_marble = 0;
}

AsyncDataEngineUpdater::AsyncDataEngineUpdater( Plasma::DataEngine *engine,
        Plasma::RunnerContext* context, PublicTransportRunner *runner )
        : QObject(0), m_engine(engine), m_context(context), m_runner(runner)
{
    m_settings = m_runner->settings();
}

QList< Result > AsyncDataEngineUpdater::results() const
{
    return m_results;
}

Plasma::DataEngine* AsyncDataEngineUpdater::engine() const
{
    return m_engine;
}

void AsyncDataEngineUpdater::abort()
{
    // Disconnect source
    m_engine->disconnectSource( m_sourceName, this );

    emit finished( false );
}

void AsyncDataEngineUpdater::query( Plasma::DataEngine *engine,
                                    const PublicTransportRunner::QueryData &data,
                                    const QString &stop, const QString &stop2 )
{
    PublicTransportRunner::Keywords keywords = data.keywords;
    int resultCount = m_context->singleRunnerQueryMode() ? 10 : m_settings.resultCount;

    if ( keywords.testFlag( PublicTransportRunner::Journeys ) ) {
        if ( stop2.isEmpty() ) {
            kDebug() << "Journey searches need two stop names, but only one given";
            emit finished( false );
            return;
        }

        m_sourceName = QString( "Journeys %1|originstop=%2|targetstop=%3|maxcount=%4|datetime=%5" )
                       .arg( m_settings.serviceProviderID ).arg( stop ).arg( stop2 )
                       .arg( resultCount )
                       .arg( QDateTime::currentDateTime()
                             .addSecs(data.minutesUntilFirstResult * 60).toString() );
    } else {
        QString type;
        if ( keywords.testFlag( PublicTransportRunner::Departures ) ) {
            type = "Departures";
        } else if ( keywords.testFlag( PublicTransportRunner::Arrivals ) ) {
            type = "Arrivals";
        } else if ( keywords.testFlag( PublicTransportRunner::StopSuggestions ) ) {
            type = "Stops";
        } else {
            kWarning() << "No keyword set from the term, shouldn't happen. Using 'Departures'.";
            type = "Departures";
        }

        m_sourceName = QString( "%1 %2|stop=%3|maxcount=%4|timeoffset=%5" )
                       .arg( type ).arg( m_settings.serviceProviderID ).arg( stop )
                       .arg( resultCount ).arg( data.minutesUntilFirstResult );
    }
    if ( !m_settings.city.isEmpty() ) {
        m_sourceName += QString( "|city=%1" ).arg( m_settings.city );
    }

    m_data = data;
    engine->connectSource( m_sourceName, this );

    // Start a timeout
    QTimer::singleShot( 15000, this, SLOT(abort()) );
}

PublicTransportRunnerHelper::PublicTransportRunnerHelper( PublicTransportRunner* runner ) : QObject()
{
    // The helper needs to be in the main thread of the data engine
    Q_ASSERT( QThread::currentThread() == QCoreApplication::instance()->thread() );

    qRegisterMetaType< Result >( "Result" );
    qRegisterMetaType< FinishType >( "FinishType" );
    connect( runner, SIGNAL(doMatch(PublicTransportRunner*,Plasma::DataEngine*,Plasma::RunnerContext*)),
             this, SLOT(match(PublicTransportRunner*,Plasma::DataEngine*,Plasma::RunnerContext*)) );
}

void AsyncDataEngineUpdater::dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data& data )
{
    m_runner->mutex().lock();

    // Disconnect source again, we don't need updates
    m_engine->disconnectSource( sourceName, this );

    if ( !m_context || !m_context->isValid() ) {
        kDebug() << "Context invalid" << sourceName;
        m_runner->mutex().unlock();
        emit finished( false );
        return;
    }
    m_runner->mutex().unlock();

    if ( data.value("error").toBool() ) {
        // Error while parsing the data or no connection to server
        kDebug() << "Error parsing or no connection to server";
//     handleDataError( sourceName, data );
        emit finished( false );
        return;
    } else if ( data.contains("stops") ) {
        processStopSuggestions( sourceName, data );
    } else if ( data.contains("journeys") ) {
        processJourneys( sourceName, data );
    } else if ( data.contains("departures") || data.contains("arrivals") ) {
        processDepartures( sourceName, data );
    }

    emit finished( true );
}

void AsyncDataEngineUpdater::processDepartures( const QString &sourceName,
                                                const Plasma::DataEngine::Data &data )
{
    QUrl url = data["requestUrl"].toUrl();
    QDateTime updated = data["updated"].toDateTime();
    qreal min = INT_MAX, max = 0;
    int filtered = 0;

    const QVariantHash allVehicleTypes = m_engine->query( "VehicleTypes" );
    QVariantList departuresData = data.contains("departures")
            ? data["departures"].toList() : data["arrivals"].toList();
    kDebug() << departuresData.count() << "departures to be processed";
    foreach ( const QVariant &departure, departuresData ) {
        QVariantHash departureData = departure.toHash();

        QList< QTime > routeTimes;
        if ( departureData.contains( "RouteTimes" ) ) {
            QVariantList times = departureData[ "RouteTimes" ].toList();
            foreach( const QVariant &time, times ) {
                routeTimes << time.toTime();
            }
        }
        QString operatorName = departureData["Operator"].toString();
        QString line = departureData["TransportLine"].toString();
        QString target = departureData["Target"].toString();
        QDateTime departureTime = departureData["DepartureDateTime"].toDateTime();
        VehicleType vehicleType = static_cast<VehicleType>( departureData["TypeOfVehicle"].toInt() );
        const QVariantHash vehicleData = allVehicleTypes[ QString::number(vehicleType) ].toHash();
        QString vehicle = vehicleData["name"].toString();
        QString vehicleIconName = vehicleData["iconName"].toString();
        KIcon vehicleIcon = KIcon( vehicleIconName.isEmpty()
                ? "public-transport-stop" : vehicleIconName );
//     QString nightline = dataMap["nightline"].toBool();
//     QString expressline = dataMap["expressline"].toBool();
        QString platform = departureData["Platform"].toString();
        int delay = departureData["Delay"].toInt();
        QString delayReason = departureData["DelayReason"].toString();
        QString journeyNews = departureData["JourneyNews"].toString();
        QStringList routeStops = departureData["RouteStops"].toStringList();
        int routeExactStops = departureData["RouteExactStops"].toInt();
        if ( routeExactStops < 3 ) {
            routeExactStops = 3;
        }

        // Mark departures/arrivals as filtered out that are either filtered out
        // or shouldn't be shown because of the first departure settings
        QDateTime predictedDeparture = delay > 0 ? departureTime.addSecs( delay * 60 ) : departureTime;
        if ( !isTimeShown( predictedDeparture, 0 ) ||
                ( m_data.keywords.testFlag( PublicTransportRunner::OnlyBuses ) &&
                    vehicleType != Bus ) ||
                ( m_data.keywords.testFlag( PublicTransportRunner::OnlyTrams ) &&
                    vehicleType != Tram ) ||
                ( m_data.keywords.testFlag( PublicTransportRunner::OnlyPublicTransport ) &&
                    vehicleType != Bus &&
                    vehicleType != Tram &&
                    vehicleType != Subway &&
                    vehicleType != InterurbanTrain &&
                    vehicleType != Metro &&
                    vehicleType != TrolleyBus ) ||
                ( m_data.keywords.testFlag( PublicTransportRunner::OnlyTrains ) &&
                    vehicleType != RegionalTrain &&
                    vehicleType != RegionalExpressTrain &&
                    vehicleType != InterregionalTrain &&
                    vehicleType != IntercityTrain &&
                    vehicleType != HighSpeedTrain ) )
        {
            // Go to the next departure
            kDebug() << "Filtered" << predictedDeparture << m_data.keywords;
            ++filtered;
            continue;
        }

        int minsToDeparture = QDateTime::currentDateTime().secsTo( predictedDeparture ) / 60;
        QString duration = minsToDeparture == 0 ? i18n( "now" )
                : i18nc("%1 is a formatted duration string",
                        "in %1", KGlobal::locale()->prettyFormatDuration(minsToDeparture * 60 * 1000));
        QString time = KGlobal::locale()->formatTime( predictedDeparture.time() );
        QString delayText = delay == 0
                ? ", " + i18nc("Used to indicate that a train, bus, etc. is departing/arriving on time",
                               "on schedule")
                : ( delay >= 1 ? ", " + i18nc("Delay of a train, bus, etc.", "%1 minutes late", delay) : "" );
        QString text = m_data.keywords.testFlag( PublicTransportRunner::Arrivals )
                ? i18n( "Line %1 arrives %2 (at %3%4)", line, duration, time, delayText )
                : i18n( "Line %1 departs %2 (at %3%4)", line, duration, time, delayText );

        QStringList subtexts;
        if ( !target.isEmpty() ) {
            if ( m_data.keywords.testFlag( PublicTransportRunner::Arrivals ) ) {
                subtexts << i18nc( "The origin stop of a train, bus, etc.",
                                   "Origin: %1", target );
            } else {
                subtexts << i18nc( "The target stop of a train, bus, etc.",
                                   "Target: %1", target );
            }
        }
        if ( delay >= 1 ) {
            subtexts << i18n( "Original Departure: %1",
                              KGlobal::locale()->formatTime( departureTime.time() ) );
        }
        if ( !delayReason.isEmpty() ) {
            subtexts << i18n( "Delay Reason: %1", delayReason );
        }
        if ( !platform.isEmpty() ) {
            subtexts << i18nc( "Used for showing the platform from which a train, bus, etc. departs/arrives",
                               "Platform: %1", platform );
        }
        if ( !journeyNews.isEmpty() ) {
            subtexts << i18n( "Information: %1", journeyNews );
        }
        if ( !operatorName.isEmpty() ) {
            subtexts << i18n( "Operator: %1", operatorName );
        }
        if ( !routeStops.isEmpty() ) {
            if ( m_data.keywords.testFlag( PublicTransportRunner::Arrivals ) ) {
                // Show the last routeExactStops stops including the stop to arrive at
                subtexts << i18n( "Route: %1", ((QStringList)routeStops.mid(
                        qMax( 0, routeStops.length() - routeExactStops ), routeExactStops ) ).join( " - " ) );
            } else {
                // Show the first routeExactStops stops after the stop to depart from
                subtexts << i18n( "Route: %1",
                        ((QStringList)routeStops.mid( 0, routeExactStops ) ).join( " - " ) );
            }
        }

        // Check context before generating a match
        m_runner->mutex().lock();
        if ( !m_context || !m_context->isValid() ) {
            kDebug() << "Context got invalid" << sourceName;
            m_runner->mutex().unlock();
            return;
        }
        m_runner->mutex().unlock();

        Result res;
        res.text = text;
        res.subtext = subtexts.join( "\n" );
        res.icon = vehicleIcon;
        res.url = url;
        res.relevance = -predictedDeparture.toTime_t() / 60;
        m_results << res;

        min = qMin( min, res.relevance );
        max = qMax( max, res.relevance );
    } // for ( int i = 0; i < count; ++i )

    if ( m_results.isEmpty() ) {
        // No departures found
        Result res;
        res.icon = KIcon( "public-transport-stop" );
        res.url = url;

        bool filterUsed = m_data.keywords.testFlag( PublicTransportRunner::OnlyBuses ) ||
                          m_data.keywords.testFlag( PublicTransportRunner::OnlyTrams ) ||
                          m_data.keywords.testFlag( PublicTransportRunner::OnlyPublicTransport ) ||
                          m_data.keywords.testFlag( PublicTransportRunner::OnlyTrains );

        if ( m_data.keywords.testFlag( PublicTransportRunner::Departures ) ) {
            res.text = i18n( "No departures found for the given stop" );
        } else { // if ( m_keywords.testFlag(PublicTransportRunner::Arrivals) ) {
            res.text = i18n( "No arrivals found for the given stop" );
        }
        if ( filterUsed ) {
            if ( filtered == 0 ) {
                res.subtext = i18n( "Maybe the service provider doesn't recognize the given stop name. To find valid stop names try using the 'stops' keyword." );
            } else {
                res.subtext += i18n( "A filter keyword was used that caused filtering of %1 results, try using a non-filtering keyword.", filtered );
            }
        } else if ( filtered > 0 ) {
            res.subtext += i18n( "Got %1 results in the past.", filtered );
        }
        res.relevance = 0.8;
        m_results << res;
    } else {
        normalizeRelevance( min, max );
    }
}

void AsyncDataEngineUpdater::processJourneys( const QString& sourceName, const Plasma::DataEngine::Data& data )
{
    QUrl url = data["requestUrl"].toUrl();
    QDateTime updated = data["updated"].toDateTime();
    qreal min = INT_MAX, max = 0;

    const QVariantHash allVehicleTypes = m_engine->query( "VehicleTypes" );
    QVariantList journeysData = data["journeys"].toList();
    kDebug() << "  - " << journeysData.count() << "journeys to be processed";
    int filtered = 0;
    foreach ( const QVariant &journey, journeysData ) {
        QVariantHash journeyData = journey.toHash();

//     QList<QTime> routeTimesDeparture, routeTimesArrival;
//     if ( dataMap.contains("routeTimesDeparture") ) {
//         QVariantList times = dataMap[ "routeTimesDeparture" ].toList();
//         foreach ( const QVariant &time, times )
//         routeTimesDeparture << time.toTime();
//     }
//     if ( dataMap.contains("routeTimesArrival") ) {
//         QVariantList times = dataMap[ "routeTimesArrival" ].toList();
//         foreach ( const QVariant &time, times )
//         routeTimesArrival << time.toTime();
//     }
        QStringList routeStops, routeTransportLines;
//             routePlatformsDeparture, routePlatformsArrival;
        if ( journeyData.contains( "RouteStops" ) ) {
            routeStops = journeyData[ "RouteStops" ].toStringList();
        }
        if ( journeyData.contains( "RouteTransportLines" ) ) {
            routeTransportLines = journeyData[ "RouteTransportLines" ].toStringList();
        }
//     if ( dataMap.contains("routePlatformsDeparture") ) {
//         routePlatformsDeparture = dataMap[ "routePlatformsDeparture" ].toStringList();
//     }
//     if ( dataMap.contains("routePlatformsArrival") ) {
//         routePlatformsArrival = dataMap[ "routePlatformsArrival" ].toStringList();
//     }

//     QList<int> routeTimesDepartureDelay, routeTimesArrivalDelay;
//     if ( dataMap.contains("routeTimesDepartureDelay") ) {
//         QVariantList list = dataMap[ "routeTimesDepartureDelay" ].toList();
//         foreach ( const QVariant &var, list )
//         routeTimesDepartureDelay << var.toInt();
//     }
//     if ( dataMap.contains("routeTimesArrivalDelay") ) {
//         QVariantList list = dataMap[ "routeTimesArrivalDelay" ].toList();
//         foreach ( const QVariant &var, list )
//         routeTimesArrivalDelay << var.toInt();
//     }

        QString operatorName = journeyData["Operator"].toString();
        QVariantList vehicleTypes = journeyData["VehicleTypes"].toList();
        QStringList vehicles;
        QStringList vehicleIconNames;
        foreach ( const QVariant &vehicleTypeVariant, vehicleTypes ) {
            VehicleType vehicleType = static_cast< VehicleType >( vehicleTypeVariant.toInt() );
            const QVariantHash vehicleData = allVehicleTypes[ QString::number(vehicleType) ].toHash();
            vehicles << vehicleData["name"].toString();
            vehicleIconNames << vehicleData["iconName"].toString();
        }
        KIcon icon = KIcon( vehicleIconNames.isEmpty() ? "public-transport-stop" : vehicleIconNames.first() );
        QDateTime departure = journeyData["DepartureDateTime"].toDateTime();
        QDateTime arrival = journeyData["ArrivalDateTime"].toDateTime();
        QString pricing = journeyData["Pricing"].toString();
        QString startStop = journeyData["StartStopName"].toString();
        QString targetStop = journeyData["TargetStopName"].toString();
        int journeyDuration = journeyData["Duration"].toInt();
        int changes = journeyData["Changes"].toInt();
        QString journeyNews = journeyData["JourneyNews"].toString();
//     dataMap["routeVehicleTypes"].toList()

        QDateTime predictedDeparture = departure;
        QDateTime predictedArrival = arrival;
//     QDateTime predictedDeparture = delay > 0 ? departure.addSecs(delay * 60) : departure;
//     QDateTime predictedArrival = delay > 0 ? arrival.addSecs(delay * 60) : arrival;

        if ( !isTimeShown( predictedDeparture, 0 ) ) {
            kDebug() << "Filtered" << predictedDeparture << m_data.keywords;
            ++filtered;
            continue;
        }

        int minsToDeparture = QDateTime::currentDateTime().secsTo( predictedDeparture ) / 60;
        QString durationDep = minsToDeparture == 0
                ? "" : KGlobal::locale()->prettyFormatDuration(minsToDeparture * 60 * 1000);
        int minsToArrival = QDateTime::currentDateTime().secsTo( predictedArrival ) / 60;
        QString durationArr = minsToArrival == 0
                ? "" : KGlobal::locale()->prettyFormatDuration(minsToArrival * 60 * 1000);
        QString timeDep = KGlobal::locale()->formatTime( predictedDeparture.time() );
        QString timeArr = KGlobal::locale()->formatTime( predictedArrival.time() );

        QString vehiclesString = vehicles.join( ", " );
        // Capitalize first letter
        if ( vehiclesString.length() >= 1 ) {
            vehiclesString = vehiclesString[0].toUpper() + vehiclesString.mid( 1 );
        }
        QString text = i18n( "Journey: %1", vehiclesString );

        QStringList subtexts;
        if ( predictedDeparture.isValid() ) {
            subtexts << i18n( "Departure: %1 (in %2)", timeDep, durationDep );
        }
        if ( predictedArrival.isValid() ) {
            subtexts << i18n( "Arrival: %1 (in %2)", timeArr, durationArr );
        }
        if ( journeyDuration > 0 ) {
            subtexts << i18nc( "The duration of a journey", "Duration: %1",
                               KGlobal::locale()->prettyFormatDuration(journeyDuration * 60 * 1000) );
        }
        if ( changes >= 0 ) {
            subtexts << i18nc( "The number of changes between vehicles in a journey",
                               "Changes: %1", changes );
        }
        if ( !pricing.isEmpty() ) {
            subtexts << i18nc( "The pricing of a journey",
                               "Pricing: %1", pricing );
        }
//     if ( delay >= 1 ) {
//         subtexts << i18n("Original Departure: %1",
//                  KGlobal::locale()->formatTime(departure.time()));
//     }
//     if ( !delayReason.isEmpty() ) {
//         subtexts << i18n("Delay Reason: %1", delayReason);
//     }
//     if ( !platform.isEmpty() ) {
//         subtexts << i18nc("Used for showing the platform from which a train, bus, etc. departs/arrives",
//             "Platform: %1", platform);
//     }
        if ( !journeyNews.isEmpty() ) {
            subtexts << i18n( "Information: %1", journeyNews );
        }
        if ( !operatorName.isEmpty() ) {
            subtexts << i18n( "Operator: %1", operatorName );
        }
        if ( !routeStops.isEmpty() ) {
            subtexts << i18n( "Route: %1", routeStops.join( " - " ) );
        }

        // Check context before generating a match
        m_runner->mutex().lock();
        if ( !m_context || !m_context->isValid() ) {
            m_runner->mutex().unlock();
            kDebug() << "Context got invalid" << sourceName;
            return;
        }
        m_runner->mutex().unlock();

        Result res;
        res.text = text;
        res.subtext = subtexts.join( "\n" );
        res.icon = icon;
        res.url = url;
        res.relevance = -predictedDeparture.toTime_t() / 60;
        m_results << res;
        kDebug() << text;

        min = qMin( min, res.relevance );
        max = qMax( max, res.relevance );
    } // for ( int i = 0; i < count; ++i )

    kDebug() << m_results.count() << "journeys found";
    if ( m_results.isEmpty() ) {
        // No journeys found
        Result res;
        res.icon = KIcon( "public-transport-stop" );
        res.url = url;
        /*
            bool filterUsed = m_keywords.testFlag(PublicTransportRunner::OnlyBuses) ||
                m_keywords.testFlag(PublicTransportRunner::OnlyTrams) ||
                m_keywords.testFlag(PublicTransportRunner::OnlyPublicTransport) ||
                m_keywords.testFlag(PublicTransportRunner::OnlyTrains);*/

        res.text = i18n( "No journeys found for the given stop names" );

        if ( filtered > 0 ) {
            res.subtext += i18n( "Got %1 results in the past.", filtered );
        } else {
            res.subtext = i18n( "Maybe the service provider doesn't recognize one of the given stop names. To find valid stop names try using the 'stops' keyword." );
        }
//     if ( filterUsed ) {
//         res.subtext += i18n("A filter keyword was used, try using a non-filtering keyword.");
//     }
        res.relevance = 0.8;
        m_results << res;
    } else {
        normalizeRelevance( min, max );
    }
}

void AsyncDataEngineUpdater::processStopSuggestions( const QString& sourceName,
        const Plasma::DataEngine::Data& data )
{
    Q_UNUSED( sourceName )

    // Cache stop icon for all stop suggestions
    KIcon icon( "public-transport-stop" );

    // Get all stop names, IDs, weights
    QUrl url = data["requestUrl"].toUrl();
    qreal min = INT_MAX, max = 0;
    QVariantList stops = data["stops"].toList();
    foreach ( const QVariant &stopData, stops ) {
        QVariantHash stop = stopData.toHash();
        QString stopName = stop["StopName"].toString();
        QString stopID = stop["StopID"].toString();
        int stopWeight = stop["StopWeight"].toInt();
        qreal longitude = stop["StopLongitude"].toReal();
        qreal latitude = stop["StopLatitude"].toReal();
        if ( stopWeight <= 0 ) {
            stopWeight = 0;
        }

        Result res;
        res.icon = icon;
        res.url = url;
        res.text = i18n( "Suggested Stop Name: \"%1\"", stopName );
        res.relevance = stopWeight;
        res.data["StopName"] = stopName;
        res.data["StopID"] = stopID;
        res.data["StopLongitude"] = longitude;
        res.data["StopLatitude"] = latitude;
        m_results << res;

        min = qMin( min, res.relevance );
        max = qMax( max, res.relevance );
    }

    if ( m_results.isEmpty() ) {
        // No stop suggestions found
        Result res;
        res.icon = icon;
        res.url = url;
        res.text = i18n( "No stop suggestions found, try another one" );
        res.relevance = 0.8;
        m_results << res;
    } else {
        normalizeRelevance( min, max );

        for ( int i = 0; i < m_results.length(); ++i ) {
            Result &res = m_results[i];
            res.subtext = i18n( "Relevance: %1%, Service Provider's Stop ID: %2",
                                qRound( res.relevance * 100 ), res.data["StopID"].toString() );
        }
    }
}

void AsyncDataEngineUpdater::normalizeRelevance( qreal min, qreal max )
{
    const qreal span = max - min;
    if ( qFuzzyIsNull( span ) ) {
        // Maximum and minimum relevance are (almost) equal
        for ( int i = 0; i < m_results.length(); ++i ) {
            m_results[i].relevance = 0.8;
        }
    } else {
        const qreal targetMin = 0.6;
        const qreal targetMax = 1.0;
        for ( int i = 0; i < m_results.length(); ++i ) {
            m_results[i].relevance = targetMin + ( targetMax - targetMin ) *
                                     ( m_results[i].relevance - min ) / span;
        }
    }
}

bool AsyncDataEngineUpdater::isTimeShown( const QDateTime& dateTime, int timeOffsetOfFirstDeparture )
{
    QDateTime firstDepartureTime = QDateTime::currentDateTime();
    int secsToDepartureTime = firstDepartureTime.secsTo( dateTime )
                              - timeOffsetOfFirstDeparture * 60;
    if ( -secsToDepartureTime / 3600 >= 23 ) { // for departures with guessed date
        secsToDepartureTime += 24 * 3600;
    }
    return secsToDepartureTime > -60;
}

#include "publictransportrunner.moc"
