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
#include "journeysearchmodel.h"
#include "journeysearchitem.h"
#include "journeysearchlistview.h"
#include "settingsui.h"
#include "settingsio.h"
#include "timetablewidget.h"
#include "popupicon.h"
#include "titlewidget.h"
#include "colorgroups.h"

// libpublictransporthelper includes
#include <global.h>

// KDE includes
#include <KDebug>
#include <KLocale>
#include <KNotification>
#include <KToolInvocation>
#include <KColorScheme>
#include <KSelectAction>
#include <KActionMenu>
#include <KPushButton>
#include <KMenu>
#include <KMimeTypeTrader>
#include <KStandardDirs>
#include <KProcess>
#include <KMessageBox> // Currently only used to ask if marble should be installed (TODO should be done using Plasma::Applet::showMessage())

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
#include <QClipboard>
#include <QGraphicsScene>
#include <QGraphicsLinearLayout>
#include <QStateMachine>
#include <QHistoryState>
#include <QDBusConnection> // DBus used for marble
#include <QDBusMessage>
#include <QTimer>
#include <QLabel>
#include <QStandardItemModel>
#include <QSignalTransition>
#include <QParallelAnimationGroup>
#include <QGraphicsSceneEvent>
#include <qmath.h>

class ToPropertyTransition : public QSignalTransition
{
public:
    ToPropertyTransition( QObject *sender, const char *signal, QState *source,
                          QObject *propertyObject, const char *targetStateProperty )
        : QSignalTransition(sender, signal, source),
          m_propertyObject(propertyObject),
          m_property(targetStateProperty)
    {
        qRegisterMetaType<QState*>("QState*");
    };

    const QObject *propertyObject() const { return m_propertyObject; };
    const char *targetStateProperty() const { return m_property; };
    QState *currentTargetState() const {
        return qobject_cast<QState*>( qvariant_cast<QObject*>(m_propertyObject->property(m_property)) );
    };
    void setTargetStateProperty( const QObject *propertyObject, const char *property ) {
        m_propertyObject = propertyObject;
        m_property = property;
    };

protected:
    virtual bool eventTest( QEvent *event )
    {
        if ( !QSignalTransition::eventTest(event) ) {
            return false;
        }

        setTargetState( currentTargetState() );
        return true;
    };

private:
    const QObject *m_propertyObject;
    const char *m_property;
};

