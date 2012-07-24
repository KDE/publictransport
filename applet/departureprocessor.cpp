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
#include "departureprocessor.h"

// KDE includes
#include <KDebug>

// Qt includes
#include <QMutex> // Member variable
#include <QUrl>

const int DepartureProcessor::DEPARTURE_BATCH_SIZE = 10;
const int DepartureProcessor::JOURNEY_BATCH_SIZE = 10;

DepartureProcessor::DepartureProcessor( QObject* parent )
        : QThread( parent ), m_mutex(new QMutex())
{
    m_currentJob = NoJob;
    m_quit = false;
    m_abortCurrentJob = false;
    m_requeueCurrentJob = false;
    m_timeOffsetOfFirstDeparture = 0;
    m_isArrival = false;
    qRegisterMetaType< QList<DepartureInfo> >( "QList<DepartureInfo>" );
    qRegisterMetaType< QList<JourneyInfo> >( "QList<JourneyInfo>" );
}

DepartureProcessor::~DepartureProcessor()
{
    m_mutex->lock();
    m_quit = true;
    m_abortCurrentJob = true;
    m_cond.wakeOne();
    m_mutex->unlock();
    wait();
    delete m_mutex;
}

void DepartureProcessor::abortJobs( DepartureProcessor::JobTypes jobTypes )
{
    QMutexLocker locker( m_mutex );

    if ( jobTypes.testFlag( m_currentJob ) ) {
        m_abortCurrentJob = true;
    }

    if ( jobTypes == AllJobs ) {
        // Clear all jobs in the queue
        m_jobQueue.clear();
    } else {
        // Remove jobs of the given types
        for ( int i = m_jobQueue.count() - 1; i >= 0; --i ) {
            if ( jobTypes.testFlag( m_jobQueue[i]->type ) ) {
                m_jobQueue.removeAt( i );
            }
        }
    }
}

void DepartureProcessor::setFilters( const FilterSettingsList &filters )
{
    QMutexLocker locker( m_mutex );
    m_filters = filters;

    if ( m_currentJob == ProcessDepartures && !m_jobQueue.isEmpty() ) {
        m_requeueCurrentJob = true;
    }
}

void DepartureProcessor::setColorGroups( const ColorGroupSettingsList& colorGroups )
{
    QMutexLocker locker( m_mutex );
    m_colorGroups = colorGroups;

    if ( m_currentJob == ProcessDepartures && !m_jobQueue.isEmpty() ) {
        m_requeueCurrentJob = true;
    }
}

void DepartureProcessor::setFirstDepartureSettings(
        FirstDepartureConfigMode firstDepartureConfigMode, const QTime& timeOfFirstDepartureCustom,
        int timeOffsetOfFirstDeparture )
{
    QMutexLocker locker( m_mutex );
    m_firstDepartureConfigMode = firstDepartureConfigMode;
    m_timeOfFirstDepartureCustom = timeOfFirstDepartureCustom;
    m_timeOffsetOfFirstDeparture = timeOffsetOfFirstDeparture;
}

void DepartureProcessor::setDepartureArrivalListType( DepartureArrivalListType type )
{
    m_isArrival = type == ArrivalList;
}

void DepartureProcessor::setAlarms( const AlarmSettingsList &alarms )
{
    QMutexLocker locker( m_mutex );
    m_alarms = alarms;

    if ( m_currentJob == ProcessDepartures && !m_jobQueue.isEmpty() ) {
        m_requeueCurrentJob = true;
    }
}

bool DepartureProcessor::isTimeShown( const QDateTime& dateTime,
        FirstDepartureConfigMode firstDepartureConfigMode,
        const QTime &timeOfFirstDepartureCustom,
        int timeOffsetOfFirstDeparture )
{
    QDateTime firstDepartureTime = firstDepartureConfigMode == AtCustomTime
            ? QDateTime( QDate::currentDate(), timeOfFirstDepartureCustom )
            : QDateTime::currentDateTime();
    int secsToDepartureTime = firstDepartureTime.secsTo( dateTime );
    if ( firstDepartureConfigMode == RelativeToCurrentTime ) {
        secsToDepartureTime -= timeOffsetOfFirstDeparture * 60;
    }
    if ( -secsToDepartureTime / 3600 >= 23 ) { // for departures with guessed date
        secsToDepartureTime += 24 * 3600;
    }
    return secsToDepartureTime > -60;
}

