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

/** @file
 * @brief This file contains the public transport applet.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef PUBLICTRANSPORT_HEADER
#define PUBLICTRANSPORT_HEADER

// KDE includes
#include <Plasma/PopupApplet> // Base class
#include <Plasma/DataEngine> // For dataUpdated() slot, Plasma::DataEngine::Data
#include <KProcess> // For QProcess::ProcessError

// Qt includes
#include <QPersistentModelIndex> // Member variable

// libpublictransporthelper includes
#include <departureinfo.h>

// Own includes
#include "stopaction.h" // For StopAction::Type
#include "settings.h" // Member variable

class PopupIcon;
class ItemBase;
class PublicTransportGraphicsItem;
class TitleWidget;
class OverlayWidget;
class TimetableWidget;
class DepartureModel;
class DepartureItem;
class DeparturePainter;
class DepartureProcessor;
class JourneyTimetableWidget;
class JourneyModel;
class JourneySearchSuggestionWidget;

class KSelectAction;

class QStateMachine;
class QState;
class QAbstractTransition;
class QPropertyAnimation;
class QParallelAnimationGroup;
class QGraphicsSceneWheelEvent;

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
class PublicTransport : public Plasma::PopupApplet {
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
    PublicTransport( QObject *parent, const QVariantList &args );

    /** @brief Destructor. */
    ~PublicTransport();

    /** @brief Gets a pointer to either the journey search state or the journeys unsupported state. */
    QVariant supportedJourneySearchState() const;

    /**
     * @brief Maximum number of recent journey searches.
     *
     * When more journey searches get added, the oldest gets removed.
     **/
    static const int MAX_RECENT_JOURNEY_SEARCHES = 10;

    /** @brief Returns the widget with the contents of the applet. */
    virtual QGraphicsWidget* graphicsWidget();

    /** @returns the number of currently shown departures/arrivals. */
    int departureCount() const { return m_departureInfos.count(); };

    /** @brief Checks if the state with the given @p stateName is currently active. */
    bool isStateActive( const QString &stateName ) const;

    /** @brief Updates the color groups to the currently shown departures. */
    void updateColorGroupSettings();

signals:
    /** @brief Emitted when the settings have changed. */
    void settingsChanged();

    /** @brief Emitted when an intermediate departure list gets requested for @p stopName. */
    void intermediateDepartureListRequested( const QString &stopName );

    /**
     * @brief Emitted when the journey search is finished.
     *
     * This triggers a transition to the journey view state.
     **/
    void journeySearchFinished();

    /** @brief Emitted when the action buttons state was cancelled. */
    void cancelActionButtons();

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
     * @brief Clears the current list of stop settings and adds a new one.
     *
     * @param serviceProviderID The ID of the service provider to use for the new stop settings.
     * @param stopName The stop name to use for the new stop settings.
     **/
    void setSettings( const QString &serviceProviderID, const QString &stopName );

    /**
     * @brief Replaces the current lists of stop and filter settings.
     *
     * @param stopSettingsList The new list of stop settings.
     * @param filterSettings The new list of filter settings.
     **/
    void setSettings( const StopSettingsList &stopSettingsList,
                      const FilterSettingsList &filterSettings );

    /**
     * @brief Replaces the current settings and updates the applet accordingly.
     *
     * Settings are written using SettingsIO::writeSettings() which also compares them with the
     * old settings and returns flags for changes (SettingsIO::ChangedFlags). Only parts of the
     * applet that are affected by changed settings get updated.
     *
     * @param settings The new settings.
     **/
    void setSettings( const Settings &settings );

