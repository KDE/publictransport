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

// KDE includes
#include <KDebug>
#include <KLocale>
#include <KIconEffect>
#include <KNotification>
#include <KConfigDialog>
#include <KToolInvocation>
#include <KColorScheme>
#include <KSelectAction>
#include <KToggleAction>
#include <KLineEdit>
#include <KCompletion>
#include <KSqueezedTextLabel>
#include <KPushButton>
#include <KMenu>
#include <KMimeTypeTrader>
#include <KColorUtils>

// Plasma includes
#include <Plasma/IconWidget>
#include <Plasma/Label>
#include <Plasma/ToolButton>
#include <Plasma/LineEdit>
#include <Plasma/TreeView>
#include <Plasma/PushButton>
#include <Plasma/PaintUtils>
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>
#include <Plasma/ToolTipContent>
#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    #include <Plasma/Animation>
#endif

// Qt includes
#include <QPainter>
#include <QGraphicsView>
#include <QFontMetrics>
#include <QSizeF>
#include <QGraphicsLinearLayout>
#include <QGraphicsGridLayout>
#include <QGraphicsScene>
#include <QListWidget>
#include <QMenu>
#include <QTreeView>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QStyle>
#include <QScrollBar>
#include <QTimer>
#include <qmath.h>
#if QT_VERSION >= 0x040600
    #include <QSequentialAnimationGroup>
#endif

// Own includes
#include "publictransport.h"
#include "htmldelegate.h"
#include "settings.h"
#include "publictransporttreeview.h"
#include "departureprocessor.h"
#include "departuremodel.h"
#include "overlaywidget.h"
#include "journeysearchparser.h"
#include "journeysearchlineedit.h"

PublicTransport::PublicTransport( QObject *parent, const QVariantList &args )
	    : Plasma::PopupApplet(parent, args),
	    m_graphicsWidget(0), m_mainGraphicsWidget(0), m_oldItem(0), 
	    m_icon(0), m_iconClose(0), m_label(0), m_labelInfo(0), m_treeView(0), m_treeViewJourney(0),
	    m_journeySearch(0), m_listStopSuggestions(0), m_btnLastJourneySearches(0),
	    m_btnStartJourneySearch(0),
	    m_overlay(0), m_model(0), m_modelJourneys(0), m_departureProcessor(0) {
    m_currentMessage = MessageNone;

    m_journeySearchLastTextLength = 0;
    setBackgroundHints( StandardBackground ); //TranslucentBackground );
    setAspectRatioMode( Plasma::IgnoreAspectRatio );
    setHasConfigurationInterface( true );
    resize( 400, 300 );
}

PublicTransport::~PublicTransport() {
    if ( hasFailedToLaunch() ) {
        // Do some cleanup here
    } else {
	// Store header state
	KConfigGroup cg = config();
	cg.writeEntry( "headerState", m_treeView->nativeWidget()->header()->saveState() );

	delete m_label;
	delete m_labelInfo;
	delete m_icon;
	delete m_graphicsWidget;
    }
}

void PublicTransport::init() {
    checkNetworkStatus();
    m_settings = SettingsIO::readSettings( config(), globalConfig() );

    m_departureProcessor = new DepartureProcessor( this );
    connect( m_departureProcessor, SIGNAL(beginDepartureProcessing(QString)),
	     this, SLOT(beginDepartureProcessing(QString)) );
    connect( m_departureProcessor,
	     SIGNAL(departuresProcessed(QString,QList<DepartureInfo>,QUrl,QDateTime)),
	     this, SLOT(departuresProcessed(QString,QList<DepartureInfo>,QUrl,QDateTime)) );
    connect( m_departureProcessor, SIGNAL(beginJourneyProcessing(QString)),
	     this, SLOT(beginJourneyProcessing(QString)) );
    connect( m_departureProcessor,
	     SIGNAL(journeysProcessed(QString,QList<JourneyInfo>,QUrl,QDateTime)),
	     this, SLOT(journeysProcessed(QString,QList<JourneyInfo>,QUrl,QDateTime)) );
    connect( m_departureProcessor,
	     SIGNAL(departuresFiltered(QString,QList<DepartureInfo>,QList<DepartureInfo>,QList<DepartureInfo>)),
	     this, SLOT(departuresFiltered(QString,QList<DepartureInfo>,QList<DepartureInfo>,QList<DepartureInfo>)) );
    
    // Load stops/filters from non-global settings (they're stored in the global
    // config since version 0.7 beta 4)
    if ( m_settings.stopSettingsList.isEmpty() ) {
	kDebug() << "No global stop settings, storing non-global stop settings globally.";
	Settings newSettings = SettingsIO::readSettings( config(), config() );

	// Store stops/filters into the global settings
	SettingsIO::writeSettings( newSettings, m_settings, config(), globalConfig() );
	m_settings = newSettings;
    }

    if ( !m_settings.stopSettingsList.isEmpty() ) {
	m_currentServiceProviderFeatures =
		currentServiceProviderData()["features"].toStringList();
    }

    if ( QAction *runAction = action("run associated application") ) {
	runAction->setText( i18n("&Show in Web-Browser") );

	KService::Ptr offer = KMimeTypeTrader::self()->preferredService( "text/html" );
	if ( !offer.isNull() )
	    runAction->setIcon( KIcon(offer->icon()) );
    }

    createModels();
    graphicsWidget();
    createTooltip();
    createPopupIcon();

    setDepartureArrivalListType( m_settings.departureArrivalListType );
    addState( ShowingDepartureArrivalList );
    addState( WaitingForDepartureData );

    connect( this, SIGNAL(geometryChanged()), this, SLOT(geometryChanged()) );
    connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
    connect( this, SIGNAL(messageButtonPressed(MessageButton)),
	     this, SLOT(slotMessageButtonPressed(MessageButton)) );
    connect( Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()),
	     this, SLOT(themeChanged()) );
    emit settingsChanged();
    
    setupActions();
    reconnectSource();
}

PublicTransport::NetworkStatus PublicTransport::queryNetworkStatus() {
    NetworkStatus status = StatusUnavailable;
    const QStringList interfaces = dataEngine("network")->sources();
    if ( interfaces.isEmpty() )
	return StatusUnknown;
    
    foreach ( const QString &iface, interfaces ) {
	QString sStatus = dataEngine("network")->query(iface)["ConnectionStatus"].toString();
	if ( sStatus.isEmpty() )
	    return StatusUnknown;
	
	if ( sStatus == "Activated" ) {
	    status = StatusActivated;
	    break;
	} else if ( sStatus == "Configuring" )
	    status = StatusConfiguring;
    }
    
    return status;
}

bool PublicTransport::checkNetworkStatus() {
    NetworkStatus status = queryNetworkStatus();
    if ( status == StatusUnavailable ) {
	hideMessage();
	m_currentMessage = MessageError;
	showMessage( KIcon("dialog-error"),
		     i18n("No network connection. Press Ok to retry."), Plasma::ButtonOk );
	return false;
    } else if ( status == StatusConfiguring ) {
	hideMessage();
	m_currentMessage = MessageError;
	showMessage( KIcon("dialog-error"),
		     i18n("Network gets configured. Please wait..."), Plasma::ButtonOk );
	return false;
    } else if ( status == StatusActivated
		&& m_currentMessage == MessageError ) {
	hideMessage();
	m_currentMessage = MessageErrorResolved;
	showMessage( KIcon("task-complete"),
		     i18n("Network connection established."), Plasma::ButtonOk );
	return false;
    } else {
	kDebug() << "Unknown network status or no error message was shown" << status;
	return true;
    }
}

void PublicTransport::slotMessageButtonPressed( MessageButton button ) {
    switch ( button ) {
	case Plasma::ButtonOk:
	default:
	    if ( m_currentMessage == MessageError )
		reconnectSource();
	    hideMessage();
	    break;
    }
}

void PublicTransport::setupActions() {
    QAction *actionUpdate = new QAction( KIcon("view-refresh"),
					 i18n("&Update timetable"), this );
    connect( actionUpdate, SIGNAL(triggered()), this, SLOT(updateDataSource()) );
    addAction( "updateTimetable", actionUpdate );

    QAction *showActionButtons = new QAction( /*KIcon("system-run"),*/ // TODO: better icon
					      i18n("&Quick Actions"), this );
    connect( showActionButtons, SIGNAL(triggered()), this, SLOT(showActionButtons()) );
    addAction( "showActionButtons", showActionButtons );
    
    QAction *actionSetAlarmForDeparture = new QAction(
	    Global::makeOverlayIcon(KIcon("task-reminder"), "list-add"),
	    m_settings.departureArrivalListType == DepartureList
	    ? i18n("Set &Alarm for This Departure")
	    : i18n("Set &Alarm for This Arrival"), this );
    connect( actionSetAlarmForDeparture, SIGNAL(triggered()),
	     this, SLOT(setAlarmForDeparture()) );
    addAction( "setAlarmForDeparture", actionSetAlarmForDeparture );

    QAction *actionRemoveAlarmForDeparture = new QAction(
	    Global::makeOverlayIcon(KIcon("task-reminder"), "list-remove"),
	    m_settings.departureArrivalListType == DepartureList
	    ? i18n("Remove &Alarm for This Departure")
	    : i18n("Remove &Alarm for This Arrival"), this );
    connect( actionRemoveAlarmForDeparture, SIGNAL(triggered()),
	     this, SLOT(removeAlarmForDeparture()) );
    addAction( "removeAlarmForDeparture", actionRemoveAlarmForDeparture );

    QAction *actionSearchJourneys = new QAction( KIcon("edit-find"),
			    i18n("Search for &Journeys..."), this );
    connect( actionSearchJourneys, SIGNAL(triggered()),
	     this, SLOT(showJourneySearch()) );
    addAction( "searchJourneys", actionSearchJourneys );

    int iconExtend = 32;
    QAction *actionShowDepartures = new QAction(
		    Global::makeOverlayIcon(KIcon("public-transport-stop"),
		    QList<KIcon>() << KIcon("go-home") << KIcon("go-next"),
		    QSize(iconExtend / 2, iconExtend / 2), iconExtend),
		    i18n("Show &Departures"), this );
    connect( actionShowDepartures, SIGNAL(triggered()),
	     this, SLOT(setShowDepartures()) );
    addAction( "showDepartures", actionShowDepartures );

    QAction *actionShowArrivals = new QAction(
		    Global::makeOverlayIcon(KIcon("public-transport-stop"),
		    QList<KIcon>() << KIcon("go-next") << KIcon("go-home"),
		    QSize(iconExtend / 2, iconExtend / 2), iconExtend),
		    i18n("Show &Arrivals"), this );
    connect( actionShowArrivals, SIGNAL(triggered()),
	     this, SLOT(setShowArrivals()) );
    addAction( "showArrivals", actionShowArrivals );
    
    QAction *actionBackToDepartures = new QAction( KIcon("go-previous"),
		    i18n("Back to &Departure List"), this );
    connect( actionBackToDepartures, SIGNAL(triggered()),
	     this, SLOT(goBackToDepartures()) );
    addAction( "backToDepartures", actionBackToDepartures );
	
    KToggleAction *actionEnableFilters = new KToggleAction( i18n("&Enable Filters"), this );
    connect( actionEnableFilters, SIGNAL(toggled(bool)), this, SLOT(setFiltersEnabled(bool)) );
    addAction( "enableFilters", actionEnableFilters );

    m_filtersGroup = new QActionGroup( this );
    m_filtersGroup->setExclusive( true );
    connect( m_filtersGroup, SIGNAL(triggered(QAction*)),
	     this, SLOT(switchFilterConfiguration(QAction*)) );
    
    KAction *actionFilterConfiguration = new KSelectAction( KIcon("view-filter"),
	    i18n("Filter"), this );
    KMenu *menu = new KMenu;
    menu->addAction( actionEnableFilters );
    menu->addSeparator();
    menu->addTitle( KIcon("view-filter"), i18n("Used Filter Configuration") );
    actionFilterConfiguration->setMenu( menu ); // TODO: Does this take ownership of menu?
    actionFilterConfiguration->setEnabled( true );
    addAction( "filterConfiguration", actionFilterConfiguration );

    QAction *actionToggleExpanded = new QAction( KIcon("arrow-down"),
				i18n("&Show additional information"), this );
    connect( actionToggleExpanded, SIGNAL(triggered()),
	     this, SLOT(toggleExpanded()) );
    addAction( "toggleExpanded", actionToggleExpanded );

    QAction *actionHideHeader = new QAction( KIcon("edit-delete"),
					     i18n("&Hide header"), this );
    connect( actionHideHeader, SIGNAL(triggered()), this, SLOT(hideHeader()) );
    addAction( "hideHeader", actionHideHeader );

    QAction *actionShowHeader = new QAction( KIcon("list-add"),
					     i18n("Show &header"), this );
    connect( actionShowHeader, SIGNAL(triggered()), this, SLOT(showHeader()) );
    addAction( "showHeader", actionShowHeader );

    QAction *actionHideColumnTarget = new QAction( KIcon("view-right-close"),
					i18n("Hide &target column"), this );
    connect( actionHideColumnTarget, SIGNAL(triggered()),
	     this, SLOT(hideColumnTarget()) );
    addAction( "hideColumnTarget", actionHideColumnTarget );

    QAction *actionShowColumnTarget = new QAction( KIcon("view-right-new"),
					i18n("Show &target column"), this );
    connect( actionShowColumnTarget, SIGNAL(triggered()),
	     this, SLOT(showColumnTarget()) );
    addAction( "showColumnTarget", actionShowColumnTarget );
}

QList< QAction* > PublicTransport::contextualActions() {
    QAction *switchDepArr = m_settings.departureArrivalListType == DepartureList
	    ? action("showArrivals") : action("showDepartures");

    KAction *actionFilter = NULL;
    QStringList filterConfigurationList = m_settings.filterSettings.keys();
    if ( !filterConfigurationList.isEmpty() ) {
	actionFilter = qobject_cast< KAction* >( action("filterConfiguration") );
	action("enableFilters")->setChecked( m_settings.filtersEnabled ); // TODO change checked state when filtersEnabled changes?
	QList< QAction* > oldActions = m_filtersGroup->actions();
	foreach ( QAction *oldAction, oldActions ) {
	    m_filtersGroup->removeAction( oldAction );
	    delete oldAction;
	}
	
	QMenu *menu = actionFilter->menu();
	QString currentFilterConfig = m_settings.currentStopSettings().filterConfiguration;
	foreach ( QString filterConfig, filterConfigurationList ) {
	    QAction *action = new QAction( SettingsUiManager::translateKey(filterConfig), m_filtersGroup );
	    action->setCheckable( true );
	    menu->addAction( action );
	    if ( filterConfig == currentFilterConfig )
		action->setChecked( true );
	}
	m_filtersGroup->setEnabled( m_settings.filtersEnabled );
    }

    QList< QAction* > actions;
    actions << action("updateTimetable"); //<< action("showActionButtons")
    if ( m_currentServiceProviderFeatures.contains("JourneySearch") )
	actions << action("searchJourneys");
    
    QAction *separator = new QAction( this );
    separator->setSeparator( true );
    actions.append( separator );
    
    if ( m_currentServiceProviderFeatures.contains("Arrivals") )
	actions << switchDepArr;
    if ( m_settings.stopSettingsList.count() > 1 )
	actions << switchStopAction( this );
    if ( actionFilter )
	actions << actionFilter;

    separator = new QAction( this );
    separator->setSeparator( true );
    actions.append( separator );
    
    return actions;
}

