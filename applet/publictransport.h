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

/** @file
 * @brief This file contains the public transport applet.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef PUBLICTRANSPORT_HEADER
#define PUBLICTRANSPORT_HEADER

// KDE includes
#include <Plasma/PopupApplet> // Base class
#include <Plasma/DataEngine> // For dataUpdated() slot, Plasma::DataEngine::Data
#include <KProcess> // For QProcess::ProcessError

// Own includes
#include "stopaction.h" // For StopAction::Type
#include "settings.h" // Member variable

namespace PublicTransport {
    class JourneyInfo;
}

class PublicTransportAppletPrivate;
class ItemBase;
class PublicTransportGraphicsItem;
class DepartureItem;

class KJob;
class KSelectAction;
class QGraphicsSceneWheelEvent;
class QPersistentModelIndex;

namespace Plasma {
    class Label;
}

/** @brief Simple pixmap graphics widgets (QGraphicsPixmapItem isn't a QGraphicsWidget). */
class GraphicsPixmapWidget : public QGraphicsWidget {
public:
    explicit GraphicsPixmapWidget( const QPixmap &pixmap, QGraphicsWidget *parent = 0 )
                : QGraphicsWidget( parent ), m_pixmap( pixmap ) {
        setGeometry( QRectF(m_pixmap.rect()) );
    };

    /** @returns the @ref QPixmap drawn by this graphics widget. */
    QPixmap pixmap() const { return m_pixmap; };
    virtual QRectF boundingRect() const { return geometry(); };
    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* /*widget*/ = 0 );

private:
    QPixmap m_pixmap;
};

/**
 * @brief Shows a departure/arrival board for public transport.
 *
 * It uses the "publictransport"-data engine and stores the data using @ref DepartureModel for
 * departures/arrivals and @ref JourneyModel for journeys. The data from the data engine is read
 * in a thread using @ref DepartureProcessor, which also applies filters and alarms.
 *
 * @ref TitleWidget is used as title of the applet. The departures/arrivals/journeys are shown
 * in @ref TimetableWidget / @ref JourneyTimetableWidget.
 **/
class PublicTransportApplet : public Plasma::PopupApplet {
    Q_OBJECT

    /** @brief The number of currently shown departures/arrivals. */
    Q_PROPERTY( int DepartureCount READ departureCount )

    /**
     * @brief The journey search state or the journey unsupported state.
     *
     * Depends on the features of the current service provider.
     **/
    Q_PROPERTY( QVariant supportedJourneySearchState READ supportedJourneySearchState )

public:
    /** @brief Basic create. */
    PublicTransportApplet( QObject *parent, const QVariantList &args );

    /** @brief Destructor. */
    ~PublicTransportApplet();

    /**
     * @brief Maximum number of recent journey searches.
     *
     * When more journey searches get added, the oldest gets removed.
     **/
    static const int MAX_RECENT_JOURNEY_SEARCHES = 10;

    /** @brief Return the widget with the contents of the applet. */
    virtual QGraphicsWidget* graphicsWidget();

    /** @returns the number of currently shown departures/arrivals. */
    int departureCount() const;

    /** @brief Get a pointer to either the journey search state or the journeys unsupported state. */
    QVariant supportedJourneySearchState() const;

signals:
    /** @brief Emitted when the settings have changed. */
    void settingsChanged();

    /** @brief Emitted when updated provider data has the state "ready". */
    void providerReady();

    /** @brief Emitted when updated provider data does @em not have the state "ready". */
    void providerNotReady();

    /** @brief Emitted when an intermediate departure list gets requested for @p stopName. */
    void intermediateDepartureListRequested( const QString &stopName );

    /**
     * @brief Emitted when the journey search is finished.
     *
     * This triggers a transition to the journey view state.
     **/
    void journeySearchFinished();

    /** @brief Emitted when the action buttons state was left. */
    void hideActionButtons();

    /** @brief Emitted when the network connection is lost to go set the corresponding states. */
    void networkConnectionLost();

    /** @brief Emitted when the network is being configured to go set the corresponding states. */
    void networkIsConfiguring();

