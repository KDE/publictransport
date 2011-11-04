/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef DeparturesTest_H
#define DeparturesTest_H

#define QT_GUI_LIB

#include <QtCore/QObject>
#include <Plasma/DataEngine>

namespace Plasma {
    class DataEngine;
}

class TestVisualization : public QObject
{
    Q_OBJECT

public slots:
    void dataUpdated( const QString &, const Plasma::DataEngine::Data &_data ) {
        data = _data;
        emit completed();
    };

signals:
    void completed();

public:
    Plasma::DataEngine::Data data;
};

class DeparturesTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();


    // Tests "Departures <serviceProvider>|stop=<stopName>" data source
    void departuresTest_data();
    void departuresTest();

    // Tests "Departures <serviceProvider>|stop=<stopName>|time=<hh:mm>" data source
    void departuresTimeTest_data();
    void departuresTimeTest();

    // Tests "Departures <serviceProvider>|stop=<stopName>|timeOffset=<offset>" data source
    void departuresTimeOffsetTest_data();
    void departuresTimeOffsetTest();

    // Tests "Departures <serviceProvider>|stop=<stopName>|dateTime=<date-and-time>" data source
    void departuresDateTimeTest_data();
    void departuresDateTimeTest();

private:
    Plasma::DataEngine *m_publicTransportEngine;
};

#endif // DeparturesTest_H
