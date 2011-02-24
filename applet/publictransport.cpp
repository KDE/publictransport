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

// Own includes
#include "publictransport.h"
#include "departureprocessor.h"
#include "departuremodel.h"
#include "overlaywidget.h"
#include "journeysearchparser.h"
#include "journeysearchlineedit.h"
#include "journeysearchsuggestionwidget.h"
#include "settings.h"
#include "timetablewidget.h"

// Helper library includes
#include <publictransporthelper/global.h>

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
#include <KStandardDirs>

// Plasma includes
#include <Plasma/IconWidget>
#include <Plasma/Label>
#include <Plasma/ToolButton>
#include <Plasma/LineEdit>
#include <Plasma/PushButton>
#include <Plasma/PaintUtils>
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>
#include <Plasma/ToolTipContent>
#include <Plasma/Animation>

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
#include <qgraphicssceneevent.h>
#if QT_VERSION >= 0x040600
	#include <QSequentialAnimationGroup>
#endif

PublicTransport::PublicTransport( QObject *parent, const QVariantList &args )
		: Plasma::PopupApplet( parent, args ),
		m_graphicsWidget(0), m_mainGraphicsWidget(0), m_oldItem(0), m_titleWidget(0),
		m_labelInfo(0), m_timetable(0), m_journeyTimetable(0), m_listStopSuggestions(0),
		m_overlay(0), m_model(0), m_popupIconTransitionAnimation(0), m_modelJourneys(0), m_departureProcessor(0)
{
	m_currentMessage = MessageNone;
	m_appletStates = Initializing;
	m_popupIconDepartureIndex = 0;
	m_startPopupIconDepartureIndex = 0;
	m_endPopupIconDepartureIndex = 0;

	setBackgroundHints( StandardBackground );
	setAspectRatioMode( Plasma::IgnoreAspectRatio );
	setHasConfigurationInterface( true );
	resize( 400, 300 );
}

PublicTransport::~PublicTransport()
{
	if ( hasFailedToLaunch() ) {
		// Do some cleanup here
	} else {
		// Store header state
		KConfigGroup cg = config();
		// TODO: Readd a header?
// 		cg.writeEntry( "headerState", m_treeView->nativeWidget()->header()->saveState() );

		delete m_labelInfo;
		delete m_graphicsWidget;
	}
}

void PublicTransport::init()
{
	m_settings = SettingsIO::readSettings( config(), globalConfig() );

	// Create and connect the worker thread
	m_departureProcessor = new DepartureProcessor( this );
	connect( m_departureProcessor, SIGNAL(beginDepartureProcessing(QString)),
			 this, SLOT(beginDepartureProcessing(QString)) );
	connect( m_departureProcessor,
	         SIGNAL(departuresProcessed(QString, QList<DepartureInfo>, QUrl, QDateTime)),
	         this, SLOT(departuresProcessed(QString, QList<DepartureInfo>, QUrl, QDateTime)) );
	connect( m_departureProcessor, SIGNAL(beginJourneyProcessing(QString)),
	         this, SLOT(beginJourneyProcessing(QString)) );
	connect( m_departureProcessor,
	         SIGNAL(journeysProcessed(QString, QList<JourneyInfo>, QUrl, QDateTime)),
	         this, SLOT(journeysProcessed(QString, QList<JourneyInfo>, QUrl, QDateTime)) );
	connect( m_departureProcessor, SIGNAL(departuresFiltered(QString,QList<DepartureInfo>,QList<DepartureInfo>, QList<DepartureInfo>)),
	         this, SLOT(departuresFiltered(QString,QList<DepartureInfo>,QList<DepartureInfo>,QList<DepartureInfo>)) );

	// Load vehicle type SVG
    m_vehiclesSvg.setImagePath( KGlobal::dirs()->findResource("data", 
			"plasma_applet_publictransport/vehicles.svg") );
	m_vehiclesSvg.setContainsMultipleImages( true );
	
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

	// Set icon and text of the default "run associated application" action
	if ( QAction *runAction = action( "run associated application" ) ) {
		runAction->setText( i18nc( "@item:inmenu", "&Show in Web-Browser" ) );

		KService::Ptr offer = KMimeTypeTrader::self()->preferredService( "text/html" );
		if ( !offer.isNull() ) {
			runAction->setIcon( KIcon(offer->icon()) );
		}
	}

	// Create models
	m_model = new DepartureModel( this );
	connect( m_model, SIGNAL(alarmFired(DepartureItem*)), this, SLOT(alarmFired(DepartureItem*)) );
	connect( m_model, SIGNAL(updateAlarms(AlarmSettingsList,QList<int>)),
			 this, SLOT(removeAlarms(AlarmSettingsList,QList<int>)) );
	connect( m_model, SIGNAL(journeysAboutToBeRemoved(QList<ItemBase*>)),
			 this, SLOT(departuresAboutToBeRemoved(QList<ItemBase*>)) );
	connect( m_model, SIGNAL(departuresLeft(QList<DepartureInfo>)),
			 this, SLOT(departuresLeft(QList<DepartureInfo>)) );
	m_modelJourneys = new JourneyModel( this );

	graphicsWidget();
	checkNetworkStatus();
	createTooltip();
	createPopupIcon();

	m_model->setDepartureArrivalListType( m_settings.departureArrivalListType );
	addState( ShowingDepartureArrivalList );
	addState( WaitingForDepartureData );
	removeState( Initializing );

	connect( this, SIGNAL(geometryChanged()), this, SLOT(geometryChanged()) );
	connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
	connect( Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()),
	         this, SLOT(themeChanged()) );
	emit settingsChanged();

	setupActions();
	reconnectSource();
}

void PublicTransport::setSettings( const QString& serviceProviderID, const QString& stopName )
{
	// Set stop settings in a copy of the current settings.
	// Then write the new settings.
	Settings settings = m_settings;
	settings.stopSettingsList.clear();
	StopSettings stopSettings;
	stopSettings.set( ServiceProviderSetting, serviceProviderID );
	stopSettings.setStop( stopName );
	settings.stopSettingsList << stopSettings;
	writeSettings( settings );
}


PublicTransport::NetworkStatus PublicTransport::queryNetworkStatus()
{
	NetworkStatus status = StatusUnavailable;
	const QStringList interfaces = dataEngine( "network" )->sources();
	if ( interfaces.isEmpty() ) {
		return StatusUnknown;
	}

	// Check if there is an activated interface or at least one that's
	// currently being configured
	foreach( const QString &iface, interfaces ) {
		QString sStatus = dataEngine( "network" )->query( iface )["ConnectionStatus"].toString();
		if ( sStatus.isEmpty() ) {
			return StatusUnknown;
		}

		if ( sStatus == "Activated" ) {
			status = StatusActivated;
			break;
		} else if ( sStatus == "Configuring" ) {
			status = StatusConfiguring;
		}
	}

	return status;
}

bool PublicTransport::checkNetworkStatus()
{
	NetworkStatus status = queryNetworkStatus();
	if ( status == StatusUnavailable ) {
		m_currentMessage = MessageError;
		m_timetable->setNoItemsText( i18nc( "@info", "No network connection." ) );
		return false;
	} else if ( status == StatusConfiguring ) {
		m_currentMessage = MessageError;
		m_timetable->setNoItemsText( i18nc( "@info", "Network gets configured. Please wait..." ) );
		return false;
	} else if ( status == StatusActivated
	            && m_currentMessage == MessageError ) {
		m_currentMessage = MessageErrorResolved;
		m_timetable->setNoItemsText( i18nc( "@info", "Network connection established" ) );
		return false;
	} else {
		kDebug() << "Unknown network status or no error message was shown" << status;
		return true;
	}
}

void PublicTransport::noItemsTextClicked()
{
	// Update the timetable if an error message inside the tree view has been clicked
	if ( m_currentMessage == MessageError ) {
		updateDataSource();
	}
}

void PublicTransport::setupActions()
{
	QAction *actionUpdate = new QAction( KIcon( "view-refresh" ),
	                                     i18nc( "@action:inmenu", "&Update timetable" ), this );
	connect( actionUpdate, SIGNAL( triggered() ), this, SLOT( updateDataSource() ) );
	addAction( "updateTimetable", actionUpdate );

	QAction *showActionButtons = new QAction( /*KIcon("system-run"),*/ // TODO: better icon
			i18nc("@action", "&Quick Actions"), this );
	connect( showActionButtons, SIGNAL( triggered() ), this, SLOT( showActionButtons() ) );
	addAction( "showActionButtons", showActionButtons );

	QAction *actionSetAlarmForDeparture = new QAction(
			GlobalApplet::makeOverlayIcon( KIcon( "task-reminder" ), "list-add" ),
			m_settings.departureArrivalListType == DepartureList
			? i18nc("@action:inmenu", "Set &Alarm for This Departure")
			: i18nc("@action:inmenu", "Set &Alarm for This Arrival"), this );
	connect( actionSetAlarmForDeparture, SIGNAL(triggered()),
			 this, SLOT(setAlarmForDeparture()) );
	addAction( "setAlarmForDeparture", actionSetAlarmForDeparture );

	QAction *actionRemoveAlarmForDeparture = new QAction(
			GlobalApplet::makeOverlayIcon( KIcon( "task-reminder" ), "list-remove" ),
			m_settings.departureArrivalListType == DepartureList
			? i18nc("@action:inmenu", "Remove &Alarm for This Departure")
			: i18nc("@action:inmenu", "Remove &Alarm for This Arrival"), this );
	connect( actionRemoveAlarmForDeparture, SIGNAL(triggered()),
			 this, SLOT(removeAlarmForDeparture()) );
	addAction( "removeAlarmForDeparture", actionRemoveAlarmForDeparture );

	QAction *actionSearchJourneys = new QAction( KIcon( "edit-find" ),
			i18nc("@action", "Search for &Journeys..."), this );
	connect( actionSearchJourneys, SIGNAL( triggered() ),
			 this, SLOT( showJourneySearch() ) );
	addAction( "searchJourneys", actionSearchJourneys );

	int iconExtend = 32;
	QAction *actionShowDepartures = new QAction(
			GlobalApplet::makeOverlayIcon( KIcon("public-transport-stop"),
				QList<KIcon>() << KIcon("go-home") << KIcon("go-next"),
				QSize(iconExtend / 2, iconExtend / 2), iconExtend ),
			i18nc("@action", "Show &Departures"), this );
	connect( actionShowDepartures, SIGNAL( triggered() ),
	         this, SLOT( setShowDepartures() ) );
	addAction( "showDepartures", actionShowDepartures );

	QAction *actionShowArrivals = new QAction(
			GlobalApplet::makeOverlayIcon( KIcon( "public-transport-stop" ),
				QList<KIcon>() << KIcon( "go-next" ) << KIcon( "go-home" ),
				QSize( iconExtend / 2, iconExtend / 2 ), iconExtend ),
			i18nc("@action", "Show &Arrivals"), this );
	connect( actionShowArrivals, SIGNAL( triggered() ),
			 this, SLOT( setShowArrivals() ) );
	addAction( "showArrivals", actionShowArrivals );

	QAction *actionBackToDepartures = new QAction( KIcon( "go-previous" ),
			i18nc( "@action", "Back to &Departure List" ), this );
	connect( actionBackToDepartures, SIGNAL(triggered()),
			 this, SLOT(goBackToDepartures()) );
	addAction( "backToDepartures", actionBackToDepartures );

	KToggleAction *actionEnableFilters = new KToggleAction(
			i18nc( "@action", "&Enable Filters" ), this );
	connect( actionEnableFilters, SIGNAL(toggled(bool)), this, SLOT(setFiltersEnabled(bool)) );
	addAction( "enableFilters", actionEnableFilters );

	m_filtersGroup = new QActionGroup( this );
	m_filtersGroup->setExclusive( true );
	connect( m_filtersGroup, SIGNAL(triggered(QAction*)),
			 this, SLOT(switchFilterConfiguration(QAction*)) );

	KAction *actionFilterConfiguration = new KSelectAction( KIcon("view-filter"),
			i18nc("@action", "Filter"), this );
	KMenu *menu = new KMenu;
	menu->addAction( actionEnableFilters );
	menu->addSeparator();
	menu->addTitle( KIcon("view-filter"), i18nc("@item:inmenu", "Used Filter Configuration") );
	actionFilterConfiguration->setMenu( menu ); // TODO: Does this take ownership of menu?
	actionFilterConfiguration->setEnabled( true );
	addAction( "filterConfiguration", actionFilterConfiguration );

	QAction *actionToggleExpanded = new QAction( KIcon( "arrow-down" ),
			i18nc( "@action:inmenu", "&Show Additional Information" ), this );
	connect( actionToggleExpanded, SIGNAL(triggered()), this, SLOT(toggleExpanded()) );
	addAction( "toggleExpanded", actionToggleExpanded );

	// TODO: Combine actionHideHeader and actionShowHeader into one action
	QAction *actionHideHeader = new QAction( KIcon( "edit-delete" ),
	        i18nc( "@action:inmenu", "&Hide Header" ), this );
	connect( actionHideHeader, SIGNAL(triggered()), this, SLOT(hideHeader()) );
	addAction( "hideHeader", actionHideHeader );

	QAction *actionShowHeader = new QAction( KIcon("list-add"),
			i18nc("@action:inmenu", "Show &Header"), this );
	connect( actionShowHeader, SIGNAL(triggered()), this, SLOT(showHeader()) );
	addAction( "showHeader", actionShowHeader );

	// TODO: Combine actionHideColumnTarget and actionShowColumnTarget into one action
	QAction *actionHideColumnTarget = new QAction( KIcon("view-right-close"),
			i18nc("@action:inmenu", "Hide &target column"), this );
	connect( actionHideColumnTarget, SIGNAL(triggered()), this, SLOT(hideColumnTarget()) );
	addAction( "hideColumnTarget", actionHideColumnTarget );

	QAction *actionShowColumnTarget = new QAction( KIcon("view-right-new"),
			i18nc("@action:inmenu", "Show &target column"), this );
	connect( actionShowColumnTarget, SIGNAL(triggered()),
	         this, SLOT(showColumnTarget()) );
	addAction( "showColumnTarget", actionShowColumnTarget );
}

