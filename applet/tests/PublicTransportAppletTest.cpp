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

#include "PublicTransportAppletTest.h"

// This can be uncommented, to wait for updates to be drawn, animations to finish (manual check)
// #define WaitForGuiUpdates

#ifdef WaitForGuiUpdates
    #define UpdateGUI() for ( int i = 0; i < 50; ++i ) { QCoreApplication::processEvents(); QTest::qSleep(10); }
#else
    #define UpdateGUI()
#endif

#include <stopsettingsdialog.h>
#include <stopwidget.h>
#include <checkcombobox.h>
#include <filterwidget.h>
#include <filter.h>

#include <QtTest/QTest>
#include <QtTest/QtTestGui>
#include <QtTest/qtestkeyboard.h>
#include <QtTest/qtestmouse.h>

#include <QMetaMethod>
#include <QToolButton>
#include <KLineEdit>
#include <plasma/containment.h>
#include <Plasma/Corona>
#include <KConfigDialog>
#include <KMessageBox>
#include <KInputDialog>
#include <KPushButton>
#include <QTimer>
#include <QThread>

void PublicTransportAppletTest::initTestCase()
{
    m_dialog = NULL;
    m_pageGeneral = NULL;
    m_pageFilter = NULL;
    m_pageAlarms = NULL;
    m_pageGeneralWidget = NULL;
    m_pageFilterWidget = NULL;
    m_pageAlarmsWidget = NULL;
    m_pageWidget = NULL;
    m_pageModel = NULL;
    m_stopsWidget = NULL;
    m_filterConfigurationsWidget = NULL;
    m_filterAction = NULL;
    m_affectedStops = NULL;
    m_filtersWidget = NULL;
    m_addFilterConfiguration = NULL;
    m_removeFilterConfiguration = NULL;

    // Init settings
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

    FilterSettings filters1, filters2;
    filters1.name = "Filter configuration 1";
    Filter filter1 = Filter() << Constraint( FilterByTarget, FilterContains, "TestTarget" );
    filters1.filters << filter1;
    filters1.affectedStops << 0;

    filters2.name = "Filter configuration 2";
    Filter filter2 = Filter() << Constraint( FilterByTarget, FilterContains, "TestTarget2" );
    filters2.filters << filter2;

    m_filterConfigurations << filters1 << filters2;

    // Add the desktop containment
    m_corona = new Plasma::Corona( this );
    m_containment = m_corona->addContainment( "desktop" );
    QVERIFY2( m_containment, "The plasma desktop containment could not be added. Ensure that you "
                             "have plasma installed." );

    // Add the PublicTransport applet
    m_applet = m_containment->addApplet( "publictransport" );
    QVERIFY2( m_applet, "The plasma desktop containment could not be added. Ensure that you have "
                        "plasma installed." );
}

