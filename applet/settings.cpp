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

// Own includes
#include "settings.h"
#include "departuremodel.h"
// #include "datasourcetester.h"
#include "departureprocessor.h"
#include <publictransporthelper/filterwidget.h>
#include <publictransporthelper/stopwidget.h>
#include <publictransporthelper/locationmodel.h>
#include <publictransporthelper/serviceprovidermodel.h>

// Qt includes
#include <QListWidget>
#include <QTreeView>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QMenu>
#include <QTimer>
#include <QLayout>
#include <QGraphicsLayout>
#include <QSignalMapper>
#if QT_VERSION >= 0x040600
    #include <QParallelAnimationGroup>
    #include <QPropertyAnimation>
    #include <QGraphicsEffect>
#endif

// KDE includes
#include <KTabWidget>
#include <KConfigDialog>
#include <KColorScheme>
#include <KFileDialog>
#include <KInputDialog>
#include <KStandardDirs>
#include <KMessageBox>
#include <KLineEdit>
#include <KCategorizedSortFilterProxyModel>

Settings::Settings()
{
    currentStopSettingsIndex = 0;
}

Settings::Settings( const Settings& other )
{
    *this = other;
}

SettingsUiManager::SettingsUiManager( const Settings &settings,
        Plasma::DataEngine* publicTransportEngine, Plasma::DataEngine* osmEngine,
        Plasma::DataEngine* favIconEngine, Plasma::DataEngine* geolocationEngine,
        KConfigDialog *parentDialog, DeletionPolicy deletionPolicy )
        : QObject(parentDialog), m_deletionPolicy(deletionPolicy),
// 	    m_dataSourceTester(new DataSourceTester("", publicTransportEngine, this)),
        m_configDialog(parentDialog), m_modelServiceProvider(0),
        m_modelLocations(0), m_stopListWidget(0),
        m_publicTransportEngine(publicTransportEngine), m_osmEngine(osmEngine),
        m_favIconEngine(favIconEngine), m_geolocationEngine(geolocationEngine)
{
    // Store settings that have no associated widgets
    m_currentStopSettingsIndex = settings.currentStopSettingsIndex;
    m_recentJourneySearches = settings.recentJourneySearches;
    m_showHeader = settings.showHeader;
    m_hideColumnTarget = settings.hideColumnTarget;

    m_filterSettings = settings.filterSettingsList;
    m_filterConfigChanged = false;

    m_alarmSettings = settings.alarmSettings;
    m_lastAlarm = -1;
    m_alarmsChanged = false;

    QWidget *widgetStop = new QWidget;
    QWidget *widgetAdvanced = new QWidget;
    QWidget *widgetAppearance = new QWidget;
    QWidget *widgetFilter = new QWidget;
    QWidget *widgetAlarms = new QWidget;
    m_ui.setupUi( widgetStop );
    m_uiAdvanced.setupUi( widgetAdvanced );
    m_uiAppearance.setupUi( widgetAppearance );
    m_uiFilter.setupUi( widgetFilter );
    m_uiAlarms.setupUi( widgetAlarms );

    // Setup tab widget of the stop settings page
    KTabWidget *tabMain = new KTabWidget;
    tabMain->addTab( widgetStop, i18nc( "@title:tab", "&Stop selection" ) );
    tabMain->addTab( widgetAdvanced, i18nc( "@title:tab Advanced settings tab label", "&Advanced" ) );

    // Add settings pages
    m_configDialog->addPage( tabMain, i18nc( "@title:group General settings page name", "General" ),
                            "public-transport-stop" );
    m_configDialog->addPage( widgetAppearance, i18nc( "@title:group", "Appearance" ),
                            "package_settings_looknfeel" );
    m_configDialog->addPage( widgetFilter, i18nc( "@title:group", "Filter" ), "view-filter" );
    m_configDialog->addPage( widgetAlarms, i18nc( "@title:group", "Alarms" ), "task-reminder" );

    // Setup model for the service provider combobox
    m_modelServiceProvider = new ServiceProviderModel( this );
    m_modelServiceProvider->syncWithDataEngine( m_publicTransportEngine, m_favIconEngine );

    // Setup model for the location combobox
    m_modelLocations = new LocationModel( this );
    m_modelLocations->syncWithDataEngine( m_publicTransportEngine );

    // Setup stop widgets
    // Build a new list of filter configuration names
//     QStringList trFilterConfigurationList =
//             GlobalApplet::translateFilterKeys( settings.filterSettingsList.names() );
//     foreach ( const FilterSettings &filterSettings, settings.filterSettingsList ) {
//         trFilterConfigurationList << GlobalApplet::translateFilterKey( filterSettings.name );
//     }
//     TODO REMOVE:
//     for ( QHash<QString, FilterSettings>::const_iterator it = settings.filterSettings.constBegin();
//                 it != settings.filterSettings.constEnd(); ++it ) {
//         trFilterConfigurationList << GlobalApplet::translateFilterKey( it.key() );
//     }

//  TODO    // Add used filter configuration names to stopSettingsList
//     for ( int i = 0; i < settings.stopSettingsList.count(); ++i ) {
//         QStringList filterConfigs;
//         foreach ( const FilterSettings &filterSettings, settings.filterSettingsList ) {
//             if ( filterSettings.affectedStops.contains(i) ) {
//                 filterConfigs << filterSettings.name;
//             }
//         }
//         settings.stopSettingsList[i].set( FilterConfigurationSetting, filterConfigs );
//     }

    m_stopListWidget = new StopListWidget( m_ui.stopList, settings.stopSettingsList,
            StopSettingsDialog::ExtendedStopSelection, AccessorInfoDialog::DefaultOptions,
            settings.filterSettingsList ); //trFilterConfigurationList );
    m_stopListWidget->setWhatsThis( i18nc("@info:whatsthis",
            "<subtitle>This shows the stop settings you have set.</subtitle>"
            "<para>The applet shows results for one of them at a time. To switch the "
            "currently used stop setting use the context menu of the applet.</para>"
            "<para>For each stop setting another filter configuration can be used. "
            "To edit filter configurations use the <interface>Filter</interface> "
            "section in the settings dialog. You can define a list of stops for "
            "each stop setting that are then displayed combined (eg. stops near "
            "to each other).</para>") );
    m_stopListWidget->setCurrentStopSettingIndex( m_currentStopSettingsIndex );
    connect( m_stopListWidget, SIGNAL(changed(int,StopSettings)), this, SLOT(stopSettingsChanged()) );
    connect( m_stopListWidget, SIGNAL(added(QWidget*)), this, SLOT(stopSettingsAdded()) );
    connect( m_stopListWidget, SIGNAL(removed(QWidget*,int)),
            this, SLOT(stopSettingsRemoved(QWidget*,int)) );
    stopSettingsChanged();

    // Add stop list widget
    QVBoxLayout *lStop = new QVBoxLayout( m_ui.stopList );
    lStop->setContentsMargins( 0, 0, 0, 0 );
    lStop->addWidget( m_stopListWidget );
    // TODO REMOVE:     connect( m_stopListWidget, SIGNAL(changed(int,StopSettings)),
//              this, SLOT(updateFilterInfoLabel()) );

    // Setup filter widgets
    m_uiFilter.filters->setWhatsThis( i18nc("@info:whatsthis",
            "<subtitle>This shows the filters of the selected filter configuration.</subtitle>"
            "<para>Each filter configuration consists of a filter action and a list "
            "of filters. Each filter contains a list of constraints.</para>"
            "<para>A filter matches, if all it's constraints match while a filter "
            "configuration matches, if one of it's filters match.</para>"
            "<para>To use a filter configuration select it in the <interface>Filter Uses</interface> tab. "
            "Each stop settings can use another filter configuration.</para>"
            "<para><emphasis strong='1'>Filter Types</emphasis><list>"
            "<item><emphasis>Vehicle:</emphasis> Filters by vehicle types.</item>"
            "<item><emphasis>Line String:</emphasis> Filters by transport line strings.</item>"
            "<item><emphasis>Line number:</emphasis> Filters by transport line numbers.</item>"
            "<item><emphasis>Target:</emphasis> Filters by target/origin.</item>"
            "<item><emphasis>Via:</emphasis> Filters by intermediate stops.</item>"
            "<item><emphasis>Delay:</emphasis> Filters by delay.</item>"
            "</list></para>") );
    m_uiFilter.affectedStops->setMultipleSelectionOptions( CheckCombobox::ShowStringList );
    connect( m_uiFilter.filters, SIGNAL(changed()), this, SLOT(setFilterConfigurationChanged()) );
    connect( m_uiFilter.affectedStops, SIGNAL(checkedItemsChanged()),
             this, SLOT(setFilterConfigurationChanged()) );

    // Setup alarm widgets
//     m_uiAlarms.alarmFilter->setSeparatorOptions( AbstractDynamicWidgetContainer::ShowSeparators );
//     m_uiAlarms.alarmFilter->setSeparatorText( i18n("and") );
    m_uiAlarms.alarmFilter->setWidgetCountRange();
    m_uiAlarms.alarmFilter->removeAllWidgets();
    m_uiAlarms.alarmFilter->setAllowedFilterTypes( QList<FilterType>()
            << FilterByDeparture << FilterByDayOfWeek << FilterByVehicleType
            << FilterByTarget << FilterByVia << FilterByTransportLine
            << FilterByTransportLineNumber << FilterByDelay );
    m_uiAlarms.alarmFilter->setWidgetCountRange( 1 );
    m_uiAlarms.affectedStops->setMultipleSelectionOptions( CheckCombobox::ShowStringList );
    m_uiAlarms.addAlarm->setIcon( KIcon("list-add") );
    m_uiAlarms.removeAlarm->setIcon( KIcon("list-remove") );
    connect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
             this, SLOT(currentAlarmChanged(int)) );
    connect( m_uiAlarms.alarmList, SIGNAL(itemChanged(QListWidgetItem*)),
             this, SLOT(alarmChanged(QListWidgetItem*)) );
    connect( m_uiAlarms.addAlarm, SIGNAL(clicked()), this, SLOT(addAlarmClicked()) );
    connect( m_uiAlarms.removeAlarm, SIGNAL(clicked()), this, SLOT(removeAlarmClicked()) );
    connect( m_uiAlarms.alarmFilter, SIGNAL(changed()), this, SLOT(alarmChanged()) );
    connect( m_uiAlarms.alarmType, SIGNAL(currentIndexChanged(int)),
             this, SLOT(currentAlarmTypeChanged(int)) );
    connect( m_uiAlarms.affectedStops, SIGNAL(checkedItemsChanged()),
             this, SLOT(affectedStopsChanged()) );

    // Set values of the given settings for each page
    setValuesOfAdvancedConfig( settings );
    setValuesOfAppearanceConfig( settings );
    setValuesOfAlarmConfig();
    setValuesOfFilterConfig();
