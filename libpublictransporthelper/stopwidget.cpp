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

// Header
#include "stopwidget.h"

// Own includes
#include "stopsettings.h"
#include "stopsettingsdialog.h"
#include "locationmodel.h"
#include "serviceprovidermodel.h"
#include "filter.h"

// KDE+Plasma includes
#include <KLineEdit>
#include <KPushButton>
#include <KLocalizedString>
#include <Plasma/DataEngineConsumer>

// Qt includes
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QFormLayout>
#include <QLabel>
#include <QToolButton>

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

class StopWidgetPrivate
{
    Q_DECLARE_PUBLIC( StopWidget )

public:
    StopWidgetPrivate( StopWidget *q, const StopSettings& _stopSettings,
        ServiceProviderModel *providerModel, FilterSettingsList *_filterConfigurations,
        StopSettingsDialog::Options _stopSettingsDialogOptions,
        ServiceProviderDataDialog::Options _providerDataDialogOptions,
        QList<int> _settings, int _stopIndex,
        StopSettingsWidgetFactory::Pointer _factory )
        : newlyAdded(_stopSettings.stops().isEmpty()), stopSettings(_stopSettings),
          filterConfigurations(_filterConfigurations), stop(0), provider(0),
          providerModel(providerModel ? providerModel : new ServiceProviderModel(q)),
          stopSettingsDialogOptions(_stopSettingsDialogOptions),
          providerDataDialogOptions(_providerDataDialogOptions),
          settings(_settings), stopIndex(_stopIndex), factory(_factory), q_ptr(q)
    {
        // Create layout
        QFormLayout *infoLayout = new QFormLayout;
        stopLabel = new QLabel( q );
        stop = new QLabel( q );
        provider = new QLabel( q );
        updateStopLabel();

        stop->setWordWrap( true );
        provider->setWordWrap( true );
        stop->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        provider->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );

        infoLayout->addRow( stopLabel, stop );
        infoLayout->addRow( i18nc("@info Label for the read only text label containing the "
                                  "service provider name", "Service Provider:"),
                            provider );

        KPushButton *change = new KPushButton( QIcon::fromTheme("configure"),
                                               i18nc("@action:button", "&Change..."), q );
        q->connect( change, SIGNAL(clicked()), q, SLOT(editSettings()) );

        QHBoxLayout *mainLayout = new QHBoxLayout( q );
        mainLayout->addLayout( infoLayout );
        mainLayout->addWidget( change );

        // Get notified when the provider model changes to update the provider label if needed
        q->connect( providerModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
                    q, SLOT(providerModelChanged(QModelIndex,QModelIndex)) );
    };

    inline QString providerId() const { return stopSettings[ServiceProviderSetting].toString(); };

    void updateStopLabel() {
        const int stopCount = stopSettings.stopList().count();
        stopLabel->setText( i18ncp("@info Label for the read only text label containing the "
                                   "stop name(s)", "Stop:", "Stops:", qMax(1, stopCount)) );
    };

    void updateProviderLabel() {
        const QString id = providerId();
        QModelIndex index = providerModel->indexOfServiceProvider( id );
        if ( !index.isValid() ) {
            if ( !id.isEmpty() ) {
                // Provider with the providerId not found but the ID is valid
                kWarning() << "Didn't find service provider" << id;
            }
            provider->setText( i18nc("@info/plain",
                                     "<warning>Provider %1 not found!</warning>", id) );
        } else {
            provider->setText( index.data().toString() );
        }
    };

    bool newlyAdded;
    StopSettings stopSettings;
    FilterSettingsList *filterConfigurations;
    QLabel *stopLabel;
    QLabel *stop;
    QLabel *provider;
    ServiceProviderModel *providerModel; // Model of service providers

    StopSettingsDialog::Options stopSettingsDialogOptions;
    ServiceProviderDataDialog::Options providerDataDialogOptions;
    QList<int> settings;
    int stopIndex;
    StopSettingsWidgetFactory::Pointer factory;

protected:
    StopWidget *q_ptr;
};

StopWidget::StopWidget( QWidget* parent, const StopSettings& stopSettings,
        ServiceProviderModel *providerModel, StopSettingsDialog::Options stopSettingsDialogOptions,
        ServiceProviderDataDialog::Options providerDataDialogOptions,
        FilterSettingsList *filterConfigurations, QList<int> settings, int stopIndex,
        StopSettingsWidgetFactory::Pointer factory )
        : QWidget(parent),
          d_ptr(new StopWidgetPrivate(this, stopSettings, providerModel, filterConfigurations,
                                      stopSettingsDialogOptions, providerDataDialogOptions,
                                      settings, stopIndex, factory))
{
    setStopSettings( stopSettings ); // TODO move setStopSettings to private class
}