void PublicTransportAppletTest::init()
{
    // Set settings of the PublicTransport applet using a specific slot
    int indexSetSettings = m_applet->metaObject()->indexOfSlot(
            /*QMetaType::*/ "setSettings(StopSettingsList,FilterSettingsList)" );
    QVERIFY2( indexSetSettings != -1, "Couldn't find slot with signarture "
              "setSettings(StopSettingsList,FilterSettingsList) in the publicTransport m_applet." );

    bool success = m_applet->metaObject()->method( indexSetSettings ).invoke( m_applet,
            Q_ARG(QList<StopSettings>, StopSettingsList() << m_stopSettings),
            Q_ARG(FilterSettingsList, m_filterConfigurations) );
    QVERIFY2( success, "A call to setSettings in the publicTransport m_applet wasn't successful." );

    // Create and show the configuration dialog
    KConfigSkeleton config;
    m_dialog = new KConfigDialog( NULL, "Applet Settings", &config );
    m_applet->createConfigurationInterface( m_dialog );
    m_dialog->show();

    // Find main page widget and get it's model
    m_pageWidget = m_dialog->findChild<KPageWidget*>();
    QVERIFY2( m_pageWidget, "No KPageWidget found in the dialog." );
    m_pageModel = static_cast<KPageWidgetModel*>(m_pageWidget->model());
    QVERIFY2( m_pageModel, "No KPageWidgetModel found for the KPageWidget." );

    // Find sub widgets (inside the pages)
    m_pageGeneralWidget = m_dialog->findChild<QWidget*>("generalTabWidget");
    QVERIFY2( m_pageGeneralWidget, "The widget for the general page wasn't found (widget with name 'generalTabWidget')." );
    m_pageFilterWidget = m_dialog->findChild<QWidget*>("publicTransportFilterConfig");
    QVERIFY2( m_pageFilterWidget, "The widget for the filter page wasn't found (widget with name 'publicTransportFilterConfig')." );
    m_pageAlarmsWidget = m_dialog->findChild<QWidget*>("alarmConfig");
    QVERIFY2( m_pageAlarmsWidget, "The widget for the alarms page wasn't found (widget with name 'alarmConfig')." );

    // Find page items
    m_pageGeneral = NULL;
    m_pageFilter = NULL;
    m_pageAlarms = NULL;
    for ( int row = 0; row < m_pageModel->rowCount(); ++row ) {
        KPageWidgetItem *page = m_pageModel->item( m_pageModel->index(row, 0) );
        if ( page->widget() == m_pageGeneralWidget->parentWidget() ) {
            m_pageGeneral = page;
        } else if ( page->widget() == m_pageFilterWidget->parentWidget() ) {
            m_pageFilter = page;
        } else if ( page->widget() == m_pageAlarmsWidget->parentWidget() ) {
            m_pageAlarms = page;
        }
    }
    QVERIFY2( m_pageGeneral, "General page wasn't found in the configuration dialog of the applet." );
    QVERIFY2( m_pageFilter, "Filter page wasn't found in the configuration dialog of the applet." );
    QVERIFY2( m_pageAlarms, "Alarms page wasn't found in the configuration dialog of the applet." );

    // Find stop list widgets
    m_stopsWidget = m_pageGeneralWidget->findChild<StopListWidget*>();
    QVERIFY2( m_stopsWidget, "The StopListWidget showing the list of stops wasn't found." );

    // Find filter widgets
    m_filterConfigurationsWidget = m_pageFilterWidget->findChild<KComboBox*>( "filterConfigurations" );
    QVERIFY2( m_filterConfigurationsWidget, "The KCombobox showing the filter configurations wasn't found (widget with name 'filterConfigurations')." );
    m_filtersWidget = m_pageFilterWidget->findChild<FilterListWidget*>("filters");
    QVERIFY2( m_filtersWidget, "The widget showing the current filter constraints wasn't found (widget with name 'filters')." );
    m_affectedStops = m_pageFilterWidget->findChild<CheckCombobox*>("affectedStops");
    QVERIFY2( m_affectedStops, "The widget showing the affected stops of the current filter wasn't found (widget with name 'affectedStops')." );
    m_filterAction = m_pageFilterWidget->findChild<KComboBox*>("filterAction");
    QVERIFY2( m_filterAction, "The widget showing the action of the current filter wasn't found (widget with name 'filterAction')." );
    m_addFilterConfiguration = m_pageFilterWidget->findChild<QToolButton*>("addFilterConfiguration");
    QVERIFY2( m_addFilterConfiguration, "The widget to add a new filter configuration wasn't found (widget with name 'addFilterConfiguration')." );
    m_removeFilterConfiguration = m_pageFilterWidget->findChild<QToolButton*>("removeFilterConfiguration");
    QVERIFY2( m_removeFilterConfiguration, "The widget to remove a filter configuration wasn't found (widget with name 'removeFilterConfiguration')." );
}

void PublicTransportAppletTest::cleanup()
{
    delete m_dialog;
    m_dialog = NULL;
    m_pageGeneral = NULL;
    m_pageFilter = NULL;
    m_pageAlarms = NULL;
    m_pageGeneralWidget = NULL;
    m_pageFilterWidget = NULL;
    m_pageAlarmsWidget = NULL;
    m_pageWidget = NULL;
    m_pageModel = NULL;
    m_stopsWidget = NULL;
    m_filterConfigurationsWidget = NULL;
    m_filterAction = NULL;
    m_affectedStops = NULL;
    m_filtersWidget = NULL;
    m_addFilterConfiguration = NULL;
    m_removeFilterConfiguration = NULL;
}

void PublicTransportAppletTest::cleanupTestCase()
{
    m_containment->clearApplets();
    delete m_applet;
    delete m_containment;
}

void PublicTransportAppletTest::simulateAddFilterConfiguration() {
    // Click the add filter configuration button,
    // call dialogDone later to automatically accept the dialog without blocking here
    QTimer::singleShot( 50, this, SLOT(acceptSubDialog()) );
    QTest::mouseClick( m_addFilterConfiguration, Qt::LeftButton ); // Blocks here until the dialogDone() timer signals
    UpdateGUI();
}

void PublicTransportAppletTest::simulateRemoveFilterConfiguration() {
    // Click the add filter configuration button,
    // call dialogDone later to automatically accept the dialog without blocking here
    QTimer::singleShot( 50, this, SLOT(acceptSubDialog()) );
    QTest::mouseClick( m_removeFilterConfiguration, Qt::LeftButton ); // Blocks here until the dialogDone() timer signals
    UpdateGUI();
}