// TODO REMOVE:    m_uiFilter.enableFilters->setChecked( settings.filtersEnabled );
    currentAlarmChanged( m_uiAlarms.alarmList->currentRow() );

    m_uiFilter.addFilterConfiguration->setIcon( KIcon("list-add") );
    m_uiFilter.removeFilterConfiguration->setIcon( KIcon("list-remove") );
    m_uiFilter.renameFilterConfiguration->setIcon( KIcon("edit-rename") );

    connect( m_configDialog, SIGNAL(finished()), this, SLOT(configFinished()) );
    connect( m_configDialog, SIGNAL(okClicked()), this, SLOT(configAccepted()) );

    connect( m_uiFilter.filterAction, SIGNAL(currentIndexChanged(int)),
             this, SLOT(filterActionChanged(int)) );

    connect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
             this, SLOT(loadFilterConfiguration(QString)) );
    connect( m_uiFilter.addFilterConfiguration, SIGNAL(clicked()),
             this, SLOT(addFilterConfiguration()) );
    connect( m_uiFilter.removeFilterConfiguration, SIGNAL(clicked()),
             this, SLOT(removeFilterConfiguration()) );
    connect( m_uiFilter.renameFilterConfiguration, SIGNAL(clicked()),
             this, SLOT(renameFilterConfiguration()) );
//     connect( m_dataSourceTester,
// 	     SIGNAL(testResult(DataSourceTester::TestResult,const QVariant&,const QVariant&,const QVariant&)),
// 	     this, SLOT(testResult(DataSourceTester::TestResult,const QVariant&,const QVariant&,const QVariant&)) );
}

void SettingsUiManager::configFinished()
{
    emit settingsFinished();
    if ( m_deletionPolicy == DeleteWhenFinished )
        deleteLater();
}

void SettingsUiManager::configAccepted()
{
    emit settingsAccepted( settings() );
}

void SettingsUiManager::removeAlarms( const AlarmSettingsList& /*newAlarmSettings*/,
                                    const QList<int> &/*removedAlarms*/ )
{
//     foreach ( int alarmIndex, removedAlarms ) {
// 	m_alarmSettings.removeAt( alarmIndex );
// 	disconnect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
// 		    this, SLOT(currentAlarmChanged(int)) );
// 	delete m_uiAlarms.alarmList->takeItem( m_uiAlarms.alarmList->currentRow() );
// 	connect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
// 		this, SLOT(currentAlarmChanged(int)) );
// 	m_lastAlarm = m_uiAlarms.alarmList->currentRow();
// 	setValuesOfAlarmConfig();
//     }

//     alarmChanged();
}

void SettingsUiManager::alarmChanged( QListWidgetItem *item )
{
    int row = m_uiAlarms.alarmList->row( item );
    m_alarmSettings[ row ].name = item->text();
    m_alarmSettings[ row ].enabled = item->checkState() == Qt::Checked;
    m_alarmsChanged = true; // Leave values of autoGenerated and lastFired
}

void SettingsUiManager::currentAlarmChanged( int row )
{
    if ( row == -1 ) {
        // No alarm selected
        m_uiAlarms.alarmFilter->removeAllWidgets();
        m_uiAlarms.splitter->widget( 1 )->setDisabled( true );
        m_uiAlarms.lblAlarmType->setDisabled( true );
        m_uiAlarms.alarmType->setDisabled( true );
        m_uiAlarms.affectedStops->setDisabled( true );
        m_uiAlarms.lblAffectedStops->setDisabled( true );
    } else {
        if ( m_alarmsChanged && m_lastAlarm != -1 ) {
            // Store to last edited alarm settings
            if ( m_lastAlarm < m_alarmSettings.count() ) {
                m_alarmSettings[ m_lastAlarm ] = currentAlarmSettings(
                                                    m_uiAlarms.alarmList->item( m_lastAlarm )->text() );
            } else {
                kDebug() << "m_lastAlarm is bad" << m_lastAlarm;
            }
        }

        disconnect( m_uiAlarms.alarmType, SIGNAL(currentIndexChanged(int)),
                    this, SLOT(currentAlarmTypeChanged(int)) );
        disconnect( m_uiAlarms.affectedStops, SIGNAL( checkedItemsChanged() ),
                    this, SLOT(affectedStopsChanged()) );
        setValuesOfAlarmConfig();
        connect( m_uiAlarms.alarmType, SIGNAL(currentIndexChanged(int)),
                this, SLOT(currentAlarmTypeChanged(int)) );
        connect( m_uiAlarms.affectedStops, SIGNAL(checkedItemsChanged()),
                this, SLOT(affectedStopsChanged()) );

        setAlarmTextColor( m_uiAlarms.alarmList->currentItem(),
                        m_uiAlarms.affectedStops->hasCheckedItems() );
        m_alarmsChanged = false;
        m_uiAlarms.splitter->widget( 1 )->setEnabled( true );
        m_uiAlarms.lblAlarmType->setEnabled( true );
        m_uiAlarms.alarmType->setEnabled( true );
        m_uiAlarms.affectedStops->setEnabled( true );
        m_uiAlarms.lblAffectedStops->setEnabled( true );
    }

    m_lastAlarm = row;
}

void SettingsUiManager::addAlarmClicked()
{
    // Get an unused name for the new alarm
    QString name = i18nc( "@info/plain Default name of a new alarm", "New Alarm" );
    int i = 2;
    for ( int n = 0; n < m_alarmSettings.count(); ++n ) {
        if ( m_alarmSettings[n].name == name ) {
            name = i18nc("@info/plain Default name of a new alarm, if other default "
                        "names are already used", "New Alarm %1", i);
            n = 0; // Restart loop with new name
            ++i;
        }
    }

    // Append new alarm settings
    AlarmSettings alarmSettings = AlarmSettings( name );
    m_alarmSettings << alarmSettings;

    disconnect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
                this, SLOT(currentAlarmChanged(int)) );
    QListWidgetItem *item = new QListWidgetItem( name );
    item->setFlags( item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable );
    item->setCheckState( Qt::Checked );
    setAlarmTextColor( item, !alarmSettings.affectedStops.isEmpty() );
    m_uiAlarms.alarmList->addItem( item );
    connect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
            this, SLOT(currentAlarmChanged(int)) );
    m_uiAlarms.alarmList->setCurrentRow( m_uiAlarms.alarmList->count() - 1 );
    // Start editing of the alarm name
    m_uiAlarms.alarmList->editItem( m_uiAlarms.alarmList->currentItem() );

    alarmChanged();
}

void SettingsUiManager::removeAlarmClicked()
{
    if ( m_uiAlarms.alarmList->currentRow() == -1 )
        return;

    m_alarmSettings.removeAt( m_uiAlarms.alarmList->currentRow() );
    disconnect( m_uiAlarms.alarmList, SIGNAL( currentRowChanged( int ) ),
                this, SLOT( currentAlarmChanged( int ) ) );
    delete m_uiAlarms.alarmList->takeItem( m_uiAlarms.alarmList->currentRow() );
    connect( m_uiAlarms.alarmList, SIGNAL( currentRowChanged( int ) ),
            this, SLOT( currentAlarmChanged( int ) ) );
    m_lastAlarm = m_uiAlarms.alarmList->currentRow();
    currentAlarmChanged( m_lastAlarm );

    alarmChanged();
}

void SettingsUiManager::alarmChanged()
{
    int row = m_uiAlarms.alarmList->currentRow();
    if ( row != -1 ) {
        // Reenable this alarm for all departures if changed
        m_alarmSettings[ row ].lastFired = QDateTime();

        // Changed alarms are no longer consired auto generated.
        // Only auto generated alarms can be removed using the applet's context menu
        m_alarmSettings[ row ].autoGenerated = false;
    }
    m_alarmsChanged = true;

    m_uiAlarms.removeAlarm->setDisabled( m_alarmSettings.isEmpty() );
}

