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

#ifndef STOPWIDGET_HEADER
#define STOPWIDGET_HEADER

/** @file
* @brief Contains StopWidget and StopListWidget.
*
* @author Friedrich Pülz <fpuelz@gmx.de> */

#include "publictransporthelper_export.h"

#include "stopsettings.h" // Return value
#include "dynamicwidget.h" // Base class
#include "stopsettingsdialog.h" // For enums

class DynamicWidget;
namespace Plasma {
    class DataEngine;
}

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

class StopListWidgetPrivate;
class StopWidgetPrivate;
class ServiceProviderModel;
class LocationModel;

/**
 * @brief Shows settings for one stop (stop name, service provider ID, location, etc.).
 *
 * A button <em>"Change..."</em> is added to open a @ref StopSettingsDialog, to edit the
 * stop settings.
 **/
class PUBLICTRANSPORTHELPER_EXPORT StopWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Creates a new stop widget.
     *
     * @param parent The parent widget of the stop widget. Default is 0.
     * @param stopSettings The stop settings to initialize the stop widget with.
     * @param stopSettingsDialogOptions Options for used StopSettingsDialog. The user can open
     *   a stop settings dialog to change the stop settings.
     *   Default is StopSettingsDialog::DefaultOptions.
     * @param providerDataDialogOptions Options for used ServiceProviderDataDialog. The user can
     *   open a provider data dialog from an opened stop settings dialog.
     *   Default is ServiceProviderDataDialog::DefaultOptions.
     * @param filterConfigurations A list of configured filter configurations.
     * @param settings A list of @ref StopSetting to create widgets for in StopSettingsDialogs.
     * @param factory A pointer to an object derived from @ref StopSettingsWidgetFactory,
     *   which can create the given @p setting by calling
     *   @ref StopSettingsWidgetFactory::widgetForSetting.
     *   To be used in StopSettingsDialogs.
     **/
    explicit StopWidget( QWidget* parent = 0,
            const StopSettings &stopSettings = StopSettings(),
            StopSettingsDialog::Options stopSettingsDialogOptions = StopSettingsDialog::DefaultOptions,
            ServiceProviderDataDialog::Options providerDataDialogOptions = ServiceProviderDataDialog::DefaultOptions,
            FilterSettingsList *filterConfigurations = 0,
            QList<int> settings = QList<int>()
                << FilterConfigurationSetting << AlarmTimeSetting << FirstDepartureConfigModeSetting,
            int stopIndex = -1,
            StopSettingsWidgetFactory::Pointer factory = StopSettingsWidgetFactory::Pointer::create() );

    /** @brief Destructor. */
    virtual ~StopWidget();

    /** @brief Gets the stop settings of this StopWidget. */
    StopSettings stopSettings() const;

    /** @brief Sets the stop settings of this StopWidget to @p stopSettings. */
    void setStopSettings( const StopSettings &stopSettings );

    FilterSettingsList *filterConfigurations() const;
    void setFilterConfigurations( FilterSettingsList *filterConfigurations );

    int stopIndex() const;
    void setStopIndex( int stopIndex );

    /** @brief Adds the given @p button. */
    void addButton( QToolButton *button );

    /** @brief Removes the given @p button. */
    void removeButton( QToolButton *button );

    /** @brief Whether or not this stop is highlighted, ie. currently used in the applet. */
    bool isHighlighted() const;

    /** @brief Sets whether or not this stop is highlighted, ie. currently used in the applet. */
    void setHighlighted( bool highlighted );

    /**
     * @brief Creates a StopSettingsDialog for this StopWidget.
     *
     * This function is also used to create a dialog when the change button is clicked
     * to edit stop settings.
     *
     * @return A pointer to the new StopSettingsDialog.
     **/
    StopSettingsDialog *createStopSettingsDialog();

Q_SIGNALS:
    /** @brief The settings of this StopWidget have been changed (StopSettingsDialog accepted). */
    void changed( const StopSettings &stopSettings );

    void remove();

public Q_SLOTS:
    /**
     * @brief The change button has been clicked. This opens a @ref StopSettingsDialog
     *   to change the settings of this StopWidget.
     **/
    void editSettings();

protected:
    StopWidgetPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( StopWidget )
    Q_DISABLE_COPY( StopWidget )
};

/**
 * @brief Manages a list of @ref StopWidget in a widget, with buttons to dynamically
 *  add/remove StopWidgets.
 **/
class PUBLICTRANSPORTHELPER_EXPORT StopListWidget : public AbstractDynamicWidgetContainer {
    Q_OBJECT

public:
    enum NewStopSettingsBehaviour {
        OpenDialogIfNoStopsGiven,
        DoNothing
    };

    /**
     * @brief Creates a new stop list widget.
     *
     * @param parent The parent widget of the dialog. Default is 0.
     * @param stopSettingsList A list of stop settings to initialize the stop list widget with.
     * @param stopSettingsDialogOptions Options for used StopSettingsDialog. The user can open
     *   a stop settings dialog to change stop settings.
     *   Default is StopSettingsDialog::DefaultOptions.
     * @param providerDataDialogOptions Options for used ServiceProviderDataDialog. The user can
     *   open a provider data dialog from an opened stop settings dialog.
     *   Default is ServiceProviderDataDialog::DefaultOptions.
     * @param filterConfigurations A list of configured filter configurations.
     * @param settings A list of @ref StopSetting to create widgets for in StopSettingsDialogs.
     * @param factory A pointer to an object derived from @ref StopSettingsWidgetFactory,
     *   which can create the given @p setting by calling
     *   @ref StopSettingsWidgetFactory::widgetForSetting.
     *   To be used in StopSettingsDialogs.
     **/
    StopListWidget( QWidget *parent = 0,
            const StopSettingsList &stopSettingsList = StopSettingsList(),
            StopSettingsDialog::Options stopSettingsDialogOptions = StopSettingsDialog::DefaultOptions,
            ServiceProviderDataDialog::Options providerDataDialogOptions = ServiceProviderDataDialog::DefaultOptions,
            FilterSettingsList *filterConfigurations = 0,
            QList<int> settings = QList<int>()
                << FilterConfigurationSetting << AlarmTimeSetting << FirstDepartureConfigModeSetting,
            StopSettingsWidgetFactory::Pointer factory = StopSettingsWidgetFactory::Pointer::create() );

