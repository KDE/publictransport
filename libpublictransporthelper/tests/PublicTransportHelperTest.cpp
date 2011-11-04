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

#include "PublicTransportHelperTest.h"

#include "../stopsettingsdialog.h"
#include "../stopwidget.h"
#include "../locationmodel.h"
#include "../checkcombobox.h"

#include <Plasma/DataEngineManager>
#include <KComboBox>
#include <KLineEdit>

#include <QtTest/QTest>
#include <QToolButton>
#include <QSpinBox>
#include <QTimeEdit>
#include <QRadioButton>
#include <qsignalspy.h>

void PublicTransportHelperTest::initTestCase()
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

    FilterSettings filterSettings1, filterSettings2;
    filterSettings1.name = "Filter configuration 1";
    filterSettings2.name = "Filter configuration 2";
    m_filterConfigurations << filterSettings1 << filterSettings2;
}

void PublicTransportHelperTest::init()
{}

void PublicTransportHelperTest::cleanup()
{}

void PublicTransportHelperTest::cleanupTestCase()
{}

void PublicTransportHelperTest::stopTest()
{
    // Test const char* conversion constructor
    Stop stop = "Test";
    QCOMPARE( stop.name, QLatin1String("Test") );
    QCOMPARE( stop.id, QString() );

    // Test QLatin1String conversion constructor
    stop = QLatin1String("Test2");
    QCOMPARE( stop.name, QLatin1String("Test2") );
    QCOMPARE( stop.id, QString() );

    // Test QString conversion constructor
    stop = QString("Test3");
    QCOMPARE( stop.name, QLatin1String("Test3") );
    QCOMPARE( stop.id, QString() );

    // Test QString constructor / assignment operator
    stop = Stop("Test4");
    QCOMPARE( stop.name, QLatin1String("Test4") );
    QCOMPARE( stop.id, QString() );
    QCOMPARE( stop.nameOrId(), QLatin1String("Test4") );

    // Test QString constructor with stop ID
    stop = Stop("Test5", "ID1");
    QCOMPARE( stop.name, QLatin1String("Test5") );
    QCOMPARE( stop.id, QLatin1String("ID1") );
    QCOMPARE( stop.nameOrId(), QLatin1String("ID1") );

    // Test equality operator with use of conversion constructors
    Stop stop2 = stop;
    QCOMPARE( stop, stop2 );
    QCOMPARE( Stop("Test"), (Stop)"Test" );
    QCOMPARE( Stop("Test"), (Stop)QLatin1String("Test") );
    QCOMPARE( Stop("Test"), (Stop)QString("Test") );
    QCOMPARE( Stop("Test"), Stop("Test") );
    QCOMPARE( Stop("Test", "ID"), Stop("Test", "ID") );

    // Test equality with stop ID only given for one of the stops
    QCOMPARE( Stop("Test"), Stop("Test", "ID") );
    QCOMPARE( Stop("Test", "ID"), Stop("Test") );
}