StopWidget::~StopWidget()
{
    delete d_ptr;
}

int StopWidget::stopIndex() const
{
    Q_D( const StopWidget );

    if ( d->stopIndex != -1 ) {
        return d->stopIndex;
    } else if ( parentWidget() && parentWidget()->parentWidget() ) {
        StopListWidget *stopListWidget = qobject_cast< StopListWidget* >(
                parentWidget()->parentWidget()->parentWidget() );
        if ( !stopListWidget ) {
            kDebug() << "Parent widget isn't a StopListWidget";
            return -1;
        }
        return stopListWidget->indexOf( const_cast<StopWidget*>(this) );
    } else {
        return -1;
    }
}

void StopWidget::setStopIndex( int stopIndex )
{
    Q_D( StopWidget );
    d->stopIndex = stopIndex;
}

void StopWidget::setStopSettings( const StopSettings& stopSettings )
{
    Q_D( StopWidget );
    d->stop->setText( stopSettings[CitySetting].toString().isEmpty()
            ? stopSettings.stops().join( ",\n" )
            : i18nc("@info Shown in a read-only widget (StopWidget) with a city "
                    "(%1: stop name(s), %2: city)", "%1 in %2",
                    stopSettings.stops().join( ",<nl/>" ), stopSettings[CitySetting].toString()) );

    // Copy filter configurations from StopSettings
    if ( stopSettings.hasSetting(FilterConfigurationSetting) && d->filterConfigurations ) {
        *d->filterConfigurations = stopSettings.get<FilterSettingsList>( FilterConfigurationSetting );
    }

    d->stopSettings = stopSettings;
    d->newlyAdded = false;

    d->updateStopLabel();
    d->updateProviderLabel();
}

void StopWidget::providerModelChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight )
{
    Q_D( StopWidget );
    const QModelIndex index = d->providerModel->indexOfServiceProvider( d->providerId() );
    if ( topLeft == index || (index.row() > topLeft.row() && index.row() <= bottomRight.row()) ) {
        // The provider has changed in the model
        d->updateProviderLabel();
    }
}

void StopWidget::addButton( QToolButton* button )
{
    QHBoxLayout *mainLayout = dynamic_cast< QHBoxLayout* >( layout() );
    mainLayout->addWidget( button );
}

void StopWidget::removeButton( QToolButton* button )
{
    QHBoxLayout *mainLayout = dynamic_cast< QHBoxLayout* >( layout() );
    mainLayout->removeWidget( button );
}

bool StopWidget::isHighlighted() const
{
    Q_D( const StopWidget );
    return d->stop->font().bold();
}

void StopWidget::setHighlighted( bool highlighted )
{
    Q_D( StopWidget );
    QFont font = d->stop->font();
    font.setBold( highlighted );
    d->stop->setFont( font );
}

StopSettingsDialog* StopWidget::createStopSettingsDialog()
{
    Q_D( StopWidget );

    return new StopSettingsDialog( this, d->stopSettings,
            d->stopSettingsDialogOptions, d->providerDataDialogOptions,
            d->filterConfigurations, stopIndex(), d->settings, d->factory );
}

void StopWidget::editSettings()
{
    Q_D( StopWidget );
    QPointer<StopSettingsDialog> dlg( createStopSettingsDialog() );
    int result = dlg->exec();
    if ( result == KDialog::Accepted ) {
        setStopSettings( dlg->stopSettings() );
        delete dlg;

        d->newlyAdded = false;
        emit changed( d->stopSettings );
    } else {
        delete dlg;
        if ( d->newlyAdded ) {
            emit remove(); // Remove again if the dialog is canceled directly
        }
        // after the StopWidget was added
    }
}

StopSettings StopWidget::stopSettings() const {
    Q_D( const StopWidget );
    return d->stopSettings;
}

FilterSettingsList *StopWidget::filterConfigurations() const
{
    Q_D( const StopWidget );
    return d->filterConfigurations;
}

void StopWidget::setFilterConfigurations( FilterSettingsList *filterConfigurations ) {
    Q_D( StopWidget );
    d->filterConfigurations = filterConfigurations;
}

