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

#ifndef PublicTransportHelperTest_H
#define PublicTransportHelperTest_H

#define QT_GUI_LIB

#include <QtCore/QObject>
#include <publictransporthelper/stopsettings.h>

class PublicTransportHelperTest : public QObject
{
	Q_OBJECT
	
private slots:
	void initTestCase();
	void init();
	void cleanup();
	void cleanupTestCase();
	
	// Tests Stop
	void stopTest();
	
	// Tests StopSettings
	void stopSettingsTest();
	
	// Tests StopSettingsDialog with simple accessor selection option
	void stopSettingsDialogSimpleAccessorSelectionTest();
	
	// Tests StopSettingsDialog with simple stop selection option
	void stopSettingsDialogSimpleStopTest();
	
	// Tests StopSettingsDialog with extended accessor selection option
	void stopSettingsDialogExtendedStopTest();
	
	// Tests StopSettingsDialog with custom options
	void stopSettingsDialogCustomStopTest();
	
	// Tests StopSettingsDialog with a custom widget factory
	void stopSettingsDialogCustomFactoryTest();
	
	// Tests StopSettingsDialog with later added extended settings widgets with a custom widget factory
	void stopSettingsDialogAddWidgetsLaterCustomFactoryTest();
	
	// Tests StopWidget
	void stopWidgetTest();
	
	// Tests StopListWidget (including added/removed signals)
	void stopListWidgetTest();
	
	void locationModelTest();
	
private:
	StopSettings m_stopSettings;
	QStringList m_filterConfigurations;
};

#endif // PublicTransportHelperTest_H