void SettingsUiManager::currentAlarmTypeChanged( int index )
{
    // Make font bold if a recurring alarm is selected
    QListWidgetItem *item = m_uiAlarms.alarmList->currentItem();
    QFont font = item->font();
    font.setBold( static_cast<AlarmType>( index ) != AlarmRemoveAfterFirstMatch );
    item->setFont( font );

    alarmChanged();
}

void SettingsUiManager::affectedStopsChanged()
{
    QListWidgetItem *item = m_uiAlarms.alarmList->currentItem();
    setAlarmTextColor( item, m_uiAlarms.affectedStops->hasCheckedItems() );

    alarmChanged();
}

void SettingsUiManager::setAlarmTextColor( QListWidgetItem *item, bool hasAffectedStops ) const
{
    // Use negative text color if no affected stop is selected
    QColor color = !hasAffectedStops
            ? KColorScheme( QPalette::Active ).foreground( KColorScheme::NegativeText ).color()
            : KColorScheme( QPalette::Active ).foreground( KColorScheme::NormalText ).color();
    item->setTextColor( color );
    QPalette p = m_uiAlarms.affectedStops->palette();
    KColorScheme::adjustForeground( p,
            hasAffectedStops ? KColorScheme::NormalText : KColorScheme::NegativeText,
            QPalette::ButtonText, KColorScheme::Button );
    m_uiAlarms.affectedStops->setPalette( p );
}

void SettingsUiManager::stopSettingsAdded()
{
    StopSettings stopSettings = m_stopListWidget->stopSettingsList().last();
    QString text = stopSettings.stops().join( ", " );

    // Add " in CITY" if a city value is given
    if ( !stopSettings.get<QString>(CitySetting).isEmpty() ) {
        text += " in " + stopSettings.get<QString>(CitySetting);
    }
    m_uiFilter.affectedStops->addItem( text );
    m_uiAlarms.affectedStops->addItem( text );

    stopSettingsChanged();
}

void SettingsUiManager::stopSettingsRemoved( QWidget*, int widgetIndex )
{
    // Store current alarm settings if they are changed
    if ( m_alarmsChanged && m_uiAlarms.alarmList->currentRow() != -1 ) {
        m_alarmSettings[ m_uiAlarms.alarmList->currentRow()] = currentAlarmSettings();
    }
    
    // Adjust stop indices in alarm settings
    for ( int i = m_alarmSettings.count() - 1; i >= 0; --i ) {
        AlarmSettings &alarmSettings = m_alarmSettings[ i ];
        for ( int n = alarmSettings.affectedStops.count() - 1; n >= 0; --n ) {
            if ( alarmSettings.affectedStops[n] == widgetIndex ) {
                alarmSettings.affectedStops.removeAt( n );
            } else if ( alarmSettings.affectedStops[n] > widgetIndex ) {
                --alarmSettings.affectedStops[n];
            }
        }
    }
    
    // Adjust stop indices in filter settings
    for ( int i = m_filterSettings.count() - 1; i >= 0; --i ) {
        FilterSettings &filterSettings = m_filterSettings[ i ];
        for ( int n = filterSettings.affectedStops.count() - 1; n >= 0; --n ) {
            if ( filterSettings.affectedStops[n] == widgetIndex ) {
                filterSettings.affectedStops.removeAt( n );
            } else if ( filterSettings.affectedStops[n] > widgetIndex ) {
                --filterSettings.affectedStops[n];
            }
        }
    }

    // Get a string for each stop setting
    QStringList stopLabels;
    foreach( const StopSettings &stopSettings, m_stopListWidget->stopSettingsList() ) {
        QString text = stopSettings.stops().join( ", " );
        if ( !stopSettings.get<QString>(CitySetting).isEmpty() ) {
            text += " in " + stopSettings.get<QString>(CitySetting);
        }
        stopLabels << text;
    }

    // Update stop list in the filter settings page
    m_uiFilter.affectedStops->clear();
    m_uiFilter.affectedStops->addItems( stopLabels );
    // Get index of filter settings
    int index = -1;
    for ( int i = 0; i < m_filterSettings.count(); ++i ) {
        if ( m_filterSettings[i].name ==
             GlobalApplet::untranslateFilterKey(m_uiFilter.filterConfigurations->currentText()) )
        {
            index = i;
            break;
        }
    }
    if ( index != -1 ) {
        // TODO TEST
        m_uiFilter.affectedStops->setCheckedRows( m_filterSettings[index].affectedStops );
//                 m_filterSettings[m_uiFilter.filterConfigurations->currentText()].affectedStops );
    }
    
    // Update stop list in the alarm settings page
    m_uiAlarms.affectedStops->clear();
    m_uiAlarms.affectedStops->addItems( stopLabels );
    if ( m_uiAlarms.alarmList->currentRow() != -1 ) {
        m_uiAlarms.affectedStops->setCheckedRows(
                m_alarmSettings[m_uiAlarms.alarmList->currentRow()].affectedStops );
    }

    stopSettingsChanged();
}

void SettingsUiManager::stopSettingsChanged()
{
    StopSettingsList stopSettingsList = m_stopListWidget->stopSettingsList();

    // Update affected stops comboboxes in the filter and alarm page
    for ( int i = 0; i < stopSettingsList.count(); ++i ) {
        const StopSettings &stopSettings = stopSettingsList[ i ];
        QString text = stopSettings.stops().join( ", " );
        if ( !stopSettings.get<QString>(CitySetting).isEmpty() ) {
            text += " in " + stopSettings.get<QString>(CitySetting);
        }

        // Update for filters
        if ( i < m_uiFilter.affectedStops->count() ) {
            m_uiFilter.affectedStops->setItemText( i, text );
        } else {
            m_uiFilter.affectedStops->addItem( text );
        }

        // Update for alarms
        if ( i < m_uiAlarms.affectedStops->count() ) {
            m_uiAlarms.affectedStops->setItemText( i, text );
        } else {
            m_uiAlarms.affectedStops->addItem( text );
        }
    }

    // TODO REMOVE: Delete old "filter uses" widgets
//     if ( m_uiFilter.filterUsesArea->layout() ) {
//         QWidgetList oldWidgets = m_uiFilter.filterUsesArea->findChildren< QWidget* >(
//                                     QRegExp( "*Uses*", Qt::CaseSensitive, QRegExp::Wildcard ) );
//         foreach( QWidget *widget, oldWidgets ) {
//             delete widget;
//         }
//         delete m_uiFilter.filterUsesArea->layout();
//     }

// TODO REMOVE: Add new "filter uses" widgets
//     QGridLayout *l = new QGridLayout( m_uiFilter.filterUsesArea );
//     l->setContentsMargins( 0, 0, 0, 0 );

//     QStringList filterConfigurations = m_filterSettings.keys();
//     QStringList trFilterConfigurations;
//     foreach( const QString &filterConfig, filterConfigurations ) {
//         trFilterConfigurations << GlobalApplet::translateFilterKey( filterConfig );
//     }

// TODO REMOVE: The signal mapper is used to map signals of the used filter configuration
    // combo boxes
//     QSignalMapper *mapper = new QSignalMapper( m_uiFilter.filterUsesArea );
//     int row = 0;
//     foreach( const StopSettings &stopSettings, stopSettingsList ) {
//         QString sRow = QString::number( row );
// 
//         // Create label for the current stop settings
//         QString text = stopSettings.stops().join( ", " );
//         if ( !stopSettings.get<QString>(CitySetting).isEmpty() ) {
//             text += " in " + stopSettings.get<QString>(CitySetting);
//         }
//         QLabel *label = new QLabel( text, m_uiFilter.filterUsesArea );
//         label->setWordWrap( true );
//         label->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
//         label->setObjectName( "lblUsesStop" + sRow ); // to delete it later easily
// 
//         // Create "uses" label
//         QLabel *lblUses = new QLabel( i18nc( "@info", "<emphasis strong='1'>uses</emphasis>" ) );
//         lblUses->setAlignment( Qt::AlignCenter );
//         lblUses->setObjectName( "lblUses" + sRow ); // to delete it later easily
// 
//         // Create combo box with all available filter configurations
//         KComboBox *cmbFilterConfigs = new KComboBox( m_uiFilter.filterUsesArea );
//         cmbFilterConfigs->setObjectName( QString::number( row ) );
//         cmbFilterConfigs->addItems( trFilterConfigurations );
//         cmbFilterConfigs->setCurrentItem( GlobalApplet::translateFilterKey(
//                 stopSettings.get<QString>(FilterConfigurationSetting)) );
//         cmbFilterConfigs->setObjectName( "lblUsesConfigs" + sRow ); // This must be 14 chars, followed by an int, see usedFilterConfigChanged().
//         connect( cmbFilterConfigs, SIGNAL(currentIndexChanged(int)), mapper, SLOT(map()) );
//         mapper->setMapping( cmbFilterConfigs, cmbFilterConfigs );
// 
//         // Add items to the layout
//         l->addItem( new QSpacerItem( 0, 0, QSizePolicy::Expanding ), row, 0 );
//         l->addWidget( label, row, 1 );
//         l->addWidget( lblUses, row, 2 );
//         l->addWidget( cmbFilterConfigs, row, 3 );
//         l->addItem( new QSpacerItem( 0, 0, QSizePolicy::Expanding ), row, 4 );
//         l->setAlignment( lblUses, Qt::AlignCenter );
//         l->setAlignment( cmbFilterConfigs, Qt::AlignLeft | Qt::AlignVCenter );
//         ++row;
//     }
// 
//     connect( mapper, SIGNAL(mapped(QWidget*)), this, SLOT(usedFilterConfigChanged(QWidget*)) );
}

