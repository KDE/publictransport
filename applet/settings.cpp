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
#include <KStandardDirs>
#include <KMessageBox>
#include <KLineEdit>
#include <KCategorizedSortFilterProxyModel>

// Own includes
#include "settings.h"
#include "htmldelegate.h"
#include "filterwidget.h"
#include "stopwidget.h"
#include "departuremodel.h"
// #include "datasourcetester.h"

SettingsUiManager::SettingsUiManager( const Settings &settings,
	    Plasma::DataEngine* publicTransportEngine, Plasma::DataEngine* osmEngine,
	    Plasma::DataEngine* favIconEngine, Plasma::DataEngine* geolocationEngine,
	    KConfigDialog *parentDialog, DeletionPolicy deletionPolicy )
	    : QObject( parentDialog ), m_deletionPolicy(deletionPolicy),
// 	    m_dataSourceTester(new DataSourceTester("", publicTransportEngine, this)),
	    m_configDialog(parentDialog), m_modelServiceProvider(0),
	    m_modelLocations(0), m_stopListWidget(0),
	    m_publicTransportEngine(publicTransportEngine), m_osmEngine(osmEngine),
	    m_favIconEngine(favIconEngine), m_geolocationEngine(geolocationEngine) {
    m_currentStopSettingsIndex = settings.currentStopSettingsIndex;
    m_recentJourneySearches = settings.recentJourneySearches;

    m_filterSettings = settings.filterSettings;
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

    KTabWidget *tabMain = new KTabWidget;
    tabMain->addTab( widgetStop, i18n("&Stop selection") );
    tabMain->addTab( widgetAdvanced, i18n("&Advanced") );

    m_configDialog->addPage( tabMain, i18n("General"), "public-transport-stop" );
    m_configDialog->addPage( widgetAppearance, i18n("Appearance"),
			     "package_settings_looknfeel" );
    m_configDialog->addPage( widgetFilter, i18n("Filter"), "view-filter" );
    m_configDialog->addPage( widgetAlarms, i18n("Alarms"), "task-reminder" );

    initModels();

    // Setup stop widgets
    QStringList trFilterConfigurationList;
    foreach ( const QString &filterConfiguration, settings.filterSettings.keys() )
	trFilterConfigurationList << translateKey( filterConfiguration );
    m_stopListWidget = new StopListWidget( settings.stopSettingsList,
	    trFilterConfigurationList, m_modelLocations,
	    m_modelServiceProvider, m_publicTransportEngine, m_osmEngine,
	    m_geolocationEngine, m_ui.stopList );
    m_stopListWidget->setWhatsThis( i18n("<b>This shows the stop settings you "
	    "have set.</b><br>"
	    "The applet shows results for one of them at a time. To switch the "
	    "currently used stop setting use the context menu of the applet.<br>"
	    "For each stop setting another filter configuration can be used, "
	    "to edit filter configurations use the filter section in the settings "
	    "dialog. You can define a list of stops for each stop setting that "
	    "are then displayed combined (eg. stops near to each other).") );
    m_stopListWidget->setCurrentStopSettingIndex( m_currentStopSettingsIndex );
    connect( m_stopListWidget, SIGNAL(changed(int,StopSettings)),
	     this, SLOT(stopSettingsChanged()) );
    connect( m_stopListWidget, SIGNAL(added(QWidget*)),
	     this, SLOT(stopSettingsAdded()) );
    connect( m_stopListWidget, SIGNAL(removed(QWidget*,int)),
	     this, SLOT(stopSettingsRemoved(QWidget*,int)) );
    stopSettingsChanged();
    
    QVBoxLayout *lStop = new QVBoxLayout( m_ui.stopList );
    lStop->setContentsMargins( 0, 0, 0, 0 );
    lStop->addWidget( m_stopListWidget );
    connect( m_stopListWidget, SIGNAL(changed(int,StopSettings)),
	     this, SLOT(updateFilterInfoLabel()) );

    // Setup filter widgets
    m_uiFilter.filters->setWhatsThis( i18n("<b>This shows the filters of the "
	    "selected filter configuration.</b><br>"
	    "Each filter configuration consists of a filter action and a list "
	    "of filters. Each filter contains a list of constraints.<br>"
	    "A filter matches, if all it's constraints match while a filter "
	    "configuration matches, if one of it's filters match.<br>"
	    "To use a filter configuration select it in the 'Filter Uses' tab. "
	    "Each stop settings can use another filter configuration.<br><br>"
	    "<b>Filter Types</b><br>"
	    "- <b>Vehicle:</b> Filters by vehicle types.<br>"
	    "- <b>Line String:</b> Filters by transport line strings.<br>"
	    "- <b>Line number:</b> Filters by transport line numbers.<br>"
	    "- <b>Target:</b> Filters by target/origin.<br>"
	    "- <b>Via:</b> Filters by intermediate stops.<br>"
	    "- <b>Delay:</b> Filters by delay.") );
    connect( m_uiFilter.filters, SIGNAL(changed()),
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

    setValuesOfAdvancedConfig( settings );
    setValuesOfAppearanceConfig( settings );
    setValuesOfAlarmConfig();
    setValuesOfFilterConfig();
    m_uiFilter.enableFilters->setChecked( settings.filtersEnabled );
    if ( !m_alarmSettings.isEmpty() )
	currentAlarmChanged( 0 );

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

void SettingsUiManager::configFinished() {
    emit settingsFinished();
    if ( m_deletionPolicy == DeleteWhenFinished )
	deleteLater();
}

void SettingsUiManager::configAccepted() {
    emit settingsAccepted( settings() );
}

void SettingsUiManager::removeAlarms( const AlarmSettingsList& /*newAlarmSettings*/,
				      const QList<int> &/*removedAlarms*/ ) {
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

void SettingsUiManager::alarmChanged( QListWidgetItem *item ) {
    int row = m_uiAlarms.alarmList->row( item );
    m_alarmSettings[ row ].name = item->text();
    m_alarmSettings[ row ].enabled = item->checkState() == Qt::Checked;
    m_alarmsChanged = true; // Leave values of autoGenerated and lastFired
    kDebug() << "ALARM CHANGED" << m_alarmSettings[ row ].name << m_alarmSettings[ row ].enabled;
}

void SettingsUiManager::currentAlarmChanged( int row ) {
    if ( row == -1 ) {
	m_uiAlarms.splitter->widget( 1 )->setDisabled( true );
    } else {
	if ( m_alarmsChanged && m_lastAlarm != -1 ) {
	    // Store to last edited alarm settings
	    if ( m_lastAlarm < m_alarmSettings.count() ) {
		m_alarmSettings[ m_lastAlarm ] = currentAlarmSettings(
			m_uiAlarms.alarmList->item(m_lastAlarm)->text() );
	    } else
		kDebug() << "m_lastAlarm is bad" << m_lastAlarm;
	}

	disconnect( m_uiAlarms.alarmType, SIGNAL(currentIndexChanged(int)),
		    this, SLOT(currentAlarmTypeChanged(int)) );
	disconnect( m_uiAlarms.affectedStops, SIGNAL(checkedItemsChanged()),
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
    }
	
    m_lastAlarm = row;
}

void SettingsUiManager::addAlarmClicked() {
    QString name = i18n( "New Alarm" );
    int i = 2;
    for ( int n = 0; n < m_alarmSettings.count(); ++n ) {
	if ( m_alarmSettings[n].name == name ) {
	    name = i18n( "New Alarm %1", i );
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
    m_uiAlarms.alarmList->editItem( m_uiAlarms.alarmList->currentItem() );

    alarmChanged();
}

void SettingsUiManager::removeAlarmClicked() {
    if ( m_uiAlarms.alarmList->currentRow() == -1 )
	return;
    
    m_alarmSettings.removeAt( m_uiAlarms.alarmList->currentRow() );
    disconnect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
		this, SLOT(currentAlarmChanged(int)) );
    delete m_uiAlarms.alarmList->takeItem( m_uiAlarms.alarmList->currentRow() );
    connect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
	     this, SLOT(currentAlarmChanged(int)) );
    m_lastAlarm = m_uiAlarms.alarmList->currentRow();
    setValuesOfAlarmConfig();

    alarmChanged();
}

void SettingsUiManager::alarmChanged() {
    int row = m_uiAlarms.alarmList->currentRow();
    if ( row != -1 ) {
	m_alarmSettings[ row ].lastFired = QDateTime(); // Reenable this alarm for all departure if changed
	m_alarmSettings[ row ].autoGenerated = false;
    }
    m_alarmsChanged = true;
}

void SettingsUiManager::currentAlarmTypeChanged( int index ) {
    // Make font bold if a recurring alarm is selected
    QListWidgetItem *item = m_uiAlarms.alarmList->currentItem();
    QFont font = item->font();
    font.setBold( static_cast<AlarmType>(index) != AlarmRemoveAfterFirstMatch );
    item->setFont( font );
    
    alarmChanged();
}

void SettingsUiManager::affectedStopsChanged() {
    QListWidgetItem *item = m_uiAlarms.alarmList->currentItem();
    setAlarmTextColor( item, m_uiAlarms.affectedStops->hasCheckedItems() );
    
    alarmChanged();
}

void SettingsUiManager::setAlarmTextColor( QListWidgetItem *item, bool hasAffectedStops ) const {
    // Use negative text color if no affected stop is selected
    QColor color = !hasAffectedStops
	    ? KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText).color()
	    : KColorScheme(QPalette::Active).foreground(KColorScheme::NormalText).color();
    item->setTextColor( color );
    QPalette p = m_uiAlarms.affectedStops->palette();
    KColorScheme::adjustForeground( p, hasAffectedStops ? KColorScheme::NormalText
				    : KColorScheme::NegativeText,
				    QPalette::ButtonText, KColorScheme::Button );
    m_uiAlarms.affectedStops->setPalette( p );
}

void SettingsUiManager::stopSettingsAdded() {
    StopSettings stopSettings = m_stopListWidget->stopSettingsList().last();
    QString text = stopSettings.stops.join( ", " );
    if ( !stopSettings.city.isEmpty() )
	text += " in " + stopSettings.city;
    m_uiAlarms.affectedStops->addItem( text );
    
    stopSettingsChanged();
}

void SettingsUiManager::stopSettingsRemoved( QWidget*, int widgetIndex ) {
    // Store current alarm settings
    if ( m_alarmsChanged && m_uiAlarms.alarmList->currentRow() != -1 )
	m_alarmSettings[ m_uiAlarms.alarmList->currentRow() ] = currentAlarmSettings();

    // Adjust stop indices
    for ( int i = m_alarmSettings.count() - 1; i >= 0; --i ) {
	AlarmSettings &alarmSettings = m_alarmSettings[ i ];
	for ( int n = alarmSettings.affectedStops.count() - 1; n >= 0; --n ) {
	    if ( alarmSettings.affectedStops[n] == widgetIndex )
		alarmSettings.affectedStops.removeAt( n );
	    else if ( alarmSettings.affectedStops[n] > widgetIndex )
		--alarmSettings.affectedStops[n];
	}
    }
    
    QStringList stopLabels;
    StopSettingsList stopSettingsList = m_stopListWidget->stopSettingsList();
    foreach ( const StopSettings &stopSettings, stopSettingsList ) {
	QString text = stopSettings.stops.join( ", " );
	if ( !stopSettings.city.isEmpty() )
	    text += " in " + stopSettings.city;
	stopLabels << text;
    }
    m_uiAlarms.affectedStops->clear();
    m_uiAlarms.affectedStops->addItems( stopLabels );
    if ( m_uiAlarms.alarmList->currentRow() != -1 ) {
	m_uiAlarms.affectedStops->setCheckedRows(
		m_alarmSettings[m_uiAlarms.alarmList->currentRow()].affectedStops );
    }
    
    stopSettingsChanged();
}

void SettingsUiManager::stopSettingsChanged() {
    StopSettingsList stopSettingsList = m_stopListWidget->stopSettingsList();

    // Update affected stops combobox in the alarm page
    for ( int i = 0; i < stopSettingsList.count(); ++i ) {
	const StopSettings &stopSettings = stopSettingsList[ i ];
	QString text = stopSettings.stops.join( ", " );
	if ( !stopSettings.city.isEmpty() )
	    text += " in " + stopSettings.city;

	if ( i < m_uiAlarms.affectedStops->count() )
	    m_uiAlarms.affectedStops->setItemText( i, text );
	else
	    m_uiAlarms.affectedStops->addItem( text );
    }
    
    // Delete old "filter uses" widgets
    if ( m_uiFilter.filterUsesArea->layout() ) {
	QWidgetList oldWidgets = m_uiFilter.filterUsesArea->findChildren< QWidget* >(
		QRegExp("*Uses*", Qt::CaseSensitive, QRegExp::Wildcard) );
	foreach ( QWidget *widget, oldWidgets )
	    delete widget;
	delete m_uiFilter.filterUsesArea->layout();
    }

    // Add new "filter uses" widgets
    QGridLayout *l = new QGridLayout( m_uiFilter.filterUsesArea );
    l->setContentsMargins( 0, 0, 0, 0 );

    QStringList filterConfigurations = m_filterSettings.keys();
    QStringList trFilterConfigurations;
    foreach ( const QString &filterConfig, filterConfigurations )
	trFilterConfigurations << translateKey( filterConfig );

    QSignalMapper *mapper = new QSignalMapper( m_uiFilter.filterUsesArea );
    int row = 0;
    foreach ( const StopSettings &stopSettings, stopSettingsList ) {
	QString sRow = QString::number( row );
	QString text = stopSettings.stops.join( ", " );
	if ( !stopSettings.city.isEmpty() )
	    text += " in " + stopSettings.city;
	QLabel *label = new QLabel( text, m_uiFilter.filterUsesArea );
	label->setWordWrap( true );
	label->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
	label->setObjectName( "lblUsesStop" + sRow );
	
	QLabel *lblUses = new QLabel( i18n("<b>uses</b>") );
	lblUses->setAlignment( Qt::AlignCenter );
	lblUses->setObjectName( "lblUses" + sRow );
	
	KComboBox *cmbFilterConfigs = new KComboBox( m_uiFilter.filterUsesArea );
	cmbFilterConfigs->setObjectName( QString::number(row) );
	cmbFilterConfigs->addItems( trFilterConfigurations );
	cmbFilterConfigs->setCurrentItem( translateKey(stopSettings.filterConfiguration) );
	cmbFilterConfigs->setObjectName( "lblUsesConfigs" + sRow ); // This must be 14 chars, followed by an int, see usedFilterConfigChanged().
	connect( cmbFilterConfigs, SIGNAL(currentIndexChanged(int)),
		 mapper, SLOT(map()) );
	mapper->setMapping( cmbFilterConfigs, cmbFilterConfigs );
	
	l->addItem( new QSpacerItem(0, 0, QSizePolicy::Expanding), row, 0 );
	l->addWidget( label, row, 1 );
	l->addWidget( lblUses, row, 2 );
	l->addWidget( cmbFilterConfigs, row, 3 );
	l->addItem( new QSpacerItem(0, 0, QSizePolicy::Expanding), row, 4 );
	l->setAlignment( lblUses, Qt::AlignCenter );
	l->setAlignment( cmbFilterConfigs, Qt::AlignLeft | Qt::AlignVCenter );
	++row;
    }

    connect( mapper, SIGNAL(mapped(QWidget*)),
	     this, SLOT(usedFilterConfigChanged(QWidget*)) );
}

void SettingsUiManager::usedFilterConfigChanged( QWidget* widget ) {
    disconnect( m_stopListWidget, SIGNAL(changed(int,StopSettings)),
		this, SLOT(stopSettingsChanged()) );
    disconnect( m_stopListWidget, SIGNAL(added(QWidget*)),
		this, SLOT(stopSettingsAdded()) );
    disconnect( m_stopListWidget, SIGNAL(removed(QWidget*,int)),
		this, SLOT(stopSettingsRemoved(QWidget*,int)) );
    
    int index = widget->objectName().mid( 14 ).toInt();
    StopSettingsList stopSettingsList = m_stopListWidget->stopSettingsList();
    if ( stopSettingsList.count() > index ) {
	stopSettingsList[ index ].filterConfiguration = untranslateKey(
		qobject_cast<KComboBox*>(widget)->currentText() );
	m_stopListWidget->setStopSettingsList( stopSettingsList );
    }
    
    connect( m_stopListWidget, SIGNAL(changed(int,StopSettings)),
	     this, SLOT(stopSettingsChanged()) );
    connect( m_stopListWidget, SIGNAL(added(QWidget*)),
	     this, SLOT(stopSettingsAdded()) );
    connect( m_stopListWidget, SIGNAL(removed(QWidget*,int)),
	     this, SLOT(stopSettingsRemoved(QWidget*,int)) );
    updateFilterInfoLabel();
}

void SettingsUiManager::updateFilterInfoLabel() {
    QString filterConfiguration = untranslateKey(
	    m_uiFilter.filterConfigurations->currentText() );
    QStringList usedByList;
    foreach ( const StopSettings &stopSettings, m_stopListWidget->stopSettingsList() ) {
	if ( stopSettings.filterConfiguration == filterConfiguration )
	    usedByList << stopSettings.stops.join("; ");
    }
    
    QPalette palette = m_uiFilter.lblInfo->palette();
    if ( usedByList.isEmpty() ) {
	m_uiFilter.lblInfo->setText( i18n("Not used by any stop settings.") );
	m_uiFilter.lblInfo->setToolTip( QString() );
	KColorScheme::adjustForeground( palette, KColorScheme::NegativeText, QPalette::WindowText );
    } else {
	m_uiFilter.lblInfo->setText( i18np("Used by %1 stop setting.",
			"Used by %1 stop settings.", usedByList.count()) );
	m_uiFilter.lblInfo->setToolTip( i18n("Used by these stops:\n - %1",
					    usedByList.join("\n - ")) );
	KColorScheme::adjustForeground( palette, KColorScheme::NormalText, QPalette::WindowText );
    }
    m_uiFilter.lblInfo->setPalette( palette );
}

void SettingsUiManager::setValuesOfAdvancedConfig( const Settings &settings ) {
    m_uiAdvanced.showDepartures->setChecked(
	    settings.departureArrivalListType == DepartureList );
    m_uiAdvanced.showArrivals->setChecked(
	    settings.departureArrivalListType == ArrivalList );
    m_uiAdvanced.updateAutomatically->setChecked( settings.autoUpdate );
    m_uiAdvanced.maximalNumberOfDepartures->setValue(
	    settings.maximalNumberOfDepartures );
}

void SettingsUiManager::setValuesOfAppearanceConfig( const Settings &settings ) {
    m_uiAppearance.linesPerRow->setValue( settings.linesPerRow );
    m_uiAppearance.size->setValue( settings.size );
    if ( settings.showRemainingMinutes && settings.showDepartureTime )
	m_uiAppearance.cmbDepartureColumnInfos->setCurrentIndex(0);
    else if ( settings.showRemainingMinutes )
	m_uiAppearance.cmbDepartureColumnInfos->setCurrentIndex(2);
    else
	m_uiAppearance.cmbDepartureColumnInfos->setCurrentIndex(1);
    m_uiAppearance.displayTimeBold->setChecked( settings.displayTimeBold );

    m_uiAppearance.shadow->setChecked( settings.drawShadows );
    m_uiAppearance.showHeader->setChecked( settings.showHeader );
    m_uiAppearance.showColumnTarget->setChecked( !settings.hideColumnTarget );
    m_uiAppearance.radioUseDefaultFont->setChecked( settings.useDefaultFont );
    m_uiAppearance.radioUseOtherFont->setChecked( !settings.useDefaultFont );
    m_uiAppearance.font->setCurrentFont( settings.font );
}

void SettingsUiManager::setValuesOfAlarmConfig() {
    kDebug() << "Set Alarm Values, in list:" << m_uiAlarms.alarmList->count()
	     << "in variable:" << m_alarmSettings.count();
	
    disconnect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
		this, SLOT(currentAlarmChanged(int)) );
    int row = m_uiAlarms.alarmList->currentRow();
    m_uiAlarms.alarmList->clear();
    foreach ( AlarmSettings alarmSettings, m_alarmSettings ) {
	QListWidgetItem *item = new QListWidgetItem( alarmSettings.name );
	item->setFlags( item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable );
	item->setCheckState( alarmSettings.enabled ? Qt::Checked : Qt::Unchecked );
	if ( alarmSettings.type != AlarmRemoveAfterFirstMatch ) {
	    // TODO: Update this on alarm type change
	    QFont font = item->font();
	    font.setBold( true );
	    item->setFont( font );
	}
	kDebug() << "ADD ALARM ITEM WITH NAME" << alarmSettings.name;
	
	setAlarmTextColor( item, !alarmSettings.affectedStops.isEmpty() );
	m_uiAlarms.alarmList->addItem( item );
    }
    if ( row < m_alarmSettings.count() && row != -1 )
	m_uiAlarms.alarmList->setCurrentRow( row );
    else if ( !m_alarmSettings.isEmpty() )
	m_uiAlarms.alarmList->setCurrentRow( row = 0 );
    else
	kDebug() << "Cannot reselect row" << row;

    if ( row < m_alarmSettings.count() && row != -1 ) {
	const AlarmSettings alarmSettings = m_alarmSettings.at( row );
	m_uiAlarms.alarmType->setCurrentIndex( static_cast<int>(alarmSettings.type) );
	m_uiAlarms.affectedStops->setCheckedRows( alarmSettings.affectedStops );
	m_uiAlarms.alarmFilter->setFilter( alarmSettings.filter );
    }

    m_uiAlarms.splitter->widget( 1 )->setDisabled( m_uiAlarms.alarmList->currentRow() == -1 );
    connect( m_uiAlarms.alarmList, SIGNAL(currentRowChanged(int)),
	     this, SLOT(currentAlarmChanged(int)) );
}

void SettingsUiManager::setValuesOfFilterConfig() {
    if ( m_uiFilter.filterConfigurations->currentIndex() == -1 )
	m_uiFilter.filterConfigurations->setCurrentIndex( 0 );
    
    QStringList filterConfigs;
    foreach ( const QString &filterConfig, m_filterSettings.keys() )
	filterConfigs << translateKey( filterConfig );

    QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();

    disconnect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
		this, SLOT(loadFilterConfiguration(QString)) );
    m_uiFilter.filterConfigurations->clear();
    m_uiFilter.filterConfigurations->addItems( filterConfigs );
    if ( trFilterConfiguration.isEmpty() )
	m_uiFilter.filterConfigurations->setCurrentIndex( 0 );
    else
	m_uiFilter.filterConfigurations->setCurrentItem( trFilterConfiguration );
    connect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
	     this, SLOT(loadFilterConfiguration(QString)) );
    
    if ( trFilterConfiguration.isEmpty() ) {
	kDebug() << "No Item Selected";
	trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
    }
    Q_ASSERT_X( !trFilterConfiguration.isEmpty(),
		"SettingsUiManager::setValuesOfFilterConfig",
		"No filter configuration" );

    updateFilterInfoLabel();
    stopSettingsChanged();

    QString filterConfiguration = untranslateKey( trFilterConfiguration );
    bool isDefaultFilterConfig = filterConfiguration == "Default";
    m_uiFilter.removeFilterConfiguration->setDisabled( isDefaultFilterConfig );
    m_uiFilter.renameFilterConfiguration->setDisabled( isDefaultFilterConfig );
	     
    FilterSettings filterSettings = m_filterSettings[ filterConfiguration ];
    m_uiFilter.filterAction->setCurrentIndex(
	    static_cast<int>(filterSettings.filterAction) );
    filterActionChanged( m_uiFilter.filterAction->currentIndex() );

    // Clear old filter widgets
    int minWidgetCount = m_uiFilter.filters->minimumWidgetCount();
    int maxWidgetCount = m_uiFilter.filters->maximumWidgetCount();
    m_uiFilter.filters->setWidgetCountRange();
    m_uiFilter.filters->removeAllWidgets();
    
    // Setup FilterWidgets from m_filters
    foreach ( const Filter &filter, filterSettings.filters )
	m_uiFilter.filters->addFilter( filter );
    
    int added = m_uiFilter.filters->setWidgetCountRange( minWidgetCount, maxWidgetCount );
    setFilterConfigurationChanged( added != 0 );
}