    /**
     * @brief Simple destructor.
     **/
    virtual ~StopListWidget();

    /** @brief Gets a list of stop settings. */
    StopSettingsList stopSettingsList() const;

    /** @brief Sets the list of stop settings to @p stopSettingsList. */
    void setStopSettingsList( const StopSettingsList &stopSettingsList );

    /** @brief Gets the stop settings at the given @p index. */
    StopSettings stopSettings( int index ) const;

    /** @brief Sets the stop settings at the given @p index to @p stopSettings. */
    void setStopSettings( int index, const StopSettings &stopSettings );

    /** @brief Gets the StopWidget at the given @p index. */
    StopWidget *stopWidget( int index ) const;

    /** @brief Gets the index of the given @p stopWidget. */
    int indexOf( StopWidget *stopWidget ) const;

    /**
     * @brief Gets the list of configured filter configurations to choose from
     *   for @ref StopSettigns::FilterConfigurationSetting.
     **/
    FilterSettingsList *filterConfigurations() const;

    /**
     * @brief Sets the list of configured filter configurations to choose from
     *   for @ref StopSettigns::FilterConfigurationSetting.
     *
     * @param filterConfigurations The new list of configured filter configurations.
     **/
    void setFilterConfigurations( FilterSettingsList *filterConfigurations );

    /**
     * @brief Gets the behaviour of the dialog when a new empty stop setting is added.
     **/
    NewStopSettingsBehaviour newStopSettingsBehaviour() const;

    /**
     * @brief Sets the behaviour of the dialog when a new empty stop setting is added.
     *
     * @param newStopSettingsBehaviour The new behaviour.
     **/
    void setNewStopSettingsBehaviour( NewStopSettingsBehaviour newStopSettingsBehaviour );

    /**
     * @brief Gets the index of the stop settings that are marked as currently active.
     *
     * The current stop settings are highlighted, ie. @ref StopWidget::highlighted returns true
     * for the @ref StopWidget at the current stop settings index.
     *
     * @return The index of the stop settings that are marked as currently active or -1 if
     *   @ref setCurrentStopSettingIndex hasn't been called.
     *
     * @see setCurrentStopSettingIndex
     **/
    int currentStopSettingIndex() const;
    /**
     * @brief Sets the currently active stop settings by it's index.
     *
     * The current stop settings widget gets highlighted, ie. for the @ref StopWidget at
     * @p currentStopIndex @ref StopWidget::setHighlighted is called.
     *
     * @param currentStopIndex The new active stop settings or -1 for no highlighted stop settings.
     **/
    void setCurrentStopSettingIndex( int currentStopIndex );

Q_SIGNALS:
    /**
     * @brief The stop settings of the @ref StopWidget at @p index has changed to @p stopSettings.
     *
     * @param index The index of the @ref StopWidget that has changed in the list of stop widgets.
     * @param stopSettings The new stop settings.
     **/
    void changed( int index, const StopSettings &stopSettings );

public Q_SLOTS:
    /**
     * @brief Creates a new StopWidget with the given @p stopSettings and adds it to this stop list.
     *
     * @note If the maximum widget count is already reached, no widgets get added.
     *
     * @param stopSettings The StopSettings for the new StopWidget. Default is StopSettings().
     * @see AbstractDynamicWidgetContainer::setWidgetCountRange
     **/
    void addStopWidget( const StopSettings &stopSettings = StopSettings() );

    /**
     * @brief Removes the last StopWidget from this list.
     *
     * @note If the minimum widget count is already reached, no widgets get removed.
     * @see AbstractDynamicWidgetContainer::setWidgetCountRange
     **/
    void removeLastStopWidget();

protected Q_SLOTS:
    void changed( const StopSettings &stopSettings );

protected:
    /** @brief Reimplemented from AbstractDynamicWidgetContainer. */
    virtual QWidget* createNewWidget();

    /**
     * @brief Creates a new StopWidget.
     *
     * @param stopSettings The stop settings for the new StopWidget.
     * @return A pointer to the new widget.
     **/
    QWidget* createNewWidget( const StopSettings &stopSettings );

    /** @brief Reimplemented from AbstractDynamicWidgetContainer. */
    virtual DynamicWidget* createDynamicWidget( QWidget* contentWidget );

    /** @brief Reimplemented from AbstractDynamicWidgetContainer. */
    virtual DynamicWidget* addWidget( QWidget* widget );

    /** @brief Reimplemented from AbstractDynamicWidgetContainer. */
    virtual int removeWidget( QWidget* widget );

    StopListWidgetPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( StopListWidget )
    Q_DISABLE_COPY( StopListWidget )
};

} // namespace Timetable

#endif // Multiple inclusion guard
