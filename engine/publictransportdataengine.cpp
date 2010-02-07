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

// Qt includes
#include <QFileSystemWatcher>

// KDE includes
#include <Plasma/DataContainer>
#include <KStandardDirs>

// Own includes
#include "publictransportdataengine.h"
#include "timetableaccessor.h"



PublicTransportEngine::PublicTransportEngine(QObject* parent, const QVariantList& args)
			    : Plasma::DataEngine(parent, args),
			    m_fileSystemWatcher(0) {
    // We ignore any arguments - data engines do not have much use for them
    Q_UNUSED( args )

    m_lastJourneyCount = 0;
    m_lastStopNameCount = 0;

    // This prevents applets from setting an unnecessarily high update interval
    // and using too much CPU.
    // 30 seconds should be enough, departure / arrival times have minute precision.
    setMinimumPollingInterval( 30000 );

    // Add service provider source, so when using
    // dataEngine("publictransport").sources() in an applet it at least returns this
    setData( sourceTypeKeyword(ServiceProviders), DataEngine::Data() );
    setData( sourceTypeKeyword(ErrornousServiceProviders), DataEngine::Data() );
    setData( sourceTypeKeyword(Locations), DataEngine::Data() );
}

PublicTransportEngine::~PublicTransportEngine() {
    qDeleteAll( m_accessors.values() );
    delete m_fileSystemWatcher;
}