void PublicTransportHelperTest::stopSettingsTest()
{
    // Test copy constructor
    StopSettings stopSettings = m_stopSettings;
    QCOMPARE( stopSettings, m_stopSettings );

    // Test setStop, stops, stop with special chars
    stopSettings.setStop( QLatin1String("Test Special Chars ÄÖÜöäüßéêèñ") );
    QCOMPARE( stopSettings.stops().count(), 1 );
    QCOMPARE( stopSettings.stop(0).name, QLatin1String("Test Special Chars ÄÖÜöäüßéêèñ") );
    // Conversion to QString gets the name
    QCOMPARE( (QString)stopSettings.stop(0), QLatin1String("Test Special Chars ÄÖÜöäüßéêèñ") );

    // Test setStop, stops, stop
    stopSettings.setStop( QLatin1String("Test Stopname") );
    QCOMPARE( stopSettings.stops().count(), 1 );
    QCOMPARE( stopSettings.stop(0).nameOrId(), QLatin1String("Test Stopname") );

    // Test setStop with a stop ID
    stopSettings.setStop( Stop(QLatin1String("Test Stopname"), QLatin1String("ID321")) );
    QCOMPARE( stopSettings.stops().count(), 1 );
    QCOMPARE( stopSettings.stop(0).nameOrId(), QLatin1String("ID321") );
    QCOMPARE( stopSettings.stopList().count(), 1 );;
    QCOMPARE( stopSettings.stopList().first().name, QLatin1String("Test Stopname") );
    QCOMPARE( stopSettings.stopList().first().id, QLatin1String("ID321") );

    // Test setStops( QStringList )
    stopSettings.setStops( QStringList() << QLatin1String("Stop1") << QLatin1String("Stop2") );
    QCOMPARE( stopSettings.stops().count(), 2 );
    QCOMPARE( stopSettings.stopList().count(), 2 );
    QCOMPARE( stopSettings.stop(0).name, QLatin1String("Stop1") );
    QCOMPARE( stopSettings.stop(1).name, QLatin1String("Stop2") );
    QCOMPARE( stopSettings.stop(0).id, QString() );
    QCOMPARE( stopSettings.stop(1).id, QString() );

    // Test setStops( QStringList, QStringList )
    stopSettings.setStops( QStringList() << QLatin1String("Stop1") << QLatin1String("Stop2"),
        QStringList() << QLatin1String("ID1") << QLatin1String("ID2") );
    QCOMPARE( stopSettings.stops().count(), 2 );
    QCOMPARE( stopSettings.stopList().count(), 2 );
    QCOMPARE( stopSettings.stop(0).name, QLatin1String("Stop1") );
    QCOMPARE( stopSettings.stop(1).name, QLatin1String("Stop2") );
    QCOMPARE( stopSettings.stop(0).id, QLatin1String("ID1") );
    QCOMPARE( stopSettings.stop(1).id, QLatin1String("ID2") );

    // Test setStops( StopNameList )
    StopList stops;
    stops << Stop(QLatin1String("Stop1"), QLatin1String("ID1"));
    stops << Stop(QLatin1String("Stop2"), QLatin1String("ID2"));
    stopSettings.setStops( stops );
    QCOMPARE( stopSettings.stops().count(), 2 );
    QCOMPARE( stopSettings.stop(0).nameOrId(), QLatin1String("ID1") );
    QCOMPARE( stopSettings.stop(1).nameOrId(), QLatin1String("ID2") );
    QCOMPARE( stopSettings.stopList().count(), 2 );
    QCOMPARE( stopSettings.stop(0).name, QLatin1String("Stop1") );
    QCOMPARE( stopSettings.stop(1).name, QLatin1String("Stop2") );
    QCOMPARE( stopSettings.stop(0).id, QLatin1String("ID1") );
    QCOMPARE( stopSettings.stop(1).id, QLatin1String("ID2") );

    // Test (set)city
    stopSettings.set( CitySetting, QLatin1String("Test city") );
    QCOMPARE( stopSettings[CitySetting].toString(), QLatin1String("Test city") );
    QCOMPARE( stopSettings.get<QString>(CitySetting), QLatin1String("Test city") );

    // Test set city with []-operator
    stopSettings[CitySetting] = QLatin1String("City 2");
    QCOMPARE( stopSettings[CitySetting].toString(), QLatin1String("City 2") );
    QCOMPARE( stopSettings.get<QString>(CitySetting), QLatin1String("City 2") );

    // Test (set)location
    stopSettings.set( LocationSetting, QLatin1String("Test location") );
    QCOMPARE( stopSettings[LocationSetting].toString(), QLatin1String("Test location") );
    QCOMPARE( stopSettings.get<QString>(LocationSetting), QLatin1String("Test location") );

    // Test (set)serviceProviderID
    stopSettings.set( ServiceProviderSetting, QLatin1String("de_xx") );
    QCOMPARE( stopSettings[ServiceProviderSetting].toString(), QLatin1String("de_xx") );
    QCOMPARE( stopSettings.get<QString>(ServiceProviderSetting), QLatin1String("de_xx") );

    // Test (set)setting
    QCOMPARE( stopSettings.hasSetting(AlarmTimeSetting), false );
    stopSettings.set( AlarmTimeSetting, 4 );
    QCOMPARE( stopSettings.hasSetting(AlarmTimeSetting), true );
    QCOMPARE( stopSettings[AlarmTimeSetting].toInt(), 4 );
    QCOMPARE( stopSettings.get<int>(AlarmTimeSetting), 4 );
//     QCOMPARE( stopSettings.extendedSettings().count(), 1 ); // Now also stores stop names, location, ...
    QVERIFY( stopSettings.usedSettings().contains(AlarmTimeSetting) );

    // Test clearSettings
    stopSettings.clearSetting( AlarmTimeSetting );
    QCOMPARE( stopSettings.hasSetting(AlarmTimeSetting), false );

    // Test (set)extendedSetting with UserSetting
    QCOMPARE( stopSettings.hasSetting(UserSetting), false );
    QStringList testStringList;
    testStringList << "Test1" << "Test2";
    stopSettings.set( UserSetting, testStringList );
    QCOMPARE( stopSettings.hasSetting(UserSetting), true );
    QCOMPARE( stopSettings[UserSetting].toStringList(), testStringList );
//     QCOMPARE( stopSettings.extendedSettings().count(), 1 );
    QVERIFY( stopSettings.usedSettings().contains(UserSetting) );

    // Test nameOrId
    stopSettings.setStop( Stop(QLatin1String("Teststop"), QLatin1String("TestID")) );
    QCOMPARE( stopSettings.stop(0).nameOrId(), QLatin1String("TestID") ); // an ID is available

    stopSettings.setStop( QLatin1String("Teststop") );
    QCOMPARE( stopSettings.stop(0).nameOrId(), QLatin1String("Teststop") ); // no ID available, use stop name as ID
}

