/********************************************************************
Copyright (C) 2016 R. Harish Navnit <harishnavnit@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

import QtQuick 2.0
import QtQuick.Layouts 1.1

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.plasma.components 2.0 as PlasmaComponents


Item {
    id: timetableApplet

    Layout.minimumWidth: 300
    Layout.minimumHeight: 200
    Layout.fillWidth: true
    Layout.fillHeight: true
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    property string gtfsState: mainDataSource.data["ServiceProviders"][defaultProviderId()]["state"]
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

    PlasmaComponents.Button {
        id: configureButton
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        text: i18n("Configure")
        visible: {
            var data = mainDataSource.data["ServiceProviders"][defaultProviderId()]["state"]
            if ( data == undefined) {
                return true
            } else if (gtfsState == "gtfs_feed_import_pending") {
                timetableLoader.active = false
                return true
            } else {
                timetableLoader.active = true
                return false
            }
        }
        onClicked: {
            this.visible = false
            serviceproviderCheckLoader.active = true
        }
    }

    Loader {
        id: serviceproviderCheckLoader
        anchors.fill: parent
        source: "CheckServiceproviders.qml"
        active: false
    }

    Loader {
        id: timetableLoader
        anchors.fill: parent
        source: "Timetable.qml"
        active: false
    }
}