QVariantHash PublicTransport::serviceProviderData( const QString& id ) const {
    Plasma::DataEngine::Data serviceProviderData =
	    dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString serviceProviderName, serviceProviderData.keys() )  {
	QVariantHash data = serviceProviderData.value( serviceProviderName ).toHash();
	if ( data["id"] == id )
	    return data;
    }

    kDebug() << "Service provider data for" << id << "not found";
    return QVariantHash();
}

void PublicTransport::updateDataSource() {
    if ( testState(ShowingDepartureArrivalList) )
	reconnectSource();
    else
	reconnectJourneySource();
}

void PublicTransport::disconnectJourneySource() {
    if ( !m_currentJourneySource.isEmpty() ) {
	kDebug() << "Disconnect journey data source" << m_currentJourneySource;
	dataEngine("publictransport")->disconnectSource( m_currentJourneySource, this );
    }
}

void PublicTransport::reconnectJourneySource( const QString& targetStopName,
					      const QDateTime& dateTime,
					      bool stopIsTarget, bool timeIsDeparture,
					      bool requestStopSuggestions ) {
    disconnectJourneySource();

    QString _targetStopName = targetStopName;
    QDateTime _dateTime = dateTime;
    if ( _targetStopName.isEmpty() ) {
	if ( m_lastSecondStopName.isEmpty() )
	    return;
	_targetStopName = m_lastSecondStopName;
	_dateTime = m_lastJourneyDateTime;
    }

    if ( requestStopSuggestions ) {
	m_currentJourneySource = QString( "Stops %1|stop=%2" )
		.arg( m_settings.currentStopSettings().serviceProviderID )
		.arg( _targetStopName );
    } else {
	m_currentJourneySource = QString( stopIsTarget
		? "%6 %1|originStop=%2|targetStop=%3|maxDeps=%4|datetime=%5"
		: "%6 %1|originStop=%3|targetStop=%2|maxDeps=%4|datetime=%5" )
		.arg( m_settings.currentStopSettings().serviceProviderID )
		.arg( m_settings.currentStopSettings().stopOrStopId(0) )
		.arg( _targetStopName )
		.arg( m_settings.maximalNumberOfDepartures )
		.arg( _dateTime.toString() )
		.arg( timeIsDeparture ? "Journeys" : "JourneysArr" );
	QString currentStop = m_settings.currentStopSettings().stops.first();
	m_journeyTitleText = stopIsTarget
		? i18n("From %1<br>to <b>%2</b>", currentStop, _targetStopName)
		: i18n("From <b>%1</b><br>to %2", _targetStopName, currentStop);
	if ( testState(ShowingJourneyList) )
	    m_label->setText( m_journeyTitleText );
    }
    
    if ( !m_settings.currentStopSettings().city.isEmpty() ) { //TODO CHECK useSeperateCityValue )
	m_currentJourneySource += QString("|city=%1").arg(
		m_settings.currentStopSettings().city );
    }

    kDebug() << "Connect journey data source" << m_currentJourneySource
	     << "Autoupdate" << m_settings.autoUpdate;
    m_lastSecondStopName = _targetStopName;
    addState( WaitingForJourneyData );
    dataEngine("publictransport")->connectSource( m_currentJourneySource, this );
}

void PublicTransport::disconnectSources() {
    if ( !m_currentSources.isEmpty() ) {
	foreach ( QString currentSource, m_currentSources ) {
	    kDebug() << "Disconnect data source" << currentSource;
	    dataEngine( "publictransport" )->disconnectSource( currentSource, this );
	}
	m_currentSources.clear();
    }
}

void PublicTransport::reconnectSource() {
    disconnectSources();
    
    // Get a list of stops (or stop IDs if available) which results are currently shown
    StopSettings curStopSettings = m_settings.currentStopSettings();
    QStringList stops = curStopSettings.stops;
    QStringList stopIDs = curStopSettings.stopIDs;
    if ( stopIDs.isEmpty() )
	stopIDs = stops;
    
    kDebug() << "Connect" << m_settings.currentStopSettingsIndex << stops;
    QStringList sources;
    m_stopIndexToSourceName.clear();
    for ( int i = 0; i < stops.count(); ++i ) {
	QString stopValue = stopIDs[i].isEmpty() ? stops[i] : stopIDs[i];
	QString currentSource = QString("%4 %1|stop=%2")
		.arg( m_settings.currentStopSettings().serviceProviderID )
		.arg( stopValue )
		.arg( m_settings.departureArrivalListType == ArrivalList
		    ? "Arrivals" : "Departures" );
	if ( curStopSettings.firstDepartureConfigMode == RelativeToCurrentTime ) {
	    currentSource += QString("|timeOffset=%1").arg(
		    curStopSettings.timeOffsetOfFirstDeparture );
	} else {
	    currentSource += QString("|time=%1").arg(
		    curStopSettings.timeOfFirstDepartureCustom.toString("hh:mm") );
	}
	if ( !curStopSettings.city.isEmpty() )
	    currentSource += QString("|city=%1").arg( curStopSettings.city );
	
	m_stopIndexToSourceName[ i ] = currentSource;
	sources << currentSource;
    }

    foreach ( QString currentSource, sources ) {
	kDebug() << "Connect data source" << currentSource
		 << "Autoupdate" << m_settings.autoUpdate;
	m_currentSources << currentSource;
	if ( m_settings.autoUpdate ) {
	    // Update once a minute to show updated duration times
	    dataEngine("publictransport")->connectSource( currentSource, this,
							  60000, Plasma::AlignToMinute );
	} else {
	    dataEngine("publictransport")->connectSource( currentSource, this );
	}
    }

    addState( WaitingForDepartureData );
}

void PublicTransport::departuresFiltered( const QString& sourceName,
					  const QList< DepartureInfo > &departures,
					  const QList< DepartureInfo > &newlyFiltered,
					  const QList< DepartureInfo > &newlyNotFiltered ) {
    if ( m_departureInfos.contains(sourceName) )
	m_departureInfos[ sourceName ] = departures;
    else
	kDebug() << "Source name not found" << sourceName << "in" << m_departureInfos.keys();
    
    kDebug() << "Remove" << newlyFiltered.count() << "previously unfiltered departures, if they are visible";
    foreach ( const DepartureInfo &departureInfo, newlyFiltered ) {
	int row = m_model->indexFromInfo( departureInfo ).row();
	if ( row == -1 ) {
	    kDebug() << "Didn't find departure" << departureInfo.lineString()
		     << departureInfo.target() << departureInfo.departure();
	} else
	    m_model->removeItem( m_model->itemFromInfo(departureInfo) );
    }
    
    kDebug() << "Add" << newlyNotFiltered.count() << "previously filtered departures";
    foreach ( const DepartureInfo &departureInfo, newlyNotFiltered )
	appendDeparture( departureInfo );

    int delta = m_model->rowCount() - m_settings.maximalNumberOfDepartures;
    if ( delta > 0 )
	m_model->removeRows( m_settings.maximalNumberOfDepartures, delta );
}

void PublicTransport::beginJourneyProcessing( const QString &/*sourceName*/ ) {
    m_journeyInfos.clear();
}

void PublicTransport::journeysProcessed( const QString &/*sourceName*/,
		const QList< JourneyInfo > &journeys, const QUrl &requestUrl,
		const QDateTime &/*lastUpdate*/ ) {
    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    setAssociatedApplicationUrls( associatedApplicationUrls() << requestUrl );
    #endif
    kDebug() << journeys.count() << "journeys received from thread";
    m_journeyInfos << journeys;
    
    // Fill the model with the received journeys
    fillModelJourney( journeys );
}

void PublicTransport::beginDepartureProcessing( const QString& sourceName ) {
    // Clear old departure / arrival list
    QString strippedSourceName = stripDateAndTimeValues( sourceName );
    m_departureInfos[ strippedSourceName ].clear();
}

void PublicTransport::departuresProcessed( const QString& sourceName,
	    const QList< DepartureInfo > &departures, const QUrl &requestUrl,
	    const QDateTime &lastUpdate ) {
    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    m_urlDeparturesArrivals = requestUrl;
    if ( testState(ShowingDepartureArrivalList) || testState(ShowingJourneySearch)
	    || testState(ShowingJourneysNotSupported) )
	setAssociatedApplicationUrls( KUrl::List() << requestUrl );
    #endif

    // Put departures into the cache
    const QString strippedSourceName = stripDateAndTimeValues( sourceName );
    m_departureInfos[ strippedSourceName ] << departures;

    // Remove config needed messages
    setConfigurationRequired( false );
    m_stopNameValid = true;

    // Update "last update" time
    if ( lastUpdate > m_lastSourceUpdate )
	m_lastSourceUpdate = lastUpdate;
    m_labelInfo->setText( infoText() );

    // Fill the model with the received departures
    fillModel( departures );

    // TODO: get total received departures count from thread and only create these at the end
    createTooltip();
    createPopupIcon();
}

bool PublicTransport::isTimeShown( const QDateTime& dateTime ) const {
    StopSettings curStopSettings = m_settings.currentStopSettings();
    return DepartureProcessor::isTimeShown( dateTime,
				    curStopSettings.firstDepartureConfigMode,
				    curStopSettings.timeOfFirstDepartureCustom,
				    curStopSettings.timeOffsetOfFirstDeparture );
}

QString PublicTransport::stripDateAndTimeValues( const QString& sourceName ) const {
    QString ret = sourceName;
    QRegExp rx( "(time=[^\\|]*|datetime=[^\\|]*)", Qt::CaseInsensitive );
    rx.setMinimal( true );
    ret.replace( rx, "" );
    return ret;
}

QList< DepartureInfo > PublicTransport::departureInfos() const {
    QList< DepartureInfo > ret;

    for ( int n = m_stopIndexToSourceName.count() - 1; n >= 0; --n ) {
	QString sourceName = stripDateAndTimeValues( m_stopIndexToSourceName[n] );
	if ( m_departureInfos.contains(sourceName) ) {
	    foreach ( const DepartureInfo &departureInfo, m_departureInfos[sourceName] ) {
		if ( !departureInfo.isFilteredOut() )
		    ret << departureInfo;
	    }
	}
    }

    qSort( ret.begin(), ret.end() );
    return ret.mid( 0, m_settings.maximalNumberOfDepartures );
}

void PublicTransport::clearDepartures() {
    m_departureInfos.clear(); // Clear data from data engine
    m_model->clear(); // Clear data to be displayed
}

void PublicTransport::clearJourneys() {
    m_journeyInfos.clear(); // Clear data from data engine
    m_modelJourneys->clear(); // Clear data to be displayed
}

void PublicTransport::handleDataError( const QString& /*sourceName*/,
				       const Plasma::DataEngine::Data& data ) {
    if ( data["parseMode"].toString() == "journeys" ) {
	addState( ReceivedErroneousJourneyData );

	#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	QUrl url = data["requestUrl"].toUrl();
	kDebug() << "Errorneous journey url" << url;
	m_urlJourneys = url;
	if ( testState(ShowingJourneyList) )
	    setAssociatedApplicationUrls( KUrl::List() << url );
	#endif
    } else if ( data["parseMode"].toString() == "departures" ) {
	addState( ReceivedErroneousDepartureData );
	m_stopNameValid = false;

	#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	QUrl url = data["requestUrl"].toUrl();
	kDebug() << "Errorneous departure/arrival url" << url;
	m_urlDeparturesArrivals = url;
	if ( testState(ShowingDepartureArrivalList) || testState(ShowingJourneySearch)
		|| testState(ShowingJourneysNotSupported) )
	    setAssociatedApplicationUrls( KUrl::List() << url );
	#endif

// 	    if ( testState(ServiceProviderSettingsJustChanged) ) {
	QString error = data["errorString"].toString();
	if ( error.isEmpty() ) {
	    if ( m_currentMessage == MessageNone ) {
		if ( m_settings.departureArrivalListType == DepartureList ) {
		    setConfigurationRequired( true, i18n("Error parsing "
			    "departure information or currently no departures") );
		} else {
		    setConfigurationRequired( true, i18n("Error parsing "
			    "arrival information or currently no arrivals") );
		}
	    }
	} else {
	    hideMessage();
	    m_currentMessage = MessageError;
	    if ( checkNetworkStatus() ) {
		showMessage( KIcon("dialog-error"),
			i18n("There was an error:\n%1\n\nThe server may be "
			"temporarily unavailable. Press Ok to retry", error),
			Plasma::ButtonOk );
	    }
	}
// 	    }
    }
}

void PublicTransport::processStopSuggestions( const QString &/*sourceName*/,
					      const Plasma::DataEngine::Data &data ) {
    bool journeyData = data["parseMode"].toString() == "journeys";
    if ( journeyData || data.value("parseMode").toString() == "stopSuggestions" ) {
	if ( journeyData )
	    addState( ReceivedErroneousJourneyData );
	QVariantHash stopToStopID;
	QHash< QString, int > stopToStopWeight;
	QStringList stopSuggestions, weightedStops;

	int count = data["count"].toInt();
	bool hasAtLeastOneWeight = false;
	for( int i = 0; i < count; ++i ) {
	    if ( !data.contains( QString("stopName %1").arg(i) ) ) {
		kDebug() << "doesn't contain 'stopName" << i << "'! count ="
			    << count << "data =" << data;
		break;
	    }

	    QHash<QString, QVariant> dataMap = data.value(
		    QString("stopName %1").arg(i) ).toHash();
	    QString sStopName = dataMap["stopName"].toString();
	    QString sStopID = dataMap["stopID"].toString();
	    int stopWeight = dataMap["stopWeight"].toInt();
	    stopSuggestions << sStopName;
	    if ( stopWeight <= 0 )
		stopWeight = 0;
	    else
		hasAtLeastOneWeight = true;
	    weightedStops << QString( "%1:%2" ).arg( sStopName ).arg( stopWeight );

	    stopToStopID.insert( sStopName, sStopID );
	    stopToStopWeight.insert( sStopName, stopWeight );
	}

	if ( m_lettersAddedToJourneySearchLine
		    && m_journeySearch->nativeWidget()->completionMode() !=
		    KGlobalSettings::CompletionNone ) {
	    int posStart, len;
	    QString stopName;
	    JourneySearchParser::stopNamePosition( m_journeySearch->nativeWidget(),
						    &posStart, &len, &stopName );
	    int selStart = m_journeySearch->nativeWidget()->selectionStart();
	    if ( selStart == -1 )
		selStart = m_journeySearch->nativeWidget()->cursorPosition();
	    bool stopNameChanged = selStart > posStart
		    && selStart + m_journeySearch->nativeWidget()->selectedText().length()
		    <= posStart + len;
	    if ( stopNameChanged ) {
		KCompletion *comp = m_journeySearch->nativeWidget()->completionObject( false );
		comp->setIgnoreCase( true );
		if ( hasAtLeastOneWeight ) {
		    comp->setOrder( KCompletion::Weighted );
		    comp->setItems( weightedStops );
		} else
		    comp->setItems( stopSuggestions );
		QString completion = comp->makeCompletion( stopName );

		if ( completion != stopName )
		    JourneySearchParser::setJourneySearchStopNameCompletion(
			    m_journeySearch->nativeWidget(), completion );
	    }
	}

	QStandardItemModel *model = qobject_cast< QStandardItemModel* >(
					m_listStopSuggestions->model() );
	if ( model ) {
	    model->clear();
	} else {
	    model = new QStandardItemModel( m_listStopSuggestions );
	    m_listStopSuggestions->setModel( model );
	}

	// Add recent items
	foreach ( const QString &recent, m_settings.recentJourneySearches ) {
	    if ( !recent.contains(m_journeySearch->text()) )
		continue;
	    QStandardItem *item = new QStandardItem( KIcon("edit-find"),
						     i18n("<b>Recent:</b> %1", recent) );
	    item->setData( "recent", Qt::UserRole + 1 );
	    item->setData( recent, Qt::UserRole + 2 );
	    model->appendRow( item );
	}

	// Add stop suggestion items
	foreach( const QString &stop, stopSuggestions )
	    model->appendRow( new QStandardItem(KIcon("public-transport-stop"), stop) );

	// Add journey search string additions
	if ( !m_journeySearch->text().isEmpty() ) {
	    QStringList words = JourneySearchParser::notDoubleQuotedWords( m_journeySearch->text() );
	    const QString timeKeywordIn = JourneySearchParser::timeKeywordsIn().first();
	    const QString timeKeywordAt = JourneySearchParser::timeKeywordsAt().first();
	    const QString arrivalKeyword = JourneySearchParser::arrivalKeywords().first();
	    const QString departureKeyword = JourneySearchParser::departureKeywords().first();
	    const QString toKeyword = JourneySearchParser::toKeywords().first();
	    const QString fromKeyword = JourneySearchParser::fromKeywords().first();
	    
	    maybeAddKeywordAddRemoveItems( model, words,
		    QStringList() << timeKeywordAt << timeKeywordIn,
		    "additionalKeywordAtEnd",
		    QStringList() << i18n("Specify the departure/arrival time, eg. \"%1 12:00, 20.04.2010\"", timeKeywordAt)
			<< i18n("Specify the departure/arrival time, eg. \"%1 %2\"", timeKeywordIn, JourneySearchParser::relativeTimeString()),
		    QStringList() << "(\\d{2}:\\d{2}|\\d{2}\\.\\d{2}(\\.\\d{2,4}))"
			<< JourneySearchParser::relativeTimeString("\\d{1,}") );
	    maybeAddKeywordAddRemoveItems( model, words,
		    QStringList() << arrivalKeyword << departureKeyword,
		    "additionalKeywordAlmostAtEnd",
		    QStringList() << i18n("Get journeys departing at the given date/time")
			<< i18n("Get journeys arriving at the given date/time") );
	    maybeAddKeywordAddRemoveItems( model, words,
		    QStringList() << toKeyword << fromKeyword,
		    "additionalKeywordAtBegin",
		    QStringList() << i18n("Get journeys to the given stop")
			<< i18n("Get journeys from the given stop") );
	}
    } else if ( data["parseMode"].toString() == "departures" && m_currentMessage == MessageNone ) {
	m_stopNameValid = false;
	addState( ReceivedErroneousDepartureData );
	clearDepartures();
	setConfigurationRequired( true, i18n("The stop name is ambiguous.") );
    }
}

