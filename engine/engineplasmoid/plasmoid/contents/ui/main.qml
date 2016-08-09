import QtQuick 2.0
import QtQuick.Layouts 1.1

import org.kde.plasmoid 2.0
import org.kde.plasma.core as PlasmaCore 2.0
import org.kde.plasma.components as PlasmaComponents 2.0
import org.kde.plasma.extras as PlasmaExtras 2.0


Item {
    id: timetableApplet

    Layout.minimumWidth: 256
    Layout.minimumHeight: 256
    Layout.fillWidth: true
    Layout.fillHeight: true
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    property Item timetable: timetableLoader.item

    PlasmaExtras.Heading {
        id: appletHeader
        width: parent.width
        visible: true
        text: i18n("Stops list");
    }

    PlasmaExtras.ScrollArea {
        id: mainScrollArea
        anchors.fill: parent

        height: parent.height
        width: parent.width

        Flickable {
            id: timetableView
            anchors.fill: parent

            contentWidth: width
            contentHeight: contentsColumn.height

            Column {
                id: contentsColumn
                width: timetableView.width

                Loader {
                    id: timetableLoader
                    width: parent.width
                    source: "Timetable.qml"
                    active: true
                }
            }
        }
    }
}
