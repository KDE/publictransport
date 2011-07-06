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
#include "departurepainter.h"
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
#include <KNotification>
#include <KToolInvocation>
#include <KColorScheme>
#include <KSelectAction>
#include <KToggleAction>
#include <KLineEdit>
#include <KMenu>
#include <KMimeTypeTrader>
#include <KColorUtils>
#include <KStandardDirs>

// Plasma includes
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
#include <QGraphicsLinearLayout>
#include <QStateMachine>
#include <QHistoryState>
#include <QClipboard>
#include <qmath.h>
#include <qgraphicssceneevent.h>

PublicTransport::PublicTransport( QObject *parent, const QVariantList &args )
        : Plasma::PopupApplet( parent, args ),
        m_graphicsWidget(0), m_mainGraphicsWidget(0), m_oldItem(0), m_titleWidget(0),
        m_labelInfo(0), m_timetable(0), m_journeyTimetable(0), m_listStopSuggestions(0),
        m_overlay(0), m_model(0), m_popupIconTransitionAnimation(0),
        m_modelJourneys(0), m_departureProcessor(0), m_stateMachine(0),
        m_journeySearchTransition1(0), m_journeySearchTransition2(0),
        m_journeySearchTransition3(0)
{
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
    }
}

void PublicTransport::init()
{
    m_settings = SettingsIO::readSettings( config(), globalConfig() );

    // Create and connect the worker thread
    m_departureProcessor = new DepartureProcessor( this );
    connect( m_departureProcessor, SIGNAL(beginDepartureProcessing(QString)),
             this, SLOT(beginDepartureProcessing(QString)) );
    connect( m_departureProcessor, SIGNAL(departuresProcessed(QString,QList<DepartureInfo>,QUrl,QDateTime,int)),
             this, SLOT(departuresProcessed(QString,QList<DepartureInfo>,QUrl,QDateTime,int)) );
    connect( m_departureProcessor, SIGNAL(beginJourneyProcessing(QString)),
             this, SLOT(beginJourneyProcessing(QString)) );
    connect( m_departureProcessor, SIGNAL(journeysProcessed(QString,QList<JourneyInfo>,QUrl,QDateTime)),
             this, SLOT(journeysProcessed(QString,QList<JourneyInfo>,QUrl,QDateTime)) );
    connect( m_departureProcessor, SIGNAL(departuresFiltered(QString,QList<DepartureInfo>,QList<DepartureInfo>, QList<DepartureInfo>)),
             this, SLOT(departuresFiltered(QString,QList<DepartureInfo>,QList<DepartureInfo>,QList<DepartureInfo>)) );

    // Load vehicle type SVG
    m_vehiclesSvg.setImagePath( KGlobal::dirs()->findResource("data",
            "plasma_applet_publictransport/vehicles.svg") );
    m_vehiclesSvg.setContainsMultipleImages( true );

    m_departurePainter = new DeparturePainter( this );
    m_departurePainter->setSvg( &m_vehiclesSvg );

    if ( !m_settings.stopSettingsList.isEmpty() ) {
        m_currentServiceProviderFeatures =
            currentServiceProviderData()["features"].toStringList();
    }

    // Set icon and text of the default "run associated application" action
    if ( QAction *runAction = action("run associated application") ) {
        runAction->setText( i18nc("@item:inmenu", "&Show in Web-Browser") );

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
    m_model->setDepartureArrivalListType( m_settings.departureArrivalListType );

    // Create widgets
    graphicsWidget();

    // Setup actions and the state machine
    setupActions();
    setupStateMachine();

    // Check for network connectivity and create tooltip / popup icon
    checkNetworkStatus();
    createTooltip();
    createPopupIcon();

    connect( this, SIGNAL(geometryChanged()), this, SLOT(geometryChanged()) );
    connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
    connect( Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()),
             this, SLOT(themeChanged()) );
    emit settingsChanged();

    reconnectSource();
}