void PublicTransportHelperTest::stopSettingsDialogSimpleAccessorSelectionTest()
{
    StopSettings stopSettings = m_stopSettings;
    stopSettings.set( LocationSetting, "cz" );
    stopSettings.set( ServiceProviderSetting, "cz_idnes" ); // Use a service provider with city selection
    StopSettingsDialog *dlg = StopSettingsDialog::createSimpleAccessorSelectionDialog(
            0, stopSettings );
    StopSettings stopSettings2 = dlg->stopSettings();
    QCOMPARE( stopSettings2[CitySetting].toString(), stopSettings[CitySetting].toString() );
    QCOMPARE( stopSettings2.stops(), stopSettings.stops() );
    QCOMPARE( stopSettings2.stopList(), stopSettings.stopList() );
    QCOMPARE( stopSettings2[ServiceProviderSetting].toString(),
              stopSettings[ServiceProviderSetting].toString() );
    QCOMPARE( stopSettings2[LocationSetting].toString(),
              stopSettings[LocationSetting].toString() );

    // The download accessor button should be visible (to the dialog, which is currently invisible)
    QToolButton *downloadServiceProviders = dlg->findChild< QToolButton* >( "downloadServiceProviders" );
    QVERIFY( downloadServiceProviders && downloadServiceProviders->isVisibleTo(dlg) );

    // Test location combobox for visibility and non-emptiness
    KComboBox *location = dlg->findChild< KComboBox* >( "location" );
    QVERIFY( location && location->isVisibleTo(dlg) ); // Location combobox should be visible
    QVERIFY( location->count() >= 1 ); // There should be at least one location

    // Test service provider combobox for visibility and non-emptiness
    KComboBox *serviceProvider = dlg->findChild< KComboBox* >( "serviceProvider" );
    QVERIFY( serviceProvider && serviceProvider->isVisibleTo(dlg) ); // Service provider combobox should be visible
    QVERIFY( serviceProvider->count() >= 1 ); // There should be at least one service provider

    // Test stops container widget for visibility
    QWidget *stops = dlg->findChild< QWidget* >( "stops" );
    QVERIFY( stops && !stops->isVisibleTo(dlg) ); // Stops widget should be invisible, it's an accessor selection dialog

    // Test city widget for visibility
    QWidget *city = dlg->findChild< QWidget* >( "city" );
    QVERIFY( city && !city->isVisibleTo(dlg) ); // City widget should not be visible

    // Test factory
    StopSettingsWidgetFactory::Pointer factory = dlg->factory();
    QVERIFY( factory );

    // Ensure the filter configuration widget wasn't created
    QVERIFY( !dlg->findChild<QWidget*>(
            factory->nameForSetting(FilterConfigurationSetting)) );

    // Ensure the alarm time widget wasn't created
    QVERIFY( !dlg->findChild<QWidget*>( factory->nameForSetting(AlarmTimeSetting)) );

    // Ensure the first departure widgets weren't created
    QVERIFY( !dlg->findChild<QWidget*>(factory->nameForSetting(FirstDepartureConfigModeSetting)) );
    QVERIFY( !dlg->findChild<QWidget*>(factory->nameForSetting(TimeOffsetOfFirstDepartureSetting)) );
    QVERIFY( !dlg->findChild<QWidget*>(factory->nameForSetting(TimeOfFirstDepartureSetting)) );

    // QCOMPARE( dlg.stopSettings(), m_stopSettings );
    stopSettings = dlg->stopSettings();

    stopSettings.setStop( QLatin1String("Another Stop") );
    QCOMPARE( stopSettings.stops().count(), 1 );
    QCOMPARE( stopSettings.stop(0).nameOrId(), QLatin1String("Another Stop") );

    dlg->setStopSettings( stopSettings );
    QCOMPARE( dlg->stopSettings(), stopSettings );

//     QList<int> extendedSettings = QList<int>()
//         << FilterConfigurationSetting << AlarmTimeSetting
//         << FirstDepartureConfigModeSetting,
//     StopSettingsWidgetFactory::Pointer factory = StopSettingsWidgetFactory::Pointer::create()

    delete dlg;
}

void PublicTransportHelperTest::stopSettingsDialogSimpleStopTest()
{
    StopSettingsDialog dlg( 0, m_stopSettings, StopSettingsDialog::SimpleStopSelection,
                            AccessorInfoDialog::DefaultOptions, &m_filterConfigurations );
    StopSettings stopSettings = dlg.stopSettings();
    QCOMPARE( stopSettings[CitySetting].toString(), m_stopSettings[CitySetting].toString() );
    QCOMPARE( stopSettings.stops(), m_stopSettings.stops() );
    QCOMPARE( stopSettings.stopList(), m_stopSettings.stopList() );
    QCOMPARE( stopSettings[ServiceProviderSetting].toString(),
              m_stopSettings[ServiceProviderSetting].toString() );
    QCOMPARE( stopSettings[LocationSetting].toString(),
              m_stopSettings[LocationSetting].toString() );
    m_stopSettings = dlg.stopSettings();

    // The download accessor button should be visible (to the dialog, which is currently invisible)
    QToolButton *downloadServiceProviders = dlg.findChild< QToolButton* >( "downloadServiceProviders" );
    QVERIFY( downloadServiceProviders && downloadServiceProviders->isVisibleTo(&dlg) );

    // Test location combobox for visibility and non-emptiness
    KComboBox *location = dlg.findChild< KComboBox* >( "location" );
    QVERIFY( location && location->isVisibleTo(&dlg) ); // Location combobox should be visible
    QVERIFY( location->count() >= 1 ); // There should be at least one location

    // Test service provider combobox for visibility and non-emptiness
    KComboBox *serviceProvider = dlg.findChild< KComboBox* >( "serviceProvider" );
    QVERIFY( serviceProvider && serviceProvider->isVisibleTo(&dlg) ); // Service provider combobox should be visible
    QVERIFY( serviceProvider->count() >= 1 ); // There should be at least one service provider

    // Test stops container widget for visibility
    QWidget *stops = dlg.findChild< QWidget* >( "stops" );
    QVERIFY( stops && stops->isVisibleTo(&dlg) ); // Stops widget should be visible

    // Test factory
    StopSettingsWidgetFactory::Pointer factory = dlg.factory();
    QVERIFY( factory );

    // Ensure the filter configuration widget wasn't created
    QVERIFY( !dlg.findChild<QWidget*>(
            factory->nameForSetting(FilterConfigurationSetting)) );

    // Ensure the alarm time widget wasn't created
    QVERIFY( !dlg.findChild<QWidget*>( factory->nameForSetting(AlarmTimeSetting)) );

    // Ensure the first departure widgets weren't created
    QVERIFY( !dlg.findChild<QWidget*>(factory->nameForSetting(FirstDepartureConfigModeSetting)) );
    QVERIFY( !dlg.findChild<QWidget*>(factory->nameForSetting(TimeOffsetOfFirstDepartureSetting)) );
    QVERIFY( !dlg.findChild<QWidget*>(factory->nameForSetting(TimeOfFirstDepartureSetting)) );

    // QCOMPARE( dlg.stopSettings(), m_stopSettings );
    m_stopSettings = dlg.stopSettings();

    m_stopSettings.setStop( QLatin1String("Another Stop") );
    QCOMPARE( m_stopSettings.stops().count(), 1 );
    QCOMPARE( m_stopSettings.stop(0).nameOrId(), QLatin1String("Another Stop") );

    dlg.setStopSettings( m_stopSettings );
    QCOMPARE( dlg.stopSettings(), m_stopSettings );
}

