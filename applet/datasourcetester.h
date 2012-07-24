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

#ifndef DATASOURCETESTER_HEADER
#define DATASOURCETESTER_HEADER

/** @file
 * @brief This file contains the DataSourceTester class.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#include <Plasma/DataEngine>
#include <Plasma/Applet>

/** @class DataSourceTester
 * @brief Tests data sources with the "publictransport"-data engine.
 *
 * The source with a given source name can be tested. The signal @a testResult is emitted when
 * the test is complete.
 **/
class DataSourceTester : public QObject {
    Q_OBJECT

public:
    /** Results of a data source test. */
    enum TestResult {
        Error, /**< The used data source name is erroneous or the data couldn't be parsed correctly.
            * TODO: It's also possible, that there just weren't any departures / arrivals.
            * In such a case JourneyListReceived should be used (or a new EmptyJourneyListReceived?). */
        JourneyListReceived, /**< The tested data source name gets a list
            * of departures / arrivals or journeys. */
        PossibleStopsReceived /**< The tested data source name gets a list of stop suggestions.
            * If you requested a journey list this means that the stop name is ambiguous.
            * You can try to use stop IDs, if the ambiguity can't be removed. */
    };

    DataSourceTester( const QString &testSource, Plasma::DataEngine *publicTransportEngine,
                    QObject* parent = 0 )
            : QObject(parent), m_testSource(testSource),
            m_publicTransportEngine(publicTransportEngine) {
    };

    ~DataSourceTester() {
        disconnectTestSource();
    }

    void processTestSourcePossibleStopList( const Plasma::DataEngine::Data& data );

    /** Source name to be tested. */
    QString testSource() { return m_testSource; };

    /** Sets the source name to be tested and connects it to the data engine. */
    void setTestSource( const QString &sourceName );

    QString stopToStopID( const QString &stopName );
    void clearStopToStopIdMap();

signals:
    void testResult( DataSourceTester::TestResult testResult,
            const QVariant &data, const QVariant &data2, const QVariant &data3 );

public slots:
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

private:
    /** Disconnects the test data source. */
    void disconnectTestSource();

    /** Connects the test data source. */
    void connectTestSource();

    QString m_testSource; /**< Source name for testing configurations. */
    QHash< QString, QVariant > m_mapStopToStopID; /**< A hash with stop names as keys
            * and the corresponding stop IDs as values. */
    Plasma::DataEngine *m_publicTransportEngine;
};

#endif // DATASOURCETESTER_HEADER
