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

#include "publictransportdataengine.h"
#include <Plasma/DataContainer>

PublicTransportEngine::PublicTransportEngine(QObject* parent, const QVariantList& args)
    : Plasma::DataEngine(parent, args)
{
    // We ignore any arguments - data engines do not have much use for them
    Q_UNUSED(args)

    // This prevents applets from setting an unnecessarily high
    // update interval and using too much CPU.
    // 30 seconds should be enough, departure times have minute precision
    setMinimumPollingInterval( 30000 );

    // Add service provider source, so when using dataEngine("publictransport").sources() in an applet it at least returns this
    setData( "ServiceProviders", DataEngine::Data() );
}

QMap< QString, QVariant > PublicTransportEngine::serviceProviderInfo ( const TimetableAccessor *&accessor ) {
    Q_ASSERT( accessor != NULL );

    QMap<QString, QVariant> dataServiceProvider;
    dataServiceProvider.insert( "id", static_cast<int>(accessor->serviceProvider()) );
    dataServiceProvider.insert( "name", accessor->timetableAccessorInfo().name() );
    dataServiceProvider.insert( "shortUrl", accessor->timetableAccessorInfo().shortUrl() );
    dataServiceProvider.insert( "country", accessor->country() );
    dataServiceProvider.insert( "cities", accessor->cities() );
    dataServiceProvider.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
    dataServiceProvider.insert( "features", accessor->features() );
    dataServiceProvider.insert( "url", accessor->timetableAccessorInfo().url() );
    dataServiceProvider.insert( "author", accessor->timetableAccessorInfo().author() );
    dataServiceProvider.insert( "email", accessor->timetableAccessorInfo().email() );
    dataServiceProvider.insert( "description", accessor->timetableAccessorInfo().description() );
    dataServiceProvider.insert( "version", accessor->timetableAccessorInfo().version() );
    return dataServiceProvider;
}

bool PublicTransportEngine::sourceRequestEvent( const QString &name ) {
//     qDebug() << "PublicTransportEngine::sourceRequestEvent" << name;

    setData( name, DataEngine::Data() ); // Create source, TODO: check if [name] is valid
    return updateSourceEvent(name);
}

