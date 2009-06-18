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
    // In the case of a clock that only has second precision,
    // a third of a second should be more than enough.
    setMinimumPollingInterval(333);
}

bool PublicTransportEngine::sourceRequestEvent(const QString &name)
{
    // Create the requested source so that the connection can be established
//     setData( name, "updated", QTime::currentTime() );
    qDebug() << "PublicTransportEngine::sourceRequestEvent" << name;
    setData( name, DataEngine::Data() ); // Create source
    qDebug() << "PublicTransportEngine::sourceRequestEvent" << "Source created";
    return updateSourceEvent(name);
}

bool PublicTransportEngine::updateSourceEvent(const QString &name)
{
    qDebug() << "PublicTransportEngine::updateSourceEvent" << name;
    
    if ( name == I18N_NOOP("ServiceProviders") )
    {
	QMap<QString, QVariant> data;
	TimetableAccessor *accessor;
	
	QMap< QString, QVariant > germany;
	accessor = TimetableAccessor::getSpecificAccessor(Fahrplaner);
	data.insert( "id", Fahrplaner );
	data.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
	data.insert( "cities", accessor->cities() );
	germany.insert( "Niedersachsen, Bremen (fahrplaner.de)", data );

	data.clear();
	accessor = TimetableAccessor::getSpecificAccessor(RMV);
	data.insert( "id", RMV );
	data.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
	data.insert( "cities", accessor->cities() );
	germany.insert( "Rhein-Main (rmv.de)",  data );
	
	data.clear();
	accessor = TimetableAccessor::getSpecificAccessor(VVS);
	data.insert( "id", VVS );
	data.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
	data.insert( "cities", accessor->cities() );
	germany.insert( "Stuttgart & surroundings (vvs.de)",  data );

	data.clear();
	accessor = TimetableAccessor::getSpecificAccessor(VRN);
	data.insert( "id", VRN );
	data.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
	data.insert( "cities", accessor->cities() );
	germany.insert( "Rhein-Neckar (vrn.de)",  data );

	data.clear();
	accessor = TimetableAccessor::getSpecificAccessor(BVG);
	data.insert( "id", BVG );
	data.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
	data.insert( "cities", accessor->cities() );
	germany.insert( "Berlin (bvg.de)",  data );

	data.clear();
	accessor = TimetableAccessor::getSpecificAccessor(DVB);
	data.insert( "id", DVB );
	data.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
	data.insert( "cities", accessor->cities() );
	germany.insert( "Dresden (dvb.de)",  data );

	data.clear();
	accessor = TimetableAccessor::getSpecificAccessor(NASA);
	data.insert( "id", NASA );
	data.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
	data.insert( "cities", accessor->cities() );
	germany.insert( "Sachsen-Anhalt (nasa.de)",  data );

	data.clear();
	accessor = TimetableAccessor::getSpecificAccessor(DB);
	data.insert( "id", DB );
	data.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
	data.insert( "cities", accessor->cities() );
	germany.insert( "Deutschlandweit (db.de)",  data );
	setData( name, I18N_NOOP("germany"), germany );

	QMap< QString, QVariant > swiss;
	data.clear();
	accessor = TimetableAccessor::getSpecificAccessor(SBB);
	data.insert( "id", SBB );
	data.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
	data.insert( "cities", accessor->cities() );
	swiss.insert( "Schweiz (sbb.ch)",  data );
	setData( name, I18N_NOOP("swiss"), swiss );

	QMap< QString, QVariant > slovakia;
	data.clear();
	accessor = TimetableAccessor::getSpecificAccessor(IMHD);
	data.insert( "id", IMHD );
	data.insert( "useSeperateCityValue", accessor->useSeperateCityValue() );
	data.insert( "cities", accessor->cities() );
	slovakia.insert( "Bratislava (imhd.sk)",  data );
	setData( name, I18N_NOOP("slovakia"), slovakia );
    }

    // Departures 0:Bremen:Pappelstraße
    if ( name.startsWith(I18N_NOOP("Departures")) )
    {
	QStringList input = name.right( name.length() - 10).split(":", QString::SkipEmptyParts);
	if ( input.length() < 2 )
	{
	    qDebug() << "PublicTransportEngine::updateSourceEvent" << QString("Wrong input: '%1'").arg(name);
	    return false; // wrong input
	}

	ServiceProvider serviceProvider = static_cast<ServiceProvider>( input.at(0).trimmed().toInt() );
	QString city, stop;
	if ( input.length() == 3 )
	{
	    city = input.at(1).trimmed();
	    stop = input.at(2).trimmed();

	    // Corrections for some cities that need different syntax
	    if ( city == "Leipzig" )
		city = "Leipzig,";
	}
	else
	    stop = input.at(1).trimmed();

	// Try to get the specific accessor from m_accessors (if it's not in there it is created)
	TimetableAccessor *accessor = m_accessors.value( serviceProvider, TimetableAccessor::getSpecificAccessor( serviceProvider ) );
	if ( accessor == NULL )
	{
	    qDebug() << "PublicTransportEngine::updateSourceEvent" << QString("Accessor %1 couldn't be created").arg(serviceProvider);
	    return false; // accessor couldn't be created
	}
	if ( accessor->useSeperateCityValue() && city.isEmpty() )
	{
	    qDebug() << "PublicTransportEngine::updateSourceEvent" << QString("Accessor %1 needs a seperate city value").arg(serviceProvider);
	    return false; // accessor needs a seperate city value
	}

	connect( accessor, SIGNAL(journeyListReceived(TimetableAccessor*,QList<DepartureInfo>,ServiceProvider,QString,QString)), this, SLOT(journeyListReceived(TimetableAccessor*,QList<DepartureInfo>,ServiceProvider,QString,QString)) );
	connect( accessor, SIGNAL(stopListReceived(TimetableAccessor*,QMap<QString,QString>,ServiceProvider,QString,QString)), this, SLOT(stopListReceived(TimetableAccessor*,QMap<QString,QString>,ServiceProvider,QString,QString)) );
	connect( accessor, SIGNAL(errorParsing(TimetableAccessor*,ServiceProvider,QString,QString)), this, SLOT(errorParsing(TimetableAccessor*,ServiceProvider,QString,QString)) );
	
	KIO::TransferJob *job = accessor->requestJourneys( city, stop );
    }

    return true;
}