void PublicTransportHelperTest::stopSettingsDialogExtendedStopTest()
{
    StopSettingsDialog *dlg = StopSettingsDialog::createExtendedStopSelectionDialog(
            0, m_stopSettings, &m_filterConfigurations );

    // Test stopSettings() for standard settings
    StopSettings stopSettings = dlg->stopSettings();
    QCOMPARE( stopSettings[CitySetting].toString(), m_stopSettings[CitySetting].toString() );
    QCOMPARE( stopSettings.stops(), m_stopSettings.stops() );
    QCOMPARE( stopSettings.stopList(), m_stopSettings.stopList() );
    QCOMPARE( stopSettings[ServiceProviderSetting].toString(),
              m_stopSettings[ServiceProviderSetting].toString() );
    QCOMPARE( stopSettings[LocationSetting].toString(),
              m_stopSettings[LocationSetting].toString() );
    m_stopSettings = dlg->stopSettings();

    // The download accessor button should be visible (to the dialog, which is currently invisible)
    QToolButton *downloadServiceProviders = dlg->findChild< QToolButton* >( "downloadServiceProviders" );
    QVERIFY( downloadServiceProviders && downloadServiceProviders->isVisibleTo(dlg) );

    // Test location combobox for visibility and non-emptiness
    KComboBox *location = dlg->findChild< KComboBox* >( "location" );
    QVERIFY( location && location->isVisibleTo(dlg) ); // Location combobox should be visible
    QVERIFY( location->count() >= 1 ); // There should be at least one location

    // Test service provider combobox for visibility and non-emptiness
    KComboBox *serviceProvider = dlg->findChild< KComboBox* >( "serviceProvider" );
    QVERIFY( serviceProvider && serviceProvider->isVisibleTo(dlg) ); // Service provider combobox should be visible
    QVERIFY( serviceProvider->count() >= 1 ); // There should be at least one service provider

    // Test stops container widget for visibility
    QWidget *stops = dlg->findChild< QWidget* >( "stops" );
    QVERIFY( stops && stops->isVisibleTo(dlg) ); // Stops widget should be visible

    // Ensure the stop list widget has been created
    DynamicLabeledLineEditList *stopList = stops->findChild<DynamicLabeledLineEditList*>();
    QVERIFY( stopList );

    // Test factory
    StopSettingsWidgetFactory::Pointer factory = dlg->factory();
    QVERIFY( factory );

    // Ensure the filter configuration widget was created
    CheckCombobox *cmbFilterConfiguration = dlg->findChild<CheckCombobox*>(
            factory->nameForSetting(FilterConfigurationSetting) );
    QVERIFY( cmbFilterConfiguration );
    // Test filter configuration widget content
    QCOMPARE( cmbFilterConfiguration->count(), m_filterConfigurations.count() );

    // Ensure the alarm time widget was created
    QSpinBox *spinAlarmTime = dlg->findChild<QSpinBox*>(
            factory->nameForSetting(AlarmTimeSetting) );
    QVERIFY( spinAlarmTime );
    // Test alarm time widget content
    QCOMPARE( spinAlarmTime->value(),
              m_stopSettings[AlarmTimeSetting].toInt() );

    // Ensure the container widget for first departure time settings was created
    QVERIFY( dlg->findChild<QWidget*>(
             factory->nameForSetting(FirstDepartureConfigModeSetting)) );

    // Ensure the first departure time offset widget was created
    QSpinBox *spinTimeOffset = dlg->findChild<QSpinBox*>(
            factory->nameForSetting(TimeOffsetOfFirstDepartureSetting) );
    QVERIFY( spinTimeOffset );
    // Test time offset widget content
    QCOMPARE( spinTimeOffset->value(), m_stopSettings[TimeOffsetOfFirstDepartureSetting].toInt() );

    // Ensure the first departure time offset radio widget was created
    QRadioButton *radioTimeOffset = dlg->findChild<QRadioButton*>( QLatin1String("radio_") +
            factory->nameForSetting(TimeOffsetOfFirstDepartureSetting) );
    QVERIFY( radioTimeOffset );
    // Test time offset radio widget value
    FirstDepartureConfigMode firstDepartureConfigMode = static_cast<FirstDepartureConfigMode>(
            m_stopSettings[FirstDepartureConfigModeSetting].toInt() );
    QCOMPARE( radioTimeOffset->isChecked(), firstDepartureConfigMode == RelativeToCurrentTime );

    // Ensure the first departure custom time widget was created
    QTimeEdit *timeEditCustom = dlg->findChild<QTimeEdit*>(
            factory->nameForSetting(TimeOfFirstDepartureSetting) );
    QVERIFY( timeEditCustom );
    // Test custom first departure time widget content
    QCOMPARE( timeEditCustom->time(), m_stopSettings[TimeOfFirstDepartureSetting].toTime() );

    // Ensure the first departure custom time radio widget was created
    QRadioButton *radioTimeCustom = dlg->findChild<QRadioButton*>( QLatin1String("radio_") +
            factory->nameForSetting(TimeOfFirstDepartureSetting) );
    QVERIFY( radioTimeCustom );
    // Test custom time radio widget value
    QCOMPARE( radioTimeCustom->isChecked(), firstDepartureConfigMode == AtCustomTime );

    // Test some extended settings
    m_stopSettings = dlg->stopSettings();
    m_stopSettings.set( FilterConfigurationSetting,
                        QVariant::fromValue(m_filterConfigurations.first()) );
    m_stopSettings.set( AlarmTimeSetting, 10 );
    m_stopSettings.set( FirstDepartureConfigModeSetting,
                        static_cast<int>(firstDepartureConfigMode = AtCustomTime) );
    m_stopSettings.set( TimeOffsetOfFirstDepartureSetting, 8 );
    m_stopSettings.set( TimeOfFirstDepartureSetting, QTime(14, 30) );
    dlg->setStopSettings( m_stopSettings );
    QCOMPARE( dlg->stopSettings(), m_stopSettings );

    // Test if widget values have been changed correctly
    QAbstractItemModel *model = cmbFilterConfiguration->model();
    for ( int row = 0; row < model->rowCount() && row < m_filterConfigurations.count(); ++row ) {
        QCOMPARE( model->data(model->index(row, 0)).toString(),
                  m_filterConfigurations[row].name );
        // All filter configurations are currently NOT checked for all stops
        QVERIFY( model->data(model->index(row, 0), Qt::CheckStateRole) == Qt::Unchecked );
    }
    // TODO: Check different sets of affected stops for the filters
    QCOMPARE( spinAlarmTime->value(), 10 );
    QCOMPARE( spinTimeOffset->value(), 8 );
    QCOMPARE( timeEditCustom->time(), QTime(14, 30) );
    QCOMPARE( radioTimeOffset->isChecked(), firstDepartureConfigMode == RelativeToCurrentTime );
    QCOMPARE( radioTimeCustom->isChecked(), firstDepartureConfigMode == AtCustomTime );

    // Test stop name
    m_stopSettings.setStop( QLatin1String("Another Stop") );
    dlg->setStopSettings( m_stopSettings );
    QCOMPARE( dlg->stopSettings(), m_stopSettings );

    delete dlg;
}