void PublicTransport::maybeAddAllKeywordAddRemoveitems( QStandardItemModel* model ) {
    if ( !model && !(model = qobject_cast<QStandardItemModel*>(m_listStopSuggestions->model())) )
	return;
    
    if ( !m_journeySearch->text().isEmpty() ) {
	QStringList words = JourneySearchParser::notDoubleQuotedWords( m_journeySearch->text() );
	QString timeKeywordIn, timeKeywordAt, arrivalKeyword, departureKeyword,
		toKeyword, fromKeyword;

	// Use the first keyword of each type for keyword suggestions
	if ( !JourneySearchParser::timeKeywordsIn().isEmpty() )
	    timeKeywordIn = JourneySearchParser::timeKeywordsIn().first();
	if ( !JourneySearchParser::timeKeywordsAt().isEmpty() )
	    timeKeywordAt = JourneySearchParser::timeKeywordsAt().first();
	if ( !JourneySearchParser::arrivalKeywords().isEmpty() )
	    arrivalKeyword = JourneySearchParser::arrivalKeywords().first();
	if ( !JourneySearchParser::departureKeywords().isEmpty() )
	    departureKeyword = JourneySearchParser::departureKeywords().first();
	if ( !JourneySearchParser::toKeywords().isEmpty() )
	    toKeyword = JourneySearchParser::toKeywords().first();
	if ( !JourneySearchParser::fromKeywords().isEmpty() )
	    fromKeyword = JourneySearchParser::fromKeywords().first();

	// Add keyword suggestions, ie. keyword add/remove items
	maybeAddKeywordAddRemoveItems( model, words,
		QStringList() << timeKeywordAt << timeKeywordIn,
		"additionalKeywordAtEnd",
		QStringList() << i18n("Specify the departure/arrival time, eg. "
				      "\"%1 12:00, 20.04.2010\"", timeKeywordAt)
		    << i18n("Specify the departure/arrival time, eg. \"%1 %2\"",
			    timeKeywordIn, JourneySearchParser::relativeTimeString()),
		QStringList() << "(\\d{2}:\\d{2}|\\d{2}\\.\\d{2}(\\.\\d{2,4}))"
		    << JourneySearchParser::relativeTimeString("\\d{1,}") );
	maybeAddKeywordAddRemoveItems( model, words,
		QStringList() << arrivalKeyword << departureKeyword,
		"additionalKeywordAtEnd",
		QStringList() << i18n("Get journeys departing at the given date/time")
		    << i18n("Get journeys arriving at the given date/time") );
	maybeAddKeywordAddRemoveItems( model, words,
		QStringList() << toKeyword << fromKeyword,
		"additionalKeywordAtBegin",
		QStringList() << i18n("Get journeys to the given stop")
		    << i18n("Get journeys from the given stop") );
    }

}

void PublicTransport::maybeAddKeywordAddRemoveItems( QStandardItemModel* model,
			const QStringList &words, const QStringList &keywords,
			const QString &type, const QStringList &descriptions,
			const QStringList &extraRegExps ) {
    bool added = false;
    QList<QStandardItem*> addItems;
    for ( int i = 0; i < keywords.count(); ++i ) {
	const QString keyword = keywords.at( i );
	QString description = descriptions.at( i );
	QString extraRegExp;
	if ( i < extraRegExps.count() )
	    extraRegExp = extraRegExps.at( i );
	
	QStandardItem *item;
	QColor keywordColor = KColorScheme( QPalette::Active )
		.foreground( KColorScheme::PositiveText ).color();
	if ( words.contains(keyword, Qt::CaseInsensitive) ) {
	    item = new QStandardItem( KIcon("list-remove"),
		    i18n("<b>Remove Keyword:</b> <span style='color=%3;'>%1</span><br>&nbsp;&nbsp;%2",
			 keyword, description, keywordColor.name()) );
	    item->setData( type + "Remove", Qt::UserRole + 1 );
	    if ( !extraRegExp.isNull() )
		item->setData( extraRegExp, Qt::UserRole + 3 );
	    model->appendRow( item );
	    added = true;
	} else {
	    item = new QStandardItem( KIcon("list-add"),
		    i18n("<b>Add Keyword:</b> <span style='color=%3;'>%1</span><br>&nbsp;&nbsp;%2",
			 keyword, description, keywordColor.name()) );
	    item->setData( type, Qt::UserRole + 1 );
	    addItems << item;
	}
	item->setData( keyword, Qt::UserRole + 2 );
	item->setData( 2, LinesPerRowRole );
    }

    if ( !added ) {
	foreach ( QStandardItem *item, addItems )
	    model->appendRow( item );
    }
}

void PublicTransport::dataUpdated( const QString& sourceName,
				   const Plasma::DataEngine::Data& data ) {
    if ( data.isEmpty() || (!m_currentSources.contains(sourceName)
		&& sourceName != m_currentJourneySource) ) {
	kDebug() << "Data discarded" << sourceName;
	return;
    }

    if ( data.value("error").toBool() ) {
	handleDataError( sourceName, data );
    } else if ( data.value("receivedPossibleStopList").toBool() ) { // Possible stop list received
	processStopSuggestions( sourceName, data );
    } else { // List of departures / arrivals / journeys received
	if ( data["parseMode"].toString() == "journeys" ) {
	    addState( ReceivedValidJourneyData );
	    if ( testState(ShowingJourneyList) )
		m_departureProcessor->processJourneys( sourceName, data );
	    else
		kDebug() << "Received journey data, but journey list is hidden.";
	} else if ( data["parseMode"].toString() == "departures" ) {
	    m_stopNameValid = true;
	    addState( ReceivedValidDepartureData );
	    m_departureProcessor->processDepartures( sourceName, data );
	}
    }

    removeState( SettingsJustChanged );
    removeState( ServiceProviderSettingsJustChanged );
}

void PublicTransport::geometryChanged() {
    setHeightOfCourtesyLabel();
}

void PublicTransport::popupEvent( bool show ) {
    destroyOverlay();
    addState( ShowingDepartureArrivalList ); // Hide opened journey views, ie. back to departure view

    Plasma::PopupApplet::popupEvent( show );
}

void PublicTransport::createPopupIcon() {
    QDateTime alarmTime = m_model->nextAlarmTime();
    if ( !alarmTime.isNull() ) {
	int minutesToAlarm = qCeil( QDateTime::currentDateTime().secsTo(alarmTime) / 60.0 );
	int hoursToAlarm = minutesToAlarm / 60;
	minutesToAlarm %= 60;
	QString text = i18nc( "This is displayed on the popup icon to indicate "
			      "the remaining time to the next alarm, %1=hours, "
			      "%2=minutes with padded 0", "%1:%2", hoursToAlarm,
			      QString("%1").arg(minutesToAlarm, 2, 10, QLatin1Char('0')) );

	QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DefaultFont);

	QPixmap pixmapIcon128 = KIcon( "public-transport-stop" ).pixmap(128);
	QPixmap pixmapAlarmIcon32 = KIcon( "task-reminder" ).pixmap(32);
	QPainter p128( &pixmapIcon128 );
	font.setPixelSize( 40 );
	p128.setFont( font );
	QPixmap shadowedText = Plasma::PaintUtils::shadowText( text, font );
	QSize sizeText( p128.fontMetrics().width(text), p128.fontMetrics().lineSpacing() );
	QRect rectText( QPoint(128 - 4 - sizeText.width(),128 - sizeText.height()), sizeText );
	QRect rectIcon( rectText.left() - 32 - 4,
			rectText.top() + (rectText.height() - 32) / 2, 32, 32 );
	p128.drawPixmap( rectIcon, pixmapAlarmIcon32 );
	p128.drawPixmap( rectText, shadowedText );
	p128.end();

	QPixmap pixmapIcon48 = KIcon( "public-transport-stop" ).pixmap(48);
	QPixmap pixmapAlarmIcon13 = KIcon("task-reminder").pixmap(13);
	QPainter p48( &pixmapIcon48 );
	font.setPixelSize( 18 );
	font.setBold( true );
	p48.setFont( font );
	shadowedText = Plasma::PaintUtils::shadowText( text, font );
	sizeText = QSize( p48.fontMetrics().width(text), p48.fontMetrics().lineSpacing() );
	rectText = QRect( QPoint(48 - sizeText.width(),48 - sizeText.height()), sizeText );
	rectIcon = QRect( rectText.left() - 11 - 1,
			  rectText.top() + (rectText.height() - 11) / 2, 13, 13 );
	rectText.adjust( 0, 2, 0, 2 );
	p48.drawPixmap( rectIcon, pixmapAlarmIcon13 );
	p48.drawPixmap( rectText, shadowedText );
	p48.end();

	KIcon icon = KIcon();
	icon.addPixmap( pixmapIcon128, QIcon::Normal );
	icon.addPixmap( pixmapIcon48, QIcon::Normal );

	setPopupIcon( icon );
    } else
	setPopupIcon( "public-transport-stop" );
}

void PublicTransport::createTooltip() {
    if ( isPopupShowing() || (formFactor() != Plasma::Horizontal
			    && formFactor() != Plasma::Vertical) )
	return;
    
    DepartureInfo nextDeparture;
    Plasma::ToolTipContent data;
    data.setMainText( i18n("Public transport") );
    if ( m_model->isEmpty() )
	data.setSubText( i18n("View departure times for public transport") );
    else if ( (nextDeparture = getFirstNotFilteredDeparture()).isValid() ) {
	if ( m_settings.departureArrivalListType ==  DepartureList ) {
	    if ( m_settings.currentStopSettings().stops.count() == 1 ) {
		data.setSubText( i18nc("%4 is the translated duration text, e.g. in 3 minutes",
				"Next departure from '%1': line %2 (%3) %4",
				m_settings.currentStopSettings().stops.first(),
				nextDeparture.lineString(), nextDeparture.target(),
				nextDeparture.durationString() ) );
	    } else {
		data.setSubText( i18nc("%3 is the translated duration text, e.g. in 3 minutes",
				 "Next departure from your home stop: line %1 (%2) %3",
				 nextDeparture.lineString(), nextDeparture.target(),
				 nextDeparture.durationString() ) );
	    }
	} else {
	    if ( m_settings.currentStopSettings().stops.count() == 1 ) {
		data.setSubText( i18nc("%4 is the translated duration text, e.g. in 3 minutes",
				 "Next arrival at '%1': line %2 (%3) %4",
				 m_settings.currentStopSettings().stops.first(),
				 nextDeparture.lineString(), nextDeparture.target(),
				 nextDeparture.durationString() ) );
	    } else {
		data.setSubText( i18nc("%3 is the translated duration text, e.g. in 3 minutes",
				 "Next arrival at your home stop: line %1 (%2) %3",
				 nextDeparture.lineString(), nextDeparture.target(),
				 nextDeparture.durationString() ) );
	    }
	}
    }
    
    data.setImage( KIcon("public-transport-stop").pixmap(IconSize(KIconLoader::Desktop)) );
    Plasma::ToolTipManager::self()->setContent( this, data );
}

DepartureInfo PublicTransport::getFirstNotFilteredDeparture() {
    QList<DepartureInfo> depInfos = departureInfos();
    return depInfos.isEmpty() ? DepartureInfo() : depInfos.first();
}

