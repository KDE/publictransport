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

#include "DebuggerTest.h"

#include "config.h"
#include <debugger/debugger.h>
#include <debugger/debuggerjobs.h>
#include <project.h>
#include <projectmodel.h>
#include <tabs/webtab.h>
#include <engine/serviceproviderdatareader.h>
#include <request.h>
#include <serviceproviderglobal.h>

#include <QtTest/QTest>
#include <QTimer>
#include <QSignalSpy>
#include <QWebView>
#include <KGlobal>

#include <KWebView>

void DebuggerTest::initTestCase()
{
    // Read provider data from XML file
    ServiceProviderDataReader reader;
    m_data = reader.read( "de_db" );
    QVERIFY( m_data );

    // Read script text from file
    const QString fileName = m_data->scriptFileName();
    QVERIFY( QFile::exists(fileName) );

    // Open script file
    QFile file( fileName );
    QVERIFY( file.open(QIODevice::ReadOnly) );

    // Read and close script file
    const QByteArray ba = file.readAll();
    file.close();
    m_program = ba;

    m_debugger = new Debugger::Debugger( this );
}

void DebuggerTest::cleanupTestCase()
{
    delete m_data;
    m_data = 0;
}

void DebuggerTest::init()
{}

void DebuggerTest::cleanup()
{}

void DebuggerTest::loadScriptTest()
{
    QEventLoop loop;
    Debugger::LoadScriptJob *loadScriptJob =
            m_debugger->loadScript( m_program, m_data, Debugger::DefaultDebugFlags );
    connect( loadScriptJob, SIGNAL(done(ThreadWeaver::Job*)), &loop, SLOT(quit()) );
    if ( !loadScriptJob->isFinished() ) {
        loop.exec();
    }
}

void DebuggerTest::getDeparturesTest()
{
    QEventLoop loop;
    connect( m_debugger, SIGNAL(requestTimetableDataResult(QSharedPointer<AbstractRequest>,bool,QString,QList<TimetableData>,QVariant)),
             &loop, SLOT(quit()) );
    DepartureRequest request( "TEST_DEPARTURES", "Berlin", QDateTime::currentDateTime(), 30 );
    QVERIFY( m_debugger->requestTimetableData(&request, QString(), Debugger::DefaultDebugFlags) );
    loop.exec();
}

void DebuggerTest::multipleTestsTest()
{
    QEventLoop loop;

    Debugger::Debugger *debugger2 = new Debugger::Debugger( this );
    Debugger::LoadScriptJob *loadScriptJob =
            debugger2->loadScript( m_program, m_data, Debugger::NeverInterrupt );
    connect( loadScriptJob, SIGNAL(done(ThreadWeaver::Job*)), &loop, SLOT(quit()) );
    connect( m_debugger, SIGNAL(stopped(ScriptRunData)), &loop, SLOT(quit()) );
    connect( debugger2, SIGNAL(stopped(ScriptRunData)), &loop, SLOT(quit()) );

    DepartureRequest request( "TEST_DEPARTURES", "Berlin", QDateTime::currentDateTime(), 100 );
    DepartureRequest request2( "TEST_DEPARTURES2", "München", QDateTime::currentDateTime(), 100 );
    DepartureRequest request3( "TEST_DEPARTURES3", "Dresden", QDateTime::currentDateTime(), 100 );
    QVERIFY( m_debugger->requestTimetableData(&request, QString(), Debugger::NeverInterrupt) );
    QVERIFY( debugger2->requestTimetableData(&request, QString(), Debugger::NeverInterrupt) );
    QVERIFY( m_debugger->requestTimetableData(&request2, QString(), Debugger::NeverInterrupt) );
    QVERIFY( debugger2->requestTimetableData(&request2, QString(), Debugger::NeverInterrupt) );
    QVERIFY( m_debugger->requestTimetableData(&request3, QString(), Debugger::NeverInterrupt) );
    QVERIFY( debugger2->requestTimetableData(&request3, QString(), Debugger::NeverInterrupt) );

    int done = 0;
    do {
        loop.exec();
        ++done;
    } while ( done < 6 );

    m_debugger->finish();
    debugger2->finish();

    // Wait until all jobs are deleted
    QTimer::singleShot( 0, &loop, SLOT(quit()) );
    loop.exec();
}

