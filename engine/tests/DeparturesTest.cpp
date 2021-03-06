/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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

#include "DeparturesTest.h"

#include <Plasma/DataEngineManager>

#include <QtTest/QTest>
#include <QTimer>

#define TIMEOUT 10

void DeparturesTest::initTestCase()
{
    Plasma::DataEngineManager *manager = Plasma::DataEngineManager::self();
    m_publicTransportEngine = manager->loadEngine( "publictransport" );
}

void DeparturesTest::init()
{}

void DeparturesTest::cleanup()
{}

void DeparturesTest::cleanupTestCase()
{
    Plasma::DataEngineManager *manager = Plasma::DataEngineManager::self();
    manager->unloadEngine( "publictransport" );
}

// Helper function to test basic departure data
void testDepartureData( const TestVisualization &testVisualization, const QString &serviceProvider )
{
    // Test main keys
    QVERIFY( !testVisualization.data["error"].toBool() );
    QVERIFY( testVisualization.data.contains("departures") );
    QVERIFY( testVisualization.data["updated"].canConvert(QVariant::DateTime) );
    QCOMPARE( testVisualization.data["serviceProvider"].toString(), serviceProvider );
    QVERIFY( !testVisualization.data["requestUrl"].toString().isEmpty() );
    QVERIFY( QUrl(testVisualization.data["requestUrl"].toString()).isValid() );

    QVariantList departuresData = testVisualization.data["departures"].toList();
    QVERIFY( departuresData.count() > 0 );

    foreach ( const QVariant &departureData, departuresData ) {
        QHash<QString, QVariant> departure = departureData.toHash();

        // Each stop object should contain some elements
        QVERIFY( !departure.isEmpty() );

        // Ensure that these keys are in the hash
        QVERIFY( departure.contains("DepartureDateTime") );
        QVERIFY( departure.contains("Target") );
        QVERIFY( departure.contains("TransportLine") );
        QVERIFY( departure.contains("TypeOfVehicle") );
    // 		QVERIFY( departureData.contains("journeyNews") ); // NOTE These values are only given if they are not empty
    // 		QVERIFY( departureData.contains("platform") );
    // 		QVERIFY( departureData.contains("operator") );
        QVERIFY( departure.contains("Delay") );
    // 		QVERIFY( departureData.contains("delayReason") );
    // 		QVERIFY( departureData.contains("routeStops") );
    // 		QVERIFY( departureData.contains("routeTimes") );
//         QVERIFY( departure.contains("RouteExactStops") );

        // Test data types
        QVERIFY( departure["DepartureDateTime"].canConvert(QVariant::DateTime) );
        QVERIFY( departure["Target"].canConvert(QVariant::String) );
        QVERIFY( departure["TransportLine"].canConvert(QVariant::String) );
        QVERIFY( departure["TypeOfVehicle"].canConvert(QVariant::Int) );
        QVERIFY( departure["Delay"].canConvert(QVariant::Int) );
//         QVERIFY( departure["RouteExactStops"].canConvert(QVariant::Int) );
        if ( departure.contains("JourneyNews") ) {
            QVERIFY( departure["JourneyNews"].canConvert(QVariant::String) );
        }
        if ( departure.contains("Platform") ) {
            QVERIFY( departure["Platform"].canConvert(QVariant::String) );
        }
        if ( departure.contains("Operator") ) {
            QVERIFY( departure["Operator"].canConvert(QVariant::String) );
        }
        if ( departure.contains("DelayReason") ) {
            QVERIFY( departure["DelayReason"].canConvert(QVariant::String) );
        }
        if ( departure.contains("RouteStops") ) {
            QVERIFY( departure["RouteStops"].canConvert(QVariant::StringList) );
        }
        if ( departure.contains("RouteTimes") ) {
            QVERIFY( departure["RouteTimes"].canConvert(QVariant::List) );
        }

        // Check routeTimes list for data types
        QVariantList times = departure[ "RouteTimes" ].toList();
        foreach( const QVariant &time, times ) {
            QVERIFY( time.canConvert(QVariant::Time) );
        }

        // The first stop of the route stop list should be the departure stop
    //         QVERIFY( departureData["routeStops"].toStringList().first().indexOf(
    //                  stopName.isNull() ? QLatin1String("Bremen") : stopName, 0, Qt::CaseInsensitive) != -1 );
    }
}

// Helper function to test departure times.
// Allows a maximum time difference in minutes (maxDifference) before the given testTime.
void testDepartureTimes( const TestVisualization &testVisualization, const QDateTime &testDateTime,
                         int maxDifference = 120 ) {
    // Test time values
    QVariantList departuresData = testVisualization.data.contains("departures")
            ? testVisualization.data["departures"].toList()
            : testVisualization.data["arrivals"].toList();

    foreach ( const QVariant &departure, departuresData ) {
        QHash<QString, QVariant> departureData = departure.toHash();

        // Check routeTimes list for data types
        QDateTime dateTimeValue = departureData[ "DepartureDateTime" ].toDateTime();

        // Get seconds from actual departure time until the testTime
        int difference = dateTimeValue.secsTo( testDateTime );
        QVERIFY2( difference <= maxDifference * 60, QString("A departure was returned which is %1 "
                  "before the given time (departure at %2, requested time was %3. "
                  "Maybe the maxDifference value should be increased in the test?")
                  .arg(KGlobal::locale()->formatDuration(difference * 1000))
                  .arg(dateTimeValue.toString()).arg(testDateTime.toString()).toLatin1().data() );
    }
}