void PublicTransport::configChanged() {
    disconnect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
    addState( SettingsJustChanged );

    setDepartureArrivalListType( m_settings.departureArrivalListType );
    m_treeView->nativeWidget()->header()->setVisible( m_settings.showHeader );
    
    if ( testState(ShowingDepartureArrivalList) )
	m_treeView->nativeWidget()->setColumnHidden( ColumnTarget, m_settings.hideColumnTarget );
    
    QFont font = m_settings.sizedFont();
//     QFont font = m_settings.font;
//     float sizeFactor = m_settings.sizeFactor;
//     if ( font.pointSize() == -1 ) {
// 	int pixelSize = font.pixelSize() * sizeFactor;
// 	font.setPixelSize( pixelSize > 0 ? pixelSize : 1 );
//     } else {
// 	int pointSize = font.pointSize() * sizeFactor;
// 	font.setPointSize( pointSize > 0 ? pointSize : 1 );
//     }
    int smallPointSize = KGlobalSettings::smallestReadableFont().pointSize() * m_settings.sizeFactor;
    QFont smallFont = font, boldFont = font;
    smallFont.setPointSize( smallPointSize > 0 ? smallPointSize : 1 );
    boldFont.setBold( true );
    
    m_treeView->setFont( font );
    if ( m_treeViewJourney )
	m_treeViewJourney->setFont( font );
    m_label->setFont( boldFont );
    m_labelInfo->setFont( smallFont );
    setHeightOfCourtesyLabel();

    m_treeView->nativeWidget()->setIndentation( 20 * m_settings.sizeFactor );

    int iconExtend = (testState(ShowingDepartureArrivalList) ? 16 : 32) * m_settings.sizeFactor;
    m_treeView->nativeWidget()->setIconSize( QSize(iconExtend, iconExtend) );
    
    int mainIconExtend = 32 * m_settings.sizeFactor;
    m_icon->setMinimumSize( mainIconExtend, mainIconExtend );
    m_icon->setMaximumSize( mainIconExtend, mainIconExtend );
//     m_iconClose->setMinimumSize( mainIconExtend, mainIconExtend );
//     m_iconClose->setMaximumSize( mainIconExtend, mainIconExtend );
    
    if ( m_titleType == ShowDepartureArrivalListTitle )
	m_label->setText( titleText() );
    m_labelInfo->setToolTip( courtesyToolTip() );
    m_labelInfo->setText( infoText() );

    HtmlDelegate *htmlDelegate = dynamic_cast< HtmlDelegate* >( m_treeView->nativeWidget()->itemDelegate() );
    htmlDelegate->setOption( HtmlDelegate::DrawShadows, m_settings.drawShadows );

    m_departureProcessor->setFilterSettings( m_settings.currentFilterSettings(),
					     m_settings.filtersEnabled );
    StopSettings stopSettings = m_settings.currentStopSettings();
    m_departureProcessor->setFirstDepartureSettings(
					stopSettings.firstDepartureConfigMode,
					stopSettings.timeOfFirstDepartureCustom,
					stopSettings.timeOffsetOfFirstDeparture );
    m_departureProcessor->setAlarmSettings( m_settings.alarmSettings );

    m_model->setLinesPerRow( m_settings.linesPerRow );
    m_model->setSizeFactor( m_settings.sizeFactor );
    m_model->setDepartureColumnSettings( m_settings.displayTimeBold,
	    m_settings.showRemainingMinutes, m_settings.showDepartureTime );
    m_model->setAlarmSettings( m_settings.alarmSettings );
    m_model->setAlarmMinsBeforeDeparture( m_settings.currentStopSettings().alarmTime );

    if ( m_model->rowCount() > m_settings.maximalNumberOfDepartures ) {
	m_model->removeRows( m_settings.maximalNumberOfDepartures,
			     m_model->rowCount() - m_settings.maximalNumberOfDepartures );
    }
    
//     updateModelJourneys();

    connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
}

void PublicTransport::serviceProviderSettingsChanged() {
    addState( ServiceProviderSettingsJustChanged );
    if ( m_settings.checkConfig() ) {
	setConfigurationRequired( false );
	reconnectSource();

	if ( !m_currentJourneySource.isEmpty() )
	    reconnectJourneySource();
    } else {
	hideMessage();
	setConfigurationRequired( true, i18n("Please check your configuration.") );
    }
}

void PublicTransport::createModels() {
    m_model = new DepartureModel( this );
    connect( m_model, SIGNAL(alarmFired(DepartureItem*)),
	     this, SLOT(alarmFired(DepartureItem*)) );
    connect( m_model, SIGNAL(updateAlarms(AlarmSettingsList,QList<int>)),
	     this, SLOT(removeAlarms(AlarmSettingsList,QList<int>)) );

    m_modelJourneys = new JourneyModel( this );
}

void PublicTransport::setMainIconDisplay( MainIconDisplay mainIconDisplay ) {
    KIcon icon;
    KIconEffect iconEffect;
    QPixmap pixmap;

    int iconExtend = m_icon->size().width();
    
    // Make disabled icon
    switch ( mainIconDisplay ) {
	case DepartureListErrorIcon:
	    if ( m_settings.departureArrivalListType == DepartureList ) {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-home") << KIcon("go-next"),
			    QSize(iconExtend / 2, iconExtend / 2), iconExtend );
	    } else {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-next") << KIcon("go-home"),
			    QSize(iconExtend / 2, iconExtend / 2), iconExtend );
	    }
	    pixmap = icon.pixmap( iconExtend );
	    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::DisabledState );
	    icon = KIcon();
	    icon.addPixmap( pixmap, QIcon::Normal );
	    break;

	case DepartureListOkIcon:
	    if ( m_settings.departureArrivalListType == DepartureList ) {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-home") << KIcon("go-next"),
			    QSize(iconExtend / 2, iconExtend / 2), iconExtend );
	    } else {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-next") << KIcon("go-home"),
			    QSize(iconExtend / 2, iconExtend / 2), iconExtend );
	    }
	    break;

	case JourneyListOkIcon:
	    icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			QList<KIcon>() << KIcon("go-home")
			<< KIcon("go-next-view") << KIcon("public-transport-stop"),
			QSize(iconExtend / 3, iconExtend / 3), iconExtend );
	    break;

	case JourneyListErrorIcon:
	    icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			QList<KIcon>() << KIcon("go-home")
			<< KIcon("go-next-view") << KIcon("public-transport-stop"),
			QSize(iconExtend / 3, iconExtend / 3), iconExtend );
	    pixmap = icon.pixmap( iconExtend );
	    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::DisabledState );
	    icon = KIcon();
	    icon.addPixmap( pixmap, QIcon::Normal );
	    break;

	case AbortJourneySearchIcon:
	    icon = KIcon( "edit-delete" );
	    break;

	case GoBackIcon:
	    icon = KIcon( "arrow-left" );
	    break;
    }

    m_icon->setIcon( icon );
}

void PublicTransport::iconClicked() {
    switch( m_titleType ) {
	case ShowSearchJourneyLineEdit:
	case ShowSearchJourneyLineEditDisabled:
	    addState( ShowingDepartureArrivalList );
	    break;
	    
	case ShowJourneyListTitle:
	case ShowDepartureArrivalListTitle:
	    showActionButtons();
	    break;
    }
}

void PublicTransport::destroyOverlay() {
    if ( m_overlay ) {
	m_overlay->destroy();
	m_overlay = NULL;
    }
}

KSelectAction* PublicTransport::switchStopAction( QObject *parent,
					bool destroyOverlayOnTrigger ) const {
    KSelectAction *switchStopAction = new KSelectAction(
	    KIcon("public-transport-stop"), i18n("Switch Current Stop"), parent );
    for ( int i = 0; i < m_settings.stopSettingsList.count(); ++i ) {
	QString stopList = m_settings.stopSettingsList[ i ].stops.join( ",\n" );
	QString stopListShort = m_settings.stopSettingsList[ i ].stops.join( ", " );
	if ( stopListShort.length() > 30 )
	    stopListShort = stopListShort.left( 30 ).trimmed() + "...";
	QAction *action = new QAction( i18n("Show Results For '%1'", stopListShort),
				       parent );
	action->setToolTip( stopList );
	action->setData( i );
	if ( destroyOverlayOnTrigger )
	    connect( action, SIGNAL(triggered()), this, SLOT(destroyOverlay()) );

	action->setCheckable( true );
	action->setChecked( i == m_settings.currentStopSettingsIndex );
	switchStopAction->addAction( action );
    }

    connect( switchStopAction, SIGNAL(triggered(QAction*)),
	     this, SLOT(setCurrentStopIndex(QAction*)) );
    return switchStopAction;
}

void PublicTransport::showActionButtons() {
    m_overlay = new OverlayWidget( m_graphicsWidget, m_mainGraphicsWidget );
    m_overlay->setGeometry( m_graphicsWidget->contentsRect() );

    Plasma::PushButton *btnJourney = 0;
    if ( m_currentServiceProviderFeatures.contains("JourneySearch") ) {
	btnJourney = new Plasma::PushButton( m_overlay );
	btnJourney->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
	btnJourney->setAction( action("searchJourneys") );
	connect( btnJourney, SIGNAL(clicked()), this, SLOT(destroyOverlay()) );
    }
    
    Plasma::PushButton *btnBackToDepartures = 0;
    if ( testState(ShowingJourneyList) ) {
	btnBackToDepartures = new Plasma::PushButton( m_overlay );
	btnBackToDepartures->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
	btnBackToDepartures->setAction( updatedAction("backToDepartures") );
	connect( btnBackToDepartures, SIGNAL(clicked()), this, SLOT(destroyOverlay()) );
    }
	
    Plasma::PushButton *btnShowDepArr = 0;
    if ( m_currentServiceProviderFeatures.contains("Arrivals") ) {
	btnShowDepArr = new Plasma::PushButton( m_overlay );
	btnShowDepArr->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
	if ( m_settings.departureArrivalListType == DepartureList )
	    btnShowDepArr->setAction( action("showArrivals") );
	else
	    btnShowDepArr->setAction( action("showDepartures") );
	connect( btnShowDepArr, SIGNAL(clicked()), this, SLOT(destroyOverlay()) );
    }

    // Add stop selector if multiple stops are defined
    Plasma::PushButton *btnMultipleStops = NULL;
    if ( m_settings.stopSettingsList.count() > 1 ) {
	btnMultipleStops = new Plasma::PushButton( m_overlay );
	btnMultipleStops->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
	btnMultipleStops->setIcon( KIcon("public-transport-stop") );
	btnMultipleStops->setZValue( 1000 );
	btnMultipleStops->setText( i18n("Switch Current Stop") );
	
	QMenu *menu = new QMenu( btnMultipleStops->nativeWidget() );
	QList< QAction* > actionList =
		switchStopAction(btnMultipleStops->nativeWidget(), true)->actions();
	menu->addActions( actionList );
	btnMultipleStops->nativeWidget()->setMenu( menu );
    }
    
    Plasma::PushButton *btnCancel = new Plasma::PushButton( m_overlay );
    btnCancel->setText( i18n("Cancel") );
    btnCancel->setIcon( KIcon("dialog-cancel") );
    btnCancel->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
    connect( btnCancel, SIGNAL(clicked()), this, SLOT(destroyOverlay()) );

    QGraphicsWidget *spacer = new QGraphicsWidget( m_overlay );
    spacer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    QGraphicsWidget *spacer2 = new QGraphicsWidget( m_overlay );
    spacer2->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    // Create a seperate layout for the cancel button to have some more space 
    // between the cancel button and the others
    QGraphicsLinearLayout *layoutCancel = new QGraphicsLinearLayout( Qt::Vertical );
    layoutCancel->setContentsMargins( 0, 10, 0, 0 );
    layoutCancel->addItem( btnCancel );
    
    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout( Qt::Vertical );
    layout->setContentsMargins( 15, 10, 15, 10 );
    layout->addItem( spacer );
    if ( btnJourney ) {
	layout->addItem( btnJourney );
	layout->setAlignment( btnJourney, Qt::AlignCenter );
    }
    if ( btnBackToDepartures ) {
	layout->addItem( btnBackToDepartures );
	layout->setAlignment( btnBackToDepartures, Qt::AlignCenter );
    }
    if ( btnShowDepArr ) {
	layout->addItem( btnShowDepArr );
	layout->setAlignment( btnShowDepArr, Qt::AlignCenter );
    }
    if ( btnMultipleStops ) {
	layout->addItem( btnMultipleStops );
	layout->setAlignment( btnMultipleStops, Qt::AlignCenter );
    }
    layout->addItem( layoutCancel );
    layout->setAlignment( layoutCancel, Qt::AlignCenter );
    layout->addItem( spacer2 );
    m_overlay->setLayout( layout );

    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	m_overlay->setOpacity( 0 );
	Plasma::Animation *fadeAnimOverlay = Global::fadeAnimation( m_overlay, 1 );

	Plasma::Animation *fadeAnim1 = NULL, *fadeAnim2 = NULL, *fadeAnim3 = NULL,
		*fadeAnim4 = NULL, *fadeAnim5 = Global::fadeAnimation(btnCancel, 1);
	if ( btnJourney ) {
	    btnJourney->setOpacity( 0 );
	    fadeAnim1 = Global::fadeAnimation( btnJourney, 1 );
	}
	if ( btnShowDepArr ) {
	    btnShowDepArr->setOpacity( 0 );
	    fadeAnim2 = Global::fadeAnimation( btnShowDepArr, 1 );
	}
	if ( btnBackToDepartures ) {
	    btnBackToDepartures->setOpacity( 0 );
	    fadeAnim3 = Global::fadeAnimation( btnBackToDepartures, 1 );
	}
	if ( btnMultipleStops ) {
	    btnMultipleStops->setOpacity( 0 );
	    fadeAnim4 = Global::fadeAnimation( btnMultipleStops, 1 );
	}
	btnCancel->setOpacity( 0 );

	QSequentialAnimationGroup *seqGroup = new QSequentialAnimationGroup;
	if ( fadeAnimOverlay )
	    seqGroup->addAnimation( fadeAnimOverlay );
	if ( fadeAnim1 ) {
	    fadeAnim1->setProperty( "duration", 150 );
	    seqGroup->addAnimation( fadeAnim1 );
	}
	if ( fadeAnim2 ) {
	    fadeAnim2->setProperty( "duration", 150 );
	    seqGroup->addAnimation( fadeAnim2 );
	}
	if ( fadeAnim3 ) {
	    fadeAnim3->setProperty( "duration", 150 );
	    seqGroup->addAnimation( fadeAnim3 );
	}
	if ( fadeAnim4 ) {
	    fadeAnim4->setProperty( "duration", 150 );
	    seqGroup->addAnimation( fadeAnim4 );
	}
	if ( fadeAnim5 ) {
	    fadeAnim5->setProperty( "duration", 150 );
	    seqGroup->addAnimation( fadeAnim5 );
	}
	seqGroup->start( QAbstractAnimation::DeleteWhenStopped );
    #endif
}

void PublicTransport::setCurrentStopIndex( QAction* action ) {
    bool ok;
    int stopIndex = action->data().toInt( &ok );
    if ( !ok ) {
	kDebug() << "Couldn't find stop index";
	return;
    }

    disconnectSources();
    m_settings.currentStopSettingsIndex = stopIndex;
    SettingsIO::writeNoGuiSettings( m_settings, config(), globalConfig() );
    m_currentServiceProviderFeatures =
	    currentServiceProviderData()["features"].toStringList();
    clearDepartures();
    reconnectSource();
    configChanged();
}

void PublicTransport::setShowDepartures() {
    m_settings.departureArrivalListType = DepartureList;
    setDepartureArrivalListType( DepartureList );
    serviceProviderSettingsChanged(); // TODO: is this needed?
}

void PublicTransport::setShowArrivals() {
    m_settings.departureArrivalListType = ArrivalList;
    setDepartureArrivalListType( ArrivalList );
    serviceProviderSettingsChanged(); // TODO: is this needed?
}

void PublicTransport::switchFilterConfiguration( QAction* action ) {
    switchFilterConfiguration( KGlobal::locale()->removeAcceleratorMarker(action->text()) );
}

void PublicTransport::switchFilterConfiguration( const QString& newFilterConfiguration ) {
    kDebug() << "Switch filter configuration to" << newFilterConfiguration << m_settings.currentStopSettingsIndex;
    Settings oldSettings = m_settings;
    m_settings.currentStopSettings().filterConfiguration =
	    SettingsUiManager::untranslateKey(newFilterConfiguration);
    SettingsIO::ChangedFlags changed = SettingsIO::writeSettings(
			    m_settings, oldSettings, config(), globalConfig() );
    if ( changed.testFlag(SettingsIO::IsChanged) ) {
	configNeedsSaving();
	emit settingsChanged();

	for ( int n = 0; n < m_stopIndexToSourceName.count(); ++n ) {
	    QString sourceName = stripDateAndTimeValues( m_stopIndexToSourceName[n] );
	    m_departureProcessor->filterDepartures( sourceName,
			m_departureInfos[sourceName], m_model->itemHashes() );
	}
    }
}