void DebuggerTest::projectTestsTest()
{
    // Load some more projects
    ProjectModel *model = new ProjectModel( this );
    QList< Project* > projects = QList< Project* >()
            << new Project(model->weaver()) << new Project(model->weaver())
            << new Project(model->weaver()) << new Project(model->weaver())
            << new Project(model->weaver()) << new Project(model->weaver());
    int i = 0;
    QVERIFY( projects[i++]->loadProject(ServiceProviderGlobal::fileNameFromId("de_db")) );
    QVERIFY( projects[i++]->loadProject(ServiceProviderGlobal::fileNameFromId("ch_sbb")) );
    QVERIFY( projects[i++]->loadProject(ServiceProviderGlobal::fileNameFromId("de_fahrplaner")) );
    QVERIFY( projects[i++]->loadProject(ServiceProviderGlobal::fileNameFromId("at_oebb")) );
    QVERIFY( projects[i++]->loadProject(ServiceProviderGlobal::fileNameFromId("dk_rejseplanen")) );
    QVERIFY( projects[i++]->loadProject(ServiceProviderGlobal::fileNameFromId("ie_eireann")) );

    // Create signal spies to test signals (testStarted() and testFinished())
    // Create an event loop that quits when a test in one of the projects
    // gets started of has finished
    QEventLoop loop;
    QList< QSignalSpy* > testBeginSpies;
    QList< QSignalSpy* > testEndSpies;
    foreach ( Project *project, projects ) {
        project->setQuestionsEnabled( false );
        model->appendProject( project );
        connect( project, SIGNAL(testStarted()), &loop, SLOT(quit()) );
        connect( project, SIGNAL(testFinished(bool)), &loop, SLOT(quit()) );
//         connect( project, SIGNAL(testFinished(bool)), project, SLOT(testProject()) );

        testBeginSpies << new QSignalSpy( project, SIGNAL(testStarted()) );
        testEndSpies << new QSignalSpy( project, SIGNAL(testFinished(bool)) );
    }

    QElapsedTimer timer;
    timer.start();

    // Call testProject() twice on each project at random times
    // to try to produce rare situations where the program may crash
    const int time = 2700;
    i = 0;
    QTimer::singleShot( 0, projects[i], SLOT(testProject()) );
    QTimer::singleShot( time, projects[i++], SLOT(testProject()) );
    QTimer::singleShot( 0.2 * time, projects[i], SLOT(testProject()) );
    QTimer::singleShot( 1.2 * time, projects[i++], SLOT(testProject()) );
    QTimer::singleShot( 0.4 * time, projects[i], SLOT(testProject()) );
    QTimer::singleShot( 0.8 * time, projects[i++], SLOT(testProject()) );
    QTimer::singleShot( 0.05 * time, projects[i], SLOT(testProject()) );
    QTimer::singleShot( 0.8 * time, projects[i++], SLOT(testProject()) );
    QTimer::singleShot( 0.05 * time, projects[i], SLOT(testProject()) );
    QTimer::singleShot( time, projects[i++], SLOT(testProject()) );
    QTimer::singleShot( 0.05 * time, projects[i], SLOT(testProject()) );
    QTimer::singleShot( time, projects[i++], SLOT(testProject()) );

    // Wait until testing starts
    loop.exec();

    // Wait until testing ends
    QTimer timeout;
    timeout.setInterval( 5000 ); // Wait 5 seconds for a project to finish it's tests
    connect( &timeout, SIGNAL(timeout()), &loop, SLOT(quit()) );
    forever {
        // Get some information from the debuggers
        QString infoString;
        foreach ( Project *project, projects ) {
            if ( !infoString.isEmpty() ) {
                infoString.append( ", " );
            }
            infoString.append( project->data()->id() + ' ' );
            if ( project->isTestRunning() ) {
                infoString.append( "Testing " );
            }
            if ( project->isDebuggerRunning() ) {
                infoString.append( QString("and debbuging at line %1 ")
                                   .arg(project->debugger()->lineNumber()) );
            }
            if ( !project->isTestRunning() && !project->isDebuggerRunning() ) {
                infoString.append( "Ready " );
            }
        }
        kDebug() << "State " << infoString << timer.elapsed() << time;

        if ( timer.elapsed() > time ) {
            bool testsFinished = true;
            foreach ( Project *project, projects ) {
                if ( TestModel::isFinishedState(project->testModel()->completeState()) ) {
                    kDebug() << "Test is finished for" << project->data()->id();
                    project->clearTestResults();
//                     disconnect( project, SIGNAL(testFinished(bool)), project, SLOT(testProject()) );
                }

                qApp->processEvents();
                if ( project->isTestRunning() ) {
                    testsFinished = false;
                    kDebug() << "Still running:" << project->data()->id();
                    break;
                }
            }
            if ( testsFinished ) {
                break;
            }
        }

        timeout.start();
        loop.exec();
        timeout.stop();
    }

    // Wait until all jobs are deleted
    QTimer::singleShot( 0, &loop, SLOT(quit()) );
    loop.exec();

    // Ensure the testFinished() signals got emitted as often as testStarted()
    for ( int i = 0; i < testBeginSpies.count(); ++i ) {
        QCOMPARE( testBeginSpies[i]->count(), testEndSpies[i]->count() );
    }
    qDeleteAll( testBeginSpies );
    qDeleteAll( testEndSpies );
}

void DebuggerTest::testAbortionTest()
{
    // Load some more projects
    ProjectModel *model = new ProjectModel( this );
    Project *project = new Project( model->weaver() );
    QVERIFY( project->loadProject(ServiceProviderGlobal::fileNameFromId("de_db")) );

    // Create signal spies to test signals (testStarted() and testFinished())
    // Create an event loop that quits when a test in one of the projects
    // gets started of has finished
    QEventLoop loop;
    QSignalSpy *testBeginSpy, *testEndSpy;
    project->setQuestionsEnabled( false );
    model->appendProject( project );
    connect( project, SIGNAL(testStarted()), &loop, SLOT(quit()) );
    connect( project, SIGNAL(testFinished(bool)), &loop, SLOT(quit()) );
    testBeginSpy = new QSignalSpy( project, SIGNAL(testStarted()) );
    testEndSpy = new QSignalSpy( project, SIGNAL(testFinished(bool)) );

    QTimer::singleShot( 0, project, SLOT(testProject()) );
    QTimer::singleShot( 200, project, SLOT(abortTests()) );

    // Wait until testing starts
    loop.exec();

    // Wait until testing ends
    while ( project->isTestRunning() ) {
        loop.exec();
    }

    // Wait until all jobs are deleted
    QTimer::singleShot( 0, &loop, SLOT(quit()) );
    loop.exec();

    // Ensure the testFinished() signals got emitted as often as testStarted()
    QCOMPARE( testBeginSpy->count(), testEndSpy->count() );
    delete testBeginSpy;
    delete testEndSpy;
}

QTEST_MAIN(DebuggerTest)
#include "DebuggerTest.moc"