protected slots:
    /** @brief The geometry of the applet has changed. */
    void geometryChanged();

    /** @brief Settings have changed. */
    void configChanged();

    /** @brief Settings that require a new data request have been changed. */
    void serviceProviderSettingsChanged();

    /**
     * @brief Overriden to update the popup icon to new sizes using updatePopupIcon().
     *
     * @see resized()
     **/
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );

    /**
     * @brief Gets connected to the geometryChanged signal of the main graphics widget.
     *
     * This is used, because resizeEvent doesn't get called if the applet is iconified and
     * m_graphicsWidget gets resized (only if the popup applet gets resized).
     **/
    void resized();

    /**
     * @brief New @p data arrived from the data engine for the source with the name @p sourceName.
     *
     * @ingroup models
     **/
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

    void setAssociatedApplicationUrlForDepartures() {
        if ( m_urlDeparturesArrivals.isValid() ) {
            setAssociatedApplicationUrls( KUrl::List() << m_urlDeparturesArrivals );
        } else {
            setAssociatedApplicationUrls( KUrl::List() );
        }
    };
    void setAssociatedApplicationUrlForJourneys() {
        if ( m_urlJourneys.isValid() ) {
            setAssociatedApplicationUrls( KUrl::List() << m_urlJourneys );
        } else {
            setAssociatedApplicationUrls( KUrl::List() );
        }
    };

    void departureDataWaitingStateEntered();
    void departureDataInvalidStateEntered();
    void departureDataValidStateEntered();

    void journeyDataWaitingStateEntered();
    void journeyDataInvalidStateEntered();
    void journeyDataValidStateEntered();

    /**
     * @brief Fills the departure data model with the given departure list.
     *
     * This will stop if the maximum departure count is reached or if all @p departures
     * have been added.
     *
     * @ingroup models
     **/
    void fillModel( const QList<DepartureInfo> &departures );

    /**
     * @brief Fills the journey data model with the given journey list.
     *
     * This will stop if all @p journeys have been added.
     *
     * @ingroup models
     **/
    void fillModelJourney( const QList<JourneyInfo> &journeys );

    void departureContextMenuRequested( PublicTransportGraphicsItem *item, const QPointF &pos );

    /**
     * @brief Gets called by the context menu of route stops.
     *
     * Performs the given @p stopAction.
     *
     * @param stopAction The action that is requested to be performed.
     * @param stopName The stop name to perform @p stopAction on.
     **/
    void requestStopAction( StopAction::Type stopAction, const QString &stopName  );

    void noItemsTextClicked();

    void updateInfoText();

    /**
     * @brief Shows the departure list.
     *
     * Can be used eg. if the journey view is currently shown to go back
     * to the departure view.
     *
     * This gets called when the departure view state is entered.
     **/
    void showDepartureList();

    /**
     * @brief Shows the journey list.
     *
     * This gets called when the journey view state is entered, eg. because
     * a journey search was finished.
     **/
    void showJourneyList();

    /**
     * @brief Shows an intermediate departure list.
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
     * @brief Shows departures in the departure list (not arrivals).
     *
     * Writes new settings with @ref Settings::departureArrivalListType set
     * to @p DepartureList. This also updates the departure view on @ref configChanged.
     *
     * @note This doesn't switch to the departure list automatically.
     *   To do this also use @ref showDepartureList.
     **/
    void showDepartures();

    /**
     * @brief Shows arrivals in the departure list.
     *
     * Writes new settings with @ref Settings::departureArrivalListType set
     * to @p ArrivalList. This also updates the departure view on @ref configChanged.
     *
     * @note This doesn't switch to the departure list automatically.
     *   To do this also use @ref showDepartureList.
     **/
    void showArrivals();

    /**
     * @brief Shows the journey search view.
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

    void showMainWidget( QGraphicsWidget *mainWidget );

    void updateDepartureListIcon();

    /** @brief Removes stop settings, that were inserted for an intermediate
     * departure list. */
    void removeIntermediateStopSettings();

    /** @brief Disconnects a currently connected journey data source.
     * @ingroup models */
    void disconnectJourneySource();

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

    /**
     * @brief Called from PublicTransportSettings to indicate the need to clear
     *   the departure list.
     **/
    void setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType );

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
    void themeChanged() { useCurrentPlasmaTheme(); };

    void showStopInMarble( qreal lon = -1.0, qreal lat = -1.0 );

    /** @brief The 'marble' process has been started. */
    void marbleHasStarted();

    /** @brief The 'marble' process has finished. */
    void marbleFinished( int exitCode );

    /** @brief There was an error in the 'marble' process. */
    void errorMarble( QProcess::ProcessError processError );

    /** @brief A recent journey @p action was triggered from the "quickJourneys" action. */
    void journeyActionTriggered( QAction *action );

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
                              int departuresToGo );

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

    /** @brief Deletes the overlay item used when showing action buttons over the plasmoid. */
    void destroyOverlay();

    /** @brief An action to change the currently shown stop has been triggered. */
    void setCurrentStopIndex( QAction *action );

    /** @brief Enables @p filterConfiguration for the currently active stop settings. */
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
    void alarmFired( DepartureItem *item, const AlarmSettings &alarmSettings );

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
    void updateTooltip() { createTooltip(); };

    /** @brief Opens a configuration dialog for recent/favorite journey searches. */
    void configureJourneySearches();