void PublicTransport::setFiltersEnabled( bool enable ) {
    Settings oldSettings = m_settings;
    m_settings.filtersEnabled = enable;
    SettingsIO::ChangedFlags changed = SettingsIO::writeSettings(
			    m_settings, oldSettings, config(), globalConfig() );
    if ( changed.testFlag(SettingsIO::IsChanged) ) {
	configNeedsSaving();
	emit settingsChanged();
	
	for ( int n = 0; n < m_stopIndexToSourceName.count(); ++n ) {
	    QString sourceName = stripDateAndTimeValues( m_stopIndexToSourceName[n] );
	    m_departureProcessor->filterDepartures( sourceName,
			m_departureInfos[sourceName], m_model->itemHashes() );
	}
    }
}

void PublicTransport::iconCloseClicked() {
    addState( ShowingDepartureArrivalList );
}

void PublicTransport::journeySearchInputFinished() {
    clearJourneys();

    if ( !m_settings.recentJourneySearches.contains(m_journeySearch->text(), Qt::CaseInsensitive) )
	m_settings.recentJourneySearches.prepend( m_journeySearch->text() );
    while ( m_settings.recentJourneySearches.count() > MAX_RECENT_JOURNEY_SEARCHES )
	m_settings.recentJourneySearches.takeLast();
    
    SettingsIO::writeNoGuiSettings( m_settings, config(), globalConfig() );
    
    QString stop;
    QDateTime departure;
    bool stopIsTarget;
    bool timeIsDeparture;
    JourneySearchParser::parseJourneySearch( m_journeySearch->nativeWidget(),
					     m_journeySearch->text(), &stop, &departure,
					     &stopIsTarget, &timeIsDeparture );
    reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture );
    addState( ShowingJourneyList );
}

void PublicTransport::journeySearchInputEdited( const QString &newText ) {
    QString stop;
    QDateTime departure;
    bool stopIsTarget, timeIsDeparture;
    
    // Only correct the input string if letters were added (eg. not after pressing backspace).
    m_lettersAddedToJourneySearchLine = newText.length() > m_journeySearchLastTextLength;
    
    JourneySearchParser::parseJourneySearch( m_journeySearch->nativeWidget(),
			newText, &stop, &departure, &stopIsTarget, &timeIsDeparture,
			0, 0, m_lettersAddedToJourneySearchLine );
    m_journeySearchLastTextLength = m_journeySearch->text().length()
	    - m_journeySearch->nativeWidget()->selectedText().length();

    reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture, true );

    // Disable start search button if the journey search line is empty
    m_btnStartJourneySearch->setEnabled( !m_journeySearch->text().isEmpty() );
}

QGraphicsLayout *PublicTransport::createLayoutTitle( TitleType titleType ) {
    QGraphicsGridLayout *layoutTop = new QGraphicsGridLayout;
    switch( titleType ) {
	case ShowDepartureArrivalListTitle:
	    m_icon->setVisible( true );
	    m_label->setVisible( true );
	    m_labelInfo->setVisible( true );
	    layoutTop->addItem( m_icon, 0, 0 );
	    layoutTop->addItem( m_label, 0, 1 );
	    break;
	    
	case ShowSearchJourneyLineEdit:
	case ShowSearchJourneyLineEditDisabled:
	    m_icon->setVisible( true );
// 	    m_journeySearch->setVisible( true );
	    layoutTop->addItem( m_icon, 0, 0 );
	    layoutTop->addItem( m_journeySearch, 0, 1 );
	    layoutTop->addItem( m_btnLastJourneySearches, 0, 2 );
	    layoutTop->addItem( m_btnStartJourneySearch, 0, 3 );
	    break;

	case ShowJourneyListTitle:
	    m_icon->setVisible( true );
	    m_label->setVisible( true );
// 	    m_iconClose->setVisible( true );
	    layoutTop->addItem( m_icon, 0, 0 );
	    layoutTop->addItem( m_label, 0, 1 );
	    layoutTop->addItem( m_iconClose, 0, 2 );
	    break;
    }
    return layoutTop;
}

void PublicTransport::possibleStopItemActivated( const QModelIndex& modelIndex ) {
    if ( !modelIndex.data(Qt::UserRole + 1).isValid() )
	possibleStopClicked( modelIndex );
}

void PublicTransport::possibleStopClicked( const QModelIndex &modelIndex ) {
    QString type = modelIndex.data( Qt::UserRole + 1 ).toString();
    if ( type == "recent" ) {
	m_journeySearch->setText( modelIndex.data(Qt::UserRole + 2).toString() );
    } else if ( modelIndex.data(Qt::UserRole + 1).toString() == "additionalKeywordAtEnd" ) {
	m_journeySearch->setText( m_journeySearch->text() + " " +
				  modelIndex.data(Qt::UserRole + 2).toString() );
	m_listStopSuggestions->model()->removeRow( modelIndex.row() );
	journeySearchInputEdited( m_journeySearch->text() ); // for autocompletion / to add a remove keyword item
    } else if ( type == "additionalKeywordAlmostAtEnd" ) {
	int posStart, len;
	JourneySearchParser::stopNamePosition( m_journeySearch->nativeWidget(),
					       &posStart, &len );
	if ( posStart != -1 ) {
	    m_journeySearch->setText( m_journeySearch->text().insert(posStart + len,
		    " " + modelIndex.data(Qt::UserRole + 2).toString()) );
	} else {
	    m_journeySearch->setText( m_journeySearch->text() + " " +
		    modelIndex.data(Qt::UserRole + 2).toString() );
	}
	m_listStopSuggestions->model()->removeRow( modelIndex.row() );
	journeySearchInputEdited( m_journeySearch->text() ); // for autocompletion / add a remove keyword item
    } else if ( type == "additionalKeywordAtBegin" ) {
	m_journeySearch->setText( modelIndex.data(Qt::UserRole + 2).toString() +
				  " " + m_journeySearch->text() );
	m_listStopSuggestions->model()->removeRow( modelIndex.row() );
	journeySearchInputEdited( m_journeySearch->text() ); // to add a remove keyword item
    } else if ( type == "additionalKeywordAtEndRemove"
	     || type == "additionalKeywordAlmostAtEndRemove" )
    {
	QString keyword = modelIndex.data( Qt::UserRole + 2 ).toString();
	QString pattern;
	if ( modelIndex.data(Qt::UserRole + 3).isValid() ) {
	    QString extraRegExp = modelIndex.data( Qt::UserRole + 3 ).toString();
	    pattern = QString("(?:\"[^\"]*\".*)?(\\s%1\\s%2)").arg( keyword ).arg( extraRegExp );
	} else {
	    pattern = QString("(?:\"[^\"]*\".*)?(\\s%1)").arg( keyword );
	}
	QRegExp regExp( pattern, Qt::CaseInsensitive );
	regExp.setMinimal( true );
	int pos = regExp.lastIndexIn( m_journeySearch->text() );
	if ( pos != -1 ) {
	    m_journeySearch->setText( m_journeySearch->text().trimmed().remove(
		    regExp.pos(1), regExp.cap(1).length()) );
	    m_listStopSuggestions->model()->removeRow( modelIndex.row() );
	    journeySearchInputEdited( m_journeySearch->text() ); // for autocompletion
	}
    } else if ( type == "additionalKeywordAtBeginRemove" ) {
	QString keyword = modelIndex.data( Qt::UserRole + 2 ).toString();
	m_journeySearch->setText( m_journeySearch->text().trimmed().remove(
		QRegExp(QString("^%1\\s").arg(keyword), Qt::CaseInsensitive)) );
	m_listStopSuggestions->model()->removeRow( modelIndex.row() );
    } else {
	// Insert the clicked stop into the journey search line,
	// don't override keywords and other infos
	int posStart, len;
	JourneySearchParser::stopNamePosition( m_journeySearch->nativeWidget(), &posStart, &len );

	QString quotedStop = "\"" + modelIndex.data().toString() + "\"";
	if ( posStart == -1 ) {
	    m_journeySearch->setText( quotedStop );
	} else {
	    m_journeySearch->setText( m_journeySearch->text().replace(
		    posStart, len, quotedStop) );
	    m_journeySearch->nativeWidget()->setCursorPosition( posStart + quotedStop.length() );
	}
	journeySearchInputEdited( m_journeySearch->text() ); // for autocompletion
    }
    m_journeySearch->setFocus();
}

void PublicTransport::possibleStopDoubleClicked( const QModelIndex& modelIndex ) {
    Q_UNUSED( modelIndex );
    journeySearchInputFinished();
}

void PublicTransport::useCurrentPlasmaTheme() {
    if ( m_settings.useDefaultFont )
	configChanged();

    // Get theme colors
    QColor textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );

    // Create palette with the used theme colors
    QPalette p = palette();
    p.setColor( QPalette::Background, Qt::transparent );
    p.setColor( QPalette::Base, Qt::transparent );
    p.setColor( QPalette::Button, Qt::transparent );
    p.setColor( QPalette::Foreground, textColor );
    p.setColor( QPalette::Text, textColor );
    p.setColor( QPalette::ButtonText, textColor );
    
    QColor bgColor = KColorScheme( QPalette::Active )
	    .background( KColorScheme::AlternateBackground ).color();
    bgColor.setAlpha( bgColor.alpha() / 3 );
    QLinearGradient bgGradient( 0, 0, 1, 0 );
    bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
    bgGradient.setColorAt( 0, Qt::transparent );
    bgGradient.setColorAt( 0.3, bgColor );
    bgGradient.setColorAt( 0.7, bgColor );
    bgGradient.setColorAt( 1, Qt::transparent );
    QBrush brush( bgGradient );
    p.setBrush( QPalette::AlternateBase, brush );
    
    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->setPalette( p );
    treeView->header()->setPalette( p );

    // To set new text color of the header items
    setDepartureArrivalListType( m_settings.departureArrivalListType );
}

QGraphicsWidget* PublicTransport::graphicsWidget() {
    if ( !m_graphicsWidget ) {
	m_graphicsWidget = new QGraphicsWidget( this );
        m_graphicsWidget->setMinimumSize( 150, 150 );
        m_graphicsWidget->setPreferredSize( 400, 300 );
	
	// Create a child graphics widget, eg. to apply a blur effect to it
	// but not to an overlay widget (which then gets a child of m_graphicsWidget).
	m_mainGraphicsWidget = new QGraphicsWidget( m_graphicsWidget );
	m_mainGraphicsWidget->setSizePolicy( QSizePolicy::Expanding,
					     QSizePolicy::Expanding );
	QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout( Qt::Vertical );
	mainLayout->setContentsMargins( 0, 0, 0, 0 );
	mainLayout->addItem( m_mainGraphicsWidget );
	m_graphicsWidget->setLayout( mainLayout );

	int iconExtend = 32 * m_settings.sizeFactor;
	m_icon = new Plasma::IconWidget;
	m_icon->setIcon( "public-transport-stop" );
	m_icon->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
	m_icon->setMinimumSize( iconExtend, iconExtend );
	m_icon->setMaximumSize( iconExtend, iconExtend );
	connect( m_icon, SIGNAL(clicked()), this, SLOT(iconClicked()) );

	m_label = new Plasma::Label;
	m_label->setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
	QLabel *label = m_label->nativeWidget();
	label->setTextInteractionFlags( Qt::LinksAccessibleByMouse );

	m_labelInfo = new Plasma::Label;
	m_labelInfo->setAlignment( Qt::AlignVCenter | Qt::AlignRight );
	connect( m_labelInfo, SIGNAL(linkActivated(QString)),
		 this, SLOT(infoLabelLinkActivated(QString)) );
	QLabel *labelInfo = m_labelInfo->nativeWidget();
	labelInfo->setOpenExternalLinks( true );
	labelInfo->setWordWrap( true );
	setHeightOfCourtesyLabel();

	// Create treeview for departures / arrivals
	m_treeView = new Plasma::TreeView();
	initTreeView( m_treeView );
	m_treeView->setModel( m_model );

	QHeaderView *header = m_treeView->nativeWidget()->header();
	KConfigGroup cg = config();
	if ( cg.hasKey("headerState") ) {
	    // Restore header state
	    QByteArray headerState = cg.readEntry( "headerState", QByteArray() );
	    if ( !headerState.isNull() )
		header->restoreState( headerState );
	} else {
	    header->resizeSection( ColumnLineString, 60 );
	}
	header->setMovable( true );

	QGraphicsLinearLayout *layout = new QGraphicsLinearLayout( Qt::Vertical );
	layout->setContentsMargins( 0, 0, 0, 0 );
	layout->setSpacing( 0 );

	QGraphicsGridLayout *layoutTop = new QGraphicsGridLayout;
	layout->addItem( layoutTop );
	layout->addItem( m_treeView );
	layout->addItem( m_labelInfo );
	layout->setAlignment( m_labelInfo, Qt::AlignRight | Qt::AlignVCenter );
	m_mainGraphicsWidget->setLayout( layout );
	setTitleType( ShowDepartureArrivalListTitle );

	registerAsDragHandle( m_mainGraphicsWidget );
	registerAsDragHandle( m_label );

	// To make the link clickable (don't let plasma eat click events for dragging)
	m_labelInfo->installSceneEventFilter( this );

	useCurrentPlasmaTheme();
    }

    return m_graphicsWidget;
}

void PublicTransport::initTreeView( Plasma::TreeView* treeView ) {
    // Create treeview for departures / arrivals
    QTreeView *_treeView = new TreeView(
	    treeView->nativeWidget()->verticalScrollBar()->style() );
    treeView->setWidget( _treeView );
    _treeView->setAlternatingRowColors( true );
    _treeView->setAllColumnsShowFocus( true );
    _treeView->setRootIsDecorated( false );
    _treeView->setAnimated( true );
    _treeView->setSortingEnabled( true );
    _treeView->setWordWrap( true );
    _treeView->setExpandsOnDoubleClick( false );
    _treeView->setEditTriggers( QAbstractItemView::NoEditTriggers );
    _treeView->setSelectionMode( QAbstractItemView::NoSelection );
    _treeView->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
    _treeView->setContextMenuPolicy( Qt::CustomContextMenu );

    QHeaderView *header = new HeaderView( Qt::Horizontal );
    _treeView->setHeader( header );
//     header->setCascadingSectionResizes( true );
    header->setStretchLastSection( true ); // TODO check
    header->setResizeMode( QHeaderView::Interactive );
    header->setSortIndicator( 2, Qt::AscendingOrder );
    header->setContextMenuPolicy( Qt::CustomContextMenu );
    header->setMinimumSectionSize( 20 );
    header->setDefaultSectionSize( 60 );
    _treeView->setItemDelegate( new PublicTransportDelegate(_treeView) );

    connect( _treeView, SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(showDepartureContextMenu(const QPoint &)) );
    if ( KGlobalSettings::singleClick() ) {
	connect( _treeView, SIGNAL(clicked(const QModelIndex &)),
		    this, SLOT(doubleClickedDepartureItem(const QModelIndex &)) );
    } else {
	connect( _treeView, SIGNAL(doubleClicked(const QModelIndex &)),
		    this, SLOT(doubleClickedDepartureItem(const QModelIndex &)) );
    }
    connect( header, SIGNAL(sectionResized(int,int,int)),
	     this, SLOT(sectionResized(int,int,int)) );
    connect( header, SIGNAL(sectionPressed(int)), this, SLOT(sectionPressed(int)) );
    connect( header, SIGNAL(sectionMoved(int,int,int)),
	     this, SLOT(sectionMoved(int,int,int)) );
    connect( header, SIGNAL(customContextMenuRequested(const QPoint &)),
	     this, SLOT(showHeaderContextMenu(const QPoint &)) );

    _treeView->setIndentation( 20 * m_settings.sizeFactor );
    int iconExtend = (testState(ShowingDepartureArrivalList) ? 16 : 32) * m_settings.sizeFactor; // TODO
    _treeView->setIconSize( QSize(iconExtend, iconExtend) );
}