void DeparturesTest::departuresTest_data()
{
    QTest::addColumn<QString>("serviceProvider");
    QTest::addColumn<QString>("city");
    QTest::addColumn<QString>("stopName");

    // 21 service provider plugins
//     QTest::newRow("at_oebb") << "at_oebb" << QString() << "Wien";
    QTest::newRow("be_brail") << "be_brail" << QString() << "Basel Bahnhof";
    QTest::newRow("ch_sbb") << "ch_sbb" << QString() << "Bern";
    QTest::newRow("cz_idnes") << "cz_idnes" << "Brno" << QString::fromUtf8("Technologický park");
    QTest::newRow("de_bvg") << "de_bvg" << QString() << "Alexanderplatz (Berlin)";
    QTest::newRow("de_db") << "de_db" << QString() << "Bremen Hbf";
    QTest::newRow("de_dvb") << "de_dvb" << QString() << "Hauptbahnhof";
    QTest::newRow("de_fahrplaner") << "de_fahrplaner" << QString() << "Bremen Hbf";
    QTest::newRow("de_nasa") << "de_nasa" << QString() << "Kirkel Bahnhof";
    QTest::newRow("de_rmv") << "de_rmv" << QString() << "3000511"; // ID for "Frankfurt (Main) Zoo"
//     QTest::newRow("de_vvs") << "de_vvs" << "Stuttgart" << "Herrenberg";
    QTest::newRow("dk_rejseplanen") << "dk_rejseplanen" << QString() << "Oslovej / Ringvejen";
//     QTest::newRow("fr_gares") << "fr_gares" << QString() << "frlpd"; // Only takes stop IDs
    QTest::newRow("it_cup2000") << "it_cup2000" << QString() << "Roma - Bologna";
    QTest::newRow("it_orario") << "it_orario" << QString() << "Genova";
    QTest::newRow("pl_pkp") << "pl_pkp" << QString() << "Warszawa Centralna";
//     QTest::newRow("sk_atlas") << "sk_atlas" << "bratislava" << QString::fromUtf8("Bradáčova");
//     QTest::newRow("sk_imhd") << "sk_imhd" << "bratislava" << "Nobelova";
    QTest::newRow("us_septa") << "us_septa" << QString() << "Pennsylvania Park Av";
    QTest::newRow("international_flightstats") << "international_flightstats" << QString() << "BRE";
}

void DeparturesTest::departuresTest()
{
    QFETCH(QString, serviceProvider);
    QFETCH(QString, stopName);
    QFETCH(QString, city);

    // Connect source and wait until the dataUpdated slot gets called in testVisualization
    QString sourceName = QString("Departures %1|stop=%2").arg(serviceProvider).arg(stopName);
    if ( !city.isNull() ) {
        sourceName.append( QString("|city=%1").arg(city) );
    }

    TestVisualization testVisualization;
    QEventLoop loop;
    loop.connect( &testVisualization, SIGNAL(completed()), SLOT(quit()) );
    QTime timer;
    timer.start();
    m_publicTransportEngine->connectSource( sourceName, &testVisualization );
    QTimer::singleShot( TIMEOUT * 1000, &loop, SLOT(quit()) ); // timeout after TIMEOUT seconds
    loop.exec();
    m_publicTransportEngine->disconnectSource( sourceName, &testVisualization );

    qDebug() << QString("Got data from %1 and parsed it in %2 seconds")
            .arg( serviceProvider ).arg( timer.elapsed() / 1000.0 );

    // Test basic departure data
    testDepartureData( testVisualization, serviceProvider );

    QVERIFY2( !testVisualization.data.isEmpty(),
              QString("No data for source name '%1' in %2 seconds").arg(sourceName).arg(TIMEOUT).toLatin1().data() );
}

void DeparturesTest::departuresTimeTest_data()
{
    QTest::addColumn<QString>("serviceProvider");
    QTest::addColumn<QString>("stopName");
    QTest::addColumn<QTime>("time");

    QTest::newRow("de_db") << "de_db" << "Bremen Hbf" << QTime(13, 30);
    QTest::newRow("de_fahrplaner") << "de_fahrplaner" << "Bremen Hbf" << QTime(11, 15);
    QTest::newRow("ch_sbb") << "ch_sbb" << "Bern" << QTime(12, 45);
    QTest::newRow("it_orario") << "it_orario" << "Genova" << QTime(15, 55);
}