void SettingsUiManager::initModels() {
    // Setup model for the service provider combobox
    m_modelServiceProvider = new QStandardItemModel( 0, 1, this );

    // Setup model for the location combobox
    m_modelLocations = new QStandardItemModel( 0, 1, this );

    // Get locations
    m_locationData = m_publicTransportEngine->query( "Locations" );
    QStringList uniqueCountries = m_locationData.keys();
    QStringList countries;

    // Get a list with the location of each service provider
    // (locations can be contained multiple times)
    m_serviceProviderData = m_publicTransportEngine->query("ServiceProviders");
    foreach ( QString serviceProviderName, m_serviceProviderData.keys() )  {
	QHash< QString, QVariant > serviceProviderData =
		m_serviceProviderData.value(serviceProviderName).toHash();
	countries << serviceProviderData["country"].toString();
    }

    // Create location items
    foreach( QString country, uniqueCountries ) {
	QStandardItem *item;
	QString text, sortText;
	item = new QStandardItem;

	if ( country.compare("international", Qt::CaseInsensitive)  == 0 ) {
	    text = i18n("International");
	    sortText = "00000" + text;
	    item->setIcon( Global::internationalIcon() );
	} else if ( country.compare("unknown", Qt::CaseInsensitive)  == 0 ) {
	    text = i18n("Unknown");
	    sortText = "00000" + text;
	    item->setIcon( KIcon("dialog-warning") );
	} else {
	    if ( KGlobal::locale()->allCountriesList().contains( country ) )
		text = KGlobal::locale()->countryCodeToName( country );
	    else
		text = country;
	    sortText = "11111" + text;
	    item->setIcon( Global::putIconIntoBiggerSizeIcon(KIcon(country), QSize(32, 23)) );
	}

	QString formattedText = QString( "<span><b>%1</b></span> "
					 "<small>(<b>%2</b>)<br-wrap>%3</small>" )
		.arg( text )
		.arg( i18np("%1 accessor", "%1 accessors", countries.count(country)) )
		.arg( m_locationData[country].toHash()["description"].toString() );

	item->setText( text );
	item->setData( country, LocationCodeRole );
	item->setData( formattedText, FormattedTextRole );
	item->setData( sortText, SortRole );
	item->setData( 4, LinesPerRowRole );

	m_modelLocations->appendRow( item );
    }

    // Append item to show all service providers
    QString sShowAll = i18n("Show all available service providers");
    QStandardItem *itemShowAll = new QStandardItem();
    itemShowAll->setData( "000000", SortRole );
    QString formattedText = QString( "<span><b>%1</b></span>"
				     "<br-wrap><small><b>%2</b></small>" )
	.arg( sShowAll )
	.arg( i18n("Total: ") + i18np("%1 accessor", "%1 accessors", countries.count()) );
    itemShowAll->setData( "showAll", LocationCodeRole );
    itemShowAll->setData( formattedText, FormattedTextRole );
    itemShowAll->setText( sShowAll );
    itemShowAll->setData( 3, LinesPerRowRole );
    itemShowAll->setIcon( KIcon("package_network") ); // TODO: Other icon?
    m_modelLocations->appendRow( itemShowAll );

    // Get errornous service providers (TODO: Get error messages)
    QStringList errornousAccessorNames = m_publicTransportEngine
	    ->query("ErrornousServiceProviders")["names"].toStringList();
    if ( !errornousAccessorNames.isEmpty() ) {
	QStringList errorLines;
	for( int i = 0; i < errornousAccessorNames.count(); ++i ) {
	    errorLines << QString("<b>%1</b>").arg( errornousAccessorNames[i] );//.arg( errorMessages[i] );
	}
	QStandardItem *itemErrors = new QStandardItem();
	itemErrors->setData( "ZZZZZ", SortRole ); // Sort to the end
	formattedText = QString( "<span><b>%1</b></span><br-wrap><small>%2</small>" )
	    .arg( i18np("%1 accessor is errornous:", "%1 accessors are errornous:",
			errornousAccessorNames.count()) )
	    .arg( errorLines.join(",<br-wrap>") );
	itemErrors->setData( formattedText, FormattedTextRole );
	itemErrors->setData( 1 + errornousAccessorNames.count(),
			     LinesPerRowRole );
	itemErrors->setSelectable( false );
	itemErrors->setIcon( KIcon("edit-delete") );
	m_modelLocations->appendRow( itemErrors );
    }

    m_modelLocations->setSortRole( SortRole );
    m_modelLocations->sort( 0 );

    // Init service provider model
    QList< QStandardItem* > appendItems;
    foreach( QString serviceProviderName, m_serviceProviderData.keys() ) {
	QVariantHash serviceProviderData = m_serviceProviderData[serviceProviderName].toHash();

	// TODO Add a flag to the accessor XML files, maybe <countryWide />
	bool isCountryWide = serviceProviderName.contains(
		serviceProviderData["country"].toString(), Qt::CaseInsensitive );
	QString formattedText;
	QStandardItem *item = new QStandardItem( serviceProviderName ); // TODO: delete?

	formattedText = QString( "<b>%1</b><br-wrap><small><b>Features:</b> %2</small>" )
	    .arg( serviceProviderName )
	    .arg( serviceProviderData["featuresLocalized"].toStringList().join(", ") );
	item->setData( 4, LinesPerRowRole );
	item->setData( formattedText, FormattedTextRole );
	item->setData( serviceProviderData, ServiceProviderDataRole );
	item->setData( serviceProviderData["country"], LocationCodeRole );
	item->setData( serviceProviderData["id"], ServiceProviderIdRole );

	// Sort service providers containing the country in it's
	// name to the top of the list for that country
	QString locationCode = serviceProviderData["country"].toString();
	QString sortString;
	if ( locationCode == "international" ) {
	    sortString = "XXXXX" + serviceProviderName;
	} else if ( locationCode == "unknown" ) {
	    sortString = "YYYYY" + serviceProviderName;
	} else {
	    QString countryName = KGlobal::locale()->countryCodeToName( locationCode );
	    sortString = isCountryWide
		    ? "WWWWW" + countryName + "11111" + serviceProviderName
		    : "WWWWW" + countryName + serviceProviderName;
	}
	item->setData( sortString, SortRole );
	
	QString title;
	if ( locationCode == "international" )
	    title = i18n("International");
	else if ( locationCode == "unknown" )
	    title = i18n("Unknown");
	else
	    title = KGlobal::locale()->countryCodeToName( locationCode );
	item->setData( title,
		       KCategorizedSortFilterProxyModel::CategoryDisplayRole );
	item->setData( sortString,
		       KCategorizedSortFilterProxyModel::CategorySortRole );
	
	m_modelServiceProvider->appendRow( item );

	// Request favicons
	QString favIconSource = serviceProviderData["url"].toString();
	m_favIconEngine->disconnectSource( favIconSource, this );
	m_favIconEngine->connectSource( favIconSource, this );
	m_favIconEngine->query( favIconSource );
    }
    m_modelServiceProvider->setSortRole( SortRole );
    m_modelServiceProvider->sort( 0 );
}

