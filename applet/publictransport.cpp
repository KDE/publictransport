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

// Qt includes
#include <QPainter>
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
#include <qmath.h>
#if QT_VERSION >= 0x040600
    #include <QGraphicsEffect>
    #include <QPropertyAnimation>
    #include <QSequentialAnimationGroup>
    #include <QParallelAnimationGroup>
#endif

// Own includes
#include "publictransport.h"
#include "htmldelegate.h"
#include "alarmtimer.h"
#include "settings.h"
#include "publictransporttreeview.h"


#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
#include <Plasma/Animator>
#include <Plasma/Animation>

Plasma::Animation *fadeAnimation( QGraphicsWidget *w, qreal targetOpacity ) {
    if ( w->geometry().width() * w->geometry().height() > 250000 ) {
	// Don't fade big widgets for performance reasons
	w->setOpacity( targetOpacity );
	return NULL;
    }
    
    Plasma::Animation *anim = Plasma::Animator::create( Plasma::Animator::FadeAnimation );
    anim->setTargetWidget( w );
    anim->setProperty( "startOpacity", w->opacity() );
    anim->setProperty( "targetOpacity", targetOpacity );
    return anim;
}

void startFadeAnimation( QGraphicsWidget *w, qreal targetOpacity ) {
    Plasma::Animation *anim = fadeAnimation( w, targetOpacity );
    if ( anim )
	anim->start( QAbstractAnimation::DeleteWhenStopped );
}
#endif

OverlayWidget::OverlayWidget( QGraphicsWidget* parent, QGraphicsWidget* under )
	    : QGraphicsWidget( parent ), opacity( 0.4 )
#if QT_VERSION >= 0x040600
	    , m_under( 0 ), m_blur( 0 )
#endif
	    {
    resize( parent->size() );
    setZValue( 10000 );
    m_under = under;
    under->setEnabled( false );
    
#if QT_VERSION >= 0x040600
    if ( under && KGlobalSettings::graphicEffectsLevel() != KGlobalSettings::NoEffects ) {
	m_blur = new QGraphicsBlurEffect( this );
	m_blur->setBlurHints( QGraphicsBlurEffect::AnimationHint );
	QPropertyAnimation *blurAnim = new QPropertyAnimation( m_blur, "blurRadius" );
	blurAnim->setStartValue( 0 );
	blurAnim->setEndValue( 5 );
	blurAnim->setDuration( 1000 );
	under->setGraphicsEffect( m_blur );
	blurAnim->start( QAbstractAnimation::DeleteWhenStopped );
    }
#endif
}

void OverlayWidget::destroy() {
#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    Plasma::Animation *fadeAnim = fadeAnimation( this, 0 );
    
    QParallelAnimationGroup *parGroup = new QParallelAnimationGroup;
    connect( parGroup, SIGNAL(finished()), this, SLOT(overlayAnimationComplete()) );
    if ( fadeAnim )
	parGroup->addAnimation( fadeAnim );
    if ( m_blur ) {
	QPropertyAnimation *blurAnim = new QPropertyAnimation( m_blur, "blurRadius" );
	blurAnim->setStartValue( m_blur->blurRadius() );
	blurAnim->setEndValue( 0 );
	parGroup->addAnimation( blurAnim );
    }
    parGroup->start( QAbstractAnimation::DeleteWhenStopped );
    
    m_under->setEnabled( true );
#else
    overlayAnimationComplete();
#endif
}

void OverlayWidget::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
			   QWidget* widget ) {
    Q_UNUSED( option )
    Q_UNUSED( widget )

    if ( qFuzzyCompare(1, 1 + opacity) )
	return;

    QColor wash = Plasma::Theme::defaultTheme()->color( Plasma::Theme::BackgroundColor );
    wash.setAlphaF( opacity );

    Plasma::Applet *applet = qobject_cast<Plasma::Applet *>(parentWidget());
    QPainterPath backgroundShape;
    if ( !applet || applet->backgroundHints() & Plasma::Applet::StandardBackground ) {
	// FIXME: a resize here is nasty, but perhaps still better than an eventfilter just for that..
	if ( parentWidget()->contentsRect().size() != size() )
	    resize( parentWidget()->contentsRect().size() );

	backgroundShape = Plasma::PaintUtils::roundedRectangle(contentsRect(), 5);
    } else {
	backgroundShape = shape();
    }

    painter->setRenderHints( QPainter::Antialiasing );
    painter->fillPath( backgroundShape, wash );
}

void OverlayWidget::overlayAnimationComplete() {
    if ( scene() )
	scene()->removeItem( this );
    deleteLater();

    m_under->setEnabled( true );
#if QT_VERSION >= 0x040600
    m_under->setGraphicsEffect( NULL );
#endif
}


PublicTransport::PublicTransport( QObject *parent, const QVariantList &args )
	    : Plasma::PopupApplet(parent, args),
	    m_graphicsWidget(0), m_mainGraphicsWidget(0),
	    m_icon(0), m_label(0), m_labelInfo(0), m_treeView(0),
	    m_journeySearch(0), m_listStopsSuggestions(0), m_btnLastJourneySearches(0),
	    m_overlay(0), m_model(0), m_modelJourneys(0), 
	    m_departureListUpdater(0), m_journeyListUpdater(0) {
    m_currentMessage = MessageNone;
    m_departureViewColumns << LineStringColumn << TargetColumn << DepartureColumn;
    m_journeyViewColumns << VehicleTypeListColumn << JourneyInfoColumn
			 << DepartureColumn << ArrivalColumn;

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
	m_journeySearch->removeEventFilter( this );
// 	emit configNeedsSaving();

	// Store header state
	KConfigGroup cg = config();
	cg.writeEntry( "headerState", m_treeView->nativeWidget()->header()->saveState() );
	
	for ( int row = 0; row < m_model->rowCount(); ++row )
	    removeAlarmForDeparture( row );
	qDeleteAll( m_abandonedAlarmTimer );
	
	delete m_model;
	delete m_modelJourneys;
	delete m_departureListUpdater;
	delete m_journeyListUpdater;
    
	delete m_label;
	delete m_labelInfo;
	delete m_icon;
	delete m_iconClose;
	delete m_journeySearch;
	delete m_labelJourneysNotSupported;
	delete m_btnLastJourneySearches;
	delete m_graphicsWidget;
    }
}

void PublicTransport::init() {
    checkNetworkStatus();
    m_settings = SettingsIO::readSettings( config(), globalConfig() );

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
    initJourneyList();
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
    connect( actionUpdate, SIGNAL(triggered(bool)), this, SLOT(updateDataSource(bool)) );
    addAction( "updateTimetable", actionUpdate );

    QAction *showActionButtons = new QAction( /*KIcon("system-run"),*/ // TODO: better icon
					      i18n("&Quick Actions"), this );
    connect( showActionButtons, SIGNAL(triggered()), this, SLOT(showActionButtons()) );
    addAction( "showActionButtons", showActionButtons );
    
    QAction *actionSetAlarmForDeparture = new QAction(
	    Global::makeOverlayIcon(KIcon("kalarm"), "list-add"),
	    m_settings.departureArrivalListType == DepartureList
	    ? i18n("Set &Alarm for This Departure")
	    : i18n("Set &Alarm for This Arrival"), this );
    connect( actionSetAlarmForDeparture, SIGNAL(triggered(bool)),
	     this, SLOT(setAlarmForDeparture(bool)) );
    addAction( "setAlarmForDeparture", actionSetAlarmForDeparture );

    QAction *actionRemoveAlarmForDeparture = new QAction(
	    Global::makeOverlayIcon(KIcon("kalarm"), "list-remove"),
	    m_settings.departureArrivalListType == DepartureList
	    ? i18n("Remove &Alarm for This Departure")
	    : i18n("Remove &Alarm for This Arrival"), this );
    connect( actionRemoveAlarmForDeparture, SIGNAL(triggered(bool)),
	     this, SLOT(removeAlarmForDeparture(bool)) );
    addAction( "removeAlarmForDeparture", actionRemoveAlarmForDeparture );

    QAction *actionSearchJourneys = new QAction( KIcon("edit-find"),
			    i18n("Search for &Journeys..."), this );
    connect( actionSearchJourneys, SIGNAL(triggered(bool)),
	     this, SLOT(showJourneySearch(bool)) );
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
    connect( actionToggleExpanded, SIGNAL(triggered(bool)),
	     this, SLOT(toggleExpanded(bool)) );
    addAction( "toggleExpanded", actionToggleExpanded );

    QAction *actionHideHeader = new QAction( KIcon("edit-delete"),
					     i18n("&Hide header"), this );
    connect( actionHideHeader, SIGNAL(triggered(bool)), this, SLOT(hideHeader(bool)) );
    addAction( "hideHeader", actionHideHeader );

    QAction *actionShowHeader = new QAction( KIcon("list-add"),
					     i18n("Show &header"), this );
    connect( actionShowHeader, SIGNAL(triggered(bool)), this, SLOT(showHeader(bool)) );
    addAction( "showHeader", actionShowHeader );

    QAction *actionHideColumnTarget = new QAction( KIcon("view-right-close"),
					i18n("Hide &target column"), this );
    connect( actionHideColumnTarget, SIGNAL(triggered(bool)),
	     this, SLOT(hideColumnTarget(bool)) );
    addAction( "hideColumnTarget", actionHideColumnTarget );

    QAction *actionShowColumnTarget = new QAction( KIcon("view-right-new"),
					i18n("Show &target column"), this );
    connect( actionShowColumnTarget, SIGNAL(triggered(bool)),
	     this, SLOT(showColumnTarget(bool)) );
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

void PublicTransport::updateDataSource( bool ) {
    if ( testState(ShowingDepartureArrivalList) )
	reconnectSource();
    else
	reconnectJourneySource();
}

void PublicTransport::disconnectJourneySource() {
    if ( !m_currentJourneySource.isEmpty() ) {
	kDebug() << "Disconnect journey data source" << m_currentJourneySource;
	dataEngine("publictransport")->disconnectSource( m_currentJourneySource, this );

	if ( m_journeyListUpdater ) {
	    m_journeyListUpdater->stop();
	    delete m_journeyListUpdater;
	    m_journeyListUpdater = NULL;
	}
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

    m_journeyListUpdater = new QTimer( this );
    connect( m_journeyListUpdater, SIGNAL(timeout()),
		this, SLOT(updateModelJourneys()) );
    m_journeyListUpdater->start( 60000 );
}

void PublicTransport::disconnectSources() {
    if ( !m_currentSources.isEmpty() ) {
	foreach ( QString currentSource, m_currentSources ) {
	    kDebug() << "Disconnect data source" << currentSource;
	    dataEngine( "publictransport" )->disconnectSource( currentSource, this );
	}
	m_currentSources.clear();

	if ( m_departureListUpdater ) {
	    m_departureListUpdater->stop();
	    delete m_departureListUpdater;
	    m_departureListUpdater = NULL;
	}
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
	QString currentSource = QString("%4 %1|stop=%2|maxDeps=%3")
		.arg( m_settings.currentStopSettings().serviceProviderID )
		.arg( stopValue ).arg( m_settings.maximalNumberOfDepartures )
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

	    // TODO: Create and start timer when the data source got some results
	    m_departureListUpdater = new QTimer( this );
	    connect( m_departureListUpdater, SIGNAL(timeout()),
		     this, SLOT(updateModel()) );
	    m_departureListUpdater->start( 30000 ); // 30 secs because it isn't aligned to minute
	}
    }

    addState( WaitingForDepartureData );
}

void PublicTransport::processJourneyList( const QString &sourceName,
					  const Plasma::DataEngine::Data& data ) {
    Q_UNUSED( sourceName );
    
    // Remove old journey list
    m_journeyInfos.clear();
    
    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    QUrl url = data["requestUrl"].toUrl();
    kDebug() << "APP-URL" << url;
    setAssociatedApplicationUrls( KUrl::List() << url );
    #endif
    
    int count = data["count"].toInt();
    for( int i = 0; i < count; ++i ) {
	QVariant journeyData = data.value( QString("%1").arg(i) );
	if ( !journeyData.isValid()
		|| m_journeyInfos.count() >= m_settings.maximalNumberOfDepartures ) {
	    if ( !journeyData.isValid() )
		kDebug() << i << "Journey data is invalid";
	    break;
	}

	QHash<QString, QVariant> dataMap = journeyData.toHash();
	QList<QTime> routeTimesDeparture, routeTimesArrival;
	if ( dataMap.contains("routeTimesDeparture") ) {
	    QVariantList times = dataMap[ "routeTimesDeparture" ].toList();
	    foreach ( QVariant time, times )
		routeTimesDeparture << time.toTime();
	}
	if ( dataMap.contains("routeTimesArrival") ) {
	    QVariantList times = dataMap[ "routeTimesArrival" ].toList();
	    foreach ( QVariant time, times )
		routeTimesArrival << time.toTime();
	}
	QStringList routeStops, routeTransportLines,
		    routePlatformsDeparture, routePlatformsArrival;
	if ( dataMap.contains("routeStops") )
	    routeStops = dataMap[ "routeStops" ].toStringList();
	if ( dataMap.contains("routeTransportLines") )
	    routeTransportLines = dataMap[ "routeTransportLines" ].toStringList();
	if ( dataMap.contains("routePlatformsDeparture") )
	    routePlatformsDeparture = dataMap[ "routePlatformsDeparture" ].toStringList();
	if ( dataMap.contains("routePlatformsArrival") )
	    routePlatformsArrival = dataMap[ "routePlatformsArrival" ].toStringList();
	
	QList<int> routeTimesDepartureDelay, routeTimesArrivalDelay;
	if ( dataMap.contains("routeTimesDepartureDelay") ) {
	    QVariantList list = dataMap[ "routeTimesDepartureDelay" ].toList();
	    foreach ( QVariant var, list )
		routeTimesDepartureDelay << var.toInt();
	}
	if ( dataMap.contains("routeTimesArrivalDelay") ) {
	    QVariantList list = dataMap[ "routeTimesArrivalDelay" ].toList();
	    foreach ( QVariant var, list )
		routeTimesArrivalDelay << var.toInt();
	}
	
	JourneyInfo journeyInfo( dataMap["operator"].toString(),
				 dataMap["vehicleTypes"].toList(),
				 dataMap["departure"].toDateTime(),
				 dataMap["arrival"].toDateTime(),
				 dataMap["pricing"].toString(),
				 dataMap["startStopName"].toString(), dataMap["targetStopName"].toString(),
				 dataMap["duration"].toInt(),
				 dataMap["changes"].toInt(),
				 dataMap["journeyNews"].toString(),
				 routeStops, routeTransportLines,
				 routePlatformsDeparture, routePlatformsArrival,
				 dataMap["routeVehicleTypes"].toList(),
				 routeTimesDeparture, routeTimesArrival,
				 routeTimesDepartureDelay, routeTimesArrivalDelay );

	m_journeyInfos.append( journeyInfo );
    }

    kDebug() << m_journeyInfos.count() << "journeys received";
//     m_lastJourneySourceUpdate = data["updated"].toDateTime();
    updateModelJourneys();
}

void PublicTransport::processDepartureList( const QString &sourceName,
					    const Plasma::DataEngine::Data& data ) {
    // Remove old departure / arrival list
    QString strippedSourceName = stripDateAndTimeValues( sourceName );
    m_departureInfos[ strippedSourceName ].clear();
    
    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    QUrl url = data["requestUrl"].toUrl();
    kDebug() << "APP-URL" << url;
    m_urlDeparturesArrivals = url;
    if ( testState(ShowingDepartureArrivalList) || testState(ShowingJourneySearch)
	    || testState(ShowingJourneysNotSupported) )
	setAssociatedApplicationUrls( KUrl::List() << url );
    #endif

    int count = data["count"].toInt();
    for ( int i = 0; i < count; ++i ) {
	QVariant departureData = data.value( QString("%1").arg(i) );
	// Don't process invalid data and stop processing once the maximal
	// departure number is reached (except multiple stops are set)
	if ( !departureData.isValid() ) {
	    kDebug() << "Departure data for departure" << i << "is invalid" << data;
	    break;
	}

	QHash<QString, QVariant> dataMap = departureData.toHash();
	QList<QTime> routeTimes;
	if ( dataMap.contains("routeTimes") ) {
	    QVariantList times = dataMap[ "routeTimes" ].toList();
	    foreach ( QVariant time, times )
		routeTimes << time.toTime();
	}
	DepartureInfo departureInfo( dataMap["operator"].toString(),
				     dataMap["line"].toString(),
				     dataMap["target"].toString(),
				     dataMap["departure"].toDateTime(), static_cast<VehicleType>(dataMap["vehicleType"].toInt()),
				     dataMap["nightline"].toBool(),
				     dataMap["expressline"].toBool(),
				     dataMap["platform"].toString(),
				     dataMap["delay"].toInt(),
				     dataMap["delayReason"].toString(),
				     dataMap["journeyNews"].toString(),
				     dataMap["routeStops"].toStringList(),
				     routeTimes,
				     dataMap["routeExactStops"].toInt() );
				     
	// Only add departures / arrivals that are in the future
	if ( isTimeShown(departureInfo.predictedDeparture()) ) {
	    departureInfo.setVisible( filterOut(departureInfo) );
	    m_departureInfos[ strippedSourceName ].append( departureInfo );
	} else
	    kDebug() << "Departure is in the past" << departureInfo.predictedDeparture();
    }

    for ( int i = m_departureInfos[strippedSourceName].count() - 1; i >= 0; --i ) {
	if ( !isTimeShown(m_departureInfos[strippedSourceName][i].predictedDeparture()) )
	    m_departureInfos[ strippedSourceName ].removeAt( i );
    }

    kDebug() << m_departureInfos[ strippedSourceName ].count() << "departures / arrivals received";
    setConfigurationRequired( false );
    m_stopNameValid = true;

    QDateTime lastUpdate = data["updated"].toDateTime();
    if ( lastUpdate > m_lastSourceUpdate )
	m_lastSourceUpdate = lastUpdate;
    updateModel();
}

bool PublicTransport::isTimeShown( const QDateTime& dateTime ) const {
    StopSettings curStopSettings = m_settings.currentStopSettings();
    QDateTime firstDepartureTime = curStopSettings.firstDepartureConfigMode == AtCustomTime
	    ? QDateTime( QDate::currentDate(), curStopSettings.timeOfFirstDepartureCustom )
	    : QDateTime::currentDateTime();
    int secsToDepartureTime = firstDepartureTime.secsTo( dateTime );
    if ( curStopSettings.firstDepartureConfigMode == RelativeToCurrentTime )
	secsToDepartureTime -= curStopSettings.timeOffsetOfFirstDeparture * 60;
    if ( -secsToDepartureTime / 3600 >= 23 )
	secsToDepartureTime += 24 * 3600;
    return secsToDepartureTime > -60;
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
		if ( departureInfo.isVisible() )
		    ret << departureInfo;
	    }
	}
    }
    
    qSort( ret.begin(), ret.end() );
    return ret.mid( 0, m_settings.maximalNumberOfDepartures );
}