QList< QAction* > PublicTransport::contextualActions()
{
	QAction *switchDepArr = m_settings.departureArrivalListType == DepartureList
			? action( "showArrivals" ) : action( "showDepartures" );

	KAction *actionFilter = NULL;
	QStringList filterConfigurationList = m_settings.filterSettings.keys();
	if ( !filterConfigurationList.isEmpty() ) {
		actionFilter = qobject_cast< KAction* >( action( "filterConfiguration" ) );
		action( "enableFilters" )->setChecked( m_settings.filtersEnabled ); // TODO change checked state when filtersEnabled changes?
		QList< QAction* > oldActions = m_filtersGroup->actions();
		foreach( QAction *oldAction, oldActions ) {
			m_filtersGroup->removeAction( oldAction );
			delete oldAction;
		}

		QMenu *menu = actionFilter->menu();
		QString currentFilterConfig = m_settings.currentStopSettings().get<QString>(
				FilterConfigurationSetting );
		foreach( const QString &filterConfig, filterConfigurationList ) {
			QAction *action = new QAction( 
					GlobalApplet::translateFilterKey(filterConfig), m_filtersGroup );
			action->setCheckable( true );
			menu->addAction( action );
			if ( filterConfig == currentFilterConfig ) {
				action->setChecked( true );
			}
		}
		m_filtersGroup->setEnabled( m_settings.filtersEnabled );
	}

	QList< QAction* > actions;
	// Add actions: Update Timetable, Search Journeys
	actions << action( "updateTimetable" ); //<< action("showActionButtons")
	if ( m_currentServiceProviderFeatures.contains("JourneySearch") ) {
		actions << action( "searchJourneys" );
	}

	QAction *separator = new QAction( this );
	separator->setSeparator( true );
	actions.append( separator );

	// Add actions: Switch Departures/Arrivals, Switch Current Stop,
	// Switch Filter Configuration
	if ( m_currentServiceProviderFeatures.contains("Arrivals") ) {
		actions << switchDepArr;
	}
	if ( m_settings.stopSettingsList.count() > 1 ) {
		actions << switchStopAction( this );
	}
	if ( actionFilter ) {
		actions << actionFilter;
	}

	separator = new QAction( this );
	separator->setSeparator( true );
	actions.append( separator );

	return actions;
}

QVariantHash PublicTransport::serviceProviderData( const QString& id ) const
{
	Plasma::DataEngine::Data serviceProviderData =
			dataEngine( "publictransport" )->query( "ServiceProviders" );
	for ( Plasma::DataEngine::Data::const_iterator it = serviceProviderData.constBegin();
				it != serviceProviderData.constEnd(); ++it ) {
		QVariantHash data = serviceProviderData.value( it.key() ).toHash();
		if ( data["id"] == id ) {
			return data;
		}
	}

	kDebug() << "Service provider data for" << id << "not found";
	return QVariantHash();
}

void PublicTransport::updateDataSource()
{
	if ( testState( ShowingDepartureArrivalList ) ) {
		reconnectSource();
	} else {
		reconnectJourneySource();
	}
}

void PublicTransport::disconnectJourneySource()
{
	if ( !m_currentJourneySource.isEmpty() ) {
		kDebug() << "Disconnect journey data source" << m_currentJourneySource;
		dataEngine( "publictransport" )->disconnectSource( m_currentJourneySource, this );
	}
}

void PublicTransport::reconnectJourneySource( const QString& targetStopName,
		const QDateTime& dateTime, bool stopIsTarget, bool timeIsDeparture,
		bool requestStopSuggestions )
{
	disconnectJourneySource();

	QString _targetStopName = targetStopName;
	QDateTime _dateTime = dateTime;
	if ( _targetStopName.isEmpty() ) {
		if ( m_lastSecondStopName.isEmpty() ) {
			return;
		}
		_targetStopName = m_lastSecondStopName;
		_dateTime = m_lastJourneyDateTime;
	}

	// Build a source name for the publictransport data engine
	if ( requestStopSuggestions ) {
		m_currentJourneySource = QString( "Stops %1|stop=%2" )
				.arg( m_settings.currentStopSettings().get<QString>(ServiceProviderSetting) )
				.arg( _targetStopName );
	} else {
		m_currentJourneySource = QString( stopIsTarget
				? "%6 %1|originStop=%2|targetStop=%3|maxCount=%4|datetime=%5"
				: "%6 %1|originStop=%3|targetStop=%2|maxCount=%4|datetime=%5" )
					.arg( m_settings.currentStopSettings().get<QString>(ServiceProviderSetting) )
					.arg( m_settings.currentStopSettings().stop(0).nameOrId() )
					.arg( _targetStopName )
					.arg( m_settings.maximalNumberOfDepartures )
					.arg( _dateTime.toString() )
					.arg( timeIsDeparture ? "Journeys" : "JourneysArr" );
		QString currentStop = m_settings.currentStopSettings().stops().first();
		m_journeyTitleText = stopIsTarget
				? i18nc("@info", "From %1<nl/>to <emphasis strong='1'>%2</emphasis>",
						currentStop, _targetStopName)
				: i18nc("@info", "From <emphasis strong='1'>%1</emphasis><nl/>to %2",
						_targetStopName, currentStop);
		if ( testState(ShowingJourneyList) ) {
			m_titleWidget->setTitle( m_journeyTitleText );
		}
	}

	if ( !m_settings.currentStopSettings().get<QString>(CitySetting).isEmpty() ) { // TODO CHECK useSeparateCityValue )
		m_currentJourneySource += QString( "|city=%1" ).arg(
				m_settings.currentStopSettings().get<QString>(CitySetting) );
	}

	kDebug() << "Connect journey data source" << m_currentJourneySource
			 << "Autoupdate" << m_settings.autoUpdate;
	m_lastSecondStopName = _targetStopName;
	addState( WaitingForJourneyData );
	dataEngine( "publictransport" )->connectSource( m_currentJourneySource, this );
}

void PublicTransport::disconnectSources()
{
	if ( !m_currentSources.isEmpty() ) {
		foreach( const QString &currentSource, m_currentSources ) {
			kDebug() << "Disconnect data source" << currentSource;
			dataEngine( "publictransport" )->disconnectSource( currentSource, this );
		}
		m_currentSources.clear();
	}
}

void PublicTransport::reconnectSource()
{
	disconnectSources();

	// Get a list of stops (or stop IDs if available) which results are currently shown
	StopSettings curStopSettings = m_settings.currentStopSettings();
	QStringList stops = curStopSettings.stops();
	QStringList stopIDs = curStopSettings.stopIDs();
	if ( stopIDs.isEmpty() ) {
		stopIDs = stops;
	}

	// Build source names for each (combined) stop for the publictransport data engine
	kDebug() << "Connect" << m_settings.currentStopSettingsIndex << stops;
	QStringList sources;
	m_stopIndexToSourceName.clear();
	for ( int i = 0; i < stops.count(); ++i ) {
		QString stopValue = stopIDs[i].isEmpty() ? stops[i] : stopIDs[i];
		QString currentSource = QString( "%4 %1|stop=%2" )
				.arg( m_settings.currentStopSettings().get<QString>(ServiceProviderSetting) )
				.arg( stopValue )
				.arg( m_settings.departureArrivalListType == ArrivalList
					? "Arrivals" : "Departures" );
		if ( static_cast<FirstDepartureConfigMode>(curStopSettings.get<int>(
			FirstDepartureConfigModeSetting)) == RelativeToCurrentTime ) 
		{
			currentSource += QString( "|timeOffset=%1" ).arg(
					curStopSettings.get<int>(TimeOffsetOfFirstDepartureSetting) );
		} else {
			currentSource += QString( "|time=%1" ).arg(
					curStopSettings.get<QTime>(TimeOfFirstDepartureSetting).toString("hh:mm") );
		}
		if ( !curStopSettings.get<QString>(CitySetting).isEmpty() ) {
			currentSource += QString( "|city=%1" ).arg( curStopSettings.get<QString>(CitySetting) );
		}

		m_stopIndexToSourceName[ i ] = currentSource;
		sources << currentSource;
	}

	foreach( const QString &currentSource, sources ) {
		kDebug() << "Connect data source" << currentSource
				 << "Autoupdate" << m_settings.autoUpdate;
		m_currentSources << currentSource;
		if ( m_settings.autoUpdate ) {
			// Update once a minute
			dataEngine( "publictransport" )->connectSource( currentSource, this,
			        60000, Plasma::AlignToMinute );
		} else {
			dataEngine( "publictransport" )->connectSource( currentSource, this );
		}
	}

	addState( WaitingForDepartureData );
}

void PublicTransport::departuresFiltered( const QString& sourceName,
		const QList< DepartureInfo > &departures,
		const QList< DepartureInfo > &newlyFiltered,
		const QList< DepartureInfo > &newlyNotFiltered )
{
	if ( m_departureInfos.contains( sourceName ) ) {
		m_departureInfos[ sourceName ] = departures;
	} else {
		kDebug() << "Source name not found" << sourceName << "in" << m_departureInfos.keys();
		return;
	}

	// Remove previously visible and now filtered out departures
	kDebug() << "Remove" << newlyFiltered.count() << "previously unfiltered departures, if they are visible";
	foreach( const DepartureInfo &departureInfo, newlyFiltered ) {
		int row = m_model->indexFromInfo( departureInfo ).row();
		if ( row == -1 ) {
			kDebug() << "Didn't find departure" << departureInfo;
		} else {
			m_model->removeItem( m_model->itemFromInfo(departureInfo) );
		}
	}

	// Append previously filtered out departures
	kDebug() << "Add" << newlyNotFiltered.count() << "previously filtered departures";
	foreach( const DepartureInfo &departureInfo, newlyNotFiltered ) {
		m_model->addItem( departureInfo );
	}

	// Limit item count to the maximal number of departure setting
	int delta = m_model->rowCount() - m_settings.maximalNumberOfDepartures;
	if ( delta > 0 ) {
		m_model->removeRows( m_settings.maximalNumberOfDepartures, delta );
	}
	
	createDepartureGroups();
	createPopupIcon();
	createTooltip();
}

void PublicTransport::beginJourneyProcessing( const QString &/*sourceName*/ )
{
	// Clear old journey list
	m_journeyInfos.clear();
}

void PublicTransport::journeysProcessed( const QString &/*sourceName*/,
        const QList< JourneyInfo > &journeys, const QUrl &requestUrl,
        const QDateTime &/*lastUpdate*/ )
{
	// Set associated app url
	setAssociatedApplicationUrls( associatedApplicationUrls() << requestUrl );

	// Append new journeys
	kDebug() << journeys.count() << "journeys received from thread";
	m_journeyInfos << journeys;

	// Fill the model with the received journeys
	fillModelJourney( journeys );
}

void PublicTransport::beginDepartureProcessing( const QString& sourceName )
{
	// Clear old departure / arrival list
	QString strippedSourceName = stripDateAndTimeValues( sourceName );
	m_departureInfos[ strippedSourceName ].clear();
}

void PublicTransport::departuresProcessed( const QString& sourceName,
		const QList< DepartureInfo > &departures, const QUrl &requestUrl,
		const QDateTime &lastUpdate )
{
	// Set associated app url
	m_urlDeparturesArrivals = requestUrl;
	if ( testState( ShowingDepartureArrivalList ) || testState( ShowingJourneySearch )
				|| testState( ShowingJourneysNotSupported ) ) {
		setAssociatedApplicationUrls( KUrl::List() << requestUrl );
	}

	// Put departures into the cache
	const QString strippedSourceName = stripDateAndTimeValues( sourceName );
	m_departureInfos[ strippedSourceName ] << departures;

	// Remove config needed messages
	setConfigurationRequired( false );
	m_stopNameValid = true;

	// Update "last update" time
	if ( lastUpdate > m_lastSourceUpdate ) {
		m_lastSourceUpdate = lastUpdate;
	}
	m_labelInfo->setText( infoText() );

	// Fill the model with the received departures
	fillModel( departures );

	// TODO: get total received departures count from thread and only create these at the end
	// These functions are now called inside fillModel()
// 	createTooltip();
// 	createPopupIcon();
}

QString PublicTransport::stripDateAndTimeValues( const QString& sourceName ) const
{
	QString ret = sourceName;
	QRegExp rx( "(time=[^\\|]*|datetime=[^\\|]*)", Qt::CaseInsensitive );
	rx.setMinimal( true );
	ret.replace( rx, QChar() );
	return ret;
}

QList< DepartureInfo > PublicTransport::departureInfos() const
{
	QList< DepartureInfo > ret;

	for ( int n = m_stopIndexToSourceName.count() - 1; n >= 0; --n ) {
		QString sourceName = stripDateAndTimeValues( m_stopIndexToSourceName[n] );
		if ( m_departureInfos.contains( sourceName ) ) {
			foreach( const DepartureInfo &departureInfo, m_departureInfos[sourceName] ) {
				// Only add not filtered items
				if ( !departureInfo.isFilteredOut() ) {
					ret << departureInfo;
				}
			}
		}
	}

	qSort( ret.begin(), ret.end() );
	return ret.mid( 0, m_settings.maximalNumberOfDepartures );
}

void PublicTransport::clearDepartures()
{
	m_departureInfos.clear(); // Clear data from data engine
	m_model->clear(); // Clear data to be displayed
}

void PublicTransport::clearJourneys()
{
	m_journeyInfos.clear(); // Clear data from data engine
	m_modelJourneys->clear(); // Clear data to be displayed
}