void SettingsUiManager::dataUpdated( const QString& sourceName,
				     const Plasma::DataEngine::Data& data ) {
    if ( sourceName.contains(QRegExp("^http")) ) {
	// Favicon of a service provider arrived
	Q_ASSERT_X( m_modelServiceProvider, "SettingsUiManager::dataUpdated",
		    "No service provider model" );

	QPixmap favicon( QPixmap::fromImage(data["Icon"].value<QImage>()) );
	if ( favicon.isNull() ) {
	    kDebug() << "Favicon is NULL for" << sourceName;
	    favicon = QPixmap( 16, 16 );
	    favicon.fill( Qt::transparent );
	}
	
	for ( int i = 0; i < m_modelServiceProvider->rowCount(); ++i ) {
	    QVariantHash serviceProviderData = m_modelServiceProvider->item(i)
		    ->data( ServiceProviderDataRole ).toHash();
	    QString favIconSource = serviceProviderData["url"].toString();
	    if ( favIconSource.compare(sourceName) == 0 )
		m_modelServiceProvider->item(i)->setIcon( KIcon(favicon) );
	}

	m_favIconEngine->disconnectSource( sourceName, this );
    }
}

QString SettingsUiManager::translateKey( const QString& key ) {
    if ( key == "Default" )
	return i18n("Default");
    else
	return key;
}

