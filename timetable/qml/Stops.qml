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
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************/


import QtQuick 2.1
import QtQuick.Controls 2.2 as Controls

import org.kde.kirigami 2.0 as Kirigami
import org.kde.plasma.core 2.0 as PlasmaCore

Kirigami.ScrollablePage {
    id: stopsPage

    /*
     * Return the ETA of arrival/departure for a given transport line
     */
    property var eta: ( function etaFromDateTime(stopDateTime) {
        var currentDate = new Date()

        if (currentDate.getHours() == stopDateTime.getHours()) {
            if (Math.abs(currentDate.getMinutes() - stopDateTime.getMinutes()) <= 1) {
                return stopDateTime.getMinutes() - currentDate.getMinutes() + " min"
            } else {
                return stopDateTime.getMinutes() - currentDate.getMinutes() + " mins"
            }
        } else {
            if (Math.abs(currentDate.getHours() - stopDateTime.getHours()) <= 1) {
                return stopDateTime.getHours() - currentDate.getHours() + " hour"
            } else {
                return stopDateTime.getHours() - currentDate.getHours()  + " hours"
            }
        }
    })

    background: Rectangle {
        color: Kirigami.Theme.viewBackgroundColor
    }

    Controls.Button {
        id: configureButton
        anchors.centerIn: parent
        text: i18n("Configure")
        onClicked: this.visible = false
        visible: {
            var data = mainDataSource.data["ServiceProviders"][defaultProviderId()]["state"]
            switch(data) {
                case undefined:
                    return true
                case "gtfs_feed_import_pending":
                    return true
                default:
                    return false
            }
        }
    }

    PlasmaCore.DataSource {
        id: timetableSource
        interval: 6000
        engine: "publictransport"
        connectedSources: ["Departures no_ruter | stop=Oslo Bussterminal"]
    }

    PlasmaCore.DataModel {
        id: timetableData
        dataSource: timetableSource
        keyRoleFilter: "departures"
    }

    ListView {
        id: stopsList
        anchors.fill: parent
        model: timetableData
        delegate: stopsDelegate
    }

    Component {
        id: stopsDelegate

        Kirigami.BasicListItem {
            id: stopItem

            Kirigami.Label {
                id: transportLineLabel
                anchors.left: parent.left
                anchors.leftMargin: 5
                text: model.TransportLine
            }

            Kirigami.Label {
                id: targetStopLabel
                anchors.centerIn: parent
                text: model.Target
            }

            Kirigami.Label {
                id: dateTimeLabel
                anchors.right: parent.right
                anchors.rightMargin: 5
                text: eta(model.DepartureDateTime)
            }
        }
    }
}
