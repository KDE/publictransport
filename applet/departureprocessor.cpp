/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

#include "departureprocessor.h"

#include <KDebug>
#include <QUrl>

const int DepartureProcessor::DEPARTURE_BATCH_SIZE = 10;
const int DepartureProcessor::JOURNEY_BATCH_SIZE = 3;

DepartureProcessor::DepartureProcessor( QObject* parent ) : QThread( parent )
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
    m_mutex.lock();
    m_quit = true;
    m_abortCurrentJob = true;
    m_cond.wakeOne();
    m_mutex.unlock();
    wait();
}

void DepartureProcessor::abortJobs( DepartureProcessor::JobTypes jobTypes )
{
    QMutexLocker locker( &m_mutex );

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

void DepartureProcessor::setFilterSettings( const FilterSettingsList &filterSettings )
{
    QMutexLocker locker( &m_mutex );
    m_filterSettings = filterSettings;

    if ( m_currentJob == ProcessDepartures && !m_jobQueue.isEmpty() ) {
        m_requeueCurrentJob = true;
    }
}

void DepartureProcessor::setColorGroups( const ColorGroupSettingsList& colorGroupSettings )
{
    QMutexLocker locker( &m_mutex );
    m_colorGroupSettings = colorGroupSettings;

    if ( m_currentJob == ProcessDepartures && !m_jobQueue.isEmpty() ) {
        m_requeueCurrentJob = true;
    }
}

void DepartureProcessor::setFirstDepartureSettings(
        FirstDepartureConfigMode firstDepartureConfigMode, const QTime& timeOfFirstDepartureCustom,
        int timeOffsetOfFirstDeparture, bool arrival )
{
    QMutexLocker locker( &m_mutex );
    m_firstDepartureConfigMode = firstDepartureConfigMode;
    m_timeOfFirstDepartureCustom = timeOfFirstDepartureCustom;
    m_timeOffsetOfFirstDeparture = timeOffsetOfFirstDeparture;
    m_isArrival = arrival;
}

void DepartureProcessor::setAlarmSettings( const AlarmSettingsList& alarmSettings )
{
    QMutexLocker locker( &m_mutex );
    m_alarmSettings = alarmSettings;

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
    QMutexLocker locker( &m_mutex );
    DepartureJobInfo *job = new DepartureJobInfo();
    job->sourceName = sourceName;
    job->data = data;
    startOrEnqueueJob( job );
}

void DepartureProcessor::processJourneys( const QString& sourceName, const QVariantHash& data )
{
    QMutexLocker locker( &m_mutex );
    JourneyJobInfo *job = new JourneyJobInfo();
    job->sourceName = sourceName;
    job->data = data;
    startOrEnqueueJob( job );
}

void DepartureProcessor::filterDepartures( const QString &sourceName,
        const QList< DepartureInfo > &departures, const QList< uint > &shownDepartures )
{
    QMutexLocker locker( &m_mutex );
    FilterJobInfo *job = new FilterJobInfo();
    job->sourceName = sourceName;
    job->departures = departures;
    job->shownDepartures = shownDepartures; // TODO
    startOrEnqueueJob( job );
}

void DepartureProcessor::run()
{
    while ( !m_quit ) {
        m_mutex.lock();
        JobInfo *job = m_jobQueue.dequeue();
        m_currentJob = job->type;
        m_mutex.unlock();

        kDebug() << "Start job" << job->type;
        QTime start = QTime::currentTime();
        if ( job->type == ProcessDepartures ) {
            doDepartureJob( static_cast<DepartureJobInfo*>( job ) );
        } else if ( job->type == FilterDepartures ) {
            doFilterJob( static_cast<FilterJobInfo*>( job ) );
        } else if ( job->type == ProcessJourneys ) {
            doJourneyJob( static_cast<JourneyJobInfo*>( job ) );
        }
        int time = start.msecsTo( QTime::currentTime() );
        kDebug() << "  - Took" << ( time / 1000.0 ) << "seconds";

        m_mutex.lock();
        if ( !m_requeueCurrentJob ) {
            kDebug() << "  Completed job" << job->type;
            delete job;
        } else {
            kDebug() << "  .. requeue job ..";
        }

        m_abortCurrentJob = false;
        m_requeueCurrentJob = false;
        // Wait if there are no more jobs in the queue
        if ( m_jobQueue.isEmpty() ) {
            kDebug() << "Waiting for new jobs...";
            m_currentJob = NoJob;
            m_cond.wait( &m_mutex );
        }
        m_mutex.unlock();
    }

    qDeleteAll( m_jobQueue );
    kDebug() << "Thread terminated";
}

void DepartureProcessor::doDepartureJob( DepartureProcessor::DepartureJobInfo* departureJob )
{
    const QString sourceName = departureJob->sourceName;
    QVariantHash data = departureJob->data;

    m_mutex.lock();
    FilterSettingsList filterSettings = m_filterSettings;
    ColorGroupSettingsList colorGroupSettings = m_colorGroupSettings;
    AlarmSettingsList alarmSettingsList = m_alarmSettings;

    FirstDepartureConfigMode firstDepartureConfigMode = m_firstDepartureConfigMode;
    const QTime &timeOfFirstDepartureCustom = m_timeOfFirstDepartureCustom;
    int timeOffsetOfFirstDeparture = m_timeOffsetOfFirstDeparture;
    bool isArrival = m_isArrival;
    m_mutex.unlock();

    emit beginDepartureProcessing( sourceName );

    QUrl url;
    QDateTime updated;
    QList< DepartureInfo > departureInfos/*, alarmDepartures*/;
    url = data["requestUrl"].toUrl();
    updated = data["updated"].toDateTime();
    int count = data["count"].toInt();
//     Q_ASSERT( departureJob->alreadyProcessed <= count );
    kDebug() << "  - " << count << "departures to be processed";
    for ( int i = departureJob->alreadyProcessed; i < count; ++i ) {
        QVariant departureData = data.value( QString::number( i ) );
        // Don't process invalid data
        if ( !departureData.isValid() ) {
            kDebug() << "Departure data for departure" << i << "is invalid" << data;
            continue;
        }

        QVariantHash dataMap = departureData.toHash();
        QList< QTime > routeTimes;
        if ( dataMap.contains("routeTimes") ) {
            QVariantList times = dataMap[ "routeTimes" ].toList();
            foreach( const QVariant &time, times ) {
                routeTimes << time.toTime();
            }
        }
        DepartureInfo departureInfo( dataMap["operator"].toString(), dataMap["line"].toString(),
                dataMap["target"].toString(), dataMap["targetShortened"].toString(),
                dataMap["departure"].toDateTime(),
                static_cast<VehicleType>( dataMap["vehicleType"].toInt() ),
                dataMap["nightline"].toBool(), dataMap["expressline"].toBool(),
                dataMap["platform"].toString(), dataMap["delay"].toInt(),
                dataMap["delayReason"].toString(), dataMap["journeyNews"].toString(),
                dataMap["routeStops"].toStringList(),
                dataMap["routeStopsShortened"].toStringList(),
                routeTimes, dataMap["routeExactStops"].toInt(), isArrival );

        // Update the list of alarms that match the current departure
        departureInfo.matchedAlarms().clear();
        for ( int a = 0; a < alarmSettingsList.count(); ++a ) {
            AlarmSettings alarmSettings = alarmSettingsList.at( a );
            if ( alarmSettings.enabled && alarmSettings.filter.match( departureInfo ) ) {
                departureInfo.matchedAlarms() << a;
//             if ( !alarmDepartures.contains(departureInfo) )
//                 alarmDepartures << departureInfo;
            }
        }

        // Mark departures/arrivals as filtered out that are either filtered out
        // or shouldn't be shown because of the first departure settings
        if ( !isTimeShown(departureInfo.predictedDeparture(), firstDepartureConfigMode,
                          timeOfFirstDepartureCustom, timeOffsetOfFirstDeparture)
             || filterSettings.filterOut(departureInfo)
             || colorGroupSettings.filterOut(departureInfo) )
        {
            departureInfo.setFilteredOut( true );
//          kDebug() << "Filter out" << filterSettings.filterOut(departureInfo)
//              << Global::vehicleTypeToString(departureInfo.vehicleType()) << departureInfo.lineString()
//              << departureInfo.target();
        }
        departureInfos << departureInfo;

        if ( departureInfos.count() == DEPARTURE_BATCH_SIZE ) {
            QMutexLocker locker( &m_mutex );
            if ( m_abortCurrentJob ) {
                break;
            } else {
                emit departuresProcessed( sourceName, departureInfos, url, updated, count - i - 1 );
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
    m_mutex.lock();
    if ( !m_abortCurrentJob && !departureInfos.isEmpty() ) {
        emit departuresProcessed( sourceName, departureInfos, url, updated, 0 );
    }
    m_mutex.unlock();
}

void DepartureProcessor::doJourneyJob( DepartureProcessor::JourneyJobInfo* journeyJob )
{
    const QString sourceName = journeyJob->sourceName;
    QVariantHash data = journeyJob->data;

    m_mutex.lock();
    AlarmSettingsList alarmSettingsList = m_alarmSettings;
    m_mutex.unlock();

    emit beginJourneyProcessing( sourceName );

    QList< JourneyInfo > journeyInfos;
    QUrl url = data["requestUrl"].toUrl();
    QDateTime updated = data["updated"].toDateTime();
    int count = data["count"].toInt();
    if ( journeyJob->alreadyProcessed > count ) {
        kDebug() << "There was an error with the journey data source (journeyJob->alreadyProcessed > count)";
    }
    kDebug() << "  - " << count << "journeys to be processed";
    for ( int i = journeyJob->alreadyProcessed; i < count; ++i ) {
        QVariant journeyData = data.value( QString::number( i ) );
        if ( !journeyData.isValid() ) {
            kDebug() << "Journey data invalid";
            break;
        }

        QVariantHash dataMap = journeyData.toHash();
        QList<QTime> routeTimesDeparture, routeTimesArrival;
        if ( dataMap.contains( "routeTimesDeparture" ) ) {
            QVariantList times = dataMap[ "routeTimesDeparture" ].toList();
            foreach( const QVariant &time, times )
            routeTimesDeparture << time.toTime();
        }
        if ( dataMap.contains( "routeTimesArrival" ) ) {
            QVariantList times = dataMap[ "routeTimesArrival" ].toList();
            foreach( const QVariant &time, times )
            routeTimesArrival << time.toTime();
        }
        QStringList routeTransportLines,
        routePlatformsDeparture, routePlatformsArrival;
        if ( dataMap.contains( "routeTransportLines" ) ) {
            routeTransportLines = dataMap[ "routeTransportLines" ].toStringList();
        }
        if ( dataMap.contains( "routePlatformsDeparture" ) ) {
            routePlatformsDeparture = dataMap[ "routePlatformsDeparture" ].toStringList();
        }
        if ( dataMap.contains( "routePlatformsArrival" ) ) {
            routePlatformsArrival = dataMap[ "routePlatformsArrival" ].toStringList();
        }

        QList<int> routeTimesDepartureDelay, routeTimesArrivalDelay;
        if ( dataMap.contains( "routeTimesDepartureDelay" ) ) {
            QVariantList list = dataMap[ "routeTimesDepartureDelay" ].toList();
            foreach( const QVariant &var, list ) {
                routeTimesDepartureDelay << var.toInt();
            }
        }
        if ( dataMap.contains( "routeTimesArrivalDelay" ) ) {
            QVariantList list = dataMap[ "routeTimesArrivalDelay" ].toList();
            foreach( const QVariant &var, list ) {
                routeTimesArrivalDelay << var.toInt();
            }
        }

        JourneyInfo journeyInfo( dataMap["operator"].toString(), dataMap["vehicleTypes"].toList(),
                dataMap["departure"].toDateTime(), dataMap["arrival"].toDateTime(),
                dataMap["pricing"].toString(), dataMap["startStopName"].toString(),
                dataMap["targetStopName"].toString(), dataMap["duration"].toInt(),
                dataMap["changes"].toInt(), dataMap["journeyNews"].toString(),
                dataMap["routeStops"].toStringList(), dataMap["routeStopsShortened"].toStringList(),
                routeTransportLines, routePlatformsDeparture, routePlatformsArrival,
                dataMap["routeVehicleTypes"].toList(), routeTimesDeparture, routeTimesArrival,
                routeTimesDepartureDelay, routeTimesArrivalDelay );

        // TODO Use a dummy DepartureInfo that "mimics" the first journey part of the current journey
//         QString lineString = journeyInfo.routeTransportLines().isEmpty()
//                 ? QString() : journeyInfo.routeTransportLines().first();
//         VehicleType vehicleType = journeyInfo.routeVehicleTypes().isEmpty()
//                 ? Unknown : journeyInfo.routeVehicleTypes().first();
//         DepartureInfo departureInfo( QString(), lineString, QString(), journeyInfo.departure(),
//                                      vehicleType );
//         journeyInfo.matchedAlarms().clear();
//         for ( int a = 0; a < alarmSettingsList.count(); ++a ) {
//             AlarmSettings alarmSettings = alarmSettingsList.at( a );
//             if ( alarmSettings.enabled && alarmSettings.filter.match(departureInfo) ) {
//                 journeyInfo.matchedAlarms() << a;
// //          if ( !alarmDepartures.contains(departureInfo) )
// //              alarmDepartures << departureInfo;
//             }
//         }

        journeyInfos << journeyInfo;
        if ( journeyInfos.count() == JOURNEY_BATCH_SIZE ) {
            QMutexLocker locker( &m_mutex );
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
    } // for ( int i = 0; i < count; ++i )

    // Emit remaining journeys
    m_mutex.lock();
    if ( !m_abortCurrentJob && !journeyInfos.isEmpty() ) {
        emit journeysProcessed( sourceName, journeyInfos, url, updated );
    }
    m_mutex.unlock();
}

void DepartureProcessor::doFilterJob( DepartureProcessor::FilterJobInfo* filterJob )
{
    QList< DepartureInfo > departures = filterJob->departures;
    QList< DepartureInfo > newlyFiltered, newlyNotFiltered;

    m_mutex.lock();
    FilterSettingsList filterSettings = m_filterSettings;
    ColorGroupSettingsList colorGroupSettings = m_colorGroupSettings;

    FirstDepartureConfigMode firstDepartureConfigMode = m_firstDepartureConfigMode;
    const QTime &timeOfFirstDepartureCustom = m_timeOfFirstDepartureCustom;
    int timeOffsetOfFirstDeparture = m_timeOffsetOfFirstDeparture;
    m_mutex.unlock();

    emit beginFiltering( filterJob->sourceName );
    kDebug() << "  - " << departures.count() << "departures to be filtered";
    for ( int i = 0; i < departures.count(); ++i ) {
        DepartureInfo &departureInfo = departures[ i ];
        const bool filterOut = filterSettings.filterOut( departureInfo )
                || colorGroupSettings.filterOut( departureInfo );

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

    m_mutex.lock();
    if ( !m_abortCurrentJob ) {
        emit departuresFiltered( filterJob->sourceName, departures, newlyFiltered, newlyNotFiltered );
    }
    m_mutex.unlock();
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