    /** @brief Emitted when the network connection is activated to go set the corresponding states. */
    void networkIsActivated();

    /** @brief Emitted when new departure data gets requested to go set the corresponding states. */
    void requestedNewDepartureData();

    /** @brief Emitted when valid departure data gets received to go set the corresponding states. */
    void validDepartureDataReceived();

    /** @brief Emitted when invalid departure data gets received to go set the corresponding states. */
    void invalidDepartureDataReceived();

    /** @brief Emitted when new journey data gets requested to go set the corresponding states. */
    void requestedNewJourneyData();

    /** @brief Emitted when valid journey data gets received to go set the corresponding states. */
    void validJourneyDataReceived();

    /** @brief Emitted when invalid journey data gets received to go set the corresponding states. */
    void invalidJourneyDataReceived();

public slots:
    /** @brief Initializes the applet. */
    virtual void init();

    /**
     * @brief Clear the current list of stop settings and adds a new one.
     *
     * @param serviceProviderID The ID of the service provider to use for the new stop settings.
     * @param stopName The stop name to use for the new stop settings.
     **/
    void setSettings( const QString &serviceProviderID, const QString &stopName );

    /**
     * @brief Replace the current lists of stop and filter settings.
     *
     * @param stops The new list of stops.
     * @param filters The new list of filters.
     **/
    void setSettings( const StopSettingsList &stops,
                      const FilterSettingsList &filters );

    /**
     * @brief Replace the current settings and updates the applet accordingly.
     *
     * Settings are written using SettingsIO::writeSettings() which also compares them with the
     * old settings and returns flags for changes (SettingsIO::ChangedFlags). Only parts of the
     * applet that are affected by changed settings get updated.
     *
     * @param settings The new settings.
     **/
    void setSettings( const Settings &settings );

    void requestEarlierJourneys();
    void requestLaterJourneys();

protected slots:
    /** @brief The geometry of the applet has changed. */
    void appletResized();