void DeparturesTest::departuresTimeTest()
{
    QFETCH(QString, serviceProvider);
    QFETCH(QString, stopName);
    QFETCH(QTime, time);

    // Connect source and wait until the dataUpdated slot gets called in testVisualization
    QString sourceName = QString("Departures %1|stop=%2|time=%3")
            .arg(serviceProvider).arg(stopName).arg(time.toString("hh:mm"));
    QEventLoop loop;
    TestVisualization testVisualization;
    loop.connect( &testVisualization, SIGNAL(completed()), SLOT(quit()) );
    m_publicTransportEngine->connectSource( sourceName, &testVisualization );
    QTimer::singleShot( TIMEOUT * 1000, &loop, SLOT(quit()) ); // timeout after TIMEOUT seconds
    loop.exec();

    // Test basic departure data
    testDepartureData( testVisualization, serviceProvider );

    // Test time values
    testDepartureTimes( testVisualization, QDateTime(QDate::currentDate(), time) );

    m_publicTransportEngine->disconnectSource( sourceName, &testVisualization );
    QVERIFY2( !testVisualization.data.isEmpty(),
              QString("No data for source name '%1' in %2 seconds").arg(sourceName).arg(TIMEOUT).toLatin1().data() );
}

void DeparturesTest::departuresTimeOffsetTest_data()
{
    QTest::addColumn<QString>("serviceProvider");
    QTest::addColumn<QString>("stopName");
    QTest::addColumn<int>("timeOffset");

    QTest::newRow("de_db") << "de_db" << "Bremen Hbf" << 240;
    QTest::newRow("ch_sbb") << "ch_sbb" << "Bern" << 500;
    QTest::newRow("it_orario") << "it_orario" << "Genova" << 366;
}

void DeparturesTest::departuresTimeOffsetTest()
{
    QFETCH(QString, serviceProvider);
    QFETCH(QString, stopName);
    QFETCH(int, timeOffset);

    // Connect source and wait until the dataUpdated slot gets called in testVisualization
    QString sourceName = QString("Departures %1|stop=%2|timeoffset=%3")
            .arg(serviceProvider).arg(stopName).arg(timeOffset);
    QEventLoop loop;
    TestVisualization testVisualization;
    loop.connect( &testVisualization, SIGNAL(completed()), SLOT(quit()) );
    m_publicTransportEngine->connectSource( sourceName, &testVisualization );
    QTimer::singleShot( TIMEOUT * 1000, &loop, SLOT(quit()) ); // timeout after TIMEOUT seconds
    loop.exec();

    // Test basic departure data
    testDepartureData( testVisualization, serviceProvider );

    // Test time values
    testDepartureTimes( testVisualization, QDateTime(QDate::currentDate(),
                                                    QTime::currentTime().addSecs(timeOffset * 60)) );

    m_publicTransportEngine->disconnectSource( sourceName, &testVisualization );
    QVERIFY2( !testVisualization.data.isEmpty(),
              QString("No data for source name '%1' in %2 seconds").arg(sourceName).arg(TIMEOUT).toLatin1().data() );
}

void DeparturesTest::departuresDateTimeTest_data()
{
    QTest::addColumn<QString>("serviceProvider");
    QTest::addColumn<QString>("stopName");
    QTest::addColumn<QDateTime>("dateTime");

    QTest::newRow("de_db") << "de_db" << "Bremen Hbf"
            << QDateTime::currentDateTime().addDays(2).addSecs(120);
    QTest::newRow("ch_sbb") << "ch_sbb" << "Bern"
            << QDateTime::currentDateTime().addDays(3).addSecs(600);
    QTest::newRow("it_orario") << "it_orario" << "Genova"
            << QDateTime::currentDateTime().addDays(2).addSecs(3600);
}

void DeparturesTest::departuresDateTimeTest()
{
    QFETCH(QString, serviceProvider);
    QFETCH(QString, stopName);
    QFETCH(QDateTime, dateTime);

    // Connect source and wait until the dataUpdated slot gets called in testVisualization
    QString sourceName = QString("Departures %1|stop=%2|datetime=%3")
            .arg(serviceProvider).arg(stopName).arg(dateTime.toString());
    QEventLoop loop;
    TestVisualization testVisualization;
    loop.connect( &testVisualization, SIGNAL(completed()), SLOT(quit()) );
    m_publicTransportEngine->connectSource( sourceName, &testVisualization );
    QTimer::singleShot( TIMEOUT * 1000, &loop, SLOT(quit()) ); // timeout after TIMEOUT seconds
    loop.exec();

    // Test basic departure data
    testDepartureData( testVisualization, serviceProvider );

    // Test time values
    testDepartureTimes( testVisualization, dateTime );

    m_publicTransportEngine->disconnectSource( sourceName, &testVisualization );
    QVERIFY2( !testVisualization.data.isEmpty(),
              QString("No data for source name '%1' in %2 seconds").arg(sourceName).arg(TIMEOUT).toLatin1().data() );
}

QTEST_MAIN(DeparturesTest)
#include "DeparturesTest.moc"