void PublicTransport::setupStateMachine()
{
    // Create the state machine
    m_stateMachine = new QStateMachine( this );

    // Create parallel main state and sub group states
    QState *mainStateGroup = new QState( QState::ParallelStates, m_stateMachine );
    QState *viewStateGroup = new QState( mainStateGroup );
    QState *departureDataStateGroup = new QState( mainStateGroup );
    QState *journeyDataStateGroup = new QState( mainStateGroup );
    QState *networkStateGroup = new QState( mainStateGroup );
    m_states.insert( "mainStateGroup", mainStateGroup );
    m_states.insert( "viewStateGroup", viewStateGroup );
    m_states.insert( "departureDataStateGroup", departureDataStateGroup );
    m_states.insert( "journeyDataStateGroup", journeyDataStateGroup );
    m_states.insert( "networkStateGroup", networkStateGroup );

    // Create View states
    QState *actionButtonsState = new QState( viewStateGroup );
    QState *departureViewState = new QState( viewStateGroup );
    QState *intermediateDepartureViewState = new QState( viewStateGroup );
    QState *journeyStateGroup = new QState( viewStateGroup );
    QState *journeyViewState = new QState( journeyStateGroup );
    QState *journeysUnsupportedViewState = new QState( journeyStateGroup );
    QState *journeySearchState = new QState( journeyStateGroup );
    m_states.insert( "actionButtons", actionButtonsState );
    m_states.insert( "departureView", departureViewState );
    m_states.insert( "intermediateDepartureView", intermediateDepartureViewState );
    m_states.insert( "journeyStateGroup", journeyStateGroup );
    m_states.insert( "journeyView", journeyViewState );
    m_states.insert( "journeysUnsupportedView", journeysUnsupportedViewState );
    m_states.insert( "journeySearch", journeySearchState );

    viewStateGroup->setInitialState( departureViewState );
    QHistoryState *lastMainState = new QHistoryState( viewStateGroup );
    lastMainState->setDefaultState( departureViewState );

    // Create sub states of the departure list state for arrivals/departures
    QState *departureState = new QState( departureViewState );
    QState *arrivalState = new QState( departureViewState );
    departureViewState->setInitialState(
            m_settings.departureArrivalListType == DepartureList
            ? departureState : arrivalState );
    QHistoryState *lastDepartureListState = new QHistoryState( departureViewState );
    lastDepartureListState->setDefaultState( departureState );

    // Create departure data states
    QState *departureDataWaitingState = new QState( departureDataStateGroup );
    QState *departureDataValidState = new QState( departureDataStateGroup );
    QState *departureDataInvalidState = new QState( departureDataStateGroup );
    departureDataStateGroup->setInitialState( departureDataWaitingState );
    m_states.insert( "departureDataWaiting", departureDataWaitingState );
    m_states.insert( "departureDataValid", departureDataValidState );
    m_states.insert( "departureDataInvalid", departureDataInvalidState );

    // Create journey data states
    QState *journeyDataWaitingState = new QState( journeyDataStateGroup );
    QState *journeyDataValidState = new QState( journeyDataStateGroup );
    QState *journeyDataInvalidState = new QState( journeyDataStateGroup );
    journeyDataStateGroup->setInitialState( journeyDataWaitingState );
    m_states.insert( "journeyDataWaiting", journeyDataWaitingState );
    m_states.insert( "journeyDataValid", journeyDataValidState );
    m_states.insert( "journeyDataInvalid", journeyDataInvalidState );

    // Create network states
    QState *networkStatusUnknownState = new QState( networkStateGroup );
    QState *networkConfiguringState = new QState( networkStateGroup );
    QState *networkActivatedState = new QState( networkStateGroup );
    QState *networkNotActivatedState = new QState( networkStateGroup );
    networkStateGroup->setInitialState( networkStatusUnknownState );
    m_states.insert( "networkStatusUnknown", networkStatusUnknownState );
    m_states.insert( "networkConfiguring", networkConfiguringState );
    m_states.insert( "networkActivated", networkActivatedState );
    m_states.insert( "networkNotActivated", networkNotActivatedState );

    // Set text to be displayed if no data is present when the network state changes
    networkConfiguringState->assignProperty( m_timetable, "noItemsText",
            i18nc("@info", "Network gets configured. Please wait...") );
    networkNotActivatedState->assignProperty( m_timetable, "noItemsText",
            i18nc("@info", "No network connection") );
    networkActivatedState->assignProperty( m_timetable, "noItemsText",
            i18nc("@info", "Network connection established") );

    m_journeySearchTransition1 =
            new ToPropertyTransition( action("searchJourneys"), SIGNAL(triggered()),
                    actionButtonsState, this, "supportedJourneySearchState" );
    m_journeySearchTransition2 =
            new ToPropertyTransition( action("searchJourneys"), SIGNAL(triggered()),
                    departureViewState, this, "supportedJourneySearchState" );
    m_journeySearchTransition3 =
            new ToPropertyTransition( action("searchJourneys"), SIGNAL(triggered()),
                    journeyViewState, this, "supportedJourneySearchState" );

    actionButtonsState->addTransition(
            action("showDepartures"), SIGNAL(triggered()), departureState );
    actionButtonsState->addTransition(
            action("showArrivals"), SIGNAL(triggered()), arrivalState );
    actionButtonsState->addTransition(
            this, SIGNAL(cancelActionButtons()), lastMainState );
    actionButtonsState->addTransition(
            this, SIGNAL(goBackToDepartureList()), lastDepartureListState );
    actionButtonsState->addTransition(
            action("backToDepartures"), SIGNAL(triggered()), lastDepartureListState );
    departureViewState->addTransition(
            m_titleWidget, SIGNAL(iconClicked()), actionButtonsState );
    departureViewState->addTransition(
            action("showActionButtons"), SIGNAL(triggered()), actionButtonsState );
    journeyViewState->addTransition(
            action("showActionButtons"), SIGNAL(triggered()), actionButtonsState );
    journeySearchState->addTransition(
            this, SIGNAL(journeySearchFinished()), journeyViewState );

    // Add a transition to the intermediate departure list state.
    // Gets triggered by eg. the context menus of route stop items.
    departureViewState->addTransition(
            this, SIGNAL(intermediateDepartureListRequested(QString)),
            intermediateDepartureViewState );

    // Add transitions to departure list and to arrival list
    departureViewState->addTransition(
            action("showDepartures"), SIGNAL(triggered()), departureState );
    departureViewState->addTransition(
            action("showArrivals"), SIGNAL(triggered()), arrivalState );

    intermediateDepartureViewState->addTransition(
            m_titleWidget, SIGNAL(iconClicked()), lastMainState );
    intermediateDepartureViewState->addTransition(
            action("backToDepartures"), SIGNAL(triggered()), lastMainState );
    
    journeySearchState->addTransition(
            m_titleWidget, SIGNAL(iconClicked()), lastMainState );
    journeySearchState->addTransition(
            m_titleWidget, SIGNAL(closeIconClicked()), lastMainState );

    journeyViewState->addTransition(
            m_titleWidget, SIGNAL(iconClicked()), actionButtonsState );
    journeyViewState->addTransition(
            m_titleWidget, SIGNAL(closeIconClicked()), lastDepartureListState );

    departureDataWaitingState->addTransition(
            this, SIGNAL(validDepartureDataReceived()), departureDataValidState );
    departureDataWaitingState->addTransition(
            this, SIGNAL(invalidDepartureDataReceived()), departureDataInvalidState );
    departureDataValidState->addTransition(
            this, SIGNAL(requestedNewDepartureData()), departureDataWaitingState );
    departureDataInvalidState->addTransition(
            this, SIGNAL(requestedNewDepartureData()), departureDataWaitingState );

    journeyDataWaitingState->addTransition(
            this, SIGNAL(validJourneyDataReceived()), journeyDataValidState );
    journeyDataWaitingState->addTransition(
            this, SIGNAL(invalidJourneyDataReceived()), journeyDataInvalidState );
    journeyDataValidState->addTransition(
            this, SIGNAL(requestedNewJourneyData()), journeyDataWaitingState );
    journeyDataInvalidState->addTransition(
            this, SIGNAL(requestedNewJourneyData()), journeyDataWaitingState );

    networkConfiguringState->addTransition(
            this, SIGNAL(networkConnectionLost()), networkNotActivatedState );
    networkActivatedState->addTransition(
            this, SIGNAL(networkConnectionLost()), networkNotActivatedState );
    networkActivatedState->addTransition(
            this, SIGNAL(networkIsConfiguring()), networkConfiguringState );
    networkNotActivatedState->addTransition(
            this, SIGNAL(networkIsConfiguring()), networkConfiguringState );
    networkConfiguringState->addTransition(
            this, SIGNAL(networkIsActivated()), networkActivatedState );
    networkNotActivatedState->addTransition(
            this, SIGNAL(networkIsActivated()), networkActivatedState );

    connect( actionButtonsState, SIGNAL(entered()),
             this, SLOT(showActionButtons()) );
    connect( actionButtonsState, SIGNAL(exited()),
             this, SLOT(destroyOverlay()) );

    connect( departureViewState, SIGNAL(entered()),
             this, SLOT(showDepartureList()) );

    connect( arrivalState, SIGNAL(entered()), this, SLOT(showArrivals()) );
    connect( departureState, SIGNAL(entered()), this, SLOT(showDepartures()) );

    connect( journeySearchState, SIGNAL(entered()),
             this, SLOT(showJourneySearch()) );
    connect( journeysUnsupportedViewState, SIGNAL(entered()),
             this, SLOT(showJourneysUnsupportedView()) );
    connect( journeyViewState, SIGNAL(entered()),
             this, SLOT(showJourneyList()) );
    connect( journeyViewState, SIGNAL(exited()),
             this, SLOT(disconnectJourneySource()) );

    connect( intermediateDepartureViewState, SIGNAL(entered()),
             this, SLOT(showIntermediateDepartureList()) );
    connect( intermediateDepartureViewState, SIGNAL(exited()),
             this, SLOT(removeIntermediateStopSettings()) );

    connect( departureViewState, SIGNAL(entered()),
             this, SLOT(setAssociatedApplicationUrlForDepartures()) );
    connect( journeyViewState, SIGNAL(entered()),
             this, SLOT(setAssociatedApplicationUrlForJourneys()) );

    connect( departureDataWaitingState, SIGNAL(entered()),
             this, SLOT(departureDataWaitingStateEntered()) );
    connect( departureDataInvalidState, SIGNAL(entered()),
             this, SLOT(departureDataInvalidStateEntered()) );
    connect( departureDataValidState, SIGNAL(entered()),
             this, SLOT(departureDataValidStateEntered()) );

    connect( journeyDataWaitingState, SIGNAL(entered()),
             this, SLOT(journeyDataWaitingStateEntered()) );
    connect( journeyDataInvalidState, SIGNAL(entered()),
             this, SLOT(journeyDataInvalidStateEntered()) );
    connect( journeyDataValidState, SIGNAL(entered()),
             this, SLOT(journeyDataValidStateEntered()) );

    m_stateMachine->setInitialState( mainStateGroup );
    m_stateMachine->start();
}

bool PublicTransport::checkNetworkStatus()
{
    QString status = queryNetworkStatus();
    if ( status == "unavailable" ) {
        emit networkConnectionLost();
        return false;
    } else if ( status == "configuring" ) {
        emit networkIsConfiguring();
        return false;
    } else if ( status == "activated" /*&& m_currentMessage == MessageError TODO*/ ) {
        emit networkIsActivated();
        return false;
    } else {
        kDebug() << "Unknown network status or no error message was shown" << status;
        return true;
    }
}