void PublicTransport::handleDataError( const QString& /*sourceName*/,
                                       const Plasma::DataEngine::Data& data )
{
	if ( data["parseMode"].toString() == "journeys" ) {
		addState( ReceivedErroneousJourneyData );

		// Set associated application url
		QUrl url = data["requestUrl"].toUrl();
		kDebug() << "Errorneous journey url" << url;
		m_urlJourneys = url;
		if ( testState( ShowingJourneyList ) ) {
			setAssociatedApplicationUrls( KUrl::List() << url );
		}
	} else if ( data["parseMode"].toString() == "departures" ) {
		addState( ReceivedErroneousDepartureData );
		m_stopNameValid = false;

		// Set associated application url
		QUrl url = data["requestUrl"].toUrl();
		kDebug() << "Errorneous departure/arrival url" << url;
		m_urlDeparturesArrivals = url;
		if ( testState( ShowingDepartureArrivalList ) || testState( ShowingJourneySearch )
					|| testState( ShowingJourneysNotSupported ) ) {
			setAssociatedApplicationUrls( KUrl::List() << url );
		}

// 	    if ( testState(ServiceProviderSettingsJustChanged) ) {
		QString error = data["errorString"].toString();
		if ( error.isEmpty() ) {
			if ( m_currentMessage == MessageNone ) {
				if ( m_settings.departureArrivalListType == DepartureList ) {
					setConfigurationRequired( true, i18nc("@info", "Error parsing "
							"departure information or currently no departures") );
				} else {
					setConfigurationRequired( true, i18nc("@info", "Error parsing "
							"arrival information or currently no arrivals") );
				}
			}
		} else {
			m_currentMessage = MessageError;
			if ( checkNetworkStatus() ) {
				m_timetable->setNoItemsText( i18nc("@info/plain", "There was an error:<nl/>"
						"<message>%1</message><nl/><nl/>"
						"The server may be temporarily unavailable.", error) );
			}
		}
// 	    }
	}
}

void PublicTransport::processStopSuggestions( const QString &/*sourceName*/,
		const Plasma::DataEngine::Data &data )
{
	bool journeyData = data["parseMode"].toString() == "journeys";
	if ( journeyData || data["parseMode"].toString() == "stopSuggestions" ) {
		if ( journeyData ) {
			addState( ReceivedErroneousJourneyData );
		}

		m_listStopSuggestions->updateStopSuggestionItems( data );
	} else if ( data["parseMode"].toString() == "departures" && m_currentMessage == MessageNone ) {
		m_stopNameValid = false;
		addState( ReceivedErroneousDepartureData );
		clearDepartures();
		setConfigurationRequired( true, i18nc( "@info", "The stop name is ambiguous." ) );
	}
}

void PublicTransport::dataUpdated( const QString& sourceName,
								   const Plasma::DataEngine::Data& data )
{
	if ( data.isEmpty() || ( !m_currentSources.contains( sourceName )
							 && sourceName != m_currentJourneySource ) ) {
		// Source isn't used anymore
		kDebug() << "Data discarded" << sourceName;
		return;
	}

	if ( data.value( "error" ).toBool() ) {
		// Error while parsing the data or no connection to server
		handleDataError( sourceName, data );
	} else if ( data.value( "receivedPossibleStopList" ).toBool() ) {
		// Possible stop list received
		processStopSuggestions( sourceName, data );
	} else { // List of departures / arrivals / journeys received
		if ( data["parseMode"].toString() == "journeys" ) {
			addState( ReceivedValidJourneyData );
			if ( testState( ShowingJourneyList ) ) {
				m_departureProcessor->processJourneys( sourceName, data );
			} else {
				kDebug() << "Received journey data, but journey list is hidden.";
			}
		} else if ( data["parseMode"].toString() == "departures" ) {
			m_stopNameValid = true;
			addState( ReceivedValidDepartureData );
			m_departureProcessor->processDepartures( sourceName, data );
		}
	}

	removeState( SettingsJustChanged );
	removeState( ServiceProviderSettingsJustChanged );
}

void PublicTransport::geometryChanged()
{
	m_labelInfo->setText( infoText() );

	// Adjust column sizes
// 	QHeaderView *header = m_treeView->nativeWidget()->header();
// 	int lastIndex = header->count() - 1;
// 	header->resizeSection( lastIndex, header->sectionSize(lastIndex) - 1 );
}

void PublicTransport::popupEvent( bool show )
{
	destroyOverlay();

	// Hide opened journey views, ie. back to departure view
	addState( ShowingDepartureArrivalList );

	Plasma::PopupApplet::popupEvent( show );
}

void PublicTransport::wheelEvent( QGraphicsSceneWheelEvent* event )
{
    QGraphicsItem::wheelEvent( event );
	
	int oldDepartureSpan = qAbs( m_endPopupIconDepartureIndex - m_startPopupIconDepartureIndex );
	int oldEndPopupIconDepartureIndex = m_endPopupIconDepartureIndex;
	
	// Compute start and end indices of the departure groups to animate between
	if ( event->delta() > 0 ) {
		// Wheel rotated forward
		if ( m_endPopupIconDepartureIndex < POPUP_ICON_DEPARTURE_GROUP_COUNT - 1 ) {
			if ( m_popupIconTransitionAnimation ) {
				// Already animating
				if ( m_endPopupIconDepartureIndex < m_startPopupIconDepartureIndex ) {
					// Direction was reversed
					m_startPopupIconDepartureIndex = m_endPopupIconDepartureIndex;
				}
				
				// Increase index of the departure group where the animation should end
				++m_endPopupIconDepartureIndex;
			} else {
				m_startPopupIconDepartureIndex = qFloor( m_popupIconDepartureIndex );
				m_endPopupIconDepartureIndex = m_startPopupIconDepartureIndex + 1;
			}
		} else {
			// Max departure already reached
			return;
		}
	} else if ( event->delta() < 0 ) {
		// Wheel rotated backward
		int minIndex = m_model->hasAlarms() ? -1 : 0; // Show alarm at index -1
		if ( m_endPopupIconDepartureIndex > minIndex ) {
			if ( m_popupIconTransitionAnimation ) {
				// Already animating
				if ( m_endPopupIconDepartureIndex > m_startPopupIconDepartureIndex ) {
					// Direction was reversed
					m_startPopupIconDepartureIndex = m_endPopupIconDepartureIndex;
				}
				
				// Decrease index of the departure group where the animation should end
				--m_endPopupIconDepartureIndex;
			} else {
				m_startPopupIconDepartureIndex = qFloor( m_popupIconDepartureIndex );
				m_endPopupIconDepartureIndex = m_startPopupIconDepartureIndex - 1;
			}
		} else {
			// Min departure or alarm departure already reached
			return;
		}
	} else {
		// Wheel not rotated?
		return;
	}
	
	if ( m_popupIconTransitionAnimation ) {
		// Compute new starting index from m_popupIconDepartureIndex
		int newDepartureSpan = qAbs( m_endPopupIconDepartureIndex - m_startPopupIconDepartureIndex );
		qreal animationPartDone = qAbs(m_popupIconDepartureIndex - m_startPopupIconDepartureIndex)
				/ oldDepartureSpan;
		if ( animationPartDone > 0.5 ) {
			// Running animation visually almost finished (the easing curve slows the animation down at the end)
			m_startPopupIconDepartureIndex = oldEndPopupIconDepartureIndex;
			m_popupIconTransitionAnimation->stop();
			m_popupIconTransitionAnimation->setStartValue( m_startPopupIconDepartureIndex );
		} else {
			qreal startIconDepartureIndex = m_startPopupIconDepartureIndex + animationPartDone * newDepartureSpan;
			m_popupIconTransitionAnimation->stop();
			m_popupIconTransitionAnimation->setStartValue( startIconDepartureIndex );
		}
	} else {
		// Create animation
		m_popupIconTransitionAnimation = new QPropertyAnimation( this, "PopupIconDepartureIndex", this );
		m_popupIconTransitionAnimation->setEasingCurve( QEasingCurve(QEasingCurve::OutQuart) );
		m_popupIconTransitionAnimation->setDuration( 500 );
		m_popupIconTransitionAnimation->setStartValue( m_startPopupIconDepartureIndex );
		connect( m_popupIconTransitionAnimation, SIGNAL(finished()), 
				 this, SLOT(popupIconTransitionAnimationFinished()) );
	}
	
	m_popupIconTransitionAnimation->setEndValue( m_endPopupIconDepartureIndex );
	m_popupIconTransitionAnimation->start();
}

void PublicTransport::departuresAboutToBeRemoved( const QList<ItemBase*>& departures )
{
	QMap< QDateTime, QList<DepartureItem*> >::iterator it = m_departureGroups.begin();
	while ( it != m_departureGroups.end() ) {
		// Remove all departures in the current group 
		// that are inside the given list of departures to be removed
		QList<DepartureItem*>::iterator sub_it = it.value().begin();
		while ( sub_it != it.value().end() ) {
			if ( departures.contains(*sub_it) ) {
				sub_it = it.value().erase( sub_it );
			} else {
				++sub_it;
			}
		}
		
		// Remove the group if all it's departures have been removed
		if ( it.value().isEmpty() ) {
			it = m_departureGroups.erase( it );
		} else {
			++it;
		}
	}
}

void PublicTransport::departuresLeft( const QList< DepartureInfo > &departures )
{
	Q_UNUSED( departures );
}

void PublicTransport::popupIconTransitionAnimationFinished()
{
	delete m_popupIconTransitionAnimation;
	m_popupIconTransitionAnimation = NULL;
}

void PublicTransport::createDepartureGroups()
{
	m_departureGroups.clear();
	
	// Create departure groups (maximally 5 groups)
	for ( int row = 0; row < m_model->rowCount(); ++row ) {
		DepartureItem *item = dynamic_cast<DepartureItem*>( m_model->item(row) );
		const DepartureInfo *info = item->departureInfo();
		
		QDateTime time = info->predictedDeparture();
		if ( m_departureGroups.count() == POPUP_ICON_DEPARTURE_GROUP_COUNT && !m_departureGroups.contains(time) ) {
			// Maximum group count reached and all groups filled
			break;
		} else {
			// Insert item into the corresponding departure group
			m_departureGroups[ time ] << item;
		}
	}
}

QPixmap PublicTransport::createDeparturesPixmap( const QList<DepartureItem*> &departures )
{
	QPixmap pixmap( 128, 128 );
	pixmap.fill( Qt::transparent );
	QPainter p( &pixmap );
	
// 			paintVehicle( &p, nextDeparture.vehicleType(), QRectF(0, 0, 128, 128), 
// 						  nextDeparture.lineString() );
	
	QRectF vehicleRect( 0, 0, pixmap.width(), pixmap.height() );
	qreal factor = departures.count() == 1 ? 1.0 : 1.0 / (0.75 * departures.count());
	vehicleRect.setWidth( vehicleRect.width() * factor );
	vehicleRect.setHeight( vehicleRect.height() * factor );
	qreal translation = departures.count() == 1 ? 0.0 
			: (128 - vehicleRect.width()) / (departures.count() - 1);
	QDateTime time;
	foreach ( const DepartureItem *item, departures ) {
		if ( !item ) {
			continue;
		}
		
		const DepartureInfo *data = item->departureInfo();
		paintVehicle( &p, data->vehicleType(), vehicleRect, data->lineString() );
		
		// Move to next vehicle type svg position
		vehicleRect.translate( translation, translation );
		
		if ( !time.isValid() ) {
			time = item->departureInfo()->predictedDeparture();
		}
	}
	
	QDateTime currentTime = QDateTime::currentDateTime();
	int minsToDeparture = qCeil( currentTime.secsTo(time) / 60.0 );
	QString text;
	if ( minsToDeparture < 0 ) {
		text.append( i18n("leaving") );
	} else if ( minsToDeparture == 0 ) {
		text.append( i18n("now") );
	} else if ( minsToDeparture >= 60 * 24 ) {
		text.append( i18np("1 day", "%1 days", qRound(minsToDeparture / (6 * 24)) / 10.0) );
	} else if ( minsToDeparture >= 60 ) {
		kDebug() << "HOURS" << (qRound(minsToDeparture / 6) / 10.0) << minsToDeparture;
		text.append( i18np("1 hour", "%1 hours", qRound(minsToDeparture / 6) / 10.0) );
	} else {
		text.append( i18np("1 min.", "%1 min.", minsToDeparture) );
	}
	
	QFont font = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
	font.setPixelSize( pixmap.width() / 4 );
	font.setBold( true );
	p.setFont( font );
	QFontMetrics fm( font );
	int textWidth = fm.width( text );
	QRectF textRect( 0, 0, pixmap.width(), pixmap.height() );
	QRectF haloRect( textRect.left() + (textRect.width() - textWidth) / 2,
					 textRect.bottom() - fm.height(), textWidth, fm.height() );
	haloRect = haloRect.intersected( textRect ).adjusted( 3, 3, -3, -3 );
	QTextOption option(Qt::AlignHCenter | Qt::AlignBottom);
	option.setWrapMode( QTextOption::NoWrap );
	
	Plasma::PaintUtils::drawHalo( &p, haloRect );
	p.drawText( textRect, fm.elidedText(text, Qt::ElideRight, pixmap.width()), option );
	
	p.end();
	return pixmap;
}

QPixmap PublicTransport::createAlarmPixmap( DepartureItem* departure )
{
	QPixmap pixmap = createDeparturesPixmap( QList<DepartureItem*>() << departure );
	int iconSize = pixmap.width() / 2;
	QPixmap pixmapAlarmIcon = KIcon( "task-reminder" ).pixmap( iconSize );
	QPainter p( &pixmap );
	// Draw alarm icon in the top-right corner.
	Plasma::PaintUtils::drawHalo( &p, QRectF(pixmap.width() - iconSize * 0.85 - 1, 1, iconSize * 0.7, iconSize) );
	p.drawPixmap( pixmap.width() - iconSize - 1, 1, pixmapAlarmIcon );
	p.end();
	
	return pixmap;
}

