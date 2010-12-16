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

#ifndef PUBLICTRANSPORTRUNNERCONFIG_H
#define PUBLICTRANSPORTRUNNERCONFIG_H

// Project-Includes
#include "ui_publicTransportRunnerConfig.h"

// KDE-Includes
#include <KCModule>
#include <Plasma/DataEngine>
#include "stopsettingsdialog.h"

namespace Plasma {
    class DataEngineManager;
}
class LocationModel;
class ServiceProviderModel;

//Names of config-entries
// static const char CONFIG_TRIGGERWORD[] = "triggerWord";
static const char CONFIG_SERVICE_PROVIDER_ID[] = "serviceProviderID";
static const char CONFIG_LOCATION[] = "location";
static const char CONFIG_CITY[] = "city";
static const char CONFIG_KEYWORD_DEPARTURE[] = "departures";
static const char CONFIG_KEYWORD_ARRIVAL[] = "arrivals";
static const char CONFIG_KEYWORD_JOURNEY[] = "journey";
static const char CONFIG_KEYWORD_STOP[] = "stops";
static const char CONFIG_RESULT_COUNT[] = "resultCount";

/**
 * @brief KCModule for handling the public transport runner configuration
 * TODO: Share this somehow with the publicTransport applet?
 **/
class PublicTransportRunnerConfig : public KCModule {
Q_OBJECT

public:
    explicit PublicTransportRunnerConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());
    virtual ~PublicTransportRunnerConfig();

public slots:
    void save();
    void load();
    void defaults();

protected slots:
    void changeStopClicked();

private:
    void updateServiceProvider();

    Ui::publicTransportRunnerConfig m_ui;
    Plasma::DataEngineManager *m_manager; // For loading data engines
    StopSettings m_stopSettings;

    Plasma::DataEngine *m_publicTransportEngine;
    Plasma::DataEngine *m_favIconEngine;
    LocationModel *m_modelLocations;
    ServiceProviderModel *m_modelServiceProviders;
};

#endif