protected:
    /**
     * @brief Create the configuration dialog contents.
     *
     * @param parent The config dialog in which the config interface should be created.
     **/
    void createConfigurationInterface( KConfigDialog *parent );

    /** @brief Gets a list of actions for the context menu. */
    virtual QList<QAction*> contextualActions();

    /** @brief The popup gets shown or hidden. */
    virtual void popupEvent( bool show );

    /** @brief Mouse wheel rotated on popup icon. */
    virtual void wheelEvent( QGraphicsSceneWheelEvent* event );

    virtual bool sceneEventFilter( QGraphicsItem* watched, QEvent* event );

    /** @brief Watching for up/down key presses in m_journeySearch to select stop suggestions. */
    virtual bool eventFilter( QObject* watched, QEvent* event );

    /** @brief Creates all used actions. */
    void setupActions();

    /**
     * @brief Gets an action with string and icon updated to the current settings.
     *
     * @param actionName The name of the action to return updated.
     * @return The updated action.
     **/
    QAction* updatedAction( const QString &actionName );

    /** @brief Updates the KMenuAction used for the filters action. */
    void updateFilterMenu();

    /** @brief Updates the KMenuAction used for the journeys menu. */
    void updateJourneyMenu();

    /** @brief Call after creating a new alarm, to update the UI accordingly. */
    void alarmCreated();

    /** @brief Generates tooltip data and registers this applet at plasma's TooltipManager. */
    void createTooltip();

    /**
     * @brief Gets the text to be displayed on the bottom of the timetable.
     *
     * Contains courtesy information and is HTML formatted.
     **/
    QString infoText();

    /** @brief Gets the text to be displayed as tooltip for the info label. */
    QString courtesyToolTip() const;

    /**
     * @brief Disconnects a currently connected departure/arrival data source and
     *   connects a new source using the current configuration.
     *
     * @ingroup models
     **/
    void reconnectSource();

    /**
     * @brief Disconnects a currently connected departure/arrival data source.
     *
     * @ingroup models
     **/
    void disconnectSources();

    /**
     * @brief Disconnects a currently connected journey data source and connects
     *   a new source using the current configuration.
     *
     * @ingroup models
     **/
    void reconnectJourneySource( const QString &targetStopName = QString(),
                                 const QDateTime &dateTime = QDateTime::currentDateTime(),
                                 bool stopIsTarget = true, bool timeIsDeparture = true,
                                 bool requestStopSuggestions = false );

    /**
     * @brief Handles errors from the publictransport data engine for @p data from source @p sourceName.
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

    /** @brief Read stop suggestions from the data engine. */
    void processOsmData( const QString &sourceName, const Plasma::DataEngine::Data& data );

    /**
     * @brief Clears the departure list received from the data engine and displayed by the applet.
     *
     * @ingroup models
     **/
    void clearDepartures();

    /**
     * @brief Clears the journey list received from the data engine and displayed by the applet.
     *
     * @ingroup models
     **/
    void clearJourneys();

    /** @brief Sets an autogenerated alarm for the given departure/arrival. */
    void createAlarmSettingsForDeparture( const QPersistentModelIndex &modelIndex,
                                          bool onlyForCurrentWeekday = false );

    /** @brief Removes an autogenerated alarm from this departure/arrival if any. */
    void removeAlarmForDeparture( int row );