void PublicTransport::createPopupIcon()
{
	if ( m_model->isEmpty() ) {
		setPopupIcon( "public-transport-stop" );
	} else {
		if ( m_departureGroups.isEmpty() ) {
			setPopupIcon( "public-transport-stop" );
		} else {
			QPixmap pixmap;
			if ( qFuzzyCompare((qreal)qFloor(m_popupIconDepartureIndex), m_popupIconDepartureIndex) ) {
				// If m_popupIconDepartureIndex is an integer draw without transition
				int popupIconDepartureIndex = qBound( m_model->hasAlarms() ? -1 : 0, 
						qFloor(m_popupIconDepartureIndex), m_departureGroups.count() - 1 );
				
				if ( popupIconDepartureIndex < 0 ) {
					pixmap = createAlarmPixmap( m_model->nextAlarmDeparture() );
				} else {
					QDateTime time = m_departureGroups.keys()[ popupIconDepartureIndex ];
					pixmap = createDeparturesPixmap( m_departureGroups[time] );
				}
			} else {
				// Draw transition
				int startPopupIconDepartureIndex = qBound( m_model->hasAlarms() ? -1 : 0, 
						m_startPopupIconDepartureIndex, m_departureGroups.count() - 1 );
				int endPopupIconDepartureIndex = qBound( m_model->hasAlarms() ? -1 : 0, 
						m_endPopupIconDepartureIndex, m_departureGroups.count() - 1 );
				QPixmap startPixmap = startPopupIconDepartureIndex < 0
						? createAlarmPixmap(m_model->nextAlarmDeparture())
						: createDeparturesPixmap( 
						  m_departureGroups[m_departureGroups.keys()[startPopupIconDepartureIndex]] );
				QPixmap endPixmap = endPopupIconDepartureIndex < 0
						? createAlarmPixmap(m_model->nextAlarmDeparture())
						: createDeparturesPixmap( 
						  m_departureGroups[m_departureGroups.keys()[endPopupIconDepartureIndex]] );
				
				pixmap = QPixmap( 128, 128 );
				pixmap.fill( Qt::transparent );
				QPainter p( &pixmap );
				p.setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
				
				qreal transition, startSize, endSize;
				if ( endPopupIconDepartureIndex > startPopupIconDepartureIndex  ) {
					// Move forward to next departure
					transition = qBound( 0.0, (m_popupIconDepartureIndex - startPopupIconDepartureIndex)
							/ (endPopupIconDepartureIndex - startPopupIconDepartureIndex), 1.0 );
				} else {
					// Mave backward to previous departure
					transition = 1.0 - qBound( 0.0, (startPopupIconDepartureIndex - m_popupIconDepartureIndex)
							/ (startPopupIconDepartureIndex - endPopupIconDepartureIndex), 1.0 );
					qSwap( startPixmap, endPixmap );
				}
				startSize = (1.0 + 0.25 * transition) * pixmap.width();
				endSize = transition * pixmap.width();
				
				p.drawPixmap( (pixmap.width() - endSize) / 2 + pixmap.width() * (1.0 - transition) / 2.0, 
							  (pixmap.height() - endSize) / 2, 
							  endSize, endSize, endPixmap );
				
				QPixmap startTransitionPixmap( pixmap.size() );
				startTransitionPixmap.fill( Qt::transparent );
				QPainter p2( &startTransitionPixmap );
				p2.drawPixmap( 0, 0, pixmap.width(), pixmap.height(), startPixmap );
				
				// Make startTransitionPixmap more transparent (for fading)
				p2.setCompositionMode( QPainter::CompositionMode_DestinationIn );
				p2.fillRect( startTransitionPixmap.rect(), QColor(0, 0, 0, 255 * (1.0 - transition * transition)) );
				p2.end();
				
				p.setTransform( QTransform().rotate(transition * 90, Qt::YAxis) );
				p.drawPixmap( (pixmap.width() - startSize) / 2 - pixmap.width() * transition / 5.0, 
							  (pixmap.height() - startSize) / 2,
							  startSize, startSize, startTransitionPixmap );
				p.end();
			}
			
			KIcon icon;
			icon.addPixmap( pixmap );
			setPopupIcon( icon );
		}
	}
}

void PublicTransport::createTooltip()
{
	if ( isPopupShowing() || (formFactor() != Plasma::Horizontal
						  && formFactor() != Plasma::Vertical) ) {
		return;
	}

	Plasma::ToolTipContent data;
	data.setMainText( i18nc("@info", "Public Transport") );
	if ( m_model->isEmpty() ) {
		data.setSubText( i18nc("@info", "View departure times for public transport") );
	} else {
		QList<DepartureInfo> depInfos = departureInfos();
		if ( !depInfos.isEmpty() ) {
			DepartureInfo nextDeparture = depInfos.first();

			if ( m_settings.departureArrivalListType ==  DepartureList ) {
				// Showing a departure list
				if ( m_settings.currentStopSettings().stops().count() == 1 ) {
					// Only one stop (not combined with others)
					data.setSubText( i18nc("@info %4 is the translated duration text, e.g. in 3 minutes",
							"Next departure from '%1': line %2 (%3) %4",
							m_settings.currentStopSettings().stops().first(), 
							nextDeparture.lineString(), nextDeparture.target(), 
							nextDeparture.durationString()) );
				} else {
					// Results for multiple combined stops are shown
					data.setSubText( i18nc("@info %3 is the translated duration text, e.g. in 3 minutes",
							"Next departure from your home stop: line %1 (%2) %3",
							nextDeparture.lineString(), nextDeparture.target(),
							nextDeparture.durationString()) );
				}
			} else {
				// Showing an arrival list
				if ( m_settings.currentStopSettings().stops().count() == 1 ) {
					// Only one stop (not combined with others)
					data.setSubText( i18nc("@info %4 is the translated duration text, e.g. in 3 minutes",
							"Next arrival at '%1': line %2 (%3) %4",
							m_settings.currentStopSettings().stops().first(), nextDeparture.lineString(),
							nextDeparture.target(), nextDeparture.durationString()) );
				} else {
					// Results for multiple combined stops are shown
					data.setSubText( i18nc("@info %3 is the translated duration text, e.g. in 3 minutes",
							"Next arrival at your home stop: line %1 (%2) %3",
							nextDeparture.lineString(), nextDeparture.target(),
							nextDeparture.durationString()) );
				}
			}
		}
	}

	data.setImage( KIcon("public-transport-stop").pixmap(IconSize(KIconLoader::Desktop)) );
	Plasma::ToolTipManager::self()->setContent( this, data );
}

void PublicTransport::paintVehicle( QPainter* painter, VehicleType vehicle, 
									const QRectF& rect, const QString &transportLine )
{
	// Draw transport line string onto the vehicle type svg 
	// only if activated in the settings and a supported vehicle type 
	// (currently only local public transport)
	const bool m_drawTransportLine = true; // TODO Make configurable?
	bool drawTransportLine = m_drawTransportLine && !transportLine.isEmpty()
			&& Timetable::Global::generalVehicleType(vehicle) == LocalPublicTransport;
	
	QString vehicleKey;
	switch ( vehicle ) {
		case Tram: vehicleKey = "tram"; break;
		case Bus: vehicleKey = "bus"; break;
		case TrolleyBus: vehicleKey = "trolleybus"; break;
		case Subway: vehicleKey = "subway"; break;
		case Metro: vehicleKey = "metro"; break;
		case InterurbanTrain: vehicleKey = "interurbantrain"; break;
		case RegionalTrain: vehicleKey = "regionaltrain"; break;
		case RegionalExpressTrain: vehicleKey = "regionalexpresstrain"; break;
		case InterregionalTrain: vehicleKey = "interregionaltrain"; break;
		case IntercityTrain: vehicleKey = "intercitytrain"; break;
		case HighSpeedTrain: vehicleKey = "highspeedtrain"; break;
		case Feet: vehicleKey = "feet"; break;
		case Ship: vehicleKey = "ship"; break;
		case Plane: vehicleKey = "plane"; break;
		default:
			kDebug() << "Unknown vehicle type" << vehicle;
			return; // TODO: draw a simple circle or something.. or an unknown vehicle type icon
	}
	if ( drawTransportLine ) {
		vehicleKey.append( "_empty" );
	}
	if ( !m_vehiclesSvg.hasElement(vehicleKey) ) {
		kDebug() << "SVG element" << vehicleKey << "not found";
		return;
	}
	
	int shadowWidth = 4;
    m_vehiclesSvg.resize( rect.width() - 2 * shadowWidth, rect.height() - 2 * shadowWidth );
	
	QPixmap pixmap( (int)rect.width(), (int)rect.height() );
	pixmap.fill( Qt::transparent );
	QPainter p( &pixmap );
    m_vehiclesSvg.paint( &p, shadowWidth, shadowWidth, vehicleKey );
	
	// Draw transport line string (only for local public transport)
	if ( drawTransportLine ) {
		QString text = transportLine;
		text.replace(' ', "");
		
		QFont f = font();
		f.setBold( true );
		if ( text.length() > 2 ) {
			f.setPixelSize( qMax(8, qCeil(1.2 * rect.width() / text.length())) );
		} else {
			f.setPixelSize( rect.width() * 0.55 );
		}
		p.setFont( f );
		p.setPen( Qt::white );
		
		QRect textRect( shadowWidth, shadowWidth, 
						rect.width() - 2 * shadowWidth, rect.height() - 2 * shadowWidth );
		p.drawText( textRect, text, QTextOption(Qt::AlignCenter) );
	}
	
	QImage shadow = pixmap.toImage();
	Plasma::PaintUtils::shadowBlur( shadow, shadowWidth - 1, Qt::black );
	painter->drawImage( rect.topLeft() + QPoint(1, 2), shadow );
	painter->drawPixmap( rect.topLeft(), pixmap );
}

void PublicTransport::configChanged()
{
	disconnect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
	addState( SettingsJustChanged );

	// Apply show departures/arrivals setting
	m_model->setDepartureArrivalListType( m_settings.departureArrivalListType );

	// Apply header settings
	if ( testState(ShowingDepartureArrivalList) ) {
		m_timetable->setTargetHidden( m_settings.hideColumnTarget );
		m_timetable->updateItemLayouts();
	}

	// Get fonts
	QFont font = m_settings.sizedFont();
	int smallPointSize = KGlobalSettings::smallestReadableFont().pointSize() * m_settings.sizeFactor;
	QFont smallFont = font/*, boldFont = font*/;
	smallFont.setPointSize( smallPointSize > 0 ? smallPointSize : 1 );

	// Apply fonts
	m_timetable->setFont( font );
	if ( m_journeyTimetable ) {
		m_journeyTimetable->setFont( font );
	}
	m_labelInfo->setFont( smallFont );

	// Update indentation and icon sizes to size factor
	m_timetable->setZoomFactor( m_settings.sizeFactor );

	// Update title widget to settings
	m_titleWidget->settingsChanged();

	// Update info label
	m_labelInfo->setToolTip( courtesyToolTip() );
	m_labelInfo->setText( infoText() );

	// Update text in the departure/arrival view, if no items are in the model 
	// TODO this is a copy of code in line ~2199
	if ( m_settings.departureArrivalListType == ArrivalList ) {
		m_timetable->setNoItemsText( m_settings.filtersEnabled
				? i18nc("@info/plain", "No unfiltered arrivals.<nl/>You can disable filters "
						"to see all arrivals.")
				: i18nc("@info/plain", "No arrivals.") );
	} else {
		m_timetable->setNoItemsText( m_settings.filtersEnabled
				? i18nc("@info/plain", "No unfiltered departures.<nl/>You can disable filters "
						"to see all departures.")
				: i18nc("@info/plain", "No departures.") );
	}

	// Apply filter, first departure and alarm settings to the worker thread
	m_departureProcessor->setFilterSettings( m_settings.currentFilterSettings(),
			m_settings.filtersEnabled );
	StopSettings stopSettings = m_settings.currentStopSettings();
	m_departureProcessor->setFirstDepartureSettings(
			static_cast<FirstDepartureConfigMode>(stopSettings.get<int>(
				FirstDepartureConfigModeSetting)),
			stopSettings.get<QTime>(TimeOfFirstDepartureSetting),
			stopSettings.get<int>(TimeOffsetOfFirstDepartureSetting) );
	m_departureProcessor->setAlarmSettings( m_settings.alarmSettings );

	// Apply other settings to the model
	m_timetable->setMaxLineCount( m_settings.linesPerRow );
	m_model->setLinesPerRow( m_settings.linesPerRow );
	m_model->setSizeFactor( m_settings.sizeFactor );
	m_model->setDepartureColumnSettings( m_settings.displayTimeBold,
			m_settings.showRemainingMinutes, m_settings.showDepartureTime );
	m_model->setAlarmSettings( m_settings.alarmSettings );
	m_model->setAlarmMinsBeforeDeparture( 
			m_settings.currentStopSettings().get<int>(AlarmTimeSetting) );

	// Limit model item count to the maximal number of departures setting
	if ( m_model->rowCount() > m_settings.maximalNumberOfDepartures ) {
		m_model->removeRows( m_settings.maximalNumberOfDepartures,
							 m_model->rowCount() - m_settings.maximalNumberOfDepartures );
	}

	connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
}

void PublicTransport::serviceProviderSettingsChanged()
{
	addState( ServiceProviderSettingsJustChanged );
	if ( m_settings.checkConfig() ) {
		setConfigurationRequired( false );
		reconnectSource();

		if ( !m_currentJourneySource.isEmpty() ) {
			reconnectJourneySource();
		}
	} else {
		setConfigurationRequired( true, i18nc("@info/plain", "Please check your configuration.") );
	}
}