void SettingsUiManager::usedFilterConfigChanged( QWidget* widget )
{
    disconnect( m_stopListWidget, SIGNAL(changed(int,StopSettings)),
                this, SLOT(stopSettingsChanged()) );
    disconnect( m_stopListWidget, SIGNAL(added(QWidget*)), this, SLOT(stopSettingsAdded()) );
    disconnect( m_stopListWidget, SIGNAL(removed(QWidget*,int)),
                this, SLOT(stopSettingsRemoved(QWidget*,int)) );

    int index = widget->objectName().mid( 14 ).toInt();
    StopSettingsList stopSettingsList = m_stopListWidget->stopSettingsList();
    if ( stopSettingsList.count() > index ) {
        stopSettingsList[ index ].set( FilterConfigurationSetting,
                GlobalApplet::untranslateFilterKey(qobject_cast<KComboBox*>(widget)->currentText()) );
        m_stopListWidget->setStopSettingsList( stopSettingsList );
    }

    connect( m_stopListWidget, SIGNAL(changed(int,StopSettings)),
            this, SLOT(stopSettingsChanged()) );
    connect( m_stopListWidget, SIGNAL(added(QWidget*)), this, SLOT(stopSettingsAdded()) );
    connect( m_stopListWidget, SIGNAL(removed(QWidget*, int)),
            this, SLOT(stopSettingsRemoved(QWidget*,int)) );
    //TODO REMOVE:      updateFilterInfoLabel();
}

void SettingsUiManager::updateFilterInfoLabel()
{// TODO REMOVE: 
//     QString filterConfiguration = GlobalApplet::untranslateFilterKey(
//             m_uiFilter.filterConfigurations->currentText() );
// 
//     // Build a list of labels for stop settings used by the current filter configuration
//     QStringList usedByList;
//     foreach( const StopSettings &stopSettings, m_stopListWidget->stopSettingsList() ) {
//         if ( stopSettings.get<QString>(FilterConfigurationSetting) == filterConfiguration ) {
//             usedByList << stopSettings.stops().join( "; " );
//         }
//     }
// 
//     // Update text of the info label and set the text color
//     QPalette palette = m_uiFilter.lblInfo->palette();
//     if ( usedByList.isEmpty() ) {
//         // Filter configuration isn't used
//         m_uiFilter.lblInfo->setText( i18nc("@info Used for the filter info label in the"
//                 "filter dialog page", "Not used by any stop settings.") );
//         m_uiFilter.lblInfo->setToolTip( QString() );
//         KColorScheme::adjustForeground( palette, KColorScheme::NegativeText, QPalette::WindowText );
//     } else {
//         m_uiFilter.lblInfo->setText( i18ncp("@info Used for the filter info label in the"
//                 "filter dialog page", "Used by %1 stop setting.", "Used by %1 stop settings.",
//                 usedByList.count()) );
//         m_uiFilter.lblInfo->setToolTip( i18nc("@info:tooltip Used for the filter info label in the"
//                 "filter dialog page", "<para>Used by these stops:<list>%1</list></para>",
//                 "<item>" + usedByList.join("</item><item>") + "</item>") );
//         KColorScheme::adjustForeground( palette, KColorScheme::NormalText, QPalette::WindowText );
//     }
//     m_uiFilter.lblInfo->setPalette( palette );
}

void SettingsUiManager::setValuesOfAdvancedConfig( const Settings &settings )
{
    m_uiAdvanced.showDepartures->setChecked( settings.departureArrivalListType == DepartureList );
    m_uiAdvanced.showArrivals->setChecked( settings.departureArrivalListType == ArrivalList );
    m_uiAdvanced.updateAutomatically->setChecked( settings.autoUpdate );
    m_uiAdvanced.maximalNumberOfDepartures->setValue( settings.maximalNumberOfDepartures );
}

void SettingsUiManager::setValuesOfAppearanceConfig( const Settings &settings )
{
    m_uiAppearance.linesPerRow->setValue( settings.linesPerRow );
    m_uiAppearance.size->setValue( settings.size );
    if ( settings.showRemainingMinutes && settings.showDepartureTime ) {
        m_uiAppearance.cmbDepartureColumnInfos->setCurrentIndex( 0 );
    } else if ( settings.showRemainingMinutes ) {
        m_uiAppearance.cmbDepartureColumnInfos->setCurrentIndex( 2 );
    } else {
        m_uiAppearance.cmbDepartureColumnInfos->setCurrentIndex( 1 );
    }
    m_uiAppearance.displayTimeBold->setChecked( settings.displayTimeBold );

    m_uiAppearance.shadow->setChecked( settings.drawShadows );
    m_uiAppearance.radioUseDefaultFont->setChecked( settings.useDefaultFont );
    m_uiAppearance.radioUseOtherFont->setChecked( !settings.useDefaultFont );
    m_uiAppearance.font->setCurrentFont( settings.font );
}

void SettingsUiManager::setValuesOfAlarmConfig()
{
    kDebug() << "Set Alarm Values, in list:" << m_uiAlarms.alarmList->count()
            << "in variable:" << m_alarmSettings.count();

    disconnect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
                this, SLOT(currentAlarmChanged(int)) );
    int row = m_uiAlarms.alarmList->currentRow();
    m_uiAlarms.alarmList->clear();
    foreach( const AlarmSettings &alarmSettings, m_alarmSettings ) {
        QListWidgetItem *item = new QListWidgetItem( alarmSettings.name );
        item->setFlags( item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable );
        item->setCheckState( alarmSettings.enabled ? Qt::Checked : Qt::Unchecked );
        if ( alarmSettings.type != AlarmRemoveAfterFirstMatch ) {
            // TODO: Update this on alarm type change
            QFont font = item->font();
            font.setBold( true );
            item->setFont( font );
        }

        setAlarmTextColor( item, !alarmSettings.affectedStops.isEmpty() );
        m_uiAlarms.alarmList->addItem( item );
    }
    if ( row < m_alarmSettings.count() && row != -1 ) {
        m_uiAlarms.alarmList->setCurrentRow( row );
    } else if ( !m_alarmSettings.isEmpty() ) {
        m_uiAlarms.alarmList->setCurrentRow( row = 0 );
    }

    // Load currently selected alarm, if any
    if ( row < m_alarmSettings.count() && row != -1 ) {
        const AlarmSettings alarmSettings = m_alarmSettings.at( row );
        m_uiAlarms.alarmType->setCurrentIndex( static_cast<int>( alarmSettings.type ) );
        m_uiAlarms.affectedStops->setCheckedRows( alarmSettings.affectedStops );
        m_uiAlarms.alarmFilter->setFilter( alarmSettings.filter );
    }
    m_uiAlarms.removeAlarm->setDisabled( m_alarmSettings.isEmpty() );

    connect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
            this, SLOT(currentAlarmChanged(int)) );
}

void SettingsUiManager::setValuesOfFilterConfig()
{
    if ( m_uiFilter.filterConfigurations->currentIndex() == -1 ) {
        m_uiFilter.filterConfigurations->setCurrentIndex( 0 );
    }

    // Build list of filter configuration names
    QStringList filterConfigs;
//     filterConfigs << i18n("No Filter");
//     for ( QHash<QString, FilterSettings>::const_iterator it = m_filterSettings.constBegin();
//           it != m_filterSettings.constEnd(); ++it )
//     {
//      filterConfigs << GlobalApplet::translateFilterKey( it.key() );
    foreach ( const FilterSettings &filterSettings, m_filterSettings ) {
        filterConfigs << GlobalApplet::translateFilterKey( filterSettings.name );
    }

    // Store selected filter configuration
    QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();

    // Clear the list of filter configurations and add the new ones.
    // The currentIndexChanged signal is disconnected meanwhile,
    // because the filter configuration doesn't need to be reloaded.
    disconnect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
                this, SLOT(loadFilterConfiguration(QString)) );
    m_uiFilter.filterConfigurations->clear();
    m_uiFilter.filterConfigurations->addItems( filterConfigs );
    if ( trFilterConfiguration.isEmpty() ) {
        m_uiFilter.filterConfigurations->setCurrentIndex( 0 );
    } else {
        m_uiFilter.filterConfigurations->setCurrentItem( trFilterConfiguration );
    }
    connect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
             this, SLOT(loadFilterConfiguration(QString)) );

    if ( trFilterConfiguration.isEmpty() ) {
        kDebug() << "No Item Selected";
        trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
    }
