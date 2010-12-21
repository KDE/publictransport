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

/** @file
* @brief This file contains the public transport applet.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef PUBLICTRANSPORT_HEADER
#define PUBLICTRANSPORT_HEADER

// Plasma includes
#include <Plasma/PopupApplet>

// Own includes
#include "global.h"
#include "departureinfo.h"
#include "settings.h"

class JourneyModel;
class DepartureModel;
class DepartureItem;
class DepartureProcessor;
class PublicTransportSettings;
class OverlayWidget;

class KSelectAction;
namespace Plasma {
	class IconWidget;
	class TreeView;
	class LineEdit;
	class ToolButton;
	class Label;
	class PushButton;
};

class QSizeF;
class QGraphicsLayout;
class QStandardItemModel;
class QStandardItem;
#if QT_VERSION >= 0x040600
class QGraphicsBlurEffect;
#endif

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

/** @class PublicTransport
* @brief Shows departure / arrival times for public transport. It uses the "publictransport"-data engine. */
class PublicTransport : public Plasma::PopupApplet {
	Q_PROPERTY( int DepartureCount READ departureCount )
	Q_OBJECT

public:
	/** @brief Basic create. */
	PublicTransport( QObject *parent, const QVariantList &args );
	/** @brief Destructor. Saves the state of the header. */
	~PublicTransport();

	/** @brief Maximum number of recent journey searches. When more journey searches
	 * get added, the oldest when gets removed. */
	static const int MAX_RECENT_JOURNEY_SEARCHES = 10;

	/** @brief Returns the widget with the contents of the applet. */
	virtual QGraphicsWidget* graphicsWidget();

	/** @brief Tests the given state.
	 * @param state The state to test.
	 * @returns True, if the state is set. False, otherwise.*/
	virtual bool testState( AppletState state ) const {
		return m_appletStates.testFlag( state ); };
	/** @brief Adds the given state. Operations are processed to set the new applet state.
	 * @param state The state to add. */
	virtual void addState( AppletState state );
	/** @brief Removes the given state. Operations are processed to unset the new applet state.
	 * @param state The state to remove. */
	virtual void removeState( AppletState state );

	/** @returns the number of shown departures/arrivals. */
	int departureCount() const { return m_departureInfos.count(); };

signals:
	/** @brief Emitted when the settings have changed. */
	void settingsChanged();

public slots:
	/** @brief Initializes the applet. */
	virtual void init();

	void setSettings( const QString &serviceProviderID, const QString &stopName );

protected slots:
	/** @brief The geometry of the applet has changed. */
	void geometryChanged();

	/** @brief Settings have changed. */
	void configChanged();
	/** @brief Settings that require a new data request have been changed. */
	void serviceProviderSettingsChanged();

	/** @brief New @p data arrived from the data engine for the source with the name @p sourceName.
	 * @ingroup models */
	void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

	/** This will stop if the maximum departure count is reached
	* or if all @p departures have been added.
	* @brief Fills the departure/arrival data model with the given departure/arrival list without removing departures that were already in the model.
	* @ingroup models */
	void fillModel( const QList<DepartureInfo> &departures );
	/** This will stop if all @p journeys have been added.
	* @brief Fills the journey data model with the given journey list without removing departures that were already in the model.
	* @ingroup models */
	void fillModelJourney( const QList<JourneyInfo> &journeys );

	/** @brief The context menu has been requested by the tree view. */
	void showDepartureContextMenu( const QPoint &position );
	/** @brief The context menu has been requested by the tree view header. */
	void showHeaderContextMenu( const QPoint &position );
	/** @brief An item in the tree view has been double clicked. */
	void doubleClickedItem( const QModelIndex &modelIndex );
	void noItemsTextClicked();

	/** @brief The icon widget was clicked. */
	void iconClicked();
	/** @brief The icon widget to close the journey view was clicked. */
	void iconCloseClicked();
	/** @brief The filter icon widget was clicked. */
	void filterIconClicked();

	void infoLabelLinkActivated( const QString &link );

