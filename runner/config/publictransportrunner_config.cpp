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

// Project-Includes
#include "publictransportrunner_config.h"
#include <publictransporthelper/stopsettingsdialog.h>
#include <publictransporthelper/locationmodel.h>
#include <publictransporthelper/serviceprovidermodel.h>

// KDE-Includes
#include <Plasma/AbstractRunner>
#include <Plasma/DataEngineManager>
#include <KCategorizedView>
#include <KCategorizedSortFilterProxyModel>
#include <KComboBox>
#include <KCategoryDrawer>
#include <KIcon>

// Qt-Includes
#include <QLineEdit>

K_EXPORT_RUNNER_CONFIG( publictransport, PublicTransportRunnerConfig )

using namespace Timetable;

PublicTransportRunnerConfig::PublicTransportRunnerConfig( QWidget* parent, const QVariantList& args )
        : KCModule( ConfigFactory::componentData(), parent, args )/*,
      m_ui(new PublicTransportRunnerConfigForm(parent))*/
{
    QWidget *widget = new QWidget( parent );
    m_ui.setupUi( widget );
    parent->layout()->addWidget( widget );

    m_manager = Plasma::DataEngineManager::self();
    m_publicTransportEngine = m_manager->loadEngine( "publictransport" );
    m_favIconEngine = m_manager->loadEngine( "favicons" );

    m_modelLocations = new LocationModel( this );
    m_modelLocations->syncWithDataEngine( m_publicTransportEngine );
    m_modelServiceProviders = new ServiceProviderModel( this );
    m_modelServiceProviders->syncWithDataEngine( m_publicTransportEngine, m_favIconEngine );

    connect( m_ui.btnChangeStop, SIGNAL( clicked() ),
             this, SLOT( changeStopClicked() ) );
    connect( m_ui.departureKeyword, SIGNAL( editingFinished() ), this, SLOT( changed() ) );
    connect( m_ui.arrivalKeyword, SIGNAL( editingFinished() ), this, SLOT( changed() ) );
    connect( m_ui.journeyKeyword, SIGNAL( editingFinished() ), this, SLOT( changed() ) );
    connect( m_ui.stopsKeyword, SIGNAL( editingFinished() ), this, SLOT( changed() ) );
    connect( m_ui.resultCount, SIGNAL( valueChanged( int ) ), this, SLOT( changed() ) );
}

PublicTransportRunnerConfig::~PublicTransportRunnerConfig()
{
    m_manager->unloadEngine( "publictransport" );
    m_manager->unloadEngine( "favicons" );
}

void PublicTransportRunnerConfig::changeStopClicked()
{
    StopSettingsDialog *dlg = StopSettingsDialog::createSimpleAccessorSelectionDialog( this, m_stopSettings );
    if ( dlg->exec() == QDialog::Accepted ) {
        m_stopSettings = dlg->stopSettings();
        updateServiceProvider();
        emit changed( true );
    }

    delete dlg;
}

void PublicTransportRunnerConfig::load()
{
    KCModule::load();

    // Create config-object
    KSharedConfig::Ptr cfg = KSharedConfig::openConfig( QLatin1String( "krunnerrc" ) );
    KConfigGroup grp = cfg->group( "Runners" );
    grp = KConfigGroup( &grp, "PublicTransportRunner" );

    // Read and select location
    m_stopSettings.set( LocationSetting, grp.readEntry(CONFIG_LOCATION, "showAll") );

    // Default is an empty string, the data engine then uses the default
    // service provider for the users country, if there's any
    m_stopSettings.set( ServiceProviderSetting, grp.readEntry(CONFIG_SERVICE_PROVIDER_ID, QString()) );

    // Select city
    m_stopSettings.set( CitySetting, grp.readEntry(CONFIG_CITY, QString()) );

    updateServiceProvider();
    m_ui.departureKeyword->setText( grp.readEntry( CONFIG_KEYWORD_DEPARTURE,
                                    i18nc( "This is a runner keyword to search for departures", "departures" ) ) );
    m_ui.arrivalKeyword->setText( grp.readEntry( CONFIG_KEYWORD_ARRIVAL,
                                  i18nc( "This is a runner keyword to search for arrivals", "arrivals" ) ) );
    m_ui.journeyKeyword->setText( grp.readEntry( CONFIG_KEYWORD_JOURNEY,
                                  i18nc( "This is a runner keyword to search for journeys", "journeys" ) ) );
    m_ui.stopsKeyword->setText( grp.readEntry( CONFIG_KEYWORD_STOP,
                                i18nc( "This is a runner keyword to search for stops", "stops" ) ) );

    m_ui.resultCount->setValue( grp.readEntry( CONFIG_RESULT_COUNT, 4 ) );

    emit changed( false );
}

void PublicTransportRunnerConfig::save()
{
    KCModule::save();

    //create config-object
    KSharedConfig::Ptr cfg = KSharedConfig::openConfig( QLatin1String( "krunnerrc" ) );
    KConfigGroup grp = cfg->group( "Runners" );
    grp = KConfigGroup( &grp, "PublicTransportRunner" );

    grp.writeEntry( CONFIG_LOCATION, m_stopSettings[LocationSetting].toString() );
    grp.writeEntry( CONFIG_SERVICE_PROVIDER_ID, m_stopSettings[ServiceProviderSetting].toString() );
    grp.writeEntry( CONFIG_CITY, m_stopSettings[CitySetting].toString() );
    grp.writeEntry( CONFIG_KEYWORD_DEPARTURE, m_ui.departureKeyword->text() );
    grp.writeEntry( CONFIG_KEYWORD_ARRIVAL, m_ui.arrivalKeyword->text() );
    grp.writeEntry( CONFIG_KEYWORD_JOURNEY, m_ui.journeyKeyword->text() );
    grp.writeEntry( CONFIG_KEYWORD_STOP, m_ui.stopsKeyword->text() );
    grp.writeEntry( CONFIG_RESULT_COUNT, m_ui.resultCount->value() );

    emit changed( false );
}

void PublicTransportRunnerConfig::defaults()
{
    KCModule::defaults();

    m_stopSettings.set( LocationSetting, QString() );
    m_stopSettings.set( ServiceProviderSetting, QString() );
    m_stopSettings.set( CitySetting, QString() );
    updateServiceProvider();
    m_ui.departureKeyword->setText(
        i18nc( "This is a runner keyword to search for departures", "departures" ) );
    m_ui.arrivalKeyword->setText(
        i18nc( "This is a runner keyword to search for arrivals", "arrivals" ) );
    m_ui.journeyKeyword->setText(
        i18nc( "This is a runner keyword to search for journeys", "journeys" ) );
    m_ui.stopsKeyword->setText(
        i18nc( "This is a runner keyword to search for stops", "stops" ) );
    m_ui.resultCount->setValue( 4 );

    emit changed( true );
}

void PublicTransportRunnerConfig::updateServiceProvider()
{
    if ( m_stopSettings[ServiceProviderSetting].toString().isEmpty() ) {
        m_ui.serviceProvider->setText( i18n("(use default for %1)",
                KGlobal::locale()->countryCodeToName(KGlobal::locale()->country())) );
    } else {
        QString name = m_modelServiceProviders->indexOfServiceProvider(
                           m_stopSettings[ServiceProviderSetting].toString() ).data().toString();
        m_ui.serviceProvider->setText( name );
    }
}
