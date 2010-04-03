/*
*   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef DEPARTUREPROCESSOR_HEADER
#define DEPARTUREPROCESSOR_HEADER

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>

#include "departureinfo.h"
#include "filter.h"
#include "settings.h"

/** A worker thread that puts data from the publicTransport data engine into
* DepartureInfo/JourneyInfo instances. It also applies filters and checks if 
* alarm filters match. 
* Filters are given as FilterSettings by @ref setFilterSettings. They contain
* a list of filters which get OR combined. Each filter has a list of constraints
* which get AND combined. This could take some time with complex filter settings
* and a long list of departures/arrivals. That's actually the main reason to do
* this in a thread.
* To ensure that only departures get marked to be shown which departure time is
* greater than or equal to the first departure time, use @ref setFirstDepartureSettings.
* The thread uses a job queue, jobs can be cancelled by their type using 
* @ref abortJobs. To add a new job to the queue use @ref processDepartures,
* @ref processJourneys or @ref filterDepartures.
* @brief Worker thread for PublicTransport */
class DepartureProcessor : public QThread {
    Q_OBJECT
    public:
	/** Types of jobs. */
	enum JobType {
	    NoJob = 0x00, /**< No job. This is returned by @ref currentJob if the
		    * thread is idle. */
	    ProcessDepartures = 0x01, /**< Processing departures, ie. putting data
		    * from the publicTransport data engine into DepartureInfo
		    * instances and apply filters/alarms. */
	    FilterDepartures = 0x02, /**< Filtering departures. */
	    ProcessJourneys = 0x04, /**< Processing journeys, ie. putting data
		    * from the publicTransport data engine into JourneyInfo instances. */
	    AllJobs = ProcessDepartures | FilterDepartures | ProcessJourneys
		    /**< All jobs, can be used with @ref abortJobs to abort all
		    * jobs at once. */
	};
	Q_DECLARE_FLAGS( JobTypes, JobType );
	
	DepartureProcessor( QObject *parent = 0 );
	~DepartureProcessor();
	
	static const int DEPARTURE_BATCH_SIZE;
	static const int JOURNEY_BATCH_SIZE;

	/** Sets the filter settings to be used.
	* @param filterSettings The filter settings to be used. Only used if 
	* @p filtersEnabled is true.
	* @param filtersEnabled Whether or not filter are enabled. */
	void setFilterSettings( const FilterSettings &filterSettings,
				bool filtersEnabled );
	/** Sets the list of @p alarmSettings to be used. */
	void setAlarmSettings( const AlarmSettingsList &alarmSettings );
	/** Sets the first departure settings to be used.
	* @param firstDepartureConfigMode The first departure time can be relative 
	* to the current time (@ref RelativeToCurrentTime) or a custom time (@ref AtCustomTime).
	* @param timeOfFirstDepartureCustom A custom, fixed first departure time.
	* Only used if @p firstDepartureConfigMode is set to @ref AtCustomTime.
	* @see FirstDepartureConfigMode */
	void setFirstDepartureSettings( FirstDepartureConfigMode firstDepartureConfigMode,
					const QTime &timeOfFirstDepartureCustom,
					int timeOffsetOfFirstDeparture );

	/** Enqueues a job of type @ref ProcessDepartures to the job queue.
	* @param sourceName The data engine source name for the departure data.
	* @param data The departure/arrival data from the publicTransport data 
	* engine to be processed, ie. put the data into DepartureInfo instances 
	* and apply filters/alarms. */
	void processDepartures( const QString &sourceName, const QVariantHash &data );
	/** Enqueues a job of type @ref FilterDepartures to the job queue.
	* @param sourceName The data engine source name for the departure data.
	* @param departures The list of departures that should be filtered.
	* @param shownDepartures A list of hashes of all currently shown 
	* departures, as returned by @ref DepartureModel::itemHashes.
	* Hashes can be retrieved using qHash or @ref DepartureInfo::hash. */
	void filterDepartures( const QString &sourceName,
			       const QList< DepartureInfo > &departures,
			       const QList< uint > &shownDepartures = QList< uint >() );
	/** Enqueues a job of type @ref ProcessJourneys to the job queue.
	* @param sourceName The data engine source name for the journey data.
	* @param data The journey data from the publicTransport data engine to 
	* be processed, ie. put the data into JourneyInfo instances. */
	void processJourneys( const QString &sourceName, const QVariantHash &data );