	/** Finished editing the journey search line (return pressed, start search
	* button clicked or stop suggestion double clicked). */
	void journeySearchInputFinished();
	/** The journey search line has been edited. */
	void journeySearchInputEdited( const QString &newText );
	void possibleStopItemActivated( const QModelIndex &modelIndex );
	void possibleStopClicked( const QModelIndex &modelIndex );
	void possibleStopDoubleClicked( const QModelIndex &modelIndex );

	/** @brief Called from PublicTransportSettings to indicate the need to clear the
	 * departure list. */
	void setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType );

	/** @brief The action to update the data source has been triggered. */
	void updateDataSource();
	/** @brief The action to set an alarm for the selected departure/arrival has been triggered. */
	void setAlarmForDeparture();
	/** @brief The action to remove an alarm from the selected departure/arrival has been triggered. */
	void removeAlarmForDeparture();

	/** @brief The action to switch to the journey search mode has been triggered. */
	void showJourneySearch();
	/** @brief The action to go back to the departure/arrival view has been triggered. */
	void goBackToDepartures();
	/** @brief The action to show the action button overlay has been triggered. */
	void showActionButtons();

	/** @brief The action to expand / collapse of the selected departure/arrival has been triggered. */
	void toggleExpanded();
	/** @brief The action to hide the header of the tree view has been triggered. */
	void hideHeader();
	/** @brief The action to show the header of the tree view has been triggered. */
	void showHeader();
	/** @brief The action to hide the direction column of the tree view header has been triggered. */
	void hideColumnTarget();
	/** @brief The action to show the direction column of the tree view header has been triggered. */
	void showColumnTarget();
	/** @brief The plasma theme has been changed. */
	void themeChanged() { useCurrentPlasmaTheme(); };

	void recentJourneyActionTriggered( QAction *action );

	/** @brief The section with the given @p logicalIndex in the departure tree
	 * view was pressed. */
	void sectionPressed( int logicalIndex );
	/** @brief The section with the given @p logicalIndex in the departure tree
	 * view was move from @p oldVisualIndex to @p newVisualIndex. */
	void sectionMoved( int logicalIndex, int oldVisualIndex, int newVisualIndex );
	/** @brief The section with the given @p logicalIndex in the departure tree
	 * view was resized from @p oldSize to @p newSize. */
	void sectionResized( int logicalIndex, int oldSize, int newSize );

	/** @brief The worker thread starts processing departures/arrivals from the
	 * data engine.
	 * @param sourceName The data engine source name for the departure data.
	 * @ingroup models */
	void beginDepartureProcessing( const QString &sourceName );
	/** @brief The worker thread has finished processing departures/arrivals.
	 * @param sourceName The data engine source name for the departure data.
	 * @param departures A list of departures that were read by the worker thread.
	 * @param requestUrl The url that was used to download the departure data.
	 * @param lastUpdate The date and time of the last update of the data.
	 * @ingroup models */
	void departuresProcessed( const QString &sourceName, const QList< DepartureInfo > &departures,
							  const QUrl &requestUrl, const QDateTime &lastUpdate );
	/** @brief The worker thread has finished filtering departures.
	 * @param sourceName The data engine source name for the departure data.
	 * @param departures The list of departures that were filtered. Each
	 * departure now returns the correct value with isFilteredOut() according
	 * to the filter settings given to the worker thread.
	 * @param newlyFiltered A list of departures that should be made visible
	 * to match the current filter settings.
	 * @param newlyNotFiltered A list of departures that should be made
	 * invisible to match the current filter settings.
	 * @ingroup models */
	void departuresFiltered( const QString &sourceName, const QList< DepartureInfo > &departures,
							 const QList< DepartureInfo > &newlyFiltered,
							 const QList< DepartureInfo > &newlyNotFiltered );
	/** @brief The worker thread starts processing journeys from the data engine.
	 * @ingroup models */
	void beginJourneyProcessing( const QString &sourceName );
	/** @brief The worker thread has finished processing journeys.
	 * @param sourceName The data engine source name for the journey data.
	 * @param journeys A list of journeys that were read by the worker thread.
	 * @param requestUrl The url that was used to download the journey data.
	 * @param lastUpdate The date and time of the last update of the data.
	 * @ingroup models */
	void journeysProcessed( const QString &sourceName, const QList< JourneyInfo > &journeys,
							const QUrl &requestUrl,
							const QDateTime &lastUpdate );

	void oldItemAnimationFinished();

	/** @brief Deletes the overlay item used when showing action buttons over the plasmoid. */
	void destroyOverlay();

	/** @brief An action to change the currently shown stop has been triggered. */
	void setCurrentStopIndex( QAction *action );

	/** @brief Writes the given @p settings and updates the applet accordingly.
	 * @param settings The settings to write. */
	void writeSettings( const Settings &settings );

	/** @brief Write new settings with @ref Settings::departureArrivalListType set
	 * to @p DepartureList. This also updates the departure tree view on
	 * @ref configChanged. */
	void setShowDepartures();
	/** @brief Write new settings with @ref Settings::departureArrivalListType set
	 * to @p ArrivalList. This also updates the departure tree view on
	 * @ref configChanged. */
	void setShowArrivals();

	/** @brief Switch the currently active filter configuration to @p newFilterConfiguration. */
	void switchFilterConfiguration( const QString &newFilterConfiguration );
	/** @brief An action to change the currently active filter configuration has been triggered. */
	void switchFilterConfiguration( QAction *action );
	/** @brief Changes the filters enabled state to @p enable. */
	void setFiltersEnabled( bool enable );

	/** @brief An alarm has been fired for the given @p item. */
	void alarmFired( DepartureItem *item );
	void removeAlarms( const AlarmSettingsList &newAlarmSettings,
					   const QList<int> &removedAlarms );