class StopListWidgetPrivate
{
    Q_DECLARE_PUBLIC( StopListWidget )

public:
    StopListWidgetPrivate( StopListWidget *q, ServiceProviderModel *providerModel,
        FilterSettingsList *_filterConfigurations,
        StopSettingsDialog::Options _stopSettingsDialogOptions,
        ServiceProviderDataDialog::Options _providerInfoDialogOptions,
        QList<int> _settings, StopSettingsWidgetFactory::Pointer _factory )
        : providerModel(providerModel ? providerModel : new ServiceProviderModel(q)),
          filterConfigurations(_filterConfigurations),
          stopSettingsDialogOptions(_stopSettingsDialogOptions),
          providerDataDialogOptions(_providerInfoDialogOptions),
          settings(_settings), factory(_factory), q_ptr(q)
    {
        currentStopIndex = -1;
        newStopSettingsBehaviour = StopListWidget::OpenDialogIfNoStopsGiven;
    };

    ServiceProviderModel *providerModel;
    FilterSettingsList *filterConfigurations;
    int currentStopIndex;
    StopSettingsDialog::Options stopSettingsDialogOptions;
    ServiceProviderDataDialog::Options providerDataDialogOptions;
    QList<int> settings;
    StopSettingsWidgetFactory::Pointer factory;
    StopListWidget::NewStopSettingsBehaviour newStopSettingsBehaviour;

protected:
    StopListWidget *q_ptr;
};

StopListWidget::StopListWidget( QWidget* parent, ServiceProviderModel *providerModel,
        const StopSettingsList& stopSettingsList,
        StopSettingsDialog::Options stopSettingsDialogOptions,
        ServiceProviderDataDialog::Options providerInfoDialogOptions,
        FilterSettingsList *filterConfigurations, QList<int> settings,
        StopSettingsWidgetFactory::Pointer factory )
        : AbstractDynamicWidgetContainer(parent, RemoveButtonsBesideWidgets,
                                         AddButtonAfterLastWidget, ShowSeparators),
        d_ptr(new StopListWidgetPrivate(this, providerModel, filterConfigurations,
              stopSettingsDialogOptions, providerInfoDialogOptions, settings, factory))
{
    addButton()->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    addButton()->setText( i18nc("@action:button", "&Add Stop") );

    setStopSettingsList( stopSettingsList );
}

StopListWidget::~StopListWidget()
{
    delete d_ptr;
}

void StopListWidget::addStopWidget( const StopSettings &stopSettings )
{
    addWidget( createNewWidget(stopSettings) );
}

void StopListWidget::removeLastStopWidget()
{
    AbstractDynamicWidgetContainer::removeLastWidget();
}

void StopListWidget::setCurrentStopSettingIndex( int currentStopIndex )
{
    Q_D( StopListWidget );
    if ( d->currentStopIndex < dynamicWidgets().count() && d->currentStopIndex >= 0 ) {
        StopWidget *oldStopWidget =
                dynamicWidgets()[ d->currentStopIndex ]->contentWidget< StopWidget* >();
        oldStopWidget->setHighlighted( false );
    }

    if ( currentStopIndex >= dynamicWidgets().count() ) {
        d->currentStopIndex = dynamicWidgets().count() - 1;
    } else {
        d->currentStopIndex = currentStopIndex;
    }

    if ( d->currentStopIndex != -1 ) {
        StopWidget *stopWidget =
                dynamicWidgets()[ d->currentStopIndex ]->contentWidget< StopWidget* >();
        stopWidget->setHighlighted( true );
    }
}

FilterSettingsList *StopListWidget::filterConfigurations() const
{
    Q_D( const StopListWidget );
    return d->filterConfigurations;
}

void StopListWidget::setFilterConfigurations( FilterSettingsList *filterConfigurations )
{
    Q_D( StopListWidget );
    if ( filterConfigurations ) {
        d->filterConfigurations = filterConfigurations;
        foreach( StopWidget *stopWidget, widgets<StopWidget*>() ) {
            stopWidget->setFilterConfigurations( filterConfigurations );
        }
    }
}

StopListWidget::NewStopSettingsBehaviour StopListWidget::newStopSettingsBehaviour() const
{
    Q_D( const StopListWidget );
    return d->newStopSettingsBehaviour;
}

void StopListWidget::setNewStopSettingsBehaviour(
        StopListWidget::NewStopSettingsBehaviour newStopSettingsBehaviour )
{
    Q_D( StopListWidget );
    d->newStopSettingsBehaviour = newStopSettingsBehaviour;
}

