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

/** @mainpage Public Transport Applet
@section intro_sec Introduction

@section install_sec Installation
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
* Shows departure / arrival times for public transport. It uses the "publictransport"-data engine. */
class PublicTransport : public AppletWithState { //Plasma::PopupApplet {
    Q_OBJECT

    public:
        // Basic Create/Destroy
        PublicTransport(QObject *parent, const QVariantList &args);
        ~PublicTransport();

	/** Initializes the applet. */
	void init();

	// Deprecated..
        void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect& contentsRect);

	/** Returns the widget with the contents of the applet. */
	virtual QGraphicsWidget* graphicsWidget();
	/** The constraints have changed. */
	virtual void constraintsEvent ( Plasma::Constraints constraints );

    protected:
	/** Create the configuration dialog contents. */
	void createConfigurationInterface(KConfigDialog *parent);
	/** The popup pops up. */
	virtual void popupEvent ( bool show );

    private:
	/** Creates all used QAction's. */
	void setupActions();
	/** Gets an action with string / icon updated to the current settings. */
	QAction* updatedAction( const QString &actionName );

	/** Updates the departure / arrival data model with the departure / arrival
	* list received from the data engine. */
	void updateModel();
	/** Updates the journey data model with the journey list received from the
	* data engine. */
	void updateModelJourneys();
	/** Creates the used models. */
	void createModel();

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

	/** Disconnects a currently connected data source and connects again using
	* the current configuration. */
	void reconnectSource();
	void reconnectJourneySource( const QString &targetStopName = QString() );

	/** Processes data received from the data engine. */
	void processData( const Plasma::DataEngine::Data& data );

	/** Processes departure / arrival list data received from the data engine. */
	void processDepartureList( const Plasma::DataEngine::Data& data );

	/** Processes journey list data received from the data engine. */
	void processJourneyList( const Plasma::DataEngine::Data& data );

	/** Clears the departure / arrival list received from the data engine and
	* displayed by the applet. */
	void clearDepartures();

	/** Clears the journey list received from the data engine and displayed by
	* the applet. */
	void clearJourneys();

	/** Wheather or not the given departure info should be filtered. */
	bool filterOut( const DepartureInfo &departureInfo ) const;

	/** Gets the text to be displayed above the treeview as title (html-tags allowed). */
	QString titleText() const;

	/** Gets the text to be displayed on right of the treeview as additional
	* information (html-tags allowed). */
	QString infoText() const;

	/** Gets the text to be displayed in the column departure / arrival for a
	* departure / arrival. */
	QString departureText( const DepartureInfo &departureInfo ) const;

	/** Gets the text to be displayed in the column arrival for a journey. */
	QString arrivalText( const JourneyInfo &journeyInfo ) const;

	/** Gets the text to be displayed in the column departure for a journey. */
	QString departureText( const JourneyInfo &journeyInfo ) const;

	/** Gets the text to be displayed in the item for delay information. */
	QString delayText( const DepartureInfo& departureInfo ) const;

	/** Gets the data of the current service provider. */
	QMap<QString, QVariant> serviceProviderData() const;

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

	/** Updates a departure / arrival in the model. */
	void updateDeparture( int row, const DepartureInfo &departureInfo);

	/** Updates a journey in the model. */
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

	void setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType );
	void setJourneyListType( JourneyListType journeyListType );
	void setTitleType( TitleType titleType );
	QGraphicsLayout *createLayoutTitle( TitleType titleType );

	virtual bool testState( AppletState state ) const;
	virtual void addState( AppletState state );
	virtual void removeState( AppletState state );
	virtual void unsetStates( QList<AppletState> states );

	/** Gets the name of a column, to be displayed in the column's header. */
	QString nameForTimetableColumn( TimetableColumn timetableColumn, DepartureArrivalListType departureArrivalListType = _UseCurrentDepartureArrivalListType );


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
	QStandardItemModel *m_model; /**< The model for the tree view containing the departure / arrival board */
	QStandardItemModel *m_modelJourneys; /**< The model for journeys from or to the "home stop" */
	QList<DepartureInfo> m_departureInfos; /**< List of current departures / arrivals */
	QList<JourneyInfo> m_journeyInfos; /**< List of current journeys */
	QString m_currentSource; /**< Current source name at the publictransport data engine */
	QString m_currentJourneySource; /**< Current source name for journeys at the publictransport data engine */
	QString m_lastSecondStopName; /**< The last used second stop name for journey search */
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
	// A column (section) of the tree view was resized. */
	void treeViewSectionResized( int logicalIndex, int oldSize, int newSize );
	// The popup dialog of the applet was resized. */
	void dialogSizeChanged();
	// The context menu has been requested by the tree view
	void showDepartureContextMenu( const QPoint &position );
	// The context menu has been requested by the tree view header
	void showHeaderContextMenu( const QPoint &position );
	// An item in the tree view has been double clicked
	void doubleClickedDepartureItem( QModelIndex modelIndex );

	// The icon widget was clicked
	void iconClicked();
	// The icon widget to close the journey view was clicked
	void iconCloseClicked();
	void journeySearchInputFinished();
