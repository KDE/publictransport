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

// Own includes
#include "settings.h"
#include "htmldelegate.h"
#include "filterwidget.h"
#include "stopwidget.h"
// #include "datasourcetester.h"
#include <QSignalMapper>


SettingsUiManager::SettingsUiManager( const Settings &settings,
	    Plasma::DataEngine* publicTransportEngine, Plasma::DataEngine* osmEngine,
	    Plasma::DataEngine* favIconEngine, Plasma::DataEngine* geolocationEngine,
	    KConfigDialog *parentDialog, DeletionPolicy deletionPolicy )
	    : QObject( parentDialog ), m_deletionPolicy(deletionPolicy),
// 	    m_dataSourceTester(new DataSourceTester("", publicTransportEngine, this)),
	    m_configDialog(parentDialog), m_modelServiceProvider(0),
	    m_modelLocations(0), m_stopListWidget(0), m_filterListWidget(0),
	    m_publicTransportEngine(publicTransportEngine), m_osmEngine(osmEngine),
	    m_favIconEngine(favIconEngine), m_geolocationEngine(geolocationEngine) {
    m_currentStopSettingsIndex = settings.currentStopSettingsIndex;
    m_recentJourneySearches = settings.recentJourneySearches;

    m_filterSettings = settings.filterSettings;

    QWidget *widgetStop = new QWidget;
    QWidget *widgetAdvanced = new QWidget;
    QWidget *widgetAppearance = new QWidget;
    QWidget *widgetFilter = new QWidget;
    m_ui.setupUi( widgetStop );
    m_uiAdvanced.setupUi( widgetAdvanced );
    m_uiAppearance.setupUi( widgetAppearance );
    m_uiFilter.setupUi( widgetFilter );

    KTabWidget *tabMain = new KTabWidget;
    tabMain->addTab( widgetStop, i18n("&Stop selection") );
    tabMain->addTab( widgetAdvanced, i18n("&Advanced") );

    m_configDialog->addPage( tabMain, i18n("General"), "public-transport-stop" );
    m_configDialog->addPage( widgetAppearance, i18n("Appearance"),
			     "package_settings_looknfeel" );
    m_configDialog->addPage( widgetFilter, i18n("Filter"), "view-filter" );

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
	     this, SLOT(stopSettingsChanged()) );
    connect( m_stopListWidget, SIGNAL(removed(QWidget*)),
	     this, SLOT(stopSettingsChanged()) );
    stopSettingsChanged();
    
    QVBoxLayout *lStop = new QVBoxLayout( m_ui.stopList );
    lStop->setContentsMargins( 0, 0, 0, 0 );
    lStop->addWidget( m_stopListWidget );
    connect( m_stopListWidget, SIGNAL(changed(int,StopSettings)),
	     this, SLOT(updateFilterInfoLabel()) );

    // Setup filter widgets
    m_filterListWidget = new FilterListWidget( m_uiFilter.filters );
    m_filterListWidget->setWhatsThis( i18n("<b>This shows the filters of the "
	    "selected filter configuration.</b><br>"
	    "Each filter configuration consists of a filter action and a list "
	    "of filters. Each filter contains a list of constraints.<br>"
	    "A filter matches, if all it's constraints match while a filter "
	    "configuration matches, if one of it's filters match.<br>"
	    "To use a filter configuration select it in the 'Filter Uses' tab. "
	    "Each stop settings can use another filter configuration.<br><br>"
	    "<b>Filter Types</b>"
	    "- <b>Vehicle:</b> Filters by vehicle types."
	    "- <b>Line String:</b> Filters by transport line strings."
	    "- <b>Line number:</b> Filters by transport line numbers."
	    "- <b>Target:</b> Filters by target/origin."
	    "- <b>Via:</b> Filters by intermediate stops."
	    "- <b>Delay:</b> Filters by delay.") );
    
    QVBoxLayout *l = new QVBoxLayout( m_uiFilter.filters );
    l->addWidget( m_filterListWidget );
    connect( m_filterListWidget, SIGNAL(changed()),
	     this, SLOT(setFilterConfigurationChanged()) );

    setValuesOfAdvancedConfig( settings );
    setValuesOfAppearanceConfig( settings );
    setValuesOfFilterConfig();
    m_uiFilter.enableFilters->setChecked( settings.filtersEnabled );

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
    if ( m_deletionPolicy == DeleteWhenFinished )
	deleteLater();
}

void SettingsUiManager::configAccepted() {
    emit settingsAccepted( settings() );
}