void PublicTransport::iconClicked()
{
	switch ( m_titleWidget->titleType() ) {
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

void PublicTransport::destroyOverlay()
{
	if ( m_overlay ) {
		m_overlay->destroy();
		m_overlay = NULL;
	}
}

KSelectAction* PublicTransport::switchStopAction( QObject *parent,
        bool destroyOverlayOnTrigger ) const
{
	KSelectAction *switchStopAction = new KSelectAction(
			KIcon("public-transport-stop"), i18nc("@action", "Switch Current Stop"), parent );
	for ( int i = 0; i < m_settings.stopSettingsList.count(); ++i ) {
		QString stopList = m_settings.stopSettingsList[ i ].stops().join( ",\n" );
		QString stopListShort = m_settings.stopSettingsList[ i ].stops().join( ", " );
		if ( stopListShort.length() > 30 ) {
			stopListShort = stopListShort.left( 30 ).trimmed() + "...";
		}

		// Use a shortened stop name list as display text
		// and the complete version as tooltip
		QAction *action = m_settings.departureArrivalListType == DepartureList
				? new QAction( i18nc("@action", "Show Departures For '%1'", stopListShort), parent )
				: new QAction( i18nc("@action", "Show Arrivals For '%1'", stopListShort), parent );
		action->setToolTip( stopList );
		action->setData( i );
		if ( destroyOverlayOnTrigger ) {
			connect( action, SIGNAL(triggered()), this, SLOT(destroyOverlay()) );
		}

		action->setCheckable( true );
		action->setChecked( i == m_settings.currentStopSettingsIndex );
		switchStopAction->addAction( action );
	}

	connect( switchStopAction, SIGNAL(triggered(QAction*)),
			 this, SLOT(setCurrentStopIndex(QAction*)) );
	return switchStopAction;
}

void PublicTransport::showActionButtons()
{
	m_overlay = new OverlayWidget( m_graphicsWidget, m_mainGraphicsWidget );
	m_overlay->setGeometry( m_graphicsWidget->contentsRect() );

	Plasma::PushButton *btnJourney = 0;
	if ( m_currentServiceProviderFeatures.contains("JourneySearch") ) {
		btnJourney = new Plasma::PushButton( m_overlay );
		btnJourney->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
		btnJourney->setAction( action("searchJourneys") );
		connect( btnJourney, SIGNAL( clicked() ), this, SLOT( destroyOverlay() ) );
	}

	Plasma::PushButton *btnBackToDepartures = 0;
	if ( testState( ShowingJourneyList ) ) {
		btnBackToDepartures = new Plasma::PushButton( m_overlay );
		btnBackToDepartures->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
		btnBackToDepartures->setAction( updatedAction("backToDepartures") );
		connect( btnBackToDepartures, SIGNAL( clicked() ), this, SLOT( destroyOverlay() ) );
	}

	Plasma::PushButton *btnShowDepArr = 0;
	if ( m_currentServiceProviderFeatures.contains("Arrivals") ) {
		btnShowDepArr = new Plasma::PushButton( m_overlay );
		btnShowDepArr->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
		if ( m_settings.departureArrivalListType == DepartureList ) {
			btnShowDepArr->setAction( action("showArrivals") );
		} else {
			btnShowDepArr->setAction( action("showDepartures") );
		}
		connect( btnShowDepArr, SIGNAL( clicked() ), this, SLOT( destroyOverlay() ) );
	}

	// Add stop selector if multiple stops are defined
	Plasma::PushButton *btnMultipleStops = NULL;
	if ( m_settings.stopSettingsList.count() > 1 ) {
		btnMultipleStops = new Plasma::PushButton( m_overlay );
		btnMultipleStops->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
		btnMultipleStops->setIcon( KIcon("public-transport-stop") );
		btnMultipleStops->setZValue( 1000 );
		btnMultipleStops->setText( i18nc("@action:button", "Switch Current Stop") );

		QMenu *menu = new QMenu( btnMultipleStops->nativeWidget() );
		QList< QAction* > actionList =
				switchStopAction( btnMultipleStops->nativeWidget(), true )->actions();
		menu->addActions( actionList );
		btnMultipleStops->nativeWidget()->setMenu( menu );
	}

	Plasma::PushButton *btnCancel = new Plasma::PushButton( m_overlay );
	btnCancel->setText( i18nc("@action:button", "Cancel") );
	btnCancel->setIcon( KIcon("dialog-cancel") );
	btnCancel->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
	connect( btnCancel, SIGNAL(clicked()), this, SLOT(destroyOverlay()) );

	QGraphicsWidget *spacer = new QGraphicsWidget( m_overlay );
	spacer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	QGraphicsWidget *spacer2 = new QGraphicsWidget( m_overlay );
	spacer2->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

	// Create a separate layout for the cancel button to have some more space
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

	m_overlay->setOpacity( 0 );
	Plasma::Animation *fadeAnimOverlay = GlobalApplet::fadeAnimation( m_overlay, 1 );

	Plasma::Animation *fadeAnim1 = NULL, *fadeAnim2 = NULL, *fadeAnim3 = NULL,
			*fadeAnim4 = NULL, *fadeAnim5 = GlobalApplet::fadeAnimation( btnCancel, 1 );
	if ( btnJourney ) {
		btnJourney->setOpacity( 0 );
		fadeAnim1 = GlobalApplet::fadeAnimation( btnJourney, 1 );
	}
	if ( btnShowDepArr ) {
		btnShowDepArr->setOpacity( 0 );
		fadeAnim2 = GlobalApplet::fadeAnimation( btnShowDepArr, 1 );
	}
	if ( btnBackToDepartures ) {
		btnBackToDepartures->setOpacity( 0 );
		fadeAnim3 = GlobalApplet::fadeAnimation( btnBackToDepartures, 1 );
	}
	if ( btnMultipleStops ) {
		btnMultipleStops->setOpacity( 0 );
		fadeAnim4 = GlobalApplet::fadeAnimation( btnMultipleStops, 1 );
	}
	btnCancel->setOpacity( 0 );

	QSequentialAnimationGroup *seqGroup = new QSequentialAnimationGroup;
	if ( fadeAnimOverlay ) {
		seqGroup->addAnimation( fadeAnimOverlay );
	}
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
}

void PublicTransport::setCurrentStopIndex( QAction* action )
{
	bool ok;
	int stopIndex = action->data().toInt( &ok );
	if ( !ok ) {
		kDebug() << "Couldn't find stop index";
		return;
	}

	Settings settings = m_settings;
	settings.currentStopSettingsIndex = stopIndex;
	writeSettings( settings );
}

void PublicTransport::setShowDepartures()
{
	// Change departure arrival list type in a copy of the settings.
	// Then write the new settings.
	Settings settings = m_settings;
	settings.departureArrivalListType = DepartureList;
	writeSettings( settings );
}

void PublicTransport::setShowArrivals()
{
	// Change departure arrival list type in a copy of the settings.
	// Then write the new settings.
	Settings settings = m_settings;
	settings.departureArrivalListType = ArrivalList;
	writeSettings( settings );
}

void PublicTransport::switchFilterConfiguration( QAction* action )
{
	switchFilterConfiguration( KGlobal::locale()->removeAcceleratorMarker( action->text() ) );
}

void PublicTransport::switchFilterConfiguration( const QString& newFilterConfiguration )
{
	QString filterConfig = GlobalApplet::untranslateFilterKey( newFilterConfiguration );
	kDebug() << "Switch filter configuration to" << filterConfig
			 << m_settings.currentStopSettingsIndex;
	if ( !m_settings.filterSettings.contains( filterConfig ) ) {
		kDebug() << "Filter" << filterConfig << "not found!";
		return;
	}

	// Change filter configuration of the current stop in a copy of the settings.
	// Then write the new settings.
	Settings settings = m_settings;
	settings.stopSettingsList[ settings.currentStopSettingsIndex ].set(
			FilterConfigurationSetting, filterConfig );
	writeSettings( settings );
}

void PublicTransport::setFiltersEnabled( bool enable )
{
	// Change filters enabled in a copy of the settings.
	// Then write the new settings.
	Settings settings = m_settings;
	settings.filtersEnabled = enable;
	writeSettings( settings );
}

void PublicTransport::iconCloseClicked()
{
	// Go back to the departure/arrival board
	addState( ShowingDepartureArrivalList );
}

void PublicTransport::journeySearchInputFinished()
{
	clearJourneys();
	Plasma::LineEdit *journeySearch =
			m_titleWidget->castedWidget<Plasma::LineEdit>( TitleWidget::WidgetJourneySearchLine );
	Q_ASSERT( journeySearch );

	Settings settings = m_settings;

	// Add journey search line to the list of recently used journey searches
	// and cut recent journey searches if the limit is exceeded
	if ( !settings.recentJourneySearches.contains(journeySearch->text(), Qt::CaseInsensitive) ) {
		settings.recentJourneySearches.prepend( journeySearch->text() );
	}
	while ( settings.recentJourneySearches.count() > MAX_RECENT_JOURNEY_SEARCHES ) {
		settings.recentJourneySearches.takeLast();
	}

	writeSettings( settings );

	m_journeyTitleText.clear();
	QString stop;
	QDateTime departure;
	bool stopIsTarget;
	bool timeIsDeparture;
	JourneySearchParser::parseJourneySearch( journeySearch->nativeWidget(),
	        journeySearch->text(), &stop, &departure,
	        &stopIsTarget, &timeIsDeparture );
	reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture );
	addState( ShowingJourneyList );
}

void PublicTransport::journeySearchLineChanged(const QString& stopName, const QDateTime& departure,
											   bool stopIsTarget, bool timeIsDeparture)
{
	reconnectJourneySource( stopName, departure, stopIsTarget, timeIsDeparture, true );
}

// TODO: Move to TitleWidget?
void PublicTransport::filterIconClicked()
{
	// TODO make a new function for this (updateFilterMenu())
	// because of redundancy in PublicTransport::contextualActions().
	KAction *actionFilter = NULL;
	QStringList filterConfigurationList = m_settings.filterSettings.keys();
	if ( !filterConfigurationList.isEmpty() ) {
		actionFilter = qobject_cast< KAction* >( action("filterConfiguration") );
		action( "enableFilters" )->setChecked( m_settings.filtersEnabled );
		QList< QAction* > oldActions = m_filtersGroup->actions();
		foreach( QAction *oldAction, oldActions ) {
			m_filtersGroup->removeAction( oldAction );
			delete oldAction;
		}
//
		QMenu *menu = actionFilter->menu();
		QString currentFilterConfig = m_settings.currentStopSettings().get<QString>(
				FilterConfigurationSetting );
		foreach( const QString &filterConfig, filterConfigurationList ) {
			QAction *action = new QAction(
					GlobalApplet::translateFilterKey(filterConfig), m_filtersGroup );
			action->setCheckable( true );
			menu->addAction( action );
			if ( filterConfig == currentFilterConfig ) {
				action->setChecked( true );
			}
		}
		m_filtersGroup->setEnabled( m_settings.filtersEnabled );
	}

	// Show the filters menu under the filter icon
	actionFilter->menu()->exec( QCursor::pos() );
//     view()->mapToGlobal(
// 	    view()->mapFromScene(m_filterIcon->mapToScene(0,
// 			    m_filterIcon->boundingRect().height()))) );
}

void PublicTransport::useCurrentPlasmaTheme()
{
	if ( m_settings.useDefaultFont ) {
		configChanged();
	}

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

	m_timetable->setPalette( p );

	// To set new text color of the header items
	m_model->setDepartureArrivalListType( m_settings.departureArrivalListType );
	m_timetable->updateItemLayouts();
}

QGraphicsWidget* PublicTransport::graphicsWidget()
{
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

		// Create the title widget and connect slots
		m_titleWidget = new TitleWidget( ShowDepartureArrivalListTitle, &m_settings,
										 m_mainGraphicsWidget );
		connect( m_titleWidget, SIGNAL(recentJourneyActionTriggered(TitleWidget::RecentJourneyAction)),
				 this, SLOT(recentJourneyActionTriggered(TitleWidget::RecentJourneyAction)) );
		connect( m_titleWidget, SIGNAL(journeySearchInputFinished()),
				 this, SLOT(journeySearchInputFinished()) );
// 		connect( m_titleWidget, SIGNAL(journeySearchInputEdited(QString)),
// 				 this, SLOT(journeySearchInputEdited(QString)) );
		connect( m_titleWidget, SIGNAL(iconClicked()), this, SLOT(iconClicked()) );
		connect( m_titleWidget, SIGNAL(closeIconClicked()), this, SLOT(iconCloseClicked()) );
		connect( m_titleWidget, SIGNAL(filterIconClicked()), this, SLOT(filterIconClicked()) );

		m_labelInfo = new Plasma::Label;
		m_labelInfo->setAlignment( Qt::AlignVCenter | Qt::AlignRight );
		connect( m_labelInfo, SIGNAL(linkActivated(QString)),
				 KToolInvocation::self(), SLOT(invokeBrowser(QString)) );
		QLabel *labelInfo = m_labelInfo->nativeWidget();
		labelInfo->setOpenExternalLinks( true );
		labelInfo->setWordWrap( true );
		m_labelInfo->setText( infoText() );
		
		// Create timetable item for departures/arrivals
		m_timetable = new TimetableWidget( m_mainGraphicsWidget );
		m_timetable->setModel( m_model );
		m_timetable->setSvg( &m_vehiclesSvg );
		connect( m_timetable, SIGNAL(contextMenuRequested(PublicTransportGraphicsItem*,QPointF)),
				 this, SLOT(departureContextMenuRequested(PublicTransportGraphicsItem*,QPointF)) );

		QGraphicsLinearLayout *layout = new QGraphicsLinearLayout( Qt::Vertical );
		layout->setContentsMargins( 0, 0, 0, 0 );
		layout->setSpacing( 0 );

		layout->addItem( m_titleWidget );
		layout->addItem( m_timetable );
		layout->addItem( m_labelInfo );
		layout->setAlignment( m_labelInfo, Qt::AlignRight | Qt::AlignVCenter );
		m_mainGraphicsWidget->setLayout( layout );
		setTitleType( ShowDepartureArrivalListTitle );

		registerAsDragHandle( m_mainGraphicsWidget );
		registerAsDragHandle( m_titleWidget->titleWidget() );

		// To make the link clickable (don't let plasma eat click events for dragging)
		m_labelInfo->installSceneEventFilter( this );

		useCurrentPlasmaTheme();
	}

	return m_graphicsWidget;
}