QString SettingsUiManager::untranslateKey( const QString& translatedKey ) {
    if ( translatedKey == i18n("Default") )
	return "Default";
    else
	return translatedKey;
}

QString SettingsUiManager::showStringInputBox( const QString& labelString,
			const QString& initialText, const QString& clickMessage,
			const QString& title, QValidator* validator, QWidget *parent ) {
    KDialog *dialog = new KDialog( parent );
    dialog->setButtons( KDialog::Ok | KDialog::Cancel );
    dialog->setWindowTitle( title );
    
    QWidget *w = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout;
    QLabel *label = new QLabel( labelString );
    label->setAlignment( Qt::AlignRight );
    KLineEdit *input = new KLineEdit( initialText );
    input->setClearButtonShown( true );
    input->setClickMessage( clickMessage );
    validator->setParent( input );
    input->setValidator( validator );
    
    layout->addWidget( label );
    layout->addWidget( input );
    w->setLayout( layout );
    dialog->setMainWidget( w );
    
    input->setFocus();
    if ( dialog->exec() == KDialog::Rejected ) {
	delete dialog;
	return QString();
    }
    
    QString text = input->text();
    delete dialog;
    
    return text;
}

Settings SettingsUiManager::settings() {
    Settings ret;

    ret.stopSettingsList = m_stopListWidget->stopSettingsList();
    foreach ( StopSettings stopSettings, ret.stopSettingsList ) {
	stopSettings.filterConfiguration = untranslateKey(
		stopSettings.filterConfiguration );
    }

    // Set stored "no-Gui" settings
    ret.recentJourneySearches = m_recentJourneySearches;
    ret.currentStopSettingsIndex = m_currentStopSettingsIndex;
    if ( ret.currentStopSettingsIndex >= ret.stopSettingsList.count() )
	ret.currentStopSettingsIndex = ret.stopSettingsList.count() - 1;

    if ( m_filterConfigChanged ) {
	QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
	QString filterConfiguration = untranslateKey( trFilterConfiguration );
	m_filterSettings[ filterConfiguration ] = currentFilterSettings();
    }
    ret.filterSettings = m_filterSettings;
    ret.filtersEnabled = m_uiFilter.enableFilters->isChecked();

    if ( m_alarmsChanged && m_uiAlarms.alarmList->currentRow() != -1 )
	m_alarmSettings[ m_uiAlarms.alarmList->currentRow() ] = currentAlarmSettings();
    ret.alarmSettings = m_alarmSettings;
    
    if ( m_uiAdvanced.showArrivals->isChecked() )
	ret.departureArrivalListType = ArrivalList;
    else
	ret.departureArrivalListType = DepartureList;
    ret.autoUpdate = m_uiAdvanced.updateAutomatically->isChecked();
    ret.maximalNumberOfDepartures = m_uiAdvanced.maximalNumberOfDepartures->value();
    
    ret.showRemainingMinutes = m_uiAppearance.cmbDepartureColumnInfos->currentIndex() != 1;
    ret.showDepartureTime = m_uiAppearance.cmbDepartureColumnInfos->currentIndex() <= 1;
    ret.displayTimeBold = m_uiAppearance.displayTimeBold->checkState() == Qt::Checked;
    ret.drawShadows = m_uiAppearance.shadow->checkState() == Qt::Checked;
    ret.showHeader = m_uiAppearance.showHeader->checkState() == Qt::Checked;
    ret.hideColumnTarget = m_uiAppearance.showColumnTarget->checkState() == Qt::Unchecked;
    ret.linesPerRow = m_uiAppearance.linesPerRow->value();
    ret.size = m_uiAppearance.size->value();
    ret.sizeFactor = (ret.size + 3) * 0.2f;
    ret.useDefaultFont = m_uiAppearance.radioUseDefaultFont->isChecked();
    if ( !ret.useDefaultFont ) // TODO: why "if(!"... ?
	ret.font.setFamily( m_uiAppearance.font->currentFont().family() );
    else
	ret.font = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );

    return ret;
}