protected:
	/** @brief Create the configuration dialog contents.
	 * @param parent The config dialog in which the config interface should be created. */
	void createConfigurationInterface( KConfigDialog *parent );

	/** @brief Gets a list of actions for the context menu. */
	virtual QList<QAction*> contextualActions();
	/** @brief The popup gets shown or hidden. */
	virtual void popupEvent( bool show );

	virtual bool sceneEventFilter( QGraphicsItem* watched, QEvent* event );
	/** @brief Watching for up/down key presses in m_journeySearch to select stop suggestions. */
	virtual bool eventFilter( QObject* watched, QEvent* event );

	/** @brief Creates all used QAction's. */
	void setupActions();
	/** @brief Gets an action with string and icon updated to the current settings.
	 * @param actionName The name of the action to return updated.
	 * @return The updated action.*/
	QAction* updatedAction( const QString &actionName );

	/** @brief Generates tooltip data and registers this applet at plasma's TooltipManager. */
	void createTooltip();
	/** @brief Creates the popup icon with information about the next departure / alarm. */
	void createPopupIcon();

	/** Gets the first departure / arrival that doesn't get filtered out. */
	DepartureInfo getFirstNotFilteredDeparture();

	/** Sets the icon that should be painted as main icon.
	* @param mainIconDisplay The icon to be set. */
	void setMainIconDisplay( MainIconDisplay mainIconDisplay );

	/** Gets the text to be displayed above the treeview as title (html-tags allowed). */
	QString titleText() const;
	/** Gets the text to be displayed on right of the treeview as additional
	* information (html-tags allowed). Contains courtesy information. */
	QString infoText();
	/** @brief Gets the text to be displayed as tooltip for the info label. */
	QString courtesyToolTip() const;

	/** @brief Disconnects a currently connected departure/arrival data source and
	 * connects a new source using the current configuration.
	 * @ingroup models*/
	void reconnectSource();
	/** @brief Disconnects a currently connected departure/arrival data source.
	 * @ingroup models */
	void disconnectSources();

	/** @brief Disconnects a currently connected journey data source and connects
	 * a new source using the current configuration.
	 * @ingroup models */
	void reconnectJourneySource( const QString &targetStopName = QString(),
								 const QDateTime &dateTime = QDateTime::currentDateTime(),
								 bool stopIsTarget = true, bool timeIsDeparture = true,
								 bool requestStopSuggestions = false );
	/** @brief Disconnects a currently connected journey data source.
	 * @ingroup models */
	void disconnectJourneySource();

	/** @brief Handles errors from the publictransport data engine for @p data from source @p sourceName.
	 * @ingroup models */
	void handleDataError( const QString &sourceName, const Plasma::DataEngine::Data& data );
	/** @brief Read stop suggestions from the data engine.
	 * @ingroup models */
	void processStopSuggestions( const QString &sourceName, const Plasma::DataEngine::Data& data );

	/** @brief Clears the departure / arrival list received from the data engine and
	 * displayed by the applet.
	 * @ingroup models */
	void clearDepartures();
	/** @brief Clears the journey list received from the data engine and displayed by
	 * the applet.
	 * @ingroup models */
	void clearJourneys();

	/** @brief Appends a new departure / arrival to the model.
	 * @param departureInfo The departure / arrival to be added.
	 * @ingroup models */
	void appendDeparture( const DepartureInfo &departureInfo );
	/** Appends a new journey to the model.
	 * @ingroup models */
	void appendJourney( const JourneyInfo &journeyInfo );

	/** @brief Sets an autogenerated alarm for the given departure/arrival. */
	void createAlarmSettingsForDeparture( const QPersistentModelIndex &modelIndex );
	/** @brief Removes an autogenerated alarm from this departure/arrival if any. */
	void removeAlarmForDeparture( int row );

	/** @brief Calls setFirstColumnSpanned on each child item and their children. */
	void stretchAllChildren( const QModelIndex &parent, QAbstractItemModel *model );

	/** @brief Helper function to set the text color of an html item with a surrounding span-tag. */
	void setTextColorOfHtmlItem( QStandardItem *item, const QColor &textColor );

	QGraphicsWidget *widgetForType( TitleType titleType );
	/** Sets the type of title to be displayed. */
	void setTitleType( TitleType titleType );
	/** Creates a layout for the given title type. */
	QGraphicsLayout *createLayoutTitle( TitleType titleType );

	void updateFilterWidget();

	/** Unsets the given states. No other operations are processed.
	* @param states A list of states to unset. */
	virtual void unsetStates( QList<AppletState> states );

