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

#include "PublicTransportHelperGuiTest.h"

#include "../stopsettingsdialog.h"
#include "../stopwidget.h"

#include <QtTest/QTest>
#include <QtTest/QtTestGui>
#include <QtTest/qtestkeyboard.h>
#include <QtTest/qtestmouse.h>

#include <QToolButton>
#include <KLineEdit>

void PublicTransportHelperGuiTest::initTestCase()
{
	m_stopSettings.setStop( Stop(QLatin1String("Custom Stop"), QLatin1String("123456")) );
	QCOMPARE( m_stopSettings.stops().count(), 1 );
	QCOMPARE( m_stopSettings.stopList().count(), 1 );
	QCOMPARE( m_stopSettings.stop(0).name, QLatin1String("Custom Stop") );
	QCOMPARE( m_stopSettings.stop(0).id, QLatin1String("123456") );
	QCOMPARE( m_stopSettings.stop(0).nameOrId(), QLatin1String("123456") );
	
	m_stopSettings.set( ServiceProviderSetting, QLatin1String("de_db") );
	QCOMPARE( m_stopSettings[ServiceProviderSetting].toString(), QLatin1String("de_db") );
	
	m_stopSettings.set( LocationSetting, QLatin1String("de") );
	QCOMPARE( m_stopSettings[LocationSetting].toString(), QLatin1String("de") );
	
	m_filterConfigurations << "Filter configuration 1" << "Filter configuration 2";
}

void PublicTransportHelperGuiTest::init()
{}

void PublicTransportHelperGuiTest::cleanup()
{}

void PublicTransportHelperGuiTest::cleanupTestCase()
{}

void PublicTransportHelperGuiTest::stopSettingsDialogGuiTest()
{
	// Set a valid service provider ID and a single stop
	m_stopSettings.set( LocationSetting, "de" );
	m_stopSettings.set( ServiceProviderSetting, "de_db" );
	m_stopSettings.setStop( QString() );
	StopSettingsDialog *dlg = StopSettingsDialog::createExtendedStopSelectionDialog( 0,
			m_stopSettings, m_filterConfigurations );
	
	// Test stops container widget for visibility
	QWidget *stops = dlg->findChild< QWidget* >( "stops" );
	QVERIFY( stops && stops->isVisibleTo(dlg) ); // Stops widget should be visible
	
	// Ensure the stop list widget has been created
	DynamicLabeledLineEditList *stopList = stops->findChild<DynamicLabeledLineEditList*>();
	QVERIFY( stopList );
	
	// There should be one stop widget (one stop set in m_stopSettings)
	QCOMPARE( stopList->widgetCount(), 1 );
	
	// Show dialog and simulate clicks on add/remove stop buttons
	dlg->show();
	QTest::qWaitForWindowShown( dlg );
	QVERIFY( stopList->addButton() );
	QTest::mouseClick( stopList->addButton(), Qt::LeftButton );
	QCOMPARE( stopList->widgetCount(), 2 );
	QTest::mouseClick( stopList->addButton(), Qt::LeftButton );
	QCOMPARE( stopList->widgetCount(), 3 );
	
	// Remove the second stop
	QVERIFY( stopList->dynamicWidget(1)->removeButton() );
	QTest::mouseClick( stopList->dynamicWidget(1)->removeButton(), Qt::LeftButton );
	QCOMPARE( stopList->widgetCount(), 2 );
	
	// Remove the new second stop
	QVERIFY( stopList->dynamicWidget(1)->removeButton() );
	QTest::mouseClick( stopList->dynamicWidget(1)->removeButton(), Qt::LeftButton );
	QCOMPARE( stopList->widgetCount(), 1 );
	
	// Ensure that there is no remove button for the last stop (because minimum widget count is 1)
	QVERIFY( !stopList->dynamicWidget(0)->removeButton() );
	
	// Get the stop edit and simulate entering a stop name, then wait for stop suggestions
	KLineEdit *stopEdit = stopList->lineEditWidgets().first();
	QVERIFY( stopEdit );
	stopEdit->clear();
	stopEdit->setFocus(); // Set focus, because stop suggestions are used only for the focused widget
	QTest::keyClicks( stopEdit, QString::fromUtf8("Berlin") );
	// Wait until stop suggestions arrive (maximally 5 secs)
	for ( int i = 0; i < 50; ++i ) {
		stopEdit->setFocus(); // Ensure focus is still set
		QTest::qWait( 100 );
		if ( !stopEdit->completionObject()->isEmpty() ) {
			qDebug() << "Waited ~" << (i * 100) << "ms for stop suggestions";
			break;
		}
	}
	// TODO When run using "make test" this fails (the script crashes on regexp.exec()).
	//      When run using "ctest" or when the test executable is run directly this succeeds.
	// There should be at least one stop suggestion for the used stop name
	QVERIFY2( stopEdit->completionObject()->allMatches().count() > 0, 
			  "Waited 5 seconds for stop suggestions from de_db for \"Berlin\".. got none, "
			  "maybe there's no connection to the service provider or it's too slow." );
	
	delete dlg;
}

QTEST_MAIN(PublicTransportHelperGuiTest)
#include "PublicTransportHelperGuiTest.moc"