PublicTransport::PublicTransport( QObject *parent, const QVariantList &args )
        : Plasma::PopupApplet( parent, args ),
        m_graphicsWidget(0), m_mainGraphicsWidget(0), m_oldItem(0), m_titleWidget(0),
        m_labelInfo(0), m_timetable(0), m_journeyTimetable(0), m_listStopSuggestions(0),
        m_overlay(0), m_model(0), m_popupIcon(0),
        m_titleToggleAnimation(0), m_modelJourneys(0), m_departureProcessor(0), m_stateMachine(0),
        m_journeySearchTransition1(0), m_journeySearchTransition2(0),
        m_journeySearchTransition3(0), m_marble(0)
{
    m_originalStopIndex = -1;

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
    connect( m_departureProcessor, SIGNAL(departuresFiltered(QString,QList<DepartureInfo>,QList<DepartureInfo>,QList<DepartureInfo>)),
             this, SLOT(departuresFiltered(QString,QList<DepartureInfo>,QList<DepartureInfo>,QList<DepartureInfo>)) );

    // Load vehicle type SVG
    m_vehiclesSvg.setImagePath( KGlobal::dirs()->findResource("data",
            "plasma_applet_publictransport/vehicles.svg") );
    m_vehiclesSvg.setContainsMultipleImages( true );

    m_departurePainter = new DeparturePainter( this );
    m_departurePainter->setSvg( &m_vehiclesSvg );

    m_popupIcon = new PopupIcon( m_departurePainter, this );
    connect( m_popupIcon, SIGNAL(currentDepartureGroupChanged(int)),
             this, SLOT(updateTooltip()) );
    connect( m_popupIcon, SIGNAL(currentDepartureGroupIndexChanged(qreal)),
             this, SLOT(updatePopupIcon()) );
    connect( m_popupIcon, SIGNAL(currentDepartureIndexChanged(qreal)),
             this, SLOT(updatePopupIcon()) );

    if ( !m_settings.stopSettingsList.isEmpty() ) {
        QVariantHash serviceProviderData = currentServiceProviderData();
        m_currentServiceProviderFeatures = serviceProviderData.isEmpty()
                ? QStringList() : serviceProviderData["features"].toStringList();
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
    StopSettings stopSettings = m_settings.currentStopSettings();
    m_model = new DepartureModel( this );
    m_model->setDepartureArrivalListType( m_settings.departureArrivalListType );
    m_model->setHomeStop( stopSettings.stopList().isEmpty() ? QString() : stopSettings.stop(0).name );
    m_model->setCurrentStopIndex( m_settings.currentStopSettingsIndex );
    connect( m_model, SIGNAL(alarmFired(DepartureItem*,AlarmSettings)),
             this, SLOT(alarmFired(DepartureItem*,AlarmSettings)) );
    connect( m_model, SIGNAL(updateAlarms(AlarmSettingsList,QList<int>)),
             this, SLOT(removeAlarms(AlarmSettingsList,QList<int>)) );
    connect( m_model, SIGNAL(itemsAboutToBeRemoved(QList<ItemBase*>)),
             this, SLOT(departuresAboutToBeRemoved(QList<ItemBase*>)) );
    connect( m_model, SIGNAL(departuresLeft(QList<DepartureInfo>)),
             this, SLOT(departuresLeft(QList<DepartureInfo>)) );
    m_modelJourneys = new JourneyModel( this );
    m_modelJourneys->setHomeStop( stopSettings.stopList().isEmpty()
                                  ? QString() : stopSettings.stop(0).name );
    m_modelJourneys->setCurrentStopIndex( m_settings.currentStopSettingsIndex );
    m_modelJourneys->setAlarmSettings( m_settings.alarmSettingsList );
    m_popupIcon->setModel( m_model );

    // Create widgets
    graphicsWidget();

    // Setup actions and the state machine
    setupActions();
    setupStateMachine();

    // Check for network connectivity and create tooltip / popup icon
    checkNetworkStatus();
    createTooltip();
    if ( isIconified() ) {
        updatePopupIcon();
    } else {
        // Set a popup icon, because otherwise the applet collapses to an icon
        // when shown in a desktop
        setPopupIcon( "public-transport-stop" );
    }

    connect( this, SIGNAL(geometryChanged()), this, SLOT(geometryChanged()) );
    connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
    connect( Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()),
             this, SLOT(themeChanged()) );
    emit settingsChanged();
    serviceProviderSettingsChanged();

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

    // "Search Journeys..." action transitions to the journey search view (state "journeySearch").
    // If journeys aren't supported by the current service provider, a message gets displayed
    // and the target state is "journeysUnsupportedView". The target states of these transitions
    // get dynamically adjusted when the service provider settings change
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
    actionButtonsState->addTransition(
            this, SIGNAL(journeySearchFinished()), journeyViewState );
    departureViewState->addTransition(
            m_titleWidget, SIGNAL(iconClicked()), actionButtonsState );
    departureViewState->addTransition(
            action("showActionButtons"), SIGNAL(triggered()), actionButtonsState );
    journeyViewState->addTransition(
            action("showActionButtons"), SIGNAL(triggered()), actionButtonsState );
    journeySearchState->addTransition(
            this, SIGNAL(journeySearchFinished()), journeyViewState );

    // Direct transition from departure view to journey view using a favorite/recent journey action
    departureViewState->addTransition(
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
    connect( journeySearchState, SIGNAL(exited()),
             this, SLOT(exitJourneySearch()) );
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
    return "unknown";
// TODO: This crashes somewhere in Solid::Control::NetworkManager::networkInterfaces()
//       This also crashes eg. plasmaengineexplorer when switching to the network engine..
    const QStringList interfaces = dataEngine( "network" )->sources();
    if ( interfaces.isEmpty() ) {
        return "unknown";
    }

    // Check if there is an activated interface or at least one that's
    // currently being configured
    QString status = "unavailable";
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

void PublicTransport::setSettings( const StopSettingsList& stopSettingsList,
                                   const FilterSettingsList& filterSettings )
{
    // Set settings in a copy of the current settings.
    // Then write the new settings.
    Settings settings = m_settings;
    settings.stopSettingsList = stopSettingsList;
    settings.filterSettingsList = filterSettings;
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
    KAction *actionUpdate = new KAction( KIcon("view-refresh"),
                                         i18nc("@action:inmenu", "&Update timetable"), this );
    connect( actionUpdate, SIGNAL(triggered()), this, SLOT(updateDataSource()) );
    addAction( "updateTimetable", actionUpdate );

    KAction *showActionButtons = new KAction( /*KIcon("system-run"),*/ // TODO: better icon
            i18nc("@action", "&Quick Actions"), this );
    addAction( "showActionButtons", showActionButtons );

    KAction *actionCreateAlarmForDeparture = new KAction(
            GlobalApplet::makeOverlayIcon( KIcon("task-reminder"), "list-add" ),
            m_settings.departureArrivalListType == DepartureList
            ? i18nc("@action:inmenu", "Set &Alarm for This Departure")
            : i18nc("@action:inmenu", "Set &Alarm for This Arrival"), this );
    connect( actionCreateAlarmForDeparture, SIGNAL(triggered()),
             this, SLOT(createAlarmForDeparture()) );
    addAction( "createAlarmForDeparture", actionCreateAlarmForDeparture );

    KAction *actionCreateAlarmForDepartureCurrentWeekDay = new KAction(
            GlobalApplet::makeOverlayIcon( KIcon("task-reminder"), "list-add" ),
            i18nc("@action:inmenu", "Set &Alarm for Current Weekday"), this );
    connect( actionCreateAlarmForDepartureCurrentWeekDay, SIGNAL(triggered()),
             this, SLOT(createAlarmForDepartureCurrentWeekDay()) );
    addAction( "createAlarmForDepartureCurrentWeekDay", actionCreateAlarmForDepartureCurrentWeekDay );

    KAction *actionRemoveAlarmForDeparture = new KAction(
            GlobalApplet::makeOverlayIcon( KIcon("task-reminder"), "list-remove" ),
            m_settings.departureArrivalListType == DepartureList
            ? i18nc("@action:inmenu", "Remove &Alarm for This Departure")
            : i18nc("@action:inmenu", "Remove &Alarm for This Arrival"), this );
    connect( actionRemoveAlarmForDeparture, SIGNAL(triggered()),
             this, SLOT(removeAlarmForDeparture()) );
    addAction( "removeAlarmForDeparture", actionRemoveAlarmForDeparture );

    KAction *actionSearchJourneys = new KAction( KIcon("edit-find"),
            i18nc("@action", "Search for &Journeys..."), this );
    addAction( "searchJourneys", actionSearchJourneys );

    KAction *actionConfigureJourneys = new KAction( KIcon("configure"),
            i18nc("@action", "&Configure Journey Searches"), this );
    connect( actionConfigureJourneys, SIGNAL(triggered()), this, SLOT(configureJourneySearches()) );
    addAction( "configureJourneys", actionConfigureJourneys );

    KActionMenu *actionJourneys = new KActionMenu( KIcon("edit-find"),
            i18nc("@action", "&Journeys"), this );
    connect( actionJourneys->menu(), SIGNAL(triggered(QAction*)),
             this, SLOT(journeyActionTriggered(QAction*)) );
    addAction( "journeys", actionJourneys );

    // Fill the journey menu with actions and pass the journey action to the title widget
    updateJourneyActionMenu();
    m_titleWidget->setJourneysAction( actionJourneys );

    int iconExtend = 32;
    KAction *actionShowDepartures = new KAction(
            GlobalApplet::makeOverlayIcon( KIcon("public-transport-stop"),
                QList<KIcon>() << KIcon("go-home") << KIcon("go-next"),
                QSize(iconExtend / 2, iconExtend / 2), iconExtend ),
            i18nc("@action", "Show &Departures"), this );
    addAction( "showDepartures", actionShowDepartures );

    KAction *actionShowArrivals = new KAction(
            GlobalApplet::makeOverlayIcon( KIcon("public-transport-stop"),
                QList<KIcon>() << KIcon("go-next") << KIcon("go-home"),
                QSize(iconExtend / 2, iconExtend / 2), iconExtend ),
            i18nc("@action", "Show &Arrivals"), this );
    addAction( "showArrivals", actionShowArrivals );

    KAction *actionBackToDepartures = new KAction( KIcon("go-previous"),
            i18nc("@action", "Back to &Departure List"), this );
    addAction( "backToDepartures", actionBackToDepartures );

    m_filtersGroup = new QActionGroup( this );
    m_filtersGroup->setExclusive( false );
    connect( m_filtersGroup, SIGNAL(triggered(QAction*)),
             this, SLOT(switchFilterConfiguration(QAction*)) );

    m_colorFiltersGroup = new QActionGroup( this );
    m_colorFiltersGroup->setExclusive( false );
    connect( m_colorFiltersGroup, SIGNAL(triggered(QAction*)),
             this, SLOT(switchFilterByGroupColor(QAction*)) );

    KActionMenu *actionFilterConfiguration = new KActionMenu( KIcon("view-filter"),
            i18nc("@action", "Filter"), this );
    addAction( "filterConfiguration", actionFilterConfiguration );
    m_titleWidget->setFiltersAction( actionFilterConfiguration );

    KAction *actionToggleExpanded = new KAction( KIcon( "arrow-down" ),
            i18nc( "@action:inmenu", "&Show Additional Information" ), this );
    connect( actionToggleExpanded, SIGNAL(triggered()), this, SLOT(toggleExpanded()) );
    addAction( "toggleExpanded", actionToggleExpanded );

    KAction *actionUnhighlightStop = new KAction( KIcon("edit-select"),
            i18nc("@action:inmenu", "&Unhighlight All Stops"), this );
    connect( actionUnhighlightStop, SIGNAL(triggered()), m_model, SLOT(setHighlightedStop()) );
    addAction( "unhighlightStop", actionUnhighlightStop );

    // TODO: Combine actionHideColumnTarget and actionShowColumnTarget into one action?
    KAction *actionHideColumnTarget = new KAction( KIcon("view-right-close"),
            i18nc("@action:inmenu", "Hide &target column"), this );
    connect( actionHideColumnTarget, SIGNAL(triggered()), this, SLOT(hideColumnTarget()) );
    addAction( "hideColumnTarget", actionHideColumnTarget );

    KAction *actionShowColumnTarget = new KAction( KIcon("view-right-new"),
            i18nc("@action:inmenu", "Show &target column"), this );
    connect( actionShowColumnTarget, SIGNAL(triggered()),
             this, SLOT(showColumnTarget()) );
    addAction( "showColumnTarget", actionShowColumnTarget );
}

void PublicTransport::updateJourneyActionMenu()
{
    KActionMenu *journeysAction = qobject_cast<KActionMenu*>( action("journeys") );
    KMenu *menu = journeysAction->menu();
    menu->clear();

    // Add action to go to journey search view
    // Do not add a separator after it, because a menu title item follows
    menu->addAction( action("searchJourneys") );

    // Extract lists of journey search strings / names
    QStringList favoriteJourneySearchNames;
    QStringList favoriteJourneySearches;
    QStringList recentJourneySearchNames;
    QStringList recentJourneySearches;
    foreach ( const JourneySearchItem &item, m_settings.currentJourneySearches() ) {
        if ( item.isFavorite() ) {
            favoriteJourneySearches << item.journeySearch();
            favoriteJourneySearchNames << item.nameOrJourneySearch();
        } else {
            recentJourneySearches << item.journeySearch();
            recentJourneySearchNames << item.nameOrJourneySearch();
        }
    }

    // Add favorite journey searches
    if ( !favoriteJourneySearches.isEmpty() ) {
        menu->addTitle( KIcon("favorites"),
                        i18nc("@title Title item in quick journey search menu",
                              "Favorite Journey Searches") );
        QList< QAction* > actions;
        KIcon icon( "edit-find", 0, QStringList() << "favorites" );
        for ( int i = 0; i < favoriteJourneySearches.count(); ++i ) {
            KAction *action = new KAction( icon, favoriteJourneySearchNames[i], menu );
            action->setData( favoriteJourneySearches[i] );
            actions << action;
        }
        menu->addActions( actions );
    }

    // Add recent journey searches
    if ( !recentJourneySearches.isEmpty() ) {
        menu->addTitle( KIcon("document-open-recent"),
                        i18nc("@title Title item in quick journey search menu",
                              "Recent Journey Searches") );
        QList< QAction* > actions;
        KIcon icon( "edit-find" );
        for ( int i = 0; i < recentJourneySearches.count(); ++i ) {
            KAction *action = new KAction( icon, recentJourneySearchNames[i], menu );
            action->setData( recentJourneySearches[i] );
            actions << action;
        }
        menu->addActions( actions );
    }

    // Add a separator before the configure action
    menu->addSeparator();

    // Add the configure action, which is distinguishable from others by having no data
    menu->addAction( action("configureJourneys") );
}

QList< QAction* > PublicTransport::contextualActions()
{
    QAction *switchDepArr = m_settings.departureArrivalListType == DepartureList
            ? action( "showArrivals" ) : action( "showDepartures" );

    // Add filter action if there is at least one filter or color group
    KAction *actionFilter = 0;
    if ( !m_settings.filterSettingsList.isEmpty() &&
         !m_settings.colorGroupSettingsList.isEmpty() )
    {
        actionFilter = qobject_cast< KAction* >( action("filterConfiguration") );
    }

    // Add "Update Timetable" action
    QList< QAction* > actions;
    actions << action( "updateTimetable" );

    // Add a separator
    QAction *separator = new QAction( this );
    separator->setSeparator( true );
    actions.append( separator );

    // Add actions: Switch Departures/Arrivals, Switch Current Stop,
    if ( m_currentServiceProviderFeatures.contains("Arrivals") ) {
        actions << switchDepArr;
    }

    // When in intermediate departureview add an action to go back to the original stop
    // Otherwise add an action to switch the current stop and a journey action if supported
    if ( isStateActive("intermediateDepartureView") ) {
        QAction *goBackAction = action("backToDepartures");
        goBackAction->setText( i18nc("@action:inmenu", "&Back To Original Stop") );
        actions << goBackAction;
    } else if ( m_settings.stopSettingsList.count() > 1 ) {
        actions << switchStopAction( this );
        if ( m_currentServiceProviderFeatures.contains("JourneySearch") ) {
//             updateJourneyActionMenu();
            actions << action("journeys");
        }
    }

    // Add an action to switch filters if filters are available
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
            dataEngine( "publictransport" )->query( QString("ServiceProvider %1").arg(id) );
    return serviceProviderData;
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
        if ( stops.isEmpty() ) {
            // Currently no stops configured
            return;
        }
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

    m_popupIcon->createDepartureGroups();
    updatePopupIcon();
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

    // Update everything that might have changed when all departure data is there
    if ( departuresToGo == 0 ) {
        updateColorGroupSettings();
        m_popupIcon->createDepartureGroups();
        updatePopupIcon();
        createTooltip();
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

QList< DepartureInfo > PublicTransport::departureInfos( bool includeFiltered, int max ) const
{
    QList< DepartureInfo > ret;

    for ( int n = m_stopIndexToSourceName.count() - 1; n >= 0; --n ) {
        QString sourceName = stripDateAndTimeValues( m_stopIndexToSourceName[n] );
        if ( m_departureInfos.contains(sourceName) ) {
            foreach( const DepartureInfo &departureInfo, m_departureInfos[sourceName] ) {
                // Only add not filtered items
                if ( !departureInfo.isFilteredOut() || includeFiltered ) {
                    ret << departureInfo;
                }
            }
        }
    }

    qSort( ret.begin(), ret.end() );
    return max == -1 ? ret.mid( 0, m_settings.maximalNumberOfDepartures )
                     : ret.mid( 0, max );
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

void PublicTransport::processOsmData( const QString& sourceName,
                                      const Plasma::DataEngine::Data& data )
{
    qreal longitude = -1.0, latitude = -1.0;
    QString name;
    for ( Plasma::DataEngine::Data::const_iterator it = data.constBegin();
        it != data.constEnd(); ++it )
    {
        QHash< QString, QVariant > item = it.value().toHash();
        if ( item.contains("longitude") && item.contains("latitude") ) {
            longitude = item[ "longitude" ].toReal();
            latitude = item[ "latitude" ].toReal();
            if ( item.contains("name") ) {
                name = item[ "name" ].toString();
            }
            break; // Only use the first coordinates found TODO offer more coordinates if they aren't too equal
        }
    }

    if ( !qFuzzyCompare(longitude, -1.0) && !qFuzzyCompare(latitude, -1.0) ) {
        kDebug() << "Coords:" << longitude << latitude << data["finished"].toBool() << name;
        m_longitude = longitude;
        m_latitude = latitude;

        // Start marble
        if ( m_marble ) {
            // Marble already started
            QString destination = QString("org.kde.marble-%1").arg(m_marble->pid());

            // Set new window title
            QDBusMessage m1 = QDBusMessage::createMethodCall(destination,
                    "/marble/MainWindow_1", "org.kde.marble.KMainWindow", "setPlainCaption");
            m1 << i18nc("@title:window Caption for marble windows started to show a stops "
                    "position in a map. %1 is the stop name.", "\"PublicTransport: %1\"",
                    name);
            if ( !QDBusConnection::sessionBus().send(m1) ) {
                kDebug() << "Couldn't set marble title with dbus" << m1.errorMessage();
            }

            showStopInMarble( m_longitude, m_latitude );
        } else {
            QString command = "marble --caption " + i18nc("@title:window Caption for "
                    "marble windows started to show a stops position in a map. %1 is the "
                    "stop name.", "\"PublicTransport: %1\"", name);
            kDebug() << "Use this command to start marble:" << command;
            m_marble = new KProcess( this );
            m_marble->setProgram( "marble", QStringList() << "--caption"
                    << i18nc("@title:window Caption for "
                    "marble windows started to show a stops position in a map. %1 is the "
                    "stop name.", "\"PublicTransport: %1\"", name) );
            connect( m_marble, SIGNAL(error(QProcess::ProcessError)),
                    this, SLOT(errorMarble(QProcess::ProcessError)) );
            connect( m_marble, SIGNAL(started()), this, SLOT(marbleHasStarted()) );
            connect( m_marble, SIGNAL(finished(int)), this, SLOT(marbleFinished(int)) );
            m_marble->start();
        }
        dataEngine("openstreetmap")->disconnectSource( sourceName, this );
    } else if ( data.contains("finished") && data["finished"].toBool() ) {
        kDebug() << "Couldn't find coordinates for the stop.";
        showMessage( KIcon("dialog-warning"),
                        i18nc("@info", "Couldn't find coordinates for the stop."),
                        Plasma::ButtonOk );

        dataEngine("openstreetmap")->disconnectSource( sourceName, this );
    }
}

void PublicTransport::dataUpdated( const QString& sourceName,
                                   const Plasma::DataEngine::Data& data )
{
    if ( sourceName.startsWith(QLatin1String("getCoords"), Qt::CaseInsensitive) ) {
        processOsmData( sourceName, data );
        return;
    }

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

    // Compute start and end indices of the departure groups to animate between
    if ( event->delta() > 0 ) {
        // Wheel rotated forward
        m_popupIcon->animateToNextGroup();
    } else if ( event->delta() < 0 ) {
        // Wheel rotated backward
        m_popupIcon->animateToPreviousGroup();
    } else {
        // Wheel not rotated?
        return;
    }
}

void PublicTransport::departuresAboutToBeRemoved( const QList<ItemBase*>& departures )
{
    m_popupIcon->departuresAboutToBeRemoved( departures );
    updatePopupIcon();
    createTooltip();
}

void PublicTransport::departuresLeft( const QList< DepartureInfo > &departures )
{
    Q_UNUSED( departures );
}

void PublicTransport::titleToggleAnimationFinished()
{
    delete m_titleToggleAnimation;
    m_titleToggleAnimation = 0;
}

void PublicTransport::updatePopupIcon()
{
    if ( isIconified() ) {
        int iconSize = qMin( 128, int(size().width()) );
        setPopupIcon( m_popupIcon->createPopupIcon(QSize(iconSize, iconSize)) );
    }
}

void PublicTransport::resized()
{
    // Get the size of the applet/popup (not the size of the popup icon if iconified)
    QSizeF size = m_graphicsWidget->size();

    if ( m_titleWidget ) {
        updatePopupIcon();

        // Show/hide title widget
        const qreal minHeightWithTitle = 200.0;
        const qreal maxHeightWithoutTitle = 225.0;
        if ( size.height() <= minHeightWithTitle // too small?
             && ((!m_titleToggleAnimation // title not already hidden?
                  && m_titleWidget->maximumHeight() > 0.1)
              || (m_titleToggleAnimation // title not currently animated to be hidden?
                  && m_titleToggleAnimation->direction() != QAbstractAnimation::Forward)) )
        {
            // Hide title: The applets vertical size is too small to show it
            //             and the title is not already hidden or currently being faded out
            if ( m_titleToggleAnimation ) {
                delete m_titleToggleAnimation;
            }

            // Create toggle animation with direction forward
            // to indicate that the title gets hidden
            m_titleToggleAnimation = new QParallelAnimationGroup( this );
            m_titleToggleAnimation->setDirection( QAbstractAnimation::Forward );

            Plasma::Animation *fadeAnimation = Plasma::Animator::create(
                    Plasma::Animator::FadeAnimation, m_titleToggleAnimation );
            fadeAnimation->setTargetWidget( m_titleWidget );
            fadeAnimation->setProperty( "startOpacity", m_titleWidget->opacity() );
            fadeAnimation->setProperty( "targetOpacity", 0.0 );

            QPropertyAnimation *shrinkAnimation = new QPropertyAnimation(
                    m_titleWidget, "maximumSize", m_titleToggleAnimation );
            shrinkAnimation->setStartValue( QSizeF(m_titleWidget->maximumWidth(),
                                                   m_titleWidget->layout()->preferredHeight()) );
            shrinkAnimation->setEndValue( QSizeF(m_titleWidget->maximumWidth(), 0) );

            connect( m_titleToggleAnimation, SIGNAL(finished()),
                     this, SLOT(titleToggleAnimationFinished()) );
            m_titleToggleAnimation->addAnimation( fadeAnimation );
            m_titleToggleAnimation->addAnimation( shrinkAnimation );
            m_titleToggleAnimation->start();
        } else if ( size.height() >= maxHeightWithoutTitle // big enough?
            && ((!m_titleToggleAnimation // title not already shown?
                 && m_titleWidget->maximumHeight() < m_titleWidget->layout()->preferredHeight())
             || (m_titleToggleAnimation // title not currently animated to be shown?
                 && m_titleToggleAnimation->direction() != QAbstractAnimation::Backward)) )
        {
            // Show title: The applets vertical size is big enough to show it
            //             and the title is not already shown or currently beging faded in
            if ( m_titleToggleAnimation ) {
                delete m_titleToggleAnimation;
            }

            // Create toggle animation with direction backward
            // to indicate that the title gets shown again.
            // The child animations use reversed start/end values.
            m_titleToggleAnimation = new QParallelAnimationGroup( this );
            m_titleToggleAnimation->setDirection( QAbstractAnimation::Backward );

            Plasma::Animation *fadeAnimation = Plasma::Animator::create(
                    Plasma::Animator::FadeAnimation, m_titleToggleAnimation );
            fadeAnimation->setTargetWidget( m_titleWidget );
            fadeAnimation->setProperty( "targetOpacity", m_titleWidget->opacity() );
            fadeAnimation->setProperty( "startOpacity", 1.0 );

            QPropertyAnimation *growAnimation = new QPropertyAnimation(
                    m_titleWidget, "maximumSize", m_titleToggleAnimation );
            growAnimation->setEndValue( QSizeF(m_titleWidget->maximumWidth(),
                                               m_titleWidget->maximumHeight()) );
            growAnimation->setStartValue( QSizeF(m_titleWidget->maximumWidth(),
                                                 m_titleWidget->layout()->preferredHeight()) );

            connect( m_titleToggleAnimation, SIGNAL(finished()),
                        this, SLOT(titleToggleAnimationFinished()) );
            m_titleToggleAnimation->addAnimation( fadeAnimation );
            m_titleToggleAnimation->addAnimation( growAnimation );
            m_titleToggleAnimation->start();
        }

        // Show/hide vertical scrollbar
        const qreal minWidthWithScrollBar = 250.0;
        const qreal maxWidthWithoutScrollBar = 275.0;
        if ( size.width() <= minWidthWithScrollBar ) {
            m_timetable->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        } else if ( size.width() >= maxWidthWithoutScrollBar ) {
            m_timetable->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
        }

        // Update quick journey search widget (show icon or icon with text)
        Plasma::ToolButton *quickJourneySearchWidget =
                m_titleWidget->castedWidget<Plasma::ToolButton>( TitleWidget::WidgetQuickJourneySearch );
        Plasma::ToolButton *filterWidget =
                m_titleWidget->castedWidget<Plasma::ToolButton>( TitleWidget::WidgetFilter );
        if ( quickJourneySearchWidget ) {
            if ( m_titleWidget->layout()->preferredWidth() > size.width() ) {
                // Show only an icon on the quick journey search toolbutton,
                // if there is not enough horizontal space
                quickJourneySearchWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonIconOnly );
                quickJourneySearchWidget->setMaximumWidth( quickJourneySearchWidget->size().height() );
            } else if ( quickJourneySearchWidget->nativeWidget()->toolButtonStyle() == Qt::ToolButtonIconOnly
                && size.width() > m_titleWidget->layout()->minimumWidth() +
                QFontMetrics(quickJourneySearchWidget->font()).width(quickJourneySearchWidget->text()) +
                (filterWidget->nativeWidget()->toolButtonStyle() == Qt::ToolButtonIconOnly
                    ? QFontMetrics(filterWidget->font()).width(filterWidget->text()) : 0) + 60 )
            {
                // Show the icon with text beside if there is enough horizontal space again
                quickJourneySearchWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                quickJourneySearchWidget->setMaximumWidth( -1 );
            }
        }

        // Update filter widget (show icon or icon with text)
        if ( filterWidget ) {
            if ( m_titleWidget->layout()->preferredWidth() > size.width() ) {
                // Show only an icon on the filter toolbutton,
                // if there is not enough horizontal space
                filterWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonIconOnly );
                filterWidget->setMaximumWidth( filterWidget->size().height() );
            } else if ( filterWidget->nativeWidget()->toolButtonStyle() == Qt::ToolButtonIconOnly
                && size.width() > m_titleWidget->layout()->minimumWidth() +
                QFontMetrics(filterWidget->font()).width(filterWidget->text()) + 60 )
            {
                // Show the icon with text beside if there is enough horizontal space again
                filterWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                filterWidget->setMaximumWidth( -1 );
            }
        }
    }

    // Update line breaking of the courtesy label
    updateInfoText();
}

void PublicTransport::resizeEvent( QGraphicsSceneResizeEvent *event )
{
    Plasma::Applet::resizeEvent( event );

    // Update popup icon to new size
    updatePopupIcon();
}

void PublicTransport::createTooltip()
{
    if ( formFactor() != Plasma::Horizontal && formFactor() != Plasma::Vertical ) {
        // Create the tooltip only when in a panel
        Plasma::ToolTipManager::self()->clearContent( this );
        return;
    }

    Plasma::ToolTipContent data;
    data.setMainText( i18nc("@info", "Public Transport") );
    if ( m_popupIcon->departureGroups()->isEmpty() ) {
        data.setSubText( i18nc("@info", "View departure times for public transport") );
    } else if ( !m_popupIcon->departureGroups()->isEmpty() ) {
        const DepartureGroup currentGroup = m_popupIcon->currentDepartureGroup();
        const bool isAlarmGroup = m_popupIcon->currentGroupIsAlarmGroup();
        const QString groupDurationString = currentGroup.first()->departureInfo()->durationString();
        QStringList infoStrings;

        if ( m_settings.departureArrivalListType ==  DepartureList ) {
            // Showing a departure list
            foreach ( const DepartureItem *item, currentGroup ) {
                infoStrings << i18nc("@info Text for one departure for the tooltip (%1: line string, "
                                     "%2: target)",
                                     "Line <emphasis strong='1'>%1<emphasis> "
                                     "to <emphasis strong='1'>%2<emphasis>",
                                     item->departureInfo()->lineString(),
                                     item->departureInfo()->target());
            }
            if ( isAlarmGroup ) {
                data.setSubText( i18ncp("@info %2 is the translated duration text (e.g. in 3 minutes), "
                                        "%4 contains texts for a list of departures",
                        "Alarm (%2) for a departure from '%3':<nl/>%4",
                        "%1 Alarms (%2) for departures from '%3':<nl/>%4",
                        currentGroup.count(),
                        groupDurationString, m_settings.currentStopSettings().stops().join(", "),
                        infoStrings.join(",<nl/>")) );
            } else {
                data.setSubText( i18ncp("@info %2 is the translated duration text (e.g. in 3 minutes), "
                                        "%4 contains texts for a list of departures",
                        "Departure (%2) from '%3':<nl/>%4",
                        "%1 Departures (%2) from '%3':<nl/>%4",
                        currentGroup.count(),
                        groupDurationString, m_settings.currentStopSettings().stops().join(", "),
                        infoStrings.join(",<nl/>")) );
            }
        } else {
            // Showing an arrival list
            foreach ( const DepartureItem *item, currentGroup ) {
                infoStrings << i18nc("@info Text for one arrival for the tooltip (%1: line string, "
                                     "%2: origin)",
                                     "Line <emphasis strong='1'>%1<emphasis> "
                                     "from <emphasis strong='1'>%2<emphasis>",
                                     item->departureInfo()->lineString(),
                                     item->departureInfo()->target());
            }
            if ( isAlarmGroup ) {
                data.setSubText( i18ncp("@info %2 is the translated duration text (e.g. in 3 minutes), "
                                        "%4 contains texts for a list of arrivals",
                        "Alarm (%2) for an arrival at '%3':<nl/>%4",
                        "%1 Alarms (%2) for arrivals at '%3':<nl/>%4",
                        currentGroup.count(),
                        groupDurationString, m_settings.currentStopSettings().stops().join(", "),
                        infoStrings.join(",<nl/>")) );
            } else {
                data.setSubText( i18ncp("@info %2 is the translated duration text (e.g. in 3 minutes), "
                                        "%4 contains texts for a list of arrivals",
                        "Arrival (%2) at '%3':<nl/>%4",
                        "%1 Arrivals (%2) at '%3':<nl/>%4",
                        currentGroup.count(),
                        groupDurationString, m_settings.currentStopSettings().stops().join(", "),
                        infoStrings.join(",<nl/>")) );
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
    m_departureProcessor->setColorGroups( m_settings.currentColorGroupSettings() );
    StopSettings stopSettings = m_settings.currentStopSettings();
    m_departureProcessor->setFirstDepartureSettings(
            static_cast<FirstDepartureConfigMode>(stopSettings.get<int>(
                FirstDepartureConfigModeSetting)),
            stopSettings.get<QTime>(TimeOfFirstDepartureSetting),
            stopSettings.get<int>(TimeOffsetOfFirstDepartureSetting),
            m_settings.departureArrivalListType == ArrivalList );
    m_departureProcessor->setAlarmSettings( m_settings.alarmSettingsList );

    // Apply other settings to the model
    m_timetable->setMaxLineCount( m_settings.linesPerRow );
    m_model->setLinesPerRow( m_settings.linesPerRow );
    m_model->setSizeFactor( m_settings.sizeFactor );
    m_model->setDepartureColumnSettings( m_settings.displayTimeBold,
            m_settings.showRemainingMinutes, m_settings.showDepartureTime );

    int alarmMinsBeforeDeparture = m_settings.currentStopSettings().get<int>(AlarmTimeSetting);
    m_model->setAlarmMinsBeforeDeparture( alarmMinsBeforeDeparture );
    m_modelJourneys->setAlarmMinsBeforeDeparture( alarmMinsBeforeDeparture );

//     m_modelJourneys->setHomeStop( m_settings.currentStopSettings().stop(0).name ); DONE IN WRITESETTINGS

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
        // Configuration is valid
        setConfigurationRequired( false );

        // Only use the default target state (journey search) if journeys
        // are supported by the used service provider. Otherwise go to the
        // alternative target state (journeys not supported).
        const bool journeysSupported = m_currentServiceProviderFeatures.contains("JourneySearch");
        QAbstractState *target = journeysSupported
                ? m_states["journeySearch"] : m_states["journeysUnsupportedView"];
        m_journeySearchTransition1->setTargetState( target );
        m_journeySearchTransition2->setTargetState( target );
        m_journeySearchTransition3->setTargetState( target );

        action("journeys")->setEnabled( journeysSupported );
        m_titleWidget->setJourneysSupported( journeysSupported );

        // Reconnect with new settings
        reconnectSource();
        if ( !m_currentJourneySource.isEmpty() ) {
            reconnectJourneySource();
        }
    } else {
        // Missing configuration, eg. no home stop
        setConfigurationRequired( true, i18nc("@info/plain", "Please check your configuration.") );

        action("journeys")->setEnabled( false );
        m_titleWidget->setJourneysSupported( false );
    }
}

void PublicTransport::destroyOverlay()
{
    if ( m_overlay ) {
        m_overlay->destroy();
        m_overlay = 0;
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
    if ( isStateActive("journeyView") ) {
        actions << action("backToDepartures");
    }
    if ( m_currentServiceProviderFeatures.contains("Arrivals") ) {
        actions << (m_settings.departureArrivalListType == DepartureList
                ? action("showArrivals") : action("showDepartures"));
    }
    if ( m_currentServiceProviderFeatures.contains("JourneySearch") ) {
//         updateJourneyActionMenu();
        actions << action("journeys");
    }

    // Add stop selector if multiple stops are defined
    if ( m_settings.stopSettingsList.count() > 1 ) {
        actions << switchStopAction( 0, true ); // Parent gets set below
    }

    // Create buttons for the actions and create a list of fade animations
    foreach ( QAction *action, actions ) {
        Plasma::PushButton *button = new Plasma::PushButton( m_overlay );
        button->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
        button->setAction( action );
        if ( action->menu() ) {
//             action->setParent( button ); // Set button as parent
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
    const QString filterConfig = KGlobal::locale()->removeAcceleratorMarker( action->text() );

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

void PublicTransport::switchFilterByGroupColor( QAction* action )
{
    const QColor color = action->data().value<QColor>();
    const bool enable = action->isChecked();

    // Change filter configuration of the current stop in a copy of the settings.
    // Then write the new settings.
    Settings settings = m_settings;
    settings.colorGroupSettingsList[settings.currentStopSettingsIndex].enableColorGroup( color, enable );
    writeSettings( settings );
}

void PublicTransport::enableFilterConfiguration( const QString& filterConfiguration, bool enable )
{
    const QString filterConfig = filterConfiguration;
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
    connect( m_journeyTimetable, SIGNAL(requestStopAction(StopAction::Type,QString)),
             this, SLOT(requestStopAction(StopAction::Type,QString)) );
    connect( m_journeyTimetable, SIGNAL(requestAlarmCreation(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)),
             this, SLOT(processAlarmCreationRequest(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)) );
    connect( m_journeyTimetable, SIGNAL(requestAlarmDeletion(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)),
             this, SLOT(processAlarmDeletionRequest(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)) );
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

    // Ensure the applet popup is shown
    showPopup();
}

void PublicTransport::showJourneySearch()
{
    fadeOutOldAppearance();
    m_titleWidget->setTitleType( ShowSearchJourneyLineEdit,
            isStateActive("departureDataValid"), isStateActive("journeyDataValid") );

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

    // Hide journey search action, because it switches to the currently active state
    action("searchJourneys")->setVisible( false );

    showMainWidget( m_listStopSuggestions );
    setBusy( false );

    // Ensure the applet popup is shown
    showPopup();
}

void PublicTransport::exitJourneySearch()
{
    // Show journey search action again
    action("searchJourneys")->setVisible( true );
}

void PublicTransport::showJourneysUnsupportedView()
{
    fadeOutOldAppearance();
    m_titleWidget->setTitleType( ShowSearchJourneyLineEditDisabled,
            isStateActive("departureDataValid"), isStateActive("journeyDataValid") );

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

    // Ensure the applet popup is shown,
    // but only for a few seconds as this just shows an error message
    showPopup( 3000 );
}

void PublicTransport::journeySearchInputFinished( const QString &text )
{
    clearJourneys();

    // Add journey search line to the list of recently used journey searches
    // and cut recent journey searches if the limit is exceeded
    Settings settings = m_settings;
    settings.addRecentJourneySearch( text );
    writeSettings( settings );

    m_journeyTitleText.clear();
    QString stop;
    QDateTime departure;
    bool stopIsTarget, timeIsDeparture;
    Plasma::LineEdit *journeySearch =
            m_titleWidget->castedWidget<Plasma::LineEdit>( TitleWidget::WidgetJourneySearchLine );
//     Q_ASSERT( journeySearch ); // May be 0 here, TODO use new journey search parser
    JourneySearchParser::parseJourneySearch( !journeySearch ? 0 : journeySearch->nativeWidget(),
            text, &stop, &departure, &stopIsTarget, &timeIsDeparture );

    reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture );
    emit journeySearchFinished();
}

void PublicTransport::journeySearchLineChanged( const QString& stopName,
        const QDateTime& departure, bool stopIsTarget, bool timeIsDeparture )
{
    reconnectJourneySource( stopName, departure, stopIsTarget, timeIsDeparture, true );
}

void PublicTransport::updateFilterMenu()
{
    KActionMenu *actionFilter = qobject_cast< KActionMenu* >( action("filterConfiguration") );
    KMenu *menu = actionFilter->menu();
    menu->clear();

    QList< QAction* > oldActions = m_filtersGroup->actions();
    foreach( QAction *oldAction, oldActions ) {
        m_filtersGroup->removeAction( oldAction );
        delete oldAction;
    }

    bool showColorGrous = m_settings.colorize && !m_settings.colorGroupSettingsList.isEmpty();
    if ( m_settings.filterSettingsList.isEmpty() && !showColorGrous ) {
        return; // Nothing to show in the filter menu
    }

    if ( !m_settings.filterSettingsList.isEmpty() ) {
        menu->addTitle( KIcon("view-filter"), i18nc("@title This is a menu title",
                                                    "Filters (reducing)") );
        foreach( const FilterSettings &filterSettings, m_settings.filterSettingsList ) {
            QAction *action = new QAction( filterSettings.name, m_filtersGroup );
            action->setCheckable( true );
            if ( filterSettings.affectedStops.contains(m_settings.currentStopSettingsIndex) ) {
                action->setChecked( true );
            }

            menu->addAction( action );
        }
    }

    if ( showColorGrous ) {
        // Add checkbox entries to toggle color groups
        if ( m_settings.departureArrivalListType == ArrivalList ) {
            menu->addTitle( KIcon("object-group"), i18nc("@title This is a menu title",
                                                         "Arrival Groups (extending)") );
        } else {
            menu->addTitle( KIcon("object-group"), i18nc("@title This is a menu title",
                                                         "Departure Groups (extending)") );
        }
        foreach( const ColorGroupSettings &colorGroupSettings,
                 m_settings.currentColorGroupSettings() )
        {
            // Create action for current color group
            QAction *action = new QAction( colorGroupSettings.lastCommonStopName,
                                           m_colorFiltersGroup );
            action->setCheckable( true );
            if ( !colorGroupSettings.filterOut ) {
                action->setChecked( true );
            }
            action->setData( QVariant::fromValue(colorGroupSettings.color) );

            // Draw a color patch with the color of the color group
            QPixmap pixmap( QSize(16, 16) );
            pixmap.fill( Qt::transparent );
            QPainter p( &pixmap );
            p.setRenderHints( QPainter::Antialiasing );
            p.setBrush( colorGroupSettings.color );
            QColor borderColor = KColorScheme(QPalette::Active).foreground().color();
            borderColor.setAlphaF( 0.75 );
            p.setPen( borderColor );
            p.drawRoundedRect( QRect(QPoint(1,1), pixmap.size() - QSize(2, 2)), 4, 4 );
            p.end();

            // Put the pixmap into a KIcon
            KIcon colorIcon;
            colorIcon.addPixmap( pixmap );
            action->setIcon( colorIcon );

            menu->addAction( action );
        }
    }
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
        m_graphicsWidget->setMinimumSize( 150, 150 ); // TODO allow smaller sizes, if zoom factor is small
        m_graphicsWidget->setPreferredSize( 400, 300 );
        connect( m_graphicsWidget, SIGNAL(geometryChanged()), this, SLOT(resized()) );

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
        connect( m_titleWidget, SIGNAL(journeySearchInputFinished(QString)),
                 this, SLOT(journeySearchInputFinished(QString)) );
        connect( m_titleWidget, SIGNAL(journeySearchListUpdated(QList<JourneySearchItem>)),
                 this, SLOT(journeySearchListUpdated(QList<JourneySearchItem>)) );

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
        connect( m_timetable, SIGNAL(requestStopAction(StopAction::Type,QString)),
                 this, SLOT(requestStopAction(StopAction::Type,QString)) );

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
//                     possibleStopItemActivated( curIndex );
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

        QVariantHash serviceProviderData = currentServiceProviderData();
        m_currentServiceProviderFeatures = serviceProviderData.isEmpty()
                ? QStringList() : serviceProviderData["features"].toStringList();
        emit configNeedsSaving();
        emit settingsChanged();

        if ( changed.testFlag(SettingsIO::ChangedServiceProvider) ||
             changed.testFlag(SettingsIO::ChangedCurrentStopSettings) ||
             changed.testFlag(SettingsIO::ChangedCurrentStop) )
        {
            serviceProviderSettingsChanged();
        }
        if ( changed.testFlag(SettingsIO::ChangedCurrentJourneySearchLists) ) {
            // Update the journeys menu
            updateJourneyActionMenu();
        }
        if ( changed.testFlag(SettingsIO::ChangedFilterSettings) ||
             changed.testFlag(SettingsIO::ChangedColorGroupSettings) )
        {
            // Update the journeys menu
            updateFilterMenu();
        }
        if ( changed.testFlag(SettingsIO::ChangedDepartureArrivalListType) ) {
            m_model->setDepartureArrivalListType( m_settings.departureArrivalListType );
            m_timetable->updateItemLayouts();

            // Adjust action texts to departure / arrival list
            action("removeAlarmForDeparture")->setText(
                    m_settings.departureArrivalListType == DepartureList
                    ? i18nc("@action", "Remove &Alarm for This Departure")
                    : i18nc("@action", "Remove &Alarm for This Arrival") );
            action("createAlarmForDeparture")->setText(
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
        if ( changed.testFlag(SettingsIO::ChangedCurrentStopSettings) ||
             changed.testFlag(SettingsIO::ChangedCurrentStop) ||
             changed.testFlag(SettingsIO::ChangedServiceProvider) )
        {
            m_settings.adjustColorGroupSettingsCount();
            clearDepartures();
            reconnectSource();
        } else if ( changed.testFlag(SettingsIO::ChangedFilterSettings)
                 || changed.testFlag(SettingsIO::ChangedColorGroupSettings) )
        {
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

        // Update current stop settings / current home stop in the models
        if ( changed.testFlag(SettingsIO::ChangedCurrentStop) ||
             changed.testFlag(SettingsIO::ChangedCurrentStopSettings) )
        {
            m_model->setHomeStop( m_settings.currentStopSettings().stop(0).name );
            m_model->setCurrentStopIndex( m_settings.currentStopSettingsIndex );

            if ( m_modelJourneys ) {
                m_modelJourneys->setHomeStop( m_settings.currentStopSettings().stop(0).name );
                m_modelJourneys->setCurrentStopIndex( m_settings.currentStopSettingsIndex );
            }
        }

        // Update the filter widget
        if ( changed.testFlag(SettingsIO::ChangedCurrentStop) ||
             changed.testFlag(SettingsIO::ChangedCurrentStopSettings) ||
             changed.testFlag(SettingsIO::ChangedFilterSettings) ||
             changed.testFlag(SettingsIO::ChangedColorGroupSettings) )
        {
            m_titleWidget->updateFilterWidget();
        }

        // Update alarm settings
        if ( changed.testFlag(SettingsIO::ChangedAlarmSettings) ) {
            m_model->setAlarmSettings( m_settings.alarmSettingsList );
            if ( m_modelJourneys ) {
                m_modelJourneys->setAlarmSettings( m_settings.alarmSettingsList );
            }
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
        return 0;
    }
}

void PublicTransport::oldItemAnimationFinished()
{
    if ( m_oldItem && m_oldItem->scene() ) {
        m_oldItem->scene()->removeItem( m_oldItem );
    }
    delete m_oldItem;
    m_oldItem = 0;
}

void PublicTransport::processJourneyRequest( const QString& stop, bool stopIsTarget )
{
    clearJourneys();
    reconnectJourneySource( stop, QDateTime(), stopIsTarget, true );
}

void PublicTransport::journeySearchListUpdated( const QList<JourneySearchItem> &newJourneySearches )
{
    Settings settings = m_settings;
    settings.setCurrentJourneySearches( newJourneySearches );
    writeSettings( settings );
}

void PublicTransport::journeyActionTriggered( QAction *_action )
{
    // The configure action has no data, only quick journey search items get the
    // journey search string as data
    if ( _action->data().isValid() ) {
        // The given action is not the configure action
        const QString journeySearch = KGlobal::locale()->removeAcceleratorMarker(
                _action->data().toString() );

        if ( isStateActive("journeySearch") ) {
            // If in journey search view, put the selected journey search into the input line edit
            m_titleWidget->setJourneySearch( journeySearch );
        } else {
            // Go directly to the journey results view
            journeySearchInputFinished( journeySearch );
        }
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

    if ( m_originalStopIndex != -1 ) {
        settings.currentStopSettingsIndex = qBound( 0, m_originalStopIndex,
                settings.stopSettingsList.count() - 1 );
    }
    m_originalStopIndex = -1;

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
        return 0;
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
    QAction *infoAction = 0;
    QObjectList objectsToBeDeleted;
    QList<QAction *> actions;
    actions.append( updatedAction("toggleExpanded") );
    if ( isStateActive("departureView") || isStateActive("intermediateDepartureView") ) {
        DepartureItem *item = static_cast<DepartureItem*>( m_model->item(m_clickedItemIndex.row()) );

        // Add alarm actions (but not for departures in an intermediate departure list)
        if ( isStateActive("departureView") ) {
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
                    objectsToBeDeleted << infoAction;
                }
            } else {
                actions.append( action("createAlarmForDeparture") );
                actions.append( action("createAlarmForDepartureCurrentWeekDay") );
            }
        }

        if ( !m_model->info().highlightedStop.isEmpty() ) {
            actions.append( action("unhighlightStop") );
        }

        if ( !item->departureInfo()->routeStops().isEmpty() ) {
            KMenu *menu = new KMenu();
            objectsToBeDeleted << menu;
            QAction *routeStopsAction = new QAction( KIcon("public-transport-intermediate-stops"),
                                                     i18nc("@action:inmenu", "Route Stops"), menu );
            routeStopsAction->setMenu( menu );
            const DepartureInfo *info = item->departureInfo();
            int count = info->routeStops().count();
            for ( int index = 0; index < count; ++index ) {
                QString stopName = info->routeStops()[index];

                // Get time information and route flags
                QTime time;
                int minsFromFirstRouteStop = 0;
                if ( index < info->routeTimes().count() && info->routeTimes()[index].isValid() ) {
                    time = info->routeTimes()[index];
                }
                RouteStopFlags routeStopFlags = item->routeStopFlags( index, &minsFromFirstRouteStop );

                KMenu *stopMenu = new KMenu( menu );
                QString stopText;
                if ( minsFromFirstRouteStop == 0 || !time.isValid() ) {
                    stopText = stopName;
                } else {
                    stopText = QString("%1 (%2)").arg(stopName)
                        .arg(KGlobal::locale()->prettyFormatDuration(minsFromFirstRouteStop * 60000));
                }
                QAction *routeStopsAction = new QAction( GlobalApplet::stopIcon(routeStopFlags),
                                                         stopText, stopMenu );

                if ( !routeStopFlags.testFlag(RouteStopIsHomeStop) ) {
                    StopAction *showDeparturesAction =
                            new StopAction( StopAction::ShowDeparturesForStop, stopMenu );
                    showDeparturesAction->setStopName( stopName );
                    connect( showDeparturesAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
                            this, SLOT(requestStopAction(StopAction::Type,QString)) );
                    stopMenu->addAction( showDeparturesAction );
                }

                if ( dataEngine("openstreetmap")->isValid() ) {
                    StopAction *showInMapAction =
                            new StopAction( StopAction::ShowStopInMap, stopMenu );
                    showInMapAction->setStopName( stopName );
                    connect( showInMapAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
                             this, SLOT(requestStopAction(StopAction::Type,QString)) );
                    stopMenu->addAction( showInMapAction );
                }

                if ( !routeStopFlags.testFlag(RouteStopIsHomeStop) ) {
                    StopAction *highlightAction =
                            new StopAction( StopAction::HighlightStop, stopMenu );
                    highlightAction->setStopName( stopName );
                    if ( m_model->info().highlightedStop == stopName ) {
                        highlightAction->setText( i18nc("@action:inmenu", "&Unhighlight This Stop") );
                    }
                    connect( highlightAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
                            this, SLOT(requestStopAction(StopAction::Type,QString)) );
                    stopMenu->addAction( highlightAction );

                    StopAction *createFilterAction =
                            new StopAction( StopAction::CreateFilterForStop, stopMenu );
                    createFilterAction->setStopName( stopName );
                    connect( createFilterAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
                            this, SLOT(requestStopAction(StopAction::Type,QString)) );
                    stopMenu->addAction( createFilterAction );
                }

                StopAction *copyToClipboardAction =
                        new StopAction( StopAction::CopyStopNameToClipboard, stopMenu );
                copyToClipboardAction->setStopName( stopName );
                connect( copyToClipboardAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
                         this, SLOT(requestStopAction(StopAction::Type,QString)) );
                stopMenu->addAction( copyToClipboardAction );

                menu->addAction( routeStopsAction );
                routeStopsAction->setMenu( stopMenu );
            }
            actions.append( routeStopsAction );
        }
    }

    // Show the menu if it's not empty
    if ( actions.count() > 0 && view() ) {
        QMenu::exec( actions, QCursor::pos() );
    }

    qDeleteAll( objectsToBeDeleted );
}

void PublicTransport::requestStopAction( StopAction::Type stopAction, const QString& stopName )
{
    // Create and enable new filter
    Settings settings = m_settings;

    switch ( stopAction ) {
        case StopAction::RequestJourneysToStop:
            // stopName is the target stop, the origin is the current home stop
            processJourneyRequest( stopName, true );
            break;
        case StopAction::RequestJourneysFromStop:
            // stopName is the origin stop,, the target is the current home stop
            processJourneyRequest( stopName, false );
            break;
        case StopAction::CreateFilterForStop: {
            QString filterName = i18nc("@info/plain Default name for a new filter via a given stop",
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
        } case StopAction::ShowStopInMap: {
            // Request coordinates from openstreetmap data engine
            QString osmStopName = stopName;
            int pos = osmStopName.lastIndexOf(',');
            if ( pos != -1 ) {
                osmStopName = osmStopName.left( pos );
            }

            osmStopName.remove( QRegExp("\\([^\\)]*\\)$") );

            QString sourceName = QString( "getCoords publictransportstops %1" ).arg( osmStopName );
            dataEngine("openstreetmap")->connectSource( sourceName, this );
            break;
        } case StopAction::ShowDeparturesForStop: {
            // Remove intermediate stop settings
            settings.stopSettingsList.removeIntermediateSettings();

            if ( m_originalStopIndex != -1 ) {
                kDebug() << "Set current stop index to" << m_originalStopIndex;
                settings.currentStopSettingsIndex = qBound( 0, m_originalStopIndex,
                        settings.stopSettingsList.count() - 1 );
            }

            // Save original stop index from where sub requests were made
            // (using the context menu). Only if the departure list wasn't requested
            // already from a sub departure list.
            m_originalStopIndex = settings.currentStopSettingsIndex;

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
        } case StopAction::HighlightStop: {
            m_model->setHighlightedStop(
                    m_model->highlightedStop().compare(stopName, Qt::CaseInsensitive) == 0
                    ? QString() : stopName );
            break;
        } case StopAction::CopyStopNameToClipboard: {
            QApplication::clipboard()->setText( stopName );
            break;
        }
    }
}

void PublicTransport::errorMarble( QProcess::ProcessError processError )
{
    if ( processError == QProcess::FailedToStart ) {
        int result = KMessageBox::questionYesNo( 0, i18nc("@info", "The map application "
                "'marble' couldn't be started, error message: <message>%1</message>.<nl/>"
                "Do you want to install 'marble' now?", m_marble->errorString()) );
        if ( result == KMessageBox::Yes ) {
            // Start KPackageKit to install marble
            KProcess *kPackageKit = new KProcess( this );
            kPackageKit->setProgram( "kpackagekit",
                                     QStringList() << "--install-package-name" << "marble" );
            kPackageKit->start();
        }
    } else if ( processError == QProcess::Crashed ) {
        showMessage( KIcon("dialog-information"),
                     i18nc("@info", "The map application 'marble' crashed"), Plasma::ButtonOk );
    }
    m_marble = 0;
}

void PublicTransport::marbleHasStarted()
{
    kDebug() << "Marble has started" << m_marble->pid();

    // Wait for output from marble
    for ( int i = 0; i < 10; ++i ) {
        if ( m_marble->waitForReadyRead(50) ) {
            break;
        }
    }

    QTimer::singleShot( 250, this, SLOT(showStopInMarble()) );
}

void PublicTransport::marbleFinished( int /*exitCode*/ )
{
    kDebug() << "Marble finished";
    m_marble = 0;
}

void PublicTransport::showStopInMarble( qreal lon, qreal lat )
{
    if ( !m_marble ) {
        kDebug() << "No marble process?";
        return;
    }

    if ( lon < 0 || lat < 0 ) {
        lon = m_longitude;
        lat = m_latitude;
    }

    kDebug() << lon << lat;
    QString destination = QString("org.kde.marble-%1").arg(m_marble->pid());

    // Load OpenStreetMap
    QDBusMessage m1 = QDBusMessage::createMethodCall(destination,
            "/MarbleMap", "org.kde.MarbleMap", "setMapThemeId");
    m1 << "earth/openstreetmap/openstreetmap.dgml";
    if ( !QDBusConnection::sessionBus().send(m1) ) {
        showMessage( KIcon("marble"), i18nc("@info", "Couldn't interact with 'marble' "
                "(DBus: %1).", m1.errorMessage()), Plasma::ButtonOk );
    }

    // Center on the stops coordinates
    QDBusMessage m2 = QDBusMessage::createMethodCall(destination,
            "/MarbleMap", "org.kde.MarbleMap", "centerOn"); // centerOn( lon, lat )
    m2 << lon << lat;
    if ( !QDBusConnection::sessionBus().send(m2) ) {
        showMessage( KIcon("marble"), i18nc("@info", "Couldn't interact with 'marble' "
                "(DBus: %1).", m2.errorMessage()), Plasma::ButtonOk );
    }

    // Set zoom factor
    QDBusMessage m3 = QDBusMessage::createMethodCall(destination,
            "/MarbleMap", "org.kde.MarbleMap", "zoomView");
    m3 << 3080;
    if ( !QDBusConnection::sessionBus().send(m3) ) {
        showMessage( KIcon("marble"), i18nc("@info", "Couldn't interact with 'marble' "
                "(DBus: %1).", m3.errorMessage()), Plasma::ButtonOk );
    }

    // Update map
    QDBusMessage m4 = QDBusMessage::createMethodCall(destination,
            "/MarbleMap", "org.kde.MarbleMap", "reload");
    if ( !QDBusConnection::sessionBus().send(m4) ) {
        showMessage( KIcon("marble"), i18nc("@info", "Couldn't interact with 'marble' "
                "(DBus: %1).", m4.errorMessage()), Plasma::ButtonOk );
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
    for ( int i = 0; i < m_settings.alarmSettingsList.count(); ++i ) {
        AlarmSettings alarmSettings = m_settings.alarmSettingsList[ i ];
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
    AlarmSettingsList newAlarmSettings = m_settings.alarmSettingsList;
    newAlarmSettings.removeAt( matchingAlarmSettings );
    removeAlarms( newAlarmSettings, QList<int>() << matchingAlarmSettings );

    if ( m_clickedItemIndex.isValid() ) {
        updatePopupIcon();
    }
}

void PublicTransport::removeAlarmForDeparture()
{
    removeAlarmForDeparture( m_clickedItemIndex.row() );
}

void PublicTransport::processAlarmCreationRequest( const QDateTime& departure,
        const QString& lineString, VehicleType vehicleType, const QString& target,
        QGraphicsWidget *item )
{
    Q_UNUSED( item );

    // Autogenerate an alarm that only matches the given departure
    AlarmSettings alarm;
    alarm.autoGenerated = true;
    alarm.affectedStops << m_settings.currentStopSettingsIndex;
    alarm.name = i18nc( "@info/plain Name for a new alarm, eg. requested using the context menu. "
                        "%1 is the departure time or the name of the used vehicle.",
                        "One-Time Alarm (%1)", departure.isValid() ? departure.toString()
                                               : Global::vehicleTypeToString(vehicleType) );

    // Add alarm filters
    if ( !departure.isNull() ) {
        alarm.filter << Constraint(FilterByDeparture, FilterEquals, departure);
    }
    if ( !lineString.isEmpty() ) {
        alarm.filter << Constraint(FilterByTransportLine, FilterEquals, lineString);
    }
    alarm.filter << Constraint(FilterByVehicleType, FilterIsOneOf, QVariantList() << vehicleType);
    if ( !target.isEmpty() ) {
        alarm.filter << Constraint(FilterByTarget, FilterEquals, target);
    }

    // Append new alarm in a copy of the settings. Then write the new settings.
    Settings settings = m_settings;
    settings.alarmSettingsList << alarm;
    writeSettings( settings );

    alarmCreated();
}

void PublicTransport::processAlarmDeletionRequest( const QDateTime& departure,
        const QString& lineString, VehicleType vehicleType, const QString& target,
        QGraphicsWidget* item)
{
    Q_UNUSED( item );

    // Autogenerate an alarm that only matches the given departure
    AlarmSettings alarm;
    alarm.autoGenerated = true;
    alarm.affectedStops << m_settings.currentStopSettingsIndex;
    if ( !departure.isNull() ) {
        alarm.filter << Constraint(FilterByDeparture, FilterEquals, departure);
    }
    if ( !lineString.isEmpty() ) {
        alarm.filter << Constraint(FilterByTransportLine, FilterEquals, lineString);
    }
    alarm.filter << Constraint(FilterByVehicleType, FilterIsOneOf, QVariantList() << vehicleType);
    if ( !target.isEmpty() ) {
        alarm.filter << Constraint(FilterByTarget, FilterEquals, target);
    }

    // Append new alarm in a copy of the settings. Then write the new settings.
    Settings settings = m_settings;
    for ( AlarmSettingsList::iterator it = settings.alarmSettingsList.begin();
          it != settings.alarmSettingsList.end(); ++it )
    {
        if ( it->equalsAutogeneratedAlarm(alarm) ) {
            settings.alarmSettingsList.erase( it );
            break;
        }
    }
    writeSettings( settings );

    updatePopupIcon();
}

void PublicTransport::createAlarmSettingsForDeparture( const QPersistentModelIndex &modelIndex,
                                                       bool onlyForCurrentWeekday )
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
    alarm.autoGenerated = true;
    alarm.affectedStops << m_settings.currentStopSettingsIndex;
    alarm.filter.append( Constraint(FilterByDeparture, FilterEquals, info.departure()) );
    alarm.filter.append( Constraint(FilterByTransportLine, FilterEquals, info.lineString()) );
    alarm.filter.append( Constraint(FilterByVehicleType, FilterIsOneOf,
                                    QVariantList() << info.vehicleType()) );
    alarm.filter.append( Constraint(FilterByTarget, FilterEquals, info.target()) );
    if ( onlyForCurrentWeekday ) {
        alarm.filter.append( Constraint(FilterByDayOfWeek, FilterIsOneOf,
                                        QVariantList() << QDate::currentDate().dayOfWeek()) );
        alarm.name = i18nc( "@info/plain Name of new automatically generated alarm filters. "
                            "%1 is the departure time, %2 is a day of the week.",
                            "One-Time Alarm (%1, every %2)", departureTime,
                            QDate::longDayName(QDate::currentDate().dayOfWeek()) );
    } else {
        alarm.name = i18nc( "@info/plain Name of new automatically generated alarm filters. "
                            "%1 is the departure time, %2 is the target.",
                            "One-Time Alarm (%1 to %2)", departureTime, info.target() );
    }

    // Append new alarm in a copy of the settings. Then write the new settings.
    Settings settings = m_settings;
    settings.alarmSettingsList << alarm;
    writeSettings( settings );

    // Add the new alarm to the list of alarms that match the given departure
    int index = settings.alarmSettingsList.count() - 1;
    info.matchedAlarms() << index;
    item->setDepartureInfo( info );
}

void PublicTransport::createAlarmForDeparture()
{
    createAlarmSettingsForDeparture( m_clickedItemIndex );
    alarmCreated();
}

void PublicTransport::createAlarmForDepartureCurrentWeekDay()
{
    createAlarmSettingsForDeparture( m_clickedItemIndex, true );
    alarmCreated();
}

void PublicTransport::alarmCreated()
{
    updatePopupIcon(); // TEST needed or already done in writeSettings?

    // Animate popup icon to show the alarm departure (at index -1)
    m_popupIcon->animateToAlarm();
}

void PublicTransport::alarmFired( DepartureItem* item, const AlarmSettings &alarmSettings )
{
    const DepartureInfo *departureInfo = item->departureInfo();
    QString sLine = departureInfo->lineString();
    QString sTarget = departureInfo->target();
    QDateTime predictedDeparture = departureInfo->predictedDeparture();
    int minsToDeparture = qCeil( QDateTime::currentDateTime().secsTo(predictedDeparture) / 60.0 );

    QString message;
    if ( minsToDeparture > 0 ) {
        // Departure is in the future
        if ( departureInfo->vehicleType() == Unknown ) {
            // Vehicle type is unknown
            message = i18ncp( "@info/plain %5: Name of the Alarm",
                              "%5: Line %2 to '%3' departs in %1 minute at %4",
                              "%5: Line %2 to '%3' departs in %1 minutes at %4",
                              minsToDeparture, sLine, sTarget,
                              predictedDeparture.toString("hh:mm"), alarmSettings.name );
        } else {
            // Vehicle type is known
            message = i18ncp( "@info/plain %2: Line string (e.g. 'U3'), %4: Vehicle type name "
                              "(e.g. tram, subway), %6: Name of the Alarm",
                              "%6: The %4 %2 to '%3' departs in %1 minute at %5",
                              "%6: The %4 %2 to '%3' departs in %1 minutes at %5",
                              minsToDeparture, sLine, sTarget,
                              Global::vehicleTypeToString(departureInfo->vehicleType()),
                              predictedDeparture.toString("hh:mm"), alarmSettings.name );
        }
    } else if ( minsToDeparture < 0 ) {
        // Has already departed
        if ( departureInfo->vehicleType() == Unknown ) {
            // Vehicle type is unknown
            message = i18ncp( "@info/plain %5: Name of the Alarm",
                              "%5: Line %2 to '%3' has departed %1 minute ago at %4",
                              "%5: Line %2 to '%3' has departed %1 minutes ago at %4",
                              -minsToDeparture, sLine, sTarget,
                              predictedDeparture.toString("hh:mm"), alarmSettings.name );
        } else {
            // Vehicle type is known
            message = i18ncp( "@info/plain %2: Line string (e.g. 'U3'), %4: Vehicle type name "
                              "(e.g. tram, subway), %6: Name of the Alarm",
                              "%6: The %4 %2 to '%3' has departed %1 minute ago at %5",
                              "%6: The %4 %2 to %3 has departed %1 minutes ago at %5",
                              -minsToDeparture, sLine, sTarget,
                              Global::vehicleTypeToString(departureInfo->vehicleType()),
                              predictedDeparture.toString("hh:mm"), alarmSettings.name );
        }
    } else {
        // Departs now
        if ( departureInfo->vehicleType() == Unknown ) {
            // Vehicle type is unknown
            message = i18nc( "@info/plain %4: Name of the Alarm",
                             "%4: Line %1 to '%2' departs now at %3",
                             sLine, sTarget, predictedDeparture.toString("hh:mm"),
                             alarmSettings.name );
        } else {
            // Vehicle type is known
            message = i18nc( "@info/plain %1: Line string (e.g. 'U3'), %3: Vehicle type name "
                             "(e.g. tram, subway), %5: Name of the Alarm",
                             "%5: The %3 %1 to '%2' departs now at %4", sLine, sTarget,
                             Global::vehicleTypeToString(departureInfo->vehicleType()),
                             predictedDeparture.toString("hh:mm"), alarmSettings.name );
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
    settings.alarmSettingsList = newAlarmSettings;
    writeSettings( settings );
}

QString PublicTransport::infoText()
{
    // Get information about the current service provider from the data engine
    const QVariantHash data = currentServiceProviderData();
    const QString shortUrl = data.isEmpty() ? "-" : data["shortUrl"].toString();
    const QString url = data.isEmpty() ? "-" : data["url"].toString();
    QString sLastUpdate = m_lastSourceUpdate.toString( "hh:mm" );
    if ( sLastUpdate.isEmpty() ) {
        sLastUpdate = i18nc( "@info/plain This is used as 'last data update' "
                             "text when there hasn't been any updates yet.", "none" );
    }

    // Plasma::Label sets it's height as if the label would show the HTML source.
    // Therefore the <nobr>'s are inserted to prevent using multiple lines unnecessarily
    const qreal minHeightForTwoLines = 250.0;
    const QString dataByTextLocalized = i18nc("@info/plain", "data by");
    const QString textNoHtml1 = QString( "%1: %2" )
            .arg( i18nc("@info/plain", "last update"), sLastUpdate );
    const QString dataByLinkHtml = QString( "<a href='%1'>%2</a>" ).arg( url, shortUrl );
    const QString textHtml2 = dataByTextLocalized + ": " + dataByLinkHtml;
    QFontMetrics fm( m_labelInfo->font() );
    const int widthLine1 = fm.width( textNoHtml1 );
    const int widthLine2 = fm.width( dataByTextLocalized + ": " + shortUrl );
    const int width = widthLine1 + fm.width( ", " ) + widthLine2;
    QSizeF size = m_graphicsWidget->size();
    if ( size.width() >= width ) {
        // Enough horizontal space to show the complete info text in one line
        return "<nobr>" + textNoHtml1 + ", " + textHtml2 + "</nobr>";
    } else if ( size.height() >= minHeightForTwoLines &&
                size.width() >= widthLine1 && size.width() >= widthLine2 )
    {
        // Not enough horizontal space to show the complete info text in one line,
        // but enough vertical space to break it into two lines, which both fit horizontally
        return "<nobr>" + textNoHtml1 + ",<br />" + textHtml2 + "</nobr>";
    } else if ( size.width() >= widthLine2 ) {
        // Do not show "last update" text, but credits info
        return "<nobr>" + textHtml2 + "</nobr>";
    } else {
        // Do not show "last update" text, but credits info, without "data by:" label
        return "<nobr>" + dataByLinkHtml + "</nobr>";
    }
}

QString PublicTransport::courtesyToolTip() const
{
    // Get courtesy information for the current service provider from the data engine
    QVariantHash data = currentServiceProviderData();
    QString credit, url;
    if ( !data.isEmpty() ) {
        credit = data["credit"].toString();
        url = data["url"].toString();
    }

    if ( credit.isEmpty() || url.isEmpty() ) {
        // No courtesy information given by the data engine
        return QString();
    } else {
        return i18nc( "@info/plain", "By courtesy of %1 (%2)", credit, url );
    }
}

void PublicTransport::fillModelJourney( const QList< JourneyInfo > &journeys )
{
    foreach( const JourneyInfo &journeyInfo, journeys ) {
        int row = m_modelJourneys->indexFromInfo( journeyInfo ).row();
        if ( row == -1 ) {
            // Journey wasn't in the model
            m_modelJourneys->addItem( journeyInfo );
        } else {
            // Update associated item in the model
            JourneyItem *item = static_cast<JourneyItem*>( m_modelJourneys->itemFromInfo( journeyInfo ) );
            m_modelJourneys->updateItem( item, journeyInfo );
        }
    }

    // Sort departures in the model.
    // They are most probably already sorted, but sometimes they are not
    m_modelJourneys->sort( ColumnDeparture );
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

    // Sort departures in the model.
    // They are most probably already sorted, but sometimes they are not
    m_model->sort( ColumnDeparture );
}

void GraphicsPixmapWidget::paint( QPainter* painter,
                                const QStyleOptionGraphicsItem* option, QWidget* )
{
    if ( !option->rect.isValid() ) {
        return;
    }
    painter->drawPixmap( option->rect, m_pixmap );
}

void PublicTransport::updateColorGroupSettings()
{
    if ( m_settings.colorize ) {
        // Generate color groups from existing departure data
        m_settings.adjustColorGroupSettingsCount();
        ColorGroupSettingsList colorGroups = m_settings.currentColorGroupSettings();
        ColorGroupSettingsList newColorGroups = ColorGroups::generateColorGroupSettingsFrom(
                departureInfos(true, 40), m_settings.departureArrivalListType );

        // Copy filterOut values from old color group settings
        for ( int i = 0; i < newColorGroups.count(); ++i ) {
            ColorGroupSettings &newColorGroup = newColorGroups[i];
            if ( colorGroups.hasColor(newColorGroup.color) ) {
                ColorGroupSettings colorGroup = colorGroups.byColor( newColorGroup.color );
                newColorGroup.filterOut = colorGroup.filterOut;
            }
        }
        m_model->setColorGroups( newColorGroups );
        m_departureProcessor->setColorGroups( newColorGroups );

        // Change color group settings in a copy of the Settings object
        // Then write the changed settings
        Settings settings = m_settings;
        settings.colorGroupSettingsList[ settings.currentStopSettingsIndex ] = newColorGroups;
        writeSettings( settings );
    } else {
        // Remove color groups if colorization was toggled off
        // or if stop/filter settings were changed (update color groups after data arrived)
        m_model->setColorGroups( ColorGroupSettingsList() );
        m_departureProcessor->setColorGroups( ColorGroupSettingsList() );
    }
}

QVariant PublicTransport::supportedJourneySearchState() const
{
    QObject *object = qobject_cast<QObject*>(
            m_currentServiceProviderFeatures.contains("JourneySearch")
            ? m_states["journeySearch"] : m_states["journeysUnsupportedView"] );
    return qVariantFromValue( object );
}

void PublicTransport::configureJourneySearches()
{
    // First let the settings object be updated, then update the menu based on the new settings
    QPointer<KDialog> dialog = new KDialog;
    dialog->setWindowTitle( i18nc("@title:window", "Configure Journey Searches") );
    dialog->setWindowIcon( KIcon("configure") );
    QVBoxLayout *l = new QVBoxLayout( dialog->mainWidget() );
    l->setMargin( 0 );

    QStyleOption option;
    initStyleOption( &option );
    const KIcon icon("favorites");

    // Add journey search list
    JourneySearchListView *journeySearchList = new JourneySearchListView( dialog->mainWidget() );
    journeySearchList->setEditTriggers( QAbstractItemView::DoubleClicked |
                                        QAbstractItemView::SelectedClicked |
                                        QAbstractItemView::EditKeyPressed |
                                        QAbstractItemView::AnyKeyPressed );

    // Create model for journey searches
    JourneySearchModel *model = new JourneySearchModel( dialog );
    QList< JourneySearchItem > journeySearches = m_settings.currentJourneySearches();
    for ( int i = 0; i < journeySearches.count(); ++i ) {
        const JourneySearchItem item = journeySearches[i];
        model->addJourneySearch( item.journeySearch(), item.name(), item.isFavorite() );
    }
    model->sort();
    journeySearchList->setModel( model );

    QLabel *label = new QLabel( i18nc("@label:listbox", "Favorite and recent journey searches "
                                      "for '%1':", currentServiceProviderData()["name"].toString()),
                                dialog->mainWidget() );
    label->setWordWrap( true );
    label->setBuddy( journeySearchList );

    l->addWidget( label );
    l->addWidget( journeySearchList );
    if ( dialog->exec() == KDialog::Accepted ) {
        journeySearchListUpdated( model->journeySearchItems() );
    }
}

#include "publictransport.moc"