void PublicTransportHelperTest::stopSettingsDialogCustomStopTest()
{
    StopSettingsDialog dlg( 0, m_stopSettings,
            StopSettingsDialog::ShowServiceProviderConfig |
            StopSettingsDialog::ShowStopInputField | StopSettingsDialog::ShowAlarmTimeConfig,
            AccessorInfoDialog::DefaultOptions, &m_filterConfigurations, -1,
            QList<int>() << AlarmTimeSetting );

    // Test stopSettings() for standard settings
    StopSettings stopSettings = dlg.stopSettings();
    QCOMPARE( stopSettings[CitySetting].toString(), m_stopSettings[CitySetting].toString() );
    QCOMPARE( stopSettings.stops(), m_stopSettings.stops() );
    QCOMPARE( stopSettings.stopList(), m_stopSettings.stopList() );
    QCOMPARE( stopSettings[ServiceProviderSetting].toString(),
              m_stopSettings[ServiceProviderSetting].toString() );
    QCOMPARE( stopSettings[LocationSetting].toString(), m_stopSettings[LocationSetting].toString() );
    m_stopSettings = dlg.stopSettings();

    // The download accessor button should be invisible
    // because StopSettingsDialog::ShowInstallAccessorButton isn't used in the constructor
    QToolButton *downloadServiceProviders = dlg.findChild< QToolButton* >( "downloadServiceProviders" );
    QVERIFY( downloadServiceProviders && !downloadServiceProviders->isVisibleTo(&dlg) );

    // Test location combobox for visibility and non-emptiness
    KComboBox *location = dlg.findChild< KComboBox* >( "location" );
    QVERIFY( location && location->isVisibleTo(&dlg) ); // Location combobox should be visible
    QVERIFY( location->count() >= 1 ); // There should be at least one location

    // Test service provider combobox for visibility and non-emptiness
    KComboBox *serviceProvider = dlg.findChild< KComboBox* >( "serviceProvider" );
    QVERIFY( serviceProvider && serviceProvider->isVisibleTo(&dlg) ); // Service provider combobox should be visible
    QVERIFY( serviceProvider->count() >= 1 ); // There should be at least one service provider

    // Test stops container widget for visibility
    QWidget *stops = dlg.findChild< QWidget* >( "stops" );
    QVERIFY( stops && stops->isVisibleTo(&dlg) ); // Stops widget should be visible

    // Test factory
    StopSettingsWidgetFactory::Pointer factory = dlg.factory();
    QVERIFY( factory );

    // Ensure the filter configuration widget wasn't created
    KComboBox *cmbFilterConfiguration = dlg.findChild<KComboBox*>(
            factory->nameForSetting(FilterConfigurationSetting) );
    QVERIFY( !cmbFilterConfiguration );

    // Ensure the alarm time widget was created
    QSpinBox *spinAlarmTime = dlg.findChild<QSpinBox*>(
            factory->nameForSetting(AlarmTimeSetting) );
    QVERIFY( spinAlarmTime );
    // Test alarm time widget content
    QCOMPARE( spinAlarmTime->value(), m_stopSettings[AlarmTimeSetting].toInt() );

    // Ensure the container widget for first departure time settings wasn't created
    QVERIFY( !dlg.findChild<QWidget*>(factory->nameForSetting(FirstDepartureConfigModeSetting)) );

    // Ensure the first departure time offset widget wasn't created
    QSpinBox *spinTimeOffset = dlg.findChild<QSpinBox*>(
            factory->nameForSetting(TimeOffsetOfFirstDepartureSetting) );
    QVERIFY( !spinTimeOffset );

    // Ensure the first departure time offset radio widget wasn't created
    QRadioButton *radioTimeOffset = dlg.findChild<QRadioButton*>( QLatin1String("radio_") +
            factory->nameForSetting(TimeOffsetOfFirstDepartureSetting) );
    QVERIFY( !radioTimeOffset );

    // Ensure the first departure custom time widget wasn't created
    QTimeEdit *timeEditCustom = dlg.findChild<QTimeEdit*>(
            factory->nameForSetting(TimeOfFirstDepartureSetting) );
    QVERIFY( !timeEditCustom );

    // Ensure the first departure custom time radio widget wasn't created
    QRadioButton *radioTimeCustom = dlg.findChild<QRadioButton*>( QLatin1String("radio_") +
            factory->nameForSetting(TimeOfFirstDepartureSetting) );
    QVERIFY( !radioTimeCustom );

    // Test changing an extended setting with an associated widget in the dialog
    m_stopSettings = dlg.stopSettings();
    m_stopSettings.set( AlarmTimeSetting, 10 );
    dlg.setStopSettings( m_stopSettings );
    QCOMPARE( dlg.stopSettings(), m_stopSettings );

    // Test if widget value has been changed correctly
    QCOMPARE( spinAlarmTime->value(), 10 );

    // Test stop name
    m_stopSettings.setStop( QLatin1String("Another Stop") );
    dlg.setStopSettings( m_stopSettings );
    QCOMPARE( dlg.stopSettings(), m_stopSettings );
}