FilterSettings SettingsUiManager::currentFilterSettings() const {
    FilterSettings filterSettings;
    filterSettings.filterAction = static_cast< FilterAction >(
	    m_uiFilter.filterAction->currentIndex() );
    filterSettings.filters = m_uiFilter.filters->filters();
    return filterSettings;
}

AlarmSettings SettingsUiManager::currentAlarmSettings( const QString &name ) const {
    Q_ASSERT( m_uiAlarms.alarmList->currentRow() != -1 );
    
    AlarmSettings alarmSettings;
    int row = m_uiAlarms.alarmList->currentRow();
    if ( row >= 0 && row < m_alarmSettings.count() )
	alarmSettings = m_alarmSettings[ row ];
    else
	kDebug() << "No existing alarm settings found for the current alarm" << name;
    alarmSettings.enabled = m_uiAlarms.alarmList->currentItem()->checkState() == Qt::Checked;
    alarmSettings.name = name.isNull() ? m_uiAlarms.alarmList->currentItem()->text()
				       : name;
    alarmSettings.affectedStops = m_uiAlarms.affectedStops->checkedRows();
    alarmSettings.type = static_cast<AlarmType>( m_uiAlarms.alarmType->currentIndex() );
    alarmSettings.filter = m_uiAlarms.alarmFilter->filter();
    return alarmSettings;
}