void PublicTransport::clearDepartures() {
    m_departureInfos.clear(); // Clear data from data engine
    m_model->removeRows( 0, m_model->rowCount() ); // Clear data to be displayed
}

void PublicTransport::clearJourneys() {
    m_journeyInfos.clear(); // Clear data from data engine
    m_modelJourneys->removeRows( 0, m_modelJourneys->rowCount() ); // Clear data to be displayed
}

void PublicTransport::processData( const QString &sourceName,
				   const Plasma::DataEngine::Data& data ) {
    bool departureData = data["parseMode"].toString() == "departures";
    bool journeyData = data["parseMode"].toString() == "journeys";

    // Check for errors from the data engine
    if ( data.value("error").toBool() ) {
	if ( journeyData ) {
	    addState( ReceivedErroneousJourneyData );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    QUrl url = data["requestUrl"].toUrl();
	    kDebug() << "Errorneous journey url" << url;
	    m_urlJourneys = url;
	    if ( testState(ShowingJourneyList) )
		setAssociatedApplicationUrls( KUrl::List() << url );
	    #endif
	} else if ( departureData ) {
	    m_stopNameValid = false;
	    addState( ReceivedErroneousDepartureData );
	    
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
// 		bool hasActivatedInterface = false;
// 		QStringList interfaces = dataEngine("network")->sources();
// 		foreach ( const QString &interface, interfaces ) {
// 		    QString status = dataEngine("network")->query(interface)
// 					    ["ConnectionStatus"].toString();
// 		    if ( status == "Activated" ) {
// 			hasActivatedInterface = true;
// 			break;
// 		    }
// 		}

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

	    // Update remaining times
	    updateModel();
	}
    } else if ( data.value("receivedPossibleStopList").toBool() ) { // Possible stop list received
	if ( journeyData || data.value("parseMode").toString() == "stopSuggestions" ) {
	    if ( journeyData )
		addState( ReceivedErroneousJourneyData );
	    QVariantHash stopToStopID;
	    QHash< QString, int > stopToStopWeight;
	    QStringList possibleStops, weightedStops;

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
		possibleStops << sStopName;
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
		stopNamePosition( &posStart, &len, &stopName );
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
			comp->setItems( possibleStops );
		    QString completion = comp->makeCompletion( stopName );

		    if ( completion != stopName )
			setJourneySearchStopNameCompletion( completion );
		}
	    }
	    
	    QStandardItemModel *model = qobject_cast< QStandardItemModel* >(
						    m_listStopsSuggestions );
	    if ( !model )
		model = new QStandardItemModel( m_listStopsSuggestions );
	    else
		model->clear();
	    m_listStopsSuggestions->setModel( model );
	    
	    foreach( QString s, possibleStops ) {
		QStandardItem *item = new QStandardItem( s );
		item->setIcon( KIcon("public-transport-stop") );
		model->appendRow( item );
	    }
	} else if ( departureData && m_currentMessage == MessageNone ) {
	    m_stopNameValid = false;
	    addState( ReceivedErroneousDepartureData );
	    clearDepartures();
	    setConfigurationRequired( true, i18n("The stop name is ambiguous.") );
	}
    } else { // List of departures / arrivals / journeys received
	if ( journeyData ) {
	    addState( ReceivedValidJourneyData );
	    if ( testState(ShowingJourneyList) )
		processJourneyList( sourceName, data );
	} else if ( departureData ) {
	    m_stopNameValid = true;
	    addState( ReceivedValidDepartureData );
	    processDepartureList( sourceName, data );
	}
    }

    removeState( SettingsJustChanged );
    removeState( ServiceProviderSettingsJustChanged );
}

void PublicTransport::dataUpdated( const QString& sourceName,
				   const Plasma::DataEngine::Data& data ) {
    if ( data.isEmpty() || (!m_currentSources.contains(sourceName)
		&& sourceName != m_currentJourneySource) ) {
	kDebug() << "Data discarded" << sourceName;
	return;
    }

    processData( sourceName, data );
    createTooltip();
    createPopupIcon();
}

void PublicTransport::geometryChanged() {
    setHeightOfCourtesyLabel();
}

void PublicTransport::treeViewSectionResized( int /*logicalIndex*/, int /*oldSize*/,
					      int /*newSize*/ ) {
//     geometryChanged();
}

void PublicTransport::popupEvent( bool show ) {
    destroyOverlay();
    addState( ShowingDepartureArrivalList ); // Hide opened journey views, ie. back to departure view

    Plasma::PopupApplet::popupEvent( show );
}