QString PublicTransport::queryNetworkStatus()
{
    QString status = "unavailable";
    const QStringList interfaces = dataEngine( "network" )->sources();
    if ( interfaces.isEmpty() ) {
        return "unknown";
    }

    // Check if there is an activated interface or at least one that's
    // currently being configured
    foreach( const QString &iface, interfaces ) {
        QString sStatus = dataEngine( "network" )->query( iface )["ConnectionStatus"].toString();
        if ( sStatus.isEmpty() ) {
            return "unknown";
        }

        if ( sStatus == "Activated" ) {
            status = "activated";
            break;
        } else if ( sStatus == "Configuring" ) {
            status = "configuring";
        }
    }

    return status;
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

bool PublicTransport::isStateActive( const QString& stateName ) const
{
    return m_states.contains(stateName)
            && m_stateMachine->configuration().contains( m_states[stateName] );
}

void PublicTransport::noItemsTextClicked()
{
    // Update the timetable if an error message inside the tree view has been clicked
    if ( !isStateActive("networkActivated") ) {
        updateDataSource();
    }
}

void PublicTransport::setupActions()
{
    QAction *actionUpdate = new QAction( KIcon("view-refresh"),
                                         i18nc("@action:inmenu", "&Update timetable"), this );
    connect( actionUpdate, SIGNAL(triggered()), this, SLOT(updateDataSource()) );
    addAction( "updateTimetable", actionUpdate );

    QAction *showActionButtons = new QAction( /*KIcon("system-run"),*/ // TODO: better icon
            i18nc("@action", "&Quick Actions"), this );
    addAction( "showActionButtons", showActionButtons );

    QAction *actionSetAlarmForDeparture = new QAction(
            GlobalApplet::makeOverlayIcon( KIcon("task-reminder"), "list-add" ),
            m_settings.departureArrivalListType == DepartureList
            ? i18nc("@action:inmenu", "Set &Alarm for This Departure")
            : i18nc("@action:inmenu", "Set &Alarm for This Arrival"), this );
    connect( actionSetAlarmForDeparture, SIGNAL(triggered()),
             this, SLOT(setAlarmForDeparture()) );
    addAction( "setAlarmForDeparture", actionSetAlarmForDeparture );

    QAction *actionRemoveAlarmForDeparture = new QAction(
            GlobalApplet::makeOverlayIcon( KIcon("task-reminder"), "list-remove" ),
            m_settings.departureArrivalListType == DepartureList
            ? i18nc("@action:inmenu", "Remove &Alarm for This Departure")
            : i18nc("@action:inmenu", "Remove &Alarm for This Arrival"), this );
    connect( actionRemoveAlarmForDeparture, SIGNAL(triggered()),
             this, SLOT(removeAlarmForDeparture()) );
    addAction( "removeAlarmForDeparture", actionRemoveAlarmForDeparture );

    QAction *actionSearchJourneys = new QAction( KIcon("edit-find"),
            i18nc("@action", "Search for &Journeys..."), this );
    addAction( "searchJourneys", actionSearchJourneys );

    int iconExtend = 32;
    QAction *actionShowDepartures = new QAction(
            GlobalApplet::makeOverlayIcon( KIcon("public-transport-stop"),
                QList<KIcon>() << KIcon("go-home") << KIcon("go-next"),
                QSize(iconExtend / 2, iconExtend / 2), iconExtend ),
            i18nc("@action", "Show &Departures"), this );
    addAction( "showDepartures", actionShowDepartures );

    QAction *actionShowArrivals = new QAction(
            GlobalApplet::makeOverlayIcon( KIcon("public-transport-stop"),
                QList<KIcon>() << KIcon("go-next") << KIcon("go-home"),
                QSize(iconExtend / 2, iconExtend / 2), iconExtend ),
            i18nc("@action", "Show &Arrivals"), this );
    addAction( "showArrivals", actionShowArrivals );

    QAction *actionBackToDepartures = new QAction( KIcon("go-previous"),
            i18nc("@action", "Back to &Departure List"), this );
    addAction( "backToDepartures", actionBackToDepartures );

    m_filtersGroup = new QActionGroup( this );
    m_filtersGroup->setExclusive( false );
    connect( m_filtersGroup, SIGNAL(triggered(QAction*)),
             this, SLOT(switchFilterConfiguration(QAction*)) );

    KAction *actionFilterConfiguration = new KSelectAction( KIcon("view-filter"),
            i18nc("@action", "Filter"), this );
    KMenu *menu = new KMenu;
    menu->addTitle( KIcon("view-filter"), i18nc("@item:inmenu", "Used Filter Configurations") );
    actionFilterConfiguration->setMenu( menu );
    actionFilterConfiguration->setEnabled( true );
    addAction( "filterConfiguration", actionFilterConfiguration );

    QAction *actionToggleExpanded = new QAction( KIcon( "arrow-down" ),
            i18nc( "@action:inmenu", "&Show Additional Information" ), this );
    connect( actionToggleExpanded, SIGNAL(triggered()), this, SLOT(toggleExpanded()) );
    addAction( "toggleExpanded", actionToggleExpanded );

    // TODO: Combine actionHideColumnTarget and actionShowColumnTarget into one action?
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
    QStringList filterConfigurationList;// = m_settings.filterSettings.keys();
    foreach ( const FilterSettings &filterSettings, m_settings.filterSettingsList ) {
        filterConfigurationList << filterSettings.name;
    }
    if ( !filterConfigurationList.isEmpty() ) {
        actionFilter = qobject_cast< KAction* >( action( "filterConfiguration" ) );
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
    }

    QList< QAction* > actions;
    // Add actions: Update Timetable, Search Journeys
    actions << action( "updateTimetable" ); //<< action("showActionButtons")
    if ( m_currentServiceProviderFeatures.contains("JourneySearch")
        && !isStateActive("intermediateDepartureView") )
    {
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
    if ( isStateActive("intermediateDepartureView") ) {
        QAction *goBackAction = action("backToDepartures");
        goBackAction->setText( i18n("&Back To Original Stop") );
        actions << goBackAction;
    } else if ( m_settings.stopSettingsList.count() > 1 ) {
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
    if ( isStateActive("journeyView") ) {
        reconnectJourneySource();
    } else {
        reconnectSource();
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
    }
    if ( !_dateTime.isValid() ) {
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
        if ( isStateActive("journeyView") ) {
            m_titleWidget->setTitle( m_journeyTitleText );
        }
    }

    if ( !m_settings.currentStopSettings().get<QString>(CitySetting).isEmpty() ) {
        m_currentJourneySource += QString( "|city=%1" ).arg(
                m_settings.currentStopSettings().get<QString>(CitySetting) );
    }

    m_lastSecondStopName = _targetStopName;
    emit requestedNewJourneyData();
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

    emit requestedNewDepartureData();

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
}

void PublicTransport::departuresFiltered( const QString& sourceName,
        const QList< DepartureInfo > &departures,
        const QList< DepartureInfo > &newlyFiltered,
        const QList< DepartureInfo > &newlyNotFiltered )
{
    if ( m_departureInfos.contains(sourceName) ) {
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
    updateColorGroupSettings();
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
    m_urlJourneys = requestUrl;
    setAssociatedApplicationUrlForJourneys();

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
        const QDateTime &lastUpdate, int departuresToGo )
{
    // Set associated app url
    m_urlDeparturesArrivals = requestUrl;
    if ( isStateActive("departureView") || isStateActive("journeySearch") ||
         isStateActive("journeysUnsupportedView") )
    {
        setAssociatedApplicationUrlForDepartures();
    }

    // Put departures into the cache
    const QString strippedSourceName = stripDateAndTimeValues( sourceName );
    m_departureInfos[ strippedSourceName ] << departures;

    // Remove config needed messages
    setConfigurationRequired( false );

    // Update "last update" time
    if ( lastUpdate > m_lastSourceUpdate ) {
        m_lastSourceUpdate = lastUpdate;
    }
    m_labelInfo->setText( infoText() );

    // Fill the model with the received departures
    fillModel( departures );

    // Update color group settings when all departure data is there
    if ( departuresToGo == 0 ) {
        updateColorGroupSettings();
    }
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
        emit invalidJourneyDataReceived();

        // Set associated application url
        m_urlJourneys = data["requestUrl"].toUrl();
        kDebug() << "Errorneous journey url" << m_urlJourneys;
        if ( isStateActive("journeyView") ) {
            setAssociatedApplicationUrlForJourneys();
        }
    } else if ( data["parseMode"].toString() == "departures" ) {
        emit invalidDepartureDataReceived();

        // Set associated application url
        m_urlDeparturesArrivals = data["requestUrl"].toUrl();
        kDebug() << "Errorneous departure/arrival url" << m_urlDeparturesArrivals;
        if ( isStateActive("departureView") || isStateActive("journeySearch") ||
             isStateActive("journeysUnsupportedView") )
        {
            setAssociatedApplicationUrlForDepartures();
        }

        QString error = data["errorString"].toString();
        if ( error.isEmpty() ) {
            if ( isStateActive("networkActivated") ) {
                if ( m_settings.departureArrivalListType == DepartureList ) {
                    setConfigurationRequired( true, i18nc("@info", "Error parsing "
                            "departure information or currently no departures") );
                } else {
                    setConfigurationRequired( true, i18nc("@info", "Error parsing "
                            "arrival information or currently no arrivals") );
                }
            }
        } else if ( checkNetworkStatus() ) {
            m_timetable->setNoItemsText( i18nc("@info/plain",
                    "There was an error:<nl/><message>%1</message><nl/><nl/>"
                    "The server may be temporarily unavailable.", error) );
        }
    }
}

void PublicTransport::processStopSuggestions( const QString &/*sourceName*/,
        const Plasma::DataEngine::Data &data )
{
    bool journeyData = data["parseMode"].toString() == "journeys";
    if ( journeyData || data["parseMode"].toString() == "stopSuggestions" ) {
        if ( journeyData ) {
            emit invalidJourneyDataReceived();
        }

        m_listStopSuggestions->updateStopSuggestionItems( data );
    } else if ( data["parseMode"].toString() == "departures" /*&& m_currentMessage == MessageNone TODO*/ ) {
        emit invalidDepartureDataReceived();
        clearDepartures();
        setConfigurationRequired( true, i18nc("@info", "The stop name is ambiguous.") );
    }
}

void PublicTransport::dataUpdated( const QString& sourceName,
                                   const Plasma::DataEngine::Data& data )
{
    if ( data.isEmpty() || (!m_currentSources.contains(sourceName)
                            && sourceName != m_currentJourneySource) ) {
        // Source isn't used anymore
        kDebug() << "Data discarded" << sourceName;
        return;
    }

    if ( data["error"].toBool() ) {
        // Error while parsing the data or no connection to server
        handleDataError( sourceName, data );
    } else if ( data["receivedPossibleStopList"].toBool() ) {
        // Stop suggestion list received
        processStopSuggestions( sourceName, data );
    } else if ( data["parseMode"].toString() == "journeys" ) {
        // List of journeys received
        emit validJourneyDataReceived();
        if ( isStateActive("journeyView") ) {
            m_departureProcessor->processJourneys( sourceName, data );
        } else {
            kDebug() << "Received journey data, but journey list is hidden.";
        }
    } else if ( data["parseMode"].toString() == "departures" ) {
        // List of departures / arrivals received
        emit validDepartureDataReceived();
        m_departureProcessor->processDepartures( sourceName, data );
    }
}

void PublicTransport::geometryChanged()
{
    m_labelInfo->setText( infoText() );
}

void PublicTransport::popupEvent( bool show )
{
    emit goBackToDepartureList();
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

void PublicTransport::createPopupIcon()
{
    if ( m_model->isEmpty() || m_departureGroups.isEmpty() ) {
        setPopupIcon( "public-transport-stop" );
    } else {
        QPixmap pixmap = m_departurePainter->createPopupIcon(
                m_startPopupIconDepartureIndex, m_endPopupIconDepartureIndex,
                m_popupIconDepartureIndex, m_model, m_departureGroups );

        KIcon icon;
        icon.addPixmap( pixmap );
        setPopupIcon( icon );
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

void PublicTransport::configChanged()
{
    disconnect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );

    // Apply show departures/arrivals setting
    m_model->setDepartureArrivalListType( m_settings.departureArrivalListType );

    // Apply header settings
    if ( m_stateMachine && isStateActive("departureView") )
    {
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
    if ( m_journeyTimetable && isStateActive("journeyView") ) {
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
    // TODO this is a copy of code in line ~2311
    if ( !m_stateMachine || isStateActive("departureDataWaiting") ) {
        m_timetable->setNoItemsText(
                i18nc("@info/plain", "Waiting for data...") );
    } else if ( m_settings.departureArrivalListType == ArrivalList ) {
        m_timetable->setNoItemsText( !m_settings.currentFilterSettings().isEmpty()
                ? i18nc("@info/plain", "No unfiltered arrivals.<nl/>You can "
                        "disable filters to see all arrivals.")
                : i18nc("@info/plain", "No arrivals.") );
    } else {
        m_timetable->setNoItemsText( !m_settings.currentFilterSettings().isEmpty()
                ? i18nc("@info/plain", "No unfiltered departures.<nl/>You can "
                        "disable filters to see all departures.")
                : i18nc("@info/plain", "No departures.") );
    }

    // Apply filter, first departure and alarm settings to the worker thread
    m_departureProcessor->setFilterSettings( m_settings.currentFilterSettings() );
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
    if ( m_settings.checkConfig() ) {
        // Only use the default target state (journey search) if journeys
        // are supported by the used service provider. Otherwise go to the
        // alternative target state (journeys not supported).
        QAbstractState *target = m_currentServiceProviderFeatures.contains("JourneySearch")
                ? m_states["journeySearch"] : m_states["journeysUnsupportedView"];
        m_journeySearchTransition1->setTargetState( target );
        m_journeySearchTransition2->setTargetState( target );
        m_journeySearchTransition3->setTargetState( target );

        setConfigurationRequired( false );
        reconnectSource();

        if ( !m_currentJourneySource.isEmpty() ) {
            reconnectJourneySource();
        }
    } else {
        setConfigurationRequired( true, i18nc("@info/plain", "Please check your configuration.") );
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
            connect( action, SIGNAL(triggered()), this, SIGNAL(goBackToDepartureList()) );
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
    // Create an overlay widget with gets placed over the applet
    // and then filled with buttons
    m_overlay = new OverlayWidget( m_graphicsWidget, m_mainGraphicsWidget );
    m_overlay->setGeometry( m_graphicsWidget->contentsRect() );
    m_overlay->setOpacity( 0 );

    // Create a layout for the buttons and add a spacer at the top
    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout( Qt::Vertical );
    layout->setContentsMargins( 15, 10, 15, 10 );
    QGraphicsWidget *spacer = new QGraphicsWidget( m_overlay );
    spacer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    layout->addItem( spacer );

    // Add actions depending on active states / service provider features
    QList<QAction*> actions;
    if ( m_currentServiceProviderFeatures.contains("JourneySearch") ) {
        actions << action("searchJourneys");
    }
    if ( isStateActive("journeyView") ) {
        actions << action("backToDepartures");
    }
    if ( m_currentServiceProviderFeatures.contains("Arrivals") ) {
        actions << (m_settings.departureArrivalListType == DepartureList
                ? action("showArrivals") : action("showDepartures"));
    }

    // Add stop selector if multiple stops are defined
    if ( m_settings.stopSettingsList.count() > 1 ) {
        actions << switchStopAction( NULL, true ); // Parent gets set below
    }

    // Create buttons for the actions and create a list of fade animations
    foreach ( QAction *action, actions ) {
        Plasma::PushButton *button = new Plasma::PushButton( m_overlay );
        button->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
        button->setAction( action );
        if ( action->menu() ) {
            action->setParent( button ); // Set button as parent
            button->nativeWidget()->setMenu( action->menu() );
        }

        layout->addItem( button );
        layout->setAlignment( button, Qt::AlignCenter );
    }

    // Add a cancel button
    Plasma::PushButton *btnCancel = new Plasma::PushButton( m_overlay );
    btnCancel->setText( i18nc("@action:button", "Cancel") );
    btnCancel->setIcon( KIcon("dialog-cancel") );
    btnCancel->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
    connect( btnCancel, SIGNAL(clicked()), this, SIGNAL(cancelActionButtons()) );

    // Create a separate layout for the cancel button to have some more space
    // between the cancel button and the others
    QGraphicsLinearLayout *layoutCancel = new QGraphicsLinearLayout( Qt::Vertical );
    layoutCancel->setContentsMargins( 0, 10, 0, 0 );
    layoutCancel->addItem( btnCancel );

    // Finish layout with the cancel button and another spacer at the bottom
    layout->addItem( layoutCancel );
    layout->setAlignment( layoutCancel, Qt::AlignCenter );
    QGraphicsWidget *spacer2 = new QGraphicsWidget( m_overlay );
    spacer2->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    layout->addItem( spacer2 );
    m_overlay->setLayout( layout );

    // Create a fade in animation for the whole overlay
    GlobalApplet::fadeAnimation( m_overlay, 1 )->start( QAbstractAnimation::DeleteWhenStopped );
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

void PublicTransport::showDepartures()
{
    // Change departure arrival list type in a copy of the settings.
    // Then write the new settings.
    Settings settings = m_settings;
    settings.departureArrivalListType = DepartureList;
    writeSettings( settings );
}

void PublicTransport::showArrivals()
{
    // Change departure arrival list type in a copy of the settings.
    // Then write the new settings.
    Settings settings = m_settings;
    settings.departureArrivalListType = ArrivalList;
    writeSettings( settings );
}

void PublicTransport::switchFilterConfiguration( QAction* action )
{
//     switchFilterConfiguration( KGlobal::locale()->removeAcceleratorMarker(action->text()) );
    const QString filterConfig = GlobalApplet::untranslateFilterKey(
            KGlobal::locale()->removeAcceleratorMarker(action->text()) );

    // Change filter configuration of the current stop in a copy of the settings.
    // Then write the new settings.
    Settings settings = m_settings;
    for ( int i = 0; i < settings.filterSettingsList.count(); ++i ) {
        const FilterSettings filterSettings = settings.filterSettingsList[i];
        if ( filterSettings.name == filterConfig ) {
            // Switch filter configuration for current stop settings
            if ( filterSettings.affectedStops.contains(settings.currentStopSettingsIndex) ) {
                settings.filterSettingsList[i].affectedStops.remove( settings.currentStopSettingsIndex );
            } else if ( !filterSettings.affectedStops.contains(settings.currentStopSettingsIndex) ) {
                settings.filterSettingsList[i].affectedStops << settings.currentStopSettingsIndex;
            }
        }
    }
    writeSettings( settings );
}

void PublicTransport::enableFilterConfiguration( const QString& filterConfiguration, bool enable )
{
    const QString filterConfig = GlobalApplet::untranslateFilterKey( filterConfiguration );
    Q_ASSERT_X( m_settings.filterSettingsList.hasName(filterConfig),
                "PublicTransport::switchFilterConfiguration",
                QString("Filter '%1' not found!").arg(filterConfig).toLatin1().data() );

    // Change filter configuration of the current stop in a copy of the settings.
    // Then write the new settings.
    Settings settings = m_settings;
    FilterSettings filterSettings = settings.filterSettingsList.byName( filterConfig );
    if ( enable && !filterSettings.affectedStops.contains(settings.currentStopSettingsIndex) ) {
        filterSettings.affectedStops << settings.currentStopSettingsIndex;
    } else if ( !enable && filterSettings.affectedStops.contains(settings.currentStopSettingsIndex) ) {
        filterSettings.affectedStops.remove( settings.currentStopSettingsIndex );
    }
    settings.filterSettingsList.set( filterSettings );
    writeSettings( settings );
}

void PublicTransport::showDepartureList()
{
    fadeOutOldAppearance();
    m_titleWidget->setTitleType( ShowDepartureArrivalListTitle,
                                 isStateActive("departureDataValid"),
                                 isStateActive("journeyDataValid") );
    updateDepartureListIcon();
    updateInfoText();

    m_timetable->update();
    geometryChanged();
    setBusy( isStateActive("departureDataWaiting") && m_model->isEmpty() );

    showMainWidget( m_timetable );
    setAssociatedApplicationUrlForDepartures();
}

void PublicTransport::showIntermediateDepartureList()
{
    fadeOutOldAppearance();
    m_titleWidget->setTitleType( ShowIntermediateDepartureListTitle,
                                 isStateActive("departureDataValid"),
                                 isStateActive("journeyDataValid") );
    updateDepartureListIcon();
    updateInfoText();

    m_timetable->update();
    geometryChanged();
    setBusy( isStateActive("departureDataWaiting") && m_model->isEmpty() );

    showMainWidget( m_timetable );
    setAssociatedApplicationUrlForDepartures();
}

void PublicTransport::showJourneyList()
{
    fadeOutOldAppearance();
    m_titleWidget->setTitleType( ShowJourneyListTitle,
                                 isStateActive("departureDataValid"),
                                 isStateActive("journeyDataValid") );

    m_journeyTimetable = new JourneyTimetableWidget( this );
    m_journeyTimetable->setModel( m_modelJourneys );
    m_journeyTimetable->setFont( m_settings.sizedFont() );
    m_journeyTimetable->setSvg( &m_vehiclesSvg );
    connect( m_journeyTimetable, SIGNAL(requestJourneys(QString,QString)),
             this, SLOT(processJourneyRequest(QString,QString)) );
    connect( m_states["journeyView"], SIGNAL(exited()),
             m_journeyTimetable, SLOT(deleteLater()) );
    m_journeyTimetable->setZoomFactor( m_settings.sizeFactor );
    m_journeyTimetable->update();

    m_titleWidget->setTitle( m_journeyTitleText.isEmpty()
            ? i18nc("@info", "<emphasis strong='1'>Journeys</emphasis>")
            : m_journeyTitleText );
    geometryChanged();
    setBusy( isStateActive("journeyDataWaiting") && m_modelJourneys->isEmpty() );

    showMainWidget( m_journeyTimetable );
    setAssociatedApplicationUrlForJourneys();
}

void PublicTransport::showJourneySearch()
{
    fadeOutOldAppearance();
    m_titleWidget->setTitleType( ShowSearchJourneyLineEdit,
                                 isStateActive("departureDataValid"),
                                 isStateActive("journeyDataValid") );

    Plasma::LineEdit *journeySearch =
            m_titleWidget->castedWidget<Plasma::LineEdit>( TitleWidget::WidgetJourneySearchLine );
    Q_ASSERT( journeySearch );

    m_listStopSuggestions = new JourneySearchSuggestionWidget(
            this, &m_settings, palette() );
    m_listStopSuggestions->attachLineEdit( journeySearch );
    connect( m_listStopSuggestions, SIGNAL(journeySearchLineChanged(QString,QDateTime,bool,bool)),
             this, SLOT(journeySearchLineChanged(QString,QDateTime,bool,bool)) );
    connect( m_states["journeySearch"], SIGNAL(exited()),
             m_listStopSuggestions, SLOT(deleteLater()) );

    showMainWidget( m_listStopSuggestions );
    setBusy( false );
}

void PublicTransport::showJourneysUnsupportedView()
{
    fadeOutOldAppearance();
    m_titleWidget->setTitleType( ShowSearchJourneyLineEditDisabled,
                                 isStateActive("departureDataValid"),
                                 isStateActive("journeyDataValid") );

    m_labelJourneysNotSupported = new Plasma::Label;
    m_labelJourneysNotSupported->setAlignment( Qt::AlignCenter );
    m_labelJourneysNotSupported->setSizePolicy( QSizePolicy::Expanding,
            QSizePolicy::Expanding, QSizePolicy::Label );
    m_labelJourneysNotSupported->setText( i18nc("@info/plain",
            "Journey searches aren't supported by the currently used "
            "service provider or it's accessor.") );
    m_labelJourneysNotSupported->nativeWidget()->setWordWrap( true );
    connect( m_states["journeysUnsupportedView"], SIGNAL(exited()),
             m_labelJourneysNotSupported, SLOT(deleteLater()) );

    showMainWidget( m_labelJourneysNotSupported );
    setBusy( false );
}

void PublicTransport::journeySearchInputFinished()
{
    clearJourneys();
    Plasma::LineEdit *journeySearch =
            m_titleWidget->castedWidget<Plasma::LineEdit>( TitleWidget::WidgetJourneySearchLine );
    Q_ASSERT( journeySearch );

    // Add journey search line to the list of recently used journey searches
    // and cut recent journey searches if the limit is exceeded
    Settings settings = m_settings;
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
    bool stopIsTarget, timeIsDeparture;
    JourneySearchParser::parseJourneySearch( journeySearch->nativeWidget(),
            journeySearch->text(), &stop, &departure,
            &stopIsTarget, &timeIsDeparture );

    reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture );
    emit journeySearchFinished();
}

void PublicTransport::journeySearchLineChanged( const QString& stopName,
        const QDateTime& departure, bool stopIsTarget, bool timeIsDeparture )
{
    reconnectJourneySource( stopName, departure, stopIsTarget, timeIsDeparture, true );
}

// TODO: Move to TitleWidget?
void PublicTransport::showFilterMenu()
{
    // TODO make a new function for this (updateFilterMenu())
    // because of redundancy in PublicTransport::contextualActions().
    KAction *actionFilter = NULL;
    if ( !m_settings.filterSettingsList.isEmpty() ) {
        actionFilter = qobject_cast< KAction* >( action("filterConfiguration") );
        QList< QAction* > oldActions = m_filtersGroup->actions();
        foreach( QAction *oldAction, oldActions ) {
            m_filtersGroup->removeAction( oldAction );
            delete oldAction;
        }

        QMenu *menu = actionFilter->menu();
        QString currentFilterConfig = m_settings.currentStopSettings().get<QString>(
                FilterConfigurationSetting );
        foreach( const FilterSettings &filterSettings, m_settings.filterSettingsList ) {
            QAction *action = new QAction(
                    GlobalApplet::translateFilterKey(filterSettings.name), m_filtersGroup );
            action->setCheckable( true );
            menu->addAction( action );
            if ( filterSettings.affectedStops.contains(m_settings.currentStopSettingsIndex) ) {
                action->setChecked( true );
            }
        }
    }

    // Show the filters menu under the filter icon
    actionFilter->menu()->exec( QCursor::pos() );
//   view()->mapToGlobal(
// 		view()->mapFromScene(m_filterIcon->mapToScene(0,
// 				m_filterIcon->boundingRect().height()))) );
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
        m_titleWidget = new TitleWidget( ShowDepartureArrivalListTitle,
                                         &m_settings, m_mainGraphicsWidget );
        connect( m_titleWidget, SIGNAL(recentJourneyActionTriggered(TitleWidget::RecentJourneyAction)),
                 this, SLOT(recentJourneyActionTriggered(TitleWidget::RecentJourneyAction)) );
        connect( m_titleWidget, SIGNAL(journeySearchInputFinished()),
                 this, SLOT(journeySearchInputFinished()) );
        connect( m_titleWidget, SIGNAL(filterIconClicked()), this, SLOT(showFilterMenu()) );

        m_labelInfo = new Plasma::Label( m_mainGraphicsWidget );
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
        connect( m_timetable, SIGNAL(requestStopAction(StopAction,QString,RouteStopTextGraphicsItem*)),
                 this, SLOT(requestStopAction(StopAction,QString,RouteStopTextGraphicsItem*)) );

        QGraphicsLinearLayout *layout = new QGraphicsLinearLayout( Qt::Vertical );
        layout->setContentsMargins( 0, 0, 0, 0 );
        layout->setSpacing( 0 );
        layout->addItem( m_titleWidget );
        layout->addItem( m_timetable );
        layout->addItem( m_labelInfo );
        layout->setAlignment( m_labelInfo, Qt::AlignRight | Qt::AlignVCenter );
        m_mainGraphicsWidget->setLayout( layout );

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
    if ( watched && watched == journeySearch && isStateActive("journeySearch")
        && m_listStopSuggestions->model()
        && m_listStopSuggestions->model()->rowCount() > 0 )
    {
        QKeyEvent *keyEvent;
        QModelIndex curIndex;
        int row;
        switch ( event->type() ) {
        case QEvent::KeyPress:
            keyEvent = dynamic_cast<QKeyEvent*>( event );
            curIndex = m_listStopSuggestions->currentIndex();

            if ( keyEvent->key() == Qt::Key_Up ) {
                if ( !curIndex.isValid() ) {
                    curIndex = m_listStopSuggestions->model()->index( 0, 0 );
                    m_listStopSuggestions->setCurrentIndex( curIndex );
// 					possibleStopItemActivated( curIndex );
                    m_listStopSuggestions->useStopSuggestion( curIndex );
                    return true;
                } else {
                    row = curIndex.row();
                    if ( row >= 1 ) {
                        m_listStopSuggestions->setCurrentIndex(
                                m_listStopSuggestions->model()->index(row - 1,
                                curIndex.column(), curIndex.parent()) );
                        m_listStopSuggestions->useStopSuggestion(
                                m_listStopSuggestions->currentIndex() );
                        return true;
                    } else {
                        return false;
                    }
                }
            } else if ( keyEvent->key() == Qt::Key_Down ) {
                if ( !curIndex.isValid() ) {
                    curIndex = m_listStopSuggestions->model()->index( 0, 0 );
                    m_listStopSuggestions->setCurrentIndex( curIndex );
                    m_listStopSuggestions->useStopSuggestion( curIndex );
                    return true;
                } else {
                    row = curIndex.row();
                    if ( row < m_listStopSuggestions->model()->rowCount() - 1 ) {
                        m_listStopSuggestions->setCurrentIndex(
                                m_listStopSuggestions->model()->index(row + 1,
                                curIndex.column(), curIndex.parent()) );
                        m_listStopSuggestions->useStopSuggestion(
                                m_listStopSuggestions->currentIndex() );
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
    // Go back from intermediate departure list (which may be requested by a
    // context menu action) before showing the configuration dialog,
    // because stop settings may be changed and the intermediate stop
    // shouldn't be shown there.
    if ( isStateActive("intermediateDepartureView") ) {
        showDepartureList();
    }

    SettingsUiManager *settingsUiManager = new SettingsUiManager(
            m_settings, dataEngine("publictransport"),
            dataEngine("openstreetmap"), dataEngine("favicons"),
            dataEngine("geolocation"), parent );
    connect( settingsUiManager, SIGNAL(settingsAccepted(Settings)),
             this, SLOT(writeSettings(Settings)) );
    connect( m_model, SIGNAL(updateAlarms(AlarmSettingsList,QList<int>)),
             settingsUiManager, SLOT(removeAlarms(AlarmSettingsList,QList<int>)) );
}

void PublicTransport::writeSettings( const Settings& settings )
{
    SettingsIO::ChangedFlags changed =
            SettingsIO::writeSettings( settings, m_settings, config(), globalConfig() );

    if ( changed.testFlag(SettingsIO::IsChanged) ) {
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

            // Adjust action texts to departure / arrival list
            action("removeAlarmForDeparture")->setText(
                    m_settings.departureArrivalListType == DepartureList
                    ? i18nc("@action", "Remove &Alarm for This Departure")
                    : i18nc("@action", "Remove &Alarm for This Arrival") );
            action("setAlarmForDeparture")->setText(
                    m_settings.departureArrivalListType == DepartureList
                    ? i18nc("@action", "Set &Alarm for This Departure")
                    : i18nc("@action", "Set &Alarm for This Arrival") );
            action("backToDepartures")->setText(
                    m_settings.departureArrivalListType == DepartureList
                    ? i18nc("@action", "Back to &Departure List")
                    : i18nc("@action", "Back to &Arrival List") );
        }

        // If stop settings have changed the whole model gets cleared and refilled.
        // Therefore the other change flags can be in 'else' parts
        if ( changed.testFlag(SettingsIO::ChangedStopSettings) ||
             changed.testFlag(SettingsIO::ChangedCurrentStop) ||
             changed.testFlag(SettingsIO::ChangedServiceProvider) )
        {
            adjustColorGroupSettingsCount();
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
             changed.testFlag(SettingsIO::ChangedFilterSettings) )
        {
            m_titleWidget->updateFilterWidget();
        }

        // (De)Colorize if colorization setting has been toggled
        if ( changed.testFlag(SettingsIO::ChangedColorization) ||
             changed.testFlag(SettingsIO::ChangedCurrentStop) ||
             changed.testFlag(SettingsIO::ChangedStopSettings) )
        {
            updateColorGroupSettings();
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

void PublicTransport::updateInfoText()
{
    m_labelInfo->setToolTip( courtesyToolTip() );
    m_labelInfo->setText( infoText() );
}

void PublicTransport::showMainWidget( QGraphicsWidget* mainWidget )
{
    // Setup new main layout
    QGraphicsLinearLayout *layoutMainNew = new QGraphicsLinearLayout(
            Qt::Vertical, m_mainGraphicsWidget );
    layoutMainNew->setContentsMargins( 0, 0, 0, 0 );
    layoutMainNew->setSpacing( 0 );
    m_timetable->setVisible( isStateActive("departureView") ||
                             isStateActive("intermediateDepartureView") );

    // Add widgets to new layout
    layoutMainNew->addItem( m_titleWidget );
    layoutMainNew->addItem( mainWidget );
    layoutMainNew->addItem( m_labelInfo );
    layoutMainNew->setAlignment( m_labelInfo, Qt::AlignRight | Qt::AlignVCenter );
}

Plasma::Animation* PublicTransport::fadeOutOldAppearance()
{
    if ( isVisible() && m_stateMachine ) {
        // Draw old appearance to pixmap
        QPixmap pixmap( m_mainGraphicsWidget->size().toSize() );
        pixmap.fill( Qt::transparent );
        QPainter p( &pixmap );
        QRect sourceRect = m_mainGraphicsWidget->mapToScene( m_mainGraphicsWidget->boundingRect() )
                .boundingRect().toRect();
        QRectF rect( QPointF(0, 0), m_mainGraphicsWidget->size() );
        m_titleWidget->scene()->render( &p, rect, sourceRect );

        // Fade from old to new appearance
        oldItemAnimationFinished();
        m_oldItem = new GraphicsPixmapWidget( pixmap, m_graphicsWidget );
        m_oldItem->setPos( 0, 0 );
        m_oldItem->setZValue( 1000 );
        Plasma::Animation *animOut = Plasma::Animator::create( Plasma::Animator::FadeAnimation );
        animOut->setProperty( "startOpacity", 1 );
        animOut->setProperty( "targetOpacity", 0 );
        animOut->setTargetWidget( m_oldItem );
        connect( animOut, SIGNAL(finished()), this, SLOT(oldItemAnimationFinished()) );
        animOut->start( QAbstractAnimation::DeleteWhenStopped );
        return animOut;
    } else {
        return NULL;
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

void PublicTransport::processJourneyRequest( const QString& startStop, const QString& targetStop )
{
    Q_UNUSED( startStop );
    clearJourneys();
    reconnectJourneySource( targetStop, QDateTime(), true, true );
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

void PublicTransport::departureDataWaitingStateEntered()
{
    updateDepartureListIcon();
    setBusy( m_model->isEmpty() ); // Only busy if there is no data currently
    m_timetable->setNoItemsText( i18nc("@info/plain", "Waiting for depatures...") );
}

void PublicTransport::departureDataInvalidStateEntered()
{
    updateDepartureListIcon();
    setBusy( false );

    m_timetable->setNoItemsText( m_settings.departureArrivalListType == ArrivalList
            ? i18nc("@info/plain", "No arrivals due to an error.")
            : i18nc("@info/plain", "No departures due to an error.") );
}

void PublicTransport::departureDataValidStateEntered()
{
    updateDepartureListIcon();
    setBusy( false );

    // TODO This is a copy of code in line ~1520
    if ( m_settings.departureArrivalListType == ArrivalList ) {
        m_timetable->setNoItemsText( !m_settings.currentFilterSettings().isEmpty()
                ? i18nc("@info/plain", "No unfiltered arrivals.<nl/>"
                        "You can disable filters to see all arrivals.")
                : i18nc("@info/plain", "No arrivals.") );
    } else {
        m_timetable->setNoItemsText( !m_settings.currentFilterSettings().isEmpty()
                ? i18nc("@info/plain", "No unfiltered departures.<nl/>"
                        "You can disable filters to see all departures.")
                : i18nc("@info/plain", "No departures.") );
    }
}

void PublicTransport::journeyDataWaitingStateEntered()
{
    if ( isStateActive("journeyView") ) {
        m_titleWidget->setIcon( JourneyListErrorIcon );
        m_journeyTimetable->setNoItemsText( i18nc("@info/plain", "Waiting for journeys...") );
        setBusy( m_modelJourneys->isEmpty() );
    }
}

void PublicTransport::journeyDataInvalidStateEntered()
{
    if ( isStateActive("journeyView") ) {
        m_titleWidget->setIcon( JourneyListErrorIcon );
        m_journeyTimetable->setNoItemsText( i18nc("@info/plain", "No journeys due to an error.") );
        setBusy( false );
    }
}

void PublicTransport::journeyDataValidStateEntered()
{
    if ( isStateActive("journeyView") ) {
        m_titleWidget->setIcon( JourneyListOkIcon );
        m_journeyTimetable->setNoItemsText( i18nc("@info/plain", "No journeys.") );
        setBusy( false );
    }
}

void PublicTransport::removeIntermediateStopSettings()
{
    // Remove intermediate stop settings
    Settings settings = m_settings;
    settings.stopSettingsList.removeIntermediateSettings();
    settings.currentStopSettingsIndex = qBound( 0, m_originalStopIndex,
            settings.stopSettingsList.count() - 1 );
    writeSettings( settings );
}

void PublicTransport::updateDepartureListIcon()
{
    if ( isStateActive("intermediateDepartureView") ) {
        m_titleWidget->setIcon( GoBackIcon );
    } else {
        m_titleWidget->setIcon( isStateActive("departureDataValid")
                ? DepartureListOkIcon : DepartureListErrorIcon );
    }
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
    if ( m_journeyTimetable && isStateActive("journeyView") ) {
        m_journeyTimetable->item( m_clickedItemIndex.row() )->toggleExpanded();
    } else {
        m_timetable->item( m_clickedItemIndex.row() )->toggleExpanded();
    }
}

QAction* PublicTransport::updatedAction( const QString& actionName )
{
    QAction *a = action( actionName );
    if ( !a ) {
        kDebug() << "Action not found:" << actionName;
        return NULL;
    }

    if ( actionName == "toggleExpanded" ) {
        if ( (m_journeyTimetable && isStateActive("journeyView"))
            ? m_journeyTimetable->item(m_clickedItemIndex.row())->isExpanded()
            : m_timetable->item(m_clickedItemIndex.row())->isExpanded() )
        {
            a->setText( i18nc("@action", "Hide Additional &Information") );
            a->setIcon( KIcon("arrow-up") );
        } else {
            a->setText( i18nc("@action", "Show Additional &Information") );
            a->setIcon( KIcon("arrow-down") );
        }
    }

    return a;
}

void PublicTransport::departureContextMenuRequested( PublicTransportGraphicsItem* item, const QPointF& pos )
{
    Q_UNUSED( pos );

    // Save the index of the clicked item
    m_clickedItemIndex = item->index();

    // List actions for the context menu
    QAction *infoAction = NULL;
    QList<QAction *> actions;
    actions.append( updatedAction("toggleExpanded") );
    if ( isStateActive("departureView") ) {
        DepartureItem *item = static_cast<DepartureItem*>( m_model->item(m_clickedItemIndex.row()) );
        if ( item->hasAlarm() ) {
            if ( item->alarmStates().testFlag(AlarmIsAutoGenerated) ) {
                actions.append( action("removeAlarmForDeparture") );
            } else if ( item->alarmStates().testFlag(AlarmIsRecurring) ) {
                // The 'Remove this Alarm' menu entry can only be
                // used with autogenerated alarms
                if ( item->departureInfo()->matchedAlarms().count() == 1 ) {
                    infoAction = new QAction( KIcon("task-recurring"),
                            i18nc("@info/plain", "(has a recurring alarm)"), this );
                } else {
                    infoAction = new QAction( i18nc("@info/plain", "(has multiple alarms)"), this );
                }
            } else {
                // The 'Remove this Alarm' menu entry can only be
                // used with autogenerated alarms
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
        } else {
            actions.append( action("setAlarmForDeparture") );
        }
    }

    // Show the menu if it's not empty
    if ( actions.count() > 0 && view() ) {
        QMenu::exec( actions, QCursor::pos() );
    }

    delete infoAction;
}

void PublicTransport::requestStopAction( StopAction stopAction,
        const QString& stopName, RouteStopTextGraphicsItem* item )
{
    Q_UNUSED( item );

    // Create and enable new filter
    Settings settings = m_settings;

    switch ( stopAction ) {
        case CreateFilterForStop: {
            QString filterName = i18nc("Default name for a new filter via a given stop",
                                       "Via %1", stopName);
            Filter viaFilter;
            viaFilter << Constraint( FilterByVia, FilterContains, stopName );
            FilterSettings filterSettings;
            filterSettings.filters << viaFilter;
            filterSettings.name = filterName;
            filterSettings.affectedStops << settings.currentStopSettingsIndex;

            settings.filterSettingsList << filterSettings;
            writeSettings( settings );
            break;
        } case ShowDeparturesForStop: {
            // Save original stop index from where sub requests were made
            // (using the context menu). Only if the departure list wasn't requested
            // already from a sub departure list.
            if ( !isStateActive("intermediateDepartureViewState") ) {
                m_originalStopIndex = m_settings.currentStopSettingsIndex;
            }

            // Search for a stop setting with the given stop name in it.
            // Create an intermediate stop item if there is no such stop setting
            // in the configuration (automatically deleted).
            int stopSettingsIndex = settings.stopSettingsList.findStopSettings( stopName );
            if ( stopSettingsIndex == -1 ) {
                StopSettings stopSettings( settings.currentStopSettings() );
                stopSettings.setStop( stopName );
                stopSettings.set( UserSetting + 100, "-- Intermediate Stop --" );
                settings.stopSettingsList << stopSettings;
                stopSettingsIndex = settings.stopSettingsList.count() - 1;
            }
            settings.currentStopSettingsIndex = stopSettingsIndex;
            writeSettings( settings );

            emit intermediateDepartureListRequested( stopName );
            break;
        } case HighlightStop: {
            m_model->setHighlightedStop(
                    m_model->highlightedStop().compare(stopName, Qt::CaseInsensitive) == 0
                    ? QString() : stopName );
            break;
        } case CopyStopNameToClipboard: {
            QApplication::clipboard()->setText( stopName );
            break;
        }
    }
}

void PublicTransport::removeAlarmForDeparture( int row )
{
    DepartureItem *item = static_cast<DepartureItem*>( m_model->item(row) );
    Q_ASSERT_X( item->alarmStates().testFlag(AlarmIsAutoGenerated),
                "PublicTransport::removeAlarmForDeparture",
                "Only auto generated alarms can be removed automatically" );

    // Find a matching autogenerated alarm
    int matchingAlarmSettings = -1;
    for ( int i = 0; i < m_settings.alarmSettings.count(); ++i ) {
        AlarmSettings alarmSettings = m_settings.alarmSettings[ i ];
        if ( alarmSettings.autoGenerated && alarmSettings.enabled
                    && alarmSettings.filter.match(*item->departureInfo()) ) {
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

    DepartureItem *item = static_cast<DepartureItem*>( m_model->itemFromIndex(modelIndex) );
    DepartureInfo info = *item->departureInfo();
    QString departureTime = KGlobal::locale()->formatTime( info.departure().time() );

    // Autogenerate an alarm that only matches the given departure
    AlarmSettings alarm;
    alarm.name = i18nc( "@info/plain Name of new automatically generated alarm filters. "
                        "%1 is the departure time, %2 is the target.",
                        "At %1 to %2", departureTime, info.target() );
    alarm.autoGenerated = true;
    alarm.affectedStops << m_settings.currentStopSettingsIndex;
    alarm.filter.append( Constraint(FilterByDeparture, FilterEquals, info.departure()) );
    alarm.filter.append( Constraint(FilterByTransportLine, FilterEquals, info.lineString()) );
    alarm.filter.append( Constraint(FilterByVehicleType, FilterIsOneOf,
                                    QVariantList() << info.vehicleType()) );
    alarm.filter.append( Constraint(FilterByTarget, FilterEquals, info.target()) );

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
                              minsToDeparture, sLine, sTarget, predictedDeparture.toString("hh:mm") );
        } else {
            // Vehicle type is known
            message = i18ncp( "@info/plain %2: Line string (e.g. 'U3'), %4: Vehicle "
                              "type name (e.g. tram, subway)",
                              "The %4 %2 to '%3' departs in %1 minute at %5",
                              "The %4 %2 to '%3' departs in %1 minutes at %5",
                              minsToDeparture, sLine, sTarget,
                              Global::vehicleTypeToString(departureInfo->vehicleType()),
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
                              Global::vehicleTypeToString(departureInfo->vehicleType()),
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
                              Global::vehicleTypeToString(departureInfo->vehicleType()),
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
        sLastUpdate = i18nc( "@info/plain This is used as 'last data update' "
                             "text when there hasn't been any updates yet.", "none" );
    }

    // HACK: This breaks the text at one position if needed
    // Plasma::Label doesn't work well will HTML formatted text and word wrap:
    // It sets the height as if the label shows the HTML source.
    QString textNoHtml1 = QString( "%1: %2" ).arg( i18nc("@info/plain", "last update"), sLastUpdate );
    QString textNoHtml2 = QString( "%1: %2" ).arg( i18nc("@info/plain", "data by"), shortUrl );
    QFontMetrics fm( m_labelInfo->font() );
    int width1 = fm.width( textNoHtml1 );
    int width2 = fm.width( textNoHtml2 );
    int width = width1 + fm.width( ", " ) + width2;
    if ( width > m_graphicsWidget->size().width() ) {
        m_graphicsWidget->setMinimumWidth( /*qMax(150,*/ qMax(width1, width2)/*)*/ );
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
                // Departure doesn't get filtered out and the model isn't full => Add departure
                m_model->addItem( departureInfo );
                modelFilled = m_model->rowCount() >= m_settings.maximalNumberOfDepartures;
            }
        } else if ( departureInfo.isFilteredOut() ) {
            // Departure has been marked as "filtered out" in the DepartureProcessor => Remove departure
            m_model->removeItem( m_model->itemFromInfo(departureInfo) );
        } else {
            // Departure isn't filtered out => Update associated item in the model
            DepartureItem *item = dynamic_cast<DepartureItem*>( m_model->itemFromIndex(index) );
            m_model->updateItem( item, departureInfo );
        }
    }

    // Update everything that might have changed
    createDepartureGroups();
    createPopupIcon();
    createTooltip();
}

void GraphicsPixmapWidget::paint( QPainter* painter,
                                const QStyleOptionGraphicsItem* option, QWidget* )
{
    if ( !option->rect.isValid() ) {
        return;
    }
    painter->drawPixmap( option->rect, m_pixmap );
}

struct RoutePartCount {
    QString latestCommonStop;
    int usedCount; // used by X transport lines (each line and target counts as one)
};

class RoutePartCountGreaterThan
{
public:
    inline RoutePartCountGreaterThan() {};

    inline bool operator()( const RoutePartCount &l, const RoutePartCount &r ) const {
        return l.usedCount > r.usedCount;
    };
};

void PublicTransport::adjustColorGroupSettingsCount()
{
    while ( m_settings.colorGroupSettingsList.count() < m_settings.stopSettingsList.count() ) {
        m_settings.colorGroupSettingsList << ColorGroupSettingsList();
    }
    while ( m_settings.colorGroupSettingsList.count() > m_settings.stopSettingsList.count() ) {
        m_settings.colorGroupSettingsList.removeLast();
    }
}

void PublicTransport::updateColorGroupSettings()
{
    if ( m_settings.colorize ) {
        // Generate color groups from existing departure data
        adjustColorGroupSettingsCount();
        ColorGroupSettingsList colorGroups =
                m_settings.colorGroupSettingsList[ m_settings.currentStopSettingsIndex ];
        kDebug() << "Generate new color group settings for" << m_model->departureInfos().count()
                 << "departures, maximal 30";
        colorGroups = generateColorGroupSettingsFrom( m_model->departureInfos().mid(0, 30) );
        m_settings.colorGroupSettingsList[ m_settings.currentStopSettingsIndex ] = colorGroups;
        m_model->setColorGroups( colorGroups );
    } else {
        // Remove color groups if colorization was toggled off
        // or if stop/filter settings were changed (update color groups after data arrived)
        m_model->setColorGroups( ColorGroupSettingsList() );
    }
}

ColorGroupSettingsList PublicTransport::generateColorGroupSettingsFrom(
        const QList< DepartureInfo >& infos )
{
    kDebug() << "Generate new color group settings for" << infos.count() << "departures";

    // Only test routes of up to 1 stops (the first 1 of the actual route)
    const int maxTestRouteLength = 1;

    // Maximal number of groups
    const int maxGroupCount = 5;

    const int opacity = 75;
    const QColor colors[5] = {
        QColor(0, 230, 0, opacity), // green
        QColor(0, 0, 255, opacity), // blue
        QColor(200, 125, 20, opacity), // brown
        QColor(50, 225, 225, opacity), // cyan
        QColor(255, 255, 0, opacity) // yellow
    };

    // Map route parts to lists of concatenated strings,
    // ie. transport line and target
    QHash< QStringList, QStringList > routePartsToLines;
    for ( int stopCount = 1; stopCount <= maxTestRouteLength; ++stopCount ) {
        foreach ( const DepartureInfo &info, infos ) {
            QString transportLineAndTarget = info.lineString().toLower() + info.target().toLower();
            QStringList routePart = info.routeStops().mid( 1, stopCount );

            // Check if the route part was already counted
            if ( routePartsToLines.contains(routePart) ) {
                // Check if the transportLineAndTarget string was already counted for the route part
                if ( !routePartsToLines[routePart].contains(transportLineAndTarget) ) {
                    // Add new transportLineAndTarget string for the route part, 
                    // ie. increment the count of the routepart by one
                    routePartsToLines[routePart] << transportLineAndTarget;
                }
            } else {
                // Add new route part with the transportLineAndTarget string counted
                routePartsToLines.insert( routePart, QStringList() << transportLineAndTarget );
            }
        }
    }

    // Create list with the last stop of each route part and it's count (wrapped in RoutePartCount)
    QList< RoutePartCount > routePartCount;
    for ( QHash< QStringList, QStringList >::const_iterator it = routePartsToLines.constBegin();
          it != routePartsToLines.constEnd(); ++it )
    {
        int count = it.value().count();
        if ( count > 1 ) {
            RoutePartCount routeCount;
            routeCount.latestCommonStop = it.key().last();
            routeCount.usedCount = it.value().count();
            routePartCount << routeCount;
        } else {
            qDebug() << "UsedCount <= 1 for stop" << it.key().last() << it.value().count();
        }
    }

    // Sort by used count
    qStableSort( routePartCount.begin(), routePartCount.end(), RoutePartCountGreaterThan() );

    // Create the color groups
    ColorGroupSettingsList colorGroups;
    for ( int i = 0; i < maxGroupCount && i < routePartCount.count(); ++i ) {
        RoutePartCount routeCount = routePartCount[i];
        ColorGroupSettings group;
        Filter groupFilter;
        groupFilter << Constraint( FilterByNextStop, FilterEquals, routeCount.latestCommonStop );
        group.filters << groupFilter;
        group.color = colors[i];
        colorGroups << group;
        qDebug() << "Colorgroup" << i << "created" << routeCount.latestCommonStop << routeCount.usedCount;
    }

    return colorGroups;
}

#include "publictransport.moc"
