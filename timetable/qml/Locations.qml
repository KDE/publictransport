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
    id: locationsPage
    anchors.fill: parent

    background: Rectangle {
        color: Kirigami.Theme.viewBackgroundColor
    }

    actions {
        main: Kirigami.Action {
            text: i18n("Add New")
            iconName: "list-add"
        }
    }

    header: Controls.ToolBar {
        Controls.ComboBox {
            id: countrySelector
            model: countryModel
            textRole: "defaultProvider"
        }
    }

    PlasmaCore.DataSource {
        id: locationSource
        interval: 6000
        engine: "publictransport"
        connectedSources: "Locations"

        Component.onCompleted: {
            for(var i = 0; i < locationModel.count; i++) {
                var locationData = locationModel.get(i)
                countryModel.append(locationData["no"])
            }
        }
    }

    PlasmaCore.DataModel {
        id: locationModel
        dataSource: locationSource
    }

    ListModel {
        id: countryModel
    }

    ListView {
        id: countryList
        width: parent.width
        model: countryModel
        delegate: Kirigami.BasicListItem {
            Kirigami.Label {
                anchors.left: parent.left
                text: model.name
            }

            Kirigami.Label {
                anchors.centerIn: parent
                text: model.description
            }

            Controls.Button {
                anchors.right: parent.right
                text: i18n("Remove")
            }
        }
    }
}