void PublicTransportEngine::journeyListReceived ( TimetableAccessor *accessor, QList< DepartureInfo > journeys, ServiceProvider serviceProvider, QString city, QString stop )
{
    qDebug() << "PublicTransportEngine::journeyListReceived" << journeys.count() << "journeys received";

    QString sourceName;
    if ( accessor->useSeperateCityValue() )
	sourceName = QString("Departures %1:%2:%3").arg( static_cast<int>(serviceProvider) ).arg( city ).arg( stop );
    else
	sourceName = QString("Departures %1:%2").arg( static_cast<int>(serviceProvider) ).arg( stop );
    int i = 0;
    foreach ( const DepartureInfo &departureInfo, journeys )
    {
	if ( !departureInfo.isValid() )
	    continue;

	QMap<QString, QVariant> data;
	data.insert("line", departureInfo.line());
	data.insert("direction", departureInfo.direction());
	data.insert("departure", departureInfo.departure());
	data.insert("lineType", static_cast<int>(departureInfo.lineType()));
	data.insert("nightline", departureInfo.isNightLine());
	data.insert("expressline", departureInfo.isExpressLine());

	setData( sourceName, QString("%1").arg(i++), data );
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
    setData( sourceName, "updated", QTime::currentTime() );
    
//     Plasma::Service *service;
//     scheduleSourcesUpdated();
//     serviceForSource("fsfd")->;
}

void PublicTransportEngine::stopListReceived( TimetableAccessor *accessor, QMap<QString, QString> stops, ServiceProvider serviceProvider, QString city, QString stop )
{
    qDebug() << "PublicTransportEngine::stopListReceived" << stops.count() << "stops received";
    QString sourceName;
    if ( accessor->useSeperateCityValue() )
	sourceName = QString("Departures %1:%2:%3").arg( static_cast<int>(serviceProvider) ).arg( city ).arg( stop );
    else
	sourceName = QString("Departures %1:%2").arg( static_cast<int>(serviceProvider) ).arg( stop );

    int i = 0;
    foreach ( const QString &stopName, stops.keys() )
    {
	QMap<QString, QVariant> data;
	data.insert("stopName", stopName);
	data.insert("stopID", data[stopName]);

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
    setData( sourceName, "updated", QTime::currentTime() );
}

void PublicTransportEngine::errorParsing( TimetableAccessor *accessor, ServiceProvider serviceProvider, QString city, QString stop )
{
    qDebug() << "PublicTransportEngine::errorParsing";
    QString sourceName;
    if ( accessor->useSeperateCityValue() )
	sourceName = QString("Departures %1:%2:%3").arg( static_cast<int>(serviceProvider) ).arg( city ).arg( stop );
    else
	sourceName = QString("Departures %1:%2").arg( static_cast<int>(serviceProvider) ).arg( stop );
    setData( sourceName, "error", true );
    setData( sourceName, "updated", QTime::currentTime() );
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE(publictransport, PublicTransportEngine)

// this is needed since PublicTransportEngine is a QObject
#include "publictransportdataengine.moc"