void PublicTransport::createPopupIcon() {
    AlarmTimer *alarmTimer = getNextAlarm();
    if ( alarmTimer && alarmTimer->timer()->isActive() ) {
	int minutesToAlarm = alarmTimer->timer()->interval() / 60000 -
				qCeil( (float)alarmTimer->startedAt().secsTo(
				       QDateTime::currentDateTime() ) / 60.0f );
	int hoursToAlarm = minutesToAlarm / 60;
	minutesToAlarm %= 60;
	QString text = i18nc( "This is displayed on the popup icon to indicate "
			      "the remaining time to the next alarm, %1=hours, "
			      "%2=minutes with padded 0", "%1:%2", hoursToAlarm,
			      QString("%1").arg(minutesToAlarm, 2, 10, QLatin1Char('0')) );

	QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DefaultFont);

	QPixmap pixmapIcon128 = KIcon( "public-transport-stop" ).pixmap(128);
	QPixmap pixmapAlarmIcon32 = KIcon( "kalarm" ).pixmap(32);
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
	QPixmap pixmapAlarmIcon13 = KIcon("kalarm").pixmap(13);
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
    DepartureInfo nextDeparture;
    Plasma::ToolTipContent data;
    data.setMainText( i18n("Public transport") );
    if ( departureInfos().isEmpty() )
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

AlarmTimer* PublicTransport::getNextAlarm() {
    for ( int row = 0; row < m_model->rowCount(); ++row ) {
	QStandardItem *itemDeparture = m_model->item( row, 2 );
	AlarmTimer *alarmTimer = static_cast< AlarmTimer* >(
		itemDeparture->data(AlarmTimerRole).value<void*>() );
	if ( alarmTimer && alarmTimer->timer()->isActive() )
	    return alarmTimer;
    }

    return NULL;
}

void PublicTransport::configChanged() {
    disconnect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
    addState( SettingsJustChanged );

    setDepartureArrivalListType( m_settings.departureArrivalListType );
    m_treeView->nativeWidget()->header()->setVisible( m_settings.showHeader );
    
    if ( testState(ShowingDepartureArrivalList) ) {
	m_treeView->nativeWidget()->setColumnHidden(
	    m_departureViewColumns.indexOf(TargetColumn), m_settings.hideColumnTarget );
    }

    QFont font = m_settings.font;
    QFont smallFont = font, boldFont = font;
    float sizeFactor = m_settings.sizeFactor;
    if ( font.pointSize() == -1 ) {
	int pixelSize = font.pixelSize() * sizeFactor;
	font.setPixelSize( pixelSize > 0 ? pixelSize : 1 );
    } else {
	int pointSize = font.pointSize() * sizeFactor;
	font.setPointSize( pointSize > 0 ? pointSize : 1 );
    }
    int smallPointSize = KGlobalSettings::smallestReadableFont().pointSize() * sizeFactor;
    smallFont.setPointSize( smallPointSize > 0 ? smallPointSize : 1 );
    boldFont.setBold( true );
    
    m_treeView->setFont( font );
    m_label->setFont( boldFont );
    m_labelInfo->setFont( smallFont );
    m_listStopsSuggestions->setFont( font );
    m_journeySearch->setFont( font );
    setHeightOfCourtesyLabel();

    int iconExtend = (testState(ShowingDepartureArrivalList) ? 16 : 32) * m_settings.sizeFactor;
    m_treeView->nativeWidget()->setIconSize( QSize(iconExtend, iconExtend) );
    
    int mainIconExtend = 32 * m_settings.sizeFactor;
    m_icon->setMinimumSize( mainIconExtend, mainIconExtend );
    m_icon->setMaximumSize( mainIconExtend, mainIconExtend );
    m_iconClose->setMinimumSize( mainIconExtend, mainIconExtend );
    m_iconClose->setMaximumSize( mainIconExtend, mainIconExtend );

    HtmlDelegate *htmlDelegate = dynamic_cast< HtmlDelegate* >( m_treeView->nativeWidget()->itemDelegate() );
    htmlDelegate->setOption( HtmlDelegate::DrawShadows, m_settings.drawShadows );

    updateModel();
    updateModelJourneys();

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
    m_model = new QStandardItemModel( 0, 3 );
    m_model->setSortRole( SortRole );

    m_modelJourneys = new QStandardItemModel( 0, 4 );
    m_modelJourneys->setSortRole( SortRole );
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
	Plasma::Animation *fadeAnimOverlay = fadeAnimation( m_overlay, 1 );

	Plasma::Animation *fadeAnim1 = NULL;
	Plasma::Animation *fadeAnim2 = NULL;
	Plasma::Animation *fadeAnim3 = NULL;
	Plasma::Animation *fadeAnim4 = NULL;
	Plasma::Animation *fadeAnim5 = fadeAnimation( btnCancel, 1 );
	if ( btnJourney ) {
	    btnJourney->setOpacity( 0 );
	    fadeAnim1 = fadeAnimation( btnJourney, 1 );
	}
	if ( btnShowDepArr ) {
	    btnShowDepArr->setOpacity( 0 );
	    fadeAnim2 = fadeAnimation( btnShowDepArr, 1 );
	}
	if ( btnBackToDepartures ) {
	    btnBackToDepartures->setOpacity( 0 );
	    fadeAnim3 = fadeAnimation( btnBackToDepartures, 1 );
	}
	if ( btnMultipleStops ) {
	    btnMultipleStops->setOpacity( 0 );
	    fadeAnim4 = fadeAnimation( btnMultipleStops, 1 );
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
    if ( SettingsIO::writeSettings(m_settings, oldSettings, config(), globalConfig()) ) {
	configNeedsSaving();
	emit settingsChanged();
    }
}

void PublicTransport::setFiltersEnabled( bool enable ) {
    Settings oldSettings = m_settings;
    m_settings.filtersEnabled = enable;
    if ( SettingsIO::writeSettings(m_settings, oldSettings, config(), globalConfig()) ) {
	configNeedsSaving();
	emit settingsChanged();
    }
}

void PublicTransport::iconCloseClicked() {
    addState( ShowingDepartureArrivalList );
}

bool PublicTransport::parseDate( const QString& sDate, QDate* date ) const {
    if ( sDate == i18n("today") ) {
	*date = QDate::currentDate();
	return true;
    } else if ( sDate == i18n("tomorrow") ) {
	*date = QDate::currentDate().addDays( 1 );
	return true;
    }
	
    bool ok;
    *date = KGlobal::locale()->readDate( sDate, &ok );
    if ( !ok ) {
	// Allow date input without year
	if ( sDate.count('-') == 1 ) { // like 12-31
	    *date = KGlobal::locale()->readDate(
		    QDate::currentDate().toString("yy") + "-" + sDate, &ok );
	} else if ( sDate.count('.') == 1 ) { // like 31.12
	    *date = KGlobal::locale()->readDate(
		    sDate + "." + QDate::currentDate().toString("yy"), &ok );
	} else if ( sDate.count('.') == 2 && sDate.endsWith('.') ) { // like 31.12.
	    *date = KGlobal::locale()->readDate(
		    sDate + QDate::currentDate().toString("yy"), &ok );
	}

	if ( !ok )
	    *date = QDate::currentDate();
    }
    return ok;
}

bool PublicTransport::parseTime( const QString& sTime, QTime *time ) const {
    if ( sTime == i18n("now") ) {
	*time = QTime::currentTime();
	return true;
    }
    
    bool ok;
    *time = KGlobal::locale()->readTime( sTime, &ok );
    if ( !ok )
	*time = QTime::currentTime();
    return ok;
}

void PublicTransport::stopNamePosition( int *posStart, int *len, QString *stop ) {
    QString stopName;
    bool stopIsTarget, timeIsDeparture;
    QDateTime departure;
    parseJourneySearch( m_journeySearch->text(), &stopName, &departure,
			&stopIsTarget, &timeIsDeparture, posStart, len, false );
    if ( stop )
	*stop = stopName;
}

bool PublicTransport::searchForJourneySearchKeywords( const QString& journeySearch,
			    const QStringList &timeKeywordsTomorrow,
			    const QStringList &departureKeywords,
			    const QStringList &arrivalKeywords,
			    QDate *date, QString *stop, bool *timeIsDeparture,
			    int *len ) const {
    if ( stop->startsWith('\"') && stop->endsWith('\"') ) {
	if ( len )
	    *len = stop->length();
	*stop = stop->mid( 1, stop->length() - 2 );
	return false;
    } else if ( stop->trimmed().isEmpty() ) {
	if ( len )
	    *len = 0;
	*stop = QString();
	return false;
    }
    
    bool found = false, continueSearch;
    do {
	continueSearch = false;
	
	// If the tomorrow keyword is found, set date to tomorrow
	QStringList wordsStop = journeySearch.split( ' ', QString::SkipEmptyParts );
	QString lastWordInStop = wordsStop.last();
	if ( !lastWordInStop.isEmpty() && timeKeywordsTomorrow.contains(lastWordInStop,
						Qt::CaseInsensitive) ) {
	    *stop = stop->left( stop->length() - lastWordInStop.length() ).trimmed();
	    *date = QDate::currentDate().addDays( 1 );

	    found = continueSearch = true;
	    lastWordInStop = wordsStop.count() >= 2 ? wordsStop[ wordsStop.count() - 2 ] : "";
	}

	// Search for departure / arrival keywords
	if ( !lastWordInStop.isEmpty() ) {
	    if ( departureKeywords.contains(lastWordInStop, Qt::CaseInsensitive) ) {
		// If a departure keyword is found, use given time as departure time
		*stop = stop->left( stop->length() - lastWordInStop.length() ).trimmed();
		*timeIsDeparture = true;
		found = continueSearch = true;
	    } else if ( arrivalKeywords.contains(lastWordInStop, Qt::CaseInsensitive) ) {
		// If an arrival keyword is found, use given time as arrival time
		*stop = stop->left( stop->length() - lastWordInStop.length() ).trimmed();
		*timeIsDeparture = false;
		found = continueSearch = true;
	    }
	}
    } while ( continueSearch );
    
    if ( len )
	*len = stop->length();
    if ( stop->startsWith('\"') && stop->endsWith('\"') )
	*stop = stop->mid( 1, stop->length() - 2 );
    return found;
}

void PublicTransport::parseDateAndTime( const QString& sDateTime, QDateTime* dateTime,
					QDate *alreadyParsedDate ) const {
    QDate date;
    QTime time;
    bool callParseDate = alreadyParsedDate->isNull();

    // Parse date and/or time from sDateTime
    QStringList timeValues = sDateTime.split( QRegExp("\\s|,"), QString::SkipEmptyParts );
    if ( timeValues.count() >= 2 ) {
	if ( callParseDate && !parseDate(timeValues[0], &date)
		    && !parseDate(timeValues[1], &date) )
	    date = QDate::currentDate();
	else
	    date = *alreadyParsedDate;

	if ( !parseTime(timeValues[1], &time) && !parseTime(timeValues[0], &time) )
	    time = QTime::currentTime();
    } else {
	if ( !parseTime(sDateTime, &time) ) {
	    time = QTime::currentTime();
	    if ( callParseDate && !parseDate(sDateTime, &date) )
		date = QDate::currentDate();
	    else
		date = *alreadyParsedDate;
	} else if ( callParseDate )
	    date = QDate::currentDate();
	else
	    date = *alreadyParsedDate;
    }

    *dateTime = QDateTime( date, time );
}

void PublicTransport::splitWordList( const QStringList& wordList, int splitWordPos,
				     QString *leftOfSplitWord, QString *rightOfSplitWord,
				     int excludeWordsFromleft ) {
    *leftOfSplitWord = ( (QStringList)wordList.mid(excludeWordsFromleft,
			splitWordPos - excludeWordsFromleft) ).join( " " );
    *rightOfSplitWord = ( (QStringList)wordList.mid(splitWordPos + 1,
			wordList.count() - splitWordPos) ).join( " " );
}

void PublicTransport::combineDoubleQuotedWords( QStringList* words ) const {
    int quotedStart = -1, quotedEnd = -1;
    for ( int i = 0; i < words->count(); ++i ) {
	if ( words->at(i).startsWith('\"') )
	    quotedStart = i;
	if ( words->at(i).endsWith('\"') ) {
	    quotedEnd = i;
	    break;
	}
    }
    if ( quotedStart != -1 ) {
	if ( quotedEnd == -1 )
	    quotedEnd = words->count() - 1;
	
	// Combine words
	QString combinedWord;
	for ( int i = quotedEnd; i >= quotedStart; --i )
	    combinedWord = words->takeAt( i ) + " " + combinedWord;
	words->insert( quotedStart, combinedWord.trimmed() );
    }
}

bool PublicTransport::parseJourneySearch( const QString& search, QString *stop,
					  QDateTime *departure,
					  bool *stopIsTarget, bool *timeIsDeparture,
					  int *posStart, int *len,
					  bool correctString ) {
    kDebug() << search;
    
    QString searchLine = search;
    *stopIsTarget = true;
    *timeIsDeparture = true;
    int removedWordsFromLeft = 0;

    // Remove double spaces without changing the cursor position or selection
    int selStart = m_journeySearch->nativeWidget()->selectionStart();
    int selLength = m_journeySearch->nativeWidget()->selectedText().length();
    int cursorPos = m_journeySearch->nativeWidget()->cursorPosition();
    cursorPos -= searchLine.left( cursorPos ).count( "  " );
    if ( selStart != -1 ) {
	selStart -= searchLine.left( selStart ).count( "  " );
	selLength -= searchLine.mid( selStart, selLength ).count( "  " );
    }
    
    searchLine = searchLine.replace( "  ", " " );
    m_journeySearch->setText( searchLine );
    m_journeySearch->nativeWidget()->setCursorPosition( cursorPos );
    if ( selStart != -1 )
	m_journeySearch->nativeWidget()->setSelection( selStart, selLength );

    // Get word list
    QStringList words = searchLine.split( ' ', QString::SkipEmptyParts );
    if ( words.isEmpty() )
	return false;

    // Combine words between double quotes to one word 
    // to allow stop names containing keywords.
    combineDoubleQuotedWords( &words );

    // Check if the cursor is inside the (first) two double quotes,
    // to disable autocompletion
    int posQuotes1 = searchLine.indexOf( '\"' );
    int posQuotes2 = searchLine.indexOf( '\"', posQuotes1 + 1 );
    if ( posQuotes2 == -1 )
	posQuotes2 = searchLine.length();
    bool isInsideQuotedString = posQuotes1 == -1
	    ? false : cursorPos > posQuotes1 && cursorPos <= posQuotes2;

    // Localized keywords
    QStringList toKeywords = i18nc("A comma seperated list of keywords for the "
	    "journey search, indicating that a journey TO the given stop should "
	    "be searched. This keyword needs to be placed at the beginning of "
	    "the field.", "to").split(',', QString::SkipEmptyParts);
    QStringList fromKeywords = i18nc("A comma seperated list of keywords for the "
	    "journey search, indicating that a journey FROM the given stop should "
	    "be searched. This keyword needs to be placed at the beginning of the "
	    "field.", "from").split(',', QString::SkipEmptyParts);
    QStringList departureKeywords = i18nc("A comma seperated list of keywords for "
	    "the journey search to indicate that given times are meant as "
	    "departures (default). The order is used for autocompletion.\n"
	    "Note: Keywords should be unique for each meaning.",
	    "departing,depart,departure,dep").split(',', QString::SkipEmptyParts);
    QStringList arrivalKeywords = i18nc("A comma seperated list of keywords for the "
	    "journey search to indicate that given times are meant as arrivals. "
	    "The order is used for autocompletion.\n"
	    "Note: Keywords should be unique for each meaning.",
	    "arriving,arrive,arrival,arr").split(',', QString::SkipEmptyParts);
    QStringList timeKeywordsAt = i18nc("A comma seperated list of keywords for "
	    "the journey search field, indicating that a date/time string follows.\n"
	    "Note: Keywords should be unique for each meaning.",
	    "at").split(',', QString::SkipEmptyParts);
    QStringList timeKeywordsIn = i18nc("A comma seperated list of keywords for "
	    "the journey search field, indicating that a relative time string "
	    "follows.\nNote: Keywords should be unique for each meaning.",
	    "in").split(',', QString::SkipEmptyParts);
    QStringList timeKeywordsTomorrow = i18nc("A comma seperated list of keywords "
	    "for the journey search field, as replacement for tomorrows date.\n"
	    "Note: Keywords should be unique for each meaning.",
	    "tomorrow").split(',', QString::SkipEmptyParts);
    
    // First search for keywords at the beginning of the string ('to' or 'from')
    QString firstWord = words.first();
    if ( toKeywords.contains(firstWord, Qt::CaseInsensitive) ) {
	searchLine = searchLine.mid( firstWord.length() + 1 );
	cursorPos -= firstWord.length() + 1;
	++removedWordsFromLeft;
    } else if ( fromKeywords.contains(firstWord, Qt::CaseInsensitive) ) {
	searchLine = searchLine.mid( firstWord.length() + 1 );
	*stopIsTarget = false; // the given stop is the origin
	cursorPos -= firstWord.length() + 1;
	++removedWordsFromLeft;
    }
    if ( posStart ) {
	QStringList removedWords = (QStringList)words.mid( 0, removedWordsFromLeft );
	if ( removedWords.isEmpty() )
	    *posStart = 0;
	else
	    *posStart = removedWords.join( " " ).length() + 1;
    }
    
    // Do corrections
    if ( correctString && !isInsideQuotedString
		&& m_journeySearch->nativeWidget()->completionMode() !=
		KGlobalSettings::CompletionNone ) {
	selStart = -1;
	selLength = 0;

	int pos = searchLine.lastIndexOf( ' ', cursorPos - 1 );
	int posEnd = searchLine.indexOf( ' ', cursorPos );
	if ( posEnd == -1 )
	    posEnd = searchLine.length();
	QString lastWordBeforeCursor;
	if ( posEnd == cursorPos && pos != -1
		    && !(lastWordBeforeCursor = searchLine.mid(
			pos, posEnd - pos).trimmed()).isEmpty() ) {
	    if ( timeKeywordsAt.contains(lastWordBeforeCursor, Qt::CaseInsensitive) ) {
		// Automatically add the current time after 'at'
		QString formattedTime = KGlobal::locale()->formatTime( QTime::currentTime() );
		searchLine.insert( posEnd, " " + formattedTime );
		selStart = posEnd + 1; // +1 for the added space
		selLength = formattedTime.length();
	    } else if ( timeKeywordsIn.contains(lastWordBeforeCursor, Qt::CaseInsensitive) ) {
		// Automatically add '5 minutes' after 'in'
		QString defaultRelTime = i18nc("The automatically added relative time "
			"string, when the journey search line ends with the keyword 'in'."
			"This should be match by the regular expression for a relative "
			"time, like '(in) 5 minutes'. "
			"That regexp and the keyword ('in') are also localizable. "
			"Don't include the 'in' here.",
			"%1 minutes", 5);
		searchLine.insert( posEnd, " " + defaultRelTime );
		selStart = posEnd + 1; // +1 for the added space
		selLength = 1; // only select the number (5)
	    } else {
		QStringList completionItems;
		completionItems << timeKeywordsAt;
		completionItems << timeKeywordsIn;
		completionItems << timeKeywordsTomorrow;
		completionItems << departureKeywords;
		completionItems << arrivalKeywords;

		KCompletion *comp = m_journeySearch->nativeWidget()->completionObject( false );
		comp->setItems( completionItems );
		comp->setIgnoreCase( true );
		QString completion = comp->makeCompletion( lastWordBeforeCursor );
		setJourneySearchWordCompletion( completion );
	    }
	}
	
	// Select an appropriate substring after inserting something
	if ( selStart != -1 ) {
	    QStringList removedWords = (QStringList)words.mid( 0, removedWordsFromLeft );
	    QString removedPart = removedWords.join( " " ).trimmed();
	    QString correctedSearch;
	    if ( removedPart.isEmpty() ) {
		correctedSearch = searchLine;
	    } else {
		correctedSearch = removedPart + " " + searchLine;
		selStart += removedPart.length() + 1;
	    }
	    m_journeySearch->setText( correctedSearch );
	    m_journeySearch->nativeWidget()->setSelection( selStart, selLength );
	}
    } // Do corrections

    // Now search for keywords inside the string from back
    QStringList parts;
    for ( int i = words.count() - 1; i >= removedWordsFromLeft; --i ) {
	QString word = words[ i ];
	if ( timeKeywordsAt.contains(word, Qt::CaseInsensitive) ) {
	    // An 'at' keyword was found at position i
	    QString sDeparture;
	    splitWordList( words, i, stop, &sDeparture, removedWordsFromLeft );

	    // Search for keywords before 'at'
	    QDate date;
	    searchForJourneySearchKeywords( *stop, timeKeywordsTomorrow,
					    departureKeywords, arrivalKeywords,
					    &date, stop, timeIsDeparture, len );
	    
	    // Parse date and/or time from the string after 'at'
	    parseDateAndTime( sDeparture, departure, &date );
	    return true;
	} else if ( timeKeywordsIn.contains(word, Qt::CaseInsensitive) ) {
	    // An 'in' keyword was found at position i
	    QString sDeparture;
	    splitWordList( words, i, stop, &sDeparture, removedWordsFromLeft );

	    QRegExp rx( i18nc("This is a regular expression used to match a string "
			    "after the 'in' keyword in the journey search line. "
			    "The english version matches strings like '5 mins.', "
			    "'1 minute', ... '\\d+' stands for at least one digit, "
			    "'\\.' is just a point, a '?' after a character means "
			    "that it's optional (eg. the 's' in 'mins?' is optional "
			    "to match singular and plural forms). Normally you will "
			    "only have to translate 'mins?' and 'minutes?'."
			    "The regexp must include one pair of matching parantheses, "
			    "that match an int (the number of minutes from now). "
			    "Note: '(?:...)' are non-matching parantheses.",
			    "(\\d+)\\s+(?:mins?\\.?|minutes?)"), Qt::CaseInsensitive );

	    // Match the regexp and extract the relative datetime value
	    int pos = rx.indexIn( sDeparture );
	    if ( pos != -1 ) {
		int minutes = rx.cap( 1 ).toInt();
		
		// Search for keywords before 'in'
		QDate date = QDate::currentDate();
		searchForJourneySearchKeywords( *stop, timeKeywordsTomorrow,
						departureKeywords, arrivalKeywords,
						&date, stop, timeIsDeparture, len );
		*departure = QDateTime( date, QTime::currentTime().addSecs(minutes * 60) );
		return true;
	    }
	}
    }
    
    *stop = searchLine;
    
    // Search for keywords at the end of the string
    QDate date = QDate::currentDate();
    searchForJourneySearchKeywords( *stop, timeKeywordsTomorrow,
				    departureKeywords, arrivalKeywords,
				    &date, stop, timeIsDeparture, len );
    *departure = QDateTime( date, QTime::currentTime() );
    return false;
}

void PublicTransport::setJourneySearchWordCompletion( const QString& match ) {
    kDebug() << "MATCH" << match;
    if ( match.isEmpty() )
	return;

    int posStart = m_journeySearch->text().lastIndexOf( ' ',
	    m_journeySearch->nativeWidget()->cursorPosition() - 1 ) + 1;
    if ( posStart == -1 )
	posStart = 0;

    int posEnd = m_journeySearch->text().indexOf( ' ',
	    m_journeySearch->nativeWidget()->cursorPosition() );
    if ( posEnd == -1 )
	posEnd = m_journeySearch->text().length();

    int len = posEnd - posStart;
    if ( len == m_journeySearch->text().length() ) {
	kDebug() << "I'm not going to replace the whole word.";
	return; // Don't replace the whole word
    }
    kDebug() << "Current Word" << m_journeySearch->text().mid( posStart, len ) << posStart << len << m_journeySearch->nativeWidget()->cursorPosition();
    
    m_journeySearch->nativeWidget()->setText( m_journeySearch->text().replace(posStart, len, match) );
    m_journeySearch->nativeWidget()->setSelection( posStart + len, match.length() - len );
}

void PublicTransport::setJourneySearchStopNameCompletion( const QString &match ) {
    kDebug() << "MATCH" << match;
    if ( match.isEmpty() )
	return;
    
    int stopNamePosStart, stopNameLen;
    stopNamePosition( &stopNamePosStart, &stopNameLen );
    kDebug() << "STOPNAME =" << m_journeySearch->text().mid( stopNamePosStart, stopNameLen );
    
    int selStart = m_journeySearch->nativeWidget()->selectionStart();
    if ( selStart == -1 )
	selStart = m_journeySearch->nativeWidget()->cursorPosition();
    bool stopNameChanged = selStart > stopNamePosStart
	    && selStart + m_journeySearch->nativeWidget()->selectedText().length()
	    <= stopNamePosStart + stopNameLen;
    int posStart, len;
    if ( stopNameChanged ) {
	posStart = stopNamePosStart;
	len = stopNameLen;
	
	m_journeySearch->setText( m_journeySearch->text().replace(posStart, len, match) );
	m_journeySearch->nativeWidget()->setSelection( posStart + len, match.length() - len );
    }
}

void PublicTransport::journeySearchInputFinished() {
    clearJourneys();
    addState( ShowingJourneyList );

    if ( !m_settings.recentJourneySearches.contains(m_journeySearch->text(), Qt::CaseInsensitive) )
	m_settings.recentJourneySearches.prepend( m_journeySearch->text() );
    
    while ( m_settings.recentJourneySearches.count() > MAX_RECENT_JOURNEY_SEARCHES )
	m_settings.recentJourneySearches.takeLast();
    
    SettingsIO::writeNoGuiSettings( m_settings, config(), globalConfig() );
    
    QString stop;
    QDateTime departure;
    bool stopIsTarget;
    bool timeIsDeparture;
    parseJourneySearch( m_journeySearch->text(), &stop, &departure,
			&stopIsTarget, &timeIsDeparture );
    reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture );
}

void PublicTransport::journeySearchInputEdited( const QString &newText ) {
    QString stop;
    QDateTime departure;
    bool stopIsTarget, timeIsDeparture;
    
    // Only correct the input string if letters were added (eg. not after pressing backspace).
    m_lettersAddedToJourneySearchLine = newText.length() > m_journeySearchLastTextLength;
    
    parseJourneySearch( newText, &stop, &departure,
			&stopIsTarget, &timeIsDeparture, 0, 0, m_lettersAddedToJourneySearchLine );
    m_journeySearchLastTextLength = m_journeySearch->text().length()
	    - m_journeySearch->nativeWidget()->selectedText().length();
	    
    reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture, true );
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
	    m_journeySearch->setVisible( true );
	    layoutTop->addItem( m_icon, 0, 0 );
	    layoutTop->addItem( m_journeySearch, 0, 1 );
	    layoutTop->addItem( m_btnLastJourneySearches, 0, 2 );
	    break;

	case ShowJourneyListTitle:
	    m_icon->setVisible( true );
	    m_label->setVisible( true );
	    m_iconClose->setVisible( true );
	    layoutTop->addItem( m_icon, 0, 0 );
	    layoutTop->addItem( m_label, 0, 1 );
	    layoutTop->addItem( m_iconClose, 0, 2 );
	    break;
    }
    return layoutTop;
}