void SettingsUiManager::loadFilterConfiguration( const QString& filterConfig ) {
    if ( filterConfig.isEmpty() )
	return;
    
    QString untrFilterConfig = untranslateKey( filterConfig );
    if ( untrFilterConfig == m_lastFilterConfiguration )
	return;

    if ( m_filterConfigChanged && !m_lastFilterConfiguration.isEmpty() ) {
	// Store to last edited filter settings
	m_filterSettings[ m_lastFilterConfiguration ] = currentFilterSettings();
    }

    m_lastFilterConfiguration = untrFilterConfig;

    setFilterConfigurationChanged( false );
    setValuesOfFilterConfig();
}

void SettingsUiManager::addFilterConfiguration() {
    QString newFilterConfig = i18n( "New Configuration" );
    int i = 2;
    while ( m_filterSettings.keys().contains(newFilterConfig) ) {
	newFilterConfig = i18n( "New Configuration %1", i );
	++i;
    }

    QString untrNewFilterConfig = untranslateKey( newFilterConfig );
    kDebug() << "Add new filter config" << newFilterConfig << untrNewFilterConfig;
    
    // Append new filter settings
    m_filterSettings.insert( untrNewFilterConfig, FilterSettings() );
    m_uiFilter.filterConfigurations->setCurrentItem( newFilterConfig, true,
						     m_uiFilter.filterConfigurations->count()-1);
    
    QStringList trFilterConfigurationList;
    foreach ( const QString &filterConfiguration, m_filterSettings.keys() )
	trFilterConfigurationList << translateKey( filterConfiguration );
    m_stopListWidget->setFilterConfigurations( trFilterConfigurationList );
    
    stopSettingsChanged(); // Rebuild "Filter Uses" tab in filter page
}

void SettingsUiManager::removeFilterConfiguration() {
    QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
    if ( KMessageBox::warningContinueCancel(m_configDialog,
	i18n("This will permanently delete the selected filter configuration '%1'.",
	     trFilterConfiguration))
	!= KMessageBox::Continue )
	return;
    
    QString filterConfiguration = untranslateKey( trFilterConfiguration );
    m_filterSettings.remove( filterConfiguration );

    int index = m_uiFilter.filterConfigurations->currentIndex();
    if ( index != -1  )
	m_uiFilter.filterConfigurations->removeItem( index );
    else
	kDebug() << "No selection";

    index = filterConfigurationIndex( i18n("Default") );
    kDebug() << "Found default filter config in combo box?" << index;
    if ( index != -1 )
	m_uiFilter.filterConfigurations->setCurrentIndex( index );
    else
	kDebug() << "Default filter configuration not found!";
    
    QStringList trFilterConfigurationList;
    foreach ( const QString &filterConfiguration, m_filterSettings.keys() )
	trFilterConfigurationList << translateKey( filterConfiguration );
    m_stopListWidget->setFilterConfigurations( trFilterConfigurationList );
    
    stopSettingsChanged(); // Rebuild "Filter Uses" tab in filter page
}

void SettingsUiManager::renameFilterConfiguration() {
    QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
    QString newFilterConfig = showStringInputBox(
	    i18n("New Name of the Filter Configuration:"),
	    trFilterConfiguration, i18nc("This is a clickMessage for the line edit "
	    "in the rename filter config dialog", "Type the new name"),
	    i18n("Choose a Name"), new QRegExpValidator(QRegExp("[^\\*]*"), this) );
    if ( newFilterConfig.isNull() )
	return; // Canceled

    QString filterConfiguration = untranslateKey( trFilterConfiguration );
    kDebug() << "newFilterConfig:" << newFilterConfig
	     << "current filter config:" << filterConfiguration;
    if ( newFilterConfig == trFilterConfiguration )
	return; // Not changed, but the old name was accepted

    // Check if the new name is valid.
    // '*' is also not allowed in the name but that's already validated by a QRegExpValidator.
    if ( newFilterConfig.isEmpty() ) {
	KMessageBox::information( m_configDialog, i18n("Empty names are not allowed.") );
	return;
    }

    // Check if the new name is already used and ask if it should be overwritten
    QString untrNewFilterConfig = untranslateKey( newFilterConfig );
    if ( m_filterSettings.keys().contains(untrNewFilterConfig)
	    && KMessageBox::warningYesNo(m_configDialog,
		i18n("There is already a filter configuration with the name '%1'.\n"
		"Do you want to overwrite it?", newFilterConfig) ) != KMessageBox::Yes ) {
	return; // No (don't overwrite) pressed
    }

    disconnect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
		this, SLOT(loadFilterConfiguration(QString)) );
    FilterSettings filterSettings = m_filterSettings[ filterConfiguration ];
    m_filterSettings.remove( filterConfiguration );
    m_filterSettings[ untrNewFilterConfig ] = filterSettings;

    int index = m_uiFilter.filterConfigurations->currentIndex();
    if ( index != -1  ) {
	kDebug() << "Remove old filter config" << trFilterConfiguration;
	m_uiFilter.filterConfigurations->removeItem( index );
    }

    m_uiFilter.filterConfigurations->setCurrentItem( newFilterConfig, true );
    m_lastFilterConfiguration = untrNewFilterConfig;
    connect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
	     this, SLOT(loadFilterConfiguration(QString)) );

    // Update filter configuration name in stop settings
    StopSettingsList stopSettingsList = m_stopListWidget->stopSettingsList();
    for ( int i = 0; i < stopSettingsList.count(); ++i ) {
	if ( stopSettingsList[i].filterConfiguration == filterConfiguration )
	    stopSettingsList[ i ].filterConfiguration = untrNewFilterConfig;
    }
    m_stopListWidget->setStopSettingsList( stopSettingsList );

    QStringList trFilterConfigurationList;
    foreach ( const QString &filterConfiguration, m_filterSettings.keys() )
	trFilterConfigurationList << translateKey( filterConfiguration );
    m_stopListWidget->setFilterConfigurations( trFilterConfigurationList );

    stopSettingsChanged(); // Rebuild "Filter Uses" tab in filter page
}

void SettingsUiManager::filterActionChanged( int index ) {
    FilterAction filterAction = static_cast< FilterAction >( index );
//     bool enableFilters = filterAction != ShowAll;
//     m_uiFilter.filters->setEnabled( enableFilters );
    
    // Store to last edited filter settings
    QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
    QString filterConfiguration = untranslateKey( trFilterConfiguration );
    m_filterSettings[ filterConfiguration ].filterAction = filterAction;

    setFilterConfigurationChanged();
}

void SettingsUiManager::setFilterConfigurationChanged( bool changed ) {
    if ( m_filterConfigChanged == changed )
	return;
    
    QString trFilterConfiguration = m_uiFilter.filterConfigurations->currentText();
    QString filterConfiguration = untranslateKey( trFilterConfiguration );
    bool defaultFilterConfig = filterConfiguration == "Default";
    m_uiFilter.removeFilterConfiguration->setDisabled( defaultFilterConfig );
    m_uiFilter.renameFilterConfiguration->setDisabled( defaultFilterConfig );

    m_filterConfigChanged = changed;
}