//     Q_ASSERT_X( !trFilterConfiguration.isEmpty(), "SettingsUiManager::setValuesOfFilterConfig",
//                 "No filter configuration" );

    //  TODO REMOVE:    updateFilterInfoLabel();
    stopSettingsChanged();

    if ( trFilterConfiguration.isEmpty() ) {
        m_uiFilter.lblAffectedStops->setDisabled( true );
        m_uiFilter.affectedStops->setDisabled( true );
        m_uiFilter.lblFilterAction->setDisabled( true );
        m_uiFilter.filterAction->setDisabled( true );
        m_uiFilter.grpFilterCriteria->setDisabled( true );
    } else {
        m_uiFilter.lblAffectedStops->setEnabled( true );
        m_uiFilter.affectedStops->setEnabled( true );
        m_uiFilter.lblFilterAction->setEnabled( true );
        m_uiFilter.filterAction->setEnabled( true );
        m_uiFilter.grpFilterCriteria->setEnabled( true );
        
        QString filterConfiguration = GlobalApplet::untranslateFilterKey( trFilterConfiguration );
    // TODO    bool isDefaultFilterConfig = filterConfiguration == "Default";
    //     m_uiFilter.removeFilterConfiguration->setDisabled( isDefaultFilterConfig );
    //     m_uiFilter.renameFilterConfiguration->setDisabled( isDefaultFilterConfig );

        FilterSettings filterSettings = m_filterSettings.byName( filterConfiguration );
        m_uiFilter.filterAction->setCurrentIndex(
                static_cast<int>( filterSettings.filterAction ) );
        filterActionChanged( m_uiFilter.filterAction->currentIndex() );

        // Set affected stops TODO DOESNT WORK CURRENTLY
        kDebug() << "affectedStops:" << filterSettings.affectedStops;
        m_uiFilter.affectedStops->setCheckedRows( filterSettings.affectedStops );

        // Clear old filter widgets
        int minWidgetCount = m_uiFilter.filters->minimumWidgetCount();
        int maxWidgetCount = m_uiFilter.filters->maximumWidgetCount();
        m_uiFilter.filters->setWidgetCountRange();
        m_uiFilter.filters->removeAllWidgets();

        // Setup FilterWidgets from m_filters
        foreach( const Filter &filter, filterSettings.filters ) {
            m_uiFilter.filters->addFilter( filter );
        }

        int added = m_uiFilter.filters->setWidgetCountRange( minWidgetCount, maxWidgetCount );
        setFilterConfigurationChanged( added != 0 );
    }
}

Settings SettingsUiManager::settings()
{
    Settings ret;

    ret.stopSettingsList = m_stopListWidget->stopSettingsList();
    for ( int i = 0; i < ret.stopSettingsList.count(); ++i ) {
        StopSettings stopSettings = ret.stopSettingsList.at( i );
        stopSettings.set( FilterConfigurationSetting,
                GlobalApplet::untranslateFilterKey(stopSettings.get<QString>(FilterConfigurationSetting)) );
    }

    // Set stored "no-Gui" settings
    ret.recentJourneySearches = m_recentJourneySearches;
    ret.currentStopSettingsIndex = m_currentStopSettingsIndex;
    if ( ret.currentStopSettingsIndex >= ret.stopSettingsList.count() ) {
        ret.currentStopSettingsIndex = ret.stopSettingsList.count() - 1;
    }
    ret.showHeader = m_showHeader;
    ret.hideColumnTarget = m_hideColumnTarget;

    if ( m_filterConfigChanged ) {
//         QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
//         QString filterConfiguration = GlobalApplet::untranslateFilterKey( trFilterConfiguration );
//         m_filterSettings[ filterConfiguration ] = currentFilterSettings();
        m_filterSettings.set( currentFilterSettings() );
    }
    ret.filterSettingsList = m_filterSettings;
// TODO REMOVE:    ret.filtersEnabled = m_uiFilter.enableFilters->isChecked();

    if ( m_alarmsChanged && m_uiAlarms.alarmList->currentRow() != -1 ) {
        m_alarmSettings[ m_uiAlarms.alarmList->currentRow()] = currentAlarmSettings();
    }
    ret.alarmSettings = m_alarmSettings;

    if ( m_uiAdvanced.showArrivals->isChecked() ) {
        ret.departureArrivalListType = ArrivalList;
    } else {
        ret.departureArrivalListType = DepartureList;
    }
    ret.autoUpdate = m_uiAdvanced.updateAutomatically->isChecked();
    ret.maximalNumberOfDepartures = m_uiAdvanced.maximalNumberOfDepartures->value();

    ret.showRemainingMinutes = m_uiAppearance.cmbDepartureColumnInfos->currentIndex() != 1;
    ret.showDepartureTime = m_uiAppearance.cmbDepartureColumnInfos->currentIndex() <= 1;
    ret.displayTimeBold = m_uiAppearance.displayTimeBold->checkState() == Qt::Checked;
    ret.drawShadows = m_uiAppearance.shadow->checkState() == Qt::Checked;
    ret.linesPerRow = m_uiAppearance.linesPerRow->value();
    ret.size = m_uiAppearance.size->value();
    ret.sizeFactor = ( ret.size + 3 ) * 0.2f;
    ret.useDefaultFont = m_uiAppearance.radioUseDefaultFont->isChecked();
    if ( ret.useDefaultFont ) {
        ret.font = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
    } else {
        ret.font.setFamily( m_uiAppearance.font->currentFont().family() );
    }

    return ret;
}

FilterSettings SettingsUiManager::currentFilterSettings() const
{
    FilterSettings filterSettings;
    filterSettings.filterAction = static_cast< FilterAction >( m_uiFilter.filterAction->currentIndex() );
    filterSettings.affectedStops = m_uiFilter.affectedStops->checkedRows();
    filterSettings.filters = m_uiFilter.filters->filters();
    filterSettings.name = m_uiFilter.filterConfigurations->currentText();
    return filterSettings;
}

AlarmSettings SettingsUiManager::currentAlarmSettings( const QString &name ) const
{
    Q_ASSERT( m_uiAlarms.alarmList->currentRow() != -1 );

    AlarmSettings alarmSettings;
    int row = m_uiAlarms.alarmList->currentRow();
    if ( row >= 0 && row < m_alarmSettings.count() ) {
        alarmSettings = m_alarmSettings[ row ];
    } else {
        kDebug() << "No existing alarm settings found for the current alarm" << name;
    }
    alarmSettings.enabled = m_uiAlarms.alarmList->currentItem()->checkState() == Qt::Checked;
    alarmSettings.name = name.isNull() ? m_uiAlarms.alarmList->currentItem()->text() : name;
    alarmSettings.affectedStops = m_uiAlarms.affectedStops->checkedRows();
    alarmSettings.type = static_cast<AlarmType>( m_uiAlarms.alarmType->currentIndex() );
    alarmSettings.filter = m_uiAlarms.alarmFilter->filter();
    return alarmSettings;
}

void SettingsUiManager::loadFilterConfiguration( const QString& filterConfig )
{
    if ( filterConfig.isEmpty() ) {
        return;
    }

    QString untrFilterConfig = GlobalApplet::untranslateFilterKey( filterConfig );
    if ( untrFilterConfig == m_lastFilterConfiguration ) {
        return; // Selected the same filter configuration again
    }

    if ( m_filterConfigChanged && !m_lastFilterConfiguration.isEmpty() ) {
        // Store to previously selected filter configuration
        FilterSettings filterSettings = currentFilterSettings();
        filterSettings.name = m_lastFilterConfiguration;
        m_filterSettings.set( filterSettings );
    }

    m_lastFilterConfiguration = untrFilterConfig;

    setFilterConfigurationChanged( false );
    setValuesOfFilterConfig();
}

void SettingsUiManager::updateFilterConfigurationLists()
{
    // Build a new list of filter configuration names
//     QStringList trFilterConfigurationList = GlobalApplet::translateFilterKeys( m_filterSettings.names() );
//     for ( QHash<QString, FilterSettings>::const_iterator it = m_filterSettings.constBegin();
//                 it != m_filterSettings.constEnd(); ++it ) {
//     foreach ( const FilterSettings &filterSettings, m_filterSettings ) {
//         trFilterConfigurationList << GlobalApplet::translateFilterKey( filterSettings.name );
//     }

    // Update list of filter configuration names in the stop list widget
//     TEST
    m_stopListWidget->setFilterConfigurations( m_filterSettings ); // trFilterConfigurationList );

    // TODO Not needed any longer?
    // Rebuild "Filter Uses" tab in filter page with the new filter configuration names
    stopSettingsChanged();
}

void SettingsUiManager::addFilterConfiguration()
{
    // Get an unused filter configuration name
    QString newFilterConfig = i18nc("@info/plain Default name of a new filter configuration",
                                    "New Configuration");
    int i = 2;
    while ( m_filterSettings.hasName(newFilterConfig) ) {
        newFilterConfig = i18nc("@info/plain Default name of a new filter configuration "
                                "if the other default names are already used",
                                "New Configuration %1", i);
        ++i;
    }

    bool ok;
    do {
        newFilterConfig = KInputDialog::getText( i18nc("@title:window", "Choose a Name"),
                i18nc("@label:textbox", "Name of the new Filter Configuration:"), newFilterConfig,
                &ok, m_configDialog, new QRegExpValidator(QRegExp("[^\\*]*"), this) );
        if ( !ok || newFilterConfig.isNull() ) {
            return; // Canceled
        }
        if ( m_filterSettings.hasName(newFilterConfig) ) {
            KMessageBox::information( m_configDialog, i18nc("@info/plain",
                    "There is already a filter configuration with the name <resource>%1</resource>. "
                    "Please choose another one.", newFilterConfig) );
        } else {
            // Got a valid name, done with asking for a name
            break;
        }
    } while ( true );

    // Append new filter settings
    FilterSettings filterSettings;
    filterSettings.name = GlobalApplet::untranslateFilterKey( newFilterConfig );
    m_filterSettings << filterSettings;
    m_uiFilter.filterConfigurations->setCurrentItem( newFilterConfig, true,
            m_uiFilter.filterConfigurations->count() );

    // Update widgets containing a list of filter configuration names
    updateFilterConfigurationLists();
}