private:
    QString queryNetworkStatus();

    /**
     * @brief Shows an error message when no interface is activated.
     *
     * @return True, if no message is shown. False, otherwise.
     **/
    bool checkNetworkStatus();

    /**
     * @brief Gets a list of current departures/arrivals for the selected stop(s).
     *
     * @param includeFiltered Whether or not to include filtered departures in the returned list.
     * @param max -1 to use the maximum departure count from the current settings. Otherwise this
     *   gets used as the maximal number of departures for the returned list.
     **/
    QList<DepartureInfo> departureInfos( bool includeFiltered = false, int max = -1 ) const;

    QString stripDateAndTimeValues( const QString &sourceName ) const;

    /** @brief Sets values of the current plasma theme. */
    void useCurrentPlasmaTheme();

    /**
     * @brief Creates a menu action, which lists all stops in the settings
     *   for switching between them.
     *
     * @param parent The parent of the menu action and it's sub-actions.
     * @param destroyOverlayOnTrigger True, if the overlay widget should be
     *   destroyed when triggered. Defaults to false.
     *
     * @return KSelectAction* The created menu action.
     **/
    KSelectAction *switchStopAction( QObject *parent,
                                     bool destroyOverlayOnTrigger = false ) const;

    QVariantHash currentServiceProviderData() const;
    QVariantHash serviceProviderData( const QString &id ) const;

    void setupStateMachine();
    Plasma::Animation *fadeOutOldAppearance();

    QGraphicsWidget *m_graphicsWidget, *m_mainGraphicsWidget;
    GraphicsPixmapWidget *m_oldItem;
    TitleWidget *m_titleWidget; // A widget used as the title of the applet.
    Plasma::Label *m_labelInfo; // A label used to display additional information.

    TimetableWidget *m_timetable; // The graphics widget showing the departure/arrival board.
    JourneyTimetableWidget *m_journeyTimetable; // The graphics widget showing journeys.

    Plasma::Label *m_labelJourneysNotSupported; // A label used to display an info about unsupported journey search.
    JourneySearchSuggestionWidget *m_listStopSuggestions; // A list of stop suggestions for the current input.
    OverlayWidget *m_overlay;
    Plasma::Svg m_vehiclesSvg; // An SVG containing SVG elements for vehicle types.

    DepartureModel *m_model; // The model containing the departures/arrivals.
    QHash< QString, QList<DepartureInfo> > m_departureInfos; // List of current departures/arrivals for each stop.
    PopupIcon *m_popupIcon;
    QParallelAnimationGroup *m_titleToggleAnimation; // Hiding/Showing the title on resizing
    QHash< int, QString > m_stopIndexToSourceName; // A hash from the stop index to the source name.
    QStringList m_currentSources; // Current source names at the publictransport data engine.

    JourneyModel *m_modelJourneys; // The model for journeys from or to the "home stop".
    QList<JourneyInfo> m_journeyInfos; // List of current journeys.
    QString m_currentJourneySource; // Current source name for journeys at the publictransport data engine.
    QString m_journeyTitleText;

    QString m_lastSecondStopName; // The last used second stop name for journey search.
    QDateTime m_lastJourneyDateTime; // The last used date and time for journey search.

    QDateTime m_lastSourceUpdate; // The last update of the data source inside the data engine.
    QUrl m_urlDeparturesArrivals, m_urlJourneys; // Urls to set as associated application urls,
            // when switching from/to journey mode.

    Settings m_settings; // Current applet settings.
    int m_originalStopIndex; // Index of the stop before showing an intermediate list via context menu.
    QStringList m_currentServiceProviderFeatures;

    QPersistentModelIndex m_clickedItemIndex; // Index of the clicked item in departure view
            // for the context menu actions.

    QActionGroup *m_filtersGroup; // An action group to toggle filter configurations.
    QActionGroup *m_colorFiltersGroup; // An action group to toggle color groups.

    DepartureProcessor *m_departureProcessor;
    DeparturePainter *m_departurePainter;

    // State machine, states and dynamic transitions
    QStateMachine *m_stateMachine;
    QHash< QString, QState* > m_states;
    QAbstractTransition *m_journeySearchTransition1;
    QAbstractTransition *m_journeySearchTransition2;
    QAbstractTransition *m_journeySearchTransition3;

    KProcess *m_marble;
    qreal m_longitude, m_latitude; // Coordinates from openstreetmap for a given stop
};

#ifndef NO_EXPORT_PLASMA_APPLET // Needed for settings.cpp to include publictransport.h
// This is the command that links the applet to the .desktop file
K_EXPORT_PLASMA_APPLET( publictransport, PublicTransport )
#endif