private:
	/** @brief Types of messages. */
	enum MessageType {
		MessageNone = 0, /**< No message. */
		MessageError, /**< An error message. */
		MessageErrorResolved /**< A message about a resolved error. */
	};
	/** @brief Statuses of the network */
	enum NetworkStatus {
		StatusUnknown = 0, /**< Network status is unknown. */
		StatusUnavailable, /**< Network is unavailable. */
		StatusConfiguring, /**< Network is being configured. */
		StatusActivated /**< Network is activated. */
	};

	NetworkStatus queryNetworkStatus();
	/** @brief Shows an error message when no interface is activated.
	 * @return True, if no message is shown. False, otherwise. */
	bool checkNetworkStatus();

	/** Checks if a departure with the given @p dateTime will be shown,
	* depending on the settings (first departure, maybe at a custom time). */
	bool isTimeShown( const QDateTime &dateTime ) const;

	/** List of current departures / arrivals for the selected stop(s). */
	QList<DepartureInfo> departureInfos() const;
	QString stripDateAndTimeValues( const QString &sourceName ) const;

	void addJourneySearchCompletions();
	void removeSuggestionItems();
	void addAllKeywordAddRemoveitems( QStandardItemModel *model = 0 );
	void maybeAddKeywordAddRemoveItems( QStandardItemModel *model,
			const QStringList &words, const QStringList &keywords,
			const QString &type, const QStringList &descriptions,
			const QStringList &extraRegExps = QStringList() );
	void journeySearchItemCompleted( const QString &newJourneySearch,
			const QModelIndex &index = QModelIndex(), int newCursorPos = -1 );

	/** Sets values of the current plasma theme. */
	void useCurrentPlasmaTheme();

	/** @brief Creates a menu action, which lists all stops in the settings
	 * for switching between them.
	 *
	 * @param parent The parent of the menu action and it's sub-actions.
	 * @param destroyOverlayOnTrigger True, if the overlay widget should be
	 * destroyed when triggered. Defaults to false.
	 * @return KSelectAction* The created menu action.
	 **/
	KSelectAction *switchStopAction( QObject *parent, bool destroyOverlayOnTrigger = false ) const;

	QVariantHash currentServiceProviderData() const {
		return serviceProviderData( m_settings.currentStopSettings().serviceProviderID ); };
	QVariantHash serviceProviderData( const QString &id ) const;

	void setHeightOfCourtesyLabel();

	void initTreeView( Plasma::TreeView *treeView );
	void createJourneySearchWidgets();


	AppletStates m_appletStates; /**< The current states of this applet */
	TitleType m_titleType; /**< The type of items to be shown as title above the tree view */

	QGraphicsWidget *m_graphicsWidget, *m_mainGraphicsWidget;
	GraphicsPixmapWidget *m_oldItem;
	Plasma::IconWidget *m_icon; /**< The icon that is displayed in the top left corner */
	Plasma::IconWidget *m_iconClose; /**< The icon that is displayed in the top right corner to close the journey view */
	Plasma::IconWidget *m_filterIcon; /**< The icon that is displayed in the top right corner, clickable to show the filter menu */
	Plasma::Label *m_filterLabel; /**< A label showing the currently used filter */
	QGraphicsWidget *m_filterWidget; /**< A widget showing the currently used filters */
	Plasma::Label *m_label; /**< A label used to display a title */
	Plasma::Label *m_labelInfo; /**< A label used to display additional information */
	Plasma::TreeView *m_treeView, *m_treeViewJourney; /**< A treeview displaying the departure board */
	Plasma::Label *m_labelJourneysNotSupported; /**< A label used to display an info about unsupported journey search */
	Plasma::LineEdit *m_journeySearch; /**< A line edit for inputting the target of a journey */
	Plasma::TreeView *m_listStopSuggestions; /**< A list of stop suggestions for the current input */
	Plasma::ToolButton *m_btnLastJourneySearches; /**< A tool button that shows last used journey searches. */
	Plasma::ToolButton *m_btnStartJourneySearch; /**< A tool button to start the journey search. */
	OverlayWidget *m_overlay;

	int m_journeySearchLastTextLength; /**< The last number of unselected characters in the journey search input field. */
	bool m_lettersAddedToJourneySearchLine; /**< Whether or not the last edit of the journey search line added letters o(r not. Used for auto completion. */

	DepartureModel *m_model; /**< The model for the tree view containing the departure / arrival board */
	QHash< QString, QList<DepartureInfo> > m_departureInfos; /**< List of current departures / arrivals for each stop */
	QHash< int, QString > m_stopIndexToSourceName; /**< A hash from the stop index to the source name */
	QStringList m_currentSources; /**< Current source names at the publictransport data engine */

	JourneyModel *m_modelJourneys; /**< The model for journeys from or to the "home stop" */
	QList<JourneyInfo> m_journeyInfos; /**< List of current journeys */
	QString m_currentJourneySource; /**< Current source name for journeys at the publictransport data engine */
	QString m_journeyTitleText;

	QString m_lastSecondStopName; /**< The last used second stop name for journey search */
	QDateTime m_lastJourneyDateTime; /**< The last used date and time for journey search */

	QDateTime m_lastSourceUpdate; /**< The last update of the data source inside the data engine */
	QUrl m_urlDeparturesArrivals, m_urlJourneys; /**< Urls to set as associated application urls, when switching from/to journey mode. */

	Settings m_settings; /**< Current applet settings. */
	QStringList m_currentServiceProviderFeatures;
	bool m_stopNameValid; /**< Wheather or not the current stop name (m_stop) is valid. */

	QPersistentModelIndex m_clickedItemIndex; /**< Index of the clicked item in the tree view
			* for the context menu actions. */

	QActionGroup *m_filtersGroup; /**< An action group to toggle between filter configurations. */

	MessageType m_currentMessage;
	DepartureProcessor *m_departureProcessor;
};

