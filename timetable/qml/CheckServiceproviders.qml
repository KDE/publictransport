/***************************************************************************
 *   Copyright (C) 2016 by R. Harish Navnit <harishnavnit@gmail.com>       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

import QtQuick 2.0

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents

import org.kde.plasma.private.publictransport.timetable 0.1 as Timetable

Item {
    id: serviceproviderCheckRoot

    anchors.fill: parent

    Timetable.TimetableBackend {
        id: backend
    }

    PlasmaComponents.Label {
        id: errorLabel
        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
        }
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        text: i18n("Failed to locate any service providers"
                + "\nPlease download a service provider and try again")
        visible: false
    }

    PlasmaComponents.Button {
        id: downloadProvidersButton
        anchors {
            top: errorLabel.bottom
            topMargin: 2
            horizontalCenter: parent.horizontalCenter
        }
        text: i18n("Download")
        visible: false
        onClicked: backend.ghnsDialogRequested()
    }

    Loader {
        id: gtfsImportLoader
        anchors.fill: parent
        source: "GtfsService.qml"
        active: {
            var data = mainDataSource.data["ServiceProviders"]
            if (data == undefined) {
                // No service providers found
                // Prompt the user to download new service providers
                errorLabel.visible = true
                downloadProvidersButton.visible = true
                return false
            }
            return true
        }
    }
}
