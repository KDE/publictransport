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

#ifndef SETTINGS_HEADER
#define SETTINGS_HEADER

#include "ui_publicTransportConfig.h"
#include "ui_publicTransportConfigAdvanced.h"
#include "ui_publicTransportFilterConfig.h"
#include "ui_publicTransportAppearanceConfig.h"
#include "ui_accessorInfo.h"
#include "datasourcetester.h"
#include "global.h"
#include "appletwithstate.h"
#include <Plasma/Theme>

class QStandardItemModel;

class PublicTransportSettings : public QObject {
    Q_OBJECT

//     friend class PublicTransport;

    public:
	PublicTransportSettings( AppletWithState *applet );

	AppletWithState *applet() { return m_applet; };

	void readSettings();

	/** Checks the current configuration for problems.
	* @return true, if all settings are ok.
	* @return false, if there were problems found with the current settings. */
	bool checkConfig();

	/** Creates the configuration dialog contents. */
	void createConfigurationInterface(KConfigDialog *parent, bool stopNameValid);

	/** Sets the status and tooltip of the KLed in the config dialog. */
	void setStopNameValid( bool valid, const QString &toolTip = "" );

	QString configCityValue() const;
	void removeAllFiltersByVehicleType();

	/** Gets a list of all filtered out vehicle types. */
	QList<VehicleType> filteredOutVehicleTypes() const;

	/** A timeout to reload the timetable information from the internet. */
	int updateTimeout() const { return m_updateTimeout; };
	/** Wheather or not remaining minutes until departure should be shown. */
	bool isRemainingMinutesShown() const { return m_showRemainingMinutes; };
	/** Wheather or not departure times should be shown. */
	bool isDepartureTimeShown() const { return m_showDepartureTime; };
	/** The currently selected city. */
	QString city() const { return m_city; };
	/** The currently selected stop. */
	QString stop() const { return m_stop; };
	/** The ID of the currently selected stop or an empty string if the ID isn't available. */
	QString stopID() const { return m_stopID; };
	/** The ID of the currently typed stop in the config dialog. */
	QString stopIDinConfig() const { return m_stopIDinConfig; };
	/** How many lines each row in the tree view should have. */
	int linesPerRow() const { return m_linesPerRow; };
	/** The id of the current service provider. */
	QString serviceProvider() const { return m_serviceProvider; };
	/** The offset in minutes from now of the first departure. */
	int timeOffsetOfFirstDeparture() const { return m_timeOffsetOfFirstDeparture; };
	/** A custom time for the first departure. */
	QTime timeOfFirstDepartureCustom() const { return m_timeOfFirstDepartureCustom; };
	/** The config mode for the time of the first departure. */
	FirstDepartureConfigMode firstDepartureConfigMode() const { return m_firstDepartureConfigMode; };
	/** The maximal number of displayed departures. */
	int maximalNumberOfDepartures() const { return m_maximalNumberOfDepartures; };
	/** The time in minutes before the departure at which the alarm should be fired. */
	int alarmTime() const { return m_alarmTime; };
	/** Wheather or not a type of vehicle should be shown TODO: rename!. */
	bool isTypeOfVehicleShown(VehicleType vehicleType) const { return m_showTypeOfVehicle[vehicleType]; };
	/** Hide a type of vehicle. */
	void hideTypeOfVehicle(VehicleType vehicleType);
	/** Wheather or not night lines should be shown. */
	bool	showNightlines() const { return m_showNightlines; };
	/** The minimal line number to be shown. */
	int filterMinLine() const { return m_filterMinLine; };
	/** The maximal line number to be shown. */
	int filterMaxLine() const { return m_filterMaxLine; };
	/** The type of the filter (ShowAll, ShowMatching, HideMatching). */
	FilterType filterTypeTarget() const { return m_filterTypeTarget; };
	void setFilterTypeTarget( FilterType filterType );
	/** A list of targets that should be filtered. */
	QStringList filterTargetList() const { return m_filterTargetList; };
	DepartureArrivalListType departureArrivalListType() const { return m_departureArrivalListType; };
	JourneyListType journeyListType() const { return m_journeyListType; };
	bool isHeaderVisible() const { return m_showHeader; };
	void setShowHeader( bool showHeader );
	bool isColumnTargetHidden() const { return m_hideColumnTarget; };
	void setHideColumnTarget( bool hideColumnTarget );
	/** Wheather or not the current service provider uses a seperate city value. */
	bool useSeperateCityValue() const { return m_useSeperateCityValue; };
	/** Wheather or not the city name input is restricted to the given list of city names. */
	bool onlyUseCitiesInList() const { return m_onlyUseCitiesInList; };
	void getServiceProviderInfo();
	bool useDefaultFont() const { return m_useDefaultFont; };
	QFont font() const { return m_useDefaultFont ? Plasma::Theme::defaultTheme()->font(Plasma::Theme::DefaultFont) : m_font; };
	bool displayTimeBold() const { return m_displayTimeBold; };

	void selectLocaleLocation();

	/** Gets the config name used to store wheather or not the given type of vehicle should be shown. */
	static QString vehicleTypeToConfigName( const VehicleType &vehicleType );