    /** @brief Overriden to update the popup icon to new sizes using updatePopupIcon(). */
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );

    /**
     * @brief New @p data arrived from the data engine for the source with the name @p sourceName.
     *
     * @ingroup models
     **/
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

    void acceptActionButtons();

    void setAssociatedApplicationUrlForDepartures();
    void setAssociatedApplicationUrlForJourneys();

    /** @brief The state of the departure data from the data engine has changed (waiting, invalid/valid). */
    void departureDataStateChanged();

    /** @brief The state of the journey data from the data engine has changed (waiting, invalid/valid). */
    void journeyDataStateChanged();

    void disconnectJourneySource();

    /** @brief The context menu for a departure @p item was requested. */
    void departureContextMenuRequested( PublicTransportGraphicsItem *item, const QPointF &pos );

    /**
     * @brief Get called by the context menu of route stops.
     *
     * Performs the given @p stopAction.
     *
     * @param stopAction The action that is requested to be performed.
     * @param stopName The stop name to perform @p stopAction on.
     * @param stopNameShortened The shortened stop name to show to the user.
     **/
    void requestStopAction( StopAction::Type stopAction,
                            const QString &stopName, const QString &stopNameShortened );

    /**
     * @brief Show the departure list.
     *
     * Can be used eg. if the journey view is currently shown to go back
     * to the departure view.
     *
     * This gets called when the departure view state is entered.
     **/
    void showDepartureList();

    /**
     * @brief Show the journey list.
     *
     * This gets called when the journey view state is entered, eg. because
     * a journey search was finished.
     **/
    void showJourneyList();

    /**
     * @brief Show an intermediate departure list.
     *
     * Shows a departure list for another stop, with the option to go back to
     * the original stop again.
     * This uses a special StopSettings item in the list of stored stop
     * settings, which will get deleted automatically when the intermediate
     * departure list is closed again.
     *
     * This gets called when the intermediate departure view state is entered,
     * eg. because it was requested by the context menu of a route stop item.
     **/
    void showIntermediateDepartureList();

    /**
     * @brief Show departures in the departure list (not arrivals).
     *
     * Writes new settings with @ref Settings::departureArrivalListType set
     * to @p DepartureList. This also updates the departure view on @ref configChanged.
     *
     * @note This doesn't switch to the departure list automatically.
     *   To do this also use @ref showDepartureList.
     **/
    void showDepartures();

    /**
     * @brief Show arrivals in the departure list.
     *
     * Writes new settings with @ref Settings::departureArrivalListType set
     * to @p ArrivalList. This also updates the departure view on @ref configChanged.
     *
     * @note This doesn't switch to the departure list automatically.
     *   To do this also use @ref showDepartureList.
     **/
    void showArrivals();

    /**
     * @brief Show the journey search view.
     *
     * Switches to journey search mode. To go back to the departure list use
     * @ref showDepartureList. If a journey search string is set the user can
     * go to the journey mode from the search mode.
     **/
    void showJourneySearch();

    /** @brief Called when exiting the journey search state. */
    void exitJourneySearch();

    /**
     * @brief Show a message about unsupported journeys.
     *
     * This gets called if the journey search state should be entered, but
     * journeys aren't supported by the current service provider.
     **/
    void showJourneysUnsupportedView();

    /**
     * @brief Show the action button overlay.
     *
     * Blurs the applet and shows some buttons to perform actions like searching
     * for a journey or toggling between departure and arrival view.
     **/
    void showActionButtons();

    /**
     * @brief Show @p mainWidget in the applet.
     *
     * This gets used to switch eg. to the departure/arrival view (showDepartureList())
     * or to the journey view (showJourneyList()).
     * Can also be used to show other widgets, eg. showing messages.
     **/
    void showMainWidget( QGraphicsWidget *mainWidget );

    /** @brief Remove stop settings, that were inserted for an intermediate
     * departure list. */
    void removeIntermediateStopSettings();

    /**
     * @brief Finished editing the journey search line (return pressed,
     *   start search button clicked or stop suggestion double clicked).
     *
     * @param text The finished journey search text.
     **/
    void journeySearchInputFinished( const QString &text );

    /**
     * @brief The journey search line has been changed and parsed.
     *
     * This slot gets called by JourneySearchSuggestionWidget.
     * @param stopName The parsed stop name.
     * @param departure The parsed departure date and time.
     * @param stopIsTarget Whether or not the parsed stop should be treated as target (true)
     *   or as origin stop (false).
     * @param timeIsDeparture Whether or not the parsed time should be treated as departure (true)
     *   or as arrival time (false).
     **/
    void journeySearchLineChanged( const QString &stopName, const QDateTime &departure,
                                   bool stopIsTarget, bool timeIsDeparture );

    /** @brief The action to update the data source has been triggered. */
    void updateDataSource();

    /** @brief The action to set an alarm for the selected departure/arrival has been triggered. */
    void createAlarmForDeparture();

    /** @brief The action to set an alarm for the current weekday has been triggered. */
    void createAlarmForDepartureCurrentWeekDay();

    /** @brief The action to remove an alarm from the selected departure/arrival has been triggered. */
    void removeAlarmForDeparture();

    /** @brief The action to expand / collapse of the selected departure/arrival has been triggered. */
    void toggleExpanded();

    /** @brief The action to hide the direction column of the tree view header has been triggered. */
    void hideColumnTarget();

    /** @brief The action to show the direction column of the tree view header has been triggered. */
    void showColumnTarget();

    /** @brief The plasma theme has been changed. */
    void themeChanged();

    /** @brief Shows the stop at the given coordinates in a running Marble process. */
    void showStopInMarble( const QString &stopName = QString(), bool coordinatesAreValid = false,
                           qreal lon = 0.0, qreal lat = 0.0 );

    /** @brief The 'marble' process has finished. */
    void marbleFinished( int exitCode );

    /** @brief There was an error with Marble */
    void marbleError( const QString &errorMessage );

    /** @brief The expanded state of @p item changed to @p expanded. */
    void expandedStateChanged( PublicTransportGraphicsItem *item, bool expanded );

    /** @brief A recent journey @p action was triggered from the "quickJourneys" action. */
    void journeyActionTriggered( QAction *action );

    /**
     * @brief Updates the settings and change the current list of journey searches.
     * Uses Settings::setCurrentJourneySearches().
     **/
    void journeySearchListUpdated( const QList<JourneySearchItem> &newJourneySearches );

    /**
     * @brief Processes a journey search request.
     *
     * @param stop The target/origin stop of the journey to search.
     * @param stopIsTarget Whether @p stop is the target or the origin. The other stop is the
     *   current home stop.
     **/
    void processJourneyRequest( const QString &stop, bool stopIsTarget );

    /**
     * @brief The worker thread starts processing departures/arrivals from the
     *   data engine.
     *
     * @param sourceName The data engine source name for the departure data.
     * @see departuresProcessed
     * @ingroup models
     **/
    void beginDepartureProcessing( const QString &sourceName );

    /**
     * @brief The worker thread has finished processing departures/arrivals.
     *
     * @param sourceName The data engine source name for the departure data.
     * @param departures A list of departures that were read by the worker thread.
     * @param requestUrl The url that was used to download the departure data.
     * @param lastUpdate The date and time of the last update of the data.
     * @param nextAutomaticUpdate The date and time of the next automatic update of the data source.
     * @param minManualUpdateTime The minimal date and time of the next (manual) update of the
     *   data source. Earlier update requests will be rejected.
     * @param departuresToGo The number of departures to still be processed. If this isn't 0
     *   this slot gets called again after the next batch of departures has been processed.
     *
     * @see DepartureInfo
     * @see beginDepartureProcessing
     * @ingroup models
     **/
    void departuresProcessed( const QString &sourceName,
                              const QList< DepartureInfo > &departures,
                              const QUrl &requestUrl, const QDateTime &lastUpdate,
                              const QDateTime &nextAutomaticUpdate,
                              const QDateTime &minManualUpdateTime, int departuresToGo );

    /**
     * @brief The worker thread has finished filtering departures.
     *
     * @param sourceName The data engine source name for the departure data.
     * @param departures The list of departures that were filtered. Each departure now returns
     *   the correct value with isFilteredOut() according to the filter settings given to the
     *   worker thread.
     * @param newlyFiltered A list of departures that should be made visible to match the current
     *   filter settings.
     * @param newlyNotFiltered A list of departures that should be made invisible to match the
     *   current filter settings.
     *
     * @ingroup models
     **/
    void departuresFiltered( const QString &sourceName,
                             const QList< DepartureInfo > &departures,
                             const QList< DepartureInfo > &newlyFiltered,
                             const QList< DepartureInfo > &newlyNotFiltered );

    /**
     * @brief The worker thread starts processing journeys from the data engine.
     *
     * @see journeysProcessed
     * @ingroup models
     **/
    void beginJourneyProcessing( const QString &sourceName );

    /**
     * @brief The worker thread has finished processing journeys.
     *
     * @param sourceName The data engine source name for the journey data.
     * @param journeys A list of journeys that were read by the worker thread.
     * @param requestUrl The url that was used to download the journey data.
     * @param lastUpdate The date and time of the last update of the data.
     *
     * @see JourneyInfo
     * @see beginJourneyProcessing
     * @ingroup models
     **/
    void journeysProcessed( const QString &sourceName,
                            const QList< JourneyInfo > &journeys,
                            const QUrl &requestUrl, const QDateTime &lastUpdate );

    /** @brief The animation fading out a pixmap of the old applet appearance has finished. */
    void oldItemAnimationFinished();

    /** @brief The animation to toggle the display of the title has finished. */
    void titleToggleAnimationFinished();

    /** @brief Delete the overlay item used when showing action buttons over the plasmoid. */
    void destroyOverlay();

    /** @brief An action to change the currently shown stop has been triggered. */
    void setCurrentStopIndex( QAction *action );

    void setCurrentStopIndex( int stopIndex );

    /** @brief Enable @p filterConfiguration for the currently active stop settings. */
    void enableFilterConfiguration( const QString &filterConfiguration, bool enable = true );

    /**
     * @brief An action to toggle a filter configuration has been triggered.
     *
     * This uses the text of @p action to know which filter configuration to toggle.
     **/
    void switchFilterConfiguration( QAction *action );

    /**
     * @brief An action to a filter by group color has been triggered.
     *
     * This uses the data in @p action (QAction::data()) to know which color group to toggle.
     **/
    void switchFilterByGroupColor( QAction *action );

    /** @brief An alarm has been fired for the given @p item. */
    void alarmFired( DepartureItem *item, const AlarmSettings &alarm );

    void removeAlarms( const AlarmSettingsList &newAlarmSettings,
                       const QList<int> &removedAlarms );

    void processAlarmCreationRequest( const QDateTime &departure, const QString &lineString,
                                      VehicleType vehicleType, const QString &target,
                                      QGraphicsWidget *item );
    void processAlarmDeletionRequest( const QDateTime &departure, const QString &lineString,
                                      VehicleType vehicleType, const QString &target,
                                      QGraphicsWidget *item );

    /**
     * @brief The @p departures will get removed after this slot was called.
     *
     * @param items A list of DepartureItems, that will get removed
     *   after this slot was called.
     *
     * @note This slot is connected to the itemsAboutToBeRemoved signal
     *   of the departure/arrival model.
     **/
    void departuresAboutToBeRemoved( const QList<ItemBase*> &departures );

    /**
     * @brief The @p departures have just left.
     *
     * The DepartureItems for the given @p departures were deleted before this
     * slot gets called.
     **/
    void departuresLeft( const QList<DepartureInfo> &departures );

    /** @brief Updates the popup icon with information about the next departure / alarm. */
    void updatePopupIcon();

    /** @brief Updates the tooltip. */
    void updateTooltip();

    /** @brief Opens a configuration dialog for recent/favorite journey searches. */
    void configureJourneySearches();

    void updateRequestFinished( KJob *job );
    void enableUpdateAction();

