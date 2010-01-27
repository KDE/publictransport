/*
 *   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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

// Qt includes
#include <QtXml>
#include <QFileSystemWatcher>

// KDE includes
#include <Plasma/DataContainer>
#include <KStandardDirs>

// Own includes
#include "publictransportdataengine.h"



PublicTransportEngine::PublicTransportEngine(QObject* parent, const QVariantList& args)
			    : Plasma::DataEngine(parent, args) {
    // We ignore any arguments - data engines do not have much use for them
    Q_UNUSED(args)

    m_lastJourneyCount = 0;
    m_lastStopNameCount = 0;
    m_fileSystemWatcher = NULL;

    // This prevents applets from setting an unnecessarily high update interval
    // and using too much CPU.
    // 30 seconds should be enough, departure / arrival times have minute precision.
    setMinimumPollingInterval( 30000 );

    // Add service provider source, so when using
    // dataEngine("publictransport").sources() in an applet it at least returns this
    setData( I18N_NOOP("ServiceProviders"), DataEngine::Data() );
    setData( I18N_NOOP("ErrornousServiceProviders"), DataEngine::Data() );
    setData( I18N_NOOP("Locations"), DataEngine::Data() );
}

QHash< QString, QVariant > PublicTransportEngine::serviceProviderInfo(
			    const TimetableAccessor *&accessor ) {
    Q_ASSERT( accessor != NULL );

    QHash<QString, QVariant> dataServiceProvider;
    dataServiceProvider.insert( "id", accessor->serviceProvider() );
    dataServiceProvider.insert( "fileName", accessor->timetableAccessorInfo().fileName() );
    dataServiceProvider.insert( "scriptFileName", accessor->timetableAccessorInfo().scriptFileName() );
    dataServiceProvider.insert( "name", accessor->timetableAccessorInfo().name() );
    dataServiceProvider.insert( "shortUrl", accessor->timetableAccessorInfo().shortUrl() );
    dataServiceProvider.insert( "country", accessor->country() );
    dataServiceProvider.insert( "cities", accessor->cities() );
    dataServiceProvider.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
    dataServiceProvider.insert( "onlyUseCitiesInList", accessor->onlyUseCitiesInList() );
    dataServiceProvider.insert( "features", accessor->features() );
    dataServiceProvider.insert( "featuresLocalized", accessor->featuresLocalized() );
    dataServiceProvider.insert( "url", accessor->timetableAccessorInfo().url() );
    dataServiceProvider.insert( "author", accessor->timetableAccessorInfo().author() );
    dataServiceProvider.insert( "email", accessor->timetableAccessorInfo().email() );
    dataServiceProvider.insert( "description", accessor->timetableAccessorInfo().description() );
    dataServiceProvider.insert( "version", accessor->timetableAccessorInfo().version() );

    return dataServiceProvider;
}

QHash< QString, QVariant > PublicTransportEngine::locations() {
    QHash<QString, QVariant> ret, locationHash;
    QString name;

    locationHash.insert( "name", name = "international" );
    locationHash.insert( "description", i18n("Contains international providers. There is one for getting flight departures / arrivals.") );
    locationHash.insert( "defaultAccessor", "others_flightstats" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "de" );
    locationHash.insert( "description", i18n("Support for all cities in Germany. There is also support for providers specific to regions / cities.") );
    locationHash.insert( "defaultAccessor", "de_db" );
    ret.insert( name, locationHash );
    
    locationHash.clear();
    locationHash.insert( "name", name = "fr" );
    locationHash.insert( "description", i18n("Support for some cities in France. No local public transportation information.") );
    locationHash.insert( "defaultAccessor", "fr_gares" );
    ret.insert( name, locationHash );
    
    locationHash.clear();
    locationHash.insert( "name", name = "be" );
    locationHash.insert( "description", i18n("Support for some cities in Belgium.") );
    locationHash.insert( "defaultAccessor", "be_brail" );
    ret.insert( name, locationHash );
    
    locationHash.clear();
    locationHash.insert( "name", name = "dk" );
    locationHash.insert( "description", i18n("Support for some cities in Denmark.") );
    locationHash.insert( "defaultAccessor", "dk_rejseplanen" );
    ret.insert( name, locationHash );
    
    locationHash.clear();
    locationHash.insert( "name", name = "us" );
    locationHash.insert( "description", i18n("Support for Southeastern Pennsylvania.") );
    locationHash.insert( "defaultAccessor", "us_septa" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "ch" );
    locationHash.insert( "description", i18n("Support for all cities in Switzerland.") );
    locationHash.insert( "defaultAccessor", "ch_sbb" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "at" );
    locationHash.insert( "description", i18n("Support for all cities in Austria.") );
    locationHash.insert( "defaultAccessor", "at_oebb" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "pl" );
    locationHash.insert( "description", i18n("Support for all cities in Poland.") );
    locationHash.insert( "defaultAccessor", "pl_pkp" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "cz" ); //= i18n("Czechia") );
    locationHash.insert( "description", i18n("Support for many cities in Czechia, but with static data.") );
    locationHash.insert( "defaultAccessor", "cz_idnes" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "sk" );
    locationHash.insert( "description", i18n("Support for many cities in Slovakia, but with static data. There is also support for bratislava with dynamic data.") );
    locationHash.insert( "defaultAccessor", "sk_atlas" );
    ret.insert( name, locationHash );

    return ret;
}

bool PublicTransportEngine::sourceRequestEvent( const QString &name ) {
    kDebug() << name;

    setData( name, DataEngine::Data() ); // Create source, TODO: check if [name] is valid
    return updateSourceEvent( name );
}

bool PublicTransportEngine::updateServiceProviderSource( const QString &name ) {
    QHash<QString, QVariant> dataSource;
    if ( m_dataSources.keys().contains(name) ) {
	// 	    kDebug() << "Data source" << name << "is up to date";
	dataSource = m_dataSources[name].toHash();
    } else {
	QStringList dirList = KGlobal::dirs()->findDirs( "data",
			"plasma_engine_publictransport/accessorInfos" );
	if ( m_fileSystemWatcher == NULL )
	    m_fileSystemWatcher = new QFileSystemWatcher( dirList );
	connect( m_fileSystemWatcher, SIGNAL(directoryChanged(QString)),
	    this, SLOT(accessorInfoDirChanged(QString)) );

	QStringList fileNames = KGlobal::dirs()->findAllResources( "data",
			"plasma_engine_publictransport/accessorInfos/*.xml" );
	if ( fileNames.isEmpty() ) {
	    kDebug() << "Couldn't find any service provider information XML files";
	    return false;
	}

	QStringList loadedAccessors;
	m_errornousAccessors.clear();
	foreach( QString fileName, fileNames ) {
	    QString s = KUrl(fileName).fileName().remove(QRegExp("\\..*$")); // Remove file extension
	    const TimetableAccessor *accessor = TimetableAccessor::getSpecificAccessor( s );
	    if ( accessor ) {
		dataSource.insert( accessor->timetableAccessorInfo().name(),
				   serviceProviderInfo(accessor) );
		loadedAccessors << s;
	    }
	    else {
		m_errornousAccessors << s;
	    }
	}

	kDebug() << "Loaded accessors from XML files:" << loadedAccessors;
	kDebug() << "Errornous accessor info XMLs, that couldn't be loaded:" << m_errornousAccessors;

	m_dataSources.insert( name, dataSource );
    }

    foreach ( QString key, dataSource.keys() )
	setData( name, key, dataSource[key] );
    return true;
}

void PublicTransportEngine::updateErrornousServiceProviderSource( const QString &name ) {
    setData( name, "names", m_errornousAccessors );
}

void PublicTransportEngine::updateLocationSource( const QString &name ) {
    QHash<QString, QVariant> dataSource;
    if ( m_dataSources.keys().contains(name) )
	dataSource = m_dataSources[name].toHash(); // locations already loaded
    else
	dataSource = locations();
    m_dataSources.insert( name, dataSource );

    foreach ( QString key, dataSource.keys() )
	setData( name, key, dataSource[key] );
}

bool PublicTransportEngine::updateDepartureOrJourneySource( const QString &name ) {
    bool containsDataSource = m_dataSources.contains( name );
    if ( containsDataSource && isSourceUpToDate(name) )
    { // Data is stored in the map and up to date
	kDebug() << "Data source" << name << "is up to date";
	QHash<QString, QVariant> dataSource = m_dataSources[name].toHash();
	foreach ( QString key, dataSource.keys() )
	    setData( name, key, dataSource[key] );
    } else { // Request new data
	if ( containsDataSource )
	    m_dataSources.remove(name); // Clear old data

	QStringList input;
	ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals;
	QString city, stop, targetStop, originStop, dataType;
	int maxDeps = DEFAULT_MAXIMUM_DEPARTURES;
	QDateTime dateTime;

	kDebug() << name;

	QString parameters;
	if ( name.startsWith(I18N_NOOP("Departures"), Qt::CaseInsensitive) ) {
	    parameters = name.mid( QString(I18N_NOOP("Departures")).length() );
	    parseDocumentMode = ParseForDeparturesArrivals;
	    dataType = "departures";
	} else if ( name.startsWith(I18N_NOOP("Arrivals"), Qt::CaseInsensitive) ) {
	    parameters = name.mid( QString(I18N_NOOP("Arrivals")).length() );
	    parseDocumentMode = ParseForDeparturesArrivals;
	    dataType = "arrivals";
	} else if ( name.startsWith(I18N_NOOP("Stops"), Qt::CaseInsensitive) ) {
	    parameters = name.mid( QString(I18N_NOOP("Stops")).length() );
	    parseDocumentMode = ParseForStopSuggestions;
	    dataType = "stopSuggestions";
	} else if ( name.startsWith(I18N_NOOP("JourneysDep"), Qt::CaseInsensitive) ) {
	    parameters = name.mid( QString(I18N_NOOP("JourneysDep")).length() );
	    parseDocumentMode = ParseForJourneys;
	    dataType = "journeysDep";
	} else if ( name.startsWith(I18N_NOOP("JourneysArr"), Qt::CaseInsensitive) ) {
	    parameters = name.mid( QString(I18N_NOOP("JourneysArr")).length() );
	    parseDocumentMode = ParseForJourneys;
	    dataType = "journeysArr";
	} else if ( name.startsWith(I18N_NOOP("Journeys"), Qt::CaseInsensitive) ) {
	    parameters = name.mid( QString(I18N_NOOP("Journeys")).length() );
	    parseDocumentMode = ParseForJourneys;
	    dataType = "journeysDep";
	}
	input = parameters.trimmed().split("|", QString::SkipEmptyParts);

// 	    if ( input.length() < 2 ) {
// 		kDebug() << QString("Wrong input: '%1'").arg(name);
// 		return false; // wrong input
// 	    }

	QString serviceProvider = input.at(0).trimmed();

	for ( int i = 0; i < input.length(); ++i ) {
	    QString s = input.at(i).toLower();
	    if ( s.startsWith("city=", Qt::CaseInsensitive) )
		city = input.at(i).mid( QString("city=").length() ).trimmed();
	    else if ( s.startsWith("stop=", Qt::CaseInsensitive) )
		stop = input.at(i).mid( QString("stop=").length() ).trimmed();
	    else if ( s.startsWith("targetStop=", Qt::CaseInsensitive) )
		targetStop = input.at(i).mid( QString("targetStop=").length() ).trimmed();
	    else if ( s.startsWith("originStop=", Qt::CaseInsensitive) )
		originStop = input.at(i).mid( QString("originStop=").length() ).trimmed();
	    else if ( s.startsWith("maxdeps=", Qt::CaseInsensitive) ) {
		s = input.at(i).mid( QString("maxdeps=").length() ).trimmed();
		maxDeps = s.toInt() == 0 ? 20 : s.toInt();
	    } else if ( s.startsWith("timeoffset=", Qt::CaseInsensitive) ) {
		s = input.at(i).mid( QString("timeoffset=").length() ).trimmed();
		dateTime = QDateTime::currentDateTime().addSecs( s.toInt() * 60 );
	    } else if ( s.startsWith("time=", Qt::CaseInsensitive) ) {
		s = input.at(i).mid( QString("time=").length() ).trimmed();
		dateTime = QDateTime( QDate::currentDate(), QTime::fromString(s, "hh:mm") );
	    } else if ( s.startsWith("datetime=", Qt::CaseInsensitive) ) {
		s = input.at(i).mid( QString("datetime=").length() ).trimmed();
		dateTime = QDateTime::fromString( s );
	    }
	}
	maxDeps += ADDITIONAL_MAXIMUM_DEPARTURES;

	if ( dateTime.isNull() )
	    dateTime = QDateTime::currentDateTime().addSecs( DEFAULT_TIME_OFFSET * 60 );

	if ( parseDocumentMode == ParseForDeparturesArrivals
		    || parseDocumentMode == ParseForStopSuggestions ) {
	    if ( stop.isEmpty() ) {
		kDebug() << "Stop name is missing in data source name";
		return false; // wrong input
	    }
	} else {
	    if ( originStop.isEmpty() && !targetStop.isEmpty() )
		originStop = stop;
	    else if ( targetStop.isEmpty() && !originStop.isEmpty() )
		targetStop = stop;
	}

	// Try to get the specific accessor from m_accessors (if it's not in there it is created)
	bool newlyCreated = false;
	TimetableAccessor *accessor;
	if ( !m_accessors.contains(serviceProvider) ) {
	    accessor = TimetableAccessor::getSpecificAccessor(serviceProvider);
	    m_accessors.insert( serviceProvider, accessor );
	    newlyCreated = true;
	} else {
	    accessor = m_accessors.value(serviceProvider);
	}

	if ( accessor == NULL ) {
	    kDebug() << QString("Accessor %1 couldn't be created").arg(serviceProvider);
	    return false; // accessor couldn't be created
	} else if ( accessor->useSeperateCityValue() && city.isEmpty() ) {
	    kDebug() << QString("Accessor %1 needs a seperate city value. Add to "
				"source name '|city=X', where X stands for the city "
				"name.").arg(serviceProvider);
	    return false; // accessor needs a seperate city value
	} else if ( parseDocumentMode == ParseForJourneys
		    && !accessor->features().contains("JourneySearch") ) {
	    kDebug() << QString("Accessor %1 doesn't support journey searches.")
			.arg(serviceProvider);
	    return false; // accessor doesn't support journey searches
	}

	if ( newlyCreated ) {
	    // 		if ( parseDocumentMode == ParseForDeparturesArrivals ) {
	    connect( accessor,
		     SIGNAL(departureListReceived(TimetableAccessor*,const QUrl&,QList<DepartureInfo*>,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)),
		     this, SLOT(departureListReceived(TimetableAccessor*,const QUrl&,QList<DepartureInfo*>,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)) );
	    // 		} else { // if ( parseDocumentMode == ParseForJourneys )
	    connect( accessor,
		     SIGNAL(journeyListReceived(TimetableAccessor*,const QUrl&,QList<JourneyInfo*>,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)),
		     this, SLOT(journeyListReceived(TimetableAccessor*,const QUrl&,QList<JourneyInfo*>,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)) );
	    // 		}
	    connect( accessor,
		     SIGNAL(stopListReceived(TimetableAccessor*,const QUrl&,const QStringList&,const QHash<QString,QString>&,const QHash<QString, int>&,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)),
		     this, SLOT(stopListReceived(TimetableAccessor*,const QUrl&,const QStringList&,const QHash<QString,QString>&,const QHash<QString, int>&,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)) );
	    connect( accessor,
		     SIGNAL(errorParsing(TimetableAccessor*,const QUrl&,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)),
		     this, SLOT(errorParsing(TimetableAccessor*,const QUrl&,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)) );
	}

	if ( parseDocumentMode == ParseForDeparturesArrivals )
	    accessor->requestDepartures( name, city, stop, maxDeps, dateTime, dataType );
	else if ( parseDocumentMode == ParseForStopSuggestions )
	    accessor->requestStopSuggestions( name, city, stop );
	else // if ( parseDocumentMode == ParseForJourneys )
	    accessor->requestJourneys( name, city, originStop, targetStop,
				       maxDeps, dateTime, dataType );
    }

    return true;
}

void PublicTransportEngine::accessorInfoDirChanged( QString path ) {
    kDebug() << path << "was changed";
    if ( m_dataSources.keys().contains(I18N_NOOP("ServiceProviders")) )
	m_dataSources.remove(I18N_NOOP("ServiceProviders"));
    updateServiceProviderSource(I18N_NOOP("ServiceProviders"));
}

bool PublicTransportEngine::updateSourceEvent( const QString &name ) {
    kDebug() << name;

    bool ret = true;
    if ( name.compare(I18N_NOOP("ServiceProviders"), Qt::CaseInsensitive) == 0 )
	ret = updateServiceProviderSource( name );
    else if ( name.compare(I18N_NOOP("ErrornousServiceProviders"), Qt::CaseInsensitive) == 0 )
	updateErrornousServiceProviderSource( name );
    else if ( name.compare(I18N_NOOP("Locations"), Qt::CaseInsensitive) == 0 )
	updateLocationSource( name );
    else if ( name.startsWith(I18N_NOOP("Departures"), Qt::CaseInsensitive)
		|| name.startsWith(I18N_NOOP("Arrivals"), Qt::CaseInsensitive)
		|| name.startsWith(I18N_NOOP("Stops"), Qt::CaseInsensitive)
		|| name.startsWith(I18N_NOOP("Journeys"), Qt::CaseInsensitive) )
	ret = updateDepartureOrJourneySource( name );
    else {
	kDebug() << "Source name incorrect" << name;
	return false;
    }

    return ret;
}

void PublicTransportEngine::departureListReceived( TimetableAccessor *accessor,
		  const QUrl &requestUrl,
		  QList< DepartureInfo* > departures,
		  const QString &serviceProvider, const QString &sourceName,
		  const QString &city, const QString &stop,
		  const QString &dataType, ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
    Q_UNUSED(stop);
    Q_UNUSED(dataType);
    Q_UNUSED(parseDocumentMode);
    kDebug() << departures.count() << "departures / arrivals received" << sourceName;

    int i = 0;
    m_dataSources.remove( sourceName );
    QHash<QString, QVariant> dataSource;
    foreach ( DepartureInfo *departureInfo, departures ) {
// 	if ( !departureInfo->isValid() ) {
// 	    kDebug() << "Departure isn't valid" << departureInfo->line();
// 	    continue;
// 	}
	QHash<QString, QVariant> data;
	data.insert("line", departureInfo->line());
	data.insert("target", departureInfo->target());
	data.insert("departure", departureInfo->departure());
	data.insert("vehicleType", static_cast<int>(departureInfo->vehicleType()));
	data.insert("nightline", departureInfo->isNightLine());
	data.insert("expressline", departureInfo->isExpressLine());
	data.insert("platform", departureInfo->platform());
	data.insert("delay", departureInfo->delay());
	data.insert("delayReason", departureInfo->delayReason());
	data.insert("journeyNews", departureInfo->journeyNews());
	data.insert("operator", departureInfo->operatorName());
	data.insert("routeStops", departureInfo->routeStops()); // TODO update doc
	data.insert("routeTimes", departureInfo->routeTimesVariant()); // TODO update doc
	data.insert("routeExactStops", departureInfo->routeExactStops()); // TODO update doc

	QString sKey = QString( "%1" ).arg( i++ );
	setData( sourceName, sKey, data );
	dataSource.insert( sKey, data );
// 	kDebug() << "setData" << sourceName << data;
    }

    // Remove old jouneys
    for ( ; i < m_lastJourneyCount; ++i )
	removeData( sourceName, QString("%1").arg(i++) );
    m_lastJourneyCount = departures.count();

    // Remove old stop suggestions
    for ( i = 0 ; i < m_lastStopNameCount; ++i )
	removeData( sourceName, QString("stopName %1").arg(i++) );
    m_lastStopNameCount = 0;
    
    setData( sourceName, "serviceProvider", serviceProvider );
    setData( sourceName, "count", departures.count() );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", "departures" );
    setData( sourceName, "receivedData", "departures" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "serviceProvider", serviceProvider );
    dataSource.insert( "count", departures.count() );
    dataSource.insert( "requestUrl", requestUrl );
    dataSource.insert( "parseMode", "departures" );
    dataSource.insert( "receivedData", "departures" );
    dataSource.insert( "receivedPossibleStopList", false );
    dataSource.insert( "error", false );
    dataSource.insert( "updated", QDateTime::currentDateTime() );
    m_dataSources.insert( sourceName, dataSource );
}


void PublicTransportEngine::journeyListReceived( TimetableAccessor* accessor,
						 const QUrl &requestUrl,
						 QList< JourneyInfo* > journeys,
						 const QString &serviceProvider,
						 const QString& sourceName,
						 const QString& city,
						 const QString& stop,
						 const QString& dataType,
						 ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
    Q_UNUSED(stop);
    Q_UNUSED(dataType);
    Q_UNUSED(parseDocumentMode);
    kDebug() << journeys.count() << "journeys received" << sourceName;

    int i = 0;
    m_dataSources.remove( sourceName );
    QHash<QString, QVariant> dataSource;
    foreach ( JourneyInfo *journeyInfo, journeys )
    {
// 	kDebug() << journeyInfo->isValid() << journeyInfo->departure() << journeyInfo->duration() << journeyInfo->changes() << journeyInfo->vehicleTypes() << journeyInfo->startStopName() << journeyInfo->targetStopName() << journeyInfo->arrival();
	if ( !journeyInfo->isValid() )
	    continue;

	QHash<QString, QVariant> data;
	data.insert("vehicleTypes", journeyInfo->vehicleTypesVariant());
	data.insert("arrival", journeyInfo->arrival());
	data.insert("departure", journeyInfo->departure());
	data.insert("duration", journeyInfo->duration());
	data.insert("changes", journeyInfo->changes());
	data.insert("pricing", journeyInfo->pricing());
	data.insert("journeyNews", journeyInfo->journeyNews());
	data.insert("startStopName", journeyInfo->startStopName());
	data.insert("targetStopName", journeyInfo->targetStopName());
	data.insert("Operator", journeyInfo->operatorName());
	data.insert("routeStops", journeyInfo->routeStops()); // TODO update doc
	data.insert("routeTimesDeparture", journeyInfo->routeTimesDepartureVariant()); // TODO update doc
	data.insert("routeTimesArrival", journeyInfo->routeTimesArrivalVariant()); // TODO update doc
	data.insert("routeExactStops", journeyInfo->routeExactStops()); // TODO update doc
	data.insert("routeVehicleTypes", journeyInfo->routeVehicleTypesVariant()); // TODO update doc
	data.insert("routeTransportLines", journeyInfo->routeTransportLines()); // TODO update doc
	data.insert("routePlatformsDeparture", journeyInfo->routePlatformsDeparture()); // TODO update doc
	data.insert("routePlatformsArrival", journeyInfo->routePlatformsArrival()); // TODO update doc
	data.insert("routeTimesDepartureDelay", journeyInfo->routeTimesDepartureDelay()); // TODO update doc
	data.insert("routeTimesArrivalDelay", journeyInfo->routeTimesArrivalDelay()); // TODO update doc

	QString sKey = QString("%1").arg(i++);
	setData( sourceName, sKey, data );
	dataSource.insert( sKey, data );
	// 	kDebug() << "setData" << sourceName << data;
    }

    // Remove old journeys
    for ( ; i < m_lastJourneyCount; ++i )
	removeData( sourceName, QString("%1").arg(i++) );
    m_lastJourneyCount = journeys.count();

    // Remove old stop suggestions
    for ( i = 0 ; i < m_lastStopNameCount; ++i )
	removeData( sourceName, QString("stopName %1").arg(i++) );
    m_lastStopNameCount = 0;
    
    setData( sourceName, "serviceProvider", serviceProvider );
    setData( sourceName, "count", journeys.count() );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", "journeys" );
    setData( sourceName, "receivedData", "journeys" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "serviceProvider", serviceProvider );
    dataSource.insert( "count", journeys.count() );
    dataSource.insert( "requestUrl", requestUrl );
    dataSource.insert( "parseMode", "journeys" );
    dataSource.insert( "receivedData", "journeys" );
    dataSource.insert( "receivedPossibleStopList", false );
    dataSource.insert( "error", false );
    dataSource.insert( "updated", QDateTime::currentDateTime() );
    m_dataSources.insert( sourceName, dataSource );
}

void PublicTransportEngine::stopListReceived( TimetableAccessor *accessor,
					      const QUrl &requestUrl,
					      const QStringList &stops,
					      const QHash<QString, QString> &stopToStopId,
					      const QHash<QString, int> &stopToStopWeight,
					      const QString &serviceProvider,
					      const QString &sourceName,
					      const QString &city,
					      const QString &stop,
					      const QString &dataType,
					      ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
    Q_UNUSED(stop);
    Q_UNUSED(dataType);
    kDebug() << stops.count() << "stops received" << dataType << parseDocumentMode << sourceName;
//     QString sStop = stopToStopId.value( stop, stop );
//     if ( sStop.isEmpty() )
// 	sStop = stop;

    int i = 0;
    foreach ( const QString &stopName, stops ) {
	QHash<QString, QVariant> data;
	data.insert("stopName", stopName);
	if ( stopToStopId.contains(stopName) )
	    data.insert("stopID", stopToStopId.value(stopName, "") );
	if ( stopToStopWeight.contains(stopName) )
	    data.insert("stopWeight", stopToStopWeight.value(stopName, 0) );
	
// 	kDebug() << "setData" << i << data;
	setData( sourceName, QString("stopName %1").arg(i++), data );
    }

    // Remove values from an old possible stop list
    for ( i = stops.count(); i < m_lastStopNameCount; ++i )
	removeData( sourceName, QString("stopName %1").arg(i) );
    m_lastStopNameCount = stops.count();
    
    setData( sourceName, "serviceProvider", serviceProvider );
    setData( sourceName, "count", stops.count() );
    setData( sourceName, "requestUrl", requestUrl );
    if ( parseDocumentMode == ParseForDeparturesArrivals )
	setData( sourceName, "parseMode", "departures" );
    else if ( parseDocumentMode == ParseForJourneys )
	setData( sourceName, "parseMode", "journeys" );
    else if ( parseDocumentMode == ParseForStopSuggestions )
	setData( sourceName, "parseMode", "stopSuggestions" );
    setData( sourceName, "receivedData", "stopList" );
    setData( sourceName, "receivedPossibleStopList", true );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // TODO: add to m_dataSources?
}

void PublicTransportEngine::errorParsing( TimetableAccessor *accessor,
					  const QUrl &requestUrl,
					  const QString &serviceProvider,
					  const QString &sourceName,
					  const QString &city, const QString &stop,
					  const QString &dataType,
					  ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
    Q_UNUSED(stop);
    Q_UNUSED(dataType);
    kDebug() << "Error while parsing" << requestUrl
	     << "\n  sourceName =" << sourceName << ", " << dataType << parseDocumentMode;

    setData( sourceName, "serviceProvider", serviceProvider );
    setData( sourceName, "count", 0 );
    setData( sourceName, "requestUrl", requestUrl );
    if ( parseDocumentMode == ParseForDeparturesArrivals )
	setData( sourceName, "parseMode", "departures" );
    else if ( parseDocumentMode == ParseForJourneys )
	setData( sourceName, "parseMode", "journeys" );
    else if ( parseDocumentMode == ParseForStopSuggestions )
	setData( sourceName, "parseMode", "stopSuggestions" );
    setData( sourceName, "receivedData", "nothing" );
    setData( sourceName, "error", true );
    setData( sourceName, "updated", QDateTime::currentDateTime() );
}

bool PublicTransportEngine::isSourceUpToDate( const QString& name ) {
    if ( !m_dataSources.keys().contains(name) )
	return false;
    
    // Data source stays up to date for min(UPDATE_TIMEOUT, minFetchWait) seconds
    QHash<QString, QVariant> dataSource = m_dataSources[name].toHash();
    
    TimetableAccessor *accessor;
    QString serviceProvider = dataSource[ "serviceProvider" ].toString();
    if ( !m_accessors.contains(serviceProvider) ) {
	accessor = TimetableAccessor::getSpecificAccessor(serviceProvider);
	m_accessors.insert( serviceProvider, accessor );
    } else
	accessor = m_accessors.value(serviceProvider);
    int minFetchWait = qMax( accessor->minFetchWait(), UPDATE_TIMEOUT );

    return dataSource["updated"].toDateTime().secsTo(
	    QDateTime::currentDateTime() ) < minFetchWait;
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE(publictransport, PublicTransportEngine)

// this is needed since PublicTransportEngine is a QObject
#include "build/publictransportdataengine.moc"
