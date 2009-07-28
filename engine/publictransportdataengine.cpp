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

#include <QtXml>

#include <Plasma/DataContainer>
#include <KStandardDirs>

#include "publictransportdataengine.h"



PublicTransportEngine::PublicTransportEngine(QObject* parent, const QVariantList& args)
    : Plasma::DataEngine(parent, args)
{
    // We ignore any arguments - data engines do not have much use for them
    Q_UNUSED(args)

    m_lastJourneyCount = 0;
    m_lastStopNameCount = 0;

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

QHash< QString, QVariant > PublicTransportEngine::serviceProviderInfo ( const TimetableAccessor *&accessor ) {
    Q_ASSERT( accessor != NULL );

    QHash<QString, QVariant> dataServiceProvider;
    dataServiceProvider.insert( "id", /*static_cast<int>(*/accessor->serviceProvider()/*)*/ );
    dataServiceProvider.insert( "fileName", accessor->timetableAccessorInfo().fileName() );
    dataServiceProvider.insert( "name", accessor->timetableAccessorInfo().name() );
    dataServiceProvider.insert( "shortUrl", accessor->timetableAccessorInfo().shortUrl() );
    dataServiceProvider.insert( "country", accessor->country() );
    dataServiceProvider.insert( "cities", accessor->cities() );
    dataServiceProvider.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
    dataServiceProvider.insert( "onlyUseCitiesInList", accessor->onlyUseCitiesInList() );
    dataServiceProvider.insert( "features", accessor->features() );
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
    qDebug() << "PublicTransportEngine::sourceRequestEvent" << name;

    setData( name, DataEngine::Data() ); // Create source, TODO: check if [name] is valid
    return updateSourceEvent(name);
}

bool PublicTransportEngine::updateServiceProviderSource( const QString &name ) {
    QHash<QString, QVariant> dataSource;
    if ( m_dataSources.keys().contains(name) ) {
	// 	    qDebug() << "PublicTransportEngine::updateSourceEvent" << "Data source" << name << "is up to date";
	dataSource = m_dataSources[name].toHash();
    } else {
	QStringList fileNames = KGlobal::dirs()->findAllResources( "data", "plasma_engine_publictransport/accessorInfos/*.xml" );
	if ( fileNames.isEmpty() ) {
	    qDebug() << "PublicTransportEngine::updateSourceEvent"
		<< "Couldn't find any service provider information XML files";
	    return false;
	}

	QStringList loadedAccessors;
	m_errornousAccessors.clear();
	foreach( QString fileName, fileNames ) {
	    QString s = KUrl(fileName).fileName().remove(QRegExp("\\..*$")); // Remove file extension
	    const TimetableAccessor *accessor = TimetableAccessor::getSpecificAccessor( s );
	    if ( accessor ) {
		dataSource.insert( accessor->timetableAccessorInfo().name(), serviceProviderInfo(accessor) );
		loadedAccessors << s;
	    }
	    else {
		m_errornousAccessors << s;
	    }
	}

	qDebug() << "PublicTransportEngine::updateSourceEvent"
	<< "Loaded accessors from XML files:" << loadedAccessors;
	qDebug() << "PublicTransportEngine::updateSourceEvent"
	<< "Errornous accessor info XMLs, that couldn't be loaded::" << m_errornousAccessors;

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
    bool containsDataSource = m_dataSources.keys().contains(name);
    if ( containsDataSource && isSourceUpToDate(name) )
    { // Data is stored in the map and up to date
	qDebug() << "PublicTransportEngine::updateSourceEvent" << "Data source" << name << "is up to date";
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

	qDebug() << "PublicTransportEngine::updateSourceEvent" << name;

	QString parameters;
	if ( name.startsWith(I18N_NOOP("Departures")) ) {
	    parameters = name.mid( QString(I18N_NOOP("Departures")).length() );
	    parseDocumentMode = ParseForDeparturesArrivals;
	    dataType = "departures";
	} else if ( name.startsWith(I18N_NOOP("Arrivals")) ) {
	    parameters = name.mid( QString(I18N_NOOP("Arrivals")).length() );
	    parseDocumentMode = ParseForDeparturesArrivals;
	    dataType = "arrivals";
	} else if ( name.startsWith(I18N_NOOP("Journeys")) ) {
	    parameters = name.mid( QString(I18N_NOOP("Journeys")).length() );
	    parseDocumentMode = ParseForJourneys;
	    dataType = "journeys";
	}
	input = parameters.trimmed().split("|", QString::SkipEmptyParts);

// 	    if ( input.length() < 2 ) {
// 		qDebug() << "PublicTransportEngine::updateSourceEvent" << QString("Wrong input: '%1'").arg(name);
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

	if ( parseDocumentMode == ParseForDeparturesArrivals ) {
	    if ( stop.isEmpty() ) {
		qDebug() << "PublicTransportEngine::updateSourceEvent" << "Stop name is missing in data source name";
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
	    qDebug() << "PublicTransportEngine::updateSourceEvent" << QString("Accessor %1 couldn't be created").arg(serviceProvider);
	    return false; // accessor couldn't be created
	} else if ( accessor->useSeperateCityValue() && city.isEmpty() ) {
	    qDebug() << "PublicTransportEngine::updateSourceEvent" << QString("Accessor %1 needs a seperate city value. Add to source name '|city=X', where X stands for the city name.").arg(serviceProvider);
	    return false; // accessor needs a seperate city value
	}

	if ( newlyCreated ) {
	    // 		if ( parseDocumentMode == ParseForDeparturesArrivals ) {
	    connect( accessor, SIGNAL(departureListReceived(TimetableAccessor*,QList<DepartureInfo*>,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)),
		this, SLOT(departureListReceived(TimetableAccessor*,QList<DepartureInfo*>,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)) );
	    // 		} else { // if ( parseDocumentMode == ParseForJourneys )
	    connect( accessor, SIGNAL(journeyListReceived(TimetableAccessor*,QList<JourneyInfo*>,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)),
		this, SLOT(journeyListReceived(TimetableAccessor*,QList<JourneyInfo*>,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)) );
	    // 		}
	    connect( accessor, SIGNAL(stopListReceived(TimetableAccessor*,QHash<QString,QString>,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)),
		this, SLOT(stopListReceived(TimetableAccessor*,QHash<QString,QString>,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)) );
	    connect( accessor, SIGNAL(errorParsing(TimetableAccessor*,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)),
		this, SLOT(errorParsing(TimetableAccessor*,const QString&,const QString&,const QString&,const QString&,const QString&,ParseDocumentMode)) );
	}

	if ( parseDocumentMode == ParseForDeparturesArrivals )
	    accessor->requestDepartures( name, city, stop, maxDeps, dateTime, dataType );
	else // if ( parseDocumentMode == ParseForJourneys )
	    accessor->requestJourneys( name, city, originStop, targetStop, maxDeps, dateTime, dataType );
    }

    return true;
}

bool PublicTransportEngine::updateSourceEvent( const QString &name ) {
    qDebug() << "PublicTransportEngine::updateSourceEvent" << name;

    bool ret = true;
    if ( name == I18N_NOOP("ServiceProviders") )
	ret = updateServiceProviderSource( name );
    else if ( name == I18N_NOOP("ErrornousServiceProviders") )
	updateErrornousServiceProviderSource( name );
    else if ( name == I18N_NOOP("Locations") )
	updateLocationSource( name );
    else if ( name.startsWith(I18N_NOOP("Departures")) ||  name.startsWith(I18N_NOOP("Arrivals")) ||
		name.startsWith(I18N_NOOP("Journeys")) )
	ret = updateDepartureOrJourneySource( name );
    else {
	qDebug() << "PublicTransportEngine::updateSourceEvent" << "Source name incorrect" << name;
	return false;
    }

    return ret;
}

void PublicTransportEngine::departureListReceived( TimetableAccessor *accessor, QList< DepartureInfo* > departures, const QString &serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
    Q_UNUSED(stop);
    Q_UNUSED(dataType);
    Q_UNUSED(parseDocumentMode);
    qDebug() << "PublicTransportEngine::departureListReceived" << departures.count() << "departures / arrivals received";

    int i = 0;
    m_dataSources.remove( sourceName );
    QHash<QString, QVariant> dataSource;
    foreach ( DepartureInfo *departureInfo, departures ) {
// 	if ( !departureInfo->isValid() ) {
// 	    qDebug() << "PublicTransportEngine::departureListReceived" << "Departure isn't valid" << departureInfo->line();
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

	QString sKey = QString("%1").arg(i++);
	setData( sourceName, sKey, data );
	dataSource.insert( sKey, data );
// 	qDebug() << "PublicTransportEngine::departureListReceived" << "setData" << sourceName << data;
    }

    // Remove old jouneys
    for ( ; i < m_lastJourneyCount; ++i )
	removeData( sourceName, QString("%1").arg(i++) );
    m_lastJourneyCount = departures.count();

    // Remove old stop suggestions
    for ( i = 0 ; i < m_lastStopNameCount; ++i )
	removeData( sourceName, QString("stopName %1").arg(i++) );
    m_lastStopNameCount = 0;

    setData( sourceName, "count", departures.count() );
    setData( sourceName, "parseMode", "departures" );
    setData( sourceName, "receivedData", "departures" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "count", departures.count() );
    dataSource.insert( "parseMode", "departures" );
    dataSource.insert( "receivedData", "departures" );
    dataSource.insert( "receivedPossibleStopList", false );
    dataSource.insert( "error", false );
    dataSource.insert( "updated", QDateTime::currentDateTime() );
    m_dataSources.insert( sourceName, dataSource );
}


void PublicTransportEngine::journeyListReceived ( TimetableAccessor* accessor, QList< JourneyInfo* > journeys, const QString &serviceProvider, const QString& sourceName, const QString& city, const QString& stop, const QString& dataType, ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
    Q_UNUSED(stop);
    Q_UNUSED(dataType);
    Q_UNUSED(parseDocumentMode);
    qDebug() << "PublicTransportEngine::journeyListReceived" << journeys.count() << "journeys received";

    int i = 0;
    m_dataSources.remove( sourceName );
    QHash<QString, QVariant> dataSource;
    foreach ( JourneyInfo *journeyInfo, journeys )
    {
// 	qDebug() << journeyInfo->isValid() << journeyInfo->departure() << journeyInfo->duration() << journeyInfo->changes() << journeyInfo->vehicleTypes() << journeyInfo->startStopName() << journeyInfo->targetStopName() << journeyInfo->arrival();
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

	QString sKey = QString("%1").arg(i++);
	setData( sourceName, sKey, data );
	dataSource.insert( sKey, data );
	// 	qDebug() << "PublicTransportEngine::departureListReceived" << "setData" << sourceName << data;
    }

    // Remove old journeys
    for ( ; i < m_lastJourneyCount; ++i )
	removeData( sourceName, QString("%1").arg(i++) );
    m_lastJourneyCount = journeys.count();

    // Remove old stop suggestions
    for ( i = 0 ; i < m_lastStopNameCount; ++i )
	removeData( sourceName, QString("stopName %1").arg(i++) );
    m_lastStopNameCount = 0;

    setData( sourceName, "count", journeys.count() );
    setData( sourceName, "parseMode", "journeys" );
    setData( sourceName, "receivedData", "journeys" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "count", journeys.count() );
    dataSource.insert( "parseMode", "journeys" );
    dataSource.insert( "receivedData", "journeys" );
    dataSource.insert( "receivedPossibleStopList", false );
    dataSource.insert( "error", false );
    dataSource.insert( "updated", QDateTime::currentDateTime() );
    m_dataSources.insert( sourceName, dataSource );
}

void PublicTransportEngine::stopListReceived( TimetableAccessor *accessor, QHash<QString, QString> stops, const QString &serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
    Q_UNUSED(dataType);
    qDebug() << "PublicTransportEngine::stopListReceived" << stops.count() << "stops received";
    QString sStop = stops.value( stop, stop );
    if ( sStop.isEmpty() )
	sStop = stop;

    int i = 0;
    foreach ( const QString &stopName, stops.keys() )
    {
	QHash<QString, QVariant> data;
	data.insert("stopName", stopName);
	data.insert("stopID", stops.value(stopName, "") );

	setData( sourceName, QString("stopName %1").arg(i++), data );
// 	qDebug() << "PublicTransportEngine::journeyListReceived" << "setData" << sourceName << data;
    }

    // Remove values from an old possible stop list
    for ( i = stops.count(); i < m_lastStopNameCount; ++i )
	removeData( sourceName, QString("stopName %1").arg(i) );
    m_lastStopNameCount = stops.count();

    setData( sourceName, "count", stops.count() );
    setData( sourceName, "parseMode", parseDocumentMode == ParseForDeparturesArrivals ? "departures" : "journeys" );
    setData( sourceName, "receivedData", "stopList" );
    setData( sourceName, "receivedPossibleStopList", true );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // TODO: add to m_dataSources?
}

void PublicTransportEngine::errorParsing( TimetableAccessor *accessor, const QString &serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
    Q_UNUSED(stop);
    Q_UNUSED(dataType);
    qDebug() << "PublicTransportEngine::errorParsing";

    setData( sourceName, "count", 0 );
    setData( sourceName, "parseMode", parseDocumentMode == ParseForDeparturesArrivals ? "departures" : "journeys" );
    setData( sourceName, "receivedData", "nothing" );
    setData( sourceName, "error", true );
    setData( sourceName, "updated", QDateTime::currentDateTime() );
}

bool PublicTransportEngine::isSourceUpToDate ( const QString& name ) {
    if ( !m_dataSources.keys().contains(name) )
	return false;
    // Data source stays up to date for UPDATE_TIMEOUT seconds
    QHash<QString, QVariant> dataSource = m_dataSources[name].toHash();
    return dataSource["updated"].toDateTime().secsTo( QDateTime::currentDateTime() ) < UPDATE_TIMEOUT;
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE(publictransport, PublicTransportEngine)

// this is needed since PublicTransportEngine is a QObject
#include "build/publictransportdataengine.moc"