void DepartureProcessor::startOrEnqueueJob( DepartureProcessor::JobInfo *job )
{
    m_jobQueue.enqueue( job );

    if ( !isRunning() ) {
        start();
    } else {
        m_cond.wakeOne();
    }
}

void DepartureProcessor::processDepartures( const QString &sourceName, const QVariantHash& data )
{
    QMutexLocker locker( m_mutex );
    DepartureJobInfo *job = new DepartureJobInfo();
    job->sourceName = sourceName;
    job->data = data;
    startOrEnqueueJob( job );
}

void DepartureProcessor::processJourneys( const QString& sourceName, const QVariantHash& data )
{
    QMutexLocker locker( m_mutex );
    JourneyJobInfo *job = new JourneyJobInfo();
    job->sourceName = sourceName;
    job->data = data;
    startOrEnqueueJob( job );
}

void DepartureProcessor::filterDepartures( const QString &sourceName,
        const QList< DepartureInfo > &departures, const QList< uint > &shownDepartures )
{
    QMutexLocker locker( m_mutex );
    FilterJobInfo *job = new FilterJobInfo();
    job->sourceName = sourceName;
    job->departures = departures;
    job->shownDepartures = shownDepartures; // TODO
    startOrEnqueueJob( job );
}

void DepartureProcessor::run()
{
    forever {
        m_mutex->lock();
        while ( m_jobQueue.isEmpty() ) {
            if ( m_quit ) {
                break;
            }
            kDebug() << "Waiting for new jobs...";
            m_currentJob = NoJob;
            m_cond.wait( m_mutex );
        }
        if ( m_quit ) {
            break;
        }

        JobInfo *job = m_jobQueue.dequeue();
        m_currentJob = job->type;
        m_mutex->unlock();

        QTime time;
        time.start();
        if ( job->type == ProcessDepartures ) {
            doDepartureJob( static_cast<DepartureJobInfo*>( job ) );
        } else if ( job->type == FilterDepartures ) {
            doFilterJob( static_cast<FilterJobInfo*>( job ) );
        } else if ( job->type == ProcessJourneys ) {
            doJourneyJob( static_cast<JourneyJobInfo*>( job ) );
        }
        kDebug() << " > Job" << job->type << "finished after"
                 << (time.elapsed() / 1000.0) << "seconds";

        m_mutex->lock();
        if ( !m_requeueCurrentJob ) {
            delete job;
        } else {
            kDebug() << "  .. requeue job ..";
        }
        m_abortCurrentJob = false;
        m_requeueCurrentJob = false;

        if ( m_quit ) {
            break;
        } else if ( m_jobQueue.isEmpty() ) {
            // Wait if there are no more jobs in the queue
            m_currentJob = NoJob;
            m_cond.wait( m_mutex );
        }
        m_mutex->unlock();
    }

    qDeleteAll( m_jobQueue );
    kDebug() << "Thread terminated";
}