bool PublicTransportEngine::updateSourceEvent( const QString &name ) {
//     qDebug() << "PublicTransportEngine::updateSourceEvent" << name;

    if ( name == I18N_NOOP("ServiceProviders") )
    {
	QMap<QString, QVariant> dataSource;
	if ( m_dataSources.keys().contains(name) ) {
// 	    qDebug() << "PublicTransportEngine::updateSourceEvent" << "Data source" << name << "is up to date";
	    dataSource = m_dataSources[name].toMap();
	} else {
	    for ( std::vector<ServiceProvider>::const_iterator it=availableServiceProviders.begin(); it < availableServiceProviders.end(); ++it ) {
		const TimetableAccessor *accessor = TimetableAccessor::getSpecificAccessor( *it );
		dataSource.insert( accessor->timetableAccessorInfo().name(), serviceProviderInfo(accessor) );
	    }

	    m_dataSources.insert( name, dataSource );
	}

	foreach ( QString key, dataSource.keys() )
	    setData( name, key, dataSource[key] );
    } else if ( name.startsWith(I18N_NOOP("Departures")) ||  name.startsWith(I18N_NOOP("Arrivals")) )
    { // Departures 0:Bremen:Pappelstraße
	if ( m_dataSources.keys().contains(name) && isSourceUpToDate(name) )
	{ // Data is stored in the map and up to date
	    qDebug() << "PublicTransportEngine::updateSourceEvent" << "Data source" << name << "is up to date";
	    QMap<QString, QVariant> dataSource = m_dataSources[name].toMap();
	    foreach ( QString key, dataSource.keys() )
		setData( name, key, dataSource[key] );
	}
	else
	{ // Request new data
	    QStringList input;
	    if ( name.startsWith(I18N_NOOP("Departures")) )
		input = name.mid( QString("Departures").length() ).trimmed().split(":", QString::SkipEmptyParts);
	    else if ( name.startsWith(I18N_NOOP("Arrivals")) )
		input = name.mid( QString("Arrivals").length() ).trimmed().split(":", QString::SkipEmptyParts);
	    if ( input.length() < 2 || input.at(0).toInt() == 0 ) {
		qDebug() << "PublicTransportEngine::updateSourceEvent" << QString("Wrong input: '%1'").arg(name);
		return false; // wrong input
	    }

	    ServiceProvider serviceProvider = static_cast<ServiceProvider>( input.at(0).trimmed().toInt() );
	    QString city, stop, dataType;
	    int maxDeps = DEFAULT_MAXIMUM_DEPARTURES;
	    int timeOffset = DEFAULT_TIME_OFFSET;
	    if ( name.startsWith(I18N_NOOP("Departures")) )
		dataType = "departures";
	    else
		dataType = "arrivals";

	    for ( int i = 0; i < input.length(); ++i ) {
		QString s = input.at(i).toLower();
		if ( s.startsWith("city") )
		    city = input.at(i).mid( QString("city").length() ).trimmed();
		else if ( s.startsWith("stop") )
		    stop = input.at(i).mid( QString("stop").length() ).trimmed();
		else if ( s.startsWith("maxdeps") ) {
		    s = input.at(i).mid( QString("maxdeps").length() ).trimmed();
		    maxDeps = s.toInt() == 0 ? 20 : s.toInt();
		} else if ( s.startsWith("timeoffset") ) {
		    s = input.at(i).mid( QString("timeoffset").length() ).trimmed();
		    timeOffset = s.toInt();
		}
	    }
	    maxDeps += ADDITIONAL_MAXIMUM_DEPARTURES;

	    // Try to get the specific accessor from m_accessors (if it's not in there it is created)
	    TimetableAccessor *accessor = m_accessors.value( serviceProvider, TimetableAccessor::getSpecificAccessor( serviceProvider ) );
	    if ( accessor == NULL ) {
		qDebug() << "PublicTransportEngine::updateSourceEvent" << QString("Accessor %1 couldn't be created").arg(serviceProvider);
		return false; // accessor couldn't be created
	    } else if ( accessor->useSeperateCityValue() && city.isEmpty() ) {
		qDebug() << "PublicTransportEngine::updateSourceEvent" << QString("Accessor %1 needs a seperate city value").arg(serviceProvider);
		return false; // accessor needs a seperate city value
	    }

	    connect( accessor, SIGNAL(journeyListReceived(TimetableAccessor*,QList<DepartureInfo>,ServiceProvider,const QString&,const QString&,const QString&,const QString&)), this, SLOT(journeyListReceived(TimetableAccessor*,QList<DepartureInfo>,ServiceProvider,const QString&,const QString&,const QString&,const QString&)) );
	    connect( accessor, SIGNAL(stopListReceived(TimetableAccessor*,QMap<QString,QString>,ServiceProvider,const QString&,const QString&,const QString&,const QString&)), this, SLOT(stopListReceived(TimetableAccessor*,QMap<QString,QString>,ServiceProvider,const QString&,const QString&,const QString&,const QString&)) );
	    connect( accessor, SIGNAL(errorParsing(TimetableAccessor*,ServiceProvider,const QString&,const QString&,const QString&,const QString&)), this, SLOT(errorParsing(TimetableAccessor*,ServiceProvider,const QString&,const QString&,const QString&,const QString&)) );

	    qDebug() << " ACCESS datatype =" << dataType;
	    accessor->requestJourneys( name, city, stop, maxDeps, timeOffset, dataType );
	}
    }

    return true;
}