int SettingsUiManager::filterConfigurationIndex( const QString& filterConfig ) {
    int index = m_uiFilter.filterConfigurations->findText( filterConfig );
    if ( index == -1 )
	index = m_uiFilter.filterConfigurations->findText( filterConfig + "*" );
    else
	kDebug() << "Item" << filterConfig << "not found!";
    
    return index;
}

void SettingsUiManager::exportFilterSettings() {
    QString fileName = KFileDialog::getSaveFileName(
	    KUrl("kfiledialog:///filterSettings"), QString(), m_configDialog,
	    i18n("Export Filter Settings") );
    if ( fileName.isEmpty() )
	return;

    KConfig config( fileName, KConfig::SimpleConfig );
    SettingsIO::writeFilterConfig( currentFilterSettings(), config.group(QString()) );
}

void SettingsUiManager::importFilterSettings() {
    QString fileName = KFileDialog::getOpenFileName(
	    KUrl("kfiledialog:///filterSettings"), QString(), m_configDialog,
	    i18n("Import Filter Settings") );
    if ( fileName.isEmpty() )
	return;

    KConfig config( fileName, KConfig::SimpleConfig );
    FilterSettings filterSettings = SettingsIO::readFilterConfig( config.group(QString()) );
//     TODO: Set filterSettings in GUI
}

Settings SettingsIO::readSettings( KConfigGroup cg, KConfigGroup cgGlobal,
				   Plasma::DataEngine *publictransportEngine ) {
    Settings settings;
    settings.autoUpdate = cg.readEntry( "autoUpdate", true );
    settings.showRemainingMinutes = cg.readEntry( "showRemainingMinutes", true );
    settings.showDepartureTime = cg.readEntry( "showDepartureTime", true );
    settings.displayTimeBold = cg.readEntry( "displayTimeBold", true );
    
    settings.stopSettingsList.clear();
    int stopSettingCount = cgGlobal.readEntry( "stopSettings", 1 );
    QString test = "location";
    int i = 1;
    while ( cgGlobal.hasKey(test) ) {
	StopSettings stopSettings;
	QString suffix = i == 1 ? QString() : "_" + QString::number( i );
	stopSettings.location = cgGlobal.readEntry( "location" + suffix, "showAll" );
	stopSettings.serviceProviderID = cgGlobal.readEntry(
		"serviceProvider" + suffix, "de_db" );
	stopSettings.filterConfiguration = cgGlobal.readEntry(
		"filterConfiguration" + suffix, "Default" );
	stopSettings.city = cgGlobal.readEntry( "city" + suffix, QString() );
	stopSettings.stops = cgGlobal.readEntry( "stop" + suffix, QStringList() );
	stopSettings.stopIDs = cgGlobal.readEntry( "stopID" + suffix, QStringList() );
	stopSettings.timeOffsetOfFirstDeparture =
		cgGlobal.readEntry( "timeOffsetOfFirstDeparture" + suffix, 0 );
	stopSettings.timeOfFirstDepartureCustom = QTime::fromString(
		cgGlobal.readEntry("timeOfFirstDepartureCustom" + suffix, "12:00"), "hh:mm" );
	stopSettings.firstDepartureConfigMode = static_cast<FirstDepartureConfigMode>(
		cgGlobal.readEntry("firstDepartureConfigMode" + suffix,
			    static_cast<int>(RelativeToCurrentTime)) );
	stopSettings.alarmTime = cgGlobal.readEntry( "alarmTime" + suffix, 5 );
	settings.stopSettingsList << stopSettings;

	++i;
	test = "location_" + QString::number( i );
	if ( i > stopSettingCount )
	    break;
    }

    settings.currentStopSettingsIndex = cg.readEntry( "currentStopIndex", 0 ); // TODO Rename settings key to "currentStopSettingsIndex"?

    // Add initial stop settings when no settings are available
    if ( settings.stopSettingsList.isEmpty() ) {
	kDebug() << "Stop settings list in settings is empty";
	if ( publictransportEngine ) {
	    QString countryCode = KGlobal::locale()->country();
	    Plasma::DataEngine::Data locationData =
		    publictransportEngine->query( "Locations" );
	    QString defaultServiceProviderId =
		locationData[countryCode].toHash()["defaultAccessor"].toString();

	    StopSettings stopSettings;
	    if ( defaultServiceProviderId.isEmpty() ) {
		stopSettings.location = "showAll";
	    } else {
		stopSettings.location = countryCode;
		stopSettings.serviceProviderID = defaultServiceProviderId;
	    }
	    stopSettings.stops << "";
	    // TODO: Get initial stop names using StopFinder

	    settings.stopSettingsList << stopSettings;
	} else
	    settings.stopSettingsList << StopSettings();

    }
    
    if ( settings.currentStopSettingsIndex < 0 )
	settings.currentStopSettingsIndex = 0; // For compatibility with versions < 0.7
    else if ( settings.currentStopSettingsIndex >= settings.stopSettingsList.count() ) {
	kDebug() << "Current stop index in settings invalid";
	settings.currentStopSettingsIndex = settings.stopSettingsList.count() - 1;
    }

    settings.recentJourneySearches = cgGlobal.readEntry( "recentJourneySearches", QStringList() );
	
    settings.maximalNumberOfDepartures = cg.readEntry( "maximalNumberOfDepartures", 20 );
    settings.linesPerRow = cg.readEntry( "linesPerRow", 2 );
    settings.size = cg.readEntry( "size", 2 );
    settings.sizeFactor = (settings.size + 3) * 0.2f;
    settings.departureArrivalListType = static_cast<DepartureArrivalListType>(
	    cg.readEntry("departureArrivalListType", static_cast<int>(DepartureList)) );
    settings.drawShadows = cg.readEntry( "drawShadows", true );
    settings.showHeader = cg.readEntry( "showHeader", true );
    settings.hideColumnTarget = cg.readEntry( "hideColumnTarget", false );

    QString fontFamily = cg.readEntry( "fontFamily", QString() );
    settings.useDefaultFont = fontFamily.isEmpty();
    if ( !settings.useDefaultFont )
	settings.font = QFont( fontFamily );
    else
	settings.font = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );

    QStringList filterConfigurationList =
	    cgGlobal.readEntry( "filterConfigurationList", QStringList() << "Default");
    for ( int i = filterConfigurationList.count() - 1; i >= 0; --i ) {
	const QString &filterConfiguration = filterConfigurationList[ i ];
	if ( filterConfiguration.isEmpty() )
	    filterConfigurationList.removeAt( i );
    }
    // Make sure, "Default" is the first
    filterConfigurationList.removeOne( "Default" );
    filterConfigurationList.prepend( "Default" );

    kDebug() << "Group list" << cgGlobal.groupList();
    kDebug() << "Filter Config List:" << filterConfigurationList;
    // Delete old filter configs
    foreach ( const QString &group, cgGlobal.groupList() ) {
	if ( !filterConfigurationList.contains(
		    group.mid(QString("filterConfig_").length())) ) {
	    kDebug() << "Delete old group" << group;
	    cgGlobal.deleteGroup( group );
	}
    }

    settings.filterSettings.clear();
    foreach ( const QString &filterConfiguration, filterConfigurationList ) {
	settings.filterSettings[ filterConfiguration ] =
		readFilterConfig( cgGlobal.group("filterConfig_" + filterConfiguration) );
    }
    settings.filtersEnabled = cg.readEntry( "filtersEnabled", false );

    // Read alarm settings
    int alarmCount = cg.readEntry( "alarmCount", 0 );
    test = "alarmType";
    i = 1;
    while ( cgGlobal.hasKey(test) ) {
	AlarmSettings alarmSettings;
	QString suffix = i == 1 ? QString() : "_" + QString::number( i );
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
	if ( i > alarmCount )
	    break;
    }
	
    return settings;
}