    signals:
	void modelNeedsUpdate();
	void departureListNeedsClearing();
	void configNeedsSaving();
	void settingsChanged();
	void configurationRequired( bool needsConfiguring, const QString &reason = QString() );

	/** Settings that require a new data request have been changed. */
	void serviceProviderSettingsChanged();
	void departureArrivalListTypeChanged( DepartureArrivalListType departureArrivalListType );
	void journeyListTypeChanged( JourneyListType journeyListType );

    public slots:
	/** Ok pressed in the config dialog. */
	void configAccepted();
	/** The selected service provider in the config dialog's combobox has changed. */
	void serviceProviderChanged( int index );
	/** The stop name in the corresponding input field in the config dialog has changed. */
	void stopNameChanged( const QString &stopName );
	/** The city name in the corresponding input field in the config dialog has changed. */
	void cityNameChanged( const QString &cityName );
	/** The service provider information dialog has been closed. */
	void accessorInfoDialogFinished();
	/** The config dialog has been closed. */
	void configDialogFinished();
	/** The result of a data source test is ready. */
	void testResult( DataSourceTester::TestResult result, const QVariant &data );
	/** The info button in the config dialog for showing all service provider information has been clicked. */
	void clickedServiceProviderInfo( bool );
	/** The data from the data engine was updated. */
	void dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data& data );

	/** A vehicle type was added to the shown vehicle types list widget. */
	void addedFilterLineType( QListWidgetItem* );
	/** A vehicle type was removed from the shown vehicle types list widget. */
	void removedFilterLineType( QListWidgetItem* );
	/** Another item was selected in the shown vehicle types list widget. */
	void filterLineTypeSelectedSelctionChanged( int );
	/** Another item was selected in the hidden vehicle types list widget. */
	void filterLineTypeAvaibleSelctionChanged( int );
	/** Updates the service provider model by inserting service provider for the
	* current location.
	* @return The index in the service provider combobox of the currently selected
	* service provider.*/
	int updateServiceProviderModel( const QString &itemText = QString() );
	void locationChanged( const QString &newLocation );

    private:
	AppletWithState *m_applet;
	DataSourceTester *m_dataSourceTester; // Tests data sources

	Ui::publicTransportConfig m_ui; // The "general" settings page
	Ui::publicTransportConfigAdvanced m_uiAdvanced; // The "advanced" section of the gerenral settings page
	Ui::publicTransportAppearanceConfig m_uiAppearance; // The "appearance" settings page
	Ui::publicTransportFilterConfig m_uiFilter; // The "filter" settings page
	Ui::accessorInfo m_uiAccessorInfo; // The accessor info dialog
	KConfigDialog *m_configDialog; // Stored for the accessor info dialog as parent
	QStandardItemModel *m_modelServiceProvider; // The model for the service provider combobox in the config dialog
	QStandardItemModel *m_modelLocations; // The model for the location combobox in the config dialog
	Plasma::DataEngine::Data m_serviceProviderData; // Service provider information from the data engine
	QVariantHash m_locationData; // Location information from the data engine.

	int m_updateTimeout; // A timeout to reload the timetable information from the internet
	bool m_showRemainingMinutes; // Wheather or not remaining minutes until departure should be shown
	bool m_showDepartureTime; // Wheather or not departure times should be shown
	QString m_city; // The currently selected city
	QString m_stop; // The currently selected stop
	QString m_stopID; // The ID of the currently selected stop or an empty string if the ID isn't available
	QString m_stopIDinConfig; // The ID of the currently typed stop in the config dialog
	int m_linesPerRow; // How many lines each row in the tree view should have
	QString m_serviceProvider; // The id of the current service provider
	QString m_location; // The current location code
	int m_timeOffsetOfFirstDeparture; // The offset in minutes from now of the first departure
	QTime m_timeOfFirstDepartureCustom; // A custom time for the first departure
	FirstDepartureConfigMode m_firstDepartureConfigMode; // The config mode for the time of the first departure
	int m_maximalNumberOfDepartures; // The maximal number of displayed departures
	int m_alarmTime; // The time in minutes before the departure at which the alarm should be fired
	QHash< VehicleType, bool > m_showTypeOfVehicle; // Wheather or not a type of vehicle should be shown
	bool	m_showNightlines; // Wheather or not night lines should be shown
	int m_filterMinLine; // The minimal line number to be shown
	int m_filterMaxLine; // The maximal line number to be shown
	FilterType m_filterTypeTarget; // The type of the filter (ShowAll, ShowMatching, HideMatching)
	QStringList m_filterTargetList; // A list of targets that should be filtered
	DepartureArrivalListType m_departureArrivalListType;
	JourneyListType m_journeyListType;
	bool m_showHeader;
	bool m_hideColumnTarget;
	bool m_useDefaultFont;
	QFont m_font;
	bool m_displayTimeBold;
// 	bool m_stopNameValid; // Wheather or not the current stop name (m_stop) is valid

	bool m_useSeperateCityValue; // Wheather or not the current service provider uses a seperate city value
	bool m_onlyUseCitiesInList; // Wheather or not the city name input is restricted to the given list of city names
        void setValuesOfAdvancedConfig();
        void setValuesOfFilterConfig();
        void setValuesOfStopSelectionConfig();
        void setValuesOfAppearanceConfig();
};

#endif // SETTINGS_HEADER