void PublicTransport::setHeightOfCourtesyLabel() {
    m_labelInfo->setText( infoText() );
}

void PublicTransport::infoLabelLinkActivated( const QString& link ) {
    KToolInvocation::invokeBrowser( link );
}

bool PublicTransport::sceneEventFilter( QGraphicsItem* watched, QEvent* event ) {
    if ( watched == m_labelInfo && event->type() == QEvent::GraphicsSceneMousePress )
	return true; // To make links clickable, otherwise Plasma takes all
		     // clicks to move the applet
    
    return Plasma::Applet::sceneEventFilter( watched, event );
}

bool PublicTransport::eventFilter( QObject *watched, QEvent *event ) {
    if ( watched && watched == m_journeySearch && m_listStopSuggestions
		&& m_listStopSuggestions->model()
		&& m_listStopSuggestions->model()->rowCount() > 0 ) {
	QKeyEvent *keyEvent;
	QModelIndex curIndex;
	int row;
	switch ( event->type() ) {
	    case QEvent::KeyPress:
		keyEvent = dynamic_cast<QKeyEvent*>( event );
		curIndex = m_listStopSuggestions->nativeWidget()->currentIndex();
		
		if ( keyEvent->key() == Qt::Key_Up ) {
		    if ( !curIndex.isValid() ) {
			curIndex = m_listStopSuggestions->nativeWidget()->model()->index( 0, 0 );
			m_listStopSuggestions->nativeWidget()->setCurrentIndex( curIndex );
			possibleStopItemActivated( curIndex );
			return true;
		    } else {
			row = curIndex.row();
			if ( row >= 1 ) {
			    m_listStopSuggestions->nativeWidget()->setCurrentIndex(
				    m_listStopSuggestions->model()->index(row - 1,
				    curIndex.column(), curIndex.parent()) );
			    possibleStopItemActivated( m_listStopSuggestions->nativeWidget()->currentIndex() );
			    return true;
			} else
			    return false;
		    }
		} else if ( keyEvent->key() == Qt::Key_Down ) {
		    if ( !curIndex.isValid() ) {
			curIndex = m_listStopSuggestions->nativeWidget()->model()->index( 0, 0 );
			m_listStopSuggestions->nativeWidget()->setCurrentIndex( curIndex );
			possibleStopItemActivated( curIndex );
			return true;
		    } else {
			row = curIndex.row();
			if ( row < m_listStopSuggestions->model()->rowCount() - 1 ) {
			    m_listStopSuggestions->nativeWidget()->setCurrentIndex(
				    m_listStopSuggestions->model()->index(row + 1,
				    curIndex.column(), curIndex.parent()) );
			    possibleStopItemActivated( m_listStopSuggestions->nativeWidget()->currentIndex() );
			    return true;
			} else
			    return false;
		    }
		}
		break;
	    
	    default:
		break;
	}
    }

    return Plasma::PopupApplet::eventFilter( watched, event );
}

void PublicTransport::createConfigurationInterface( KConfigDialog* parent ) {
    SettingsUiManager *settingsUiManager = new SettingsUiManager(
			    m_settings, dataEngine("publictransport"),
			    dataEngine("openstreetmap"), dataEngine("favicons"),
			    dataEngine("geolocation"), parent );
    connect( settingsUiManager, SIGNAL(settingsAccepted(Settings)),
	     this, SLOT(writeSettings(Settings)) );
    connect( m_model, SIGNAL(updateAlarms(AlarmSettingsList,QList<int>)),
	     settingsUiManager, SLOT(removeAlarms(AlarmSettingsList,QList<int>)) );
}

void PublicTransport::writeSettings( const Settings& settings ) {
    SettingsIO::ChangedFlags changed =
	    SettingsIO::writeSettings( settings, m_settings, config(), globalConfig() );
    
    if ( changed.testFlag(SettingsIO::IsChanged) ) {
	m_settings = settings;
	m_currentServiceProviderFeatures =
		currentServiceProviderData()["features"].toStringList();
	configNeedsSaving();
	emit settingsChanged();

	if ( changed.testFlag(SettingsIO::ChangedServiceProvider) )
	    serviceProviderSettingsChanged();
	if ( changed.testFlag(SettingsIO::ChangedDepartureArrivalListType) )
	    setDepartureArrivalListType( m_settings.departureArrivalListType );
	if ( changed.testFlag(SettingsIO::ChangedStopSettings) ) {
	    clearDepartures();
	    reconnectSource();
	} else if ( changed.testFlag(SettingsIO::ChangedFilterSettings) ) {
	    for ( int n = 0; n < m_stopIndexToSourceName.count(); ++n ) {
		QString sourceName = stripDateAndTimeValues( m_stopIndexToSourceName[n] );
		m_departureProcessor->filterDepartures( sourceName,
			m_departureInfos[sourceName], m_model->itemHashes() );
	    }
	} else if ( changed.testFlag(SettingsIO::ChangedLinesPerRow) ) {
	    // Refill model to recompute item sizehints
	    m_model->clear();
	    fillModel( departureInfos() );
	}
    } else
	kDebug() << "No changes made in the settings";
}

void PublicTransport::setDepartureArrivalListType(
		DepartureArrivalListType departureArrivalListType ) {
    m_model->setDepartureArrivalListType( departureArrivalListType );
}

void PublicTransport::sectionMoved( int logicalIndex, int oldVisualIndex,
				    int newVisualIndex ) {
    // Don't allow sections to be moved to visual index 0
    // because otherwise the child items aren't spanned...
    if ( newVisualIndex == 0 ) {
	m_treeView->nativeWidget()->header()->moveSection(
		m_treeView->nativeWidget()->header()->visualIndex(logicalIndex),
		oldVisualIndex );
    }
}

void PublicTransport::sectionResized( int logicalIndex, int /*oldSize*/, int newSize ) {
    QHeaderView *header = static_cast<QHeaderView*>( sender() );
    if ( header ) {
	int visibleCount = header->count() - header->hiddenSectionCount();
	if ( header->visualIndex(logicalIndex) == visibleCount - 1 ) {
	    // Section is on the right
	    int size = 0;
	    for ( int i = 0; i < header->count() - 1; ++i ) {
		if ( !header->isSectionHidden(i) )
		    size += header->sectionSize( i );
	    }

	    int remainingSize = header->contentsRect().width() - size;
	    if ( newSize > remainingSize )
		header->resizeSection( logicalIndex, remainingSize );
	}
    } else
	kDebug() << "Couldn't get the header view";
}

void PublicTransport::sectionPressed( int logicalIndex ) {
    // Don't allow moving of visual index 0
    m_treeView->nativeWidget()->header()->setMovable(
	m_treeView->nativeWidget()->header()->visualIndex(logicalIndex) != 0 );
}

void PublicTransport::createJourneySearchWidgets() {
    m_btnLastJourneySearches = new Plasma::ToolButton;
    m_btnLastJourneySearches->setIcon( KIcon("document-open-recent") );
    m_btnLastJourneySearches->setToolTip( i18n("Use a recent journey search") );
    m_btnLastJourneySearches->nativeWidget()->setPopupMode( QToolButton::InstantPopup );
    
    // This is needed, to have the popup menu drawn above other widgets
    m_btnLastJourneySearches->setZValue( 999 );

    m_btnStartJourneySearch = new Plasma::ToolButton;
    m_btnStartJourneySearch->setIcon( KIcon("edit-find") );
    m_btnStartJourneySearch->setToolTip( i18n("Find journeys") );
    m_btnStartJourneySearch->setEnabled( false );
    connect( m_btnStartJourneySearch, SIGNAL(clicked()),
	     this, SLOT(journeySearchInputFinished()) );

    m_journeySearch = new Plasma::LineEdit;
    m_journeySearch->setNativeWidget( new JourneySearchLineEdit );
    m_journeySearch->setToolTip( i18nc("This should match the localized keywords.",
	    "Type a <b>target stop</b> or <b>journey request</b>."
	    "<br><br> <b>Samples:</b><br>"
	    "&nbsp;&nbsp;&bull; <i>To target in 15 mins</i><br>"
	    "&nbsp;&nbsp;&bull; <i>From origin arriving tomorrow at 18:00</i><br>"
	    "&nbsp;&nbsp;&bull; <i>Target at 6:00 2010-03-07</i>"/*, sColor*/) );
    m_journeySearch->installEventFilter( this ); // Handle up/down keys (selecting stop suggestions)
//     m_journeySearch->setClearButtonShown( true );
    m_journeySearch->nativeWidget()->setCompletionMode( KGlobalSettings::CompletionAuto );
    m_journeySearch->nativeWidget()->setCompletionModeDisabled(
	    KGlobalSettings::CompletionMan );
    m_journeySearch->nativeWidget()->setCompletionModeDisabled(
	    KGlobalSettings::CompletionPopup );
    m_journeySearch->nativeWidget()->setCompletionModeDisabled(
	    KGlobalSettings::CompletionPopupAuto );
    m_journeySearch->nativeWidget()->setCompletionModeDisabled(
	    KGlobalSettings::CompletionShell );
    m_journeySearch->setEnabled( true );
    KLineEdit *journeySearch = m_journeySearch->nativeWidget();
    journeySearch->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    journeySearch->setClickMessage( i18n("Target stop name or journey request") );
    KCompletion *completion = journeySearch->completionObject( false );
    completion->setIgnoreCase( true );
    m_journeySearch->setFont( m_settings.sizedFont() );
    connect( m_journeySearch, SIGNAL(returnPressed()),
	    this, SLOT(journeySearchInputFinished()) );
    connect( m_journeySearch, SIGNAL(textEdited(QString)),
	    this, SLOT(journeySearchInputEdited(QString)) );
}

QGraphicsWidget* PublicTransport::widgetForType( TitleType titleType ) {
    // Setup new main layout
    QGraphicsLinearLayout *layoutMainNew = new QGraphicsLinearLayout( Qt::Vertical, m_mainGraphicsWidget );
    layoutMainNew->setContentsMargins( 0, 0, 0, 0 );
    layoutMainNew->setSpacing( 0 );

    // Delete widgets of the old view
    switch ( m_titleType ) {
	case ShowDepartureArrivalListTitle:
	    m_label->hide();
	    break;
	
	case ShowSearchJourneyLineEdit:
	    m_listStopSuggestions->deleteLater();
	    m_listStopSuggestions = NULL;
	    m_journeySearch->deleteLater();
	    m_journeySearch = NULL;
	    m_btnLastJourneySearches->deleteLater();
	    m_btnLastJourneySearches = NULL;
	    m_btnStartJourneySearch->deleteLater();
	    m_btnStartJourneySearch = NULL;
	    break;
	    
	case ShowSearchJourneyLineEditDisabled:
	    m_labelJourneysNotSupported->deleteLater();
	    m_labelJourneysNotSupported = NULL;
	    m_journeySearch->deleteLater();
	    m_journeySearch = NULL;
	    m_btnLastJourneySearches->deleteLater();
	    m_btnLastJourneySearches = NULL;
	    break;

	case ShowJourneyListTitle:
	    m_iconClose->deleteLater();
	    m_iconClose = NULL;
	    m_treeViewJourney->deleteLater();
	    m_treeViewJourney = NULL;
	    break;

	default:
	    break;
    }

    QGraphicsWidget *widget;
    m_treeView->setVisible( titleType == ShowDepartureArrivalListTitle );

    // New type
    switch ( titleType ) {
	case ShowDepartureArrivalListTitle:
	    setMainIconDisplay( testState(ReceivedValidDepartureData)
		    ? DepartureListOkIcon : DepartureListErrorIcon );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_label->setText( titleText() );
	    m_label->show();
	    m_labelInfo->setToolTip( courtesyToolTip() );
	    m_labelInfo->setText( infoText() );
	    widget = m_treeView;
	    break;
	    
	case ShowSearchJourneyLineEdit: {
	    setMainIconDisplay( AbortJourneySearchIcon );
	    m_icon->setToolTip( i18n("Abort search for journeys to or from the home stop") );

	    createJourneySearchWidgets();
	    m_listStopSuggestions = new Plasma::TreeView();
	    m_listStopSuggestions->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	    QTreeView *treeView = m_listStopSuggestions->nativeWidget();
	    treeView->setRootIsDecorated( false );
	    treeView->setHeaderHidden( true );
	    treeView->setAlternatingRowColors( true );
	    treeView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	    treeView->setAutoFillBackground( false );
	    treeView->setAttribute( Qt::WA_NoSystemBackground );
	    treeView->setItemDelegate( new PublicTransportDelegate(m_listStopSuggestions) );
	    treeView->setPalette( m_treeView->nativeWidget()->palette() );
	    m_listStopSuggestions->setFont( m_settings.sizedFont() );

	    if ( !m_settings.recentJourneySearches.isEmpty() ) {
		QStandardItemModel *model = new QStandardItemModel( m_listStopSuggestions );
		foreach ( const QString &recent, m_settings.recentJourneySearches ) {
		    QStandardItem *item = new QStandardItem( KIcon("emblem-favorite"),
							     i18n("<b>Recent:</b> %1", recent) );
		    item->setData( "recent", Qt::UserRole + 1 );
		    item->setData( recent, Qt::UserRole + 2 );
		    model->appendRow( item );
		}
		m_listStopSuggestions->setModel( model );
	    }
	    
	    connect( treeView, SIGNAL(clicked(QModelIndex)),
		     this, SLOT(possibleStopClicked(QModelIndex)) );
	    connect( treeView, SIGNAL(doubleClicked(QModelIndex)),
		     this, SLOT(possibleStopDoubleClicked(QModelIndex)) );
	    widget = m_listStopSuggestions;
	    break;
	}
	case ShowSearchJourneyLineEditDisabled:
	    setMainIconDisplay( AbortJourneySearchIcon );
	    m_icon->setToolTip( i18n("Abort search for journeys to or from the home stop") );
	    
	    createJourneySearchWidgets();
	    m_labelJourneysNotSupported = new Plasma::Label;
	    m_labelJourneysNotSupported->setAlignment( Qt::AlignCenter );
	    m_labelJourneysNotSupported->setSizePolicy( QSizePolicy::Expanding,
							QSizePolicy::Expanding, QSizePolicy::Label );
	    m_labelJourneysNotSupported->setText( i18n("Journey searches aren't supported "
						    "by the currently used service provider "
						    "or it's accessor.") );
	    m_labelJourneysNotSupported->nativeWidget()->setWordWrap( true );
	    
	    m_journeySearch->setEnabled( false );
	    m_btnLastJourneySearches->setEnabled( false );
	    m_btnStartJourneySearch->setEnabled( false );
	    widget = m_labelJourneysNotSupported;
	    break;

	case ShowJourneyListTitle: {
	    setMainIconDisplay( testState(ReceivedValidJourneyData)
		    ? JourneyListOkIcon : JourneyListErrorIcon );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_label->setText( m_journeyTitleText.isEmpty() ? i18n("<b>Journeys</b>") : m_journeyTitleText );
	    
	    int iconExtend = 32 * m_settings.sizeFactor;
	    m_iconClose = new Plasma::IconWidget;
	    m_iconClose->setIcon( "window-close" );
	    m_iconClose->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
	    m_iconClose->setMinimumSize( iconExtend, iconExtend );
	    m_iconClose->setMaximumSize( iconExtend, iconExtend );
	    connect( m_iconClose, SIGNAL(clicked()), this, SLOT(iconCloseClicked()) );
	    m_iconClose->setToolTip( i18n("Show departures / arrivals") );
	    
	    m_treeViewJourney = new Plasma::TreeView;
	    initTreeView( m_treeViewJourney );
	    QTreeView *treeViewJourneys = m_treeViewJourney->nativeWidget();
	    treeViewJourneys->setPalette( m_treeView->nativeWidget()->palette() );
	    treeViewJourneys->header()->setPalette( m_treeView->nativeWidget()->header()->palette() );
	    treeViewJourneys->setIconSize( QSize(32 * m_settings.sizeFactor, 32 * m_settings.sizeFactor) );
	    treeViewJourneys->header()->resizeSection( 1, 120 );
	    m_treeViewJourney->setModel( m_modelJourneys );
	    m_treeViewJourney->setFont( m_settings.sizedFont() );

	    widget = m_treeViewJourney;
	    break;
	}
    }
    
    // Add new title layout
    QGraphicsLayout *layoutTopNew = createLayoutTitle( titleType );
    if ( layoutTopNew ) {
	layoutMainNew->addItem( layoutTopNew );
	layoutMainNew->setAlignment( layoutTopNew, Qt::AlignTop );
    }
    layoutMainNew->addItem( widget );
    layoutMainNew->addItem( m_labelInfo );
    layoutMainNew->setAlignment( m_labelInfo, Qt::AlignRight | Qt::AlignVCenter );
    return m_mainGraphicsWidget;
}