bool PublicTransport::sceneEventFilter( QGraphicsItem* watched, QEvent* event )
{
	if ( watched == m_labelInfo && event->type() == QEvent::GraphicsSceneMousePress ) {
		return true; // To make links clickable, otherwise Plasma takes all clicks to move the applet
	}

	return Plasma::Applet::sceneEventFilter( watched, event );
}

bool PublicTransport::eventFilter( QObject *watched, QEvent *event )
{
	Plasma::LineEdit *journeySearch =
			m_titleWidget->castedWidget<Plasma::LineEdit>( TitleWidget::WidgetJourneySearchLine );
	if ( watched && watched == journeySearch && m_listStopSuggestions
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
// 					possibleStopItemActivated( curIndex );
					m_listStopSuggestions->useStopSuggestion( curIndex );
					return true;
				} else {
					row = curIndex.row();
					if ( row >= 1 ) {
						m_listStopSuggestions->nativeWidget()->setCurrentIndex(
								m_listStopSuggestions->model()->index(row - 1,
								curIndex.column(), curIndex.parent()) );
						m_listStopSuggestions->useStopSuggestion(
								m_listStopSuggestions->nativeWidget()->currentIndex() );
						return true;
					} else {
						return false;
					}
				}
			} else if ( keyEvent->key() == Qt::Key_Down ) {
				if ( !curIndex.isValid() ) {
					curIndex = m_listStopSuggestions->nativeWidget()->model()->index( 0, 0 );
					m_listStopSuggestions->nativeWidget()->setCurrentIndex( curIndex );
					m_listStopSuggestions->useStopSuggestion( curIndex );
					return true;
				} else {
					row = curIndex.row();
					if ( row < m_listStopSuggestions->model()->rowCount() - 1 ) {
						m_listStopSuggestions->nativeWidget()->setCurrentIndex(
								m_listStopSuggestions->model()->index(row + 1,
								curIndex.column(), curIndex.parent()) );
						m_listStopSuggestions->useStopSuggestion(
								m_listStopSuggestions->nativeWidget()->currentIndex() );
						return true;
					} else {
						return false;
					}
				}
			}
			break;

		default:
			break;
		}
	}

	return Plasma::PopupApplet::eventFilter( watched, event );
}

void PublicTransport::createConfigurationInterface( KConfigDialog* parent )
{
	SettingsUiManager *settingsUiManager = new SettingsUiManager(
			m_settings, dataEngine( "publictransport" ),
			dataEngine( "openstreetmap" ), dataEngine( "favicons" ),
			dataEngine( "geolocation" ), parent );
	connect( settingsUiManager, SIGNAL(settingsAccepted(Settings)),
			 this, SLOT(writeSettings(Settings)) );
	connect( m_model, SIGNAL(updateAlarms(AlarmSettingsList,QList<int>)),
			 settingsUiManager, SLOT(removeAlarms(AlarmSettingsList,QList<int>)) );
}

void PublicTransport::writeSettings( const Settings& settings )
{
	SettingsIO::ChangedFlags changed =
			SettingsIO::writeSettings( settings, m_settings, config(), globalConfig() );

	if ( changed.testFlag( SettingsIO::IsChanged ) ) {
		m_settings = settings;
		m_currentServiceProviderFeatures = currentServiceProviderData()["features"].toStringList();
		emit configNeedsSaving();
		emit settingsChanged();

		if ( changed.testFlag(SettingsIO::ChangedServiceProvider) ) {
			serviceProviderSettingsChanged();
		}
		if ( changed.testFlag(SettingsIO::ChangedDepartureArrivalListType) ) {
			m_model->setDepartureArrivalListType( m_settings.departureArrivalListType );
			m_timetable->updateItemLayouts();
		}

		// If stop settings have changed the whole model gets cleared and refilled.
		// Therefore the other change flags can be in 'else' parts
		if ( changed.testFlag(SettingsIO::ChangedStopSettings) ||
			 changed.testFlag(SettingsIO::ChangedCurrentStop) ||
			 changed.testFlag(SettingsIO::ChangedServiceProvider) )
		{
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

		if ( changed.testFlag(SettingsIO::ChangedCurrentStop) ||
					changed.testFlag(SettingsIO::ChangedStopSettings) ||
					changed.testFlag(SettingsIO::ChangedFilterSettings) ) {
			m_titleWidget->updateFilterWidget();
		}
	} else {
		kDebug() << "No changes made in the settings";
	}
}

void PublicTransport::setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType )
{
	m_model->setDepartureArrivalListType( departureArrivalListType );
	m_timetable->updateItemLayouts();
}

QGraphicsWidget* PublicTransport::widgetForType( TitleType titleType, TitleType oldTitleType )
{
	// Setup new main layout
	QGraphicsLinearLayout *layoutMainNew = new QGraphicsLinearLayout( Qt::Vertical, m_mainGraphicsWidget );
	layoutMainNew->setContentsMargins( 0, 0, 0, 0 );
	layoutMainNew->setSpacing( 0 );

	// Delete widgets of the old view
	switch ( oldTitleType ) {
	case ShowDepartureArrivalListTitle:
		kDebug() << "Hide Departure List";
		break;

	case ShowSearchJourneyLineEdit:
		kDebug() << "Delete Stop Suggestion List";
		m_listStopSuggestions->deleteLater();
		m_listStopSuggestions = NULL;
		break;

	case ShowSearchJourneyLineEditDisabled:
		kDebug() << "Delete Info Label";
		m_labelJourneysNotSupported->deleteLater();
		m_labelJourneysNotSupported = NULL;
		break;

	case ShowJourneyListTitle:
		kDebug() << "Delete Journey List";
		m_journeyTimetable->deleteLater();
		m_journeyTimetable = NULL;
		break;

	default:
		break;
	}

	QGraphicsWidget *widget;
	m_timetable->setVisible( titleType == ShowDepartureArrivalListTitle );

	// New type
	switch ( titleType ) {
	case ShowDepartureArrivalListTitle:
		m_labelInfo->setToolTip( courtesyToolTip() );
		m_labelInfo->setText( infoText() );

		widget = m_timetable;
		break;

	case ShowSearchJourneyLineEdit: {
		Plasma::LineEdit *journeySearch =
				m_titleWidget->castedWidget<Plasma::LineEdit>( TitleWidget::WidgetJourneySearchLine );
		Q_ASSERT( journeySearch );

		m_listStopSuggestions = new JourneySearchSuggestionWidget(
				&m_settings, palette() );
		m_listStopSuggestions->attachLineEdit( journeySearch );
		connect( m_listStopSuggestions, SIGNAL(journeySearchLineChanged(QString,QDateTime,bool,bool)),
				 this, SLOT(journeySearchLineChanged(QString,QDateTime,bool,bool)) );

		widget = m_listStopSuggestions;
		break;
	}
	case ShowSearchJourneyLineEditDisabled:
		m_labelJourneysNotSupported = new Plasma::Label;
		m_labelJourneysNotSupported->setAlignment( Qt::AlignCenter );
		m_labelJourneysNotSupported->setSizePolicy( QSizePolicy::Expanding,
				QSizePolicy::Expanding, QSizePolicy::Label );
		m_labelJourneysNotSupported->setText( i18nc("@info/plain",
				"Journey searches aren't supported by the currently used "
				"service provider or it's accessor.") );
		m_labelJourneysNotSupported->nativeWidget()->setWordWrap( true );

		widget = m_labelJourneysNotSupported;
		break;

	case ShowJourneyListTitle: {
		m_journeyTimetable = new JourneyTimetableWidget( this );
		m_journeyTimetable->setModel( m_modelJourneys );
		m_journeyTimetable->setFont( m_settings.sizedFont() );
		m_journeyTimetable->setSvg( &m_vehiclesSvg );

		widget = m_journeyTimetable;
		break;
	}
	}

	// Add widgets
	layoutMainNew->addItem( m_titleWidget );
	layoutMainNew->addItem( widget );
	layoutMainNew->addItem( m_labelInfo );
	layoutMainNew->setAlignment( m_labelInfo, Qt::AlignRight | Qt::AlignVCenter );
	return m_mainGraphicsWidget;
}

void PublicTransport::setTitleType( TitleType titleType )
{
	bool doFade = isVisible() && !m_appletStates.testFlag(Initializing);
	QPixmap pix;
	if ( doFade ) {
		// Draw old appearance to pixmap
		pix = QPixmap( m_mainGraphicsWidget->size().toSize() );	
		pix.fill( Qt::transparent );
		QPainter p( &pix ); 
		if ( !p.isActive() ) {
			doFade = false;
			kDebug() << "Painter isn't ready to fade the applets title (and it doesn't make sense at startup)";
		} else {
			QRect sourceRect = m_mainGraphicsWidget->mapToScene( m_mainGraphicsWidget->boundingRect() )
					.boundingRect().toRect();
			QRectF rect( QPointF( 0, 0 ), m_mainGraphicsWidget->size() );
			scene()->render( &p, rect, sourceRect );
			p.end();
		}
	}

	TitleType oldTitleType = m_titleWidget->titleType();
	m_titleWidget->setTitleType( titleType, m_appletStates );
	m_mainGraphicsWidget = widgetForType( titleType, oldTitleType );

	// Setup the new main layout
	switch ( titleType ) {
	case ShowDepartureArrivalListTitle:
		m_labelInfo->setToolTip( courtesyToolTip() );
		m_labelInfo->setText( infoText() );
		break;
	case ShowSearchJourneyLineEdit:
	case ShowSearchJourneyLineEditDisabled:

		break;
	case ShowJourneyListTitle:
		m_titleWidget->setTitle( m_journeyTitleText.isEmpty()
				? i18nc("@info", "<emphasis strong='1'>Journeys</emphasis>") : m_journeyTitleText );
		break;
	}

	if ( doFade ) {
		// Fade from old to new appearance
		oldItemAnimationFinished();
		m_oldItem = new GraphicsPixmapWidget( pix, m_graphicsWidget );
		m_oldItem->setPos( 0, 0 );
		m_oldItem->setZValue( 1000 );
		Plasma::Animation *animOut = Plasma::Animator::create( Plasma::Animator::FadeAnimation );
		animOut->setProperty( "startOpacity", 1 );
		animOut->setProperty( "targetOpacity", 0 );
		animOut->setTargetWidget( m_oldItem );
		connect( animOut, SIGNAL( finished() ), this, SLOT( oldItemAnimationFinished() ) );
		animOut->start( QAbstractAnimation::DeleteWhenStopped );
	}
}

void PublicTransport::oldItemAnimationFinished()
{
	if ( m_oldItem && m_oldItem->scene() ) {
		m_oldItem->scene()->removeItem( m_oldItem );
	}
	delete m_oldItem;
	m_oldItem = NULL;
}

void PublicTransport::recentJourneyActionTriggered( TitleWidget::RecentJourneyAction recentJourneyAction )
{
	if ( recentJourneyAction == TitleWidget::ActionClearRecentJourneys ) {
		// Clear recent journey list
		Settings settings = m_settings;
		settings.recentJourneySearches.clear();
		writeSettings( settings );
	}
}

void PublicTransport::unsetStates( QList< AppletState > states )
{
	foreach( const AppletState &appletState, states ) {
		m_appletStates &= ~appletState;
	}
}