void SettingsIO::writeNoGuiSettings( const Settings& settings, KConfigGroup cg,
				     KConfigGroup cgGlobal ) {
    cg.writeEntry( "currentStopIndex", settings.currentStopSettingsIndex );
    cgGlobal.writeEntry( "recentJourneySearches", settings.recentJourneySearches );
}

SettingsIO::ChangedFlags SettingsIO::writeSettings( const Settings &settings,
	    const Settings &oldSettings, KConfigGroup cg, KConfigGroup cgGlobal ) {
    ChangedFlags changed = NothingChanged;

    // Write stop settings
    if ( settings.stopSettingsList != oldSettings.stopSettingsList ) {
	changed |= IsChanged | ChangedStopSettings;
	int i = 1;
	cgGlobal.writeEntry( "stopSettings", settings.stopSettingsList.count() ); // Not needed if deleteEntry/Group works, don't know what's wrong (sync() and Plasma::Applet::configNeedsSaving() doesn't help)
	foreach ( StopSettings stopSettings, settings.stopSettingsList ) {
	    QString suffix = i == 1 ? QString() : "_" + QString::number( i );
	    cgGlobal.writeEntry( "location" + suffix, stopSettings.location );
	    cgGlobal.writeEntry( "serviceProvider" + suffix, stopSettings.serviceProviderID );
	    cgGlobal.writeEntry( "city" + suffix, stopSettings.city );
	    cgGlobal.writeEntry( "stop" + suffix, stopSettings.stops );
	    cgGlobal.writeEntry( "stopID" + suffix, stopSettings.stopIDs );
	    
	    cgGlobal.writeEntry( "filterConfiguration" + suffix,
				 stopSettings.filterConfiguration );
	    cgGlobal.writeEntry( "timeOffsetOfFirstDeparture" + suffix,
				 stopSettings.timeOffsetOfFirstDeparture );
	    cgGlobal.writeEntry( "timeOfFirstDepartureCustom" + suffix,
				 stopSettings.timeOfFirstDepartureCustom.toString("hh:mm") );
	    cgGlobal.writeEntry( "firstDepartureConfigMode" + suffix,
				 static_cast<int>(stopSettings.firstDepartureConfigMode) );
	    cgGlobal.writeEntry( "alarmTime" + suffix, stopSettings.alarmTime );
	    ++i;
	}

	// Delete old stop settings entries
	QString test = "location_" + QString::number( i );
	while ( cgGlobal.hasKey(test) ) {
	    QString suffix = "_" + QString::number( i );
	    cgGlobal.deleteEntry( "location" + suffix );
	    cgGlobal.deleteEntry( "serviceProvider" + suffix );
	    cgGlobal.deleteEntry( "city" + suffix );
	    cgGlobal.deleteEntry( "stop" + suffix );
	    cgGlobal.deleteEntry( "stopID" + suffix );
	    cgGlobal.deleteEntry( "filterConfiguration" + suffix );
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
		|| (!settings.useDefaultFont && settings.font != oldSettings.font) ) {
	cg.writeEntry( "fontFamily", settings.useDefaultFont
			? QString() : settings.font.family() );
	changed |= IsChanged;
    }
    
    if ( settings.departureArrivalListType != oldSettings.departureArrivalListType ) {
	cg.writeEntry( "departureArrivalListType",
		       static_cast<int>(settings.departureArrivalListType) );
	changed |= IsChanged | ChangedServiceProvider | ChangedDepartureArrivalListType;
    }

    // Write filter settings
    if ( settings.filterSettings.keys() != oldSettings.filterSettings.keys() ) {
	cgGlobal.writeEntry( "filterConfigurationList", settings.filterSettings.keys() );
	changed |= IsChanged;
    }

    if ( settings.filterSettings != oldSettings.filterSettings ) {
	QHash< QString, FilterSettings >::const_iterator it;
	for ( it = settings.filterSettings.constBegin();
		it != settings.filterSettings.constEnd(); ++it ) {
	    QString currentFilterConfig = it.key();
	    if ( currentFilterConfig.isEmpty() ) {
		kDebug() << "Empty filter config name, can't write settings";
		continue;
	    }

	    FilterSettings currentfilterSettings = it.value();
	    if ( oldSettings.filterSettings.contains(currentFilterConfig) ) {
		if ( SettingsIO::writeFilterConfig(currentfilterSettings,
			oldSettings.filterSettings[currentFilterConfig],
			cgGlobal.group("filterConfig_" + currentFilterConfig)) ) {
		    changed |= IsChanged | ChangedFilterSettings;
		}
	    } else {
		SettingsIO::writeFilterConfig( currentfilterSettings,
			cgGlobal.group("filterConfig_" + currentFilterConfig) );
		changed |= IsChanged | ChangedFilterSettings;
	    }
	}
    }
    
    if ( settings.filtersEnabled != oldSettings.filtersEnabled ) {
	cg.writeEntry( "filtersEnabled", settings.filtersEnabled );
	changed |= IsChanged;
    }

    // Write alarm settingsAccepted
    if ( settings.alarmSettings != oldSettings.alarmSettings ) {
	changed |= IsChanged | ChangedAlarmSettings;
	int i = 1;
	cg.writeEntry( "alarmCount", settings.alarmSettings.count() );
	foreach ( AlarmSettings alarmSettings, settings.alarmSettings ) {
	    QString suffix = i == 1 ? QString() : "_" + QString::number( i );
	    cgGlobal.writeEntry( "alarmType" + suffix, static_cast<int>(alarmSettings.type) );
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
	while ( cgGlobal.hasKey(test) ) {
	    QString suffix = i == 1 ? QString() : "_" + QString::number( i );
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

FilterSettings SettingsIO::readFilterConfig( const KConfigGroup &cgGlobal ) {
    FilterSettings filterSettings;
    filterSettings.filterAction = static_cast< FilterAction >(
	    cgGlobal.readEntry("FilterAction", static_cast<int>(ShowMatching)) );
	    
    QByteArray baFilters = cgGlobal.readEntry( "Filters", QByteArray() );
    filterSettings.filters.fromData( baFilters );
    return filterSettings;
}

bool SettingsIO::writeFilterConfig( const FilterSettings &filterSettings,
				    const FilterSettings &oldFilterSettings,
				    KConfigGroup cgGlobal ) {
    bool changed = false;

    if ( filterSettings.filters != oldFilterSettings.filters ) {
	cgGlobal.writeEntry( "Filters", filterSettings.filters.toData() );
	changed = true;
    }

    if ( filterSettings.filterAction != oldFilterSettings.filterAction ) {
	cgGlobal.writeEntry( "FilterAction", static_cast<int>(filterSettings.filterAction) );
	changed = true;
    }

    return changed;
}

void SettingsIO::writeFilterConfig( const FilterSettings& filterSettings,
				    KConfigGroup cgGlobal ) {
    cgGlobal.writeEntry( "Filters", filterSettings.filters.toData() );
    cgGlobal.writeEntry( "FilterAction", static_cast<int>(filterSettings.filterAction) );
}

bool operator ==( const AlarmSettings &l, const AlarmSettings &r ) {
    return l.name == r.name && l.enabled == r.enabled && l.type == r.type
	    && l.affectedStops == r.affectedStops && l.filter == r.filter
	    && l.lastFired == r.lastFired;
};
