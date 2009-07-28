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

#ifndef DATASOURCETESTER_HEADER
#define DATASOURCETESTER_HEADER

#include <Plasma/DataEngine>
#include <Plasma/Applet>

class DataSourceTester : public QObject {
    Q_OBJECT

    public:
	enum TestResult {
	    Error,
	    JourneyListReceived,
	    PossibleStopsReceived
	};

	DataSourceTester( const QString &testSource, Plasma::Applet *applet, QObject* parent = 0 ) : QObject(parent) {
	    Q_ASSERT( applet != NULL);
	    m_testSource = testSource;
	    m_applet = applet;
	};
	~DataSourceTester() {
	    disconnectTestSource();
	}

	void processTestSourcePossibleStopList( const Plasma::DataEngine::Data& data );

	// Source name to be tested
	QString testSource() { return m_testSource; };
	// Sets the source name to be tested and connects it to the data engine
	void setTestSource( const QString &sourceName );

	Plasma::Applet *applet() { return m_applet; };

	QString stopToStopID( const QString &stopName );
	void clearStopToStopIdMap();

    signals:
	void testResult( DataSourceTester::TestResult testResult, const QVariant &data );

    public slots:
	void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

    private:
	// Disconnects the test data source
	void disconnectTestSource();
	// Connects the test data source
	void connectTestSource();

	QString m_testSource; // Source name for testing configurations
	Plasma::Applet *m_applet;
	QHash< QString, QVariant > m_mapStopToStopID; // A map with stop names as keys and the corresponding stop IDs as values
};

#endif // DATASOURCETESTER_HEADER
