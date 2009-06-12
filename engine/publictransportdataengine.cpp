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

#include "departureinfo.h"

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
    setData( name, 0 );
    return updateSourceEvent(name);
}

bool PublicTransportEngine::updateSourceEvent(const QString &name)
{
    if ( name == I18N_NOOP("ServiceProviders") )
    {
	QMap< QString, QVariant > germany;
	germany.insert( "Niedersachsen, Bremen (fahrplaner.de)", Fahrplaner );
	germany.insert( "Rhein-Main (rmv.de)", RMV );
	germany.insert( "Stuttgart & surroundings (vvs.de)", VVS );
	germany.insert( "Rhein-Neckar (vrn.de)", VRN );
	germany.insert( "Berlin (bvg.de)", BVG );
	setData( name, I18N_NOOP("germany"), germany );
	    
	QMap< QString, QVariant > slovakia;
	slovakia.insert( "Bratislava (imhd.sk)", IMHD );
	setData( name, I18N_NOOP("slovakia"), slovakia );
    }
    
    qDebug() << "publicTransport DataEngine: updateSourceEvent" << name;
    
    // Departures 0:Bremen:Pappelstraße
    if ( name.startsWith(I18N_NOOP("Departures")) )
    {
	QStringList input = name.right( name.length() - 10).split(":", QString::SkipEmptyParts);
	if (input.length() < 3)
	{
	    qDebug() << QString("publicTransport DataEngine: Wrong input: '%1'").arg(name);
	    return false; // wrong input
	}

	ServiceProvider serviceProvider = static_cast<ServiceProvider>( input.at(0).trimmed().toInt() );
	QString city = input.at(1).trimmed();
	QString stop = input.at(2).trimmed();

	// Try to get the specific accessor from m_accessors (if it's not in there it is created)
	TimetableAccessor *accessor = m_accessors.value( serviceProvider, TimetableAccessor::getSpecificAccessor(serviceProvider) );
	if ( accessor == NULL )
	{
	    qDebug() << QString("publicTransport DataEngine: Accessor couldn't be created for %1").arg(serviceProvider);
	    return false; // accessor couldn't be created
	}

	connect( accessor, SIGNAL(journeyListReceived(QList<DepartureInfo>,ServiceProvider,QString,QString)), this, SLOT(journeyListReceived(QList<DepartureInfo>,ServiceProvider,QString,QString)) );
	KIO::TransferJob *job = accessor->requestJourneys( city, stop );
// 	QList< DepartureInfo > journeys = accessor->getJourneys( city, stop );
// 	journeyListReceived( journeys, serviceProvider, city, stop );
    }

    return true;
}

void PublicTransportEngine::journeyListReceived ( QList< DepartureInfo > journeys, ServiceProvider serviceProvider, QString city, QString stop )
{
    qDebug() << "publicTransport DataEngine:" << journeys.count() << "journeys received";
    int i = 0;
    foreach (const DepartureInfo &departureInfo, journeys)
    {
	QString sourceName = QString("Departures %1:%2:%3").arg( static_cast<int>(serviceProvider) ).arg( city ).arg( stop );
	QMap<QString, QVariant> data;
	data.insert("line", departureInfo.lineString());
	data.insert("lineNumber", departureInfo.line());
	data.insert("target", departureInfo.target());
	data.insert("duration", departureInfo.getDurationString());
	data.insert("departure", departureInfo.departure());
// 	QString data = departureInfo.lineString() + ", " + departureInfo.target() + ", " + departureInfo.getDurationString();
// 	qDebug() << data;
	setData( sourceName, QString("%1").arg(i++), data );
	qDebug() << "setData" << sourceName << data;
    }
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE(publictransport, PublicTransportEngine)

// this is needed since PublicTransportEngine is a QObject
#include "publictransportdataengine.moc"
