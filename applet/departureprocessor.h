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

/** @file
 * @brief This file contains the QThread-derived class DepartureProcessor to process data from the public transport data engine.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef DEPARTUREPROCESSOR_HEADER
#define DEPARTUREPROCESSOR_HEADER

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>

#include <departureinfo.h>
#include <filter.h>
#include "settings.h"

/**
 * @brief Worker thread for PublicTransport
 *
 * A worker thread that puts data from the publicTransport data engine into DepartureInfo/
 * JourneyInfo instances. It also applies filters and checks if alarm filters match.
 * Filters are given as FilterSettings by @ref setFilterSettings. They contain a list of filters
 * which get OR combined. Each filter has a list of constraints which get AND combined. This could
 * take some time with complex filter settings and a long list of departures/arrivals. That's
 * actually the main reason to do this in a thread.
 * To ensure that only departures get marked to be shown which departure time is greater than or
 * equal to the first departure time, use @ref setFirstDepartureSettings. The thread uses a job
 * queue, jobs can be cancelled by their type using @ref abortJobs. To add a new job to the queue
 * use @ref processDepartures, @ref processJourneys or @ref filterDepartures.
 *
 * @ingroup models
 **/
class DepartureProcessor : public QThread {
    Q_OBJECT

public:
    /** @brief Types of jobs. */
    enum JobType {
        NoJob = 0x00, /**< No job. This is returned by @ref currentJob if the
            * thread is idle. */
        ProcessDepartures = 0x01, /**< Processing departures, ie. putting data
            * from the publicTransport data engine into DepartureInfo
            * instances and apply filters/alarms. */
        FilterDepartures = 0x02, /**< Filtering departures @see filterSystem. */
        ProcessJourneys = 0x04, /**< Processing journeys, ie. putting data
            * from the publicTransport data engine into JourneyInfo instances. */
        AllJobs = ProcessDepartures | FilterDepartures | ProcessJourneys
            /**< All jobs, can be used with @ref abortJobs to abort all
            * jobs at once. */
    };
    Q_DECLARE_FLAGS( JobTypes, JobType )

    DepartureProcessor( QObject *parent = 0 );
    ~DepartureProcessor();

    /**
     * @brief The number of departures/arrivals in one batch, which gets send to the applet.
     *
     * If there are more items to be processed, they will be send in a later call to the applet.
     **/
    static const int DEPARTURE_BATCH_SIZE;

    /**
     * @brief The number of journeys in one batch, which gets send to the applet.
     *
     * If there are more items to be processed, they will be send in a later call to the applet.
     **/
    static const int JOURNEY_BATCH_SIZE;

    /**
     * @brief Sets the filter settings to be used.
     *
     * @param filterSettings A list of filter settings to be used.
     **/
    void setFilterSettings( const FilterSettingsList &filterSettings );

    /**
     * @brief Sets the color group settings to be used.
     *
     * @param colorGroupSettings A list of color group settings to be used.
     **/
    void setColorGroups( const ColorGroupSettingsList &colorGroupSettings );

    /** @brief Sets the list of @p alarmSettings to be used. */
    void setAlarmSettings( const AlarmSettingsList &alarmSettings );

    /**
     * @brief Sets the first departure settings to be used.
     *
     * @param firstDepartureConfigMode The first departure time can be relative to the current
     *   time (@ref RelativeToCurrentTime) or a custom time (@ref AtCustomTime).
     *
     * @param timeOfFirstDepartureCustom A custom, fixed first departure time.
     *   Only used if @p firstDepartureConfigMode is set to @ref AtCustomTime.
     *
     * @param timeOffsetOfFirstDeparture The offset in minutes of the first result departure/arrival.
     *
     * @param arrival The departures to be processed are actually arrivals.
     *
     * @see FirstDepartureConfigMode
     **/
    void setFirstDepartureSettings( FirstDepartureConfigMode firstDepartureConfigMode,
            const QTime &timeOfFirstDepartureCustom, int timeOffsetOfFirstDeparture,
            bool arrival );

    /**
     * @brief Enqueues a job of type @ref ProcessDepartures to the job queue.
     *
     * @param sourceName The data engine source name for the departure data.
     *
     * @param data The departure/arrival data from the publicTransport data engine to be processed,
     *   ie. put the data into DepartureInfo instances and apply filters/alarms.
     **/
    void processDepartures( const QString &sourceName, const QVariantHash &data );

    /**
     * @brief Enqueues a job of type @ref FilterDepartures to the job queue.
     *
     * @param sourceName The data engine source name for the departure data.
     *
     * @param departures The list of departures that should be filtered.
     *
     * @param shownDepartures A list of hashes of all currently shown departures, as returned by
     *   @ref DepartureModel::itemHashes. Hashes can be retrieved using qHash or
     *   @ref DepartureInfo::hash.
     **/
    void filterDepartures( const QString &sourceName, const QList< DepartureInfo > &departures,
                           const QList< uint > &shownDepartures = QList< uint >() );

    /**
     * @brief Enqueues a job of type @ref ProcessJourneys to the job queue.
     *
     * @param sourceName The data engine source name for the journey data.
     *
     * @param data The journey data from the publicTransport data engine to
     *   be processed, ie. put the data into JourneyInfo instances.
     **/
    void processJourneys( const QString &sourceName, const QVariantHash &data );

    /**
     * @brief Aborts all jobs of the given @p jobTypes.
     *
     * @param jobTypes The types of jobs to abort, by default all jobs are aborted.
     **/
    void abortJobs( DepartureProcessor::JobTypes jobTypes = AllJobs );

