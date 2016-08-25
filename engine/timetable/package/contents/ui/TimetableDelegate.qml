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

import org.kde.plasma.components 2.0 as PlasmaComponents

PlasmaComponents.ListItem {
    id: timetableItem

    enabled: true

    PlasmaComponents.Label {
        id: transportLineLabel
        anchors {
            left: parent.left
            leftMargin: 5
        }
        text: model.TransportLine
        elide: Text.ElideRight
        wrapMode: Text.Wrap
        textFormat: Text.StyledText
    }

    PlasmaComponents.Label {
        id: targetStopLabel
        anchors {
            left: transportLineLabel.right
            right: dateTimeLabel.left
            leftMargin: 50          //FIXME: do not hardcode values
            rightMargin: 50         //FIXME: do not hardcode values
        }
        text: model.Target
        elide: Text.ElideRight
        wrapMode: Text.Wrap
        textFormat: Text.StyledText
    }

    PlasmaComponents.Label {
        id: dateTimeLabel
        anchors {
            right: parent.right
            rightMargin: 5
        }
        text: model.DepartureDateTime
        elide: Text.ElideRight
        wrapMode: Text.Wrap
        textFormat: Text.StyledText
    }
}