	/** Aborts all jobs of the given @p jobTypes.
	* @p jobTypes The types of jobs to abort, by default all jobs are aborted. */
	void abortJobs( JobTypes jobTypes = AllJobs );
	/** @returns the job that's currently being processed by this thread. */
	JobType currentJob() const { return m_currentJob; };
	
	static bool isTimeShown( const QDateTime& dateTime,
				 FirstDepartureConfigMode firstDepartureConfigMode,
				 const QTime &timeOfFirstDepartureCustom,
				 int timeOffsetOfFirstDeparture );
	
    signals:
	/** A departure/arrival processing job now gets started.
	* @param sourceName The data engine source name for the departure data. */
	void beginDepartureProcessing( const QString &sourceName );
	/** A departure/arrival processing job is finished.
	* @param sourceName The data engine source name for the departure data.
	* @param departures A list of departures that were read.
	* @param requestUrl The url that was used to download the departure data.
	* @param lastUpdate The date and time of the last update of the data. */
	void departuresProcessed( const QString &sourceName,
				  const QList< DepartureInfo > &departures,
				  const QUrl &requestUrl,
				  const QDateTime &lastUpdate );
	
	/** A journey processing job now gets started.
	* @param sourceName The data engine source name for the journey data. */
	void beginJourneyProcessing( const QString &sourceName );
	/** A journey processing job is finished.
	* @param sourceName The data engine source name for the journey data.
	* @param journeys A list of journeys that were read.
	* @param requestUrl The url that was used to download the journey data.
	* @param lastUpdate The date and time of the last update of the data. */
	void journeysProcessed( const QString &sourceName,
				const QList< JourneyInfo > &journeys,
				const QUrl &requestUrl,
				const QDateTime &lastUpdate );

	/** A filter departures job now gets started.
	* @param sourceName The data engine source name for the departure data. */
	void beginFiltering( const QString &sourceName );
	/** A filter departures job is finished.
	* @param sourceName The data engine source name for the departure data.
	* @param departures The list of departures that were filtered. Each
	* departure now returns the correct value with isFilteredOut() according
	* to the filter settings given to the worker thread.
	* @param newlyFiltered A list of departures that should be made visible
	* to match the current filter settings.
	* @param newlyNotFiltered A list of departures that should be made
	* invisible to match the current filter settings. */
	void departuresFiltered( const QString &sourceName,
				 const QList< DepartureInfo > &departures,
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
	    DepartureJobInfo() { type = ProcessDepartures;
				 alreadyProcessed = 0; };
	    
	    QVariantHash data;
	    int alreadyProcessed; // used for requeuing
	};
	struct FilterJobInfo : public JobInfo {
	    FilterJobInfo() { type = FilterDepartures; };
	    
	    QList< DepartureInfo > departures;
	    QList< uint > shownDepartures;
	};
	struct JourneyJobInfo : public DepartureJobInfo {
	    JourneyJobInfo() { type = ProcessJourneys;
			       alreadyProcessed = 0; };
	};
	
	void doDepartureJob( DepartureJobInfo *departureJob );
	void doJourneyJob( JourneyJobInfo *journeyJob );
	void doFilterJob( FilterJobInfo *filterJob );
	void startOrEnqueueJob( JobInfo *jobInfo );

	QQueue< JobInfo* > m_jobQueue;
	JobType m_currentJob;
	
	FilterSettings m_filterSettings;
	bool m_filtersEnabled;
	AlarmSettingsList m_alarmSettings;
	FirstDepartureConfigMode m_firstDepartureConfigMode;
	QTime m_timeOfFirstDepartureCustom;
	int m_timeOffsetOfFirstDeparture;

	bool m_quit, m_abortCurrentJob, m_requeueCurrentJob;
	QMutex m_mutex;
	QWaitCondition m_cond;
};
Q_DECLARE_OPERATORS_FOR_FLAGS( DepartureProcessor::JobTypes );

QDebug &operator <<( QDebug debug, DepartureProcessor::JobType jobType );

#endif // Multiple inclusion guard
