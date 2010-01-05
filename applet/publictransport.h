/*
 *   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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
#include <Plasma/Label>
#include <Plasma/LineEdit>
#include <Plasma/TreeView>
#include <Plasma/IconWidget>

// KDE includes
#include <KIcon>
#include <KDateTimeWidget>

// Qt includes
#include <QStandardItemModel>
#include <QGraphicsProxyWidget>

// Own includes
#include "global.h"
#include "departureinfo.h"
#include "alarmtimer.h"
#include "settings.h"
#include "appletwithstate.h"

class QSizeF;
class QGraphicsLayout;


/** @class PublicTransport
* @brief Shows departure / arrival times for public transport. It uses the "publictransport"-data engine. */
class PublicTransport : public AppletWithState { //Plasma::PopupApplet {
    Q_OBJECT

    public:
        /** Basic create. */
	PublicTransport(QObject *parent, const QVariantList &args);
	/** Basic destroy. */
        ~PublicTransport();

	/** Initializes the applet. */
	void init();

	// Deprecated..
        void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect& contentsRect);

	/** Returns the widget with the contents of the applet. */
	virtual QGraphicsWidget* graphicsWidget();

	/** Sets values of the current plasma theme. */
	void useCurrentPlasmaTheme();

	/** The constraints have changed. */
	virtual void constraintsEvent ( Plasma::Constraints constraints );

    protected:
	/** Create the configuration dialog contents.
	* @param parent The config dialog in which the config interface should be created. */
	void createConfigurationInterface(KConfigDialog *parent);

	/** The popup pops up. */
	virtual void popupEvent ( bool show );

	/** Creates all used QAction's. */
	void setupActions();

	/** Gets an action with string and icon updated to the current settings.
	* @param actionName The name of the action to return updated.
	* @return The updated action.*/
	QAction* updatedAction( const QString &actionName );

	/** Updates the departure / arrival data model with the departure / arrival
	* list received from the data engine. */
	void updateModel();

	/** Updates the journey data model with the journey list received from the
	* data engine. */
	void updateModelJourneys();

	/** Creates the used models. */
	void createModels();

	/** Generates tooltip data and registers this applet at plasma's TooltipManager. */
	void createTooltip();

	/** Creates the popup icon with information about the next departure / alarm. */
	void createPopupIcon();

	/** Gets the first departure / arrival that doesn't get filtered out. */
	DepartureInfo getFirstNotFilteredDeparture();

	/** Gets the next active alarm or NULL if there is no active alarm.
	* @return NULL if there is no active alarm. */
	AlarmTimer *getNextAlarm();
	/** Sets the icon that should be painted as main icon.
	* @param mainIconDisplay The icon to be set. */
	void setMainIconDisplay( MainIconDisplay mainIconDisplay );

	/** Disconnects a currently connected departure / arrival data source and
	* connects again using the current configuration. */
	void reconnectSource();

	/** Disconnects a currently connected journey data source. */
	void disconnectJourneySource();

	/** Disconnects a currently connected journey data source and connects again
	* using the current configuration. */
	void reconnectJourneySource( const QString &targetStopName = QString(),
				     const QDateTime &dateTime = QDateTime::currentDateTime() );

	/** Processes data received from the data engine.
	* @param data The data object from the data engine. */
	void processData( const Plasma::DataEngine::Data& data );

	/** Processes departure / arrival list data received from the data engine.
	* @param data The data object from the data engine. */
	void processDepartureList( const Plasma::DataEngine::Data& data );

	/** Processes journey list data received from the data engine.
	* @param data The data object from the data engine. */
	void processJourneyList( const Plasma::DataEngine::Data& data );

	/** Clears the departure / arrival list received from the data engine and
	* displayed by the applet. */
	void clearDepartures();

	/** Clears the journey list received from the data engine and displayed by
	* the applet. */
	void clearJourneys();

	/** Wheather or not the given departure info should be filtered.
	* @param departureInfo The departure / arrival that should be tested.  */
	bool filterOut( const DepartureInfo &departureInfo ) const;

	/** Gets the text to be displayed above the treeview as title (html-tags allowed). */
	QString titleText() const;

	/** Gets the text to be displayed on right of the treeview as additional
	* information (html-tags allowed). */
	QString infoText() const;

	/** Gets the text to be displayed in the column departure / arrival for a given
	* DepartureInfo object.
	* @param departureInfo The departure / arrival for which the departure text
	* should be returned. */
	QString departureText( const DepartureInfo &departureInfo ) const;

	/** Gets the text to be displayed in the item for delay information.
	* @param departureInfo The departure / arrival for which the delay text
	* should be returned. */
	QString delayText( const DepartureInfo& departureInfo ) const;

	/** Gets the text to be displayed in the column departure for a journey.
	* @param departureInfo The journey for which the departure text should be
	* returned. */
	QString departureText( const JourneyInfo &journeyInfo ) const;

	/** Gets the text to be displayed in the column arrival for a journey.
	* @param departureInfo The journey for which the arrival text should be
	* returned. */
	QString arrivalText( const JourneyInfo &journeyInfo ) const;

	/** Gets the data of the current service provider. */
	QHash<QString, QVariant> serviceProviderData() const;

	/** Gets the current stop id if available. Otherwise it returns the current
	* stop name. */
	QString stop() const;

	/** Gets a list of actions for the context menu. */
	virtual QList<QAction*> contextualActions();

	/** Returns the index of the given departure / arrival info.
	* @return -1 if the given departure / arrival info couldn't be found. */
	int findDeparture( const DepartureInfo &departureInfo ) const;

	/** Returns the index of the given journey info.
	* @return -1 if the given journey info couldn't be found. */
	int findJourney( const JourneyInfo &journeyInfo ) const;

	/** Appends a new departure / arrival to the model.
	* @param departureInfo The departure / arrival to be added. */
	void appendDeparture( const DepartureInfo &departureInfo );

	/** Appends a new journey to the model. */
	void appendJourney( const JourneyInfo &journeyInfo );

	/** Updates a departure / arrival in the model.
	* @param departureInfo The departure / arrival to be updated. */
	void updateDeparture( int row, const DepartureInfo &departureInfo);

	/** Updates a journey in the model.
	* @param journeyInfo The journey to be added. */
	void updateJourney( int row, const JourneyInfo &journeyInfo);

	/** Removes old departures / arrivals from the model. */
	void removeOldDepartures();

	/** Removes old journeys from the journey model. */
	void removeOldJourneys();

	/** Sets the values of a QStandardItem in the tree view (text, icon, data, ...). */
	void setValuesOfDepartureItem( QStandardItem *departureItem, DepartureInfo departureInfo, ItemInformation departureInformation, bool update = false );

	/** Sets the values of a QStandardItem in the tree view (text, icon, data, ...)
	* to be displayed as journey item. */
	void setValuesOfJourneyItem( QStandardItem *departureItem, JourneyInfo journeyInfo, ItemInformation journeyInformation, bool update = false );

	/** Sets an alarm for the given departure / arrival. */
	void setAlarmForDeparture( const QPersistentModelIndex &modelIndex, AlarmTimer *alarmTimer = NULL );

	/** Marks a row in the tree view as having an alarm or not. */
	void markAlarmRow( const QPersistentModelIndex &index, AlarmState alarmState );

	/** Helper function to set the text color of an html item with a surrounding span-tag. */
	void setTextColorOfHtmlItem( QStandardItem *item, QColor textColor );

	/** Sets the type of the departure / arrival list. Can be a list of departures or a list of arrivals.
	* @param departureArrivalListType The departure / arrival list - type to be set. */
	void setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType );

	/** Sets the type of the journey list. Can be a list of journeys to or from the home stop.
	* @param departureArrivalListType The journey list - type to be set. */
	void setJourneyListType( JourneyListType journeyListType );

	/** Sets the type of title to be displayed. */
	void setTitleType( TitleType titleType );

	/** Creates a layout for the given title type. */
	QGraphicsLayout *createLayoutTitle( TitleType titleType );

	/** Tests the given state.
	* @param state The state to test.
	* @returns true, if the state is set.
	* @returns false, if the state is not set.*/
	virtual bool testState( AppletState state ) const;

	/** Adds the given state. Operations are processed to set the new applet state.
	* @param state The state to add. */
	virtual void addState( AppletState state );

	/** Removes the given state. Operations are processed to unset the new applet state.
	* @param state The state to remove. */
	virtual void removeState( AppletState state );

	/** Unsets the given states. No other operations are processed.
	* @param states A list of states to unset. */
	virtual void unsetStates( QList<AppletState> states );

	/** Gets the name of a column, to be displayed in the column's header. */
	QString nameForTimetableColumn( TimetableColumn timetableColumn, DepartureArrivalListType departureArrivalListType = _UseCurrentDepartureArrivalListType );

    private:
	AppletStates m_appletStates; /**< The current states of this applet */
	TitleType m_titleType; /**< The type of items to be shown as title above the tree view */

	QGraphicsWidget *m_graphicsWidget;
	Plasma::IconWidget *m_icon; /**< The icon that displayed in the top left corner */
	Plasma::IconWidget *m_iconClose; /**< The icon that displayed in the top right corner to close the journey view */
	Plasma::Label *m_label; /**< A label used to display a title */
	Plasma::Label *m_labelInfo; /**< A label used to display additional information */
	Plasma::TreeView *m_treeView; /**< A treeview displaying the departure board */
	Plasma::LineEdit *m_journeySearch; /**< A line edit for inputting the target of a journey */
	Plasma::TreeView *m_listPossibleStops; /**< A list of possible stops for the current input */
	QGraphicsProxyWidget *m_dateTimeProxy; /**< Graphics proxy widget for the date time widget for the journey search */
	KDateTimeWidget *m_dateTimeWidget; /**< Date time widget for the journey search */
	QStandardItemModel *m_model; /**< The model for the tree view containing the departure / arrival board */
	QStandardItemModel *m_modelJourneys; /**< The model for journeys from or to the "home stop" */
	QList<DepartureInfo> m_departureInfos; /**< List of current departures / arrivals */
	QList<JourneyInfo> m_journeyInfos; /**< List of current journeys */
	QString m_currentSource; /**< Current source name at the publictransport data engine */
	QString m_currentJourneySource; /**< Current source name for journeys at the publictransport data engine */
	QString m_lastSecondStopName; /**< The last used second stop name for journey search */
	QDateTime m_lastJourneyDateTime; /**< The last used date and time for journey search */
	QDateTime m_lastSourceUpdate; /**< The last update of the data source inside the data engine */
	QColor m_colorSubItemLabels; /**< The color to be used for sub item labels ("Delay:", "Platform:", ...) */

	PublicTransportSettings m_settings;
	bool m_stopNameValid; /**< Wheather or not the current stop name (m_stop) is valid */

	QPersistentModelIndex m_clickedItemIndex; /**< Index of the clicked item in the tree view for the context menu actions */
	QList< AlarmTimer* > m_abandonedAlarmTimer; /**< List of AlarmTimer's which departure row has disappeared from the list of received departures. It's kept to set the alarm again, when the departure appears again. */

	QList<TimetableColumn> m_departureViewColumns;
	QList<TimetableColumn> m_journeyViewColumns;

    signals:
	/** Emitted when the settings have changed. */
	void settingsChanged();

    public slots:
	/** The geometry of the applet has changed. */
	void geometryChanged();

	/** Settings have changed. */
	void configChanged();
	/** Settings that require a new data request have been changed. */
	void serviceProviderSettingsChanged();
	/** New data arrived from the data engine. */
	void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

	/** Shows an alarm message for given modelIndex. */
	void showAlarmMessage( const QPersistentModelIndex &modelIndex );
	/** A column (section) of the tree view was resized. */
	void treeViewSectionResized( int logicalIndex, int oldSize, int newSize );
	/** The popup dialog of the applet was resized. */
	void dialogSizeChanged();
	/** The context menu has been requested by the tree view. */
	void showDepartureContextMenu( const QPoint &position );
	/** The context menu has been requested by the tree view header. */
	void showHeaderContextMenu( const QPoint &position );
	/** An item in the tree view has been double clicked. */
	void doubleClickedDepartureItem( QModelIndex modelIndex );

	/** The icon widget was clicked. */
	void iconClicked();
	/** The icon widget to close the journey view was clicked. */
	void iconCloseClicked();
	void journeySearchInputFinished();
	void journeySearchInputEdited( const QString &newText );
	void possibleStopClicked( const QModelIndex &modelIndex );
	void possibleStopDoubleClicked( const QModelIndex &modelIndex );

	void emitConfigNeedsSaving();
	void configurationIsRequired( bool needsConfiguring, const QString &reason );
	void emitSettingsChanged();
	/** Called from PublicTransportSettings to indicate the need to update the
	* model by calling updateModel(). */
	void modelNeedsUpdate();
	/** Called from PublicTransportSettings to indicate the need to clear the
	* departure list. */
	void departureListNeedsClearing();
	void departureArrivalListTypeChanged ( DepartureArrivalListType departureArrivalListType );
	void journeyListTypeChanged ( JourneyListType journeyListType );

	/** The action to update the data source has been triggered. */
	void updateDataSource( bool );
	/** The action to set an alarm for the selected departure/arrival has been triggered. */
	void setAlarmForDeparture( bool );
	/** The action to remove an alarm from the selected departure/arrival has been triggered. */
	void removeAlarmForDeparture( bool );

	/** The action to switch to the journey search mode has been triggered. */
	void showJourneySearch( bool );
	
	/** The action to show all departures/arrivals has been triggered (remove all filters). */
	void showEverything( bool );
	
	/** The action to add the target of the selected departure/arrival to the filter list has been triggered. */
	void addTargetToFilterList( bool );
	/** The action to remove the target of the selected departure/arrival from the filter list has been triggered. */
	void removeTargetFromFilterList( bool );
	/** The action to add the target of the selected departure/arrival from the filter list and set the filter type to HideMatching has been triggered. */
	void addTargetToFilterListAndHide( bool );
	/** The action to set the filter type for targets to ShowAll has been triggered. */
	void setTargetFilterToShowAll( bool );
	/** The action to set the filter type for targets to HideMatching has been triggered. */
	void setTargetFilterToHideMatching( bool );
	
	/** The action to add the line number of the selected departure/arrival to the filter list has been triggered. */
	void addLineNumberToFilterList( bool );
	/** The action to remove the line number of the selected departure/arrival from the filter list has been triggered. */
	void removeLineNumberFromFilterList( bool );
	/** The action to add the line number of the selected departure/arrival from the filter list and set the filter type to HideMatching has been triggered. */
	void addLineNumberToFilterListAndHide( bool );
	/** The action to set the filter type for line numbers to ShowAll has been triggered. */
	void setLineNumberFilterToShowAll( bool );
	/** The action to set the filter type for line numbers to HideMatching has been triggered. */
	void setLineNumberFilterToHideMatching( bool );
	
	/** The action to add the vehicle type of the selected departure/arrival to the list of filtered vehicle types has been triggered. */
	void filterOutByVehicleType( bool );
	/** The action to clear the list of filtered vehicle types has been triggered. */
	void removeAllFiltersByVehicleType( bool );
	
	/** The action to expand / collapse of the selected departure/arrival has been triggered. */
	void toggleExpanded( bool );
	/** The action to hide the header of the tree view has been triggered. */
	void hideHeader( bool );
	/** The action to show the header of the tree view has been triggered. */
	void showHeader( bool );
	/** The action to hide the direction column of the tree view header has been triggered. */
	void hideColumnTarget( bool );
	/** The action to show the direction column of the tree view header has been triggered. */
	void showColumnTarget( bool );
	/** The plasma theme has been changed. */
	void themeChanged();
};

// This is the command that links the applet to the .desktop file
K_EXPORT_PLASMA_APPLET(publictransport, PublicTransport)


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

    alarmTimer [
	label="{AlarmTimer|Stores an alarm timer and a QPersistantModelIndex\lto the item the alarm belongs.\l}"
	URL="\ref AlarmTimer"
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
    applet -> alarmTimer [ label="uses", minlen=3 ];
    applet -> htmlDelegate [ label="uses", minlen=3 ];
    applet -> departureInfo [ label="uses", minlen=2 ];
    applet -> journeyInfo [ label="uses", minlen=2 ];
}
@enddot
*/

#endif