void SettingsUiManager::removeFilterConfiguration()
{
    int index = m_uiFilter.filterConfigurations->currentIndex();
    if ( index == -1 ) {
        kDebug() << "No selection";
        return;
    }

    // Show a warning
    QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
    if ( KMessageBox::warningContinueCancel(m_configDialog,
            i18nc("@info", "<warning>This will permanently delete the selected filter "
                "configuration <resource>%1</resource>.</warning>",
                trFilterConfiguration)) != KMessageBox::Continue ) {
        return; // Cancel clicked
    }

    // Remove filter configuration from the filter settings list
    QString filterConfiguration = GlobalApplet::untranslateFilterKey( trFilterConfiguration );
    kDebug() << "COUNT A:" << m_filterSettings.count();
    m_filterSettings.removeByName( filterConfiguration );
    kDebug() << "COUNT B:" << m_filterSettings.count();

    // Remove filter configuration from the UI filter list
    m_uiFilter.filterConfigurations->removeItem( index );

    // Select default filter configuration
    if ( index >= m_uiFilter.filterConfigurations->count() ) {
        index = m_uiFilter.filterConfigurations->count() - 1;
    }
    if ( index != -1 ) {
        m_uiFilter.filterConfigurations->setCurrentIndex( index );
    } else {
        setValuesOfFilterConfig();
    }

    // Update widgets containing a list of filter configuration names
    updateFilterConfigurationLists();
}

void SettingsUiManager::renameFilterConfiguration()
{
    QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
    bool ok;
    QString newFilterConfig = KInputDialog::getText( i18nc("@title:window", "Choose a Name"),
            i18nc("@label:textbox", "New Name of the Filter Configuration:"), trFilterConfiguration,
            &ok, m_configDialog, new QRegExpValidator(QRegExp("[^\\*]*"), this) );
    if ( !ok || newFilterConfig.isNull() ) {
        return; // Canceled
    }

    // Get key name of the current filter configuration
    QString filterConfiguration = GlobalApplet::untranslateFilterKey( trFilterConfiguration );
    if ( newFilterConfig == trFilterConfiguration ) {
        return; // Not changed, but the old name was accepted
    }

    // Check if the new name is valid.
    // '*' is also not allowed in the name but that's already validated by a QRegExpValidator.
    if ( newFilterConfig.isEmpty() ) {
        KMessageBox::information( m_configDialog, i18nc( "@info", "Empty names are not allowed." ) );
        return;
    }

    // Check if the new name is already used and ask if it should be overwritten
    QString untrNewFilterConfig = GlobalApplet::untranslateFilterKey( newFilterConfig );
    if ( m_filterSettings.hasName(untrNewFilterConfig)
        && KMessageBox::warningYesNo(m_configDialog,
            i18nc("@info", "<warning>There is already a filter configuration with the name "
                  "<resource>%1</resource>.</warning><nl/>Do you want to overwrite it?",
                  newFilterConfig))
        != KMessageBox::Yes )
    {
        return; // No (don't overwrite) pressed
    }

    // Remove the filter configuration from the old key name
    // and add it with the new key name
    FilterSettings filterSettings = m_filterSettings.byName( filterConfiguration );
    m_filterSettings.removeByName( filterConfiguration );
    filterSettings.name = untrNewFilterConfig;
    m_filterSettings.set( filterSettings );

    // Remove old name from the list of filter configurations and add the new one.
    // The currentIndexChanged signal is disconnected while changing the name,
    // because the filter configuration doesn't need to be reloaded.
    disconnect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
                this, SLOT(loadFilterConfiguration(QString)) );
    int index = m_uiFilter.filterConfigurations->currentIndex();
    if ( index == -1 ) {
        kDebug() << "Removed filter config not found in list" << trFilterConfiguration;
    } else {
        m_uiFilter.filterConfigurations->removeItem( index );
    }
    m_uiFilter.filterConfigurations->setCurrentItem( newFilterConfig, true );
    m_lastFilterConfiguration = untrNewFilterConfig;
    connect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(loadFilterConfiguration(QString)) );

    // Update filter configuration name in stop settings
    StopSettingsList stopSettingsList = m_stopListWidget->stopSettingsList();
    for ( int i = 0; i < stopSettingsList.count(); ++i ) {
        if ( stopSettingsList[i].get<QString>(FilterConfigurationSetting) == filterConfiguration ) {
            stopSettingsList[i].set( FilterConfigurationSetting, untrNewFilterConfig );
        }
    }
    m_stopListWidget->setStopSettingsList( stopSettingsList );

    // Update widgets containing a list of filter configuration names
    updateFilterConfigurationLists();
}

void SettingsUiManager::filterActionChanged( int index )
{
    FilterAction filterAction = static_cast< FilterAction >( index );
//     bool enableFilters = filterAction != ShowAll;
//     m_uiFilter.filters->setEnabled( enableFilters );

    // Store to last edited filter settings
    QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
    QString filterConfiguration = GlobalApplet::untranslateFilterKey( trFilterConfiguration );
    FilterSettings filterSettings = m_filterSettings.byName( filterConfiguration );
    filterSettings.filterAction = filterAction;
    m_filterSettings.set( filterSettings );

    setFilterConfigurationChanged();
}

void SettingsUiManager::setFilterConfigurationChanged( bool changed )
{
//     TODO REMOVE:
//     if ( m_filterConfigChanged == changed ) {
//         return;
//     }

//     QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
//     QString filterConfiguration = GlobalApplet::untranslateFilterKey( trFilterConfiguration );
//     bool defaultFilterConfig = filterConfiguration == "Default";
//     m_uiFilter.removeFilterConfiguration->setDisabled( defaultFilterConfig );
//     m_uiFilter.renameFilterConfiguration->setDisabled( defaultFilterConfig );

    m_filterConfigChanged = changed;
}

int SettingsUiManager::filterConfigurationIndex( const QString& filterConfig )
{
    int index = m_uiFilter.filterConfigurations->findText( filterConfig );
    if ( index == -1 ) {
        kDebug() << "Item" << filterConfig << "not found!";
    }

    return index;
}

void SettingsUiManager::exportFilterSettings()
{
    QString fileName = KFileDialog::getSaveFileName(
            KUrl("kfiledialog:///filterSettings"), QString(), m_configDialog,
            i18nc("@title:window", "Export Filter Settings") );
    if ( fileName.isEmpty() ) {
        return;
    }

    KConfig config( fileName, KConfig::SimpleConfig );
    SettingsIO::writeFilterConfig( currentFilterSettings(), config.group( QString() ) );
}

void SettingsUiManager::importFilterSettings()
{
    QString fileName = KFileDialog::getOpenFileName(
            KUrl("kfiledialog:///filterSettings"), QString(), m_configDialog,
            i18nc("@title:window", "Import Filter Settings") );
    if ( fileName.isEmpty() ) {
        return;
    }

    KConfig config( fileName, KConfig::SimpleConfig );
    FilterSettings filterSettings = SettingsIO::readFilterConfig( config.group( QString() ) );
//     TODO: Set filterSettings in GUI
}