class CustomFactory : public StopSettingsWidgetFactory {
public:
    virtual QString nameForSetting(int extendedSetting) const {
        if ( extendedSetting == UserSetting ) {
            return "TestSetting";
        } else {
            return StopSettingsWidgetFactory::nameForSetting( extendedSetting );
        }
    };
    virtual QString textForSetting(int extendedSetting) const {
        if ( extendedSetting == UserSetting ) {
            return "Test Setting:";
        } else {
            return StopSettingsWidgetFactory::textForSetting( extendedSetting );
        }
    };
    virtual QWidget* widgetForSetting(int extendedSetting, QWidget* parent = 0) const {
        if ( extendedSetting == UserSetting ) {
            return new QDateEdit( parent );
        } else {
            return StopSettingsWidgetFactory::widgetForSetting( extendedSetting, parent );
        }
    };
    virtual QVariant valueOfSetting(const QWidget* widget, int extendedSetting, int stopIndex = -1) const {
        if ( extendedSetting == UserSetting ) {
            return qobject_cast< const QDateEdit* >( widget )->date();
        } else {
            return StopSettingsWidgetFactory::valueOfSetting( widget, extendedSetting, stopIndex );
        }
    };
    virtual void setValueOfSetting(QWidget* widget, int extendedSetting, const QVariant& value) const {
        if ( extendedSetting == UserSetting ) {
            qobject_cast< QDateEdit* >( widget )->setDate( value.toDate() );
        } else {
            StopSettingsWidgetFactory::setValueOfSetting( widget, extendedSetting, value );
        }
    };

    typedef QSharedPointer<CustomFactory> Pointer;
};

void PublicTransportHelperTest::stopSettingsDialogCustomFactoryTest()
{
    // Create a dialog with CustomFactory and set a value for the custom date setting
    m_stopSettings.set( UserSetting, QDate(2011, 1, 4) );
    StopSettingsDialog dlg( 0, m_stopSettings,
            StopSettingsDialog::ShowStopInputField | StopSettingsDialog::ShowAlarmTimeConfig,
            AccessorInfoDialog::DefaultOptions, &m_filterConfigurations, -1,
            QList<int>() << AlarmTimeSetting << UserSetting,
            CustomFactory::Pointer::create() );

    // Test stopSettings() for the custom extended setting
    QVERIFY( dlg.stopSettings().hasSetting(UserSetting) );
    QCOMPARE( dlg.stopSettings()[UserSetting].toDate(),
              QDate(2011, 1, 4) );
    m_stopSettings = dlg.stopSettings();

    // Test factory
    StopSettingsWidgetFactory::Pointer factory = dlg.factory();
    QVERIFY( factory );

    // Test if the base widget factory correctly generates a default name,
    // if the custom factory doesn't
    QCOMPARE( factory->nameForSetting(UserSetting + 2), QLatin1String("UserSetting_102") );

    // Ensure the alarm time widget was created
    QSpinBox *spinAlarmTime = dlg.findChild<QSpinBox*>( factory->nameForSetting(AlarmTimeSetting) );
    QVERIFY( spinAlarmTime );
    // Test alarm time widget content
    QCOMPARE( spinAlarmTime->value(), m_stopSettings[AlarmTimeSetting].toInt() );

    // Ensure the custom date widget was created
    QDateEdit *dateEditCustom = dlg.findChild<QDateEdit*>( factory->nameForSetting(UserSetting) );
    QVERIFY( dateEditCustom );
    // Test custom date widget content
    QCOMPARE( dateEditCustom->date(), m_stopSettings[UserSetting].toDate() );

    // Test changing an extended setting with an associated widget in the dialog
    m_stopSettings = dlg.stopSettings();
    m_stopSettings.set( UserSetting, QDate(1966, 6, 6) );
    dlg.setStopSettings( m_stopSettings );
    QCOMPARE( dlg.stopSettings(), m_stopSettings );

    // Test if the date widgets value has been changed correctly
    QCOMPARE( dateEditCustom->date(), QDate(1966, 6, 6) );
}