/** @mainpage Public Transport Applet
@section intro_applet_sec Introduction
This applet shows a departure / arrival board for public transport, trains, ferries and planes.
Journeys can also be searched for. It uses the public transport data engine and has some advanced
configuration possibilities like filters, alarms and a flexible appearance.

@see models for more information about how the applet interacts with the PublicTransport data engine and how the data is stored in models.
@see filterSystem for more information about how the filters work.

@section install_applet_sec Installation
To install this applet type the following commands:<br>
\> cd /path-to-extracted-applet-sources/build<br>
\> cmake -DCMAKE_INSTALL_PREFIX=`kde4-config --prefix` ..<br>
\> make<br>
\> make install<br>
<br>
After installation do the following to use the applet in your plasma desktop:
Restart plasma to load the applet:<br>
\> kquitapp plasma-desktop<br>
\> plasma-desktop<br>
<br>
or test it with:<br>
\> plasmoidviewer publictransport<br>
<br>
You might need to run kbuildsycoca4 in order to get the .desktop file recognized.
*/

/**
* @defgroup models Models
@brief Data is retrieved from data engines (like the publictransport data engine), processed in a thread and stored in models.

The models used for storing public transport data are: @ref DepartureModel for departures/arrivals
and @ref JourneyModel for journeys. They are both based on @ref PublicTransportModel.

The applet uses five data engines: <em>publictransport</em>, <em>geolocation</em>,
<em>openstreetmap</em> (to get stops near the user), <em>favicons</em> (to get favicons from the
service providers) and <em>network</em> (to check the network status).
The publictransport data engine expects data source names in a specific format, which is explained
in detail in it's documentation. Here are some examples of what source names the applet generates
(based on the settings):
@par
<em>"Departures de_db|stop=Leipzig|timeOffset=5"</em> for departures from the service provider with
the ID "de_db", a stop named "Leipzig" and an offset (from now) in minutes for the first departure
of 5.
@par
<em>"Journeys de_db|originStop=Leipzig|targetStop=Bremen"</em> for journeys from the service
provider with the ID "de_db", an origin stop named "Leipzig" and a target stop named "Bremen".

@note The service provider ID <em>"de_db"</em> can be left away to use a default service provider
for the users country (from KDE's global settings).

The format of the data structure returned from the data engine is again explained in detail in the
data engine's documentation (@ref pageUsage). It arrives in the slot
@ref PublicTransport::dataUpdated. From there one of the following functions is called, based on
the data returned by the data engine:
@par
@ref PublicTransport::handleDataError, if the "error" key of the data structure is true, ie. there
was an error while running the query in the data engine (eg. server not reachable or an error in
the accessor while trying to parse the document from the server),
@par
@ref PublicTransport::processStopSuggestions, if the "receivedPossibleStopList" key of the data
structure is true, which can also happen if eg. "Departures" were queried for, but the stop name
is ambigous,
@par
@ref DepartureProcessor::processJourneys, @ref DepartureProcessor::processDepartures if there's
no error and no stoplist, but "parseMode" is "journeys" or "departures" (also for arrivals). A new
job is added to the background thread. The thread then reads the data and creates data structures
of type @ref DepartureInfo for departures/arrivals or @ref JourneyInfo for journeys. It also checks
for alarms and applies filters. That way complex filters and or many alarms applied to many
departures/arrivals won't freeze the applet.

Before beginning a new departure/arrival/journey job the thread emits a signal that is connected
to @ref PublicTransport::beginDepartureProcessing / @ref PublicTransport::beginJourneyProcessing.
Once a chunk of departures/arrivals is ready @ref PublicTransport::departuresProcessed gets called
through a signal/slot connection. In that function the processed departures are cached based on
the source name (but with date and time values stripped) and then the departure/arrival model gets
filled with them in @ref PublicTransport::fillModel. If journeys are ready
@ref PublicTransport::journeysProcessed gets called by the thread, which calls
@ref PublicTransport::fillModelJourney. If filter settings are changed the thread is used again to
run filters on the current data. Once the filter job is ready it calls
@ref PublicTransport::departuresFiltered.

The @ref PublicTransport::fillModel and @ref PublicTransport::fillModelJourney functions add,
update and/or remove items in the models. Both the departure/arrival and the journey model have
functions called @ref DepartureModel::indexFromInfo / @ref JourneyModel::indexFromInfo, which use
a hash generated from the data items (@ref DepartureInfo / @ref JourneyInfo) to quickly check, if
there already is an item in the model for a given data item. Hashes are generated automatically in
the constructors and can be retrieved using @ref PublicTransportInfo::hash. Two data items don't
have to be exactly equal to generade an equal hash. That is important to also find
departures/arrivals/journeys which data has changed since the last update, eg. departures with a
changed delay.

@see filterSystem
*/