Settings SettingsIO::readSettings( KConfigGroup cg, KConfigGroup cgGlobal,
                                   Plasma::DataEngine *publictransportEngine )
{
    Settings settings;
    settings.autoUpdate = cg.readEntry( "autoUpdate", true );
    settings.showRemainingMinutes = cg.readEntry( "showRemainingMinutes", true );
    settings.showDepartureTime = cg.readEntry( "showDepartureTime", true );
    settings.displayTimeBold = cg.readEntry( "displayTimeBold", true );

    // Read stop settings TODO: Store in config groups like filters
    settings.stopSettingsList.clear();
    int stopSettingCount = cgGlobal.readEntry( "stopSettings", 1 );
    QString test = "location";
    int i = 1;
    while ( cgGlobal.hasKey(test) ) {
        StopSettings stopSettings;
        QString suffix = i == 1 ? QString() : '_' + QString::number( i );
        stopSettings.set( LocationSetting, cgGlobal.readEntry("location" + suffix, "showAll") );
        stopSettings.set( ServiceProviderSetting,
                          cgGlobal.readEntry("serviceProvider" + suffix, "de_db") );
//         TODO REMOVE
//         stopSettings.set( FilterConfigurationSetting,
//                           cgGlobal.readEntry("filterConfiguration" + suffix, "Default") );
        stopSettings.set( CitySetting, cgGlobal.readEntry("city" + suffix, QString()) );
        stopSettings.setStops( cgGlobal.readEntry("stop" + suffix, QStringList()),
                              cgGlobal.readEntry("stopID" + suffix, QStringList()) );
        stopSettings.set( TimeOffsetOfFirstDepartureSetting,
                          cgGlobal.readEntry("timeOffsetOfFirstDeparture" + suffix, 0) );
        stopSettings.set( TimeOfFirstDepartureSetting,
                          QTime::fromString(cgGlobal.readEntry("timeOfFirstDepartureCustom" + suffix,
                                                               "12:00"), "hh:mm") );
        stopSettings.set( FirstDepartureConfigModeSetting,
                          cgGlobal.readEntry("firstDepartureConfigMode" + suffix,
                                             static_cast<int>(RelativeToCurrentTime)) );
        stopSettings.set( AlarmTimeSetting, cgGlobal.readEntry("alarmTime" + suffix, 5) );
        settings.stopSettingsList << stopSettings;

        ++i;
        test = "location_" + QString::number( i );
        if ( i > stopSettingCount ) {
            break;
        }
    }

    settings.currentStopSettingsIndex = cg.readEntry( "currentStopIndex", 0 );

    // Add initial stop settings when no settings are available
    if ( settings.stopSettingsList.isEmpty() ) {
        kDebug() << "Stop settings list in settings is empty";
        if ( publictransportEngine ) {
            QString countryCode = KGlobal::locale()->country();
            Plasma::DataEngine::Data locationData = publictransportEngine->query( "Locations" );
            QString defaultServiceProviderId =
                    locationData[countryCode].toHash()["defaultAccessor"].toString();

            StopSettings stopSettings;
            if ( defaultServiceProviderId.isEmpty() ) {
                stopSettings.set( LocationSetting, "showAll" );
            } else {
                stopSettings.set( LocationSetting, countryCode );
                stopSettings.set( ServiceProviderSetting, defaultServiceProviderId );
            }
            stopSettings.setStop( QString() );
            // TODO: Get initial stop names using StopFinder

            settings.stopSettingsList << stopSettings;
        } else {
            settings.stopSettingsList << StopSettings();
        }

    }

    if ( settings.currentStopSettingsIndex < 0 ) {
        settings.currentStopSettingsIndex = 0; // For compatibility with versions < 0.7
    } else if ( settings.currentStopSettingsIndex >= settings.stopSettingsList.count() ) {
        kDebug() << "Current stop index in settings invalid";
        settings.currentStopSettingsIndex = settings.stopSettingsList.count() - 1;
    }

    settings.recentJourneySearches = cgGlobal.readEntry( "recentJourneySearches", QStringList() );

    settings.maximalNumberOfDepartures = cg.readEntry( "maximalNumberOfDepartures", 20 );
    settings.linesPerRow = cg.readEntry( "linesPerRow", 2 );
    settings.size = cg.readEntry( "size", 2 );
    settings.sizeFactor = ( settings.size + 3 ) * 0.2f;
    settings.departureArrivalListType = static_cast<DepartureArrivalListType>(
            cg.readEntry("departureArrivalListType", static_cast<int>(DepartureList)) );
    settings.drawShadows = cg.readEntry( "drawShadows", true );
    settings.showHeader = cg.readEntry( "showHeader", true );
    settings.hideColumnTarget = cg.readEntry( "hideColumnTarget", false );

    QString fontFamily = cg.readEntry( "fontFamily", QString() );
    settings.useDefaultFont = fontFamily.isEmpty();
    if ( !settings.useDefaultFont ) {
        settings.font = QFont( fontFamily );
    } else {
        settings.font = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
    }

    // ***************** DEPRECATED BEGIN *****************************************************
    // *** Used for migration of filter settings from versions prior to version 0.10 Beta 9 ***
    if ( cgGlobal.hasKey("filterConfigurationList") ) {
        QStringList filterConfigurationList =
                cgGlobal.readEntry( "filterConfigurationList", QStringList() );
        for ( int i = filterConfigurationList.count() - 1; i >= 0; --i ) {
            const QString &filterConfiguration = filterConfigurationList[ i ];
            if ( filterConfiguration.isEmpty() ) {
                filterConfigurationList.removeAt( i );
            }
        }

        // Make sure, "Default" is the first
//         filterConfigurationList.removeOne( "Default" );
//         filterConfigurationList.prepend( "Default" );

        kDebug() << "Config group list" << cgGlobal.groupList();
        kDebug() << "Filter config list:" << filterConfigurationList;
        // Delete old filter configs
        foreach( const QString &group, cgGlobal.groupList() ) {
            if ( !filterConfigurationList.contains(
                        group.mid( QString( "filterConfig_" ).length() ) ) ) {
                kDebug() << "Delete old group" << group;
                cgGlobal.deleteGroup( group );
            }
        }

        settings.filterSettingsList.clear();
        foreach( const QString &filterConfiguration, filterConfigurationList ) {
            FilterSettings filterSettings =
                    readFilterConfig( cgGlobal.group("filterConfig_" + filterConfiguration) );
            filterSettings.name = filterConfiguration;
            settings.filterSettingsList << filterSettings;
        }
    // ***************** DEPRECATED END *******************************************************
    } else {
        // New filter storage structure
        int filterCount = cgGlobal.readEntry( "filterCount", 0 );
        test = "filterConfig_1";
        i = 1;
        kDebug() << "Filter count is" << filterCount;
        while ( i <= filterCount && cgGlobal.hasGroup(test) ) {
            FilterSettings filterSettings = readFilterConfig( cgGlobal.group(test) );
            if ( filterSettings.name.isEmpty() ) {
                kDebug() << "Filter settings without a name found!";
                filterSettings.name = "Unnamed" + QString::number( i );
            }
            settings.filterSettingsList << filterSettings;

            ++i;
            test = "filterConfig_" + QString::number( i );
        }
    }

    // Read alarm settings TODO: Store in config groups like filters
    int alarmCount = cgGlobal.readEntry( "alarmCount", 0 );
    test = "alarmType";
    i = 1;
    kDebug() << "Alarm count is" << alarmCount;
    while ( i <= alarmCount && cgGlobal.hasKey(test) ) {
        AlarmSettings alarmSettings;
        QString suffix = i == 1 ? QString() : '_' + QString::number( i );
        alarmSettings.type = static_cast<AlarmType>(
                cgGlobal.readEntry("alarmType" + suffix, static_cast<int>(AlarmRemoveAfterFirstMatch)) );
        alarmSettings.affectedStops = cgGlobal.readEntry( "alarmStops" + suffix, QList<int>() );
        alarmSettings.enabled = cgGlobal.readEntry( "alarmEnabled" + suffix, true );
        alarmSettings.name = cgGlobal.readEntry( "alarmName" + suffix, "Unnamed" );
        alarmSettings.lastFired = cgGlobal.readEntry( "alarmLastFired" + suffix, QDateTime() );
        alarmSettings.autoGenerated = cgGlobal.readEntry( "alarmAutogenerated" + suffix, false );
        QByteArray baAlarmFilter = cgGlobal.readEntry( "alarmFilter" + suffix, QByteArray() );
        alarmSettings.filter.fromData( baAlarmFilter );
        settings.alarmSettings << alarmSettings;

        ++i;
        test = "alarmType_" + QString::number( i );
    }

    return settings;
}