void SettingsUiManager::stopSettingsChanged() {
    // Delete old widgets
    if ( m_uiFilter.filterUsesArea->layout() ) {
	QWidgetList oldWidgets = m_uiFilter.filterUsesArea->findChildren< QWidget* >(
		QRegExp("*Uses*", Qt::CaseSensitive, QRegExp::Wildcard) );
	foreach ( QWidget *widget, oldWidgets )
	    delete widget;
	delete m_uiFilter.filterUsesArea->layout();
    }

    // Add new widgets
    QGridLayout *l = new QGridLayout( m_uiFilter.filterUsesArea );
    l->setContentsMargins( 0, 0, 0, 0 );

    QStringList filterConfigurations = m_filterSettings.keys();
    QStringList trFilterConfigurations;
    foreach ( const QString &filterConfig, filterConfigurations )
	trFilterConfigurations << translateKey( filterConfig );

    QSignalMapper *mapper = new QSignalMapper( m_uiFilter.filterUsesArea );
    int row = 0;
    StopSettingsList stopSettingsList = m_stopListWidget->stopSettingsList();
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
	cmbFilterConfigs->setObjectName( "lblUsesConfigs" + sRow );
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
		this, SLOT(stopSettingsChanged()) );
    disconnect( m_stopListWidget, SIGNAL(removed(QWidget*)),
		this, SLOT(stopSettingsChanged()) );
    
    int index = widget->objectName().toInt();
    StopSettingsList stopSettingsList = m_stopListWidget->stopSettingsList();
    if ( stopSettingsList.count() > index ) {
	stopSettingsList[ index ].filterConfiguration = untranslateKey(
		qobject_cast<KComboBox*>(widget)->currentText() );
	m_stopListWidget->setStopSettingsList( stopSettingsList );
    }
    
    connect( m_stopListWidget, SIGNAL(changed(int,StopSettings)),
	     this, SLOT(stopSettingsChanged()) );
    connect( m_stopListWidget, SIGNAL(added(QWidget*)),
	     this, SLOT(stopSettingsChanged()) );
    connect( m_stopListWidget, SIGNAL(removed(QWidget*)),
	     this, SLOT(stopSettingsChanged()) );
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
    
    m_uiAppearance.showHeader->setChecked( settings.showHeader );
    m_uiAppearance.showColumnTarget->setChecked( !settings.hideColumnTarget );
    m_uiAppearance.radioUseDefaultFont->setChecked( settings.useDefaultFont );
    m_uiAppearance.radioUseOtherFont->setChecked( !settings.useDefaultFont );
    m_uiAppearance.font->setCurrentFont( settings.font );
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
    int minWidgetCount = m_filterListWidget->minimumWidgetCount();
    int maxWidgetCount = m_filterListWidget->maximumWidgetCount();
    m_filterListWidget->setWidgetCountRange();
    m_filterListWidget->removeAllWidgets();
    
    // Setup FilterWidgets from m_filters
    foreach ( const Filter &filter, filterSettings.filters ) //m_settings.filters )
	m_filterListWidget->addFilter( filter );
    
    int added = m_filterListWidget->setWidgetCountRange( minWidgetCount, maxWidgetCount );
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

    QString highlightTextColor;
    QColor textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
    if ( qGray(textColor.rgb()) > 192 )
	highlightTextColor = "red";
    else
	highlightTextColor = "darkred";

    // Create location items
    foreach( QString country, uniqueCountries ) {
	QStandardItem *item;
	QString text, sortText, cssTitle;
	item = new QStandardItem;

	if ( country.compare("international", Qt::CaseInsensitive)  == 0 ) {
	    text = i18n("International");
	    sortText = "00000" + text;
	    cssTitle = QString("color: %1;").arg(highlightTextColor);
	    item->setIcon( Global::internationalIcon() );
	} else if ( country.compare("unknown", Qt::CaseInsensitive)  == 0 ) {
	    text = i18n("Unknown");
	    sortText = "00000" + text;
	    cssTitle = "color: darkgray;";
	    item->setIcon( KIcon("dialog-warning") );
	} else {
	    if ( KGlobal::locale()->allCountriesList().contains( country ) )
		text = KGlobal::locale()->countryCodeToName( country );
	    else
		text = country;
	    sortText = "11111" + text;
	    item->setIcon( Global::putIconIntoBiggerSizeIcon(KIcon(country), QSize(32, 23)) );
	}

	QString formattedText = QString( "<span style='%4'><b>%1</b></span> "
					 "<small>(<b>%2</b>)<br-wrap>%3</small>" )
		.arg( text )
		.arg( i18np("%1 accessor", "%1 accessors", countries.count(country)) )
		.arg( m_locationData[country].toHash()["description"].toString() )
		.arg( cssTitle );

	item->setText( text );
	item->setData( country, LocationCodeRole );
	item->setData( formattedText, HtmlDelegate::FormattedTextRole );
	item->setData( sortText, SortRole );
	item->setData( QStringList() << "raised" << "drawFrameForWholeRow",
		       HtmlDelegate::TextBackgroundRole );
	item->setData( 4, HtmlDelegate::LinesPerRowRole );

	m_modelLocations->appendRow( item );
    }

    // Append item to show all service providers
    QString sShowAll = i18n("Show all available service providers");
    QStandardItem *itemShowAll = new QStandardItem();
    itemShowAll->setData( "000000", SortRole );
    QString formattedText = QString( "<span style='color:%3;'><b>%1</b></span>"
				     "<br-wrap><small><b>%2</b></small>" )
	.arg( sShowAll )
	.arg( i18n("Total: ") + i18np("%1 accessor", "%1 accessors", countries.count()) )
	.arg( highlightTextColor );
    itemShowAll->setData( "showAll", LocationCodeRole );
    itemShowAll->setData( formattedText, HtmlDelegate::FormattedTextRole );
    itemShowAll->setText( sShowAll );
    itemShowAll->setData( QStringList() << "raised" << "drawFrameForWholeRow",
			  HtmlDelegate::TextBackgroundRole );
    itemShowAll->setData( 3, HtmlDelegate::LinesPerRowRole );
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
	formattedText = QString( "<span style='color:%3;'><b>%1</b></span><br-wrap><small>%2</small>" )
	    .arg( i18np("%1 accessor is errornous:", "%1 accessors are errornous:",
			errornousAccessorNames.count()) )
	    .arg( errorLines.join(",<br-wrap>") )
	    .arg( highlightTextColor );
	itemErrors->setData( formattedText, HtmlDelegate::FormattedTextRole );
	itemErrors->setData( QStringList() << "raised" << "drawFrameForWholeRow",
			     HtmlDelegate::TextBackgroundRole );
	itemErrors->setData( 1 + errornousAccessorNames.count(), HtmlDelegate::LinesPerRowRole );
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

	item->setData( QStringList() << "raised" << "drawFrameForWholeRow",
			HtmlDelegate::TextBackgroundRole );
	formattedText = QString( "<b>%1</b><br-wrap><small><b>Features:</b> %2</small>" )
	    .arg( serviceProviderName )
	    .arg( serviceProviderData["features"].toStringList().join(", ") );
	item->setData( 4, HtmlDelegate::LinesPerRowRole );
	item->setData( formattedText, HtmlDelegate::FormattedTextRole );
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
	m_modelServiceProvider->appendRow( item );

	// Request favicons
	QString favIconSource = serviceProviderData["url"].toString();
	m_favIconEngine->disconnectSource( favIconSource, this );
	m_favIconEngine->connectSource( favIconSource, this );
	m_favIconEngine->query( favIconSource );
    }
    m_modelServiceProvider->setSortRole( SortRole );
    m_modelServiceProvider->sort( 0 );

    // Add title items
    QStringList lastTitles;
    for( int row = 0; row < m_modelServiceProvider->rowCount(); ++row ) {
	QString locationCode = m_modelServiceProvider->item( row )->data( ServiceProviderDataRole ).toHash()["country"].toString();
	QString title;
	if ( locationCode == "international" )
	    title = i18n("International");
	else if ( locationCode == "unknown" )
	    title = i18n("Unknown");
	else
	    title = KGlobal::locale()->countryCodeToName( locationCode );

	if ( lastTitles.contains(title) )
	    continue;

	QStandardItem *itemTitle = new QStandardItem( title );
	QColor textColor = KColorScheme( QPalette::Active ).foreground().color();
	itemTitle->setData( QString("<span style='font-weight:bold;font-size:large;text-decoration:underline;color:rgb(%1,%2,%3);'>")
		.arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
		+ title + ":</span>", HtmlDelegate::FormattedTextRole );
	itemTitle->setData( 0, HtmlDelegate::GroupTitleRole );
	itemTitle->setData( 2, HtmlDelegate::LinesPerRowRole );
	itemTitle->setData( locationCode, LocationCodeRole );
	itemTitle->setSelectable( false );
	m_modelServiceProvider->insertRow( row, itemTitle );

	lastTitles << title;
	++row;
    }
}

