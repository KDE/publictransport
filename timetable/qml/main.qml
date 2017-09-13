/********************************************************************
Copyright (C) 2017 R. Harish Navnit <harishnavnit@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>
*********************************************************************/

import QtQuick 2.1
import QtQuick.Controls 2.2 as Controls

import org.kde.kirigami 2.0 as Kirigami
import org.kde.plasma.core 2.0 as PlasmaCore


Kirigami.ApplicationWindow {
    id: root

    property var defaultProviderId: (function getDefaultServiceProviderId() {
        var data = mainDataSource.data["ServiceProviders"]
        var serviceproviders = Object.keys(data)
        return serviceproviders[0]
    })

    PlasmaCore.DataSource {
        id: mainDataSource

        interval: 6000
        engine: "publictransport"

        // At this point we don't know if service providers exist locally
        // Hence it's not possible to form and connect to a arrival/departure source
        connectedSources: ["ServiceProviders"]
    }

    header: Kirigami.ApplicationHeader {}

    globalDrawer: Kirigami.GlobalDrawer {
        title: "Public Transport"
        titleIcon: "applications-graphics"

        actions: [
            Kirigami.Action {
                text: "Stops"
                iconName: "dialog-cancel"
                onTriggered: {
                    stopsLoader.active  = true
                    locationsLoader.active = false
                    serviceprovidersLoader.active = false
                }
            },
            Kirigami.Action {
                text: "Locations"
                iconName: "bookmarks"
                onTriggered: {
                    stopsLoader.active = false
                    locationsLoader.active = true
                    serviceprovidersLoader.active = false
                }
            },
            Kirigami.Action {
                text: "Alarms"
                iconName: "document-edit"
                enabled: false
            },
            Kirigami.Action {
                text: "Service Providers"
                iconName: "folder"
                onTriggered: {
                    stopsLoader.active = false
                    locationsLoader.active = false
                    serviceprovidersLoader.active = true
                }
            }
        ]
    }

    Loader {
        id: stopsLoader
        anchors.fill: parent
        source: "Stops.qml"
        active: false
    }

    Loader {
        id: locationsLoader
        anchors.fill: parent
        source: "Locations.qml"
        active: false
    }

    Loader {
        id: serviceprovidersLoader
        anchors.fill: parent
        source: "Serivecproviders.qml"
        active: false
    }
}