void PublicTransportEngine::journeyListReceived( TimetableAccessor *accessor, QList< DepartureInfo > journeys, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
    Q_UNUSED(stop);
    Q_UNUSED(dataType);
    qDebug() << "PublicTransportEngine::journeyListReceived" << journeys.count() << "journeys received";

//     QString sourceName;
//     if ( accessor->useSeperateCityValue() )
// 	sourceName = QString("Departures %1:%2:%3").arg( static_cast<int>(serviceProvider) ).arg( city ).arg( stop );
//     else
// 	sourceName = QString("Departures %1:%2").arg( static_cast<int>(serviceProvider) ).arg( stop );
    int i = 0;
    m_dataSources.remove( sourceName );
    QMap<QString, QVariant> dataSource;
    foreach ( const DepartureInfo &departureInfo, journeys )
    {
	if ( !departureInfo.isValid() )
	    continue;

	QMap<QString, QVariant> data;
	data.insert("line", departureInfo.line());
	data.insert("target", departureInfo.target());
	data.insert("departure", departureInfo.departure());
	data.insert("lineType", static_cast<int>(departureInfo.vehicleType()));
	data.insert("nightline", departureInfo.isNightLine());
	data.insert("expressline", departureInfo.isExpressLine());
	data.insert("platform", departureInfo.platform());
	data.insert("delay", departureInfo.delay());
	data.insert("delayReason", departureInfo.delayReason());
	data.insert("journeyNews", departureInfo.journeyNews());

	QString sKey = QString("%1").arg(i++);
	setData( sourceName, sKey, data );
	dataSource.insert( sKey, data );
// 	qDebug() << "PublicTransportEngine::journeyListReceived" << "setData" << sourceName << data;
    }

    int m_lastJourneyCount = 15; // TODO: member variable
    for ( ; i < m_lastJourneyCount; ++i )
	removeData( sourceName, QString("%1").arg(i++) );
    m_lastJourneyCount = journeys.count();

    // Remove possible stop list
    int m_lastStopNameCount = 30; // TODO: member variable
    for ( i = 0 ; i < m_lastStopNameCount; ++i )
	removeData( sourceName, QString("stopName %1").arg(i++) );
    m_lastStopNameCount = 0;

    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "receivedPossibleStopList", false );
    dataSource.insert( "error", false );
    dataSource.insert( "updated", QDateTime::currentDateTime() );
    m_dataSources.insert( sourceName, dataSource );
}

void PublicTransportEngine::stopListReceived( TimetableAccessor *accessor, QMap<QString, QString> stops, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
//     Q_UNUSED(stop);
    Q_UNUSED(dataType);
    qDebug() << "PublicTransportEngine::stopListReceived" << stops.count() << "stops received";
    QString /*sourceName, */sStop = stops.value( stop, stop );
    if ( sStop.isEmpty() )
	sStop = stop;
//     if ( accessor->useSeperateCityValue() )
// 	sourceName = QString("Departures %1:city%2:stop%3").arg( static_cast<int>(serviceProvider) ).arg( city ).arg( sStop );
//     else
// 	sourceName = QString("Departures %1:stop%2").arg( static_cast<int>(serviceProvider) ).arg( sStop );

    int i = 0;
    foreach ( const QString &stopName, stops.keys() )
    {
	QMap<QString, QVariant> data;
	data.insert("stopName", stopName);
	data.insert("stopID", stops.value(stopName, "") );

	setData( sourceName, QString("stopName %1").arg(i++), data );
// 	qDebug() << "PublicTransportEngine::journeyListReceived" << "setData" << sourceName << data;
    }

    // Remove values from an old possible stop list
    int m_lastStopNameCount = 30; // TODO: member variable
    for ( ; i < m_lastStopNameCount; ++i )
	removeData( sourceName, QString("stopName %1").arg(i++) );
    m_lastStopNameCount = stops.count();

    setData( sourceName, "receivedPossibleStopList", true );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );
}

void PublicTransportEngine::errorParsing( TimetableAccessor *accessor, ServiceProvider serviceProvider, const QString &sourceName, const QString &city, const QString &stop, const QString &dataType ) {
    Q_UNUSED(accessor);
    Q_UNUSED(serviceProvider);
    Q_UNUSED(city);
    Q_UNUSED(stop);
    Q_UNUSED(dataType);
    qDebug() << "PublicTransportEngine::errorParsing";
//     QString sourceName;
//     if ( accessor->useSeperateCityValue() )
// 	sourceName = QString("Departures %1:%2:%3").arg( static_cast<int>(serviceProvider) ).arg( city ).arg( stop );
//     else
// 	sourceName = QString("Departures %1:%2").arg( static_cast<int>(serviceProvider) ).arg( stop );
    setData( sourceName, "error", true );
    setData( sourceName, "updated", QDateTime::currentDateTime() );
}

bool PublicTransportEngine::isSourceUpToDate ( const QString& name ) {
    if ( !m_dataSources.keys().contains(name) )
	return false;
    // Data source stays up to date for UPDATE_TIMEOUT seconds
    QMap<QString, QVariant> dataSource = m_dataSources[name].toMap();
    return dataSource["updated"].toDateTime().secsTo( QDateTime::currentDateTime() ) < UPDATE_TIMEOUT;
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE(publictransport, PublicTransportEngine)

// this is needed since PublicTransportEngine is a QObject
#include "publictransportdataengine.moc"
