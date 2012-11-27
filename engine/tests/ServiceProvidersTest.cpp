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

#include "ServiceProvidersTest.h"

#include <Plasma/DataEngineManager>

#include <QtTest/QTest>
#include <QTimer>
#include <QFile>

void ServiceProvidersTest::initTestCase()
{
    Plasma::DataEngineManager *manager = Plasma::DataEngineManager::self();
    m_publicTransportEngine = manager->loadEngine( "publictransport" );
}

void ServiceProvidersTest::init()
{}

void ServiceProvidersTest::cleanup()
{}

void ServiceProvidersTest::cleanupTestCase()
{
    Plasma::DataEngineManager *manager = Plasma::DataEngineManager::self();
    manager->unloadEngine( "publictransport" );
}

void ServiceProvidersTest::serviceProviderTest()
{
    // Connect source and wait until the dataUpdated slot gets called in testVisualization
    QString sourceName = "ServiceProviders";
    QEventLoop loop;
    TestVisualization testVisualization;
    loop.connect( &testVisualization, SIGNAL(completed()), SLOT(quit()) );
    m_publicTransportEngine->connectSource( sourceName, &testVisualization );
    QTimer::singleShot( 5000, &loop, SLOT(quit()) ); // timeout after 5 seconds
    loop.exec();

    QRegExp regExpServiceProviderId( "([a-z]*)_[a-z*]" );
    QRegExp regExpVersion( "[0-9]+(\\.[0-9]*)*" );
    foreach ( const QString &serviceProvider, testVisualization.data.keys() ) {
        QVariantHash serviceProviderData = testVisualization.data[serviceProvider].toHash();

        // Each service provider object should contain some elements
        QVERIFY2( !serviceProviderData.isEmpty(), qPrintable(serviceProvider) );

        // Ensure that these keys are in the hash
        QVERIFY2( serviceProviderData.contains("id"), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData.contains("type"), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData.contains("country"), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData.contains("name"), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData.contains("description"), qPrintable(serviceProvider) );

        // Test data types
        QVERIFY2( serviceProviderData["id"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["type"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["country"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["name"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["description"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["features"].canConvert(QVariant::StringList), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["featureNames"].canConvert(QVariant::StringList), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["email"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["author"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["fileName"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["onlyUseCitiesInList"].canConvert(QVariant::Bool), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["cities"].canConvert(QVariant::StringList), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["url"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["shortUrl"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["credit"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        QVERIFY2( serviceProviderData["version"].canConvert(QVariant::String), qPrintable(serviceProvider) );

        QVERIFY2( QUrl(serviceProviderData["url"].toString()).isValid(), qPrintable(serviceProvider) );
        QVERIFY2( QUrl(serviceProviderData["shortUrl"].toString()).isValid(), qPrintable(serviceProvider) );

        const QString type = serviceProviderData["type"].toString();
        if ( type == QLatin1String("GTFS") ) {
            QVERIFY2( serviceProviderData["feedUrl"].canConvert(QVariant::String), qPrintable(serviceProvider) );
            QVERIFY2( QUrl(serviceProviderData["feedUrl"].toString()).isValid(), qPrintable(serviceProvider) );
        } else if ( type == QLatin1String("Scripted") ) {
            QVERIFY2( serviceProviderData["scriptFileName"].canConvert(QVariant::String), qPrintable(serviceProvider) );
        }

        // Ensure the id has correct format
        QString id = serviceProviderData["id"].toString();
        int pos = regExpServiceProviderId.indexIn( id );
        QVERIFY2( pos != -1, qPrintable(QString("The service provider ID \"%1\" has a wrong format, "
                "should be \"<country_code>_<short_a-z_name>\", all lowercase").arg(id)) );

        // Ensure that the used country code in the id is known
        QString countryCode = regExpServiceProviderId.cap( 1 );
        QVERIFY2( KGlobal::locale()->allCountriesList().contains(countryCode)
            || countryCode == QLatin1String("international") || countryCode == QLatin1String("unknown")
            || countryCode == QLatin1String("erroneous"),
            qPrintable(QString("Invalid country code \"%1\" for %2")
            .arg(countryCode, serviceProvider)) );

        // Ensure the country key contains the same country code as the id
        QCOMPARE( countryCode, serviceProviderData["country"].toString() );

        // Ensure the version string has correct format
        QString version = serviceProviderData["version"].toString();
        QVERIFY2( regExpVersion.indexIn(version) != -1,
                  qPrintable(QString("Invalid version format \"%1\" for \"%2\"")
                  .arg(version).arg(id)) );

        // Ensure the xml file exists
        QVERIFY2( QFile::exists(serviceProviderData["fileName"].toString()), qPrintable(serviceProvider) );

        // Ensure that there are cities in the list, if only those cities should be used
        if ( serviceProviderData["onlyUseCitiesInList"].toBool() ) {
            QVERIFY2( !serviceProviderData["cities"].toStringList().isEmpty(),
                      qPrintable(QString("The \"cities\" key should contain city names if the "
                      "\"onlyUseCitiesInList\" key is true for provider %1").arg(serviceProvider)) );
        }
    }
    m_publicTransportEngine->disconnectSource( sourceName, this );
    QVERIFY2( !testVisualization.data.isEmpty(), "No data for source name 'ServiceProviders' in 5 seconds" );
//     QCOMPARE( stop.id, QString() );
}

QTEST_MAIN(ServiceProvidersTest)
#include "ServiceProvidersTest.moc"