void PublicTransportHelperTest::stopSettingsDialogAddWidgetsLaterCustomFactoryTest()
{
    m_stopSettings.clearSetting( AlarmTimeSetting );
    m_stopSettings.clearSetting( UserSetting );

    StopSettingsDialog dlg( 0, m_stopSettings,
            StopSettingsDialog::ShowStopInputField, AccessorInfoDialog::DefaultOptions,
            &m_filterConfigurations, -1, QList<int>(), CustomFactory::Pointer::create() );
    m_stopSettings = dlg.stopSettings();

    QVERIFY( !dlg.stopSettings().hasSetting(AlarmTimeSetting) );
    dlg.addSettingWidget( AlarmTimeSetting, 10 );
    QVERIFY( dlg.stopSettings().hasSetting(AlarmTimeSetting) );
    QCOMPARE( dlg.stopSettings()[AlarmTimeSetting].toInt(), 10 );

    QVERIFY( !dlg.stopSettings().hasSetting(UserSetting) );
    dlg.addSettingWidget( UserSetting, QDate(2010, 3, 3) );
    QVERIFY( dlg.stopSettings().hasSetting(UserSetting) );
    QCOMPARE( dlg.stopSettings()[UserSetting].toDate(), QDate(2010, 3, 3) );

    // Test stopSettings() for the custom extended setting
    QVERIFY( dlg.stopSettings().hasSetting(UserSetting) );
    QCOMPARE( dlg.stopSettings()[UserSetting].toDate(),
              QDate(2010, 3, 3) );
    m_stopSettings = dlg.stopSettings();

    // Test factory
    StopSettingsWidgetFactory::Pointer factory = dlg.factory();
    QVERIFY( factory );

    // Ensure the alarm time widget was created
    QSpinBox *spinAlarmTime = dlg.findChild<QSpinBox*>( factory->nameForSetting(AlarmTimeSetting) );
    QVERIFY( spinAlarmTime );
    // Test alarm time widget content
    QCOMPARE( spinAlarmTime->value(), m_stopSettings[AlarmTimeSetting].toInt() );

    // Ensure the custom date widget was created
    QDateEdit *dateEditCustom = dlg.findChild<QDateEdit*>( factory->nameForSetting(UserSetting) );
    QVERIFY( dateEditCustom );
    // Test custom date widget content
    QCOMPARE( dateEditCustom->date(), m_stopSettings[UserSetting].toDate() );

    // Test changing an extended setting with an associated widget in the dialog
    m_stopSettings = dlg.stopSettings();
    m_stopSettings.set( UserSetting, QDate(1966, 6, 6) );
    dlg.setStopSettings( m_stopSettings );
    QCOMPARE( dlg.stopSettings(), m_stopSettings );

    // Test if the date widgets value has been changed correctly
    QCOMPARE( dateEditCustom->date(), QDate(1966, 6, 6) );
}

void PublicTransportHelperTest::stopWidgetTest()
{
    StopWidget stopWidget( 0, m_stopSettings, StopSettingsDialog::DefaultOptions,
                           AccessorInfoDialog::DefaultOptions, &m_filterConfigurations );

//     QSignalSpy changedSpy( &stopWidget, SIGNAL(changed(StopSettings)) );
//     QSignalSpy removeSpy( &stopWidget, SIGNAL(remove()) ); TODO

    // Test set/is Highlighted
    stopWidget.setHighlighted( true );
    QCOMPARE( stopWidget.isHighlighted(), true );
    stopWidget.setHighlighted( false );
    QCOMPARE( stopWidget.isHighlighted(), false );

    // Test stopSettings
    QCOMPARE( stopWidget.stopSettings(), m_stopSettings );

    // Test setStopSettings
    m_stopSettings.setStop( Stop("Test-Stop", "654321") );
    stopWidget.setStopSettings( m_stopSettings );
    QCOMPARE( stopWidget.stopSettings(), m_stopSettings );

    // Test (set) filterConfigurations
    QCOMPARE( *stopWidget.filterConfigurations(), m_filterConfigurations );
    FilterSettings newFilterSettings;
    newFilterSettings.name = "New filter configuration";
    m_filterConfigurations << newFilterSettings;
    stopWidget.setFilterConfigurations( &m_filterConfigurations );
    QCOMPARE( *stopWidget.filterConfigurations(), m_filterConfigurations );

    // Test createStopSettingsDialog
    StopSettingsDialog *dlg = stopWidget.createStopSettingsDialog();
    StopSettings stopSettings = dlg->stopSettings();
    // TODO Documentation: Custom extended settings don't stay if there's no widget for them
//     QCOMPARE( dlg->stopSettings(), stopWidget.stopSettings() );
    QCOMPARE( stopSettings[CitySetting].toString(), m_stopSettings[CitySetting].toString() );
    QCOMPARE( stopSettings.stops(), m_stopSettings.stops() );
    QCOMPARE( stopSettings.stopList(), m_stopSettings.stopList() );
    QCOMPARE( stopSettings[ServiceProviderSetting].toString(),
              m_stopSettings[ServiceProviderSetting].toString() );
    QCOMPARE( stopSettings[LocationSetting].toString(), m_stopSettings[LocationSetting].toString() );
//     m_stopSettings = dlg.stopSettings();
}