void SettingsUiManager::dataUpdated( const QString& sourceName,
				     const Plasma::DataEngine::Data& data ) {
    if ( sourceName.contains(QRegExp("^http")) ) {
	// Favicon of a service provider arrived
	Q_ASSERT_X( m_modelServiceProvider, "SettingsUiManager::dataUpdated",
		    "No service provider model" );

	QPixmap favicon( QPixmap::fromImage(data["Icon"].value<QImage>()) );
	if ( !favicon.isNull() ) {
	    for ( int i = 0; i < m_modelServiceProvider->rowCount(); ++i ) {
		QHash< QString, QVariant > serviceProviderData
		    = m_modelServiceProvider->item(i)->data( ServiceProviderDataRole ).toHash();
		QString favIconSource = serviceProviderData["url"].toString();
		if ( favIconSource.compare( sourceName ) == 0 )
		    m_modelServiceProvider->item(i)->setIcon( KIcon(favicon) );
	    }
	}
	else
	    kDebug() << "Favicon is NULL for" << sourceName;

	m_favIconEngine->disconnectSource( sourceName, this );
    }
}

QString SettingsUiManager::translateKey( const QString& key ) {
    if ( key == "Default" )
	return i18n("Default");
//     else if ( key == "Default*" )
// 	return i18n("Default") + "*";
    else
	return key;
}