void PublicTransport::addState( AppletState state )
{
	QColor textColor;

	switch ( state ) {
	case ShowingDepartureArrivalList:
		setTitleType( ShowDepartureArrivalListTitle );
// 		m_timetable->setIconSize( 32 * m_settings.sizeFactor );
		m_timetable->setZoomFactor( m_settings.sizeFactor );
		m_timetable->setTargetHidden( m_settings.hideColumnTarget );
		m_timetable->update();
		geometryChanged();
		setBusy( testState(WaitingForDepartureData) && m_model->isEmpty() );
		disconnectJourneySource();

		setAssociatedApplicationUrls( KUrl::List() << m_urlDeparturesArrivals );
		unsetStates( QList<AppletState>() << ShowingJourneyList
		             << ShowingJourneySearch << ShowingJourneysNotSupported );
		break;

	case ShowingJourneyList:
		setTitleType( ShowJourneyListTitle );
		setBusy( testState( WaitingForJourneyData ) && m_modelJourneys->isEmpty() );

		setAssociatedApplicationUrls( KUrl::List() << m_urlJourneys );
		unsetStates( QList<AppletState>() << ShowingDepartureArrivalList
		             << ShowingJourneySearch << ShowingJourneysNotSupported );
		break;

	case ShowingJourneySearch:
		setTitleType( ShowSearchJourneyLineEdit );
		setBusy( false );

		unsetStates( QList<AppletState>() << ShowingDepartureArrivalList
		             << ShowingJourneyList << ShowingJourneysNotSupported );
		break;

	case ShowingJourneysNotSupported:
		setTitleType( ShowSearchJourneyLineEditDisabled );
		setBusy( false );

		unsetStates( QList<AppletState>() << ShowingDepartureArrivalList
				<< ShowingJourneyList << ShowingJourneySearch );
		break;

	case ReceivedValidDepartureData: {
		if ( m_titleWidget->titleType() == ShowDepartureArrivalListTitle ) {
			m_titleWidget->setIcon( DepartureListOkIcon );
			setBusy( false );
		}
		// TODO This is a copy of code in line ~1331
// 		TreeView *treeView = qobject_cast<TreeView*>( m_treeView->nativeWidget() );
		if ( m_settings.departureArrivalListType == ArrivalList ) {
			m_timetable->setNoItemsText( m_settings.filtersEnabled
					? i18nc("@info/plain", "No unfiltered arrivals.<nl/>You can disable filters "
							"to see all arrivals.")
					: i18nc("@info/plain", "No arrivals.") );
		} else {
			m_timetable->setNoItemsText( m_settings.filtersEnabled
					? i18nc("@info/plain", "No unfiltered departures.<nl/>You can disable filters "
							"to see all departures.")
					: i18nc("@info/plain", "No departures.") );
		}

		if ( m_currentMessage == MessageError )
			m_currentMessage = MessageNone;
		unsetStates( QList<AppletState>() << WaitingForDepartureData << ReceivedErroneousDepartureData );
		break;
	}
	case ReceivedValidJourneyData: {
		if ( m_titleWidget->titleType() == ShowJourneyListTitle ) {
			m_titleWidget->setIcon( JourneyListOkIcon );
			setBusy( false );
// 			TreeView *treeView = qobject_cast<TreeView*>( m_treeViewJourney->nativeWidget() );
			m_journeyTimetable->setNoItemsText( i18nc("@info/plain", "No journeys.") );
		}
		unsetStates( QList<AppletState>() << WaitingForJourneyData << ReceivedErroneousJourneyData );
		break;
	}
	case ReceivedErroneousDepartureData: {
		if ( m_titleWidget->titleType() == ShowDepartureArrivalListTitle ) {
			m_titleWidget->setIcon( DepartureListErrorIcon );
			setBusy( false );
		}
// 		TreeView *treeView = qobject_cast<TreeView*>( m_treeView->nativeWidget() );
		m_timetable->setNoItemsText( m_settings.departureArrivalListType == ArrivalList
				? i18nc("@info/plain", "No arrivals due to an error.")
				: i18nc("@info/plain", "No departures due to an error.") );
		unsetStates( QList<AppletState>() << WaitingForDepartureData << ReceivedValidDepartureData );
		break;
	}
	case ReceivedErroneousJourneyData: {
		if ( m_titleWidget->titleType() == ShowJourneyListTitle ) {
			m_titleWidget->setIcon( JourneyListErrorIcon );
			setBusy( false );
// 			TreeView *treeView = qobject_cast<TreeView*>( m_treeViewJourney->nativeWidget() );
			m_journeyTimetable->setNoItemsText( i18nc( "@info/plain", "No journeys due to an error." ) );
		}
		unsetStates( QList<AppletState>() << WaitingForJourneyData << ReceivedValidJourneyData );
		break;
	}
	case WaitingForDepartureData: {
		if ( m_titleWidget->titleType() == ShowDepartureArrivalListTitle ) {
			m_titleWidget->setIcon( DepartureListErrorIcon ); // TODO: Add a special icon for "waiting for data"? (waits for first data of a new data source)
			setBusy( m_model->isEmpty() );
		}
// 		TreeView *treeView = qobject_cast<TreeView*>( m_treeView->nativeWidget() );
		m_timetable->setNoItemsText( i18nc( "@info/plain", "Waiting for depatures..." ) );
		unsetStates( QList<AppletState>() << ReceivedValidDepartureData << ReceivedErroneousDepartureData );
		break;
	}
	case WaitingForJourneyData: {
		if ( m_titleWidget->titleType() == ShowJourneyListTitle ) {
			m_titleWidget->setIcon( JourneyListErrorIcon );
			setBusy( m_modelJourneys->isEmpty() );
// 			TreeView *treeView = qobject_cast<TreeView*>( m_treeViewJourney->nativeWidget() );
			m_journeyTimetable->setNoItemsText( i18nc( "@info/plain", "Waiting for journeys..." ) );
		}
		unsetStates( QList<AppletState>() << ReceivedValidJourneyData << ReceivedErroneousJourneyData );
		break;
	}
	case SettingsJustChanged:
	case ServiceProviderSettingsJustChanged:
	case Initializing:
	case NoState:
		break;
	}

	m_appletStates |= state;
}

