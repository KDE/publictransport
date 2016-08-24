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

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.extras 2.0 as PlasmaExtras

Item {
    id: timetableRoot
    height: parent.height
    width: parent.width
    anchors.fill: parent

    property var timetableSourceExists: false
    property var alternativeSourceExists: false

    property var arrivalSource: ( function isArrivalSource(source) {
        return source.indexOf("Arrivals") >= 0
    })
    property var departureSource: ( function isDepartureSource(source) {
        return source.indexOf("Departures") >= 0
    })

    PlasmaCore.DataModel {
        id: timetableData
        dataSource: timetableSource
        keyRoleFilter: "departures"
    }

    PlasmaCore.DataSource {
        id: timetableSource

        interval: 6000
        engine: "publictransport"
        connectedSources: ["Departures no_ruter|stop=Oslo Bussterminal"]

        Component.onCompleted: {
            for (var i = 0; i < sources.length; i++) {
                if ( arrivalSource(sources[i]) || departureSource(sources[i]) )
                    timetableSourceExists: true
            }
        }
    }

    PlasmaExtras.Heading {
        id: appletHeader
        visible: true
        text: i18n("Stops list");
    }

    PlasmaExtras.ScrollArea {
        width: parent.width
        height: parent.height
        anchors.top: appletHeader.bottom
        anchors.topMargin: 5

        ListView {
        id: timetableList
        anchors.fill: parent
        model: timetableData
        delegate: TimetableDelegate {}
    }
    }
}