QString SettingsUiManager::untranslateKey( const QString& translatedKey ) {
    if ( translatedKey == i18n("Default") /*|| translatedKey == i18n("Default") + "*"*/ )
	return "Default";
//     else if ( translatedKey.endsWith("*") )
// 	return translatedKey.left( translatedKey.length() - 1 );
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
    
    if ( m_uiAdvanced.showArrivals->isChecked() )
	ret.departureArrivalListType = ArrivalList;
    else
	ret.departureArrivalListType = DepartureList;
    ret.autoUpdate = m_uiAdvanced.updateAutomatically->isChecked();
    ret.maximalNumberOfDepartures = m_uiAdvanced.maximalNumberOfDepartures->value();
//     ret.timeOffsetOfFirstDeparture = m_uiAdvanced.timeOfFirstDeparture->value();
//     ret.timeOfFirstDepartureCustom = m_uiAdvanced.timeOfFirstDepartureCustom->time();
//     ret.firstDepartureConfigMode = m_uiAdvanced.firstDepartureUseCurrentTime->isChecked()
// 					? RelativeToCurrentTime : AtCustomTime;
//     ret.alarmTime = m_uiAdvanced.alarmTime->value();
    
    ret.showRemainingMinutes = m_uiAppearance.cmbDepartureColumnInfos->currentIndex() != 1;
    ret.showDepartureTime = m_uiAppearance.cmbDepartureColumnInfos->currentIndex() <= 1;
    ret.displayTimeBold = m_uiAppearance.displayTimeBold->checkState() == Qt::Checked;
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
    filterSettings.filters = m_filterListWidget->filters();
    return filterSettings;
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
    bool deaultFilterConfig = filterConfiguration == "Default";
    m_uiFilter.removeFilterConfiguration->setDisabled( deaultFilterConfig );
    m_uiFilter.renameFilterConfiguration->setDisabled( deaultFilterConfig );

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



Settings::Settings() {
    currentStopSettingsIndex = 0;
    filtersEnabled = false;
}



Settings SettingsIO::readSettings( KConfigGroup cg, KConfigGroup cgGlobal ) {
    Settings settings;
    settings.autoUpdate = cg.readEntry( "autoUpdate", true );
    settings.showRemainingMinutes = cg.readEntry( "showRemainingMinutes", true );
    settings.showDepartureTime = cg.readEntry( "showDepartureTime", true );
    settings.displayTimeBold = cg.readEntry( "displayTimeBold", true );

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
    if ( settings.currentStopSettingsIndex < 0 )
	settings.currentStopSettingsIndex = 0; // For compatibility with versions < 0.7
	
    settings.recentJourneySearches = cgGlobal.readEntry( "recentJourneySearches", QStringList() );
	
    settings.maximalNumberOfDepartures = cg.readEntry( "maximalNumberOfDepartures", 20 );
    settings.linesPerRow = cg.readEntry( "linesPerRow", 2 );
    settings.size = cg.readEntry( "size", 2 );
    settings.sizeFactor = (settings.size + 3) * 0.2f;
    settings.departureArrivalListType = static_cast<DepartureArrivalListType>(
	    cg.readEntry("departureArrivalListType", static_cast<int>(DepartureList)) );
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
	changed |= IsChanged;
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

    if ( settings.filterSettings.keys() != oldSettings.filterSettings.keys() ) {
	cgGlobal.writeEntry( "filterConfigurationList", settings.filterSettings.keys() );
	changed |= IsChanged;
    }

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
    
    if ( settings.filtersEnabled != oldSettings.filtersEnabled ) {
	cg.writeEntry( "filtersEnabled", settings.filtersEnabled );
	changed |= IsChanged;
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

