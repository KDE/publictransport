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

typedef Plasma::MessageButton MessageButton;

/** @class PublicTransport
* @brief Shows departure / arrival times for public transport. It uses the "publictransport"-data engine. */
class PublicTransport : public Plasma::PopupApplet {
    Q_OBJECT
    public:
        /** Basic create. */
	PublicTransport( QObject *parent, const QVariantList &args );
	/** Destructor. Saves the list of recently used journey searches. */
        ~PublicTransport();

	/** Maximum number of recent journey searches. When more journey searches
	* get added, the oldest when gets removed. */
	static const int MAX_RECENT_JOURNEY_SEARCHES = 10;

	/** Returns the widget with the contents of the applet. */
	virtual QGraphicsWidget* graphicsWidget();
	
	/** Tests the given state.
	* @param state The state to test.
	* @returns True, if the state is set. False, otherwise.*/
	virtual bool testState( AppletState state ) const {
	    return m_appletStates.testFlag( state ); };
	/** Adds the given state. Operations are processed to set the new applet state.
	* @param state The state to add. */
	virtual void addState( AppletState state );
	/** Removes the given state. Operations are processed to unset the new applet state.
	* @param state The state to remove. */
	virtual void removeState( AppletState state );
	
    protected:
	/** Create the configuration dialog contents.
	* @param parent The config dialog in which the config interface should be created. */
	void createConfigurationInterface( KConfigDialog *parent );
	
	/** Gets a list of actions for the context menu. */
	virtual QList<QAction*> contextualActions();
	/** The popup gets shown or hidden. */
	virtual void popupEvent( bool show );
	
	virtual bool sceneEventFilter( QGraphicsItem* watched, QEvent* event );
	/** Watching for up/down key presses in m_journeySearch to select stop suggestions. */
	virtual bool eventFilter( QObject* watched, QEvent* event );
	
	/** Creates all used QAction's. */
	void setupActions();
	/** Gets an action with string and icon updated to the current settings.
	* @param actionName The name of the action to return updated.
	* @return The updated action.*/
	QAction* updatedAction( const QString &actionName );

	/** Creates the used models. */
	void createModels();
	/** Generates tooltip data and registers this applet at plasma's TooltipManager. */
	void createTooltip();
	/** Creates the popup icon with information about the next departure / alarm. */
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
	QString courtesyToolTip() const;

	/** Disconnects a currently connected departure / arrival data source and
	* connects a new source using the current configuration. */
	void reconnectSource();
	void disconnectSources();

	/** Disconnects a currently connected journey data source and connects
	* a new source using the current configuration. */
	void reconnectJourneySource( const QString &targetStopName = QString(),
				     const QDateTime &dateTime = QDateTime::currentDateTime(),
				     bool stopIsTarget = true, bool timeIsDeparture = true,
				     bool requestStopSuggestions = false );
	/** Disconnects a currently connected journey data source. */
	void disconnectJourneySource();

	void handleDataError( const QString &sourceName, const Plasma::DataEngine::Data& data );
	void processStopSuggestions( const QString &sourceName, const Plasma::DataEngine::Data& data );
	/** Clears the departure / arrival list received from the data engine and
	* displayed by the applet. */
	void clearDepartures();
	/** Clears the journey list received from the data engine and displayed by
	* the applet. */
	void clearJourneys();

	/** Appends a new departure / arrival to the model.
	* @param departureInfo The departure / arrival to be added. */
	void appendDeparture( const DepartureInfo &departureInfo );

	/** Appends a new journey to the model. */
	void appendJourney( const JourneyInfo &journeyInfo );

	/** Sets an alarm for the given departure / arrival. */
	void createAlarmSettingsForDeparture( const QPersistentModelIndex &modelIndex );
	void removeAlarmForDeparture( int row );
	
	void stretchAllChildren( const QModelIndex &parent,
					  QAbstractItemModel *model );

	/** Helper function to set the text color of an html item with a surrounding span-tag. */
	void setTextColorOfHtmlItem( QStandardItem *item, const QColor &textColor );

	/** Sets the type of title to be displayed. */
	void setTitleType( TitleType titleType );
	/** Creates a layout for the given title type. */
	QGraphicsLayout *createLayoutTitle( TitleType titleType );