#ifndef NO_EXPORT_PLASMA_APPLET // Needed for settings.cpp to include publictransport.h
// This is the command that links the applet to the .desktop file
K_EXPORT_PLASMA_APPLET( publictransport, PublicTransport )
#endif


/** @mainpage Public Transport Applet
@section intro_applet_sec Introduction
This applet shows a departure / arrival board for public transport, trains, ferries and planes.
Journeys can also be searched for. It uses the public transport data engine.
It has some advanced configuration possibilities like filters, alarms and a flexible appearance.

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
 * \defgroup models Models
@brief Data is retrieved from data engines (like the publictransport data engine), processed in a thread and stored in models.
The applet uses four data engines: publictransport, geolocation, openstreetmap (to get stops near the user) and favicons (to get favicons from the service providers).
The publictransport data engine expects data source names in a specific format, which is explained in detail in it's documentation. Here are some examples of what source names the applet generates (based on the settings):
@par
<em>"Departures de_db|stop=Leipzig|timeOffset=5"</em> for departures from the service provider with the ID "de_db", a stop named "Leipzig" and an offset (from now) in minutes for the first departure of 5.
@par
<em>"Journeys de_db|originStop=Leipzig|targetStop=Bremen"</em> for journeys from the service provider with the ID "de_db", an origin stop named "Leipzig" and a target stop named "Bremen".

The format of the data structure returned from the data engine is again explained in detail in the data engine's documentation. It arrives in the slot @ref PublicTransport::dataUpdated. From there one of the following functions is called, based on the data returned by the data engine:
@ref handleDataError, if the "error" key of the data structure is true
@ref processStopSuggestions, if the "receivedPossibleStopList" key of the data structure is true, which can also happen if eg. "Departures" where queried for, but the stop name is ambigous
@ref DepartureProcessor::processJourneys, @ref DepartureProcessor::processDepartures to add the data in a job to the background thread

The thread then reads the data and creates data structures of type @ref DepartureInfo for departures/arrivals or @ref JourneyInfo for journeys. It also checks for alarms and applies filters. That way complex filters and or many alarms applied to many departures/arrivals won't freeze the applet.

Before beginning a new departure/arrival/journey job the thread calls @ref PublicTransport::beginDepartureProcessing or @ref PublicTransport::beginJourneyProcessing. Once a chunk of departures/arrivals is ready the thread calls @ref PublicTransport::departuresProcessed. In that function the processed departures are cached based on the source name (but with date and time values stripped) and then the departure/arrival model gets filled with them in @ref PublicTransport::fillModel. If journeys are ready @ref PublicTransport::journeysProcessed gets called by the thread, which calls @ref PublicTransport::fillModelJourney. If filter settings are changed the thread is used again to run filters on the current data. Once the filter job is ready it calls @ref PublicTransport::departuresFiltered.

The @ref PublicTransport::fillModel and @ref PublicTransport::fillModelJourney functions add, update and/or remove items in the models. Both the departure/arrival and the journey model have funtions called @ref DepartureModel::indexFromInfo / @ref JourneyModel::indexFromInfo, which use a hash generated from the data items (@ref DepartureInfo / @ref JourneyInfo) to quickly check, if there already is an item in the model for a given data item. Hashes are generated automatically in @ref DepartureInfo::generateHash / @ref JourneyInfo::generateHash, the hash of an item can be retrieved using @ref PublicTransportInfo::hash. Two data items don't have to be exactly equal to generade an equal hash. That is important to also find departures/arrivals/journeys which data has changed since the last update, eg. departures with a changed delay.
*/

