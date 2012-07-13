/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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
#include "publictransport_p.h"
#include "overlaywidget.h"
#include "journeysearchparser.h"
#include "journeysearchsuggestionwidget.h"
#include "journeysearchmodel.h"
#include "journeysearchlistview.h"
#include "settingsui.h"

// KDE includes
#include <KNotification>
#include <KSelectAction>
#include <KActionMenu>
#include <KPushButton>
#include <KMenu>
#include <KMimeTypeTrader>
#include <KMessageBox> // Currently only used to ask if marble should be installed (TODO should be done using Plasma::Applet::showMessage())

// Plasma includes
#include <Plasma/LineEdit>
#include <Plasma/PushButton>
#include <Plasma/Theme>
#include <Plasma/Animation>

// Qt includes
#include <QPainter>
#include <QClipboard>
#include <QDBusConnection> // DBus used for marble
#include <QDBusMessage>
#include <QTimer>
#include <QStandardItemModel>
#include <QParallelAnimationGroup>
#include <QGraphicsSceneEvent>
#include <qmath.h>

PublicTransportApplet::PublicTransportApplet( QObject *parent, const QVariantList &args )
        : Plasma::PopupApplet( parent, args ), d_ptr(new PublicTransportAppletPrivate(this))
{
    setBackgroundHints( StandardBackground );
    setAspectRatioMode( Plasma::IgnoreAspectRatio );
    setHasConfigurationInterface( true );
    setPreferredSize( 400, 300 );
}

PublicTransportApplet::~PublicTransportApplet()
{
    if ( hasFailedToLaunch() ) {
        // Do some cleanup here
    } else {
    }

    delete d_ptr;
}

void PublicTransportApplet::init()
{
    Q_D( PublicTransportApplet );

    // Create, initialize and connect objects
    d->init();

    // Set icon and text of the default "run associated application" action
    if ( QAction *runAction = action("run associated application") ) {
        runAction->setText( i18nc("@item:inmenu", "&Show in Web-Browser") );

        KService::Ptr offer = KMimeTypeTrader::self()->preferredService( "text/html" );
        if ( !offer.isNull() ) {
            runAction->setIcon( KIcon(offer->icon()) );
        }
    }

    // Check for network connectivity
    checkNetworkStatus();

    // Set popup icon
    if ( isIconified() ) {
        updatePopupIcon();
    } else {
        // Set a popup icon, because otherwise the applet collapses to an icon
        // when shown in a desktop
        setPopupIcon( "public-transport-stop" );
    }

    connect( this, SIGNAL(geometryChanged()), this, SLOT(geometryChanged()) );
//     connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
    connect( Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()),
             this, SLOT(themeChanged()) );
    emit settingsChanged();
    d->onServiceProviderSettingsChanged();
}

void PublicTransportApplet::themeChanged()
{
    Q_D( PublicTransportApplet );
    d->applyTheme();
}

bool PublicTransportApplet::checkNetworkStatus()
{
    QString status = queryNetworkStatus();
    if ( status == QLatin1String("unavailable") ) {
        emit networkConnectionLost();
        return false;
    } else if ( status == QLatin1String("configuring") ) {
        emit networkIsConfiguring();
        return false;
    } else if ( status == QLatin1String("activated") /*&& m_currentMessage == MessageError TODO*/ ) {
        emit networkIsActivated();
        return false;
    } else {
        kDebug() << "Unknown network status or no error message was shown" << status;
        return true;
    }
}

QString PublicTransportApplet::queryNetworkStatus()
{
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

        if ( sStatus == QLatin1String("Activated") ) {
            status = "activated";
            break;
        } else if ( sStatus == QLatin1String("Configuring") ) {
            status = "configuring";
        }
    }

    return status;
}

void PublicTransportApplet::setSettings( const QString& serviceProviderID, const QString& stopName )
{
    Q_D( const PublicTransportApplet );

    // Set stop settings in a copy of the current settings.
    // Then write the new settings.
    Settings settings = d->settings;
    StopSettings stop;
    stop.set( ServiceProviderSetting, serviceProviderID );
    stop.setStop( stopName );
    StopSettingsList stopList;
    stopList << stop;
    settings.setStops( stopList );
    setSettings( settings );
}

void PublicTransportApplet::setSettings( const StopSettingsList& stops,
                                   const FilterSettingsList& filters )
{
    Q_D( const PublicTransportApplet );

    // Set settings in a copy of the current settings.
    // Then write the new settings.
    Settings settings = d->settings;
    settings.setStops( stops );
    settings.setFilters( filters );
    setSettings( settings );
}

void PublicTransportApplet::noItemsTextClicked()
{
    Q_D( const PublicTransportApplet );

    // Update the timetable if an error message inside the tree view has been clicked
    if ( !d->isStateActive("networkActivated") ) {
        updateDataSource();
    }
}

