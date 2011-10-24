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

#ifndef PublicTransportAppletTest_H
#define PublicTransportAppletTest_H

#define QT_GUI_LIB

#include <QtCore/QObject>
#include <stopsettings.h>
#include <filter.h>

class CheckCombobox;
class QToolButton;
class KComboBox;
class KPageWidgetItem;
class KPageWidgetModel;
class KPageWidget;
class KConfigDialog;
namespace Plasma {
    class Applet;
    class Containment;
    class Corona;
};
namespace Timetable {
    class StopListWidget;
    class FilterListWidget;
};
using namespace Timetable;

class PublicTransportAppletTest : public QObject
{
    Q_OBJECT

public slots:
    void acceptSubDialog();
    void checkAndCloseStopSettingsDialog();
    void simulateAddFilterConfiguration();
    void simulateRemoveFilterConfiguration();
	
private slots:
    // Initialize containment and applet, set settings of the applet
	void initTestCase();

    // Create the applet's configuration dialog and find widgets (store in member variables)
    // The dialog gets created for each test (because init is called before each test)
	void init();

    // Close and delete the configuration dialog
	void cleanup();

	void cleanupTestCase();
	
    void appletTest();
	
private:
	StopSettings m_stopSettings;
	FilterSettingsList m_filterConfigurations;

    Plasma::Applet *m_applet;
    Plasma::Containment *m_containment;
    Plasma::Corona *m_corona;

    KConfigDialog *m_dialog;
    KPageWidget *m_pageWidget;
    KPageWidgetModel *m_pageModel;

    KPageWidgetItem *m_pageGeneral;
    KPageWidgetItem *m_pageFilter;
    KPageWidgetItem *m_pageAlarms;

    QWidget *m_pageGeneralWidget;
    QWidget *m_pageFilterWidget;
    QWidget *m_pageAlarmsWidget;

    StopListWidget *m_stopsWidget;

    KComboBox *m_filterConfigurationsWidget;
    FilterListWidget *m_filtersWidget;
    CheckCombobox *m_affectedStops;
    KComboBox *m_filterAction;
    QToolButton *m_addFilterConfiguration;
    QToolButton *m_removeFilterConfiguration;
};

Q_DECLARE_METATYPE( QList<int> );

#endif // PublicTransportAppletTest_H