QList<int> vehicleTypesToRows( const QVariantList &vehicleTypes,
                               QAbstractItemModel *vehicleConstraintListModel )
{
    QList<int> rows;
    // "Translate" checkedVehicles (stored in Qt::UserRole) to checkedVehicleRows
    for ( int row = 0; row < vehicleConstraintListModel->rowCount(); ++row ) {
        QModelIndex index = vehicleConstraintListModel->index(row, 0);
        VehicleType currentVehicleType = static_cast<VehicleType>(
                vehicleConstraintListModel->data(index, Qt::UserRole).toInt() );
        if ( vehicleTypes.contains(currentVehicleType) ) {
            rows << row;
        }
    }
    return rows;
}

void PublicTransportAppletTest::appletTest()
{
    m_dialog->setCurrentPage( m_pageFilter );

    // Check if all filter configuration names are listed in the combobox
    QCOMPARE( m_filterConfigurationsWidget->count(), m_filterConfigurations.count() );
    for ( int i = 0; i < m_filterConfigurationsWidget->count(); ++i ) {
        QCOMPARE( m_filterConfigurationsWidget->itemText(i), m_filterConfigurations[i].name );
        QCOMPARE( (*m_stopsWidget->filterConfigurations())[i].name, m_filterConfigurations[i].name );
    }

    // Compare values in the filter widgets with the values in the configuration object
    FilterSettings currentFilterSettings = m_filterConfigurations[
            m_filterConfigurationsWidget->currentIndex() ];
    QCOMPARE( m_filtersWidget->filters().count(),
              qMax(m_filtersWidget->minimumWidgetCount(), currentFilterSettings.filters.count()) );
    QCOMPARE( m_filtersWidget->filters(), currentFilterSettings.filters );
    QCOMPARE( m_affectedStops->checkedRows().toSet(), currentFilterSettings.affectedStops );
    QCOMPARE( static_cast<FilterAction>(m_filterAction->currentIndex()),
              currentFilterSettings.filterAction );

    // Add a filter configuration by clicking the add button
    simulateAddFilterConfiguration();

    // Check if all filter configuration names are still listed in the combobox
    QCOMPARE( m_filterConfigurationsWidget->count(), m_filterConfigurations.count() + 1 ); // There should be one more filter configuration now
    QCOMPARE( m_filtersWidget->filterWidgets().count(), 1 );
    QCOMPARE( m_filtersWidget->filterWidgets().first()->constraintWidgets().count(), 1 ); // New filter configuration should have one default constraint
    for ( int i = 0; i < m_filterConfigurations.count(); ++i ) {
        // The first values shouldn't be changed when adding another filter configuration
        QCOMPARE( m_filterConfigurationsWidget->itemText(i), m_filterConfigurations[i].name );
        QCOMPARE( (*m_stopsWidget->filterConfigurations())[i].name, m_filterConfigurations[i].name );
    }

    // Change filter of the newly added filter configuration
    ConstraintListWidget *vehicleConstraint = qobject_cast<ConstraintListWidget*>(
                m_filtersWidget->filterWidgets().first()->constraintWidgets().first() );
    CheckCombobox *vehicleConstraintList = vehicleConstraint->list();
    QAbstractItemModel *vehicleConstraintListModel = vehicleConstraintList->model();
    QVariantList checkedVehicles = QVariantList() << static_cast<int>(Tram) << static_cast<int>(Bus);
    QList<int> checkedVehicleRows = vehicleTypesToRows( checkedVehicles, vehicleConstraintListModel );
    vehicleConstraintList->setCheckedRows( checkedVehicleRows );
    QCOMPARE( vehicleConstraint->value().toList(), checkedVehicles );
    UpdateGUI();

    // Store current filter settings from the widgets (currently at the new filter configuration)
    Filter newFilter;
    newFilter << Constraint( FilterByVehicleType, FilterIsOneOf, checkedVehicles );
    FilterSettings newFilterSettings( m_filterConfigurationsWidget->currentText() );
    newFilterSettings.affectedStops = m_affectedStops->checkedRows().toSet();
    newFilterSettings.filterAction = static_cast<FilterAction>(m_filterAction->currentIndex());
    newFilterSettings.filters << newFilter;
    QCOMPARE( m_filtersWidget->filters(), newFilterSettings.filters );

    // Select all except the newly added filter configuration and test values of widgets
    for ( int i = 0; i < m_filterConfigurations.count(); ++i ) {
        m_filterConfigurationsWidget->setCurrentIndex( i );
        QCoreApplication::processEvents(); // Wait for filter widgets to get updated
        FilterSettings currentFilterSettings = m_filterConfigurations[ i ];

        QCOMPARE( m_filterConfigurationsWidget->currentText(), currentFilterSettings.name );
        QCOMPARE( m_filtersWidget->filters().count(),
                  qMax(m_filtersWidget->minimumWidgetCount(), currentFilterSettings.filters.count()) );
        QCOMPARE( m_filtersWidget->filters(), currentFilterSettings.filters );
        QCOMPARE( m_affectedStops->checkedRows().toSet(), currentFilterSettings.affectedStops );
        QCOMPARE( static_cast<FilterAction>(m_filterAction->currentIndex()),
                  currentFilterSettings.filterAction );
        UpdateGUI();
    }

    // Select newly added (and changed) filter configuration
    m_filterConfigurationsWidget->setCurrentIndex( 2 );
    QCoreApplication::processEvents(); // Wait for filter widgets to get updated
    UpdateGUI();

    // Check filter widget values
    QCOMPARE( m_filterConfigurationsWidget->currentText(), newFilterSettings.name );
    QCOMPARE( m_filtersWidget->filters().count(),
              qMax(m_filtersWidget->minimumWidgetCount(), newFilterSettings.filters.count()) );
    QCOMPARE( m_filtersWidget->filters(), newFilterSettings.filters );
    QCOMPARE( m_affectedStops->checkedRows().toSet(), newFilterSettings.affectedStops );
    QCOMPARE( static_cast<FilterAction>(m_filterAction->currentIndex()),
              newFilterSettings.filterAction );
    vehicleConstraint = qobject_cast<ConstraintListWidget*>(
            m_filtersWidget->filterWidgets().first()->constraintWidgets().first() );
    vehicleConstraintList = vehicleConstraint->list();
    QCOMPARE( vehicleConstraint->value().toList(), checkedVehicles );

    // Check values in a StopSettingsDialog, which gets opened from StopListWidget
    m_dialog->setCurrentPage( m_pageGeneral );

    QTimer::singleShot( 50, this, SLOT(checkAndCloseStopSettingsDialog()) );
    m_stopsWidget->stopWidget(0)->editSettings();

//     TODO add more stop settings in init()
//     QTimer::singleShot( 50, this, SLOT(checkAndCloseStopSettingsDialog()) );
//     m_stopsWidget->stopWidget(1)->editSettings();
}

