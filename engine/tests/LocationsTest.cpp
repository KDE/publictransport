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

#include "LocationsTest.h"

#include <Plasma/DataEngineManager>

#include <QtTest/QTest>
#include <QTimer>

void LocationsTest::initTestCase()
{
	Plasma::DataEngineManager *manager = Plasma::DataEngineManager::self();
	m_publicTransportEngine = manager->loadEngine( "publictransport" );
}

void LocationsTest::init()
{}

void LocationsTest::cleanup()
{}

void LocationsTest::cleanupTestCase()
{
	Plasma::DataEngineManager *manager = Plasma::DataEngineManager::self();
	manager->unloadEngine( "publictransport" );
}

void LocationsTest::locationTest()
{
	// Connect source and wait until the dataUpdated slot gets called in testVisualization
	QString sourceName = "Locations";
	QEventLoop loop;
	TestVisualization testVisualization;
	loop.connect( &testVisualization, SIGNAL(completed()), SLOT(quit()) );
	m_publicTransportEngine->connectSource( sourceName, &testVisualization );
	QTimer::singleShot( 5000, &loop, SLOT(quit()) ); // timeout after 5 seconds
	loop.exec();
	
	foreach ( const QString &country, testVisualization.data.keys() )
	{
		QVariantHash locationData = testVisualization.data[country].toHash();
		
		// Each location object should contain some elements
		QVERIFY( !locationData.isEmpty() );
		
		// Ensure that these keys are in the hash
		QVERIFY( locationData.contains("name") );
		QVERIFY( locationData.contains("description") );
		QVERIFY( locationData.contains("defaultAccessor") );
		
		// Test data types
		QVERIFY( locationData["name"].canConvert(QVariant::String) );
		QVERIFY( locationData["description"].canConvert(QVariant::String) );
		QVERIFY( locationData["defaultAccessor"].canConvert(QVariant::String) );
		
		// Ensure that the used country code is known
		QString name = locationData["name"].toString();
		QVERIFY2( KGlobal::locale()->allCountriesList().contains(name)
			|| name == QLatin1String("international") || name == QLatin1String("unknown")
			|| name == QLatin1String("errornous"),
			QString("Invalid country code \"%1\"").arg(name).toLatin1().data() );
		
		// Ensure that the default accessor starts with the country code (given in "name")
		QVERIFY2( locationData["defaultAccessor"].toString().startsWith(locationData["name"].toString()),
			QString("Wrong defaultAccessor \"%1\" for \"%2\", should start with \"%2_\"")
			.arg(locationData["defaultAccessor"].toString())
			.arg(locationData["name"].toString()).toLatin1().data() );
	}
	m_publicTransportEngine->disconnectSource( sourceName, this );
	QVERIFY2( !testVisualization.data.isEmpty(), "No data for source name 'Locations' in 5 seconds" );
// 	QCOMPARE( stop.id, QString() );
}

QTEST_MAIN(LocationsTest)
#include "LocationsTest.moc"