QHash< QString, QVariant > PublicTransportEngine::serviceProviderInfo(
			    const TimetableAccessor *&accessor ) {
    Q_ASSERT( accessor != NULL );

    QHash<QString, QVariant> dataServiceProvider;
    dataServiceProvider.insert( "id", accessor->serviceProvider() );
    dataServiceProvider.insert( "fileName", accessor->timetableAccessorInfo().fileName() );
    dataServiceProvider.insert( "scriptFileName", accessor->timetableAccessorInfo().scriptFileName() );
    dataServiceProvider.insert( "name", accessor->timetableAccessorInfo().name() );
    dataServiceProvider.insert( "url", accessor->timetableAccessorInfo().url() );
    dataServiceProvider.insert( "shortUrl", accessor->timetableAccessorInfo().shortUrl() );
    dataServiceProvider.insert( "country", accessor->country() );
    dataServiceProvider.insert( "cities", accessor->cities() );
    dataServiceProvider.insert( "credit", accessor->credit() );
    dataServiceProvider.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
    dataServiceProvider.insert( "onlyUseCitiesInList", accessor->onlyUseCitiesInList() );
    dataServiceProvider.insert( "features", accessor->features() );
    dataServiceProvider.insert( "featuresLocalized", accessor->featuresLocalized() );
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
    locationHash.insert( "description", i18n("Support for all cities in Germany (and limited support for cities in europe). There is also support for providers specific to regions / cities.") );
    locationHash.insert( "defaultAccessor", "de_db" );
    ret.insert( name, locationHash );
    
    locationHash.clear();
    locationHash.insert( "name", name = "fr" );
    locationHash.insert( "description", i18n("Support for some cities in France. No local public transportation information.") );
    locationHash.insert( "defaultAccessor", "fr_gares" );
    ret.insert( name, locationHash );
    
    locationHash.clear();
    locationHash.insert( "name", name = "it" );
    locationHash.insert( "description", i18n("Support for some cities in Italia.") );
    locationHash.insert( "defaultAccessor", "it_cup2000" );
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
    if ( m_dataSources.contains(name) ) {
// 	kDebug() << "Data source" << name << "is up to date";
	dataSource = m_dataSources[ name ].toHash();
    } else {
	if ( !m_fileSystemWatcher ) {
	    QStringList dirList = KGlobal::dirs()->findDirs( "data",
		    "plasma_engine_publictransport/accessorInfos" );
	    m_fileSystemWatcher = new QFileSystemWatcher( dirList );
	    connect( m_fileSystemWatcher, SIGNAL(directoryChanged(QString)),
		     this, SLOT(accessorInfoDirChanged(QString)) );
	}

	QStringList fileNames = KGlobal::dirs()->findAllResources( "data",
			"plasma_engine_publictransport/accessorInfos/*.xml" );
	if ( fileNames.isEmpty() ) {
	    kDebug() << "Couldn't find any service provider information XML files";
	    return false;
	}

	kDebug() << "Check accessors";
	QStringList loadedAccessors;
	m_errornousAccessors.clear();
	foreach( QString fileName, fileNames ) {
	    QString s = KUrl(fileName).fileName().remove(QRegExp("\\..*$")); // Remove file extension
	    const TimetableAccessor *accessor = TimetableAccessor::getSpecificAccessor( s );
	    if ( accessor ) {
		dataSource.insert( accessor->timetableAccessorInfo().name(),
				   serviceProviderInfo(accessor) );
		loadedAccessors << s;

		delete accessor;
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
	SourceType sourceType = sourceTypeFromName( name );
	if ( sourceType == Departures ) {
	    parameters = name.mid( sourceTypeKeyword(Departures).length() );
	    parseDocumentMode = ParseForDeparturesArrivals;
	    dataType = "departures";
	} else if ( sourceType == Arrivals ) {
	    parameters = name.mid( sourceTypeKeyword(Arrivals).length() );
	    parseDocumentMode = ParseForDeparturesArrivals;
	    dataType = "arrivals";
	} else if ( sourceType == Stops ) {
	    parameters = name.mid( sourceTypeKeyword(Stops).length() );
	    parseDocumentMode = ParseForStopSuggestions;
	    dataType = "stopSuggestions";
	} else if ( sourceType == JourneysDep ) {
	    parameters = name.mid( sourceTypeKeyword(JourneysDep).length() );
	    parseDocumentMode = ParseForJourneys;
	    dataType = "journeysDep";
	} else if ( sourceType == JourneysArr ) {
	    parameters = name.mid( sourceTypeKeyword(JourneysArr).length() );
	    parseDocumentMode = ParseForJourneys;
	    dataType = "journeysArr";
	} else if ( sourceType == Journeys ) {
	    parameters = name.mid( sourceTypeKeyword(Journeys).length() );
	    parseDocumentMode = ParseForJourneys;
	    dataType = "journeysDep";
	} else
	    return false;
	
	input = parameters.trimmed().split("|", QString::SkipEmptyParts);
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
	    accessor = TimetableAccessor::getSpecificAccessor( serviceProvider );
	    m_accessors.insert( serviceProvider, accessor );
	    newlyCreated = true;
	} else {
	    accessor = m_accessors.value( serviceProvider );
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
		     SIGNAL(departureListReceived(TimetableAccessor*,const QUrl&,const QList<DepartureInfo*>&,const GlobalTimetableInfo&,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)),
		     this, SLOT(departureListReceived(TimetableAccessor*,const QUrl&,const QList<DepartureInfo*>&,const GlobalTimetableInfo&,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)) );
	    // 		} else { // if ( parseDocumentMode == ParseForJourneys )
	    connect( accessor,
		     SIGNAL(journeyListReceived(TimetableAccessor*,const QUrl&,const QList<JourneyInfo*>&,const GlobalTimetableInfo&,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)),
		     this, SLOT(journeyListReceived(TimetableAccessor*,const QUrl&,const QList<JourneyInfo*>&,const GlobalTimetableInfo&,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)) );
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

QString PublicTransportEngine::stripDateAndTimeValues( const QString& sourceName ) {
    QString ret = sourceName;
    QRegExp rx( "(time=[^\\|]*|datetime=[^\\|]*)", Qt::CaseInsensitive );
    rx.setMinimal( true );
    ret.replace( rx, "" );
    return ret;
}

void PublicTransportEngine::accessorInfoDirChanged( QString path ) {
    kDebug() << path << "was changed";

    // Remove all accessors (could have been changed)
    qDeleteAll( m_accessors );
    m_accessors.clear();
    
    // Clear all cached data (use the new accessor to parse the data again)
    QStringList cachedSources = m_dataSources.keys();
    foreach ( QString cachedSource, cachedSources ) {
	SourceType sourceType = sourceTypeFromName( cachedSource );
	if ( isDataRequestingSourceType(sourceType) )
	    m_dataSources.remove( cachedSource );
    }

    // Remove cached service provider source
    const QString serviceProvidersKey = sourceTypeKeyword( ServiceProviders );
    if ( m_dataSources.keys().contains(serviceProvidersKey) )
	m_dataSources.remove( serviceProvidersKey );
    
    updateServiceProviderSource( serviceProvidersKey );
}

const QString PublicTransportEngine::sourceTypeKeyword( SourceType sourceType ) {
    switch ( sourceType ) {
	case ServiceProviders:
	    return "ServiceProviders";
	case ErrornousServiceProviders:
	    return "ErrornousServiceProviders";
	case Locations:
	    return "Locations";
	case Departures:
	    return "Departures";
	case Arrivals:
	    return "Arrivals";
	case Stops:
	    return "Stops";
	case Journeys:
	    return "Journeys";
	case JourneysDep:
	    return "JourneysDep";
	case JourneysArr:
	    return "JourneysArr";
	    
	default:
	    return "";
    }
}

PublicTransportEngine::SourceType PublicTransportEngine::sourceTypeFromName(
		const QString& sourceName ) const {
    if ( sourceName.compare(sourceTypeKeyword(ServiceProviders), Qt::CaseInsensitive) == 0 )
	return ServiceProviders;
    else if ( sourceName.compare(sourceTypeKeyword(ErrornousServiceProviders),
				 Qt::CaseInsensitive) == 0 )
	return ErrornousServiceProviders;
    else if ( sourceName.compare(sourceTypeKeyword(Locations), Qt::CaseInsensitive) == 0 )
	return Locations;
    else if ( sourceName.startsWith(sourceTypeKeyword(Departures), Qt::CaseInsensitive) )
	return Departures;
    else if ( sourceName.startsWith(sourceTypeKeyword(Arrivals), Qt::CaseInsensitive) )
	return Arrivals;
    else if ( sourceName.startsWith(sourceTypeKeyword(Stops), Qt::CaseInsensitive) )
	return Stops;
    else if ( sourceName.startsWith(sourceTypeKeyword(JourneysDep), Qt::CaseInsensitive) )
	return JourneysDep;
    else if ( sourceName.startsWith(sourceTypeKeyword(JourneysArr), Qt::CaseInsensitive) )
	return JourneysArr;
    else if ( sourceName.startsWith(sourceTypeKeyword(Journeys), Qt::CaseInsensitive) )
	return Journeys;
    else
	return InvalidSourceName;
}

bool PublicTransportEngine::updateSourceEvent( const QString &name ) {
    kDebug() << name;

    bool ret = true;
    SourceType sourceType = sourceTypeFromName( name );
    switch ( sourceType ) {
	case ServiceProviders:
	    ret = updateServiceProviderSource( name );
	    break;
	case ErrornousServiceProviders:
	    updateErrornousServiceProviderSource( name );
	    break;
	case Locations:
	    updateLocationSource( name );
	    break;
	case Departures:
	case Arrivals:
	case Stops:
	case Journeys:
	    ret = updateDepartureOrJourneySource( name );
	    break;
	default:
	    kDebug() << "Source name incorrect" << name;
	    ret = false;
	    break;
    }

    return ret;
}

void PublicTransportEngine::departureListReceived( TimetableAccessor *accessor,
		const QUrl &requestUrl,
		const QList<DepartureInfo*> &departures,
		const GlobalTimetableInfo &globalInfo,
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
	data.insert("routeStops", departureInfo->routeStops());
	data.insert("routeTimes", departureInfo->routeTimesVariant());
	data.insert("routeExactStops", departureInfo->routeExactStops());

	QString sKey = QString( "%1" ).arg( i++ );
	setData( sourceName, sKey, data );
	dataSource.insert( sKey, data );
// 	kDebug() << "setData" << sourceName << data;
    }
    int departureCount = departures.count();
    QDateTime first, last;
    if ( departureCount > 0 ) {
	first = departures.first()->departure();
	last = departures.last()->departure();
    } else
	first = last = QDateTime::currentDateTime();
    qDeleteAll( departures );

    // Remove old jouneys
    for ( ; i < m_lastJourneyCount; ++i )
	removeData( sourceName, QString("%1").arg(i++) );
    m_lastJourneyCount = departures.count();

    // Remove old stop suggestions
    for ( i = 0 ; i < m_lastStopNameCount; ++i )
	removeData( sourceName, QString("stopName %1").arg(i++) );
    m_lastStopNameCount = 0;

    // Store a proposal for the next download time
    int secs = QDateTime::currentDateTime().secsTo( last ) / 3;
    QDateTime downloadTime = QDateTime::currentDateTime().addSecs( secs );
    m_nextDownloadTimeProposals[ stripDateAndTimeValues(sourceName) ] = downloadTime;
//     kDebug() << "Set next download time proposal:" << downloadTime;
    
    setData( sourceName, "serviceProvider", serviceProvider );
    setData( sourceName, "count", departureCount );
    setData( sourceName, "delayInfoAvailable", globalInfo.delayInfoAvailable );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", "departures" );
    setData( sourceName, "receivedData", "departures" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "serviceProvider", serviceProvider );
    dataSource.insert( "count", departureCount );
    dataSource.insert( "delayInfoAvailable", globalInfo.delayInfoAvailable );
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
						 const QList<JourneyInfo*> &journeys,
						 const GlobalTimetableInfo &globalInfo,
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
    foreach ( JourneyInfo *journeyInfo, journeys ) {
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
	data.insert("routeStops", journeyInfo->routeStops());
	data.insert("routeTimesDeparture", journeyInfo->routeTimesDepartureVariant());
	data.insert("routeTimesArrival", journeyInfo->routeTimesArrivalVariant());
	data.insert("routeExactStops", journeyInfo->routeExactStops());
	data.insert("routeVehicleTypes", journeyInfo->routeVehicleTypesVariant());
	data.insert("routeTransportLines", journeyInfo->routeTransportLines());
	data.insert("routePlatformsDeparture", journeyInfo->routePlatformsDeparture());
	data.insert("routePlatformsArrival", journeyInfo->routePlatformsArrival());
	data.insert("routeTimesDepartureDelay", journeyInfo->routeTimesDepartureDelay());
	data.insert("routeTimesArrivalDelay", journeyInfo->routeTimesArrivalDelay());

	QString sKey = QString("%1").arg(i++);
	setData( sourceName, sKey, data );
	dataSource.insert( sKey, data );
	// 	kDebug() << "setData" << sourceName << data;
    }
    int journeyCount = journeys.count();
    QDateTime first, last;
    if ( journeyCount > 0 ) {
	first = journeys.first()->departure();
	last = journeys.last()->departure();
    } else
	first = last = QDateTime::currentDateTime();
    qDeleteAll( journeys );

    // Remove old journeys
    for ( ; i < m_lastJourneyCount; ++i )
	removeData( sourceName, QString("%1").arg(i++) );
    m_lastJourneyCount = journeys.count();

    // Remove old stop suggestions
    for ( i = 0 ; i < m_lastStopNameCount; ++i )
	removeData( sourceName, QString("stopName %1").arg(i++) );
    m_lastStopNameCount = 0;
    
    // Store a proposal for the next download time
    int secs = (journeyCount / 3) * first.secsTo( last );
    QDateTime downloadTime = QDateTime::currentDateTime().addSecs( secs );
    m_nextDownloadTimeProposals[ stripDateAndTimeValues(sourceName) ] = downloadTime;
    
    setData( sourceName, "serviceProvider", serviceProvider );
    setData( sourceName, "count", journeyCount );
    setData( sourceName, "delayInfoAvailable", globalInfo.delayInfoAvailable );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", "journeys" );
    setData( sourceName, "receivedData", "journeys" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "serviceProvider", serviceProvider );
    dataSource.insert( "count", journeyCount );
    dataSource.insert( "delayInfoAvailable", globalInfo.delayInfoAvailable );
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
    if ( !m_dataSources.contains(name) )
	return false;
    
    // Data source stays up to date for max(UPDATE_TIMEOUT, minFetchWait) seconds
    QHash<QString, QVariant> dataSource = m_dataSources[ name ].toHash();
    
    TimetableAccessor *accessor;
    QString serviceProvider = dataSource[ "serviceProvider" ].toString();
    if ( !m_accessors.contains(serviceProvider) ) {
	accessor = TimetableAccessor::getSpecificAccessor( serviceProvider );
	m_accessors.insert( serviceProvider, accessor );
    } else
	accessor = m_accessors.value( serviceProvider );

    QDateTime downloadTime = m_nextDownloadTimeProposals[ stripDateAndTimeValues(name) ];
    int minForSufficientChanges = downloadTime.isValid()
	    ? QDateTime::currentDateTime().secsTo(downloadTime) : 0;
    int minFetchWait = qMax( minForSufficientChanges, MIN_UPDATE_TIMEOUT );
    
    // If delays are available set maximum fetch wait
    if ( accessor->features().contains("Delay")
		&& dataSource["delayInfoAvailable"].toBool() )
	minFetchWait = qMin( minFetchWait, MAX_UPDATE_TIMEOUT_DELAY );
    
    minFetchWait = qMax( minFetchWait, accessor->minFetchWait() );
    kDebug() << "Wait time until next download:"
	     << ((minFetchWait - dataSource["updated"].toDateTime().secsTo(
		 QDateTime::currentDateTime())) / 60) << "min";

    return dataSource["updated"].toDateTime().secsTo(
			QDateTime::currentDateTime() ) < minFetchWait;
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE(publictransport, PublicTransportEngine)

// this is needed since PublicTransportEngine is a QObject
#include "build/publictransportdataengine.moc"
