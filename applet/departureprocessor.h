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

class DepartureProcessor : public QThread {
    Q_OBJECT
    public:
	enum JobType {
	    NoJob = 0x00,
	    ProcessDepartures = 0x01,
	    FilterDepartures = 0x02,
	    ProcessJourneys = 0x04,
	    AllJobs = ProcessDepartures | FilterDepartures | ProcessJourneys
	};
	Q_DECLARE_FLAGS( JobTypes, JobType );
	
	DepartureProcessor( QObject *parent = 0 );
	~DepartureProcessor();
	
	static const int DEPARTURE_BATCH_SIZE;
	static const int JOURNEY_BATCH_SIZE;
	
	void setFilterSettings( const FilterSettings &filterSettings,
				bool filtersEnabled );
	void setAlarmSettings( const AlarmSettingsList &alarmSettings );
	void setFirstDepartureSettings( FirstDepartureConfigMode firstDepartureConfigMode,
					const QTime &timeOfFirstDepartureCustom,
					int timeOffsetOfFirstDeparture );
	
	void processDepartures( const QString &sourceName, const QVariantHash &data );
	void filterDepartures( const QString &sourceName,
			       const QList< DepartureInfo > &departures,
			       const QList< uint > &shownDepartures );
	void processJourneys( const QString &sourceName, const QVariantHash &data );
	
	void abortJobs( JobTypes jobTypes );
	
	static bool isTimeShown( const QDateTime& dateTime,
				 FirstDepartureConfigMode firstDepartureConfigMode,
				 const QTime &timeOfFirstDepartureCustom,
				 int timeOffsetOfFirstDeparture );
	
    signals:
	void beginDepartureProcessing( const QString &sourceName );
	void departuresProcessed( const QString &sourceName,
				  const QList< DepartureInfo > &departures,
				  const QUrl &requestUrl,
				  const QDateTime &lastUpdate );
				  
	void beginJourneyProcessing( const QString &sourceName );
	void journeysProcessed( const QString &sourceName,
				const QList< JourneyInfo > &journeys,
				const QUrl &requestUrl,
				const QDateTime &lastUpdate );

	void beginFiltering( const QString &sourceName );
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