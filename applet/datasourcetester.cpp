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

#include "datasourcetester.h"


void DataSourceTester::connectTestSource() {
    applet()->dataEngine("publictransport")->connectSource( m_testSource, this );
}

void DataSourceTester::disconnectTestSource() {
    if ( !m_testSource.isEmpty() ) {
	applet()->dataEngine("publictransport")->disconnectSource( m_testSource, this );
	m_testSource = "";
    }
}

void DataSourceTester::setTestSource ( const QString& sourceName ) {
    disconnectTestSource();

//     qDebug() << "DataSourceTester::setTestSource" << sourceName;
    m_testSource = sourceName;
    connectTestSource();
}

QString DataSourceTester::stopToStopID ( const QString& stopName )
{
    return m_mapStopToStopID.value( stopName, "" ).toString();
}

void DataSourceTester::clearStopToStopIdMap()
{
    m_mapStopToStopID.clear();
}

void DataSourceTester::dataUpdated ( const QString& sourceName, const Plasma::DataEngine::Data& data )
{
    Q_UNUSED( sourceName );
//     qDebug() << "DataSourceTester::dataUpdated" << sourceName;
    disconnectTestSource();

    // Check for errors from the data engine
    if ( data.value("error").toBool() ) {
	emit testResult( Error, i18n("The stop name is invalid.") );
	// setStopNameValid( false, i18n("The stop name is invalid.") );
	// m_ui.stop->setCompletedItems( QStringList() );
    } else {
	// Check if we got a possible stop list or a journey list
	if ( data.value("receivedPossibleStopList").toBool() ) {
	    processTestSourcePossibleStopList( data );
	    emit testResult( Error, i18n("The stop name is ambigous.") );
// 	    setStopNameValid( false, i18n("The stop name is ambigous.") );
	} else {
	    // List of journeys received
	    disconnectTestSource();
	    emit testResult( JourneyListReceived, QVariant() );
// 	    setStopNameValid( true );
	}
    }
}

void DataSourceTester::processTestSourcePossibleStopList ( const Plasma::DataEngine::Data& data ) {
    qDebug() << "DataSourceTester::processTestSourcePossibleStopList";
    disconnectTestSource();

    QStringList possibleStops;
    for (int i = 0; i < data.keys().count(); ++i)
    {
	QVariant stopData = data.value( QString("stopName %1").arg(i) );
	if ( !stopData.isValid() )
	    break; // Stop on first invalid key. Thats a key with a too high number, maybe the data engine should provide a counter for how many stop names it provides currently

	    QMap<QString, QVariant> dataMap = stopData.toMap();

	QString sStopName = dataMap["stopName"].toString();
	QString sStopID = dataMap["stopID"].toString();
	possibleStops << sStopName;
	m_mapStopToStopID.insert( sStopName, sStopID );
    }

    emit testResult( PossibleStopsReceived, m_mapStopToStopID );
//     m_ui.stop->setCompletedItems( possibleStops );
//     m_stopIDinConfig = m_mapStopToStopID.value( m_ui.stop->text(), "" );
}




