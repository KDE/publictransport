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
import QtQuick.Layouts 1.1 as Layouts
import QtQuick.Controls 2.2 as Controls

import org.kde.kirigami 2.0 as Kirigami
import org.kde.plasma.core 2.0 as PlasmaCore

Kirigami.ScrollablePage {
    id: stopsPage

    property string sourceStr: sourceSwitcher.currentText + defaultProviderId() +
                                     "|stop=" + stopSwitcher.currentText

    property var stopEta: (function etaFromDateTime(stopDateTime) {
        var currentDate = new Date()
        var etaHrsString = Math.abs(stopDateTime.getHours() - currentDate.getHours())
        var etaMinString = Math.abs(stopDateTime.getMinutes() - currentDate.getMinutes())

        if (currentDate.getHours() == stopDateTime.getHours()) {
            etaMinString <= 1 ? etaMinString += " min" : etaMinString += " mins"
            return etaMinString
        } else {
            etaHrsString <= 1 ? etaHrsString += " hour" : etaHrsString += " hours"
            return etaHrsString
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
        connectedSources: [sourceStr]
    }

    PlasmaCore.DataModel {
        id: timetableData
        dataSource: timetableSource
        keyRoleFilter: sourceSwitcher.currentIndex ? "departures" : "arrivals"
    }

    header: Controls.ToolBar {
        Layouts.RowLayout {
            spacing: Kirigami.Units.smallSpacing

            Controls.ComboBox {
                id: providerSwitcher
                model: serviceproviderModel
                textRole: "id"
            }

            // TODO: Read stop names from the dataengine
            //       Might require re-introducing the stop completion module
            Controls.ComboBox {
                id: stopSwitcher
                model: ["Villingstadveien", "Oslo Bussterminal", "Brattilia", "Sandli", "Listad"]
            }

            Controls.ComboBox {
                id: sourceSwitcher
                model: ["Arrivals", "Departures" ]
            }
        }
    }

    ListView {
        id: stopsList
        anchors.fill: parent
        model: timetableData
        delegate: Kirigami.BasicListItem {
            Kirigami.Label {
                anchors.left: parent.left
                anchors.leftMargin: Kirigami.Units.smallSpacing
                text: model.TransportLine
            }

            Kirigami.Label {
                anchors.centerIn: parent
                text: model.Target
            }

            Kirigami.Label {
                anchors.right: parent.right
                anchors.rightMargin: Kirigami.Units.smallSpacing
                text: stopEta(model.DepartureDateTime)
            }
        }
    }
}