void PublicTransportHelperTest::stopListWidgetTest()
{
    StopSettingsList list;
    list << m_stopSettings;
    StopListWidget stopListWidget( 0, list, StopSettingsDialog::DefaultOptions,
                                   AccessorInfoDialog::DefaultOptions, &m_filterConfigurations );

    QSignalSpy addedSpy( &stopListWidget, SIGNAL(added(QWidget*)) );
    QSignalSpy removedSpy( &stopListWidget, SIGNAL(removed(QWidget*,int)) );
//     QSignalSpy changedSpy( &stopListWidget, SIGNAL(changed(int,StopSettings)) );
    QCOMPARE( addedSpy.count(), 0 );
    QCOMPARE( removedSpy.count(), 0 );

    // Prevent opening StopSettingsDialogs for newly added StopSettings without given stop names
    stopListWidget.setNewStopSettingsBehaviour( StopListWidget::DoNothing );

    // Test (set) stopSettingsList
    QCOMPARE( stopListWidget.stopSettingsList(), list );
    list << StopSettings();
    stopListWidget.setStopSettingsList( list );
    QCOMPARE( stopListWidget.stopSettingsList(), list );

    QCOMPARE( removedSpy.count(), 1 ); // The single old stop was removed in setStopSettingsList
    QCOMPARE( addedSpy.count(), 2 ); // Two new stops were added in setStopSettingsList, the ones now in list
    removedSpy.clear();
    addedSpy.clear();

    // Test (set) filterConfigurations
    QCOMPARE( *stopListWidget.filterConfigurations(), m_filterConfigurations );
    FilterSettings newFilterSettings2;
    newFilterSettings2.name = "New filter configuration 2";
    m_filterConfigurations << newFilterSettings2;
    stopListWidget.setFilterConfigurations( &m_filterConfigurations );
    QCOMPARE( *stopListWidget.filterConfigurations(), m_filterConfigurations );

    // Test setWidgetCountRange / minimumWidgetCount / maximumWidgetCount
    stopListWidget.setWidgetCountRange( 1, 3 );
    QCOMPARE( stopListWidget.minimumWidgetCount(), 1 );
    QCOMPARE( stopListWidget.maximumWidgetCount(), 3 );

    QCOMPARE( stopListWidget.stopSettingsList().count(), stopListWidget.widgetCount() );
    QCOMPARE( stopListWidget.widgetCount(), 2 );

    QCOMPARE( removedSpy.count(), 0 ); // Nothing removed
    QCOMPARE( addedSpy.count(), 0 ); // Nothing added

    stopListWidget.addStopWidget();
    QCOMPARE( stopListWidget.widgetCount(), 3 );
    QCOMPARE( addedSpy.count(), 1 ); // Added 1 stop

    stopListWidget.addStopWidget(); // Should fail, because maximum widget count is 3
    QCOMPARE( stopListWidget.widgetCount(), 3 );
    QCOMPARE( addedSpy.count(), 1 ); // Still added 1 stop

    stopListWidget.removeLastStopWidget();
    QCOMPARE( stopListWidget.widgetCount(), 2 );
    QCOMPARE( removedSpy.count(), 1 ); // Removed 1 stop

    // Test if the remove signal had the right index as second argument
    QList<QVariant> args = removedSpy.first();
    QCOMPARE( args.at(1).toInt(), 2 ); // Stop widget at index 2 was removed

    stopListWidget.removeLastStopWidget();
    QCOMPARE( stopListWidget.widgetCount(), 1 );
    QCOMPARE( removedSpy.count(), 2 ); // Removed 2 stops

    stopListWidget.removeLastStopWidget(); // Should fail, because minimum widget count is 1
    QCOMPARE( stopListWidget.widgetCount(), 1 );
    QCOMPARE( removedSpy.count(), 2 ); // Still removed 2 stops

    stopListWidget.removeAllWidgets(); // Should fail, because minimum widget count is 1
    QCOMPARE( stopListWidget.widgetCount(), 1 );
    QCOMPARE( removedSpy.count(), 2 ); // Still removed 2 stops

    stopListWidget.addStopWidget();
    QCOMPARE( stopListWidget.widgetCount(), 2 );
    QCOMPARE( addedSpy.count(), 2 ); // Added 2 stops

    stopListWidget.removeAllWidgets(); // Should remove widgets until minimum widget count is reached (1)
    QCOMPARE( stopListWidget.widgetCount(), 1 );
    QCOMPARE( removedSpy.count(), 3 ); // Removed 3 stops

    stopListWidget.addStopWidget();
    QCOMPARE( stopListWidget.widgetCount(), 2 );
    QCOMPARE( addedSpy.count(), 3 ); // Added 3 stops
    stopListWidget.addStopWidget();
    QCOMPARE( stopListWidget.widgetCount(), 3 );
    QCOMPARE( addedSpy.count(), 4 ); // Added 4 stops

    // Test (set) currentStopSettingIndex
    stopListWidget.setCurrentStopSettingIndex( 2 );
    QCOMPARE( stopListWidget.currentStopSettingIndex(), 2 );
    QCOMPARE( stopListWidget.stopWidget(0)->isHighlighted(), false );
    QCOMPARE( stopListWidget.stopWidget(1)->isHighlighted(), false );
    QCOMPARE( stopListWidget.stopWidget(2)->isHighlighted(), true );

    stopListWidget.setCurrentStopSettingIndex( 1 );
    QCOMPARE( stopListWidget.currentStopSettingIndex(), 1 );
    QCOMPARE( stopListWidget.stopWidget(0)->isHighlighted(), false );
    QCOMPARE( stopListWidget.stopWidget(1)->isHighlighted(), true );
    QCOMPARE( stopListWidget.stopWidget(2)->isHighlighted(), false );
}

void PublicTransportHelperTest::locationModelTest()
{
    LocationModel model;
    QCOMPARE( model.rowCount(), 0 );

    Plasma::DataEngineManager *manager = Plasma::DataEngineManager::self();
    model.syncWithDataEngine( manager->loadEngine("publictransport") );
    QVERIFY( model.rowCount() > 0 );

    QModelIndex index = model.indexOfLocation("de");
    QVERIFY( index.isValid() );

    QCOMPARE( model.data(index, LocationCodeRole).toString(), QLatin1String("de") );

    manager->unloadEngine("publictransport");
}

QTEST_MAIN(PublicTransportHelperTest)
#include "PublicTransportHelperTest.moc"