void PublicTransport::removeState( AppletState state )
{
	if ( !m_appletStates.testFlag( state ) ) {
		return;
	}

	switch ( state ) {
	case ShowingJourneyList:
		m_titleWidget->setIcon( m_appletStates.testFlag( ReceivedValidDepartureData )
				? DepartureListOkIcon : DepartureListErrorIcon );
		m_model->setDepartureArrivalListType( m_settings.departureArrivalListType );
		m_timetable->updateItemLayouts();
		break;

	case Initializing:
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

void PublicTransport::hideHeader()
{
// 	if ( !m_treeViewJourney ) {
		kDebug() << "DEPRECATED";
// 	}
// 	QTreeView *treeView = /*m_treeViewJourney ?*/ m_treeViewJourney->nativeWidget() /*: m_treeView->nativeWidget()*/;
// 	treeView->header()->setVisible( false );
// 
// 	// Change show header setting in a copy of the settings.
// 	// Then write the new settings.
// 	Settings settings = m_settings;
// 	settings.showHeader = false;
// 	writeSettings( settings );
}

void PublicTransport::showHeader()
{
// 	if ( !m_treeViewJourney ) {
		kDebug() << "DEPRECATED";
// 	}
// 	QTreeView *treeView = /*m_treeViewJourney ? */m_treeViewJourney->nativeWidget()/* : m_treeView->nativeWidget()*/;
// 	treeView->header()->setVisible( true );
// 
// 	// Change show header setting in a copy of the settings.
// 	// Then write the new settings.
// 	Settings settings = m_settings;
// 	settings.showHeader = true;
// 	writeSettings( settings );
}

void PublicTransport::hideColumnTarget()
{
	// Change hide column target setting in a copy of the settings.
	// Then write the new settings.
	Settings settings = m_settings;
	settings.hideColumnTarget = true;
	writeSettings( settings );
}

void PublicTransport::showColumnTarget()
{
	// Change hide column target setting in a copy of the settings.
	// Then write the new settings.
	Settings settings = m_settings;
	settings.hideColumnTarget = false;
	writeSettings( settings );
}

void PublicTransport::toggleExpanded()
{
	doubleClickedItem( m_clickedItemIndex );
}

void PublicTransport::doubleClickedItem( const QModelIndex &modelIndex )
{
// 	if ( !m_treeViewJourney ) {
// 		kDebug() << "DEPRECATED";
// 	}

	if ( m_journeyTimetable ) {
// 		QModelIndex firstIndex;
// 		if ( testState(ShowingDepartureArrivalList) ) {
// 			firstIndex = m_model->index( modelIndex.row(), 0, modelIndex.parent() );
// 		} else {
// 			firstIndex = m_modelJourneys->index( modelIndex.row(), 0, modelIndex.parent() );
// 		}
// 	
// 		QTreeView* treeView =  m_treeViewJourney->nativeWidget();
// 		if ( treeView->isExpanded( firstIndex ) ) {
// 			treeView->collapse( firstIndex );
// 		} else {
// 			treeView->expand( firstIndex );
// 		}
		m_journeyTimetable->item( modelIndex.row() )->toggleExpanded();
	} else {
		m_timetable->item( modelIndex.row() )->toggleExpanded();
	}
}

QAction* PublicTransport::updatedAction( const QString& actionName )
{
	QAction *a = action( actionName );
	if ( !a ) {
		if ( actionName == "separator" ) {
			a = new QAction( this );
			a->setSeparator( true );
			return a;
		} else {
			kDebug() << "Action not found:" << actionName;
			return NULL;
		}
	}

	if ( actionName == "backToDepartures" ) {
		a->setText( m_settings.departureArrivalListType == DepartureList
		            ? i18nc( "@action", "Back to &Departure List" )
		            : i18nc( "@action", "Back to &Arrival List" ) );
	} else if ( actionName == "toggleExpanded" ) {
		if ( m_journeyTimetable 
			? m_journeyTimetable->item(m_clickedItemIndex.row())->isExpanded()
			: m_timetable->item(m_clickedItemIndex.row())->isExpanded() )
		{
			a->setText( i18nc( "@action", "Hide Additional &Information" ) );
			a->setIcon( KIcon( "arrow-up" ) );
		} else {
			a->setText( i18nc( "@action", "Show Additional &Information" ) );
			a->setIcon( KIcon( "arrow-down" ) );
		}
	} else if ( actionName == "removeAlarmForDeparture" ) {
		a->setText( m_settings.departureArrivalListType == DepartureList
		            ? i18nc( "@action", "Remove &Alarm for This Departure" )
		            : i18nc( "@action", "Remove &Alarm for This Arrival" ) );
	} else if ( actionName == "setAlarmForDeparture" ) {
		a->setText( m_settings.departureArrivalListType == DepartureList
		            ? i18nc( "@action", "Set &Alarm for This Departure" )
		            : i18nc( "@action", "Set &Alarm for This Arrival" ) );
	}

	return a;
}

void PublicTransport::showHeaderContextMenu( const QPoint& position )
{
	Q_UNUSED( position );
// 	if ( !m_treeViewJourney ) {
		kDebug() << "DEPRECATED";
// 		return;
// 	}
// 	QHeaderView *header = ( /*m_treeViewJourney ?*/ m_treeViewJourney /*: m_treeView*/ )->nativeWidget()->header();
// 	QList<QAction *> actions;
// 
// 	if ( testState( ShowingDepartureArrivalList ) ) {
// 		if ( header->logicalIndexAt( position ) == 1 ) {
// 			actions.append( action( "hideColumnTarget" ) );
// 		} else if ( header->isSectionHidden( 1 ) ) {
// 			actions.append( action( "showColumnTarget" ) );
// 		}
// 	}
// 	actions.append( action("hideHeader") );
// 
// 	if ( actions.count() > 0 && view() ) {
// 		QMenu::exec( actions, QCursor::pos() );
// 	}
}

void PublicTransport::departureContextMenuRequested( PublicTransportGraphicsItem* item, const QPointF& pos )
{
	Q_UNUSED( pos );
	
	m_clickedItemIndex = item->index();
	QList<QAction *> actions;
	QAction *infoAction = NULL;
	
	actions.append( updatedAction("toggleExpanded") );
	if ( testState(ShowingDepartureArrivalList) ) {
		DepartureItem *item = static_cast<DepartureItem*>( m_model->item(m_clickedItemIndex.row()) );
		if ( item->hasAlarm() ) {
			if ( item->alarmStates().testFlag(AlarmIsAutoGenerated) ) {
				actions.append( updatedAction("removeAlarmForDeparture") );
			} else if ( item->alarmStates().testFlag(AlarmIsRecurring) ) {
				kDebug() << "The 'Remove this Alarm' menu entry can only be "
				"used with autogenerated alarms.";
				if ( item->departureInfo()->matchedAlarms().count() == 1 ) {
					infoAction = new QAction( KIcon("task-recurring"),
												i18nc("@info/plain", "(has a recurring alarm)"), this );
				} else {
					infoAction = new QAction( i18nc("@info/plain", "(has multiple alarms)"), this );
				}
			} else {
				kDebug() << "The 'Remove this Alarm' menu entry can only be "
				"used with autogenerated alarms.";
				if ( item->departureInfo()->matchedAlarms().count() == 1 ) {
					infoAction = new QAction( KIcon("task-recurring"),
												i18nc("@info/plain", "(has a custom alarm)"), this );
				} else {
					infoAction = new QAction( i18nc("@info/plain", "(has multiple alarms)"), this );
				}
			}
			if ( infoAction ) {
				infoAction->setDisabled( true );
				actions.append( infoAction );
			}
		} else
			actions.append( updatedAction("setAlarmForDeparture") );
	}

// 	if ( !treeView->header()->isVisible() ) {
// 		actions.append( updatedAction("separator") );
// 		actions.append( action("showHeader") );
// 	} else if ( treeView->header()->isSectionHidden( ColumnTarget ) ) {
// 		if ( testState(ShowingDepartureArrivalList) ) {
// 			actions.append( updatedAction("separator") );
// 			actions.append( action("showColumnTarget") );
// 		}
// 	}

	if ( actions.count() > 0 && view() ) {
		QMenu::exec( actions, QCursor::pos() );
	}

	delete infoAction;
}

void PublicTransport::showDepartureContextMenu( const QPoint& position )
{
	Q_UNUSED( position );
// 	if ( !m_treeViewJourney ) {
		kDebug() << "DEPRECATED";
// 		return;
// 	}
// 	QTreeView* treeView =  /*m_treeViewJourney ?*/ m_treeViewJourney->nativeWidget() /*: m_treeView->nativeWidget()*/;
// 	QList<QAction *> actions;
// 	QAction *infoAction = NULL;
// 
// 	if ( (m_clickedItemIndex = treeView->indexAt(position)).isValid() ) {
// 		while ( m_clickedItemIndex.parent().isValid() ) {
// 			m_clickedItemIndex = m_clickedItemIndex.parent();
// 		}
// 
// 		actions.append( updatedAction("toggleExpanded") );
// 
// 		if ( testState(ShowingDepartureArrivalList) ) {
// 			DepartureItem *item = static_cast<DepartureItem*>( m_model->item( m_clickedItemIndex.row() ) );
// 			if ( item->hasAlarm() ) {
// 				if ( item->alarmStates().testFlag(AlarmIsAutoGenerated) ) {
// 					actions.append( updatedAction("removeAlarmForDeparture") );
// 				} else if ( item->alarmStates().testFlag(AlarmIsRecurring) ) {
// 					kDebug() << "The 'Remove this Alarm' menu entry can only be "
// 					"used with autogenerated alarms.";
// 					if ( item->departureInfo()->matchedAlarms().count() == 1 ) {
// 						infoAction = new QAction( KIcon("task-recurring"),
// 						                          i18nc("@info/plain", "(has a recurring alarm)"), this );
// 					} else {
// 						infoAction = new QAction( i18nc("@info/plain", "(has multiple alarms)"), this );
// 					}
// 				} else {
// 					kDebug() << "The 'Remove this Alarm' menu entry can only be "
// 					"used with autogenerated alarms.";
// 					if ( item->departureInfo()->matchedAlarms().count() == 1 ) {
// 						infoAction = new QAction( KIcon("task-recurring"),
// 						                          i18nc("@info/plain", "(has a custom alarm)"), this );
// 					} else {
// 						infoAction = new QAction( i18nc("@info/plain", "(has multiple alarms)"), this );
// 					}
// 				}
// 				if ( infoAction ) {
// 					infoAction->setDisabled( true );
// 					actions.append( infoAction );
// 				}
// 			} else
// 				actions.append( updatedAction("setAlarmForDeparture") );
// 		}
// 
// 		if ( !treeView->header()->isVisible() ) {
// 			actions.append( updatedAction("separator") );
// 			actions.append( action("showHeader") );
// 		} else if ( treeView->header()->isSectionHidden( ColumnTarget ) ) {
// 			if ( testState(ShowingDepartureArrivalList) ) {
// 				actions.append( updatedAction("separator") );
// 				actions.append( action("showColumnTarget") );
// 			}
// 		}
// 	} else { // No context item
// 		actions.append( updatedAction("searchJourneys") );
// 		actions.append( updatedAction("separator") );
// 
// 		if ( !treeView->header()->isVisible() ) {
// 			actions.append( action("showHeader") );
// 		}
// 	}
// 
// 	if ( actions.count() > 0 && view() ) {
// 		QMenu::exec( actions, QCursor::pos() );
// 	}
// 
// 	delete infoAction;
}

void PublicTransport::showJourneySearch()
{
	addState( m_currentServiceProviderFeatures.contains( "JourneySearch" )
			  ? ShowingJourneySearch : ShowingJourneysNotSupported );
}

void PublicTransport::goBackToDepartures()
{
	addState( ShowingDepartureArrivalList );
}

void PublicTransport::removeAlarmForDeparture( int row )
{
	DepartureItem *item = static_cast<DepartureItem*>( m_model->item( row ) );
	Q_ASSERT_X( item->alarmStates().testFlag( AlarmIsAutoGenerated ),
				"PublicTransport::removeAlarmForDeparture",
				"Only auto generated alarms can be removed" );

	// Find a matching autogenerated alarm
	int matchingAlarmSettings = -1;
	for ( int i = 0; i < m_settings.alarmSettings.count(); ++i ) {
		AlarmSettings alarmSettings = m_settings.alarmSettings[ i ];
		if ( alarmSettings.autoGenerated && alarmSettings.enabled
					&& alarmSettings.filter.match( *item->departureInfo() ) ) {
			matchingAlarmSettings = i;
			break;
		}
	}
	if ( matchingAlarmSettings == -1 ) {
		kDebug() << "Couldn't find a matching autogenerated alarm";
		return;
	}

	// Remove the found alarm
	item->removeAlarm();
	AlarmSettingsList newAlarmSettings = m_settings.alarmSettings;
	newAlarmSettings.removeAt( matchingAlarmSettings );
	removeAlarms( newAlarmSettings, QList<int>() << matchingAlarmSettings );

	if ( m_clickedItemIndex.isValid() ) {
		createPopupIcon();
	}
}

void PublicTransport::removeAlarmForDeparture()
{
	removeAlarmForDeparture( m_clickedItemIndex.row() );
}

void PublicTransport::createAlarmSettingsForDeparture( const QPersistentModelIndex &modelIndex )
{
	if ( !modelIndex.isValid() ) {
		kDebug() << "!modelIndex.isValid()";
		return;
	}

	DepartureItem *item = static_cast<DepartureItem*>( m_model->itemFromIndex( modelIndex ) );
	DepartureInfo info = *item->departureInfo();
	QString departureTime = KGlobal::locale()->formatTime( info.departure().time() );

	// Autogenerate an alarm that only matches the given departure
	AlarmSettings alarm;
	alarm.name = i18nc( "@info/plain Name of new automatically generated alarm filters. "
						"%1 is the departure time, %2 is the target.",
						"At %1 to %2", departureTime, info.target() );
	alarm.autoGenerated = true;
	alarm.affectedStops << m_settings.currentStopSettingsIndex;
	alarm.filter.append( Constraint( FilterByDeparture, FilterEquals, info.departure() ) );
	alarm.filter.append( Constraint( FilterByTransportLine, FilterEquals, info.lineString() ) );
	alarm.filter.append( Constraint( FilterByVehicleType, FilterIsOneOf,
	                                 QVariantList() << info.vehicleType() ) );
	alarm.filter.append( Constraint( FilterByTarget, FilterEquals, info.target() ) );

	// Append new alarm in a copy of the settings. Then write the new settings.
	Settings settings = m_settings;
	settings.alarmSettings << alarm;
	writeSettings( settings );

	// Add the new alarm to the list of alarms that match the given departure
	int index = settings.alarmSettings.count() - 1;
	info.matchedAlarms() << index;
	item->setDepartureInfo( info );

	createPopupIcon();
}

void PublicTransport::setAlarmForDeparture()
{
	createAlarmSettingsForDeparture( m_model->index(m_clickedItemIndex.row(), 2) );
	
	// Animate popup icon to show the alarm departure (at index -1)
	if ( m_popupIconTransitionAnimation ) {
		m_popupIconTransitionAnimation->stop();
		m_popupIconTransitionAnimation->setStartValue( m_popupIconDepartureIndex );
	} else {
		m_popupIconTransitionAnimation = new QPropertyAnimation( this, "PopupIconDepartureIndex", this );
		m_popupIconTransitionAnimation->setStartValue( m_startPopupIconDepartureIndex );
		connect( m_popupIconTransitionAnimation, SIGNAL(finished()), 
				 this, SLOT(popupIconTransitionAnimationFinished()) );
	}
	
	m_popupIconTransitionAnimation->setEndValue( -1 );
	m_popupIconTransitionAnimation->start();
}

void PublicTransport::alarmFired( DepartureItem* item )
{
	const DepartureInfo *departureInfo = item->departureInfo();
	QString sLine = departureInfo->lineString();
	QString sTarget = departureInfo->target();
	QDateTime predictedDeparture = departureInfo->predictedDeparture();
	int minsToDeparture = qCeil( (float)QDateTime::currentDateTime().secsTo(predictedDeparture) / 60.0f );

	QString message;
	if ( minsToDeparture > 0 ) {
		// Departure is in the future
		if ( departureInfo->vehicleType() == Unknown ) {
			// Vehicle type is unknown
			message = i18ncp( "@info/plain", "Line %2 to '%3' departs in %1 minute at %4",
							  "Line %2 to '%3' departs in %1 minutes at %4",
							  minsToDeparture, sLine, sTarget, predictedDeparture.toString( "hh:mm" ) );
		} else {
			// Vehicle type is known
			message = i18ncp( "@info/plain %2: Line string (e.g. 'U3'), %4: Vehicle "
							  "type name (e.g. tram, subway)",
							  "The %4 %2 to '%3' departs in %1 minute at %5",
							  "The %4 %2 to '%3' departs in %1 minutes at %5",
							  minsToDeparture, sLine, sTarget,
							  GlobalApplet::vehicleTypeToString(departureInfo->vehicleType()),
							  predictedDeparture.toString("hh:mm") );
		}
	} else if ( minsToDeparture < 0 ) {
		// Has already departed
		if ( departureInfo->vehicleType() == Unknown ) {
			// Vehicle type is unknown
			message = i18ncp( "@info/plain", "Line %2 to '%3' has departed %1 minute ago at %4",
							  "Line %2 to '%3' has departed %1 minutes ago at %4",
							  -minsToDeparture, sLine, sTarget,
							  predictedDeparture.toString("hh:mm") );
		} else {
			// Vehicle type is known
			message = i18ncp( "@info/plain %2: Line string (e.g. 'U3'), %4: Vehicle "
							  "type name (e.g. tram, subway)",
							  "The %4 %2 to '%3' has departed %1 minute ago at %5",
							  "The %4 %2 to %3 has departed %1 minutes ago at %5",
							  -minsToDeparture, sLine, sTarget,
							  GlobalApplet::vehicleTypeToString(departureInfo->vehicleType()),
							  predictedDeparture.toString("hh:mm") );
		}
	} else {
		// Departs now
		if ( departureInfo->vehicleType() == Unknown ) {
			// Vehicle type is unknown
			message = i18nc( "@info/plain", "Line %1 to '%2' departs now at %3",
							 sLine, sTarget, predictedDeparture.toString( "hh:mm" ) );
		} else {
			// Vehicle type is known
			message = i18nc( "@info/plain %1: Line string (e.g. 'U3'), %3: Vehicle "
							 "type name (e.g. tram, subway)",
							 "The %3 %1 to '%2' departs now at %4", sLine, sTarget,
							 GlobalApplet::vehicleTypeToString(departureInfo->vehicleType()),
							 predictedDeparture.toString("hh:mm") );
		}
	}

	KNotification::event( KNotification::Warning, message,
						  KIcon("public-transport-stop").pixmap(16), 0L,
						  KNotification::Persistent );
}

void PublicTransport::removeAlarms( const AlarmSettingsList &newAlarmSettings,
									const QList<int> &/*removedAlarms*/ )
{
	// Change alarm settings in a copy of the settings. Then write the new settings.
	Settings settings = m_settings;
	settings.alarmSettings = newAlarmSettings;
	writeSettings( settings );
}

QString PublicTransport::infoText()
{
	QVariantHash data = currentServiceProviderData();
	QString shortUrl = data[ "shortUrl" ].toString();
	QString url = data[ "url" ].toString();
	QString sLastUpdate = m_lastSourceUpdate.toString( "hh:mm" );
	if ( sLastUpdate.isEmpty() ) {
		sLastUpdate = i18nc( "@info/plain This is used as 'last data update' text when there "
							 "hasn't been any updates yet.", "none" );
	}

	// HACK: This breaks the text at one position if needed
	// Plasma::Label doesn't work well will HTML formatted text and word wrap:
	// It sets the height as if the label shows the HTML source.
	QString textNoHtml1 = QString( "%1: %2" ).arg( i18nc( "@info/plain", "last update" ), sLastUpdate );
	QString textNoHtml2 = QString( "%1: %2" ).arg( i18nc( "@info/plain", "data by" ), shortUrl );
	QFontMetrics fm( m_labelInfo->font() );
	int width1 = fm.width( textNoHtml1 );
	int width2 = fm.width( textNoHtml2 );
	int width = width1 + fm.width( ", " ) + width2;
	if ( width > m_graphicsWidget->size().width() ) {
		m_graphicsWidget->setMinimumWidth( qMax( 150, qMax( width1, width2 ) ) );
		return QString( "<nobr>%1: %2<br>%3: <a href='%4'>%5</a><nobr>" )
		       .arg( i18nc( "@info/plain", "last update" ), sLastUpdate,
		             i18nc( "@info/plain", "data by" ), url, shortUrl );
	} else {
		return QString( "<nobr>%1: %2, %3: <a href='%4'>%5</a><nobr>" )
		       .arg( i18nc( "@info/plain", "last update" ), sLastUpdate,
		             i18nc( "@info/plain", "data by" ), url, shortUrl );
	}
}

QString PublicTransport::courtesyToolTip() const
{
	QVariantHash data = currentServiceProviderData();
	QString credit = data["credit"].toString();
	if ( credit.isEmpty() ) {
		return QString();
	} else {
		return i18nc( "@info/plain", "By courtesy of %1 (%2)", credit, data["url"].toString() );
	}
}

void PublicTransport::setTextColorOfHtmlItem( QStandardItem *item, const QColor &textColor )
{
	item->setText( item->text().prepend( "<span style='color:rgba(%1,%2,%3,%4);'>" )
			.arg( textColor.red() ).arg( textColor.green() ).arg( textColor.blue() )
			.arg( textColor.alpha() ).append( "</span>" ) );
}

void PublicTransport::fillModelJourney( const QList< JourneyInfo > &journeys )
{
	foreach( const JourneyInfo &journeyInfo, journeys ) {
		int row = m_modelJourneys->indexFromInfo( journeyInfo ).row();
		if ( row == -1 ) {
			m_modelJourneys->addItem( journeyInfo );
		} else {
			JourneyItem *item = static_cast<JourneyItem*>( m_modelJourneys->itemFromInfo( journeyInfo ) );
			m_modelJourneys->updateItem( item, journeyInfo );
		}
	}
}

void PublicTransport::fillModel( const QList<DepartureInfo> &departures )
{
	bool modelFilled = m_model->rowCount() >= m_settings.maximalNumberOfDepartures;
	foreach( const DepartureInfo &departureInfo, departures ) {
		QModelIndex index = m_model->indexFromInfo( departureInfo );
		if ( !index.isValid() ) {
			// Departure wasn't in the model
			if ( !modelFilled && !departureInfo.isFilteredOut() ) {
				m_model->addItem( departureInfo );
				modelFilled = m_model->rowCount() >= m_settings.maximalNumberOfDepartures;
			}
		} else if ( departureInfo.isFilteredOut() ) {
			// Departure has been marked as "filtered out" in the DepartureProcessor
			m_model->removeItem( m_model->itemFromInfo(departureInfo) );
		} else {
			// Departure isn't filtered out
			DepartureItem *item = dynamic_cast<DepartureItem*>( m_model->itemFromIndex(index) );
			m_model->updateItem( item, departureInfo );
		}
		
		// Update groups
		createDepartureGroups();
		createPopupIcon();
		createTooltip();
	}
}

void GraphicsPixmapWidget::paint( QPainter* painter,
                                  const QStyleOptionGraphicsItem* option, QWidget* )
{
	if ( !option->rect.isValid() ) {
		return;
	}
	painter->drawPixmap( option->rect, m_pixmap );
}

#include "publictransport.moc"