void PublicTransportAppletTest::checkAndCloseStopSettingsDialog()
{
    QVERIFY2( m_dialog, "No config dialog created?" );

    QPointer<StopSettingsDialog> stopSettingsDialog = NULL;
    stopSettingsDialog = m_dialog->findChild<StopSettingsDialog*>();
    QVERIFY2( stopSettingsDialog, "No stop settings dialog found" );

    CheckCombobox *filterConfigurationsOfStopWidget = qobject_cast<CheckCombobox*>(
            stopSettingsDialog->settingWidget(FilterConfigurationSetting) );
    QVERIFY2( filterConfigurationsOfStopWidget, "No filter configuration setting found in the StopSettingsDialog" );

    QSet<int> filterConfigurationsOfStop = filterConfigurationsOfStopWidget->checkedRows().toSet();
    int stopIndex = stopSettingsDialog->stopIndex();
    for ( int index = 0; index < m_filterConfigurations.count(); ++index ) {
        const FilterSettings &filterConfiguration = m_filterConfigurations[index];
        if ( filterConfiguration.affectedStops.contains(stopIndex) ) {
            QVERIFY2( filterConfigurationsOfStop.contains(index), "If a filter configuration is "
                    "checked for the stop in the StopSettingsDialog, the filter configuration "
                    "should have that stop in it's list of affected stops" );
        }
    }

    kDebug() << "Close opened sub dialog" << stopSettingsDialog->windowTitle() << stopSettingsDialog;
    QTest::mouseClick( stopSettingsDialog->button(KDialog::Cancel), Qt::LeftButton );
}

void PublicTransportAppletTest::acceptSubDialog()
{
    QVERIFY2( m_dialog, "No config dialog created?" );

    QPointer<KDialog> subDialog = NULL;
    subDialog = m_dialog->findChild<KDialog*>(); // KInputDialogHelper
    QVERIFY2( subDialog, "No sub dialog found" );

    kDebug() << "Close opened sub dialog" << subDialog->windowTitle() << subDialog;
    QTest::mouseClick( subDialog->button(KDialog::Ok), Qt::LeftButton );
}

QTEST_MAIN(PublicTransportAppletTest)
#include "PublicTransportAppletTest.moc"
