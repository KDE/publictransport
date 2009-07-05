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

#ifndef PUBLICTRANSPORT_HEADER
#define PUBLICTRANSPORT_HEADER

// Plasma includes
#include <Plasma/PopupApplet>
#include <Plasma/Label>
#include <Plasma/TreeView>

// Qt includes
#include <QStandardItemModel>

// Own includes
#include "enums.h"
#include "ui_publicTransportConfig.h"
#include "ui_publicTransportFilterConfig.h"
#include "ui_accessorInfo.h"
#include "departureinfo.h"
#include "datasourcetester.h"
#include "alarmtimer.h"

class QSizeF;

// Shows departure / arrival times for public transport. It uses the "publictransport"-data engine.
class PublicTransport : public Plasma::PopupApplet {
    Q_OBJECT

    public:
        // Basic Create/Destroy
        PublicTransport(QObject *parent, const QVariantList &args);
        ~PublicTransport();

	// Initializes the applet
	void init();

	// Deprecated..
        void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect& contentsRect);
	// Returns the widget with the contents of the applet
	virtual QGraphicsWidget* graphicsWidget();
	// Constraints changed
	virtual void constraintsEvent ( Plasma::Constraints constraints );

	// Creates an icon that has another icon as overlay on the bottom right
	static KIcon makeOverlayIcon( const KIcon &icon, const QString &overlayIconName, const QSize &overlaySize = QSize(10, 10) );
	static KIcon makeOverlayIcon( const KIcon &icon, const KIcon &overlayIcon, const QSize &overlaySize = QSize(10, 10) );
	// Gets an icon for the given type of vehicle
	static KIcon iconFromVehicleType( const VehicleType &vehicleType, const QString &overlayIcon = QString() );
	// Gets the name of the given type of vehicle
	static QString vehicleTypeToString( const VehicleType &vehicleType, bool plural = false );
	// Gets the config name used to store wheather or not the given type of vehicle should be shown
	static QString vehicleTypeToConfigName( const VehicleType &vehicleType );

    protected:
	// Create the configuration dialog contents
	void createConfigurationInterface(KConfigDialog *parent);
	// The popup pops up ;)
	virtual void popupEvent ( bool show );

    private:
	// Creates all used QAction's
	void setupActions();
	// Gets an action with string / icon updated to the current settings
	QAction* updatedAction( const QString &actionName );
	// Updates the departure data model with the journey list received from the data engine
	void updateModel();
	// Creates the model
	void createModel();
	// Generates tooltip data and registers this applet at plasma's TooltipManager
	void createTooltip();
	// Creates the popup icon with information about the next departure / alarm
	void createPopupIcon();
	// Sorts the departures
	DepartureInfo getFirstNotFilteredDeparture();
	// Gets the next active alarm or NULL if there is no active alarm
	AlarmTimer *getNextAlarm();

	// Disconnects a currently connected data source and connects again using the current configuration
	void reconnectSource();
	// Processes data received from the data engine
	void processData( const Plasma::DataEngine::Data& data );
	// Processes journey list data received from the data engine
	void processJourneyList( const Plasma::DataEngine::Data& data );
	// Clears the journey list received from the data engine and displayed by the applet
	void clearDepartures();
	// Wheather or not the given departure info should be filtered
	bool filterOut( const DepartureInfo &departureInfo ) const;
	// Gets a list of all filtered out vehicle types
	QList<VehicleType> filteredOutVehicleTypes() const;

	// Gets the text to be displayed above the treeview (html-tags allowed)
	QString titleText( const QString &sServiceProvider ) const;
	// Gets the text to be displayed in the column departure / arrival
	QString departureText( const DepartureInfo &departureInfo ) const;
	// Creates items for extended delay information
	void getDelayItems( const DepartureInfo &departureInfo, QStandardItem **itemDelayInfo, QStandardItem **itemDelay );
	// Gets the data of the current service provider
	QMap<QString, QVariant> serviceProviderData() const;
	// Gets the current stop id if available. Otherwise it returns the current stop name
	QString stop() const;

	// Helper method for creating QStandardItems
	QStandardItem *createStandardItem( const QString &text, Qt::Alignment textAlignment = Qt::AlignLeft, QVariant sortData = QVariant() );
	// Helper method for creating QStandardItems with a given text color
	QStandardItem *createStandardItem( const QString &text, QColor textColor, Qt::Alignment textAlignment = Qt::AlignLeft, QVariant sortData = QVariant() );

	// Gets a list of actions for the context menu
	virtual QList<QAction*> contextualActions();

	// Returns true, if all settings are ok, otherwise it returns false
	bool checkConfig();
	// Sets the status and tooltip of the KLed in the config dialog
	void setStopNameValid( bool valid, const QString &toolTip = "" );

	// Returns the index of the departure / arrival info to be searched if found. Otherwise it returns -1
	int findDeparture( const DepartureInfo &departureInfo ) const;
	// Appends a new departure / arrival to the model
	void appendDeparture( const DepartureInfo &departureInfo );
	// Updates a departure / arrival in the model
	void updateDeparture( int row, const DepartureInfo &departureInfo);
	// Removes old departures / arrivals from the model
	void removeOldDepartures();
	// Sets the values of a QStandardItem in the tree view (text, icon, data, ...)
	void setValuesOfDepartureItem( QStandardItem *departureItem, DepartureInfo departureInfo, ItemInformation departureInformation, bool update = false );
	// Sets an alarm for the given departure/arrival
	void setAlarmForDeparture( const QPersistentModelIndex &modelIndex, AlarmTimer *alarmTimer = NULL );
	// Marks a row in the tree view as having an alarm or not
	void markAlarmRow( const QPersistentModelIndex &index, AlarmState alarmState );
	// Helper function to set the text color of an html item with a surrounding span-tag
	void setTextColorOfHtmlItem( QStandardItem *item, QColor textColor );

        Ui::publicTransportConfig m_ui; // The "general" settings page
        Ui::publicTransportFilterConfig m_uiFilter; // The "filter" settings page
	Ui::accessorInfo m_uiAccessorInfo; // The accessor info dialog
	KConfigDialog *m_configDialog; // Stored for the accessor info dialog as parent
	bool m_isAccessorInfoDialogShown; // Wheather or not the accessor info dialog is currently shown
	bool m_isConfigDialogShown; // Wheather or not the config dialog is currently shown

        Plasma::Svg m_svg;
        KIcon m_icon;
	QGraphicsWidget *m_graphicsWidget;
	Plasma::Label *m_label; // A label used to display a title
	Plasma::TreeView *m_treeView; // A treeview displaying the departure board
	QStandardItemModel *m_model; // The model for the tree view containing the departure board
	QStandardItemModel *m_modelServiceProvider; // The model for the service provider combobox in the config dialog
	QList<DepartureInfo> m_departureInfos; // List of current departures
	QString m_currentSource; // Current source name at the publictransport data engine
	QDateTime m_lastSourceUpdate; // The last update of the data source inside the data engine
	QColor m_colorSubItemLabels; // The color to be used for sub item labels ("Delay:", "Platform:", ...)

	DataSourceTester *m_dataSourceTester; // Tests data sources
	int m_updateTimeout; // A timeout to reload the timetable information from the internet
	bool m_showRemainingMinutes; // Wheather or not remaining minutes until departure should be shown
	bool m_showDepartureTime; // Wheather or not departure times should be shown
	QString m_city; // The currently selected city
	QString m_stop; // The currently selected stop
	QString m_stopID; // The ID of the currently selected stop or an empty string if the ID isn't available
	QString m_stopIDinConfig; // The ID of the currently typed stop in the config dialog
	int m_linesPerRow; // How many lines each row in the tree view should have
	int m_serviceProvider; // The id of the current service provider
	int m_timeOffsetOfFirstDeparture; // The offset in minutes from now of the first departure
	int m_maximalNumberOfDepartures; // The maximal number of displayed departures
	int m_alarmTime; // The time in minutes before the departure at which the alarm should be fired
	DataType m_dataType; // Type type of data the data source provides
	QMap< VehicleType, bool > m_showTypeOfVehicle; // Wheather or not a type of vehicle should be shown
	bool	m_showNightlines; // Wheather or not night lines should be shown
	int m_filterMinLine; // The minimal line number to be shown
	int m_filterMaxLine; // The maximal line number to be shown
	FilterType m_filterTypeTarget; // The type of the filter (ShowAll, ShowMatching, HideMatching)
	QStringList m_filterTargetList; // A list of targets that should be filtered
	bool m_settingsJustChanged; // True when the settingsChanged signal was just emitted, false after dataUpdated() was called

	bool m_useSeperateCityValue; // Wheather or not the current service provider uses a seperate city value
	bool m_stopNameValid; // Wheather or not the current stop name (m_stop) is valid
	QPersistentModelIndex m_clickedItemIndex; // Index of the clicked item in the tree view for the context menu actions
	QList< AlarmTimer* > m_abandonedAlarmTimer; // List of AlarmTimer's which departure row has disappeared from the list of received departures. It's kept to set the alarm again, when the departure appears again.

    signals:
	// Emitted when the settings have changed
	void settingsChanged();

    public slots:
	// Ok pressed in the config dialog
	void configAccepted();
	// Settings have changed
	void configChanged();
	// The geometry of the applet has changed
	void geometryChanged();
	// New data arrived from the data engine
	void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );
	// The selected service provider in the config dialog's combobox has changed
	void serviceProviderChanged( int index );
	// The stop name in the corresponding input field in the config dialog has changed
	void stopNameChanged( QString stopName );

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
	// Shows an alarm message for given modelIndex
	void showAlarmMessage( const QPersistentModelIndex &modelIndex );
	// A column (section) of the tree view was resized
	void treeViewSectionResized( int logicalIndex, int oldSize, int newSize );
	// The popup dialog of the applet was resized
	void dialogSizeChanged();
	// The context menu has been requested by the tree view
	void showDepartureContextMenu( const QPoint &position );
	// An item in the tree view has been double clicked
	void doubleClickedDepartureItem( QModelIndex modelIndex );
	// The info button in the config dialog for showing all service provider information has been clicked
	void clickedServiceProviderInfo( bool );
	// The service provider information dialog has been closed
	void accessorInfoDialogFinished();
	// The config dialog has been closed
	void configDialogFinished();
	// The result of a data source test is ready
	void testResult( DataSourceTester::TestResult result, const QVariant &data );

	// A vehicle type was added to the shown vehicle types list widget
	void addedFilterLineType ( QListWidgetItem* );
	// A vehicle type was removed from the shown vehicle types list widget
	void removedFilterLineType ( QListWidgetItem* );
	// Another item was selected in the shown vehicle types list widget
	void filterLineTypeSelectedSelctionChanged( int );
	// Another item was selected in the hidden vehicle types list widget
	void filterLineTypeAvaibleSelctionChanged( int );
};

// This is the command that links the applet to the .desktop file
K_EXPORT_PLASMA_APPLET(publictransport, PublicTransport)
#endif