void PublicTransport::setTitleType( TitleType titleType ) {
    // Draw old appearance to pixmap
    QPixmap pix( m_mainGraphicsWidget->size().toSize() );
    pix.fill( Qt::transparent );
    QPainter p( &pix );
    QRect sourceRect = m_mainGraphicsWidget->mapToScene( m_mainGraphicsWidget->boundingRect() ).boundingRect().toRect();
    QRectF rect( QPointF(0, 0), m_mainGraphicsWidget->size() );
    scene()->render( &p, rect, sourceRect );
    p.end();

//     m_mainGraphicsWidget->hide();
    m_mainGraphicsWidget = widgetForType( titleType ); // TODO

    // Setup the new main layout
    switch( titleType ) {
	case ShowDepartureArrivalListTitle:
	    setMainIconDisplay( testState(ReceivedValidDepartureData)
		    ? DepartureListOkIcon : DepartureListErrorIcon );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_label->setText( titleText() );
	    m_labelInfo->setToolTip( courtesyToolTip() );
	    m_labelInfo->setText( infoText() );
	    break;

	case ShowSearchJourneyLineEdit:
	    setMainIconDisplay( AbortJourneySearchIcon );
	    m_icon->setToolTip( i18n("Abort search for journeys to or from the home stop") );
	    m_journeySearch->setEnabled( true );
	    
	    m_journeySearch->setFocus();
	    m_journeySearch->nativeWidget()->selectAll();
	    break;
	    
	case ShowSearchJourneyLineEditDisabled:
// 	    setMainIconDisplay( AbortJourneySearchIcon );
// 	    m_icon->setToolTip( i18n("Abort search for journeys to or from the home stop") );

// 	    m_journeySearch->setEnabled( false );
// 	    m_btnLastJourneySearches->setEnabled( false );
// 	    m_btnStartJourneySearch->setEnabled( false );
	    break;

	case ShowJourneyListTitle:
	    setMainIconDisplay( testState(ReceivedValidJourneyData)
		    ? JourneyListOkIcon : JourneyListErrorIcon );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_iconClose->setToolTip( i18n("Show departures / arrivals") );
	    m_label->setText( i18n("<b>Journeys</b>") );
	    break;
    }

    oldItemAnimationFinished();
    m_oldItem = new GraphicsPixmapWidget( pix, m_graphicsWidget );
    m_oldItem->setPos( 0, 0 );
    m_oldItem->setZValue( 1000 );
    Plasma::Animation *animOut = Plasma::Animator::create( Plasma::Animator::FadeAnimation );
    animOut->setProperty( "startOpacity", 1 );
    animOut->setProperty( "targetOpacity", 0 );
    animOut->setTargetWidget( m_oldItem );
    connect( animOut, SIGNAL(finished()), this, SLOT(oldItemAnimationFinished()) );
    animOut->start( QAbstractAnimation::DeleteWhenStopped );
    
    m_titleType = titleType;
}

void PublicTransport::oldItemAnimationFinished() {
    if ( m_oldItem && m_oldItem->scene() )
	m_oldItem->scene()->removeItem( m_oldItem );
    delete m_oldItem;
    m_oldItem = NULL;
}

void PublicTransport::recentJourneyActionTriggered( QAction* action ) {
    if ( action->data().isValid() && action->data().toBool() ) {
	// Clear recent journey list
	m_settings.recentJourneySearches.clear();
	m_btnLastJourneySearches->setEnabled( false );
	SettingsIO::writeNoGuiSettings( m_settings, config(), globalConfig() );
    } else
	m_journeySearch->setText( action->text() );

    m_journeySearch->setFocus();
}

void PublicTransport::unsetStates( QList< AppletState > states ) {
    foreach( AppletState appletState, states )
	m_appletStates &= ~appletState;
}

void PublicTransport::addState( AppletState state ) {
    QColor textColor;
    QMenu *menu;

    switch ( state ) {
	case ShowingDepartureArrivalList:
	    setTitleType( ShowDepartureArrivalListTitle );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
// 	    m_treeView->setModel( m_model );
	    m_treeView->nativeWidget()->setIconSize(
		    QSize(16 * m_settings.sizeFactor, 16 * m_settings.sizeFactor) );
	    m_treeView->nativeWidget()->setColumnHidden( ColumnTarget, m_settings.hideColumnTarget );
	    geometryChanged(); // TODO: Only resize columns
	    setBusy( testState(WaitingForDepartureData) && m_model->rowCount() == 0 );
	    disconnectJourneySource();
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    setAssociatedApplicationUrls( KUrl::List() << m_urlDeparturesArrivals );
	    #endif
	    unsetStates( QList<AppletState>() << ShowingJourneyList
		    << ShowingJourneySearch << ShowingJourneysNotSupported );
	    break;

	case ShowingJourneyList:
	    setTitleType( ShowJourneyListTitle );
	    m_icon->setToolTip( i18n("Quick configuration and journey search") );
	    setBusy( testState(WaitingForJourneyData) && m_modelJourneys->rowCount() == 0 );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    setAssociatedApplicationUrls( KUrl::List() << m_urlJourneys );
	    #endif
	    unsetStates( QList<AppletState>() << ShowingDepartureArrivalList
		    << ShowingJourneySearch << ShowingJourneysNotSupported );
	    break;

	case ShowingJourneySearch:
	    setTitleType( ShowSearchJourneyLineEdit );
	    
	    if ( m_settings.recentJourneySearches.isEmpty() ) {
		m_btnLastJourneySearches->setEnabled( false );
	    } else {
		menu = new QMenu( m_btnLastJourneySearches->nativeWidget() );
		foreach ( QString recent, m_settings.recentJourneySearches )
		    menu->addAction( recent );
		menu->addSeparator();
		menu->addAction( KIcon("edit-clear-list"),
				 i18n("&Clear list") )->setData( true );
		connect( menu, SIGNAL(triggered(QAction*)),
			this, SLOT(recentJourneyActionTriggered(QAction*)) );
		m_btnLastJourneySearches->nativeWidget()->setMenu( menu );
		m_btnLastJourneySearches->setEnabled( true );
	    }

	    m_icon->setToolTip( i18n("Go back to the departure / arrival list") );
	    setBusy( false);

	    unsetStates( QList<AppletState>() << ShowingDepartureArrivalList
		    << ShowingJourneyList << ShowingJourneysNotSupported );
	    break;

	case ShowingJourneysNotSupported:
	    setTitleType( ShowSearchJourneyLineEditDisabled );
	    m_icon->setToolTip( i18n("Go back to the departure / arrival list") );
	    setBusy( false);
	    
	    unsetStates( QList<AppletState>() << ShowingDepartureArrivalList
		    << ShowingJourneyList << ShowingJourneySearch );
	    break;

	case ReceivedValidDepartureData:
	    if ( m_titleType == ShowDepartureArrivalListTitle ) {
		setMainIconDisplay( DepartureListOkIcon );
		setBusy( false );
	    }

	    if ( m_currentMessage == MessageError ) {
		hideMessage();
		m_currentMessage = MessageNone;
	    }
	    unsetStates( QList<AppletState>() << WaitingForDepartureData << ReceivedErroneousDepartureData );
	    break;

	case ReceivedValidJourneyData:
	    if ( m_titleType == ShowJourneyListTitle ) {
		setMainIconDisplay( JourneyListOkIcon );
		setBusy( false );
	    }
	    unsetStates( QList<AppletState>() << WaitingForJourneyData << ReceivedErroneousJourneyData );
	    break;

	case ReceivedErroneousDepartureData:
	    if ( m_titleType == ShowDepartureArrivalListTitle ) {
		setMainIconDisplay( DepartureListErrorIcon );
		setBusy( false );
	    }
	    unsetStates( QList<AppletState>() << WaitingForDepartureData << ReceivedValidDepartureData );
	    break;

	case ReceivedErroneousJourneyData:
	    if ( m_titleType == ShowJourneyListTitle ) {
		setMainIconDisplay( JourneyListErrorIcon );
		setBusy( false );
	    }
	    unsetStates( QList<AppletState>() << WaitingForJourneyData << ReceivedValidJourneyData );
	    break;

	case WaitingForDepartureData:
	    if ( m_titleType == ShowDepartureArrivalListTitle ) {
		setMainIconDisplay( DepartureListErrorIcon ); // TODO: Add a special icon for "waiting for data"? (waits for first data of a new data source)
		setBusy( m_model->rowCount() == 0 );
	    }
	    unsetStates( QList<AppletState>() << ReceivedValidDepartureData << ReceivedErroneousDepartureData );
	    break;

	case WaitingForJourneyData:
	    if ( m_titleType == ShowJourneyListTitle ) {
		setMainIconDisplay( JourneyListErrorIcon );
		setBusy( m_modelJourneys->rowCount() == 0 );
	    }
	    unsetStates( QList<AppletState>() << ReceivedValidJourneyData << ReceivedErroneousJourneyData );
	    break;

	case SettingsJustChanged:
	case ServiceProviderSettingsJustChanged:
	case NoState:
	    break;
    }

    m_appletStates |= state;
}

void PublicTransport::removeState( AppletState state ) {
    if ( !m_appletStates.testFlag(state) )
	return;

    switch ( state ) {
	case ShowingJourneyList:
	    setMainIconDisplay( m_appletStates.testFlag(ReceivedValidDepartureData)
				? DepartureListOkIcon : DepartureListErrorIcon );
	    setDepartureArrivalListType( m_settings.departureArrivalListType );
	    break;

	case SettingsJustChanged:
	case ServiceProviderSettingsJustChanged:
	case ShowingDepartureArrivalList:
	case ShowingJourneySearch:
	case ShowingJourneysNotSupported:
	case WaitingForDepartureData:
	case ReceivedValidDepartureData:
	case ReceivedErroneousDepartureData:
	case WaitingForJourneyData:
	case ReceivedValidJourneyData:
	case ReceivedErroneousJourneyData:
	case NoState:
	    break;
    }

    m_appletStates ^= state;
}

void PublicTransport::hideHeader() {
    QTreeView *treeView = m_treeViewJourney ? m_treeViewJourney->nativeWidget() : m_treeView->nativeWidget();
    treeView->header()->setVisible( false );
    
    Settings settings = m_settings;
    settings.showHeader = false;
    writeSettings( settings );
}

void PublicTransport::showHeader() {
    QTreeView *treeView = m_treeViewJourney ? m_treeViewJourney->nativeWidget() : m_treeView->nativeWidget();
    treeView->header()->setVisible( true );
    
    Settings settings = m_settings;
    settings.showHeader = true;
    writeSettings( settings );
}

void PublicTransport::hideColumnTarget() {
    Settings settings = m_settings;
    settings.hideColumnTarget = true;
    writeSettings( settings );
}

void PublicTransport::showColumnTarget() {
    Settings settings = m_settings;
    settings.hideColumnTarget = false;
    writeSettings( settings );
}

void PublicTransport::toggleExpanded() {
    doubleClickedDepartureItem( m_clickedItemIndex );
}

void PublicTransport::doubleClickedDepartureItem( const QModelIndex &modelIndex ) {
    QModelIndex firstIndex;
    if ( testState(ShowingDepartureArrivalList) )
	firstIndex = m_model->index( modelIndex.row(), 0, modelIndex.parent() );
    else
	firstIndex = m_modelJourneys->index( modelIndex.row(), 0, modelIndex.parent() );

    QTreeView* treeView =  m_treeViewJourney ? m_treeViewJourney->nativeWidget() : m_treeView->nativeWidget();
    if ( treeView->isExpanded(firstIndex) )
	treeView->collapse( firstIndex );
    else
	treeView->expand( firstIndex );
}

QAction* PublicTransport::updatedAction( const QString& actionName ) {
    QAction *a = action(actionName);
    if ( !a ) {
	if ( actionName == "separator" ) {
	    a = new QAction(this);
	    a->setSeparator(true);
	    return a;
	} else {
	    kDebug() << "Action not found:" << actionName;
	    return NULL;
	}
    }

    QAbstractItemModel *model;
    if ( testState(ShowingDepartureArrivalList) )
	model = m_model;
    else
	model = m_modelJourneys;

    if ( actionName == "backToDepartures" ) {
	a->setText( m_settings.departureArrivalListType == DepartureList
	    ? i18n("Back to &Departure List")
	    : i18n("Back to &Arrival List") );
    } else if ( actionName == "toggleExpanded" ) {
	if ( ( m_treeViewJourney ? m_treeViewJourney : m_treeView)->nativeWidget()->isExpanded( model->index(m_clickedItemIndex.row(), 0) ) ) {
	    a->setText( i18n("Hide Additional &Information") );
	    a->setIcon( KIcon("arrow-up") );
	} else {
	    a->setText( i18n("Show Additional &Information") );
	    a->setIcon( KIcon("arrow-down") );
	}
    } else if ( actionName == "removeAlarmForDeparture" ) {
	a->setText( m_settings.departureArrivalListType == DepartureList
		? i18n("Remove &Alarm for This Departure")
		: i18n("Remove &Alarm for This Arrival") );
    } else if ( actionName == "setAlarmForDeparture" ) {
	a->setText( m_settings.departureArrivalListType == DepartureList
		? i18n("Set &Alarm for This Departure")
		: i18n("Set &Alarm for This Arrival") );
    }

    return a;
}