/**
 * \defgroup filterSystem Filter System
@brief The applet has the possibility to filter departures/arrivals based on various constraints. Those constraints are combined to filters using logical AND. Filters on the other hand can be combined to filter lists using logical OR. The filter system is also used to match alarms.

The filtering is performed in classes described under @ref filter_classes_sec,
while those filters can be setup using widgets described under @ref filter_widgets_sec.

@section filter_classes_sec Classes That Perform Filtering
The lowest class in the hierarchy of filter classes is @ref Constraint, which describes a single
constraint which should or should not match the departures/arrivals to be shown. One step higher
in the hierarchy comes the class @ref Filter, which is a list of constraints (combined using
logical AND). Another step higher comes @ref FilterList, which is a list of filters (combined
using logical OR). A @ref FilterList is wrapped by an object of type @ref FilterSettings together
with the @ref FilterAction for that filter configuration (show or hide matching
departures/arrivals).

Each @ref Constraint has a @ref FilterType, ie. what to filter with this constraint. For example
a constraint can filter departures/arrivals by the used vehicle type using @ref FilterByVehicleType.
Each @ref Constraint also has a @ref FilterVariant, eg. equals / doesn't equal. The used
@ref FilterVariant affects the way a constraint matches specific departures/arrivals. Last but not
least each @ref Constraint has a value.
So for example a constraint can be assembled like this: Filter by vehicle type, match
departures/arrivals that have the same value as stored in the constraint.

Filters can be serialized using toData() / fromData() methods.
Filter widgets described in the next section can be easily created from these filter classes.

@section filter_widgets_sec Widgets for Editing Filters
There are accompanying widget filter classes for the filter classes from the previous section:
@ref ConstraintWidget for @ref Constraint, @ref FilterWidget for @ref Filter and
@ref FilterListWidget for @ref FilterList. Filter widgets can be constructed from the filter
classes of the previous section.

For each constraint data type there is a separate constraint widget class:
@ref ConstraintListWidget to select values of a given list of values,
@ref ConstraintStringWidget to enter a string for matching (eg. matching intermediate stops),
@ref ConstraintIntWidget to enter an integer for matching and
@ref ConstraintTimeWidget to enter a time for matching.

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

    appletState [
	label="{AppletWithState|This is just a Plasma::PopupApplet with some methods for \lstate checking / setting.\l|+ testState(state) : bool\l+ addState(state) : void\l+ removeState(state) : void\l+ unsetStates(states) : void\l}"
	URL="\ref AppletWithState"
	style=filled
	];

    applet [
	fillcolor="#ffdddd"
	label="{PublicTransportApplet|Shows a departure / arrival list or a list of journeys.\l|+ dataUpdated( QString, Data ) : void\l# createTooltip() : void\l# createPopupIcon() : void\l# processData( Data ) : void\l# processDepartureList( Data ) : void\l# processJourneyList( Data ) : void\l# appendDeparture( DepartureInfo ) : void\l# appendJourney( JourneyInfo ) : void\l# updateDeparture( DepartureInfo ) : void\l# updateJourney( JourneyInfo ) : void\l}"
	URL="\ref PublicTransport"
	];

    htmlDelegate [
	label="{HtmlDelegate|Paints items of the departure board.\l}"
	URL="\ref HtmlDelegate"
	];

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

    subgraph clusterDataTypes {
	label="Data Types";
	style="rounded, filled";
	color="#ccffcc";
	node [ fillcolor="#dfffdf" ];
	rank="sink";


	departureInfo [ rank=min
	label="{DepartureInfo|Stores information about a departure / arrival.\l}"
	URL="\ref DepartureInfo"
	];

	journeyInfo [ rank=max
	label="{JourneyInfo|Stores information about a journey.\l}"
	URL="\ref JourneyInfo"
	];
    };

    edge [ dir=back, arrowhead="none", arrowtail="empty", style="solid" ];
    appletState -> applet;

    edge [ dir=forward, arrowhead="none", arrowtail="normal", style="dashed", fontcolor="gray", headlabel="", taillabel="0..*" ];
    dataSourceTester -> settings [ label="m_dataSourceTester" ];
    appletState -> settings [ label="m_applet" ];

    edge [ dir=back, arrowhead="normal", arrowtail="none", style="dashed", fontcolor="gray", taillabel="", headlabel="0..*" ];
    applet -> settings [ label="m_settings" ];
    applet -> htmlDelegate [ label="uses", minlen=3 ];
    applet -> departureInfo [ label="uses", minlen=2 ];
    applet -> journeyInfo [ label="uses", minlen=2 ];
}
@enddot
*/

#endif
