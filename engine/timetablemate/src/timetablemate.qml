/*
*   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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

import QtQuick 1.1

import org.kde.plasma.core 0.1 as PlasmaCore
import org.kde.plasma.graphicswidgets 0.1 as PlasmaWidgets
import org.kde.plasma.components 0.1 as PlasmaComponents

// Root element, size set to viewport size (in C++, QDeclarativeView)
Item { id: root
    anchors.fill: parent

    PlasmaCore.FrameSvgItem { id: background
        imagePath: "widgets/translucentbackground"
        x: container.x - (visible ? offset : 0)
        y: container.y - (visible ? offset : 0)
        z: 10 // Put to the background
        width: container.width + (visible ? 2 * offset : 0)
        height: container.height + (visible ? 2 * offset : 0)
        visible: container.width >= 500

        property int offset: 35
        Behavior on opacity { PropertyAnimation{} }
    }

    Column { id: container
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min( root.width - 15, 500 )
        spacing: 5
        z: 50

        // Show a title bar
        Row { id: titleBar
            spacing: 10
            width: Math.max( 200, parent.width )
            anchors.horizontalCenter: parent.horizontalCenter
            move: Transition { NumberAnimation { properties: "x,y" } }

            // Add an icon
            PlasmaWidgets.IconWidget { id: icon;
                icon: QIcon("timetablemate");
                anchors.verticalCenter: parent.verticalCenter
                Behavior on opacity { PropertyAnimation{} }
            }

            // Add a column to the right of the icon
            Column {
                spacing: 5
                width: parent.width - (icon.opacity > 0.0 ? icon.width : 0) - parent.spacing
                move: Transition { NumberAnimation { properties: "x,y" } }

                // Add a title
                Text { id: title;
                    text: "TimetableMate"
                    font { pointSize: 20; bold: true }
                    width: parent.width
                    wrapMode: Text.WordWrap
                }

                // Add another line of text
                Text {
                    font.bold: true; width: title.width; wrapMode: Text.WordWrap
                    text: "Create, test & debug PublicTransport provider plugins"
                }
            }
        } // titleBar

        // Show some buttons with actions, surrounded by some SVG gradients
        PlasmaCore.Svg { id: svg; imagePath: svgFileName }
        PlasmaCore.SvgItem { width: titleBar.width; height: 20;
                             svg: svg; elementId: "gradientTop" }
        Flow {
            spacing: 5
            width: Math.max( 100, parent.width * 0.85 )
            anchors.horizontalCenter: parent.horizontalCenter
            move: Transition { NumberAnimation { properties: "x,y" } }

            PlasmaComponents.Button {
                text: " Show Opened Projects "; iconSource: "project-development"
                onClicked: timetableMate.showDock("projects")
                visible: timetableMate.hasOpenedProjects
            }
            PlasmaComponents.Button {
                text: " View Script API Documentation "; iconSource: "documentation"
                onClicked: timetableMate.showDock("documentation")
            }
        }
        PlasmaCore.SvgItem { width: titleBar.width; height: 20;
                             svg: svg; elementId: "gradientBottom" }

        Grid { id: actions
            property bool compact: width < 350
            columns: compact ? 1 : 2
            spacing: 20
            width: Math.max( 100, parent.width * 0.85 )
            anchors.horizontalCenter: parent.horizontalCenter
            move: Transition { NumberAnimation { properties: "x,y" } }

            // Recent projects
            PlasmaWidgets.IconWidget { icon: QIcon("document-open-recent") }
            Text { id: recentProjects; wrapMode: Text.WordWrap; width: 300
                text: i18nc("@info", "Continue work on a recently opened plugin: %1",
                            recentProjectsLinks(timetableMate.recentProjects(),
                                                timetableMate.recentUrls()))
                function recentProjectsLinks( projects, urls ) {
                    var text = "";
                    for ( i = 0; i < projects.length; ++i ) {
                        if ( text.length > 0 ) {
                            text += ", ";
                        }
                        text += "<a href='" + urls[i] + "'>" + projects[i] + "</a>";
                    }
                    return text.length > 0 ? text : i18nc("@info", "(none)");
                }
                onLinkActivated: timetableMate.open(link)
            }

            // Create new project
            ActionButton { action: timetableMate.qmlAction("project_new") }
            Text { id: newProject; wrapMode: Text.WordWrap; width: 300
                text: i18nc("@info", "Create a new service provider plugin for the " +
                            "PublicTrasnport data engine.")
            }

            // Open installed project
            ActionButton { action: timetableMate.qmlAction("project_open_installed") }
            Text { id: openInstalled; wrapMode: Text.WordWrap; width: 300
                text: i18nc("@info", "Open an installed service provider plugin. These are the " +
                            "providers that get used by the PublicTransport data engine.")
            }

            // Open abritrary project
            ActionButton { action: timetableMate.qmlAction("project_open") }
            Text { id: open; wrapMode: Text.WordWrap; width: 300
                text: i18nc("@info", "Open a service provider plugin file.")
            }

            // Fetch project
            ActionButton { action: timetableMate.qmlAction("project_fetch") }
            Text { id: fetchProject; wrapMode: Text.WordWrap; width: 300
                text: i18nc("@info", "Fetch a service provider plugin from " +
                            "<link url='http://kde-files.org/index.php?xcontentmode=638'>kde-files.org</link>.")
            }
        }
    } // container

    // Background gradient
    Rectangle { id: gradient
        anchors.centerIn: container
        width: Math.max(container.width, container.height) * 3; height: width
        gradient: Gradient {
            GradientStop { position: 0.0; color: "white"; }
            GradientStop { position: 1.0; color: "#d8e8c2"; } // Oxygen "forest green1"
        }
        rotation: -45.0
    }
}