void PublicTransport::showHeaderContextMenu( const QPoint& position ) {
    Q_UNUSED( position );
    QHeaderView *header = ( m_treeViewJourney ? m_treeViewJourney : m_treeView)->nativeWidget()->header();
    QList<QAction *> actions;

    if ( testState(ShowingDepartureArrivalList) ) {
	if ( header->logicalIndexAt(position) == 1 )
	    actions.append( action("hideColumnTarget") );
	else if ( header->isSectionHidden(1) )
	    actions.append( action("showColumnTarget") );
    }
    actions.append( action("hideHeader") );

    if ( actions.count() > 0 && view() )
	QMenu::exec( actions, QCursor::pos() );
}

void PublicTransport::showDepartureContextMenu ( const QPoint& position ) {
    QTreeView* treeView =  m_treeViewJourney ? m_treeViewJourney->nativeWidget() : m_treeView->nativeWidget();
    QList<QAction *> actions;
    QAction *infoAction = NULL;

    if ( (m_clickedItemIndex = treeView->indexAt(position)).isValid() ) {
	while( m_clickedItemIndex.parent().isValid() )
	    m_clickedItemIndex = m_clickedItemIndex.parent();

	actions.append( updatedAction("toggleExpanded") );

	if ( testState(ShowingDepartureArrivalList) ) {
	    DepartureItem *item = static_cast<DepartureItem*>( m_model->item(m_clickedItemIndex.row()) );
	    if ( item->hasAlarm() ) {
		if ( item->alarmStates().testFlag(AlarmIsAutoGenerated) )
		    actions.append( updatedAction("removeAlarmForDeparture") );
		else if ( item->alarmStates().testFlag(AlarmIsRecurring) ) {
		    kDebug() << "The 'Remove this Alarm' menu entry can only be "
				"used with autogenerated alarms.";
		    if ( item->departureInfo()->matchedAlarms().count() == 1 ) {
			infoAction = new QAction( KIcon("task-recurring"),
						  i18n("(has a recurring alarm)"), this );
		    } else {
			infoAction = new QAction( i18n("(has multiple alarms)"), this );
		    }
		} else {
		    kDebug() << "The 'Remove this Alarm' menu entry can only be "
				"used with autogenerated alarms.";
		    if ( item->departureInfo()->matchedAlarms().count() == 1 ) {
			infoAction = new QAction( KIcon("task-recurring"),
						  i18n("(has a custom alarm)"), this );
		    } else {
			infoAction = new QAction( i18n("(has multiple alarms)"), this );
		    }
		}
		if ( infoAction ) {
		    infoAction->setDisabled( true );
		    actions.append( infoAction );
		}
	    } else
		actions.append( updatedAction("setAlarmForDeparture") );
	}

	if ( !treeView->header()->isVisible() ) {
	    actions.append( updatedAction("separator") );
	    actions.append( action("showHeader") );
	} else if ( treeView->header()->isSectionHidden(ColumnTarget) ) {
	    if ( testState(ShowingDepartureArrivalList) ) {
		actions.append( updatedAction("separator") );
		actions.append( action("showColumnTarget") );
	    }
	}
    } else { // No context item
	actions.append( updatedAction("searchJourneys") );
	actions.append( updatedAction("separator") );
	
	if ( !treeView->header()->isVisible() )
	    actions.append( action("showHeader") );
    }

    if ( actions.count() > 0 && view() )
	QMenu::exec( actions, QCursor::pos() );

    delete infoAction;
}

void PublicTransport::showJourneySearch() {
    addState( m_currentServiceProviderFeatures.contains("JourneySearch")
	      ? ShowingJourneySearch : ShowingJourneysNotSupported );
}

void PublicTransport::goBackToDepartures() {
    addState( ShowingDepartureArrivalList );
}

void PublicTransport::removeAlarmForDeparture( int row ) {
    DepartureItem *item = static_cast<DepartureItem*>( m_model->item(row) );
    Q_ASSERT_X( item->alarmStates().testFlag(AlarmIsAutoGenerated),
		"PublicTransport::removeAlarmForDeparture",
		"Only auto generated alarms can be removed" );

    // Find a matching autogenerated alarm
    int matchingAlarmSettings = -1;
    for ( int i = 0; i < m_settings.alarmSettings.count(); ++i ) {
	AlarmSettings alarmSettings = m_settings.alarmSettings[ i ];
	if ( alarmSettings.autoGenerated && alarmSettings.enabled
	    && alarmSettings.filter.match(*item->departureInfo()) )
	{
	    matchingAlarmSettings = i;
	    break;
	}
    }
    if ( matchingAlarmSettings == -1 ) {
	kDebug() << "Couldn't find a matching autogenerated alarm";
	return;
    }
    
    item->removeAlarm();
    AlarmSettingsList newAlarmSettings = m_settings.alarmSettings;
    newAlarmSettings.removeAt( matchingAlarmSettings );
    removeAlarms( newAlarmSettings, QList<int>() << matchingAlarmSettings );

    if ( m_clickedItemIndex.isValid() )
	createPopupIcon();
}

void PublicTransport::removeAlarmForDeparture() {
    removeAlarmForDeparture( m_clickedItemIndex.row() );
}

void PublicTransport::createAlarmSettingsForDeparture( const QPersistentModelIndex &modelIndex ) {
    if( !modelIndex.isValid() ) {
	kDebug() << "!modelIndex.isValid()";
	return;
    }

    const DepartureInfo *info = static_cast<DepartureItem*>(
	    m_model->itemFromIndex(modelIndex) )->departureInfo();
    AlarmSettings alarm;
    QString departureTime = KGlobal::locale()->formatTime( info->departure().time() );
    alarm.name = i18nc( "Name of new automatically generated alarm filters. "
			"%1 is the departure time, %2 is the target.",
			"At %1 to %2", departureTime, info->target() );
    alarm.autoGenerated = true;
    alarm.affectedStops << m_settings.currentStopSettingsIndex;
    alarm.filter.append( Constraint(FilterByDeparture, FilterEquals, info->departure()) );
    alarm.filter.append( Constraint(FilterByTransportLine, FilterEquals, info->lineString()) );
    alarm.filter.append( Constraint(FilterByVehicleType, FilterIsOneOf,
				    QVariantList() << info->vehicleType()) );
    alarm.filter.append( Constraint(FilterByTarget, FilterEquals, info->target()) );
    Settings settings = m_settings;
    settings.alarmSettings << alarm;
    writeSettings( settings );
    
    createPopupIcon();
}

void PublicTransport::setAlarmForDeparture() {
    createAlarmSettingsForDeparture( m_model->index(m_clickedItemIndex.row(), 2) );
}

void PublicTransport::alarmFired( DepartureItem* item ) {
    const DepartureInfo *departureInfo = item->departureInfo();
    QString sLine = departureInfo->lineString();
    QString sTarget = departureInfo->target();
    QDateTime predictedDeparture = departureInfo->predictedDeparture();
    int minsToDeparture = qCeil( (float)QDateTime::currentDateTime().secsTo(predictedDeparture) / 60.0f );

    QString message;
    if ( minsToDeparture > 0 ) {
	if ( departureInfo->vehicleType() == Unknown )
	    message = i18np("Line %2 to '%3' departs in %1 minute at %4",
			    "Line %2 to '%3' departs in %1 minutes at %4",
			    minsToDeparture, sLine, sTarget, predictedDeparture.toString("hh:mm") );
	else
	    message = i18ncp("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)",
			     "The %4 %2 to '%3' departs in %1 minute at %5",
			     "The %4 %2 to '%3' departs in %1 minutes at %5",
			     minsToDeparture, sLine, sTarget,
			     Global::vehicleTypeToString(departureInfo->vehicleType()),
			     predictedDeparture.toString("hh:mm"));
    }
    else if ( minsToDeparture < 0 ) {
	if ( departureInfo->vehicleType() == Unknown )
	    message = i18np("Line %2 to '%3' has departed %1 minute ago at %4",
			    "Line %2 to '%3' has departed %1 minutes ago at %4",
			    -minsToDeparture, sLine, sTarget,
			    predictedDeparture.toString("hh:mm"));
	else
	    message = i18ncp("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)",
			     "The %4 %2 to '%3' has departed %1 minute ago at %5",
			     "The %4 %2 to %3 has departed %1 minutes ago at %5",
			     -minsToDeparture, sLine, sTarget,
			     Global::vehicleTypeToString(departureInfo->vehicleType()),
			     predictedDeparture.toString("hh:mm"));
    }
    else {
	if ( departureInfo->vehicleType() == Unknown )
	    message = i18n("Line %1 to '%2' departs now at %3", sLine, sTarget,
			   predictedDeparture.toString("hh:mm"));
	else
	    message = i18nc("%1: Line string (e.g. 'U3'), %3: Vehicle type name (e.g. tram, subway)",
			    "The %3 %1 to '%2' departs now at %4", sLine, sTarget,
			    Global::vehicleTypeToString(departureInfo->vehicleType()),
			    predictedDeparture.toString("hh:mm"));
    }

    KNotification::event( KNotification::Warning, message,
			  KIcon("public-transport-stop").pixmap(16), 0L,
			  KNotification::Persistent );
}

void PublicTransport::removeAlarms( const AlarmSettingsList &newAlarmSettings,
				    const QList<int> &/*removedAlarms*/ ) {
    Settings settings = m_settings;
    settings.alarmSettings = newAlarmSettings;
    writeSettings( settings );
}

QString PublicTransport::titleText() const {
    QString sStops = m_settings.currentStopSettings().stops.join(", ");
    if ( !m_settings.currentStopSettings().city.isEmpty() )
	return QString("%1, %2").arg( sStops ).arg( m_settings.currentStopSettings().city );
    else
	return QString("%1").arg( sStops );
}

QString PublicTransport::infoText() {
    QVariantHash data = currentServiceProviderData();
    QString shortUrl = data[ "shortUrl" ].toString();
    QString url = data[ "url" ].toString();
    QString sLastUpdate = m_lastSourceUpdate.toString( "hh:mm" );
    if ( sLastUpdate.isEmpty() )
	sLastUpdate = i18nc("This is used as 'last data update' text when there "
			    "hasn't been any updates yet.", "none");

    // HACK: This breaks the text at one position if needed
    // Plasma::Label doesn't work well will HTML formatted text and word wrap: 
    // It sets the height as if the label shows the HTML source.
    QString textNoHtml1 = QString("%1: %2").arg( i18n("last update"), sLastUpdate );
    QString textNoHtml2 = QString("%1: %2").arg( i18n("data by"), shortUrl );
    QFontMetrics fm( m_labelInfo->font() );
    int width1 = fm.width( textNoHtml1 );
    int width2 = fm.width( textNoHtml2 );
    int width = width1 + fm.width( ", " ) + width2;
    if ( width > m_graphicsWidget->size().width() ) {
	m_graphicsWidget->setMinimumWidth( qMax(150, qMax(width1, width2)) );
	return QString("<nobr>%1: %2<br>%3: <a href='%4'>%5</a><nobr>")
	    .arg( i18n("last update"), sLastUpdate, i18n("data by"), url, shortUrl );
    } else {
	return QString("<nobr>%1: %2, %3: <a href='%4'>%5</a><nobr>")
	    .arg( i18n("last update"), sLastUpdate, i18n("data by"), url, shortUrl );
    }
}

QString PublicTransport::courtesyToolTip() const {
    QVariantHash data = currentServiceProviderData();
    QString credit = data["credit"].toString();
    if ( credit.isEmpty() )
	return QString();
    else
	return i18n( "By courtesy of %1 (%2)", credit, data["url"].toString() );
}

void PublicTransport::setTextColorOfHtmlItem( QStandardItem *item, const QColor &textColor ) {
    item->setText( item->text().prepend("<span style='color:rgba(%1,%2,%3,%4);'>")
	    .arg( textColor.red() ).arg( textColor.green() ).arg( textColor.blue() )
	    .arg( textColor.alpha() ).append("</span>") );
}

void PublicTransport::appendJourney( const JourneyInfo& journeyInfo ) {
    QHeaderView *header =  (m_treeViewJourney ? m_treeViewJourney : m_treeView)->nativeWidget()->header();
    JourneyItem *item = m_modelJourneys->addItem( journeyInfo,
	    static_cast<Columns>(header->sortIndicatorSection()),
	    header->sortIndicatorOrder() );
    ChildItem *routeItem = item->childByType( RouteItem );
    if ( routeItem )
	m_treeView->nativeWidget()->expand( routeItem->index() );
    stretchAllChildren( m_modelJourneys->indexFromItem(item), m_modelJourneys );
}

void PublicTransport::appendDeparture( const DepartureInfo& departureInfo ) {
    QHeaderView *header =  (m_treeViewJourney ? m_treeViewJourney : m_treeView)->nativeWidget()->header();
    DepartureItem *item = m_model->addItem( departureInfo,
	    static_cast<Columns>(header->sortIndicatorSection()),
	    header->sortIndicatorOrder() );
    ChildItem *routeItem = item->childByType( RouteItem );
    if ( routeItem ) 
	(m_treeViewJourney ? m_treeViewJourney : m_treeView)->nativeWidget()->expand( routeItem->index() );
    stretchAllChildren( m_model->indexFromItem(item), m_model );
}

void PublicTransport::fillModelJourney( const QList< JourneyInfo > &journeys ) {
    foreach ( const JourneyInfo &journeyInfo, journeys ) {
	int row = m_modelJourneys->indexFromInfo( journeyInfo ).row();
	if ( row == -1 )
	    appendJourney( journeyInfo );
	else {
	    JourneyItem *item = static_cast<JourneyItem*>( m_modelJourneys->itemFromInfo(journeyInfo) );
	    m_modelJourneys->updateItem( item, journeyInfo );
	    ChildItem *routeItem = item->childByType( RouteItem );
	    if ( routeItem )
		m_treeViewJourney->nativeWidget()->expand( routeItem->index() );
	    stretchAllChildren( m_modelJourneys->indexFromItem(item), m_modelJourneys );
	}
    }
}

void PublicTransport::fillModel( const QList<DepartureInfo> &departures ) {
    bool modelFilled = m_model->rowCount() >= m_settings.maximalNumberOfDepartures;
    foreach ( const DepartureInfo &departureInfo, departures ) {
	int row = m_model->indexFromInfo( departureInfo ).row();
	if ( row == -1 ) {
	    if ( !modelFilled && !departureInfo.isFilteredOut() ) {
		appendDeparture( departureInfo );
		modelFilled = m_model->rowCount() >= m_settings.maximalNumberOfDepartures;
	    }
	} else if ( !departureInfo.isFilteredOut() ) {
	    DepartureItem *item = static_cast<DepartureItem*>( m_model->itemFromInfo(departureInfo) );
	    m_model->updateItem( item, departureInfo );
	    ChildItem *routeItem = item->childByType( RouteItem );
	    if ( routeItem )
		m_treeView->nativeWidget()->expand( routeItem->index() );
	    stretchAllChildren( m_model->indexFromItem(item), m_model );
	} else
	    m_model->removeItem( m_model->itemFromInfo(departureInfo) );
    }
}

void PublicTransport::stretchAllChildren( const QModelIndex& parent,
					  QAbstractItemModel *model ) {
    if ( !parent.isValid() )
	return; // Don't stretch top level items
    for ( int row = 0; row < model->rowCount(parent); ++row ) {
	(m_treeViewJourney ? m_treeViewJourney : m_treeView)->nativeWidget()->setFirstColumnSpanned( row, parent, true );
	stretchAllChildren( model->index(row, 0, parent), model );
    }
}

#include "publictransport.moc"