SettingsIO::ChangedFlags SettingsIO::writeSettings( const Settings &settings,
        const Settings &oldSettings, KConfigGroup cg, KConfigGroup cgGlobal )
{
    ChangedFlags changed = NothingChanged;

    if ( settings.currentStopSettingsIndex != oldSettings.currentStopSettingsIndex ) {
        cg.writeEntry( "currentStopIndex", settings.currentStopSettingsIndex );
        changed |= IsChanged | ChangedCurrentStop;
    }
    if ( settings.recentJourneySearches != oldSettings.recentJourneySearches ) {
        cgGlobal.writeEntry( "recentJourneySearches", settings.recentJourneySearches );
        changed |= IsChanged | ChangedRecentJourneySearches;
    }

    // Write stop settings
    if ( settings.stopSettingsList != oldSettings.stopSettingsList ) {
        kDebug() << "Stop settings changed";

        changed |= IsChanged | ChangedStopSettings;
        int i = 1;
        cgGlobal.writeEntry( "stopSettings", settings.stopSettingsList.count() ); // Not needed if deleteEntry/Group works, don't know what's wrong (sync() and Plasma::Applet::configNeedsSaving() doesn't help)

        foreach( const StopSettings &stopSettings, settings.stopSettingsList ) {
            QString suffix = i == 1 ? QString() : '_' + QString::number( i );
            cgGlobal.writeEntry( "location" + suffix,
                                stopSettings.get<QString>(LocationSetting) );
            cgGlobal.writeEntry( "serviceProvider" + suffix,
                                stopSettings.get<QString>(ServiceProviderSetting) );
            cgGlobal.writeEntry( "city" + suffix, stopSettings.get<QString>(CitySetting) );
            cgGlobal.writeEntry( "stop" + suffix, stopSettings.stops() );
            cgGlobal.writeEntry( "stopID" + suffix, stopSettings.stopIDs() );

//             TODO REMOVE:
//             cgGlobal.writeEntry( "filterConfiguration" + suffix,
//                     stopSettings.get<QString>(FilterConfigurationSetting) );
            cgGlobal.writeEntry( "timeOffsetOfFirstDeparture" + suffix,
                    stopSettings.get<int>(TimeOffsetOfFirstDepartureSetting) );
            cgGlobal.writeEntry( "timeOfFirstDepartureCustom" + suffix,
                    stopSettings.get<QTime>(TimeOfFirstDepartureSetting).toString("hh:mm") );
            cgGlobal.writeEntry( "firstDepartureConfigMode" + suffix,
                    stopSettings.get<int>(FirstDepartureConfigModeSetting) );
            cgGlobal.writeEntry( "alarmTime" + suffix, stopSettings.get<int>(AlarmTimeSetting) );
            ++i;
        }

        // Delete old stop settings entries
        QString test = "location_" + QString::number( i );
        while ( cgGlobal.hasKey( test ) ) {
            QString suffix = '_' + QString::number( i );
            cgGlobal.deleteEntry( "location" + suffix );
            cgGlobal.deleteEntry( "serviceProvider" + suffix );
            cgGlobal.deleteEntry( "city" + suffix );
            cgGlobal.deleteEntry( "stop" + suffix );
            cgGlobal.deleteEntry( "stopID" + suffix );
//             cgGlobal.deleteEntry( "filterConfiguration" + suffix ); TODO REMOVE
            cgGlobal.deleteEntry( "timeOffsetOfFirstDeparture" + suffix );
            cgGlobal.deleteEntry( "timeOfFirstDepartureCustom" + suffix );
            cgGlobal.deleteEntry( "firstDepartureConfigMode" + suffix );
            cgGlobal.deleteEntry( "alarmTime" + suffix );
            ++i;
            test = "location_" + QString::number( i );
        }
    }

    if ( settings.autoUpdate != oldSettings.autoUpdate ) {
        cg.writeEntry( "autoUpdate", settings.autoUpdate );
        changed |= IsChanged;
    }

    if ( settings.showRemainingMinutes != oldSettings.showRemainingMinutes ) {
        cg.writeEntry( "showRemainingMinutes", settings.showRemainingMinutes );
        changed |= IsChanged;
    }

    if ( settings.showDepartureTime != oldSettings.showDepartureTime ) {
        cg.writeEntry( "showDepartureTime", settings.showDepartureTime );
        changed |= IsChanged;
    }

    if ( settings.displayTimeBold != oldSettings.displayTimeBold ) {
        cg.writeEntry( "displayTimeBold", settings.displayTimeBold );
        changed |= IsChanged;
    }

    if ( settings.drawShadows != oldSettings.drawShadows ) {
        cg.writeEntry( "drawShadows", settings.drawShadows );
        changed |= IsChanged;
    }

    if ( settings.showHeader != oldSettings.showHeader ) {
        cg.writeEntry( "showHeader", settings.showHeader );
        changed |= IsChanged;
    }

    if ( settings.hideColumnTarget != oldSettings.hideColumnTarget ) {
        cg.writeEntry( "hideColumnTarget", settings.hideColumnTarget );
        changed |= IsChanged;
    }

    if ( settings.maximalNumberOfDepartures != oldSettings.maximalNumberOfDepartures ) {
        cg.writeEntry( "maximalNumberOfDepartures", settings.maximalNumberOfDepartures );
        changed |= IsChanged | ChangedServiceProvider;
    }

    if ( settings.linesPerRow != oldSettings.linesPerRow ) {
        cg.writeEntry( "linesPerRow", settings.linesPerRow );
        changed |= IsChanged | ChangedLinesPerRow;
    }

    if ( settings.size != oldSettings.size ) {
        cg.writeEntry( "size", settings.size );
        changed |= IsChanged;
    }

    if ( settings.useDefaultFont != oldSettings.useDefaultFont
            || ( !settings.useDefaultFont && settings.font != oldSettings.font ) ) {
        cg.writeEntry( "fontFamily", settings.useDefaultFont
                    ? QString() : settings.font.family() );
        changed |= IsChanged;
    }

    if ( settings.departureArrivalListType != oldSettings.departureArrivalListType ) {
        cg.writeEntry( "departureArrivalListType",
                    static_cast<int>( settings.departureArrivalListType ) );
        changed |= IsChanged | ChangedServiceProvider | ChangedDepartureArrivalListType;
    }

    // Write filter settings
// TODO REMOVE: (DEPRECATED since 0.10 Beta 9)
//     if ( settings.filterSettingsList.names() != oldSettings.filterSettingsList.names() ) {
//         cgGlobal.writeEntry( "filterConfigurationList", settings.filterSettingsList.names() );
//         changed |= IsChanged | ChangedFilterSettings;
//     }

    // ***************** DEPRECATED BEGIN *****************************************************
    // *** Used for migration of filter settings from versions prior to version 0.10 Beta 9 ***
    if ( cgGlobal.hasKey("filterConfigurationList") ) {
        cgGlobal.deleteEntry( "filterConfigurationList" );
    }
    // ***************** DEPRECATED END *******************************************************

    if ( settings.filterSettingsList != oldSettings.filterSettingsList ) {
//         QHash< QString, FilterSettings >::const_iterator it;
//         TODO
//         for ( it = settings.filterSettings.constBegin();
//                 it != settings.filterSettings.constEnd(); ++it ) {
        cgGlobal.writeEntry( "filterCount", settings.filterSettingsList.count() );
        int i = 1;
        foreach ( const FilterSettings &filterSettings, settings.filterSettingsList ) {
//             QString currentFilterConfig = it.key();
            if ( filterSettings.name.isEmpty() ) {
                kDebug() << "Empty filter config name, can't write settings";
                continue;
            }

            if ( oldSettings.filterSettingsList.hasName(filterSettings.name) ) {
                if ( SettingsIO::writeFilterConfig(filterSettings,
                     oldSettings.filterSettingsList.byName(filterSettings.name),
                     cgGlobal.group("filterConfig_" + QString::number(i))) )
                {
                    changed |= IsChanged | ChangedFilterSettings;
                }
            } else {
                SettingsIO::writeFilterConfig( filterSettings,
                        cgGlobal.group("filterConfig_" + QString::number(i)) );
                changed |= IsChanged | ChangedFilterSettings;
            }
            ++i;
        }
    }

    // Write alarm settings
    if ( settings.alarmSettings != oldSettings.alarmSettings ) {
        changed |= IsChanged | ChangedAlarmSettings;
        int i = 1;
        cgGlobal.writeEntry( "alarmCount", settings.alarmSettings.count() );
        foreach( const AlarmSettings &alarmSettings, settings.alarmSettings ) {
            QString suffix = i == 1 ? QString() : '_' + QString::number( i );
            cgGlobal.writeEntry( "alarmType" + suffix, static_cast<int>( alarmSettings.type ) );
            cgGlobal.writeEntry( "alarmStops" + suffix, alarmSettings.affectedStops );
            cgGlobal.writeEntry( "alarmFilter" + suffix, alarmSettings.filter.toData() );
            cgGlobal.writeEntry( "alarmEnabled" + suffix, alarmSettings.enabled );
            cgGlobal.writeEntry( "alarmName" + suffix, alarmSettings.name );
            cgGlobal.writeEntry( "alarmLastFired" + suffix, alarmSettings.lastFired );
            cgGlobal.writeEntry( "alarmAutogenerated" + suffix, alarmSettings.autoGenerated );
            ++i;
        }

        // Delete old stop settings entries
        QString test = "alarmType" + QString::number( i );
        while ( cgGlobal.hasKey( test ) ) {
            QString suffix = i == 1 ? QString() : '_' + QString::number( i );
            cgGlobal.deleteEntry( "alarmType" + suffix );
            cgGlobal.deleteEntry( "alarmStops" + suffix );
            cgGlobal.deleteEntry( "alarmFilter" + suffix );
            cgGlobal.deleteEntry( "alarmEnabled" + suffix );
            cgGlobal.deleteEntry( "alarmName" + suffix );
            cgGlobal.deleteEntry( "alarmLastFired" + suffix );
            cgGlobal.deleteEntry( "alarmAutogenerated" + suffix );
            ++i;
            test = "alarmType_" + QString::number( i );
        }
    }

    return changed;
}

FilterSettings SettingsIO::readFilterConfig( const KConfigGroup &cgGlobal )
{
    FilterSettings filterSettings;
    filterSettings.name = cgGlobal.readEntry( "Name", QString() );
    filterSettings.filterAction = static_cast< FilterAction >(
            cgGlobal.readEntry("FilterAction", static_cast<int>(ShowMatching)) );
    filterSettings.affectedStops = cgGlobal.readEntry( "AffectedStops", QList<int>() );

    QByteArray baFilters = cgGlobal.readEntry( "Filters", QByteArray() );
    filterSettings.filters.fromData( baFilters );
    return filterSettings;
}

bool SettingsIO::writeFilterConfig( const FilterSettings &filterSettings,
        const FilterSettings &oldFilterSettings, KConfigGroup cgGlobal )
{
    bool changed = false;
    
    if ( filterSettings.name != oldFilterSettings.name ) {
        cgGlobal.writeEntry( "Name", filterSettings.name );
        changed = true;
    }
    
    if ( filterSettings.filters != oldFilterSettings.filters ) {
        cgGlobal.writeEntry( "Filters", filterSettings.filters.toData() );
        changed = true;
    }

    if ( filterSettings.filterAction != oldFilterSettings.filterAction ) {
        cgGlobal.writeEntry( "FilterAction", static_cast<int>(filterSettings.filterAction) );
        changed = true;
    }

    if ( filterSettings.affectedStops != oldFilterSettings.affectedStops ) {
        cgGlobal.writeEntry( "AffectedStops", filterSettings.affectedStops );
        changed = true;
    }

    return changed;
}

void SettingsIO::writeFilterConfig( const FilterSettings& filterSettings,
                                    KConfigGroup cgGlobal )
{
    cgGlobal.writeEntry( "Name", filterSettings.name );
    cgGlobal.writeEntry( "Filters", filterSettings.filters.toData() );
    cgGlobal.writeEntry( "FilterAction", static_cast<int>( filterSettings.filterAction ) );
    cgGlobal.writeEntry( "AffectedStops", filterSettings.affectedStops );
}

bool operator ==( const AlarmSettings &l, const AlarmSettings &r )
{
    return l.name == r.name && l.enabled == r.enabled && l.type == r.type
        && l.affectedStops == r.affectedStops && l.filter == r.filter
        && l.lastFired == r.lastFired;
}

bool operator==( const ColorGroupSettings &l, const ColorGroupSettings &r )
{
    return l.color == r.color && l.filters == r.filters;
}