void PublicTransportApplet::setupActions()
{
    Q_D( PublicTransportApplet );

    KAction *actionUpdate = new KAction( KIcon("view-refresh"),
                                         i18nc("@action:inmenu", "&Update timetable"), this );
    connect( actionUpdate, SIGNAL(triggered()), this, SLOT(updateDataSource()) );
    addAction( "updateTimetable", actionUpdate );

    KAction *showActionButtons = new KAction( /*KIcon("system-run"),*/ // TODO: better icon
            i18nc("@action", "&Quick Actions"), this );
    addAction( "showActionButtons", showActionButtons );

    KAction *actionCreateAlarmForDeparture = new KAction(
            GlobalApplet::makeOverlayIcon( KIcon("task-reminder"), "list-add" ),
            d->settings.departureArrivalListType() == DepartureList
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
            d->settings.departureArrivalListType() == DepartureList
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
    d->updateJourneyMenu();
    d->titleWidget->setJourneysAction( actionJourneys );

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

    d->filtersGroup = new QActionGroup( this );
    d->filtersGroup->setExclusive( false );
    connect( d->filtersGroup, SIGNAL(triggered(QAction*)),
             this, SLOT(switchFilterConfiguration(QAction*)) );

    d->colorFiltersGroup = new QActionGroup( this );
    d->colorFiltersGroup->setExclusive( false );
    connect( d->colorFiltersGroup, SIGNAL(triggered(QAction*)),
             this, SLOT(switchFilterByGroupColor(QAction*)) );

    KActionMenu *actionFilterConfiguration = new KActionMenu( KIcon("view-filter"),
            i18nc("@action", "Filter"), this );
    addAction( "filterConfiguration", actionFilterConfiguration );
    d->titleWidget->setFiltersAction( actionFilterConfiguration );

    KAction *actionToggleExpanded = new KAction( KIcon( "arrow-down" ),
            i18nc( "@action:inmenu", "&Show Additional Information" ), this );
    connect( actionToggleExpanded, SIGNAL(triggered()), this, SLOT(toggleExpanded()) );
    addAction( "toggleExpanded", actionToggleExpanded );

    KAction *actionUnhighlightStop = new KAction( KIcon("edit-select"),
            i18nc("@action:inmenu", "&Unhighlight All Stops"), this );
    connect( actionUnhighlightStop, SIGNAL(triggered()), d->model, SLOT(setHighlightedStop()) );
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

QList< QAction* > PublicTransportApplet::contextualActions()
{
    Q_D( const PublicTransportApplet );

    QAction *switchDepArr = d->settings.departureArrivalListType() == DepartureList
            ? action( "showArrivals" ) : action( "showDepartures" );

    // Add filter action if there is at least one filter or color group
    KAction *actionFilter = 0;
    if ( !d->settings.filters().isEmpty() &&
         !d->settings.colorGroups().isEmpty() )
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
    if ( d->currentServiceProviderFeatures.contains("Arrivals") ) {
        actions << switchDepArr;
    }

    // When in intermediate departureview add an action to go back to the original stop
    // Otherwise add an action to switch the current stop and a journey action if supported
    if ( d->isStateActive("intermediateDepartureView") ) {
        QAction *goBackAction = action("backToDepartures");
        goBackAction->setText( i18nc("@action:inmenu", "&Back To Original Stop") );
        actions << goBackAction;
    } else if ( d->settings.stops().count() > 1 ) {
        actions << d->createSwitchStopAction( this );
        if ( d->currentServiceProviderFeatures.contains("JourneySearch") ) {
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

void PublicTransportApplet::updateDataSource()
{
    Q_D( PublicTransportApplet );
    if ( d->isStateActive("journeyView") ) {
        d->reconnectJourneySource();
    } else {
        d->reconnectSource();
    }
}

void PublicTransportApplet::departuresFiltered( const QString& sourceName,
        const QList< DepartureInfo > &departures,
        const QList< DepartureInfo > &newlyFiltered,
        const QList< DepartureInfo > &newlyNotFiltered )
{
    Q_D( PublicTransportApplet );

    if ( d->departureInfos.contains(sourceName) ) {
        d->departureInfos[ sourceName ] = departures;
    } else {
        kDebug() << "Source name not found" << sourceName << "in" << d->departureInfos.keys();
        return;
    }

    // Remove previously visible and now filtered out departures
    kDebug() << "Remove" << newlyFiltered.count() << "previously unfiltered departures, if they are visible";
    foreach( const DepartureInfo &departureInfo, newlyFiltered ) {
        int row = d->model->indexFromInfo( departureInfo ).row();
        if ( row == -1 ) {
            kDebug() << "Didn't find departure" << departureInfo;
        } else {
            d->model->removeItem( d->model->itemFromInfo(departureInfo) );
        }
    }

    // Append previously filtered out departures
    kDebug() << "Add" << newlyNotFiltered.count() << "previously filtered departures";
    foreach( const DepartureInfo &departureInfo, newlyNotFiltered ) {
        d->model->addItem( departureInfo );
    }

    // Limit item count to the maximal number of departure setting
    int delta = d->model->rowCount() - d->settings.maximalNumberOfDepartures();
    if ( delta > 0 ) {
        d->model->removeRows( d->settings.maximalNumberOfDepartures(), delta );
    }

    d->popupIcon->createDepartureGroups();
    updatePopupIcon();
    d->createTooltip();
    d->updateColorGroupSettings();
}

void PublicTransportApplet::beginJourneyProcessing( const QString &/*sourceName*/ )
{
    Q_D( PublicTransportApplet );

    // Clear old journey list
    d->journeyInfos.clear();
}

void PublicTransportApplet::journeysProcessed( const QString &/*sourceName*/,
        const QList< JourneyInfo > &journeys, const QUrl &requestUrl,
        const QDateTime &/*lastUpdate*/ )
{
    Q_D( PublicTransportApplet );

    // Set associated app url
    d->urlJourneys = requestUrl;
    setAssociatedApplicationUrlForJourneys();

    // Append new journeys
    kDebug() << journeys.count() << "journeys received from thread";
    d->journeyInfos << journeys;

    // Fill the model with the received journeys
    d->fillModelJourney( journeys );
}

void PublicTransportApplet::beginDepartureProcessing( const QString& sourceName )
{
    Q_D( PublicTransportApplet );

    // Clear old departure / arrival list
    QString strippedSourceName = d->stripDateAndTimeValues( sourceName );
    d->departureInfos[ strippedSourceName ].clear();
}

void PublicTransportApplet::departuresProcessed( const QString& sourceName,
        const QList< DepartureInfo > &departures, const QUrl &requestUrl,
        const QDateTime &lastUpdate, int departuresToGo )
{
    Q_D( PublicTransportApplet );

    // Set associated app url
    d->urlDeparturesArrivals = requestUrl;
    if ( d->isStateActive("departureView") || d->isStateActive("journeySearch") ||
         d->isStateActive("journeysUnsupportedView") )
    {
        setAssociatedApplicationUrlForDepartures();
    }

    // Put departures into the cache
    const QString strippedSourceName = d->stripDateAndTimeValues( sourceName );
    d->departureInfos[ strippedSourceName ] << departures;

    // Remove config needed messages
    setConfigurationRequired( false );

    // Update "last update" time
    if ( lastUpdate > d->lastSourceUpdate ) {
        d->lastSourceUpdate = lastUpdate;
    }
    d->labelInfo->setText( d->infoText() );

    // Fill the model with the received departures
    d->fillModel( departures );

    // Update everything that might have changed when all departure data is there
    if ( departuresToGo == 0 ) {
        d->updateColorGroupSettings();
        d->popupIcon->createDepartureGroups();
        updatePopupIcon();
        d->createTooltip();
    }
}

void PublicTransportApplet::handleDataError( const QString& /*sourceName*/,
                                       const Plasma::DataEngine::Data& data )
{
    Q_D( PublicTransportApplet );

    if ( data["parseMode"].toString() == QLatin1String("journeys") ) {
        emit invalidJourneyDataReceived();

        // Set associated application url
        d->urlJourneys = data["requestUrl"].toUrl();
        kDebug() << "Errorneous journey url" << d->urlJourneys;
        if ( d->isStateActive("journeyView") ) {
            setAssociatedApplicationUrlForJourneys();
        }
    } else if ( data["parseMode"].toString() == QLatin1String("departures") ) {
        emit invalidDepartureDataReceived();

        // Set associated application url
        d->urlDeparturesArrivals = data["requestUrl"].toUrl();
        kDebug() << "Errorneous departure/arrival url" << d->urlDeparturesArrivals;
        if ( d->isStateActive("departureView") || d->isStateActive("journeySearch") ||
             d->isStateActive("journeysUnsupportedView") )
        {
            setAssociatedApplicationUrlForDepartures();
        }

        QString error = data["errorString"].toString();
        if ( error.isEmpty() ) {
            if ( d->isStateActive("networkActivated") ) {
                if ( d->settings.departureArrivalListType() == DepartureList ) {
                    setConfigurationRequired( true, i18nc("@info", "Error parsing "
                            "departure information or currently no departures") );
                } else {
                    setConfigurationRequired( true, i18nc("@info", "Error parsing "
                            "arrival information or currently no arrivals") );
                }
            }
        } else if ( checkNetworkStatus() ) {
            if ( d->currentServiceProviderData()["type"] == QLatin1String("GTFS") ) {
                d->timetable->setNoItemsText( i18nc("@info/plain",
                        "There was an error:<nl/><message>%1</message><nl/><nl/>"
                        "The GTFS feed database may need to be updated. Please wait.", error) );
            } else {
                d->timetable->setNoItemsText( i18nc("@info/plain",
                        "There was an error:<nl/><message>%1</message><nl/><nl/>"
                        "The server may be temporarily unavailable.", error) );
            }
        }
    }
}

void PublicTransportApplet::processStopSuggestions( const QString &/*sourceName*/,
                                              const Plasma::DataEngine::Data &data )
{
    Q_D( PublicTransportApplet );

    bool journeyData = data["parseMode"].toString() == QLatin1String("journeys");
    if ( journeyData || data["parseMode"].toString() == QLatin1String("stopSuggestions") ) {
        if ( journeyData ) {
            emit invalidJourneyDataReceived();
        }

        d->listStopSuggestions->updateStopSuggestionItems( data );
    } else if ( data["parseMode"].toString() == QLatin1String("departures") /*&& m_currentMessage == MessageNone TODO*/ ) {
        emit invalidDepartureDataReceived();
        d->clearDepartures();
        setConfigurationRequired( true, i18nc("@info", "The stop name is ambiguous.") );
    }
}

void PublicTransportApplet::processOsmData( const QString& sourceName,
                                      const Plasma::DataEngine::Data& data )
{
    Q_D( PublicTransportApplet );

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
        d->longitude = longitude;
        d->latitude = latitude;

        // Start marble
        if ( d->marble ) {
            // Marble already started
            QString destination = QString("org.kde.marble-%1").arg(d->marble->pid());

            // Set new window title
            QDBusMessage m1 = QDBusMessage::createMethodCall(destination,
                    "/marble/MainWindow_1", "org.kde.marble.KMainWindow", "setPlainCaption");
            m1 << i18nc("@title:window Caption for marble windows started to show a stops "
                    "position in a map. %1 is the stop name.", "\"PublicTransport: %1\"",
                    name);
            if ( !QDBusConnection::sessionBus().send(m1) ) {
                kDebug() << "Couldn't set marble title with dbus" << m1.errorMessage();
            }

            showStopInMarble( d->longitude, d->latitude );
        } else {
            QString command = "marble --caption " + i18nc("@title:window Caption for "
                    "marble windows started to show a stops position in a map. %1 is the "
                    "stop name.", "\"PublicTransport: %1\"", name);
            kDebug() << "Use this command to start marble:" << command;
            d->marble = new KProcess( this );
            d->marble->setProgram( "marble", QStringList() << "--caption"
                    << i18nc("@title:window Caption for "
                    "marble windows started to show a stops position in a map. %1 is the "
                    "stop name.", "\"PublicTransport: %1\"", name) );
            connect( d->marble, SIGNAL(error(QProcess::ProcessError)),
                    this, SLOT(errorMarble(QProcess::ProcessError)) );
            connect( d->marble, SIGNAL(started()), this, SLOT(marbleHasStarted()) );
            connect( d->marble, SIGNAL(finished(int)), this, SLOT(marbleFinished(int)) );
            d->marble->start();
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

void PublicTransportApplet::dataUpdated( const QString& sourceName,
                                   const Plasma::DataEngine::Data& data )
{
    Q_D( PublicTransportApplet );

    if ( sourceName.startsWith(QLatin1String("getCoords"), Qt::CaseInsensitive) ) {
        processOsmData( sourceName, data );
        return;
    }

    if ( data.isEmpty() || (!d->currentSources.contains(sourceName)
                            && sourceName != d->currentJourneySource) ) {
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
    } else if ( data["parseMode"].toString() == QLatin1String("journeys") ) {
        // List of journeys received
        emit validJourneyDataReceived();
        if ( d->isStateActive("journeyView") ) {
            d->departureProcessor->processJourneys( sourceName, data );
        } else {
            kDebug() << "Received journey data, but journey list is hidden.";
        }
    } else if ( data["parseMode"].toString() == QLatin1String("departures") ||
                data["parseMode"].toString() == QLatin1String("arrivals") )
    {
        // List of departures / arrivals received
        emit validDepartureDataReceived();
        d->departureProcessor->processDepartures( sourceName, data );
    }
}

void PublicTransportApplet::geometryChanged()
{
    Q_D( PublicTransportApplet );
    d->onResized();
}

void PublicTransportApplet::popupEvent( bool show )
{
    action("backToDepartures")->trigger();
    Plasma::PopupApplet::popupEvent( show );
}

void PublicTransportApplet::wheelEvent( QGraphicsSceneWheelEvent* event )
{
    Q_D( PublicTransportApplet );

    QGraphicsItem::wheelEvent( event );

    // Compute start and end indices of the departure groups to animate between
    if ( event->delta() > 0 ) {
        // Wheel rotated forward
        d->popupIcon->animateToNextGroup();
    } else if ( event->delta() < 0 ) {
        // Wheel rotated backward
        d->popupIcon->animateToPreviousGroup();
    } else {
        // Wheel not rotated?
        return;
    }
}

void PublicTransportApplet::departuresAboutToBeRemoved( const QList<ItemBase*>& departures )
{
    Q_D( PublicTransportApplet );

    d->popupIcon->departuresAboutToBeRemoved( departures );
    updatePopupIcon();
    d->createTooltip();
}

void PublicTransportApplet::departuresLeft( const QList< DepartureInfo > &departures )
{
    Q_UNUSED( departures );
}

void PublicTransportApplet::titleToggleAnimationFinished()
{
    Q_D( PublicTransportApplet );
    delete d->titleToggleAnimation;
    d->titleToggleAnimation = 0;
}

void PublicTransportApplet::updatePopupIcon()
{
    Q_D( PublicTransportApplet );

    if ( isIconified() ) {
        int iconSize = qMin( 128, int(size().width()) );
        setPopupIcon( d->popupIcon->createPopupIcon(QSize(iconSize, iconSize)) );
    }
}

void PublicTransportApplet::updateTooltip()
{
    Q_D( PublicTransportApplet );
    d->createTooltip();
}

// TODO
// void PublicTransportApplet::resized()
// {
//     Q_D( PublicTransportApplet );
//     d->onResized();
// }

void PublicTransportApplet::resizeEvent( QGraphicsSceneResizeEvent *event )
{
    Plasma::Applet::resizeEvent( event );

    // Update popup icon to new size
    updatePopupIcon();
}

// void PublicTransportApplet::configChanged()
// {
//     Q_D( PublicTransportApplet );
//
//     disconnect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
//     d->onUnknownSettingsChanged();
//     connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
// }

void PublicTransportApplet::destroyOverlay()
{
    Q_D( PublicTransportApplet );

    if ( d->overlay ) {
        d->overlay->destroy();
        d->overlay = 0;
    }
}

void PublicTransportApplet::showActionButtons()
{
    Q_D( PublicTransportApplet );

    // Create an overlay widget with gets placed over the applet
    // and then filled with buttons
    d->overlay = new OverlayWidget( d->graphicsWidget, d->mainGraphicsWidget );
    d->overlay->setGeometry( d->graphicsWidget->contentsRect() );
    d->overlay->setOpacity( 0 );

    // Create a layout for the buttons and add a spacer at the top
    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout( Qt::Vertical );
    layout->setContentsMargins( 15, 10, 15, 10 );
    QGraphicsWidget *spacer = new QGraphicsWidget( d->overlay );
    spacer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    layout->addItem( spacer );

    // Add actions depending on active states / service provider features
    QList<QAction*> actions;
    if ( d->isStateActive("journeyView") ) {
        actions << action("backToDepartures");
    }
    if ( d->currentServiceProviderFeatures.contains("Arrivals") ) {
        actions << (d->settings.departureArrivalListType() == DepartureList
                ? action("showArrivals") : action("showDepartures"));
    }
    if ( d->currentServiceProviderFeatures.contains("JourneySearch") ) {
//         updateJourneyActionMenu();
        actions << action("journeys");
    }

    // Add stop selector if multiple stops are defined
    if ( d->settings.stops().count() > 1 ) {
        actions << d->createSwitchStopAction( 0, true ); // Parent gets set below
    }

    // Create buttons for the actions and create a list of fade animations
    foreach ( QAction *action, actions ) {
        Plasma::PushButton *button = new Plasma::PushButton( d->overlay );
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
    Plasma::PushButton *btnCancel = new Plasma::PushButton( d->overlay );
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
    QGraphicsWidget *spacer2 = new QGraphicsWidget( d->overlay );
    spacer2->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    layout->addItem( spacer2 );
    d->overlay->setLayout( layout );

    // Create a fade in animation for the whole overlay
    GlobalApplet::fadeAnimation( d->overlay, 1 )->start( QAbstractAnimation::DeleteWhenStopped );
}

void PublicTransportApplet::setCurrentStopIndex( QAction* stopAction )
{
    Q_D( const PublicTransportApplet );

    bool ok;
    int stopIndex = stopAction->data().toInt( &ok );
    if ( !ok ) {
        kDebug() << "Couldn't find stop index";
        return;
    }

    action("backToDepartures")->trigger();

    Settings settings = d->settings;
    settings.setCurrentStop( stopIndex );
    setSettings( settings );
}

void PublicTransportApplet::showDepartures()
{
    Q_D( const PublicTransportApplet );

    // Change departure arrival list type in a copy of the settings.
    // Then write the new settings.
    Settings settings = d->settings;
    settings.setDepartureArrivalListType( DepartureList );
    setSettings( settings );
}

void PublicTransportApplet::showArrivals()
{
    Q_D( const PublicTransportApplet );

    // Change departure arrival list type in a copy of the settings.
    // Then write the new settings.
    Settings settings = d->settings;
    settings.setDepartureArrivalListType( ArrivalList );
    setSettings( settings );
}

void PublicTransportApplet::switchFilterConfiguration( QAction* action )
{
    Q_D( const PublicTransportApplet );

    const QString filterConfig = KGlobal::locale()->removeAcceleratorMarker( action->text() );

    // Change filter configuration of the current stop in a copy of the settings.
    // Then write the new settings.
    Settings settings = d->settings;
    FilterSettingsList filters = settings.filters();
    for ( int i = 0; i < filters.count(); ++i ) {
        const FilterSettings filter = filters[i];
        if ( filter.name == filterConfig ) {
            // Switch filter configuration for current stop settings
            if ( filter.affectedStops.contains(settings.currentStopIndex()) ) {
                filters[i].affectedStops.remove( settings.currentStopIndex() );
            } else if ( !filter.affectedStops.contains(settings.currentStopIndex()) ) {
                filters[i].affectedStops << settings.currentStopIndex();
            }
        }
    }
    settings.setFilters( filters );
    setSettings( settings );
}

void PublicTransportApplet::switchFilterByGroupColor( QAction* action )
{
    Q_D( const PublicTransportApplet );

    const QColor color = action->data().value<QColor>();
    const bool enable = action->isChecked();

    // Change filter configuration of the current stop in a copy of the settings.
    // Then write the new settings.
    Settings settings = d->settings;
    QList<ColorGroupSettingsList> colorGroups = settings.colorGroups();
    colorGroups[ settings.currentStopIndex() ].enableColorGroup( color, enable );
    settings.setColorGroups( colorGroups );
    setSettings( settings );
}

void PublicTransportApplet::enableFilterConfiguration( const QString& filterConfiguration, bool enable )
{
    Q_D( const PublicTransportApplet );

    const QString filterConfig = filterConfiguration;
    Q_ASSERT_X( d->settings.filters().hasName(filterConfig),
                "PublicTransportApplet::switchFilterConfiguration",
                QString("Filter '%1' not found!").arg(filterConfig).toLatin1().data() );

    // Change filter configuration of the current stop in a copy of the settings.
    // Then write the new settings.
    Settings settings = d->settings;
    FilterSettingsList filters = settings.filters();
    FilterSettings filter = filters.byName( filterConfig );
    if ( enable && !filter.affectedStops.contains(settings.currentStopIndex()) ) {
        filter.affectedStops << settings.currentStopIndex();
    } else if ( !enable && filter.affectedStops.contains(settings.currentStopIndex()) ) {
        filter.affectedStops.remove( settings.currentStopIndex() );
    }
    filters.set( filter );
    settings.setFilters( filters );
    setSettings( settings );
}

void PublicTransportApplet::showDepartureList()
{
    Q_D( PublicTransportApplet );

    d->fadeOutOldAppearance();
    d->titleWidget->setTitleType( ShowDepartureArrivalListTitle,
                                  d->isStateActive("departureDataValid"),
                                  d->isStateActive("journeyDataValid") );
    d->updateDepartureListIcon();
    d->updateInfoText();

    d->timetable->update();
    geometryChanged();
    setBusy( d->isStateActive("departureDataWaiting") && d->model->isEmpty() );

    showMainWidget( d->timetable );
    setAssociatedApplicationUrlForDepartures();
}

void PublicTransportApplet::showIntermediateDepartureList()
{
    Q_D( PublicTransportApplet );

    d->fadeOutOldAppearance();
    d->titleWidget->setTitleType( ShowIntermediateDepartureListTitle,
                                  d->isStateActive("departureDataValid"),
                                  d->isStateActive("journeyDataValid") );
    d->updateDepartureListIcon();
    d->updateInfoText();

    d->timetable->update();
    geometryChanged();
    setBusy( d->isStateActive("departureDataWaiting") && d->model->isEmpty() );

    showMainWidget( d->timetable );
    setAssociatedApplicationUrlForDepartures();
}

void PublicTransportApplet::showJourneyList()
{
    Q_D( PublicTransportApplet );

    d->fadeOutOldAppearance();
    d->titleWidget->setTitleType( ShowJourneyListTitle,
                                  d->isStateActive("departureDataValid"),
                                  d->isStateActive("journeyDataValid") );

    // Create timetable widget for journeys
    d->journeyTimetable = new JourneyTimetableWidget( d->settings.drawShadows()
            ? PublicTransportWidget::DrawShadowsOrHalos : PublicTransportWidget::NoOption, this );
    d->journeyTimetable->setModel( d->modelJourneys );
    d->journeyTimetable->setFont( d->settings.sizedFont() );
    d->journeyTimetable->setSvg( &d->vehiclesSvg );
    connect( d->journeyTimetable, SIGNAL(requestStopAction(StopAction::Type,QString,QString)),
             this, SLOT(requestStopAction(StopAction::Type,QString,QString)) );
    connect( d->journeyTimetable, SIGNAL(requestAlarmCreation(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)),
             this, SLOT(processAlarmCreationRequest(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)) );
    connect( d->journeyTimetable, SIGNAL(requestAlarmDeletion(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)),
             this, SLOT(processAlarmDeletionRequest(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)) );
    connect( d->states["journeyView"], SIGNAL(exited()),
             d->journeyTimetable, SLOT(deleteLater()) );
    d->journeyTimetable->setZoomFactor( d->settings.sizeFactor() );
    d->journeyTimetable->update();

    d->titleWidget->setTitle( d->journeyTitleText.isEmpty()
            ? i18nc("@info", "<emphasis strong='1'>Journeys</emphasis>")
            : d->journeyTitleText );
    geometryChanged();
    setBusy( d->isStateActive("journeyDataWaiting") && d->modelJourneys->isEmpty() );

    showMainWidget( d->journeyTimetable );
    setAssociatedApplicationUrlForJourneys();

    // Ensure the applet popup is shown
    showPopup();
}

void PublicTransportApplet::showJourneySearch()
{
    Q_D( PublicTransportApplet );

    d->fadeOutOldAppearance();
    d->titleWidget->setTitleType( ShowSearchJourneyLineEdit,
            d->isStateActive("departureDataValid"), d->isStateActive("journeyDataValid") );

    Plasma::LineEdit *journeySearch =
            d->titleWidget->castedWidget<Plasma::LineEdit>( TitleWidget::WidgetJourneySearchLine );
    Q_ASSERT( journeySearch );

    d->listStopSuggestions = new JourneySearchSuggestionWidget(
            this, &d->settings, palette() );
    d->listStopSuggestions->attachLineEdit( journeySearch );
    connect( d->listStopSuggestions, SIGNAL(journeySearchLineChanged(QString,QDateTime,bool,bool)),
             this, SLOT(journeySearchLineChanged(QString,QDateTime,bool,bool)) );
    connect( d->states["journeySearch"], SIGNAL(exited()),
             d->listStopSuggestions, SLOT(deleteLater()) );

    // Hide journey search action, because it switches to the currently active state
    action("searchJourneys")->setVisible( false );

    showMainWidget( d->listStopSuggestions );
    setBusy( false );

    // Ensure the applet popup is shown
    showPopup();
}

void PublicTransportApplet::exitJourneySearch()
{
    // Show journey search action again
    action("searchJourneys")->setVisible( true );
}

void PublicTransportApplet::showJourneysUnsupportedView()
{
    Q_D( PublicTransportApplet );

    d->fadeOutOldAppearance();
    d->titleWidget->setTitleType( ShowSearchJourneyLineEditDisabled,
            d->isStateActive("departureDataValid"), d->isStateActive("journeyDataValid") );

    d->labelJourneysNotSupported = new Plasma::Label;
    d->labelJourneysNotSupported->setAlignment( Qt::AlignCenter );
    d->labelJourneysNotSupported->setSizePolicy( QSizePolicy::Expanding,
            QSizePolicy::Expanding, QSizePolicy::Label );
    d->labelJourneysNotSupported->setText( i18nc("@info/plain",
            "Journey searches aren't supported by the currently used service provider.") );
    d->labelJourneysNotSupported->nativeWidget()->setWordWrap( true );
    connect( d->states["journeysUnsupportedView"], SIGNAL(exited()),
             d->labelJourneysNotSupported, SLOT(deleteLater()) );

    showMainWidget( d->labelJourneysNotSupported );
    setBusy( false );

    // Ensure the applet popup is shown,
    // but only for a few seconds as this just shows an error message
    showPopup( 3000 );
}

void PublicTransportApplet::journeySearchInputFinished( const QString &text )
{
    Q_D( PublicTransportApplet );

    d->clearJourneys();

    // Add journey search line to the list of recently used journey searches
    // and cut recent journey searches if the limit is exceeded
    Settings settings = d->settings;
    settings.addRecentJourneySearch( text );
    setSettings( settings );

    d->journeyTitleText.clear();
    QString stop;
    QDateTime departure;
    bool stopIsTarget, timeIsDeparture;
    Plasma::LineEdit *journeySearch =
            d->titleWidget->castedWidget<Plasma::LineEdit>( TitleWidget::WidgetJourneySearchLine );
//     Q_ASSERT( journeySearch ); // May be 0 here, TODO use new journey search parser
    JourneySearchParser::parseJourneySearch( !journeySearch ? 0 : journeySearch->nativeWidget(),
            text, &stop, &departure, &stopIsTarget, &timeIsDeparture );

    d->reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture );
    emit journeySearchFinished();
}

void PublicTransportApplet::journeySearchLineChanged( const QString& stopName,
        const QDateTime& departure, bool stopIsTarget, bool timeIsDeparture )
{
    Q_D( PublicTransportApplet );
    d->reconnectJourneySource( stopName, departure, stopIsTarget, timeIsDeparture, true );
}

QGraphicsWidget* PublicTransportApplet::graphicsWidget()
{
    Q_D( PublicTransportApplet );
    return d->graphicsWidget;
}

bool PublicTransportApplet::sceneEventFilter( QGraphicsItem* watched, QEvent* event )
{
    Q_D( const PublicTransportApplet );

    if ( watched == d->labelInfo && event->type() == QEvent::GraphicsSceneMousePress ) {
        return true; // To make links clickable, otherwise Plasma takes all clicks to move the applet
    }

    return Plasma::Applet::sceneEventFilter( watched, event );
}

bool PublicTransportApplet::eventFilter( QObject *watched, QEvent *event )
{
    Q_D( PublicTransportApplet );

    Plasma::LineEdit *journeySearch =
            d->titleWidget->castedWidget<Plasma::LineEdit>( TitleWidget::WidgetJourneySearchLine );
    if ( watched && watched == journeySearch && d->isStateActive("journeySearch")
        && d->listStopSuggestions->model()
        && d->listStopSuggestions->model()->rowCount() > 0 )
    {
        QKeyEvent *keyEvent;
        QModelIndex curIndex;
        int row;
        switch ( event->type() ) {
        case QEvent::KeyPress:
            keyEvent = dynamic_cast<QKeyEvent*>( event );
            curIndex = d->listStopSuggestions->currentIndex();

            if ( keyEvent->key() == Qt::Key_Up ) {
                if ( !curIndex.isValid() ) {
                    curIndex = d->listStopSuggestions->model()->index( 0, 0 );
                    d->listStopSuggestions->setCurrentIndex( curIndex );
//                     possibleStopItemActivated( curIndex );
                    d->listStopSuggestions->useStopSuggestion( curIndex );
                    return true;
                } else {
                    row = curIndex.row();
                    if ( row >= 1 ) {
                        d->listStopSuggestions->setCurrentIndex(
                                d->listStopSuggestions->model()->index(row - 1,
                                curIndex.column(), curIndex.parent()) );
                        d->listStopSuggestions->useStopSuggestion(
                                d->listStopSuggestions->currentIndex() );
                        return true;
                    } else {
                        return false;
                    }
                }
            } else if ( keyEvent->key() == Qt::Key_Down ) {
                if ( !curIndex.isValid() ) {
                    curIndex = d->listStopSuggestions->model()->index( 0, 0 );
                    d->listStopSuggestions->setCurrentIndex( curIndex );
                    d->listStopSuggestions->useStopSuggestion( curIndex );
                    return true;
                } else {
                    row = curIndex.row();
                    if ( row < d->listStopSuggestions->model()->rowCount() - 1 ) {
                        d->listStopSuggestions->setCurrentIndex(
                                d->listStopSuggestions->model()->index(row + 1,
                                curIndex.column(), curIndex.parent()) );
                        d->listStopSuggestions->useStopSuggestion(
                                d->listStopSuggestions->currentIndex() );
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

void PublicTransportApplet::createConfigurationInterface( KConfigDialog* parent )
{
    Q_D( const PublicTransportApplet );

    // Go back from intermediate departure list (which may be requested by a
    // context menu action) before showing the configuration dialog,
    // because stop settings may be changed and the intermediate stop
    // shouldn't be shown in the configuration dialog.
    if ( d->isStateActive("intermediateDepartureView") ) {
        showDepartureList();
    }

    SettingsUiManager *settingsUiManager = new SettingsUiManager(
            d->settings, dataEngine("publictransport"),
            dataEngine("openstreetmap"), dataEngine("favicons"),
            dataEngine("geolocation"), parent );
    connect( settingsUiManager, SIGNAL(settingsAccepted(Settings)),
             this, SLOT(setSettings(Settings)) );
    connect( d->model, SIGNAL(updateAlarms(AlarmSettingsList,QList<int>)),
             settingsUiManager, SLOT(removeAlarms(AlarmSettingsList,QList<int>)) );
}

void PublicTransportApplet::setSettings( const Settings& settings )
{
    Q_D( PublicTransportApplet );

    SettingsIO::ChangedFlags changed =
            SettingsIO::writeSettings( settings, d->settings, config(), globalConfig() );
    d->onSettingsChanged( settings, changed );
}

void PublicTransportApplet::showMainWidget( QGraphicsWidget* mainWidget )
{
    Q_D( PublicTransportApplet );

    // Setup new main layout
    QGraphicsLinearLayout *layoutMainNew = new QGraphicsLinearLayout(
            Qt::Vertical, d->mainGraphicsWidget );
    layoutMainNew->setContentsMargins( 0, 0, 0, 0 );
    layoutMainNew->setSpacing( 0 );
    d->timetable->setVisible( d->isStateActive("departureView") ||
                              d->isStateActive("intermediateDepartureView") );

    // Add widgets to new layout
    layoutMainNew->addItem( d->titleWidget );
    layoutMainNew->addItem( mainWidget );
    layoutMainNew->addItem( d->labelInfo );
    layoutMainNew->setAlignment( d->labelInfo, Qt::AlignRight | Qt::AlignVCenter );
}

void PublicTransportApplet::oldItemAnimationFinished()
{
    Q_D( PublicTransportApplet );
    d->onOldItemAnimationFinished();
}

void PublicTransportApplet::processJourneyRequest( const QString& stop, bool stopIsTarget )
{
    Q_D( PublicTransportApplet );
    d->clearJourneys();
    d->reconnectJourneySource( stop, QDateTime(), stopIsTarget, true );
}

void PublicTransportApplet::journeySearchListUpdated( const QList<JourneySearchItem> &newJourneySearches )
{
    Q_D( const PublicTransportApplet );
    Settings settings = d->settings;
    settings.setCurrentJourneySearches( newJourneySearches );
    setSettings( settings );
}

void PublicTransportApplet::journeyActionTriggered( QAction *_action )
{
    Q_D( PublicTransportApplet );

    // The configure action has no data, only quick journey search items get the
    // journey search string as data
    if ( _action->data().isValid() ) {
        // The given action is not the configure action
        const QString journeySearch = KGlobal::locale()->removeAcceleratorMarker(
                _action->data().toString() );

        if ( d->isStateActive("journeySearch") ) {
            // If in journey search view, put the selected journey search into the input line edit
            d->titleWidget->setJourneySearch( journeySearch );
        } else {
            // Go directly to the journey results view
            journeySearchInputFinished( journeySearch );
        }
    }
}

void PublicTransportApplet::departureDataStateChanged()
{
    Q_D( PublicTransportApplet );
    d->onDepartureDataStateChanged();
}

void PublicTransportApplet::journeyDataStateChanged()
{
    Q_D( PublicTransportApplet );
    d->onJourneyDataStateChanged();
}

void PublicTransportApplet::disconnectJourneySource()
{
    Q_D( PublicTransportApplet );
    d->disconnectJourneySource();
}

void PublicTransportApplet::removeIntermediateStopSettings()
{
    Q_D( PublicTransportApplet );

    // Remove intermediate stop settings
    Settings settings = d->settings;
    settings.removeIntermediateStops();

    if ( d->originalStopIndex != -1 ) {
        settings.setCurrentStop( qBound(0, d->originalStopIndex, settings.stops().count() - 1) );
    }
    d->originalStopIndex = -1;

    setSettings( settings );
}

void PublicTransportApplet::hideColumnTarget()
{
    Q_D( const PublicTransportApplet );

    // Change hide column target setting in a copy of the settings.
    // Then write the new settings.
    Settings settings = d->settings;
    settings.setHideTargetColumn( true );
    setSettings( settings );
}

void PublicTransportApplet::showColumnTarget()
{
    Q_D( const PublicTransportApplet );

    // Change hide column target setting in a copy of the settings.
    // Then write the new settings.
    Settings settings = d->settings;
    settings.setHideTargetColumn( false );
    setSettings( settings );
}

void PublicTransportApplet::toggleExpanded()
{
    Q_D( PublicTransportApplet );
    if ( d->journeyTimetable && d->isStateActive("journeyView") ) {
        d->journeyTimetable->item( d->clickedItemIndex.row() )->toggleExpanded();
    } else {
        d->timetable->item( d->clickedItemIndex.row() )->toggleExpanded();
    }
}

QAction* PublicTransportApplet::updatedAction( const QString& actionName )
{
    Q_D( const PublicTransportApplet );

    QAction *a = action( actionName );
    if ( !a ) {
        kDebug() << "Action not found:" << actionName;
        return 0;
    }

    if ( actionName == QLatin1String("toggleExpanded") ) {
        if ( (d->journeyTimetable && d->isStateActive("journeyView"))
            ? d->journeyTimetable->item(d->clickedItemIndex.row())->isExpanded()
            : d->timetable->item(d->clickedItemIndex.row())->isExpanded() )
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

void PublicTransportApplet::departureContextMenuRequested( PublicTransportGraphicsItem* item, const QPointF& pos )
{
    Q_UNUSED( pos );
    Q_D( PublicTransportApplet );

    // Save the index of the clicked item
    d->clickedItemIndex = item->index();

    // List actions for the context menu
    QAction *infoAction = 0;
    QObjectList objectsToBeDeleted;
    QList<QAction *> actions;
    actions.append( updatedAction("toggleExpanded") );
    if ( d->isStateActive("departureView") || d->isStateActive("intermediateDepartureView") ) {
        DepartureItem *item = static_cast<DepartureItem*>( d->model->item(d->clickedItemIndex.row()) );

        // Add alarm actions (but not for departures in an intermediate departure list)
        if ( d->isStateActive("departureView") ) {
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

        if ( !d->model->info().highlightedStop.isEmpty() ) {
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
                const QString stopName = info->routeStops()[index];
                const QString stopNameShortened = info->routeStopsShortened()[index];

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
                    stopText = stopNameShortened;
                } else {
                    stopText = QString("%1 (%2)").arg(stopNameShortened)
                        .arg(KGlobal::locale()->prettyFormatDuration(minsFromFirstRouteStop * 60000));
                }
                QAction *routeStopsAction = new QAction( GlobalApplet::stopIcon(routeStopFlags),
                                                         stopText, stopMenu );

                if ( !routeStopFlags.testFlag(RouteStopIsHomeStop) ) {
                    StopAction *showDeparturesAction =
                            new StopAction( StopAction::ShowDeparturesForStop, stopMenu );
                    showDeparturesAction->setStopName( stopName, stopNameShortened );
                    connect( showDeparturesAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
                             this, SLOT(requestStopAction(StopAction::Type,QString,QString)) );
                    stopMenu->addAction( showDeparturesAction );
                }

                if ( dataEngine("openstreetmap")->isValid() ) {
                    StopAction *showInMapAction =
                            new StopAction( StopAction::ShowStopInMap, stopMenu );
                    showInMapAction->setStopName( stopName, stopNameShortened );
                    connect( showInMapAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
                             this, SLOT(requestStopAction(StopAction::Type,QString,QString)) );
                    stopMenu->addAction( showInMapAction );
                }

                if ( !routeStopFlags.testFlag(RouteStopIsHomeStop) ) {
                    StopAction *highlightAction =
                            new StopAction( StopAction::HighlightStop, stopMenu );
                    highlightAction->setStopName( stopName, stopNameShortened );
                    if ( d->model->info().highlightedStop == stopName ) {
                        highlightAction->setText( i18nc("@action:inmenu", "&Unhighlight This Stop") );
                    }
                    connect( highlightAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
                             this, SLOT(requestStopAction(StopAction::Type,QString,QString)) );
                    stopMenu->addAction( highlightAction );

                    StopAction *createFilterAction =
                            new StopAction( StopAction::CreateFilterForStop, stopMenu );
                    createFilterAction->setStopName( stopName, stopNameShortened );
                    connect( createFilterAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
                             this, SLOT(requestStopAction(StopAction::Type,QString,QString)) );
                    stopMenu->addAction( createFilterAction );
                }

                StopAction *copyToClipboardAction =
                        new StopAction( StopAction::CopyStopNameToClipboard, stopMenu );
                copyToClipboardAction->setStopName( stopName, stopNameShortened );
                connect( copyToClipboardAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
                         this, SLOT(requestStopAction(StopAction::Type,QString,QString)) );
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

void PublicTransportApplet::requestStopAction( StopAction::Type stopAction,
                                         const QString& stopName, const QString &stopNameShortened )
{
    Q_D( PublicTransportApplet );

    // Create and enable new filter
    Settings settings = d->settings;

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
                                       "Via %1", stopNameShortened);
            Filter viaFilter;
            viaFilter << Constraint( FilterByVia, FilterContains, stopName );
            FilterSettings filter;
            filter.filters << viaFilter;
            filter.name = filterName;
            filter.affectedStops << settings.currentStopIndex();

            settings.appendFilter( filter );
            setSettings( settings );
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
            settings.removeIntermediateStops();

            if ( d->originalStopIndex != -1 ) {
                kDebug() << "Set current stop index to" << d->originalStopIndex;
                settings.setCurrentStop( qBound(0, d->originalStopIndex,
                        settings.stops().count() - 1) );
            }

            // Save original stop index from where sub requests were made
            // (using the context menu). Only if the departure list wasn't requested
            // already from a sub departure list.
            d->originalStopIndex = settings.currentStopIndex();

            // Search for a stop setting with the given stop name in it.
            // Create an intermediate stop item if there is no such stop setting
            // in the configuration (automatically deleted).
            int stopIndex = settings.stops().findStopSettings( stopName );
            if ( stopIndex == -1 ) {
                StopSettings stop( settings.currentStop() );
                stop.setStop( stopName );
                stop.set( UserSetting + 100, "-- Intermediate Stop --" );
                settings.appendStop( stop );
                stopIndex = settings.stops().count() - 1;
            }
            settings.setCurrentStop( stopIndex );
            setSettings( settings );

            emit intermediateDepartureListRequested( stopName );
            break;
        } case StopAction::HighlightStop: {
            d->model->setHighlightedStop(
                    d->model->highlightedStop().compare(stopName, Qt::CaseInsensitive) == 0
                    ? QString() : stopName );
            break;
        } case StopAction::CopyStopNameToClipboard: {
            QApplication::clipboard()->setText( stopNameShortened );
            break;
        }
    }
}

void PublicTransportApplet::errorMarble( QProcess::ProcessError processError )
{
    Q_D( PublicTransportApplet );

    if ( processError == QProcess::FailedToStart ) {
        int result = KMessageBox::questionYesNo( 0, i18nc("@info", "The map application "
                "'marble' couldn't be started, error message: <message>%1</message>.<nl/>"
                "Do you want to install 'marble' now?", d->marble->errorString()) );
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
    d->marble = 0;
}

void PublicTransportApplet::marbleHasStarted()
{
    Q_D( const PublicTransportApplet );
    kDebug() << "Marble has started" << d->marble->pid();

    // Wait for output from marble
    for ( int i = 0; i < 10; ++i ) {
        if ( d->marble->waitForReadyRead(50) ) {
            break;
        }
    }

    QTimer::singleShot( 250, this, SLOT(showStopInMarble()) );
}

void PublicTransportApplet::marbleFinished( int /*exitCode*/ )
{
    Q_D( PublicTransportApplet );

    kDebug() << "Marble finished";
    d->marble = 0;
}

void PublicTransportApplet::showStopInMarble( qreal lon, qreal lat )
{
    Q_D( const PublicTransportApplet );

    if ( !d->marble ) {
        kDebug() << "No marble process?";
        return;
    }

    if ( lon < 0 || lat < 0 ) {
        lon = d->longitude;
        lat = d->latitude;
    }

    kDebug() << lon << lat;
    QString destination = QString("org.kde.marble-%1").arg(d->marble->pid());

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

void PublicTransportApplet::removeAlarmForDeparture( int row )
{
    Q_D( const PublicTransportApplet );

    DepartureItem *item = static_cast<DepartureItem*>( d->model->item(row) );
    Q_ASSERT_X( item->alarmStates().testFlag(AlarmIsAutoGenerated),
                "PublicTransportApplet::removeAlarmForDeparture",
                "Only auto generated alarms can be removed automatically" );

    // Find a matching autogenerated alarm
    int matchingAlarmSettings = -1;
    for ( int i = 0; i < d->settings.alarms().count(); ++i ) {
        const AlarmSettings alarm = d->settings.alarm( i );
        if ( alarm.autoGenerated && alarm.enabled &&
             alarm.filter.match(*item->departureInfo()) )
        {
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
    AlarmSettingsList newAlarmSettings = d->settings.alarms();
    newAlarmSettings.removeAt( matchingAlarmSettings );
    removeAlarms( newAlarmSettings, QList<int>() << matchingAlarmSettings );

    if ( d->clickedItemIndex.isValid() ) {
        updatePopupIcon();
    }
}

void PublicTransportApplet::removeAlarmForDeparture()
{
    Q_D( const PublicTransportApplet );
    removeAlarmForDeparture( d->clickedItemIndex.row() );
}

void PublicTransportApplet::processAlarmCreationRequest( const QDateTime& departure,
        const QString& lineString, VehicleType vehicleType, const QString& target,
        QGraphicsWidget *item )
{
    Q_UNUSED( item );
    Q_D( const PublicTransportApplet );

    // Autogenerate an alarm that only matches the given departure
    AlarmSettings alarm;
    alarm.autoGenerated = true;
    alarm.affectedStops << d->settings.currentStopIndex();
    alarm.name = i18nc( "@info/plain Name for a new alarm, eg. requested using the context menu. "
                        "%1 is the departure time or the name of the used vehicle.",
                        "One-Time Alarm (%1)", departure.isValid() ? departure.toString()
                                               : Global::vehicleTypeToString(vehicleType) );

    // Add alarm filters
    if ( !departure.isNull() ) {
        alarm.filter << Constraint(FilterByDepartureTime, FilterEquals, departure.time());
        alarm.filter << Constraint(FilterByDepartureDate, FilterEquals, departure.date());
    }
    if ( !lineString.isEmpty() ) {
        alarm.filter << Constraint(FilterByTransportLine, FilterEquals, lineString);
    }
    alarm.filter << Constraint(FilterByVehicleType, FilterIsOneOf, QVariantList() << vehicleType);
    if ( !target.isEmpty() ) {
        alarm.filter << Constraint(FilterByTarget, FilterEquals, target);
    }

    // Append new alarm in a copy of the settings. Then write the new settings.
    Settings settings = d->settings;
    settings.appendAlarm( alarm );
    setSettings( settings );

    alarmCreated();
}

void PublicTransportApplet::processAlarmDeletionRequest( const QDateTime& departure,
        const QString& lineString, VehicleType vehicleType, const QString& target,
        QGraphicsWidget* item)
{
    Q_UNUSED( item );
    Q_D( const PublicTransportApplet );

    // Autogenerate an alarm that only matches the given departure
    AlarmSettings alarm;
    alarm.autoGenerated = true;
    alarm.affectedStops << d->settings.currentStopIndex();
    if ( !departure.isNull() ) {
        alarm.filter << Constraint(FilterByDepartureTime, FilterEquals, departure.time());
        alarm.filter << Constraint(FilterByDepartureDate, FilterEquals, departure.date());
    }
    if ( !lineString.isEmpty() ) {
        alarm.filter << Constraint(FilterByTransportLine, FilterEquals, lineString);
    }
    alarm.filter << Constraint(FilterByVehicleType, FilterIsOneOf, QVariantList() << vehicleType);
    if ( !target.isEmpty() ) {
        alarm.filter << Constraint(FilterByTarget, FilterEquals, target);
    }

    // Remove autogenerated alarms that equal [alarm] in a copy of the settings. Then write the new settings.
    Settings settings = d->settings;
    settings.removeAlarm( alarm );
    setSettings( settings );

    updatePopupIcon();
}

void PublicTransportApplet::createAlarmSettingsForDeparture( const QPersistentModelIndex &modelIndex,
                                                       bool onlyForCurrentWeekday )
{
    Q_D( const PublicTransportApplet );

    if ( !modelIndex.isValid() ) {
        kDebug() << "!modelIndex.isValid()";
        return;
    }

    DepartureItem *item = static_cast<DepartureItem*>( d->model->itemFromIndex(modelIndex) );
    DepartureInfo info = *item->departureInfo();
    QString departureTime = KGlobal::locale()->formatTime( info.departure().time() );

    // Autogenerate an alarm that only matches the given departure
    AlarmSettings alarm;
    alarm.autoGenerated = true;
    alarm.affectedStops << d->settings.currentStopIndex();
    alarm.filter.append( Constraint(FilterByDepartureTime, FilterEquals, info.departure().time()) );
    alarm.filter.append( Constraint(FilterByDepartureDate, FilterEquals, info.departure().date()) );
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
    Settings settings = d->settings;
    settings.appendAlarm( alarm );
    setSettings( settings );

    // Add the new alarm to the list of alarms that match the given departure
    const int index = settings.alarms().count() - 1;
    info.matchedAlarms() << index;
    item->setDepartureInfo( info );
}

void PublicTransportApplet::createAlarmForDeparture()
{
    Q_D( const PublicTransportApplet );
    createAlarmSettingsForDeparture( d->clickedItemIndex );
    alarmCreated();
}

void PublicTransportApplet::createAlarmForDepartureCurrentWeekDay()
{
    Q_D( const PublicTransportApplet );
    createAlarmSettingsForDeparture( d->clickedItemIndex, true );
    alarmCreated();
}

void PublicTransportApplet::alarmCreated()
{
    Q_D( PublicTransportApplet );

    updatePopupIcon(); // TEST needed or already done in writeSettings?

    // Animate popup icon to show the alarm departure (at index -1)
    d->popupIcon->animateToAlarm();
}

void PublicTransportApplet::alarmFired( DepartureItem* item, const AlarmSettings &alarm )
{
    const DepartureInfo *departureInfo = item->departureInfo();
    QString sLine = departureInfo->lineString();
    QString sTarget = departureInfo->target();
    QDateTime predictedDeparture = departureInfo->predictedDeparture();
    int minsToDeparture = qCeil( QDateTime::currentDateTime().secsTo(predictedDeparture) / 60.0 );

    QString message;
    if ( minsToDeparture > 0 ) {
        // Departure is in the future
        if ( departureInfo->vehicleType() == UnknownVehicleType ) {
            // Vehicle type is unknown
            message = i18ncp( "@info/plain %5: Name of the Alarm",
                              "%5: Line %2 to '%3' departs in %1 minute at %4",
                              "%5: Line %2 to '%3' departs in %1 minutes at %4",
                              minsToDeparture, sLine, sTarget,
                              predictedDeparture.toString("hh:mm"), alarm.name );
        } else {
            // Vehicle type is known
            message = i18ncp( "@info/plain %2: Line string (e.g. 'U3'), %4: Vehicle type name "
                              "(e.g. tram, subway), %6: Name of the Alarm",
                              "%6: The %4 %2 to '%3' departs in %1 minute at %5",
                              "%6: The %4 %2 to '%3' departs in %1 minutes at %5",
                              minsToDeparture, sLine, sTarget,
                              Global::vehicleTypeToString(departureInfo->vehicleType()),
                              predictedDeparture.toString("hh:mm"), alarm.name );
        }
    } else if ( minsToDeparture < 0 ) {
        // Has already departed
        if ( departureInfo->vehicleType() == UnknownVehicleType ) {
            // Vehicle type is unknown
            message = i18ncp( "@info/plain %5: Name of the Alarm",
                              "%5: Line %2 to '%3' has departed %1 minute ago at %4",
                              "%5: Line %2 to '%3' has departed %1 minutes ago at %4",
                              -minsToDeparture, sLine, sTarget,
                              predictedDeparture.toString("hh:mm"), alarm.name );
        } else {
            // Vehicle type is known
            message = i18ncp( "@info/plain %2: Line string (e.g. 'U3'), %4: Vehicle type name "
                              "(e.g. tram, subway), %6: Name of the Alarm",
                              "%6: The %4 %2 to '%3' has departed %1 minute ago at %5",
                              "%6: The %4 %2 to %3 has departed %1 minutes ago at %5",
                              -minsToDeparture, sLine, sTarget,
                              Global::vehicleTypeToString(departureInfo->vehicleType()),
                              predictedDeparture.toString("hh:mm"), alarm.name );
        }
    } else {
        // Departs now
        if ( departureInfo->vehicleType() == UnknownVehicleType ) {
            // Vehicle type is unknown
            message = i18nc( "@info/plain %4: Name of the Alarm",
                             "%4: Line %1 to '%2' departs now at %3",
                             sLine, sTarget, predictedDeparture.toString("hh:mm"),
                             alarm.name );
        } else {
            // Vehicle type is known
            message = i18nc( "@info/plain %1: Line string (e.g. 'U3'), %3: Vehicle type name "
                             "(e.g. tram, subway), %5: Name of the Alarm",
                             "%5: The %3 %1 to '%2' departs now at %4", sLine, sTarget,
                             Global::vehicleTypeToString(departureInfo->vehicleType()),
                             predictedDeparture.toString("hh:mm"), alarm.name );
        }
    }

    KNotification::event( KNotification::Warning, message,
                          KIcon("public-transport-stop").pixmap(16), 0L,
                          KNotification::Persistent );
}

void PublicTransportApplet::removeAlarms( const AlarmSettingsList &newAlarmSettings,
                                    const QList<int> &/*removedAlarms*/ )
{
    Q_D( const PublicTransportApplet );

    // Change alarm settings in a copy of the settings. Then write the new settings.
    Settings settings = d->settings;
    settings.setAlarms( newAlarmSettings );
    setSettings( settings );
}

void GraphicsPixmapWidget::paint( QPainter* painter,
                                const QStyleOptionGraphicsItem* option, QWidget* )
{
    if ( !option->rect.isValid() ) {
        return;
    }
    painter->drawPixmap( option->rect, m_pixmap );
}

QVariant PublicTransportApplet::supportedJourneySearchState() const
{
    Q_D( const PublicTransportApplet );

    QObject *object = qobject_cast<QObject*>(
            d->currentServiceProviderFeatures.contains("JourneySearch")
            ? d->states["journeySearch"] : d->states["journeysUnsupportedView"] );
    return qVariantFromValue( object );
}

void PublicTransportApplet::configureJourneySearches()
{
    Q_D( const PublicTransportApplet );

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
    QList< JourneySearchItem > journeySearches = d->settings.currentJourneySearches();
    for ( int i = 0; i < journeySearches.count(); ++i ) {
        const JourneySearchItem item = journeySearches[i];
        model->addJourneySearch( item.journeySearch(), item.name(), item.isFavorite() );
    }
    model->sort();
    journeySearchList->setModel( model );

    QLabel *label = new QLabel( i18nc("@label:listbox", "Favorite and recent journey searches "
                                      "for '%1':", d->currentServiceProviderData()["name"].toString()),
                                dialog->mainWidget() );
    label->setWordWrap( true );
    label->setBuddy( journeySearchList );

    l->addWidget( label );
    l->addWidget( journeySearchList );
    if ( dialog->exec() == KDialog::Accepted ) {
        journeySearchListUpdated( model->journeySearchItems() );
    }
}

void PublicTransportApplet::setAssociatedApplicationUrlForJourneys()
{
    Q_D( const PublicTransportApplet );
    setAssociatedApplicationUrls (KUrl::List() << d->urlJourneys);
}

void PublicTransportApplet::setAssociatedApplicationUrlForDepartures()
{
    Q_D( const PublicTransportApplet );
    setAssociatedApplicationUrls (KUrl::List() << d->urlDeparturesArrivals);
}

int PublicTransportApplet::departureCount() const
{
    Q_D( const PublicTransportApplet );
    return d->departureInfos.count();
}

#include "publictransport.moc"