    /** @returns the job that's currently being processed by this thread. */
    JobType currentJob() const { return m_currentJob; };

    /**
     * @brief Checks if a departure/arrival/journey should be shown with the given settings.
     *
     * Items won't be shown, if it's eg. before the configured time of the first departure/arrival.
     *
     * @param dateTime The date and time of the departure/arrival.
     *
     * @param firstDepartureConfigMode The settings mode for the first shown departure/arrival.
     *
     * @param timeOfFirstDepartureCustom The time set as the first departure/arrival time.
     *   Only used if @ref AtCustomTime is used in @p firstDepartureConfigMode.
     *
     * @param timeOffsetOfFirstDeparture The offset in minutes from now for the first
     *   departure/arrival. Only used if @ref RelativeToCurrentTime is used in
     *   @p firstDepartureConfigMode.
     *
     * @return True, if the departure/arrival/journey should be shown. False otherwise.
     **/
    static bool isTimeShown( const QDateTime& dateTime,
            FirstDepartureConfigMode firstDepartureConfigMode,
            const QTime &timeOfFirstDepartureCustom, int timeOffsetOfFirstDeparture );

signals:
    /**
     * @brief A departure/arrival processing job now gets started.
     *
     * @param sourceName The data engine source name for the departure data.
     **/
    void beginDepartureProcessing( const QString &sourceName );

    /**
     * @brief A departure/arrival processing job is finished (or a batch of departures/arrivals).
     *
     * @param sourceName The data engine source name for the departure data.
     *
     * @param departures A list of departures that were read.
     *
     * @param requestUrl The url that was used to download the departure data.
     *
     * @param lastUpdate The date and time of the last update of the data.
     *
     * @param departuresToGo The number of departures to still be processed. If this isn't 0
     *   this signal gets emitted again after the next batch of departures has been processed.
     **/
    void departuresProcessed( const QString &sourceName, const QList< DepartureInfo > &departures,
            const QUrl &requestUrl, const QDateTime &lastUpdate, int departuresToGo = 0 );

    /**
     * @brief A journey processing job now gets started.
     *
     * @param sourceName The data engine source name for the journey data.
     **/
    void beginJourneyProcessing( const QString &sourceName );

    /**
     * @brief A journey processing job is finished (or a batch of journeys).
     *
     * @param sourceName The data engine source name for the journey data.
     *
     * @param journeys A list of journeys that were read.
     *
     * @param requestUrl The url that was used to download the journey data.
     *
     * @param lastUpdate The date and time of the last update of the data.
     **/
    void journeysProcessed( const QString &sourceName, const QList< JourneyInfo > &journeys,
                            const QUrl &requestUrl, const QDateTime &lastUpdate );

    /**
     * @brief A filter departures job now gets started.
     *
     * @param sourceName The data engine source name for the departure data.
     **/
    void beginFiltering( const QString &sourceName );

    /**
     * @brief A filter departures job is finished.
     *
     * @param sourceName The data engine source name for the departure data.
     *
     * @param departures The list of departures that were filtered. Each
     *   departure now returns the correct value with isFilteredOut() according
     *   to the filter settings given to the worker thread.
     *
     * @param newlyFiltered A list of departures that should be made visible
     *   to match the current filter settings.
     *
     * @param newlyNotFiltered A list of departures that should be made
     *   invisible to match the current filter settings.
     **/
    void departuresFiltered( const QString &sourceName, const QList< DepartureInfo > &departures,
                             const QList< DepartureInfo > &newlyFiltered,
                             const QList< DepartureInfo > &newlyNotFiltered );

protected:
    virtual void run();

private:
    struct JobInfo {
        JobType type;
        QString sourceName;
    };
    struct DepartureJobInfo : public JobInfo {
        DepartureJobInfo() {
            type = ProcessDepartures;
            alreadyProcessed = 0;
        };

        QVariantHash data;
        int alreadyProcessed; // used for requeuing
    };
    struct FilterJobInfo : public JobInfo {
        FilterJobInfo() { type = FilterDepartures; };

        QList< DepartureInfo > departures;
        QList< uint > shownDepartures;
    };
    struct JourneyJobInfo : public DepartureJobInfo {
        JourneyJobInfo() {
            type = ProcessJourneys;
            alreadyProcessed = 0;
        };
    };

    void doDepartureJob( DepartureJobInfo *departureJob );
    void doJourneyJob( JourneyJobInfo *journeyJob );
    void doFilterJob( FilterJobInfo *filterJob );
    void startOrEnqueueJob( JobInfo *jobInfo );

    QQueue< JobInfo* > m_jobQueue;
    JobType m_currentJob;

    FilterSettingsList m_filterSettings;
    ColorGroupSettingsList m_colorGroupSettings;
    AlarmSettingsList m_alarmSettings;
    FirstDepartureConfigMode m_firstDepartureConfigMode;
    QTime m_timeOfFirstDepartureCustom;
    int m_timeOffsetOfFirstDeparture;
    bool m_isArrival;

    bool m_quit, m_abortCurrentJob, m_requeueCurrentJob;
    QMutex m_mutex;
    QWaitCondition m_cond;
};
Q_DECLARE_OPERATORS_FOR_FLAGS( DepartureProcessor::JobTypes )

QDebug &operator <<( QDebug debug, DepartureProcessor::JobType jobType );

#endif // Multiple inclusion guard