void DepartureProcessor::doDepartureJob( DepartureProcessor::DepartureJobInfo* departureJob )
{
    const QString sourceName = departureJob->sourceName;
    QVariantHash data = departureJob->data;

    m_mutex->lock();
    FilterSettingsList filters = m_filters;
    ColorGroupSettingsList colorGroups = m_colorGroups;
    AlarmSettingsList alarms = m_alarms;

    FirstDepartureConfigMode firstDepartureConfigMode = m_firstDepartureConfigMode;
    const QTime &timeOfFirstDepartureCustom = m_timeOfFirstDepartureCustom;
    int timeOffsetOfFirstDeparture = m_timeOffsetOfFirstDeparture;
    bool isArrival = m_isArrival;
    m_mutex->unlock();

    emit beginDepartureProcessing( sourceName );

    QUrl url;
    QDateTime updated;
    QList< DepartureInfo > departureInfos/*, alarmDepartures*/;
    url = data["requestUrl"].toUrl();
    updated = data["updated"].toDateTime();
    QVariantList departuresData = data.contains("departures") ? data["departures"].toList()
                                                              : data["arrivals"].toList();

//     Q_ASSERT( departureJob->alreadyProcessed <= count );
    kDebug() << "  - " << departuresData.count() << "departures to be processed";
    for ( int i = departureJob->alreadyProcessed; i < departuresData.count(); ++i ) {
        QVariantHash departureData = departuresData[ i ].toHash();
        QList< QTime > routeTimes;
        if ( departureData.contains("RouteTimes") ) {
            QVariantList times = departureData[ "RouteTimes" ].toList();
            foreach( const QVariant &time, times ) {
                routeTimes << time.toTime();
            }
        }
        DepartureInfo departureInfo( sourceName, i, departureData["Operator"].toString(),
                departureData["TransportLine"].toString(),
                departureData["Target"].toString(), departureData["TargetShortened"].toString(),
                departureData["DepartureDateTime"].toDateTime(),
                static_cast<VehicleType>( departureData["TypeOfVehicle"].toInt() ),
                departureData["Nightline"].toBool(), departureData["Expressline"].toBool(),
                departureData["Platform"].toString(), departureData["Delay"].toInt(),
                departureData["DelayReason"].toString(), departureData["JourneyNews"].toString(),
                departureData["RouteStops"].toStringList(),
                departureData["RouteStopsShortened"].toStringList(),
                routeTimes, departureData["RouteExactStops"].toInt(), isArrival );

        // Update the list of alarms that match the current departure
        departureInfo.matchedAlarms().clear();
        for ( int a = 0; a < alarms.count(); ++a ) {
            AlarmSettings alarm = alarms.at( a );
            if ( alarm.enabled && alarm.filter.match( departureInfo ) ) {
                departureInfo.matchedAlarms() << a;
//             if ( !alarmDepartures.contains(departureInfo) )
//                 alarmDepartures << departureInfo;
            }
        }

        // Mark departures/arrivals as filtered out that are either filtered out
        // or shouldn't be shown because of the first departure settings
        if ( !isTimeShown(departureInfo.predictedDeparture(), firstDepartureConfigMode,
                          timeOfFirstDepartureCustom, timeOffsetOfFirstDeparture)
             || filters.filterOut(departureInfo)
             || colorGroups.filterOut(departureInfo) )
        {
            departureInfo.setFilteredOut( true );
//          kDebug() << "Filter out" << filters.filterOut(departureInfo)
//              << Global::vehicleTypeToString(departureInfo.vehicleType()) << departureInfo.lineString()
//              << departureInfo.target();
        }
        departureInfos << departureInfo;

        if ( departureInfos.count() == DEPARTURE_BATCH_SIZE ) {
            QMutexLocker locker( m_mutex );
            if ( m_abortCurrentJob ) {
                break;
            } else {
                emit departuresProcessed( sourceName, departureInfos, url, updated,
                                          departuresData.count() - i - 1 );
                departureInfos.clear();
//                 alarmDepartures.clear();
            }

            if ( m_requeueCurrentJob ) {
                departureJob->alreadyProcessed = i + 1;
                m_jobQueue << departureJob;
                break;
            }
        }
    } // for ( int i = 0; i < count; ++i )

    // Emit remaining departures
    m_mutex->lock();
    if ( !m_abortCurrentJob && !departureInfos.isEmpty() ) {
        emit departuresProcessed( sourceName, departureInfos, url, updated, 0 );
    }
    m_mutex->unlock();
}

