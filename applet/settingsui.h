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
 * @brief This file contains classes for synchronizing settings with widgets in the UI.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SETTINGSUI_HEADER
#define SETTINGSUI_HEADER

// UI includes
#include "ui_publicTransportConfig.h"
#include "ui_publicTransportConfigAdvanced.h"
#include "ui_publicTransportFilterConfig.h"
#include "ui_publicTransportAppearanceConfig.h"
#include "ui_alarmConfig.h"

// Own includes
#include "settings.h"

class KConfigDialog;
namespace PublicTransport {
    class LocationModel;
    class ServiceProviderModel;
    class StopListWidget;
}

/**
 * @brief Manages the configuration dialog and synchronizes with Settings.
 *
 * Get the current settings in the dialog using settings(), changing the settings programatically
 * is only done class intern.
 **/
class SettingsUiManager : public QObject {
    Q_OBJECT

public:
    enum DeletionPolicy {
        DeleteWhenFinished,
        KeepWhenFinished
    };

    SettingsUiManager( const Settings &settings, KConfigDialog *parentDialog,
                       DeletionPolicy deletionPolicy = DeleteWhenFinished );

    /** @brief Gets a Settings object with the current settings in the dialog. */
    Settings settings();

signals:
    void settingsAccepted( const Settings &settings );
    void settingsFinished();

public slots:
    void removeAlarms( const AlarmSettingsList &newAlarmSettings,
                       const QList<int> &removedAlarms );

protected slots:
    void changed();

    /** @brief The config dialog has been closed. */
    void configFinished();

    /** @brief Ok pressed in the config dialog. */
    void configAccepted();

    /** @brief Loads the given @p filterConfig into the dialog. */
    void loadFilterConfiguration( const QString &filterConfig );

    /** @brief Adds a new filter configuration. */
    void addFilterConfiguration();

    /** @brief Removes the current filter configuration. */
    void removeFilterConfiguration();

    /** @brief Renames the current filter configuration. */
    void renameFilterConfiguration();

    /** @brief The action of the current filter has been changed. */
    void filterActionChanged( int index );

    /** @brief The list of affected stops of a filter configuration has been changed. */
    void affectedStopsFilterChanged();

    void filtersChanged();
    void setFilterConfigurationChanged( bool changed = true );

    void exportFilterSettings();
    void importFilterSettings();

    /** @brief Another alarm has been selected. */
    void currentAlarmChanged( int row );

    /** @brief The add alarm button has been clicked. */
    void addAlarmClicked();

    /** @brief The remove alarm button has been clicked. */
    void removeAlarmClicked();

    /** @brief The rename alarm button has been clicked. */
    void renameAlarmClicked();

    /** @brief The current alarm has been changed. */
    void alarmChanged();

    /** @brief The type of the current alarm has been changed. */
    void currentAlarmTypeChanged( int index );

    /** @brief The list of affected stops of this alarm has been changed. */
    void affectedStopsAlarmChanged();

    /** @brief An alarm item has been changed, eg. it's text or checked state. */
    void alarmChanged( int index );

    /** @brief The list of stop settings has been changed. */
    void stopSettingsChanged();

    /** @brief A new stop has been added. */
    void stopSettingsAdded();

    /** @brief A stop has been removed. */
    void stopSettingsRemoved( QWidget *widget, int widgetIndex );

    /** @brief The used filter configuration has been changed in the "filter uses"
     * tab of the filters page. */
    void usedFilterConfigChanged( QWidget *widget );

protected:
    void setValuesOfAdvancedConfig( const Settings &settings );
    void setValuesOfFilterConfig();
    void setValuesOfAlarmConfig();
    void setValuesOfAppearanceConfig( const Settings &settings );

private:
    FilterSettings currentFilterSettings() const;
    AlarmSettings currentAlarmSettings( const QString &name = QString() ) const;
    int filterConfigurationIndex( const QString &filterConfig );
    void setAlarmTextColor( int index, bool hasAffectedStops = true ) const;
    void updateStopNamesInWidgets();

    DeletionPolicy m_deletionPolicy;
//     DataSourceTester *m_dataSourceTester; // Tests data sources
    KConfigDialog *m_configDialog; // Stored for the service provider info dialog as parent

    Ui::publicTransportConfig m_ui;
    Ui::publicTransportConfigAdvanced m_uiAdvanced;
    Ui::publicTransportAppearanceConfig m_uiAppearance;
    Ui::publicTransportFilterConfig m_uiFilter;
    Ui::alarmConfig m_uiAlarms;

    ServiceProviderModel *m_modelServiceProvider; // The model for the service provider combobox in the config dialog
    LocationModel *m_modelLocations; // The model for the location combobox in the config dialog
    QVariantHash m_serviceProviderData; // Service provider information from the data engine
    QVariantHash m_locationData; // Location information from the data engine.

    StopListWidget *m_stopListWidget;

    int m_currentStopSettingsIndex;
    bool m_showHeader;
    bool m_hideTargetColumn;

    FilterSettingsList m_filters;
    QString m_lastFilterConfiguration; // The last set filter configuration
    bool m_filterConfigChanged; // Whether or not the filter configuration has changed from that defined in the filter configuration with the name [m_lastFilterConfiguration]

    QList<ColorGroupSettingsList> m_colorGroup;

    AlarmSettingsList m_alarm;
    int m_lastAlarm;
    bool m_alarmsChanged;
};

#endif // SETTINGSUI_HEADER