protected:
    /**
     * @brief Create the configuration dialog contents.
     *
     * @param parent The config dialog in which the config interface should be created.
     **/
    virtual void createConfigurationInterface( KConfigDialog *parent );

    /** @brief Get a list of actions for the context menu. */
    virtual QList<QAction*> contextualActions();

    /** @brief The popup gets shown or hidden. */
    virtual void popupEvent( bool show );

    /** @brief Mouse wheel rotated on popup icon. */
    virtual void wheelEvent( QGraphicsSceneWheelEvent* event );

    /** @brief Overridden to make links clickable in the bottom info label. */
    virtual bool sceneEventFilter( QGraphicsItem* watched, QEvent* event );

    /** @brief Watch for up/down key presses in m_journeySearch to select stop suggestions. */
    virtual bool eventFilter( QObject* watched, QEvent* event );

    /** @brief Create all used actions. */
    void setupActions();

    void disableUpdateAction();

    /**
     * @brief Get an action with string and icon updated to the current settings.
     *
     * @param actionName The name of the action to return updated.
     * @return The updated action.
     **/
    QAction* updatedAction( const QString &actionName );

    /** @brief Call after creating a new alarm, to update the UI accordingly. */
    void alarmCreated();

    /**
     * @brief Handle errors from the publictransport data engine for @p data from source @p sourceName.
     *
     * @ingroup models
     **/
    void handleDataError( const QString &sourceName, const Plasma::DataEngine::Data& data );

    /**
     * @brief Read stop suggestions from the data engine.
     *
     * @ingroup models
     **/
    void processStopSuggestions( const QString &sourceName, const Plasma::DataEngine::Data& data );

    /** @brief Set an autogenerated alarm for the given departure/arrival. */
    void createAlarmSettingsForDeparture( const QPersistentModelIndex &modelIndex,
                                          bool onlyForCurrentWeekday = false );

    /** @brief Remove an autogenerated alarm from this departure/arrival if any. */
    void removeAlarmForDeparture( int row );

private:
    PublicTransportAppletPrivate* const d_ptr;
    Q_DECLARE_PRIVATE( PublicTransportApplet )
};

#ifndef NO_EXPORT_PLASMA_APPLET // Needed for publictransport_p.h to include publictransport.h
// This is the command that links the applet to the .desktop file
K_EXPORT_PLASMA_APPLET( publictransport, PublicTransportApplet )
#endif

#endif // Multiple inclusion guard