void StopListWidget::setStopSettingsList( const StopSettingsList& stopSettingsList )
{
    setWidgetCountRange();
    removeAllWidgets();

    // TODO The "qMin(..., 5)" part removed all settings other than the first 5 once the config dialog was opened!
    for ( int i = 0; i < /*qMin(*/stopSettingsList.count()/*, 5)*/; ++i ) {
        QWidget *widget = createNewWidget();
        StopWidget *stopWidget = qobject_cast< StopWidget* >( widget );
        stopWidget->setStopSettings( stopSettingsList[i] );
        addWidget( widget );
    }

    setWidgetCountRange( 1 );
}

StopSettingsList StopListWidget::stopSettingsList() const
{
    StopSettingsList list;
    foreach( StopWidget *stopWidget, widgets<StopWidget*>() ) {
        list << stopWidget->stopSettings();
    }
    return list;
}

StopSettings StopListWidget::stopSettings( int index ) const
{
    Q_ASSERT( index >= 0 && index < widgetCount() );
    return widgets<StopWidget*>().at( index )->stopSettings();
}

void StopListWidget::setStopSettings( int index, const StopSettings& stopSettings )
{
    Q_ASSERT( index >= 0 && index < widgetCount() );
    return widgets<StopWidget*>().at( index )->setStopSettings( stopSettings );
}

StopWidget* StopListWidget::stopWidget( int index ) const
{
    Q_ASSERT( index >= 0 && index < widgetCount() );
    return widgets<StopWidget*>().at( index );
}

int StopListWidget::indexOf( StopWidget* stopWidget ) const
{
    Q_ASSERT( stopWidget );
    return AbstractDynamicWidgetContainer::indexOf( stopWidget );
}

void StopListWidget::changed( const StopSettings& stopSettings )
{
    Q_D( StopListWidget );
    StopWidget *stopWidget = qobject_cast< StopWidget* >( sender() );
    Q_ASSERT_X( stopWidget, "StopListWidget::changed", "Sender isn't a StopWidget" );

    // Update filter configurations
    if ( stopSettings.hasSetting(FilterConfigurationSetting) && d->filterConfigurations ) {
        *d->filterConfigurations = stopSettings.get<FilterSettingsList>( FilterConfigurationSetting );
        foreach ( StopWidget *currentStopWidget, widgets<StopWidget*>() ) {
            currentStopWidget->setFilterConfigurations( d->filterConfigurations );
        }
    }

    int index = indexOf( stopWidget );
    emit changed( index, stopSettings );
}

QWidget* StopListWidget::createNewWidget()
{
    return createNewWidget( StopSettings() );
}

QWidget* StopListWidget::createNewWidget( const StopSettings &stopSettings )
{
    Q_D( StopListWidget );
    StopWidget *stopWidget = new StopWidget( this, stopSettings, d->providerModel,
            d->stopSettingsDialogOptions, d->providerDataDialogOptions,
            d->filterConfigurations, d->settings, -1, d->factory );
    connect( stopWidget, SIGNAL(remove()), this, SLOT(removeLastWidget()) );
    connect( stopWidget, SIGNAL(changed(StopSettings)), this, SLOT(changed(StopSettings)) );
    return stopWidget;
}

DynamicWidget* StopListWidget::createDynamicWidget( QWidget* contentWidget )
{
    DynamicWidget *dynamicWidget =
            AbstractDynamicWidgetContainer::createDynamicWidget( contentWidget );
    // Move the remove button into the StopWidget
    StopWidget *stopWidget = qobject_cast< StopWidget* >( contentWidget );
    stopWidget->addButton( dynamicWidget->takeRemoveButton() );
    return dynamicWidget;
}

DynamicWidget* StopListWidget::addWidget( QWidget* widget )
{
    Q_D( const StopListWidget );
    DynamicWidget *dynamicWidget = AbstractDynamicWidgetContainer::addWidget( widget );
    StopWidget *stopWidget = qobject_cast< StopWidget* >( widget );
    if ( d->currentStopIndex == dynamicWidgets().count() - 1 ) {
        stopWidget->setHighlighted( true );
    }

    // Open the configuration dialog when a StopWidget without settings gets added
    if ( stopWidget->stopSettings().stops().isEmpty() &&
         d->newStopSettingsBehaviour == OpenDialogIfNoStopsGiven )
    {
        stopWidget->editSettings();
    }
    return dynamicWidget;
}

int StopListWidget::removeWidget( QWidget* widget )
{
    Q_D( const StopListWidget );
    int index = AbstractDynamicWidgetContainer::removeWidget( widget );
    if ( index == d->currentStopIndex ) {
        setCurrentStopSettingIndex( index );
    }
    return index;
}

int StopListWidget::currentStopSettingIndex() const {
    Q_D( const StopListWidget );
    return d->currentStopIndex;
}

} // namespace Timetable