void DepartureProcessor::doJourneyJob( DepartureProcessor::JourneyJobInfo* journeyJob )
{
    const QString sourceName = journeyJob->sourceName;
    QVariantHash data = journeyJob->data;

    m_mutex->lock();
    AlarmSettingsList alarms = m_alarms;
    m_mutex->unlock();

    emit beginJourneyProcessing( sourceName );

    QList< JourneyInfo > journeyInfos;
    QUrl url = data["requestUrl"].toUrl();
    QDateTime updated = data["updated"].toDateTime();
    QVariantList journeysData = data["journeys"].toList();
    if ( journeyJob->alreadyProcessed > journeysData.count() ) {
        kDebug() << "There was an error with the journey data source (journeyJob->alreadyProcessed > count)";
    }
    kDebug() << "  - " << journeysData.count() << "journeys to be processed";
    for ( int i = journeyJob->alreadyProcessed; i < journeysData.count(); ++i ) {
        QVariantHash journeyData = journeysData[i].toHash();
        QList<QTime> routeTimesDeparture, routeTimesArrival;
        if ( journeyData.contains( "RouteTimesDeparture" ) ) {
            QVariantList times = journeyData[ "RouteTimesDeparture" ].toList();
            foreach( const QVariant &time, times )
            routeTimesDeparture << time.toTime();
        }
        if ( journeyData.contains( "RouteTimesArrival" ) ) {
            QVariantList times = journeyData[ "RouteTimesArrival" ].toList();
            foreach( const QVariant &time, times )
            routeTimesArrival << time.toTime();
        }
        QStringList routeTransportLines,
        routePlatformsDeparture, routePlatformsArrival;
        if ( journeyData.contains( "RouteTransportLines" ) ) {
            routeTransportLines = journeyData[ "RouteTransportLines" ].toStringList();
        }
        if ( journeyData.contains( "RoutePlatformsDeparture" ) ) {
            routePlatformsDeparture = journeyData[ "RoutePlatformsDeparture" ].toStringList();
        }
        if ( journeyData.contains( "RoutePlatformsArrival" ) ) {
            routePlatformsArrival = journeyData[ "RoutePlatformsArrival" ].toStringList();
        }

        QList<int> routeTimesDepartureDelay, routeTimesArrivalDelay;
        if ( journeyData.contains( "RouteTimesDepartureDelay" ) ) {
            QVariantList list = journeyData[ "RouteTimesDepartureDelay" ].toList();
            foreach( const QVariant &var, list ) {
                routeTimesDepartureDelay << var.toInt();
            }
        }
        if ( journeyData.contains( "RouteTimesArrivalDelay" ) ) {
            QVariantList list = journeyData[ "RouteTimesArrivalDelay" ].toList();
            foreach( const QVariant &var, list ) {
                routeTimesArrivalDelay << var.toInt();
            }
        }

//         TODO
        JourneyInfo journeyInfo( journeyData["Operator"].toString(),
                                 journeyData["TypesOfVehicleInJourney"].toList(),
                                 journeyData["DepartureDateTime"].toDateTime(),
                                 journeyData["ArrivalDateTime"].toDateTime(),
                                 journeyData["Pricing"].toString(),
                                 journeyData["StartStopName"].toString(),
                                 journeyData["TargetStopName"].toString(),
                                 journeyData["Duration"].toInt(),
                                 journeyData["Changes"].toInt(),
                                 journeyData["JourneyNews"].toString(),
                                 journeyData["RouteStops"].toStringList(),
                                 journeyData["RouteStopsShortened"].toStringList(),
                                 routeTransportLines, routePlatformsDeparture,
                                 routePlatformsArrival,
                                 journeyData["RouteVehicleTypes"].toList(),
                                 routeTimesDeparture, routeTimesArrival,
                                 routeTimesDepartureDelay, routeTimesArrivalDelay );

        // TODO Use a dummy DepartureInfo that "mimics" the first journey part of the current journey
//         QString lineString = journeyInfo.routeTransportLines().isEmpty()
//                 ? QString() : journeyInfo.routeTransportLines().first();
//         VehicleType vehicleType = journeyInfo.routeVehicleTypes().isEmpty()
//                 ? Unknown : journeyInfo.routeVehicleTypes().first();
//         DepartureInfo departureInfo( QString(), lineString, QString(), journeyInfo.departure(),
//                                      vehicleType );
//         journeyInfo.matchedAlarms().clear();
//         for ( int a = 0; a < alarms.count(); ++a ) {
//             AlarmSettings alarm = alarms.at( a );
//             if ( alarm.enabled && alarm.filter.match(departureInfo) ) {
//                 journeyInfo.matchedAlarms() << a;
// //          if ( !alarmDepartures.contains(departureInfo) )
// //              alarmDepartures << departureInfo;
//             }
//         }

        journeyInfos << journeyInfo;
        if ( journeyInfos.count() == JOURNEY_BATCH_SIZE ) {
            QMutexLocker locker( m_mutex );
            if ( m_abortCurrentJob ) {
                break;
            } else {
                emit journeysProcessed( sourceName, journeyInfos, url, updated );
                journeyInfos.clear();
            }

            if ( m_requeueCurrentJob ) {
                journeyJob->alreadyProcessed = i + 1;
                m_jobQueue << journeyJob;
                break;
            }
        }
    }

    // Emit remaining journeys
    m_mutex->lock();
    if ( !m_abortCurrentJob && !journeyInfos.isEmpty() ) {
        emit journeysProcessed( sourceName, journeyInfos, url, updated );
    }
    m_mutex->unlock();
}