void PublicTransport::possibleStopClicked( const QModelIndex &modelIndex ) {
    // Insert the clicked stop into the journey search line,
    // don't override keywords and other infos
    int posStart, len;
    stopNamePosition( &posStart, &len );

    QString quotedStop = "\"" + modelIndex.data().toString() + "\"";
    if ( posStart == -1 ) {
	m_journeySearch->nativeWidget()->setText( quotedStop );
    } else {
	m_journeySearch->nativeWidget()->setText( m_journeySearch->text().replace(
		posStart, len, quotedStop) );
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
//     m_colorSubItemLabels = textColor;
//     m_colorSubItemLabels.setAlpha( 180 );

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
	m_iconClose = new Plasma::IconWidget;
	m_iconClose->setIcon("window-close");
	m_iconClose->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
	m_iconClose->setMinimumSize( iconExtend, iconExtend );
	m_iconClose->setMaximumSize( iconExtend, iconExtend );
	connect( m_iconClose, SIGNAL(clicked()), this, SLOT(iconCloseClicked()) );

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

	m_btnLastJourneySearches = new Plasma::ToolButton;
	m_btnLastJourneySearches->setIcon( KIcon("document-open-recent") );
	m_btnLastJourneySearches->setToolTip( i18n("Use a recent journey search") );
	m_btnLastJourneySearches->nativeWidget()->setPopupMode( QToolButton::InstantPopup );

	m_journeySearch = new Plasma::LineEdit;
	m_journeySearch->setToolTip( i18nc("This should match the localized keywords.",
		"Type a <b>target stop</b> or <b>journey request</b>."
		"<br><br> <b>Samples:</b><br>"
		"&nbsp;&nbsp;&bull; <i>To target in 15 mins</i><br>"
		"&nbsp;&nbsp;&bull; <i>From origin arriving tomorrow at 18:00</i><br>"
		"&nbsp;&nbsp;&bull; <i>Target at 6:00 2010-03-07</i>"/*, sColor*/) );
	m_journeySearch->installEventFilter( this ); // Handle up/down keys (selecting stop suggestions)
	KLineEdit *journeySearch = m_journeySearch->nativeWidget();
	journeySearch->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	journeySearch->setClickMessage( i18n("Target stop name or journey request") );

	m_labelJourneysNotSupported = new Plasma::Label;
	m_labelJourneysNotSupported->setAlignment( Qt::AlignCenter );
	m_labelJourneysNotSupported->setSizePolicy( QSizePolicy::Expanding,
						    QSizePolicy::Expanding, QSizePolicy::Label );
	m_labelJourneysNotSupported->setText( i18n("Journey searches aren't supported "
						   "by the currently used service provider "
						   "or it's accessor.") );
	QLabel *labelJourneysNotSupported = m_labelJourneysNotSupported->nativeWidget();
	labelJourneysNotSupported->setWordWrap( true );

	m_listStopsSuggestions = new Plasma::TreeView( m_mainGraphicsWidget );
	m_listStopsSuggestions->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	m_listStopsSuggestions->nativeWidget()->setRootIsDecorated( false );
	m_listStopsSuggestions->nativeWidget()->setHeaderHidden( true );
	m_listStopsSuggestions->nativeWidget()->setAlternatingRowColors( true );
	m_listStopsSuggestions->nativeWidget()->setEditTriggers( QAbstractItemView::NoEditTriggers );
	#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    m_listStopsSuggestions->setOpacity( 0 );
	#else
	    m_listStopsSuggestions->hide();
	#endif

	m_journeySearch->nativeWidget()->setCompletionMode( KGlobalSettings::CompletionAuto );
	m_journeySearch->nativeWidget()->setCompletionModeDisabled(
		KGlobalSettings::CompletionMan );
	m_journeySearch->nativeWidget()->setCompletionModeDisabled(
		KGlobalSettings::CompletionPopup );
	m_journeySearch->nativeWidget()->setCompletionModeDisabled(
		KGlobalSettings::CompletionPopupAuto );
	m_journeySearch->nativeWidget()->setCompletionModeDisabled(
		KGlobalSettings::CompletionShell );
	KCompletion *completion = m_journeySearch->nativeWidget()->completionObject( false );
	completion->setIgnoreCase( true );
	
	connect( m_journeySearch, SIGNAL(returnPressed()),
		 this, SLOT(journeySearchInputFinished()) );
	connect( m_journeySearch, SIGNAL(textEdited(QString)),
		 this, SLOT(journeySearchInputEdited(QString)) );
	connect( m_listStopsSuggestions->nativeWidget(), SIGNAL(clicked(QModelIndex)),
		 this, SLOT(possibleStopClicked(QModelIndex)) );
	connect( m_listStopsSuggestions->nativeWidget(),
		 SIGNAL(doubleClicked(QModelIndex)),
		 this, SLOT(possibleStopDoubleClicked(QModelIndex)) );

	// Create treeview for departures / arrivals
	m_treeView = new Plasma::TreeView( m_mainGraphicsWidget );
	QTreeView *treeView = new TreeView(
		m_treeView->nativeWidget()->verticalScrollBar()->style() );
	m_treeView->setWidget( treeView );
	treeView->setAlternatingRowColors( true );
	treeView->setAllColumnsShowFocus( true );
	treeView->setRootIsDecorated( false );
	treeView->setAnimated( true );
	treeView->setSortingEnabled( true );
	treeView->setWordWrap( true );
	treeView->setExpandsOnDoubleClick( false );
	treeView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	treeView->setSelectionMode( QAbstractItemView::NoSelection );
	treeView->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
	treeView->setContextMenuPolicy( Qt::CustomContextMenu );
	
	QHeaderView *header = new HeaderView( Qt::Horizontal );
	treeView->setHeader( header );
	header->setCascadingSectionResizes( true );
	// 	header->setStretchLastSection( true );
	header->setResizeMode( QHeaderView::Interactive );
	header->setSortIndicator( 2, Qt::AscendingOrder );
	header->setContextMenuPolicy( Qt::CustomContextMenu );
	header->setMinimumSectionSize( 20 );
	header->setDefaultSectionSize( 60 );
	PublicTransportDelegate *htmlDelegate = new PublicTransportDelegate( this );
	treeView->setItemDelegate( htmlDelegate );
	connect( treeView, SIGNAL(customContextMenuRequested(const QPoint &)),
		 this, SLOT(showDepartureContextMenu(const QPoint &)) );

	if ( KGlobalSettings::singleClick() ) {
	    connect( treeView, SIGNAL(clicked(const QModelIndex &)),
		     this, SLOT(doubleClickedDepartureItem(const QModelIndex &)) );
	} else {
	    connect( treeView, SIGNAL(doubleClicked(const QModelIndex &)),
		     this, SLOT(doubleClickedDepartureItem(const QModelIndex &)) );
	} 

	if ( !m_model )
	    createModels();
	m_treeView->setModel( m_model );

	KConfigGroup cg = config();
	if ( cg.hasKey("headerState") ) {
	    // Restore header state
	    QByteArray headerState = cg.readEntry( "headerState", QByteArray() );
	    if ( !headerState.isNull() )
		header->restoreState( headerState );
	} else {
	    header->resizeSection( m_departureViewColumns.indexOf(LineStringColumn), 60 );
	}
	
	header->setMovable( true );
	connect( header, SIGNAL(sectionResized(int,int,int)),
		 this, SLOT(treeViewSectionResized(int,int,int)) );
	connect( header, SIGNAL(customContextMenuRequested(const QPoint &)),
		 this, SLOT(showHeaderContextMenu(const QPoint &)) );

	QGraphicsLinearLayout *layout = new QGraphicsLinearLayout( Qt::Vertical );
	layout->setContentsMargins( 0, 0, 0, 0 );
	layout->setSpacing( 0 );

	QGraphicsGridLayout *layoutTop = new QGraphicsGridLayout;
	layout->addItem( layoutTop );
	layout->addItem( m_treeView );
	layout->addItem( m_labelInfo );
	layout->setAlignment( m_labelInfo, Qt::AlignRight | Qt::AlignVCenter );
	m_mainGraphicsWidget->setLayout( layout );

	registerAsDragHandle( m_mainGraphicsWidget );
	registerAsDragHandle( m_label );

	// To make the link clickable (don't let plasma eat click events for dragging)
	m_labelInfo->installSceneEventFilter( this );

	useCurrentPlasmaTheme();
    }

    return m_graphicsWidget;
}

void PublicTransport::setHeightOfCourtesyLabel() {
    m_labelInfo->setText( infoText() );
}

void PublicTransport::infoLabelLinkActivated( const QString& link ) {
    KToolInvocation::invokeBrowser( link );
}

bool PublicTransport::sceneEventFilter( QGraphicsItem* watched, QEvent* event ) {
    if ( watched == m_labelInfo ) {
	if ( event->type() == QEvent::GraphicsSceneMousePress ) {
	    return true; // To make links clickable, otherwise Plasma takes all
			 // clicks to move the applet
	}
    }
    
    return Plasma::Applet::sceneEventFilter( watched, event );
}