// 	void focusOutTitleJourneySearch( QFocusEvent *event );
	void journeySearchInputEdited( const QString &newText );
	void possibleStopClicked( const QModelIndex &modelIndex );
	void possibleStopDoubleClicked( const QModelIndex &modelIndex );

	void emitConfigNeedsSaving();
	void configurationIsRequired( bool needsConfiguring, const QString &reason );
	void emitSettingsChanged();
	// Called from PublicTransportSettings to indicate the need to update the model by calling updateModel()
	void modelNeedsUpdate();
	// Called from PublicTransportSettings to indicate the need to clear the departure list
	void departureListNeedsClearing();
	void departureArrivalListTypeChanged ( DepartureArrivalListType departureArrivalListType );
	void journeyListTypeChanged ( JourneyListType journeyListType );

	// The action to update the data source has been triggered
	void updateDataSource( bool );
	// The action to set an alarm for the selected departure/arrival has been triggered
	void setAlarmForDeparture( bool );
	// The action to remove an alarm from the selected departure/arrival has been triggered
	void removeAlarmForDeparture( bool );
	// The action to add the target of the selected departure/arrival to the filter list has been triggered
	void addTargetToFilterList( bool );
	// The action to remove the target of the selected departure/arrival from the filter list has been triggered
	void removeTargetFromFilterList( bool );
	// The action to remove the target of the selected departure/arrival from the filter list and set the filter type to HideMatching has been triggered
	void addTargetToFilterListAndHide( bool );
	// The action to set the filter type to ShowAll has been triggered
	void setFilterListToShowAll ( bool );
	// The action to set the filter type to HideMatching has been triggered
	void setFilterListToHideMatching ( bool );
	// The action to add the vehicle type of the selected departure/arrival to the list of filtered vehicle types has been triggered
	void filterOutByVehicleType( bool );
	// The action to clear the list of filtered vehicle types has been triggered
	void removeAllFiltersByVehicleType( bool );
	// The action to expand / collapse of the selected departure/arrival has been triggered
	void toggleExpanded( bool );
	// The action to hide the header of the tree view has been triggered
	void hideHeader( bool );
	// The action to show the header of the tree view has been triggered
	void showHeader( bool );
	// The action to hide the direction column of the tree view header has been triggered
	void hideColumnTarget( bool );
	// The action to show the direction column of the tree view header has been triggered
	void showColumnTarget( bool );
};

// This is the command that links the applet to the .desktop file
K_EXPORT_PLASMA_APPLET(publictransport, PublicTransport)



// class WidgetWithSignals : public QGraphicsProxyWidget {
    //     Q_OBJECT
    //
    //     public:
    // 	WidgetWithSignals( QGraphicsItem *parent = 0, Qt::WindowFlags f = 0 )
    // 	: QGraphicsProxyWidget( parent, f ) {
   // 	};
   //
   //     protected:
   // 	virtual void focusInEvent( QFocusEvent *event ) {
   // 	    emit focusIn( event );
   // 	};
   //
   // 	virtual void focusOutEvent ( QFocusEvent *event ) {
   // 	    emit focusOut( event );
   // 	};
   //
   // 	virtual void childEvent( QChildEvent *event ) {
   // 	    if ( event->added() && event->child()->isWidgetType() ) {
   // 		QGraphicsProxyWidget *widget = (QGraphicsProxyWidget*)event->child();
   // // 		widget->ung
   // 	    }
   // 	};
   //
   //     signals:
   // 	void focusIn( QFocusEvent *event );
   // 	void focusOut( QFocusEvent *event );
   // };
   //
   // class FocusEventSignaler : public QObject
   // {
       //     Q_OBJECT
       //
       //     protected:
       // 	bool eventFilter(QObject *obj, QEvent *event) {
   // 	    if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
   // 		QFocusEvent *focusEvent = static_cast<QFocusEvent *>(event);
   // 		if ( focusEvent->gotFocus() )
   // 		    emit focusIn( focusEvent );
   // 		else if ( focusEvent->lostFocus() )
   // 		    emit focusOut( focusEvent );
   // 		return focusEvent->isAccepted();
   // 	    } else {
       // 		// standard event processing
       // 		return QObject::eventFilter(obj, event);
       // 	    }
       // 	};
       //
       //     signals:
       // 	void focusIn( QFocusEvent *event );
       // 	void focusOut( QFocusEvent *event );
       // };




#endif