void DepartureProcessor::doFilterJob( DepartureProcessor::FilterJobInfo* filterJob )
{
    QList< DepartureInfo > departures = filterJob->departures;
    QList< DepartureInfo > newlyFiltered, newlyNotFiltered;

    m_mutex->lock();
    FilterSettingsList filters = m_filters;
    ColorGroupSettingsList colorGroups = m_colorGroups;

    FirstDepartureConfigMode firstDepartureConfigMode = m_firstDepartureConfigMode;
    const QTime &timeOfFirstDepartureCustom = m_timeOfFirstDepartureCustom;
    int timeOffsetOfFirstDeparture = m_timeOffsetOfFirstDeparture;
    m_mutex->unlock();

    emit beginFiltering( filterJob->sourceName );
    kDebug() << "  - " << departures.count() << "departures to be filtered";
    for ( int i = 0; i < departures.count(); ++i ) {
        DepartureInfo &departureInfo = departures[ i ];
        const bool filterOut = filters.filterOut( departureInfo )
                || colorGroups.filterOut( departureInfo );

        // Newly filtered departures are now filtered out and were shown.
        // They may be newly filtered if they weren't filtered out, but
        // they may haven't been shown nevertheless, because the maximum
        // departure count was exceeded.
        if ( filterOut && !departureInfo.isFilteredOut()
                    && filterJob->shownDepartures.contains(departureInfo.hash()) ) {
            newlyFiltered << departureInfo;

            // Newly not filtered departures are now not filtered out and
            // weren't shown, ie. were filtered out.
            // Newly not filtered departures also need to be shown with the current
            // first departure settings
        } else if ( !filterOut
                    && ( departureInfo.isFilteredOut()
                        || !filterJob->shownDepartures.contains(departureInfo.hash()) )
                    && isTimeShown(departureInfo.predictedDeparture(),
                                firstDepartureConfigMode, timeOfFirstDepartureCustom,
                                timeOffsetOfFirstDeparture) ) {
            newlyNotFiltered << departureInfo;
        }
        departureInfo.setFilteredOut( filterOut );
    }

    m_mutex->lock();
    if ( !m_abortCurrentJob ) {
        emit departuresFiltered( filterJob->sourceName, departures, newlyFiltered, newlyNotFiltered );
    }
    m_mutex->unlock();
}

QDebug& operator <<( QDebug debug, DepartureProcessor::JobType jobType )
{
    switch ( jobType ) {
    case DepartureProcessor::ProcessDepartures:
        return debug << "ProcessDepartures";
    case DepartureProcessor::ProcessJourneys:
        return debug << "ProcessJourneys";
    case DepartureProcessor::FilterDepartures:
        return debug << "FilterDepartures";

    default:
        return debug << "Job type unknown!" << static_cast< int >( jobType );
    }
}