bool PublicTransport::eventFilter( QObject *watched, QEvent *event ) {
    if ( watched && watched == m_journeySearch && m_listStopsSuggestions
		&& m_listStopsSuggestions->model()
		&& m_listStopsSuggestions->model()->rowCount() > 0 ) {
	QKeyEvent *keyEvent;
	QModelIndex curIndex;
	int row;
	switch ( event->type() ) {
	    case QEvent::KeyPress:
		keyEvent = dynamic_cast<QKeyEvent*>( event );
		curIndex = m_listStopsSuggestions->nativeWidget()->currentIndex();
		
		if ( keyEvent->key() == Qt::Key_Up ) {
		    if ( !curIndex.isValid() ) {
			curIndex = m_listStopsSuggestions->nativeWidget()->model()->index( 0, 0 );
			m_listStopsSuggestions->nativeWidget()->setCurrentIndex( curIndex );
			possibleStopClicked( curIndex );
			return true;
		    } else {
			row = curIndex.row();
			if ( row >= 1 ) {
			    m_listStopsSuggestions->nativeWidget()->setCurrentIndex(
				    m_listStopsSuggestions->model()->index(row - 1,
				    curIndex.column(), curIndex.parent()) );
			    possibleStopClicked( m_listStopsSuggestions->nativeWidget()->currentIndex() );
			    return true;
			} else
			    return false;
		    }
		} else if ( keyEvent->key() == Qt::Key_Down ) {
		    if ( !curIndex.isValid() ) {
			curIndex = m_listStopsSuggestions->nativeWidget()->model()->index( 0, 0 );
			m_listStopsSuggestions->nativeWidget()->setCurrentIndex( curIndex );
			possibleStopClicked( curIndex );
			return true;
		    } else {
			row = curIndex.row();
			if ( row < m_listStopsSuggestions->model()->rowCount() - 1 ) {
			    m_listStopsSuggestions->nativeWidget()->setCurrentIndex(
				    m_listStopsSuggestions->model()->index(row + 1,
				    curIndex.column(), curIndex.parent()) );
			    possibleStopClicked( m_listStopsSuggestions->nativeWidget()->currentIndex() );
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
// 	m_treeView->nativeWidget()->doItemsLayout();

	if ( changed.testFlag(SettingsIO::ChangedServiceProvider) )
	    serviceProviderSettingsChanged();
	if ( changed.testFlag(SettingsIO::ChangedDepartureArrivalListType) )
	    setDepartureArrivalListType( m_settings.departureArrivalListType );
	if ( changed.testFlag(SettingsIO::ChangedStopSettings) ) {
	    clearDepartures();
	    reconnectSource();
	} else if ( changed.testFlag(SettingsIO::ChangedLinesPerRow) ) {
	    // Refill model to recompute item sizehints
	    m_model->removeRows( 0, m_model->rowCount() );
	    updateModel();
	}
    } else
	kDebug() << "No changes made in the settings";
}

QString PublicTransport::nameForTimetableColumn( TimetableColumn timetableColumn,
						 DepartureArrivalListType departureArrivalListType ) {
    if ( departureArrivalListType == _UseCurrentDepartureArrivalListType )
	departureArrivalListType = m_settings.departureArrivalListType;

    switch( timetableColumn ) {
	case LineStringColumn:
	    return i18nc("A tramline or busline", "Line");
	case TargetColumn:
	    if ( !testState(ShowingDepartureArrivalList) || departureArrivalListType == DepartureList )
		return i18nc("Target of a tramline or busline", "Target");
	    else // if ( departureArrivalListType == ArrivalList )
		return i18nc("Origin of a tramline or busline", "Origin");
	case DepartureColumn:
	    if ( !testState(ShowingDepartureArrivalList) || departureArrivalListType == DepartureList )
		return i18nc("Time of departure of a tram or bus", "Departure");
	    else
		return i18nc("Time of arrival of a tram or bus", "Arrival");
	case ArrivalColumn:
	    return i18nc("Time of arrival of a tram or bus", "Arrival");
	case JourneyInfoColumn:
	    return i18nc("Information about a journey with public transport", "Information");
	case VehicleTypeListColumn:
	    return i18nc("Vehicle types used in a journey with public transport", "Vehicle types");
    }

    return QString();
}

void PublicTransport::departureArrivalListTypeChanged(
		DepartureArrivalListType departureArrivalListType ) {
    setDepartureArrivalListType( departureArrivalListType );
}

void PublicTransport::setDepartureArrivalListType(
		DepartureArrivalListType departureArrivalListType ) {
    QStringList titles;
    foreach( TimetableColumn column, m_departureViewColumns )
	titles << nameForTimetableColumn( column, departureArrivalListType );
    m_model->setHorizontalHeaderLabels( titles );
    
    for( int i = 0; i < m_departureViewColumns.count(); ++i ) {
	m_model->horizontalHeaderItem( i )->setTextAlignment(
		Qt::AlignLeft | Qt::AlignVCenter );
    }
    connect( m_treeView->nativeWidget()->header(), SIGNAL(sectionPressed(int)),
	     this, SLOT(sectionPressed(int)) );
    connect( m_treeView->nativeWidget()->header(), SIGNAL(sectionMoved(int,int,int)),
	     this, SLOT(sectionMoved(int,int,int)) );
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

void PublicTransport::sectionPressed( int logicalIndex ) {
    // Don't allow moving of visual index 0
    m_treeView->nativeWidget()->header()->setMovable(
	m_treeView->nativeWidget()->header()->visualIndex(logicalIndex) != 0 );
}

void PublicTransport::initJourneyList() {
    QStringList titles;
    foreach( TimetableColumn column, m_journeyViewColumns )
	titles << nameForTimetableColumn(column);
    m_modelJourneys->setHorizontalHeaderLabels( titles );
    
    for( int i = 0; i < m_journeyViewColumns.count(); ++i ) {
	m_modelJourneys->horizontalHeaderItem( i )->setTextAlignment(
		Qt::AlignLeft | Qt::AlignVCenter );
    }
}

void PublicTransport::setTitleType( TitleType titleType ) {
    if ( !m_graphicsWidget )//|| m_titleType == titleType )
	return;

    QGraphicsLinearLayout *layoutMain =
	    dynamic_cast<QGraphicsLinearLayout*>( m_mainGraphicsWidget->layout() );
    QGraphicsGridLayout *layoutTop =
	    dynamic_cast<QGraphicsGridLayout*>( layoutMain->itemAt(0) );
    Q_ASSERT( layoutTop );
    Q_ASSERT( layoutMain );
    
    QGraphicsWidget *w = static_cast< QGraphicsWidget* >( layoutMain->itemAt(1) );
    if ( w ) {
	#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    startFadeAnimation( w, 0 );
	#else
	    w->hide();
	#endif
    }
    
    // Hide widgets from the old title layout
    for ( int col = 0; col < layoutTop->columnCount(); ++col ) {
	for ( int row = 0; row < layoutTop->rowCount(); ++row ) {
	    QGraphicsWidget *w = static_cast< QGraphicsWidget* >( layoutTop->itemAt(row, col) );
	    if ( w ) {
		#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
		    startFadeAnimation( w, 0 );
		#else
		    w->hide();
		#endif
	    }
	}
    }
    
    // Delete old title layout
    layoutMain->removeItem( layoutTop );
    delete layoutTop;

    // Setup new main layout
    QGraphicsLinearLayout *layoutMainNew = new QGraphicsLinearLayout( Qt::Vertical );
    layoutMainNew->setContentsMargins( 0, 0, 0, 0 );
    layoutMainNew->setSpacing( 0 );
    m_mainGraphicsWidget->setLayout( layoutMainNew );
    layoutMain = NULL;
    
    // Add new title layout
    QGraphicsLayout *layoutTopNew = createLayoutTitle( titleType );
    if ( layoutTopNew ) {
	layoutMainNew->addItem( layoutTopNew );
	layoutMainNew->setAlignment( layoutTopNew, Qt::AlignTop );
    }

    // Setup the new main layout
    switch( titleType ) {
	case ShowDepartureArrivalListTitle:
	    setMainIconDisplay( testState(ReceivedValidDepartureData)
		    ? DepartureListOkIcon : DepartureListErrorIcon );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_label->setText( titleText() );
	    m_labelInfo->setToolTip( courtesyToolTip() );
	    m_labelInfo->setText( infoText() );
		    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
		startFadeAnimation( m_icon, 1 );
		startFadeAnimation( m_label, 1 );
		startFadeAnimation( m_labelInfo, 1 );
		startFadeAnimation( m_treeView, 1 );
	    #else
		m_treeView->show();
	    #endif
	    layoutMainNew->addItem( m_treeView );
	    break;

	case ShowSearchJourneyLineEdit:
	    setMainIconDisplay( AbortJourneySearchIcon );
	    m_icon->setToolTip( i18n("Abort search for journeys to or from the home stop") );

	    m_journeySearch->setEnabled( true );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
		startFadeAnimation( m_icon, 1 );
		startFadeAnimation( m_journeySearch, 1 );
		startFadeAnimation( m_listStopsSuggestions, 1 );
		startFadeAnimation( m_btnLastJourneySearches, 1 );
	    #else
		m_listStopsSuggestions->show();
		m_btnLastJourneySearches->show();
	    #endif
	    layoutMainNew->addItem( m_listStopsSuggestions );
	    break;
	    
	case ShowSearchJourneyLineEditDisabled:
	    setMainIconDisplay( AbortJourneySearchIcon );
	    m_icon->setToolTip( i18n("Abort search for journeys to or from the home stop") );

	    m_journeySearch->setEnabled( false );
	    m_btnLastJourneySearches->setEnabled( false );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
		startFadeAnimation( m_icon, 1 );
		startFadeAnimation( m_btnLastJourneySearches, 1 );
		startFadeAnimation( m_labelJourneysNotSupported, 1 );
	    #else
		m_btnLastJourneySearches->show();
		m_labelJourneysNotSupported->show();
	    #endif
	    layoutMainNew->addItem( m_labelJourneysNotSupported );
	    break;

	case ShowJourneyListTitle:
	    setMainIconDisplay( testState(ReceivedValidJourneyData)
		    ? JourneyListOkIcon : JourneyListErrorIcon );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_iconClose->setToolTip( i18n("Show departures / arrivals") );
	    m_label->setText( i18n("<b>Journeys</b>") );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
		startFadeAnimation( m_label, 1 );
		startFadeAnimation( m_icon, 1 );
		startFadeAnimation( m_iconClose, 1 );
		startFadeAnimation( m_treeView, 1 );
	    #else
		m_treeView->show();
	    #endif
	    layoutMainNew->addItem( m_treeView );
	    break;
    }

    layoutMainNew->addItem( m_labelInfo );
    layoutMainNew->setAlignment( m_labelInfo, Qt::AlignRight | Qt::AlignVCenter );

    if ( titleType == ShowSearchJourneyLineEdit )
	m_journeySearch->setFocus();
    if ( titleType != ShowDepartureArrivalListTitle )
	m_journeySearch->nativeWidget()->selectAll();

    m_titleType = titleType;
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
    foreach( AppletState appletState, states ) {
	if ( m_appletStates.testFlag(appletState) )
	    m_appletStates ^= appletState;
    }
}

void PublicTransport::addState( AppletState state ) {
    QColor textColor;
    QMenu *menu;

    switch ( state ) {
	case ShowingDepartureArrivalList:
	    setTitleType( ShowDepartureArrivalListTitle );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_treeView->setModel( m_model );
	    m_treeView->nativeWidget()->setIconSize(
		    QSize(16 * m_settings.sizeFactor, 16 * m_settings.sizeFactor) );
	    m_treeView->nativeWidget()->setColumnHidden(
		m_departureViewColumns.indexOf(TargetColumn), m_settings.hideColumnTarget );
	    geometryChanged(); // TODO: Only resize columns
	    setBusy( testState(WaitingForDepartureData) );
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
	    m_treeView->setModel( m_modelJourneys );
	    m_treeView->nativeWidget()->setIconSize(
		    QSize(32 * m_settings.sizeFactor, 32 * m_settings.sizeFactor) );
	    m_treeView->nativeWidget()->setColumnHidden(
		m_departureViewColumns.indexOf(TargetColumn), false );
	    setBusy( testState(WaitingForJourneyData) );
	    
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
// 		m_currentMessage = MessageErrorResolved;
// 		showMessage( QIcon(), i18n("Network connection established."),
// 			     Plasma::ButtonOk );
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
		setBusy( true );
	    }
	    unsetStates( QList<AppletState>() << ReceivedValidDepartureData << ReceivedErroneousDepartureData );
	    break;

	case WaitingForJourneyData:
	    if ( m_titleType == ShowJourneyListTitle ) {
		setMainIconDisplay( JourneyListErrorIcon );
		setBusy( true );
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
	    setMainIconDisplay( m_appletStates.testFlag(ReceivedValidDepartureData) ? DepartureListOkIcon : DepartureListErrorIcon );
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

void PublicTransport::hideHeader( bool ) {
    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->header()->setVisible( false );
    
    Settings settings = m_settings;
    settings.showHeader = false;
    writeSettings( settings );
}

void PublicTransport::showHeader( bool ) {
    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->header()->setVisible( true );
    
    Settings settings = m_settings;
    settings.showHeader = true;
    writeSettings( settings );
}

void PublicTransport::hideColumnTarget( bool ) {
    Settings settings = m_settings;
    settings.hideColumnTarget = true;
    writeSettings( settings );
}

void PublicTransport::showColumnTarget( bool ) {
    Settings settings = m_settings;
    settings.hideColumnTarget = false;
    writeSettings( settings );
//     geometryChanged();
}

void PublicTransport::toggleExpanded( bool ) {
    doubleClickedDepartureItem( m_clickedItemIndex );
}

void PublicTransport::doubleClickedDepartureItem( const QModelIndex &modelIndex ) {
    QModelIndex firstIndex;
    if ( testState(ShowingDepartureArrivalList) )
	firstIndex = m_model->index( modelIndex.row(), 0, modelIndex.parent() );
    else
	firstIndex = m_modelJourneys->index( modelIndex.row(), 0, modelIndex.parent() );

    QTreeView* treeView = m_treeView->nativeWidget();
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

    QAbstractItemModel *model = testState( ShowingDepartureArrivalList )
	    ? m_model : m_modelJourneys;

    if ( actionName == "backToDepartures" ) {
	a->setText( m_settings.departureArrivalListType == DepartureList
	    ? i18n("Back to &Departure List")
	    : i18n("Back to &Arrival List") );
    } else if ( actionName == "toggleExpanded" ) {
	if ( m_treeView->nativeWidget()->isExpanded( model->index(m_clickedItemIndex.row(), 0) ) ) {
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
    QHeaderView *header = m_treeView->nativeWidget()->header();
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
    QTreeView* treeView = m_treeView->nativeWidget();
    QList<QAction *> actions;

    if ( (m_clickedItemIndex = treeView->indexAt(position)).isValid() ) {
	while( m_clickedItemIndex.parent().isValid() )
	    m_clickedItemIndex = m_clickedItemIndex.parent();

	actions.append( updatedAction("toggleExpanded") );

	if ( testState(ShowingDepartureArrivalList) ) {
	    if ( m_model->item( m_clickedItemIndex.row(), 2 )->icon().isNull() )
		actions.append( updatedAction("setAlarmForDeparture") );
	    else
		actions.append( updatedAction("removeAlarmForDeparture") );
	}

	if ( !treeView->header()->isVisible() ) {
	    actions.append( updatedAction("separator") );
	    actions.append( action("showHeader") );
	} else if ( treeView->header()->isSectionHidden(m_departureViewColumns.indexOf(TargetColumn)) ) {
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
}

void PublicTransport::showJourneySearch( bool ) {
    addState( m_currentServiceProviderFeatures.contains("JourneySearch")
	      ? ShowingJourneySearch : ShowingJourneysNotSupported );
}

void PublicTransport::goBackToDepartures() {
    addState( ShowingDepartureArrivalList );
}

void PublicTransport::markAlarmRow( const QPersistentModelIndex& modelIndex,
				    AlarmState alarmState ) {
    if( !modelIndex.isValid() ) {
	kDebug() << "!index.isValid(), row =" << modelIndex.row();
	return;
    }
    
    QStandardItem *itemDeparture = m_model->item( modelIndex.row(), 2 );
    if ( !itemDeparture )
	return;
    if ( alarmState == AlarmPending ) {
	itemDeparture->setIcon( KIcon("kalarm") );
	itemDeparture->setData( static_cast<int>(HtmlDelegate::Right),
				HtmlDelegate::DecorationPositionRole );
    } else if (alarmState == NoAlarm) {
	itemDeparture->setIcon( KIcon() );
    } else if (alarmState == AlarmFired) {
	KIconEffect iconEffect;
	QPixmap pixmap = iconEffect.apply(
		KIcon("kalarm").pixmap(16 * m_settings.sizeFactor),
		KIconLoader::Small, KIconLoader::DisabledState );
	KIcon disabledAlarmIcon;
	disabledAlarmIcon.addPixmap( pixmap, QIcon::Normal );
	itemDeparture->setIcon( disabledAlarmIcon );
	itemDeparture->setData( static_cast<int>(HtmlDelegate::Right),
				HtmlDelegate::DecorationPositionRole );
    }
    
    for ( int col = 0; col < m_model->columnCount(); ++col ) {
	m_model->item( modelIndex.row(), col )->setData(
		alarmState == AlarmPending, HtmlDelegate::DrawAlarmBackground );
    }

    if ( m_treeView->nativeWidget()->isExpanded(m_model->index(modelIndex.row(), 0)) )
	m_treeView->update(); // Update children with the new row color
}

void PublicTransport::removeAlarmForDeparture( int row ) {
    QStandardItem *itemDeparture = m_model->item( row, 2 );
    AlarmTimer *alarmTimer = static_cast< AlarmTimer* >(
	    itemDeparture->data(AlarmTimerRole).value<void*>() );
    if ( alarmTimer ) {
	itemDeparture->setData( 0, AlarmTimerRole );
	alarmTimer->timer()->stop();
	delete alarmTimer;
    }

    if ( m_clickedItemIndex.isValid() ) {
	markAlarmRow( m_clickedItemIndex, NoAlarm );
	createPopupIcon();
    }
}

void PublicTransport::removeAlarmForDeparture( bool ) {
    removeAlarmForDeparture( m_clickedItemIndex.row() );
}

void PublicTransport::setAlarmForDeparture( const QPersistentModelIndex &modelIndex,
					    AlarmTimer *alarmTimer ) {
    if( !modelIndex.isValid() ) {
	kDebug() << "!modelIndex.isValid(), row =" << modelIndex.row();
	return;
    }

    QStandardItem *itemDeparture = m_model->item( modelIndex.row(), 2 );
    markAlarmRow( modelIndex, AlarmPending );

    if ( !alarmTimer ) {
	QDateTime predictedDeparture = itemDeparture->data( SortRole ).toDateTime();
	int secsTo = QDateTime::currentDateTime().secsTo(
		predictedDeparture.addSecs(-m_settings.currentStopSettings().alarmTime * 60) );
	if ( secsTo < 0 )
	    secsTo = 0;
	alarmTimer = new AlarmTimer( secsTo * 1000, modelIndex );
    }
    itemDeparture->setData( qVariantFromValue((void*)alarmTimer), AlarmTimerRole );
    connect( alarmTimer, SIGNAL(timeout(const QPersistentModelIndex &)),
	     this, SLOT(showAlarmMessage(const QPersistentModelIndex &)) );

    createPopupIcon();
}

void PublicTransport::setAlarmForDeparture ( bool ) {
    setAlarmForDeparture( m_model->index(m_clickedItemIndex.row(), 2) );
}

void PublicTransport::showAlarmMessage( const QPersistentModelIndex &modelIndex ) {
    if ( !modelIndex.isValid() ) {
	kDebug() << "!modelIndex.isValid(), row =" << modelIndex.row();
	return;
    }

    QModelIndex topLevelIndex = modelIndex;
    while( topLevelIndex.parent().isValid() )
	topLevelIndex = topLevelIndex.parent();
    markAlarmRow( topLevelIndex, AlarmFired );
    
    int row = topLevelIndex.row();
    QStandardItem *itemDeparture = m_model->item( row, 2 );
    AlarmTimer *alarmTimer = static_cast< AlarmTimer* >(
	    itemDeparture->data(AlarmTimerRole).value<void*>() );    
    itemDeparture->setData( 0, AlarmTimerRole );
    delete alarmTimer;

    QString sLine = m_model->item( row, 0 )->text();
    QString sTarget = m_model->item( row, 1 )->text();
    QDateTime predictedDeparture = itemDeparture->data( SortRole ).toDateTime();
    int minsToDeparture = qCeil( (float)QDateTime::currentDateTime().secsTo(predictedDeparture) / 60.0f );
    VehicleType vehicleType = static_cast<VehicleType>(
	    m_model->item(row, 2)->data(VehicleTypeRole).toInt() );
	    
    QString message;
    if ( minsToDeparture > 0 ) {
	if ( vehicleType == Unknown )
	    message = i18np("Line %2 to '%3' departs in %1 minute at %4",
			    "Line %2 to '%3' departs in %1 minutes at %4",
			    minsToDeparture, sLine, sTarget, predictedDeparture.toString("hh:mm") );
	else
	    message = i18ncp("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)",
			     "The %4 %2 to '%3' departs in %1 minute at %5",
			     "The %4 %2 to '%3' departs in %1 minutes at %5",
			     minsToDeparture, sLine, sTarget,
			     Global::vehicleTypeToString(vehicleType),
			     predictedDeparture.toString("hh:mm"));
    }
    else if ( minsToDeparture < 0 ) {
	if ( vehicleType == Unknown )
	    message = i18np("Line %2 to '%3' has departed %1 minute ago at %4",
			    "Line %2 to '%3' has departed %1 minutes ago at %4",
			    -minsToDeparture, sLine, sTarget,
			    predictedDeparture.toString("hh:mm"));
	else
	    message = i18ncp("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)",
			     "The %4 %2 to '%3' has departed %1 minute ago at %5",
			     "The %4 %2 to %3 has departed %1 minutes ago at %5",
			     -minsToDeparture, sLine, sTarget,
			     Global::vehicleTypeToString(vehicleType),
			     predictedDeparture.toString("hh:mm"));
    }
    else {
	if ( vehicleType == Unknown )
	    message = i18n("Line %1 to '%2' departs now at %3", sLine, sTarget,
			   predictedDeparture.toString("hh:mm"));
	else
	    message = i18nc("%1: Line string (e.g. 'U3'), %3: Vehicle type name (e.g. tram, subway)",
			    "The %3 %1 to '%2' departs now at %4", sLine, sTarget,
			    Global::vehicleTypeToString(vehicleType),
			    predictedDeparture.toString("hh:mm"));
    }

    KNotification::event( KNotification::Warning, message,
			  KIcon("public-transport-stop").pixmap(16), 0L,
			  KNotification::Persistent );
}

bool PublicTransport::filterOut( const DepartureInfo &departureInfo ) const {
    return (m_settings.filtersEnabled
	   && m_settings.currentFilterSettings().filterOut(departureInfo))
	   || !isTimeShown( departureInfo.predictedDeparture() );
}

QString PublicTransport::titleText() const {
    QString sServiceProvider = currentServiceProviderData()["shortUrl"].toString();
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

QString PublicTransport::formatDateFancyFuture( const QDate& date ) const {
    int dayDiff = QDate::currentDate().daysTo( date );
    if ( dayDiff == 1 )
	return i18n( "tomorrow" );
    else if ( dayDiff <= 6 )
	return date.toString( "ddd" );
    else
	return KGlobal::locale()->formatDate( date, KLocale::ShortDate );
}

QString PublicTransport::departureText( const JourneyInfo& journeyInfo,
					bool htmlFormatted ) const {
    QString sTime, sDeparture = journeyInfo.departure().toString("hh:mm");
    if ( htmlFormatted && m_settings.displayTimeBold )
	sDeparture = sDeparture.prepend("<span style='font-weight:bold;'>")
			       .append("</span>");
    
    if ( journeyInfo.departure().date() != QDate::currentDate() )
	sDeparture += ", " + formatDateFancyFuture( journeyInfo.departure().date() );
    
    if ( m_settings.showDepartureTime && m_settings.showRemainingMinutes ) {
	QString sText = journeyInfo.durationToDepartureString();
	if ( htmlFormatted ) {
	    sText = sText.replace( QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"),
		    QString("<span style='color:%1;'>+&nbsp;\\1</span>")
		    .arg(Global::textColorDelayed().name()) );
	}

	if ( m_settings.linesPerRow > 1 )
	    sTime = QString(htmlFormatted ? "%1<br>(%2)" : "%1\n(%2)")
		    .arg( sDeparture ).arg( sText );
	else
	    sTime = QString("%1 (%2)").arg( sDeparture ).arg( sText );
    } else if ( m_settings.showDepartureTime ) {
	sTime = sDeparture;
    } else if ( m_settings.showRemainingMinutes ) {
	sTime = journeyInfo.durationToDepartureString();
	if ( htmlFormatted ) {
	    sTime = sTime.replace( QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"),
		    QString("<span style='color:%1;'>+&nbsp;\\1</span>")
		    .arg(Global::textColorDelayed().name()) );
	}
    } else
	sTime = QString();

    return sTime;
}

QString PublicTransport::arrivalText( const JourneyInfo& journeyInfo ) const {
    QString sTime, sArrival = journeyInfo.arrival().toString("hh:mm");
    if ( m_settings.displayTimeBold )
	sArrival = sArrival.prepend("<span style='font-weight:bold;'>").append("</span>");
    
    if ( journeyInfo.arrival().date() != QDate::currentDate() )
	sArrival += ", " + formatDateFancyFuture( journeyInfo.arrival().date() );
    
    if ( m_settings.showDepartureTime && m_settings.showRemainingMinutes ) {
	QString sText = journeyInfo.durationToDepartureString(true);
	sText = sText.replace( QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"),
		QString("<span style='color:%1;'>+&nbsp;\\1</span>")
		.arg(Global::textColorDelayed().name()) );

	if ( m_settings.linesPerRow > 1 )
	    sTime = QString("%1<br>(%2)").arg( sArrival ).arg( sText );
	else
	    sTime = QString("%1 (%2)").arg( sArrival ).arg( sText );
    } else if ( m_settings.showDepartureTime ) {
	sTime = sArrival;
    } else if ( m_settings.showRemainingMinutes ) {
	sTime = journeyInfo.durationToDepartureString( true );
	sTime = sTime.replace( QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"),
		QString("<span style='color:%1;'>+&nbsp;\\1</span>")
		.arg(Global::textColorDelayed().name()) );
    } else
	sTime = QString();

    return sTime;
}

QString PublicTransport::departureText( const DepartureInfo &departureInfo,
					bool htmlFormatted ) const {
    QDateTime predictedDeparture = departureInfo.predictedDeparture();
    QString sTime, sDeparture = predictedDeparture.toString("hh:mm");
    QString sColor;
    if ( htmlFormatted ) {
	if ( departureInfo.delayType() == OnSchedule )
	    sColor = QString( "color:%1;" ).arg( Global::textColorOnSchedule().name() );
	else if ( departureInfo.delayType() == Delayed )
	    sColor = QString( "color:%1;" ).arg( Global::textColorDelayed().name() );

	QString sBold;
	if ( m_settings.displayTimeBold )
	    sBold = "font-weight:bold;";
	
	sDeparture = sDeparture.prepend(
		QString("<span style='%1%2'>").arg(sColor).arg(sBold) )
		.append( "</span>" );
    }
    if ( predictedDeparture.date() != QDate::currentDate() )
	sDeparture += ", " + formatDateFancyFuture( predictedDeparture.date() );

    if ( m_settings.showDepartureTime && m_settings.showRemainingMinutes ) {
	QString sText = departureInfo.durationString( false );
	sDeparture += departureInfo.delayString(); // Show delay after time
	if ( htmlFormatted ) {
	    sDeparture = sDeparture.replace( QRegExp("(\\(?\\+(?:\\s|&nbsp;)*\\d+\\)?)"),
		    QString("<span style='color:%1;'>\\1</span>")
		    .arg(Global::textColorDelayed().name()) );
	    sTime = QString( m_settings.linesPerRow > 1 ? "%1<br>(%2)" : "%1 (%2)" )
		    .arg( sDeparture ).arg( sText );
	} else
	    sTime = QString( m_settings.linesPerRow > 1 ? "%1\n(%2)" : "%1 (%2)" )
		    .arg( sDeparture ).arg( sText );
    } else if ( m_settings.showDepartureTime ) {
	if ( htmlFormatted ) {
	    sDeparture += departureInfo.delayString().replace(
		    QRegExp("(\\(?\\+(?:\\s|&nbsp;)*\\d+\\)?)"),
		    QString("<span style='color:%1;'>\\1</span>")
		    .arg(Global::textColorDelayed().name()) ); // Show delay after time
	} else
	    sDeparture += departureInfo.delayString();
	sTime = sDeparture;
    } else if ( m_settings.showRemainingMinutes ) {
	sTime = departureInfo.durationString();
	if ( htmlFormatted ) {
	    if ( m_settings.linesPerRow == 1 )
		sTime = sTime.replace( ' ', "&nbsp;" ); // No line breaking
	    DelayType type = departureInfo.delayType();
	    if ( type == Delayed ) {
		sTime = sTime.replace( QRegExp("(\\(?\\+(?:\\s|&nbsp;)*\\d+\\)?)"),
			QString("<span style='color:%1;'>\\1</span>")
			.arg(Global::textColorDelayed().name()) );
	    } else if ( type == OnSchedule ) {
		sTime = sTime.prepend( QString("<span style='color:%1;'>")
			.arg(Global::textColorOnSchedule().name()) )
			.append( "</span>" );
	    }
	}
    } else
	sTime = QString();

    return sTime;
}

void PublicTransport::setTextColorOfHtmlItem( QStandardItem *item, const QColor &textColor ) {
    item->setText( item->text().prepend("<span style='color:rgba(%1,%2,%3,%4);'>")
	    .arg( textColor.red() ).arg( textColor.green() ).arg( textColor.blue() )
	    .arg( textColor.alpha() ).append("</span>") );
}

int PublicTransport::findDeparture( const DepartureInfo& departureInfo ) const {
    // TODO: Could start at an index based on the departure time.
    // Compute some index that is somewhere at the position of that time.
    QModelIndexList indices = m_model->match( m_model->index(0, 0),
	    TimetableItemHashRole, departureInfo.hash(), 1, Qt::MatchFixedString );
    return indices.isEmpty() ? -1 : indices.first().row();
}

int PublicTransport::findJourney( const JourneyInfo& journeyInfo ) const {
    // TODO: Could start at an index based on the departure time.
    // Compute some index that is somewhere at the position of that time.
    QModelIndexList indices = m_modelJourneys->match( m_modelJourneys->index(0, 0),
	    TimetableItemHashRole, journeyInfo.hash(), 1, Qt::MatchFixedString );
    return indices.isEmpty() ? -1 : indices.first().row();
}

void PublicTransport::setValuesOfJourneyItem( QStandardItem* journeyItem,
					      const JourneyInfo &journeyInfo,
					      ItemInformation journeyInformation,
					      bool update ) {
    QStringList sList;
    QString s, s2, s3;
    QStandardItem *item;
    int row;
    switch ( journeyInformation ) {
	case VehicleTypeListItem:
	    journeyItem->setIcon( Global::iconFromVehicleTypeList(
		    journeyInfo.vehicleTypes(), 32 * m_settings.sizeFactor) );
	    journeyItem->setData( journeyItem->text(), SortRole );
	    journeyItem->setData( journeyInfo.operatorName(), OperatorRole );
	    journeyItem->setData( m_settings.linesPerRow, HtmlDelegate::LinesPerRowRole );
	    if ( !update ) {
		journeyItem->setData( journeyInfo.hash(), TimetableItemHashRole );
		journeyItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case JourneyInfoItem:
	    s = QString( m_settings.linesPerRow > 1 ? i18n("<b>From:</b> %1<br><b>To:</b> %2")
			 : i18n("<b>From:</b> %1, <b>To:</b> %2") )
		    .arg( journeyInfo.startStopName() )
		    .arg( journeyInfo.targetStopName() );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
// 	    journeyItem->setData( journeyItem->text(), SortRole );
	    journeyItem->setData( m_settings.linesPerRow, HtmlDelegate::LinesPerRowRole );
	    
	    if ( !journeyInfo.journeyNews().isEmpty() ) {
		journeyItem->setIcon( Global::makeOverlayIcon(
			KIcon("view-pim-news"), "arrow-down", QSize(12,12)) );
		journeyItem->setData( static_cast<int>(HtmlDelegate::Right),
				      HtmlDelegate::DecorationPositionRole );
	    }
	    if ( !update )
		journeyItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    break;
	    
	case JourneyNewsItem:
	    s2 = journeyInfo.journeyNews();
	    if ( s2.startsWith("http://") ) // TODO: Make the link clickable...
		s2 = QString("<a href='%1'>%2</a>").arg(s2).arg(i18n("Link to journey news"));
	    s3 = s = QString("<b>%1</b> %2").arg( i18nc("News for a journey with public "
			"transport, like 'platform changed'", "News:") ).arg( s2 );
	    s3.replace(QRegExp("<[^>]*>"), "");
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s3 );
	    if ( !update ) {
		journeyItem->setData( static_cast<int>(JourneyNewsItem), Qt::UserRole );
		journeyItem->setData( qMin(3, s3.length() / 20),
				      HtmlDelegate::LinesPerRowRole );
// 		setTextColorOfHtmlItem( journeyItem, m_colorSubItemLabels );
	    }
	    break;
	    
	case DepartureItem:
	    journeyItem->setData( departureText(journeyInfo),
				  HtmlDelegate::FormattedTextRole );
	    s = departureText( journeyInfo, false );
	    if ( m_settings.linesPerRow > 1 ) {
		// Get longest line for auto column sizing
		sList = s.split( "\n", QString::SkipEmptyParts, Qt::CaseInsensitive );
		s = QString();
		foreach( QString sCurrent, sList ) {
// 		    sCurrent.replace(QRegExp("<[^>]*>"), "");
		    if ( sCurrent.length() > s.length() )
			s = sCurrent;
		}
	    }
	    journeyItem->setText( s ); // This is just used for auto column sizing
	    
	    journeyItem->setData( journeyInfo.departure(), SortRole );
	    journeyItem->setData( m_settings.linesPerRow, HtmlDelegate::LinesPerRowRole );
	    journeyItem->setData( qCeil((float)QDateTime::currentDateTime().secsTo(
		    journeyInfo.departure() ) / 60.0f), RemainingMinutesRole );
	    journeyItem->setData( journeyInfo.vehicleTypesVariant(), VehicleTypeListRole );
	    if ( !update ) {
		journeyItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case ArrivalItem:
	    journeyItem->setData( s = arrivalText(journeyInfo),
				  HtmlDelegate::FormattedTextRole );
				  // TODO arrivalText() with htmlFormatted == false
	    if ( m_settings.linesPerRow > 1 ) {
		// Get longest line for auto column sizing
		sList = s.split("<br>", QString::SkipEmptyParts, Qt::CaseInsensitive);
		s = QString();
		foreach( QString sCurrent, sList ) {
		    sCurrent.replace(QRegExp("<[^>]*>"), "");
		    if ( sCurrent.replace(QRegExp("(&\\w{2,5};|&#\\d{3,4};)"), " ").length()
				> s.length() )
			s = sCurrent;
		}
		journeyItem->setText( s ); // This is just used for auto column sizing
	    } else
		journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") ); // This is just used for auto column sizing
	    journeyItem->setData( journeyInfo.arrival(), SortRole );
	    journeyItem->setData( m_settings.linesPerRow, HtmlDelegate::LinesPerRowRole );
	    journeyItem->setData( qCeil((float)QDateTime::currentDateTime().secsTo(
		    journeyInfo.arrival() ) / 60.0f), RemainingMinutesRole );
	    journeyItem->setData( journeyInfo.vehicleTypesVariant(), VehicleTypeListRole );
	    if ( !update ) {
		journeyItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case StartStopNameItem:
	    journeyItem->setText( journeyInfo.startStopName() );
// 	    journeyItem->setData( journeyInfo.startStopName(), SortRole );
	    break;

	case TargetStopNameItem:
	    journeyItem->setText( journeyInfo.targetStopName() );
// 	    journeyItem->setData( journeyInfo.targetStopName(), SortRole );
	    break;

	case DurationItem:
	    if ( journeyInfo.duration() <= 0 ) {
		s = QString( "<b>%1</b> %2" )
			.arg( i18nc("The duration of a journey", "Duration:") )
			.arg( 0 );
	    } else {
		s = QString( "<b>%1</b> %2" )
			.arg( i18nc("The duration of a journey", "Duration:") )
			.arg( Global::durationString(journeyInfo.duration() * 60) );
	    }
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
// 	    journeyItem->setData( journeyInfo.duration(), SortRole );
	    if ( !update )
		journeyItem->setData( static_cast<int>(DurationItem), Qt::UserRole );
	    break;

	case ChangesItem:
	    s = QString( "<b>%1</b> %2" )
		    .arg( i18nc("The changes of a journey", "Changes:") )
		    .arg( journeyInfo.changes() );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
// 	    journeyItem->setData( journeyInfo.changes(), SortRole );
	    if ( !update )
		journeyItem->setData( static_cast<int>(ChangesItem), Qt::UserRole );
	    break;

	case PricingItem:
	    s = QString( "<b>%1</b> %2" )
		    .arg( i18nc("The pricing of a journey", "Pricing:") )
		    .arg( journeyInfo.pricing() );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s );
// 	    journeyItem->setData( journeyInfo.pricing(), SortRole );
	    if ( !update )
		journeyItem->setData( static_cast<int>(PricingItem), Qt::UserRole );
	    break;

	case OperatorItem:
	    s = QString( "<b>%1</b> %2" )
		    .arg( i18nc("The company that is responsible for this "
				"departure/arrival/journey", "Operator:") )
		    .arg( journeyInfo.operatorName() );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		journeyItem->setData( static_cast<int>(OperatorItem), Qt::UserRole );
// 		setTextColorOfHtmlItem( journeyItem, m_colorSubItemLabels );
	    }
	    break;
	    
	case RouteItem:
	    if ( journeyInfo.routeExactStops() > 0
		    && journeyInfo.routeExactStops() < journeyInfo.routeStops().count() ) {
		s = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("> %1 stops", journeyInfo.routeStops().count()) );
	    } else {
		s = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("%1 stops", journeyInfo.routeStops().count()) );
	    }
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    
	    // Remove all child rows
	    journeyItem->removeRows( 0, journeyItem->rowCount() );
	    
	    // Add route stops as child rows
	    for ( row = 0; row < journeyInfo.routeStops().count() - 1; ++row ) {
		// Add a separator item, when the exact route ends
		if ( row == journeyInfo.routeExactStops() && row > 0 ) {
		    QStandardItem *itemSeparator = new QStandardItem(
			    i18n("  - End of exact route -  ") );
// 		    setTextColorOfHtmlItem( itemSeparator, m_colorSubItemLabels );
		    journeyItem->appendRow( itemSeparator );
		}

		KIcon icon;
		QString sTransportLine;
		if ( row < journeyInfo.routeVehicleTypes().count()
			    && journeyInfo.routeVehicleTypes()[row] != Unknown)
		    icon = Global::vehicleTypeToIcon( journeyInfo.routeVehicleTypes()[row] );
		    if ( journeyInfo.routeVehicleTypes()[row] == Feet )
			sTransportLine = i18n("Footway");
		    else if ( journeyInfo.routeTransportLines().count() > row )
			sTransportLine = journeyInfo.routeTransportLines()[row];
		else {
		    icon = KIcon("public-transport-stop");
		    if ( journeyInfo.routeTransportLines().count() > row )
			sTransportLine = journeyInfo.routeTransportLines()[row];
		}

		QString stopDep = journeyInfo.routeStops()[row];
		QString stopArr = journeyInfo.routeStops()[row + 1];
		if ( journeyInfo.routePlatformsDeparture().count() > row
			&& !journeyInfo.routePlatformsDeparture()[row].isEmpty() ) {
		    stopDep = i18n("Platform %1", journeyInfo.routePlatformsDeparture()[row])
			    + " - " + stopDep;
		}
		if ( journeyInfo.routePlatformsArrival().count() > row 
			&& !journeyInfo.routePlatformsArrival()[row].isEmpty() ) {
		    stopArr = i18n("Platform %1", journeyInfo.routePlatformsArrival()[row])
			    + " - " + stopArr;
		}
		
		QString sTimeDep = journeyInfo.routeTimesDeparture()[row].toString("hh:mm");
		if ( journeyInfo.routeTimesDepartureDelay().count() > row ) {
		    int delay = journeyInfo.routeTimesDepartureDelay()[ row ];
		    if ( delay > 0 ) {
			sTimeDep += QString( " <span style='color:%2;'>+%1</span>" )
				.arg( delay ).arg( Global::textColorDelayed().name() );
		    } else if ( delay == 0 ) {
			sTimeDep = sTimeDep.prepend( QString("<span style='color:%1;'>")
				.arg(Global::textColorOnSchedule().name()) )
				.append( "</span>" );
		    }
		}
		
		QString sTimeArr = journeyInfo.routeTimesArrival()[row].toString("hh:mm");
		if ( journeyInfo.routeTimesArrivalDelay().count() > row ) {
		    int delay = journeyInfo.routeTimesArrivalDelay()[ row ];
		    if ( delay > 0 ) {
			sTimeArr += QString( " <span style='color:%2;'>+%1</span>" )
				.arg( delay ).arg( Global::textColorDelayed().name() );
		    } else if ( delay == 0 ) {
			sTimeArr = sTimeArr.prepend( QString("<span style='color:%1;'>")
				.arg(Global::textColorOnSchedule().name()) )
				.append( "</span>" );
		    }
		    
		}
			    
		if ( sTransportLine.isEmpty() ) {
		    item = new QStandardItem( icon,
			    i18nc("%1 is the departure time, %2 the origin stop name, "
				  "%3 the arrival time, %4 the target stop name.",
				  "dep: %1 - %2<br>arr: %3 - %4",
				  sTimeDep, stopDep, sTimeArr, stopArr) );
		    item->setData( 2, HtmlDelegate::LinesPerRowRole );
		} else {
		    item = new QStandardItem( icon,
			    i18nc("%1 is the departure time, %2 the origin stop name, "
				  "%3 the arrival time, %4 the target stop name, "
				  "%5 the transport line.",
				  "<b>%5</b><br>dep: %1 - %2<br>arr: %3 - %4",
				  sTimeDep, stopDep, sTimeArr, stopArr,
				  sTransportLine) );
		    item->setData( 3, HtmlDelegate::LinesPerRowRole );
		}

		item->setData( true, HtmlDelegate::DrawBackgroundGradientRole );
		item->setData( row, SortRole );
		int iconExtend = 16 * m_settings.sizeFactor;
		item->setData( QSize(iconExtend, iconExtend),
			       HtmlDelegate::IconSizeRole );
// 		setTextColorOfHtmlItem( item, m_colorSubItemLabels );

		journeyItem->appendRow( item );
	    }
	    
	    if ( !update ) {
		journeyItem->setData( static_cast<int>(RouteItem), Qt::UserRole );
// 		setTextColorOfHtmlItem( journeyItem, m_colorSubItemLabels );
	    }
	    break;

	default:
	    break;
    }
}

void PublicTransport::setValuesOfDepartureItem( QStandardItem* departureItem,
						const DepartureInfo &departureInfo,
						ItemInformation departureInformation,
						bool update ) {
    QString s, s2, s3;
    QStringList sList;
    QStandardItem *item;
    QVariant v;
    int row;
    QFont font;
    switch ( departureInformation ) {
	case LineNameItem:
	    departureItem->setText( departureInfo.lineString() );
	    departureItem->setData( QString("<span style='font-weight:bold;'>%1</span>")
		    .arg(departureInfo.lineString()), HtmlDelegate::FormattedTextRole );
	    departureItem->setData( departureInfo.lineString(), SortRole );
	    departureItem->setData( departureInfo.operatorName(), OperatorRole );
	    departureItem->setData( m_settings.linesPerRow, HtmlDelegate::LinesPerRowRole );
// 	    departureItem->setData( QVariant::fromValue<DepartureInfo>(departureInfo), DepartureInfoRole );
	    if ( departureInfo.vehicleType() != Unknown )
		departureItem->setIcon( Global::vehicleTypeToIcon(departureInfo.vehicleType()) );
	    if ( !update ) {
		departureItem->setTextAlignment( Qt::AlignRight );
		departureItem->setData( departureInfo.hash(), TimetableItemHashRole );
		departureItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case TargetItem:
	    departureItem->setText( departureInfo.target() );
	    departureItem->setData( departureInfo.target(), SortRole );
	    if ( !departureInfo.journeyNews().isEmpty() ) {
		departureItem->setIcon( Global::makeOverlayIcon(KIcon("view-pim-news"),
								"arrow-down", QSize(12,12)) );
		departureItem->setData( static_cast<int>(HtmlDelegate::Right),
					HtmlDelegate::DecorationPositionRole );
	    }
	    if ( !update ) {
		departureItem->setData( m_settings.linesPerRow,
					HtmlDelegate::LinesPerRowRole );
		departureItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case DepartureItem:
	    s = departureText( departureInfo );
	    s2 = departureText( departureInfo, false );
	    v = departureItem->data( HtmlDelegate::FormattedTextRole );
	    if ( !v.isValid() || v.toString() != s ) {
		departureItem->setData( s, HtmlDelegate::FormattedTextRole );
		if ( m_settings.linesPerRow > 1 ) {
		    // Get longest line for auto column sizing
		    sList = s2.split( "\n", QString::SkipEmptyParts, Qt::CaseInsensitive );
		    s2 = QString();
		    foreach( QString sCurrent, sList ) {
			if ( sCurrent.length() > s.length() )
			    s = sCurrent;
		    }
		}
		departureItem->setText( s2 ); // This is just used for auto column sizing
	    }
	    departureItem->setData( departureInfo.predictedDeparture(), SortRole );
	    departureItem->setData( m_settings.linesPerRow, HtmlDelegate::LinesPerRowRole );
	    departureItem->setData( qCeil((float)QDateTime::currentDateTime().secsTo(
		    departureInfo.predictedDeparture() ) / 60.0f), RemainingMinutesRole );
	    departureItem->setData( static_cast<int>(departureInfo.vehicleType()), VehicleTypeRole );
	    if ( !update ) {
		departureItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case PlatformItem:
	    s = QString("<b>%1</b> %2")
		    .arg( i18nc("The platform from which a tram/bus/train departs",
				"Platform:") )
		    .arg( departureInfo.platform() );
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		departureItem->setData( static_cast<int>(PlatformItem), Qt::UserRole );
// 		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case OperatorItem:
	    s = QString("<b>%1</b> %2").arg( i18nc("The company that is responsible for this departure/arrival/journey", "Operator:") ).arg( departureInfo.operatorName() );
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		departureItem->setData( static_cast<int>(OperatorItem), Qt::UserRole );
// 		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;
	    
	case RouteItem:
	    if ( departureInfo.routeExactStops() < departureInfo.routeStops().count() ) {
		s = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("> %1 stops", departureInfo.routeStops().count()) );
	    } else {
		s = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("%1 stops", departureInfo.routeStops().count()) );
	    }
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );

	    // Remove all child rows
	    departureItem->removeRows( 0, departureItem->rowCount() );
	    
	    // Add route stops as child rows
	    for ( row = 0; row < departureInfo.routeStops().count(); ++row ) {
		// Add a separator item, when the exact route ends
		if ( row == departureInfo.routeExactStops() && row > 0 ) {
		    QStandardItem *itemSeparator = new QStandardItem(
			    i18n("  - End of exact route -  "));
// 		    setTextColorOfHtmlItem( itemSeparator, m_colorSubItemLabels );
		    departureItem->appendRow( itemSeparator );
		}
		
		item = new QStandardItem( KIcon("public-transport-stop"),
			QString("%1 - %2")
			.arg(departureInfo.routeTimes()[row].toString("hh:mm"))
			.arg(departureInfo.routeStops()[row]) );
		item->setData( row, SortRole );
// 		setTextColorOfHtmlItem( item, m_colorSubItemLabels );
		departureItem->appendRow( item );
	    }
	    
	    if ( !update ) {
		departureItem->setData( static_cast<int>(RouteItem), Qt::UserRole );
// 		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case JourneyNewsItem:
	    s2 = departureInfo.journeyNews();
	    if ( s2.startsWith("http://") ) // TODO: Make the link clickable...
		s2 = QString("<a href='%1'>%2</a>").arg(s2).arg(i18n("Link to journey news"));
	    s3 = s = QString("<b>%1</b> %2").arg( i18nc("News for a journey with public "
		    "transport, like 'platform changed'", "News:") ).arg( s2 );
	    s3.replace( QRegExp("<[^>]*>"), "" );
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s3 );
	    if ( !update ) {
		departureItem->setData( static_cast<int>(JourneyNewsItem), Qt::UserRole );
		departureItem->setData( qMin(3, s3.length() / 20),
					HtmlDelegate::LinesPerRowRole );
// 		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case DelayItem:
	    s = QString("<b>%1</b> %2")
		    .arg( i18nc("Information about delays of a journey with "
				    "public transport", "Delay:") )
		    .arg( departureInfo.delayText() );
	    if ( departureInfo.delayType() == Delayed ) {
		s += "<br><b>" + (m_settings.departureArrivalListType == ArrivalList
			? i18n("Original arrival time:")
			: i18n("Original departure time:")) + "</b> " +
			  departureInfo.departure().toString("hh:mm");
		departureItem->setData( 2, HtmlDelegate::LinesPerRowRole );
	    }

	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		departureItem->setData( static_cast<int>(DelayItem), Qt::UserRole );
// 		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	default:
	    break;
    }
}

void PublicTransport::appendJourney ( const JourneyInfo& journeyInfo ) {
    QList< QStandardItem* > items;
    foreach( TimetableColumn column, m_journeyViewColumns ) {
	QStandardItem *item = new QStandardItem();
	switch( column ) {
	    case VehicleTypeListColumn:
		setValuesOfJourneyItem( item, journeyInfo, VehicleTypeListItem );
		break;
	    case JourneyInfoColumn:
		setValuesOfJourneyItem( item, journeyInfo, JourneyInfoItem );
		break;
	    case DepartureColumn:
		setValuesOfJourneyItem( item, journeyInfo, DepartureItem );
		break;
	    case ArrivalColumn:
		setValuesOfJourneyItem( item, journeyInfo, ArrivalItem );
		break;

	    default:
		kDebug() << "not included" << column;
		break;
	}

	items << item;
    }
    m_modelJourneys->appendRow( items );

    if ( journeyInfo.changes() >= 0 ) {
	QStandardItem *itemChanges = new QStandardItem();
	setValuesOfJourneyItem( itemChanges, journeyInfo, ChangesItem );
	items[0]->appendRow( itemChanges );
    }

    if ( !journeyInfo.pricing().isEmpty() ) {
	QStandardItem *itemPricing = new QStandardItem();
	setValuesOfJourneyItem( itemPricing, journeyInfo, PricingItem );
	items[0]->appendRow( itemPricing );
    }

    if ( journeyInfo.duration() > 0 ) {
	QStandardItem *itemDuration = new QStandardItem();
	setValuesOfJourneyItem( itemDuration, journeyInfo, DurationItem );
	items[0]->appendRow( itemDuration );
    }
    
    if ( !journeyInfo.journeyNews().isEmpty() ) {
	QStandardItem *itemJourneyNews = new QStandardItem();
	setValuesOfJourneyItem( itemJourneyNews, journeyInfo, JourneyNewsItem );
	items[0]->appendRow( itemJourneyNews );
    }
    
    if ( !journeyInfo.operatorName().isEmpty() ) {
	QStandardItem *itemOperatorName = new QStandardItem();
	setValuesOfJourneyItem( itemOperatorName, journeyInfo, OperatorItem );
	items[0]->appendRow( itemOperatorName );
    }
    
    if ( !journeyInfo.routeStops().isEmpty() ) {
	QStandardItem *itemRoute = new QStandardItem();
	setValuesOfJourneyItem( itemRoute, journeyInfo, RouteItem );
	items[0]->appendRow( itemRoute );

	m_treeView->nativeWidget()->expand( itemRoute->index() );
	for ( int row = 0; row < itemRoute->rowCount(); ++row ) {
	    m_treeView->nativeWidget()->setFirstColumnSpanned(
		    row, itemRoute->index(), true );
	}
    }

    for ( int row = 0; row < items[0]->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned(row, items[0]->index(), true);
}

void PublicTransport::appendDeparture( const DepartureInfo& departureInfo ) {
    QList<QStandardItem*> items = QList<QStandardItem*>();
    foreach( TimetableColumn column, m_departureViewColumns ) {
	QStandardItem *item = new QStandardItem();
	switch( column ) {
	    case LineStringColumn:
		setValuesOfDepartureItem( item, departureInfo, LineNameItem );
		break;
	    case TargetColumn:
		setValuesOfDepartureItem( item, departureInfo, TargetItem );
		break;
	    case DepartureColumn:
		setValuesOfDepartureItem( item, departureInfo, DepartureItem );

		// TODO: check this!
		// Search if an abandoned alarm timer matches
		for( int i = m_abandonedAlarmTimer.count() - 1; i >= 0; --i ) {
		    AlarmTimer* alarmTimer = m_abandonedAlarmTimer[i];
		    setAlarmForDeparture( item->index(), alarmTimer );
		    m_abandonedAlarmTimer.removeAt(i);
		}
		break;

	    default:
		break;
	}

	items << item;
    }
    m_model->appendRow( items );

    int iRow = 0;
    if ( !departureInfo.platform().isEmpty() ) {
	QStandardItem *itemPlatform = new QStandardItem();
	setValuesOfDepartureItem( itemPlatform, departureInfo, PlatformItem );
	items[0]->insertRow( iRow++, itemPlatform );
    }

    if ( !departureInfo.journeyNews().isEmpty() ) {
	QStandardItem *itemJourneyNews = new QStandardItem();
	setValuesOfDepartureItem( itemJourneyNews, departureInfo, JourneyNewsItem );
	items[0]->insertRow( iRow++, itemJourneyNews );
    }

    if ( !departureInfo.operatorName().isEmpty() ) {
	QStandardItem *itemOperatorName = new QStandardItem();
	setValuesOfDepartureItem( itemOperatorName, departureInfo, OperatorItem );
	items[0]->insertRow( iRow++, itemOperatorName );
    }
    
    if ( !departureInfo.routeStops().isEmpty() ) {
	QStandardItem *itemRoute = new QStandardItem();
	setValuesOfDepartureItem( itemRoute, departureInfo, RouteItem );
	items[0]->insertRow( iRow++, itemRoute );

	for ( int row = 0; row < itemRoute->rowCount(); ++row ) {
	    m_treeView->nativeWidget()->setFirstColumnSpanned(
		    row, itemRoute->index(), true );
	}
    }

    QStandardItem *itemDelay = new QStandardItem();;
    setValuesOfDepartureItem( itemDelay, departureInfo, DelayItem );
    items[0]->insertRow( iRow++, itemDelay );

    for ( int row = 0; row < items[0]->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned( row, items[0]->index(), true );
}

void PublicTransport::updateJourney( int row, const JourneyInfo& journeyInfo ) {
    QStandardItem *itemFirst = m_modelJourneys->item( row, 0 );
    QStandardItem *itemVehicleTypes = m_modelJourneys->item(
	    row, m_journeyViewColumns.indexOf(VehicleTypeListColumn) );
    QStandardItem *itemJourneyInfo = m_modelJourneys->item(
	    row, m_journeyViewColumns.indexOf(JourneyInfoColumn) );
    QStandardItem *itemDeparture = m_modelJourneys->item(
	    row, m_journeyViewColumns.indexOf(DepartureColumn) );
    QStandardItem *itemArrival = m_modelJourneys->item(
	    row, m_journeyViewColumns.indexOf(ArrivalColumn) );
    setValuesOfJourneyItem( itemVehicleTypes, journeyInfo, VehicleTypeListItem, true );
    setValuesOfJourneyItem( itemJourneyInfo, journeyInfo, JourneyInfoItem, true );
    setValuesOfJourneyItem( itemDeparture, journeyInfo, DepartureItem, true );
    setValuesOfJourneyItem( itemArrival, journeyInfo, ArrivalItem, true );
    
    // Get items
    QStandardItem *itemChanges = NULL, *itemPricing = NULL, *itemDuration = NULL,
	    *itemJourneyNews = NULL, *itemOperatorName = NULL, *itemRoute = NULL;
    for ( int row = 0; row < itemFirst->rowCount(); ++row ) {
	ItemInformation type = static_cast< ItemInformation >(
		itemFirst->child(row)->data(Qt::UserRole).toInt() );
	switch ( type ) {
	    case ChangesItem: itemChanges = itemFirst->child( row );
		break;
	    case PricingItem: itemPricing = itemFirst->child( row );
		break;
	    case DurationItem: itemDuration = itemFirst->child( row );
		break;
	    case JourneyNewsItem: itemJourneyNews = itemFirst->child( row );
		break;
	    case OperatorItem: itemOperatorName = itemFirst->child( row );
		break;
	    case RouteItem: itemRoute = itemFirst->child( row );
		break;
	    default:
		break; // Always visible item
	}
    }
    
    // Update changes
    updateOrCreateItem( journeyInfo.changes() <= 0,
	    itemFirst, itemChanges, journeyInfo, ChangesItem );

    // Update pricing
    updateOrCreateItem( journeyInfo.pricing().isEmpty(),
	    itemFirst, itemPricing, journeyInfo, PricingItem );

    // Update duration
    updateOrCreateItem( journeyInfo.duration() <= 0,
	    itemFirst, itemDuration, journeyInfo, DurationItem );
    
    // Update journey news
    updateOrCreateItem( journeyInfo.journeyNews().isEmpty(),
	    itemFirst, itemJourneyNews, journeyInfo, JourneyNewsItem );
    
    // Update operator name
    updateOrCreateItem( journeyInfo.operatorName().isEmpty(),
	    itemFirst, itemOperatorName, journeyInfo, OperatorItem );
    
    // Update route
    if ( !journeyInfo.routeStops().isEmpty() ) {
	if ( !itemRoute ) { // Create new route item
	    itemRoute = new QStandardItem();
	    setValuesOfJourneyItem( itemRoute, journeyInfo, RouteItem );
	    itemFirst->appendRow( itemRoute );
	} else // Update old route item
	    setValuesOfJourneyItem( itemRoute, journeyInfo, RouteItem, true );
	
	for ( int row = 0; row < itemRoute->rowCount(); ++row ) {
	    m_treeView->nativeWidget()->setFirstColumnSpanned(
	    row, itemRoute->index(), true );
	}
    } else if ( itemRoute ) {
	itemFirst->removeRow( itemRoute->row() );
    }
    
    for ( int row = 0; row < itemFirst->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned( row, itemFirst->index(), true );
}

void PublicTransport::updateOrCreateItem( bool remove, QStandardItem* parentItem,
		QStandardItem* item, const JourneyInfo &journeyInfo,
		ItemInformation itemInfo ) {
    if ( !remove ) {
	if ( !item ) { // Create new item
	    item = new QStandardItem();
	    setValuesOfJourneyItem( item, journeyInfo, itemInfo );
	    parentItem->appendRow( item );
	} else // Update old item
	    setValuesOfJourneyItem( item, journeyInfo, itemInfo, true );
    } else if ( item ) {
	parentItem->removeRow( item->row() );
    }
}

void PublicTransport::updateOrCreateItem( bool remove, QStandardItem* parentItem,
		QStandardItem* item, const DepartureInfo &departureInfo,
		ItemInformation itemInfo ) {
    if ( !remove ) {
	if ( !item ) { // Create new item
	    item = new QStandardItem();
	    setValuesOfDepartureItem( item, departureInfo, itemInfo );
	    parentItem->appendRow( item );
	} else // Update old item
	    setValuesOfDepartureItem( item, departureInfo, itemInfo, true );
    } else if ( item ) {
	parentItem->removeRow( item->row() );
    }
}

void PublicTransport::updateDeparture( int row, const DepartureInfo& departureInfo ) {
    QStandardItem *itemLineString = m_model->item(
	    row, m_departureViewColumns.indexOf(LineStringColumn) );
    QStandardItem *itemTarget = m_model->item(
	    row, m_departureViewColumns.indexOf(TargetColumn) );
    QStandardItem *itemDeparture = m_model->item(
	    row, m_departureViewColumns.indexOf(DepartureColumn) );
    setValuesOfDepartureItem( itemLineString, departureInfo, LineNameItem, true );
    setValuesOfDepartureItem( itemTarget, departureInfo, TargetItem, true );
    setValuesOfDepartureItem( itemDeparture, departureInfo, DepartureItem, true );

    // Get items
    QStandardItem *itemPlatform = NULL, *itemJourneyNews = NULL,
	    *itemOperatorName = NULL, *itemRoute = NULL, *itemDelay = NULL;
    for ( int i = 0; i < itemLineString->rowCount(); ++i ) {
	ItemInformation type = static_cast< ItemInformation >(
		itemLineString->child(i)->data(Qt::UserRole).toInt() );
	switch ( type ) {
	    case DelayItem: itemDelay = itemLineString->child( i );
		break;
	    case PlatformItem: itemPlatform = itemLineString->child( i );
		break;
	    case JourneyNewsItem: itemJourneyNews = itemLineString->child( i );
		break;
	    case OperatorItem: itemOperatorName = itemLineString->child( i );
		break;
	    case RouteItem: itemRoute = itemLineString->child( i );
		break;
	    default:
		break; // Always visible item
	}
    }
    
    // Update platform
    updateOrCreateItem( departureInfo.platform().isEmpty(),
	    itemLineString, itemPlatform, departureInfo, PlatformItem );
   
    // Update journey news
    updateOrCreateItem( departureInfo.journeyNews().isEmpty(),
	    itemLineString, itemJourneyNews, departureInfo, JourneyNewsItem );
    
    // Update operator name
    updateOrCreateItem( departureInfo.operatorName().isEmpty(),
	    itemLineString, itemOperatorName, departureInfo, OperatorItem );
    
    // Update route
    if ( !departureInfo.routeStops().isEmpty() ) {
	if ( !itemRoute ) { // Create new route item
	    itemRoute = new QStandardItem();
	    setValuesOfDepartureItem( itemRoute, departureInfo, RouteItem );
	    itemLineString->appendRow( itemRoute );
	} else // Update old route item
	    setValuesOfDepartureItem( itemRoute, departureInfo, RouteItem, true );
	
	for ( int row = 0; row < itemRoute->rowCount(); ++row ) {
	    m_treeView->nativeWidget()->setFirstColumnSpanned(
		    row, itemRoute->index(), true );
	}
    } else if ( itemRoute ) {
	itemLineString->removeRow( itemRoute->row() );
    }

    // Update delay
    if ( !itemDelay ) { // Create new delay item
	itemDelay = new QStandardItem();
	setValuesOfDepartureItem( itemDelay, departureInfo, DelayItem );
	itemLineString->appendRow( itemDelay );
    } else { // Update old delay item
	setValuesOfDepartureItem( itemDelay, departureInfo, DelayItem );
    }

    for ( int row = 0; row < itemLineString->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned( row, itemLineString->index(), true );
}

void PublicTransport::removeOldDepartures() {
    // Go through departure lists of all stops.
    // Add all departures that can be found in the model to foundRows.
    int newDepartureCount = 0;
    QList< int > foundRows;
    for ( int n = m_stopIndexToSourceName.count() - 1; n >= 0; --n ) {
	QString strippedSourceName = stripDateAndTimeValues( m_stopIndexToSourceName[n] );
	if ( !m_departureInfos.contains(strippedSourceName) ) {
	    kDebug() << "Source has no stored data in applet" << strippedSourceName;
	    continue;
	}
	
	// Go through departure infos of the current stop in the loop
	QList<DepartureInfo> &depInfos = m_departureInfos[ strippedSourceName ];
	newDepartureCount += depInfos.count(); // For debug only
	for ( int i = depInfos.count() - 1; i >= 0; --i ) {
	    DepartureInfo &departureInfo = depInfos[ i ];
	    int row = findDeparture( departureInfo );
	    if ( row != -1 ) {
		if ( filterOut(departureInfo) ) {
		    departureInfo.setVisible( false );
		} else {
		    foundRows << row;
		    departureInfo.setVisible( true );
		}
	    } else {
		departureInfo.setVisible( !filterOut(departureInfo) );
	    }
	}
    }
    kDebug() << "Found" << foundRows.count() << "of" << newDepartureCount << "departures";

    // Remove all departures that no longer have a corresponding departure 
    // from the data engine (old departures).
    for ( int row = m_model->rowCount() - 1; row >= 0; --row ) {
	if ( foundRows.contains(row) )
	    continue;

	QStandardItem *itemDeparture = m_model->item( row, 2 );
	AlarmTimer *alarmTimer = static_cast< AlarmTimer* >(
		itemDeparture->data(AlarmTimerRole).value<void*>() );
	if ( alarmTimer && alarmTimer->timer()->isActive() ) {
	    m_abandonedAlarmTimer.append( alarmTimer );
	    kDebug() << "Append abandoned alarm timer of not found row" << row
		     << "Abandoned alarm timer count:" << m_abandonedAlarmTimer.count();
	}

	m_model->removeRow( row );
    }
}

void PublicTransport::removeOldJourneys() {
    kDebug() << m_journeyInfos.count() << "journeys";

    QList< int > foundRows;
    foreach( JourneyInfo journeyInfo, m_journeyInfos ) {
	int row = findJourney( journeyInfo );
	if ( row != -1 ) {
	    // TODO: Filtering for journeys?
	    foundRows << row;
	}
    }
    
    for ( int row = m_modelJourneys->rowCount() - 1; row >= 0; --row ) {
	if ( foundRows.contains(row) )
	    continue;
	
	m_modelJourneys->removeRow( row );
    }
}

void PublicTransport::updateModelJourneys() {
    if ( !m_graphicsWidget )
	graphicsWidget();

    removeOldJourneys();
    foreach( JourneyInfo journeyInfo, m_journeyInfos ) {
	int row = findJourney( journeyInfo );

	if ( row != -1 ) // just update departure data
	    updateJourney( row, journeyInfo );
	else // append new departure
	    appendJourney( journeyInfo );
    }

    // Sort list of journeys
    qSort( m_journeyInfos.begin(), m_journeyInfos.end() );
}

void PublicTransport::updateModel() {
    if ( !m_graphicsWidget )
	graphicsWidget();

    if ( m_titleType == ShowDepartureArrivalListTitle ) {
	m_label->setText( titleText() );
	m_labelInfo->setToolTip( courtesyToolTip() );
	m_labelInfo->setText( infoText() );
    }
    removeOldDepartures();
    
    QList< DepartureInfo > depInfos = departureInfos();
    kDebug() << "Update / add" << depInfos.count() << "departures";
    foreach( const DepartureInfo &departureInfo, depInfos ) {
	QTime findStart = QTime::currentTime();
	int row = findDeparture( departureInfo );
	if ( row != -1 ) // just update departure data
	    updateDeparture( row, departureInfo );
	else // append new departure
	    appendDeparture( departureInfo );
    }
}

#include "publictransport.moc"