/**
* @defgroup filterSystem Filter System
@brief The applet has the possibility to filter departures/arrivals based on various constraints.

Those constraints are combined to filters using logical AND. Filters on the other hand can be
combined to filter lists using logical OR. The filter system is also used to match alarms.

The filtering is performed in classes described under @ref filter_classes_sec, while those filters
can be setup using widgets described under @ref filter_widgets_sec.

@n
@section filter_classes_sec Classes That Perform Filtering
The lowest class in the hierarchy of filter classes is @ref Constraint, which describes a single
constraint which should match the departures/arrivals to be shown/hidden. One step higher
in the hierarchy comes the class @ref Filter, which is a list of constraints (combined using
logical AND). Another step higher comes @ref FilterList, which is a list of filters (combined
using logical OR). A @ref FilterList is wrapped by an object of type @ref FilterSettings together
with the @ref FilterAction (show or hide matching departures/arrivals), name and affected stops
for that filter configuration.

Each @ref Constraint has a @ref FilterType, ie. what to filter with this constraint. For example
a constraint can filter departures/arrivals by the used vehicle type using @ref FilterByVehicleType.
Each @ref Constraint also has a @ref FilterVariant, eg. equals / doesn't equal. The used
@ref FilterVariant affects the way a constraint matches specific departures/arrivals. Last but not
least each @ref Constraint has a value.
So for example a constraint can be assembled like this: Filter by vehicle type, match
departures/arrivals that have the same value as stored in the constraint.

Filters can be serialized using toData() / fromData() methods.
Filter widgets described in the next section can be easily created from these filter classes.

@n
@section filter_widgets_sec Widgets for Editing Filters
There are accompanying QWidget based classes for the filter classes from the previous section:
@par
@ref ConstraintWidget for @ref Constraint, @ref FilterWidget for @ref Filter and
@ref FilterListWidget for @ref FilterList. Filter widgets can be constructed from the filter
classes of the previous section.

For each constraint data type there is a separate constraint widget class:
@par
@ref ConstraintListWidget to select values of a given list of values (eg. a list of vehicle types),
@par
@ref ConstraintStringWidget to enter a string for matching (eg. matching intermediate stops),
@par
@ref ConstraintIntWidget to enter an integer for matching and
@par
@ref ConstraintTimeWidget to enter a time for matching (eg. a departure time, used for alarms).

@ref FilterWidget uses @ref AbstractDynamicLabeledWidgetContainer as base class to allow dynamic
adding / removing of constraints. @ref FilterListWidget uses @ref AbstractDynamicWidgetContainer
to do the same with filters. Those base classes automatically add buttons to let the user
add / remove widgets. They are pretty flexible and will maybe end up in a library later for
reuse in other projects (like the publictransport runner).
*/