	/** Unsets the given states. No other operations are processed.
	* @param states A list of states to unset. */
	virtual void unsetStates( QList<AppletState> states );

	/** Gets the name of a column, to be displayed in the column's header. */
	QString nameForTimetableColumn( TimetableColumn timetableColumn,
					DepartureArrivalListType departureArrivalListType
					= _UseCurrentDepartureArrivalListType );

    signals:
	/** Emitted when the settings have changed. */
	void settingsChanged();

    public slots:
	/** Initializes the applet. */
	virtual void init();

    protected slots:
	/** The geometry of the applet has changed. */
	void geometryChanged();

	/** Settings have changed. */
	void configChanged();
	/** Settings that require a new data request have been changed. */
	void serviceProviderSettingsChanged();
	/** New data arrived from the data engine. */
	void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );
	
	/** Fills the departure / arrival data model with the given 
	* departure / arrival list without removing departures that were already
	* in the model. This will stop if the maximum departure count is reached
	* or if all @p departures have been added. */
	void fillModel( const QList<DepartureInfo> &departures );
	void fillModelJourney( const QList<JourneyInfo> &journeys );

	/** The context menu has been requested by the tree view. */
	void showDepartureContextMenu( const QPoint &position );
	/** The context menu has been requested by the tree view header. */
	void showHeaderContextMenu( const QPoint &position );
	/** An item in the tree view has been double clicked. */
	void doubleClickedDepartureItem( const QModelIndex &modelIndex );

	/** The icon widget was clicked. */
	void iconClicked();
	/** The icon widget to close the journey view was clicked. */
	void iconCloseClicked();

	void infoLabelLinkActivated( const QString &link );
	
	void journeySearchInputFinished();
	void journeySearchInputEdited( const QString &newText );
	void possibleStopClicked( const QModelIndex &modelIndex );
	void possibleStopDoubleClicked( const QModelIndex &modelIndex );

	/** Called from PublicTransportSettings to indicate the need to clear the
	* departure list. */
	void setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType );

	/** The action to update the data source has been triggered. */
	void updateDataSource();
	/** The action to set an alarm for the selected departure/arrival has been triggered. */
	void setAlarmForDeparture();
	/** The action to remove an alarm from the selected departure/arrival has been triggered. */
	void removeAlarmForDeparture();

	/** The action to switch to the journey search mode has been triggered. */
	void showJourneySearch();
	void goBackToDepartures();
	void showActionButtons();
	
	/** The action to expand / collapse of the selected departure/arrival has been triggered. */
	void toggleExpanded();
	/** The action to hide the header of the tree view has been triggered. */
	void hideHeader();
	/** The action to show the header of the tree view has been triggered. */
	void showHeader();
	/** The action to hide the direction column of the tree view header has been triggered. */
	void hideColumnTarget();
	/** The action to show the direction column of the tree view header has been triggered. */
	void showColumnTarget();
	/** The plasma theme has been changed. */
	void themeChanged() { useCurrentPlasmaTheme(); };
	
	void recentJourneyActionTriggered( QAction *action );
	
	void sectionPressed( int logicalIndex );
	void sectionMoved( int logicalIndex, int oldVisualIndex, int newVisualIndex );
	
	void beginDepartureProcessing( const QString &sourceName );
	void departuresProcessed( const QString &sourceName,
				  const QList< DepartureInfo > &departures,
				  const QUrl &requestUrl,
				  const QDateTime &lastUpdate );
	void departuresFiltered( const QString &sourceName,
				 const QList< DepartureInfo > &departures,
				 const QList< DepartureInfo > &newlyFiltered,
				 const QList< DepartureInfo > &newlyNotFiltered );
				 
	void beginJourneyProcessing( const QString &sourceName );
	void journeysProcessed( const QString &sourceName,
				const QList< JourneyInfo > &journeys,
				const QUrl &requestUrl,
				const QDateTime &lastUpdate );

    protected slots:
	void destroyOverlay();
	void setCurrentStopIndex( QAction *action );
	
	void writeSettings( const Settings &settings );

	void setShowDepartures();
	void setShowArrivals();
	
	void switchFilterConfiguration( const QString &newFilterConfiguration );
	void switchFilterConfiguration( QAction *action );
	void setFiltersEnabled( bool enable );
	
	void slotMessageButtonPressed( MessageButton button );
	
	void alarmFired( DepartureItem *item );
	void removeAlarms( const AlarmSettingsList &newAlarmSettings,
			   const QList<int> &removedAlarms );
	
    private:
	enum MessageType {
	    MessageNone = 0,
	    MessageError,
	    MessageErrorResolved
	};
	enum NetworkStatus {
	    StatusUnknown = 0,
	    StatusUnavailable,
	    StatusConfiguring,
	    StatusActivated
	};

	NetworkStatus queryNetworkStatus();
	/** Shows an error message when no interface is activated.
	* @return True, if no message is shown. False, otherwise. */
	bool checkNetworkStatus();

	/** Checks if a departure with the given @p dateTime will be shown,
	* depending on the settings (first departure, maybe at a custom time). */
	bool isTimeShown( const QDateTime &dateTime ) const;
	
	/** List of current departures / arrivals for the selected stop(s). */
	QList<DepartureInfo> departureInfos() const;
	QString stripDateAndTimeValues( const QString &sourceName ) const;
	
	/** Sets values of the current plasma theme. */
	void useCurrentPlasmaTheme();
	
	KSelectAction *switchStopAction( QObject *parent,
					 bool destroyOverlayOnTrigger = false ) const;
	QVariantHash currentServiceProviderData() const {
	    return serviceProviderData( m_settings.currentStopSettings().serviceProviderID ); };
	QVariantHash serviceProviderData( const QString &id ) const;

	void setHeightOfCourtesyLabel();

	void hideMessage() {
	    m_currentMessage = MessageNone;
	    showMessage( QIcon(), QString(), Plasma::ButtonNone );
	};
	
				 
	AppletStates m_appletStates; /**< The current states of this applet */
	TitleType m_titleType; /**< The type of items to be shown as title above the tree view */
	
	QGraphicsWidget *m_graphicsWidget, *m_mainGraphicsWidget;
	Plasma::IconWidget *m_icon; /**< The icon that displayed in the top left corner */
	Plasma::IconWidget *m_iconClose; /**< The icon that displayed in the top right corner to close the journey view */
	Plasma::Label *m_label; /**< A label used to display a title */
	Plasma::Label *m_labelInfo; /**< A label used to display additional information */
	Plasma::TreeView *m_treeView; /**< A treeview displaying the departure board */
	Plasma::Label *m_labelJourneysNotSupported; /**< A label used to display an info about unsupported journey search */
	Plasma::LineEdit *m_journeySearch; /**< A line edit for inputting the target of a journey */
	Plasma::TreeView *m_listStopsSuggestions; /**< A list of stop suggestions for the current input */
	Plasma::ToolButton *m_btnLastJourneySearches; /**< A tool button that shows last used journey searches. */
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

	Settings m_settings;
	QStringList m_currentServiceProviderFeatures;
	bool m_stopNameValid; /**< Wheather or not the current stop name (m_stop) is valid */
	
	QPersistentModelIndex m_clickedItemIndex; /**< Index of the clicked item in the tree view for the context menu actions */

	QActionGroup *m_filtersGroup;
	
	QList<TimetableColumn> m_departureViewColumns;
	QList<TimetableColumn> m_journeyViewColumns;

	MessageType m_currentMessage;
	DepartureProcessor *m_departureProcessor;
};

#ifndef NO_EXPORT_PLASMA_APPLET // Needed for settings.cpp to include publictransport.h
// This is the command that links the applet to the .desktop file
K_EXPORT_PLASMA_APPLET( publictransport, PublicTransport )
#endif


/** @mainpage Public Transport Applet
@section intro_applet_sec Introduction
This applet shows a departure / arrival board for public transport, trains, ferries and planes. It uses the public transport data engine.

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
\> plasmoidviewer<br>
<br>
You might need to run kbuildsycoca4 in order to get the .desktop file recognized.
*/

/** @page pageClassDiagram Class Diagram
\dot
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