/** @page pageClassDiagram Class Diagram
@dot
digraph publicTransportDataEngine {
    ratio="compress";
    size="10,100";
    concentrate="true";
    // rankdir="LR";
    clusterrank=local;

    node [
    shape=record
    fontname=Helvetica, fontsize=10
    style=filled
    fillcolor="#eeeeee"
    ];

    applet [
    fillcolor="#ffdddd"
    label="{PublicTransportApplet|Shows a departure / arrival list or a list of journeys.\l|+ dataUpdated( QString, Data ) : void\l# createTooltip() : void\l# updatePopupIcon() : void\l# processData( Data ) : void\l}"
    URL="\ref PublicTransport"
    ];

    subgraph clusterWidgets {
        label="Widgets";
        style="rounded, filled";
        color="#cceeee";
        node [ fillcolor="#ccffff" ];

        timetableWidget [
        label="{TimetableWidget|Represents the departure/arrial board.\l}"
        URL="\ref TimetableWidget"
        ];

        departureGraphicsItem [
        label="{DepartureGraphicsItem|Represents one item in the departure/arrial board.\l}"
        URL="\ref DepartureGraphicsItem"
        ];

        journeyTimetableWidget [
        label="{JourneyTimetableWidget|Represents the journey board.\l}"
        URL="\ref JourneyTimetableWidget"
        ];

        journeyGraphicsItem [
        label="{JourneyGraphicsItem|Represents one item in the journey board.\l}"
        URL="\ref JourneyGraphicsItem"
        ];

        titleWidget [
        label="{TitleWidget|Represents the title of the applet.\l}"
        URL="\ref TitleWidget"
        ];

        routeGraphicsItem [
        label="{RouteGraphicsItem|Represents the route item in a departure/arrival item.\l}"
        URL="\ref RouteGraphicsItem"
        ];

        journeyRouteGraphicsItem [
        label="{JourneyRouteGraphicsItem|Represents the route item in a journey item.\l}"
        URL="\ref JourneyRouteGraphicsItem"
        ];
    };

    subgraph thread {
        label="Background Thread";
        style="rounded, filled";
        color="#ffcccc";
        node [ fillcolor="#ffdfdf" ];

        departureProcessor [
        label="{DepartureProcessor|Processes data from the data engine and applies filters/alarms.\l}"
        URL="\ref DepartureProcessor"
        ];
    };

    subgraph clusterSettings {
        label="Settings";
        style="rounded, filled";
        color="#ccccff";
        node [ fillcolor="#dfdfff" ];

        settings [
            label="{PublicTransportSettings|Manages the settings of the applet.\l}"
            URL="\ref PublicTransportSettings"
            ];

        dataSourceTester [
            label="{DataSourceTester|Tests a departure / arrival or journey data source \lat the public transport data engine.\l|+ setTestSource( QString ) : void\l+ testResult( TestResult, QVariant ) : void [signal] }"
            URL="\ref DataSourceTester"
            ];
    };

    subgraph clusterModels {
        label="Models";
        style="rounded, filled";
        color="#ccffcc";
        node [ fillcolor="#dfffdf" ];
        rank="sink";

        departureModel [
            label="{DepartureModel|Stores information about a departures / arrivals.\l}"
            URL="\ref DepartureModel"
            ];

        journeyModel [
            label="{JourneyModel|Stores information about a journeys.\l}"
            URL="\ref JourneyModel"
            ];

        departureItem [
            label="{DepartureItem|Wraps DepartureInfo objects for DepartureModel.\l}"
            URL="\ref DepartureItem"
            ];

        journeyItem [
            label="{JourneyItem|Wraps JourneyInfo objects for JourneyModel.\l}"
            URL="\ref JourneyItem"
            ];

        departureInfo [
            label="{DepartureInfo|Stores information about a single departure / arrival.\l}"
            URL="\ref DepartureInfo"
            ];

        journeyInfo [
            label="{JourneyInfo|Stores information about a single journey.\l}"
            URL="\ref JourneyInfo"
            ];
    };

    edge [ dir=back, arrowhead="normal", arrowtail="none", style="dashed", fontcolor="darkgray",
           taillabel="", headlabel="0..*" ];
    timetableWidget -> departureGraphicsItem [ label="uses" ];
    journeyTimetableWidget -> journeyGraphicsItem [ label="uses" ];
    departureModel -> departureItem [ label="uses" ];
    journeyModel -> journeyItem [ label="uses" ];

    edge [ dir=forward, arrowhead="none", arrowtail="normal", style="dashed", fontcolor="darkgray",
           taillabel="1", headlabel="" ];
    departureProcessor -> applet [ label="m_departureProcessor" ];
    settings -> applet [ label="m_settings" ];
    dataSourceTester -> settings [ label="m_dataSourceTester" ];

    edge [ dir=back, arrowhead="normal", arrowtail="none", style="dashed", fontcolor="darkgray",
           taillabel="", headlabel="1" ];
    applet -> timetableWidget [ label="m_timetable" ];
    applet -> journeyTimetableWidget [ label="m_journeyTimetable" ];
    applet -> titleWidget [ label="m_titleWidget" ];
    applet -> departureModel [ label="m_model" ];
    applet -> journeyModel [ label="m_modelJourneys" ];
    departureItem -> departureInfo [ label="uses" ];
    journeyItem -> journeyInfo [ label="uses" ];
    departureGraphicsItem -> routeGraphicsItem [ label="uses" ];
    journeyGraphicsItem -> journeyRouteGraphicsItem [ label="uses" ];
}
@enddot
*/

#endif
