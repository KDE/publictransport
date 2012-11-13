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
import TimetableMate 1.0

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

        property int offset: 25
        Behavior on opacity { PropertyAnimation{} }
    }

//     PlasmaCore.DataSource {
//         id: dataSource
//         engine: "publictransport"
//         connectedSources: ["gtfs"]
//     }

//     function deleteGtfsDatabase() {
//         var service = dataSource.serviceForSource( "gtfs" );
//         var operation = service.operationDescription( "deleteGtfsDatabase" );
//         operation.serviceProviderId = project.data.id();
//         var job = service.startOperationCall( operation );
//     }
//
//     function importGtfsDatabase() {
//         var service = dataSource.serviceForSource( "gtfs" );
//         var operation = service.operationDescription( "importGtfsFeed" );
//         operation.serviceProviderId = project.data.id();
//         var job = service.startOperationCall( operation );
//
//     //         connect( importJob, SIGNAL(result(KJob*)), this, SLOT(importFinished(KJob*)) );
//     //         connect( importJob, SIGNAL(percent(KJob*,ulong)), this, SLOT(importProgress(KJob*,ulong)) );
//     }

    Column { id: container
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: 500
        spacing: 5
        z: 50

        // Show a title bar
        Row { id: titleBar
            spacing: 10
            width: Math.max( 200, parent.width )
            anchors.horizontalCenter: parent.horizontalCenter
            move: Transition { NumberAnimation { properties: "x,y" } }

            // Add an icon (invisible in compact mode)
            PlasmaWidgets.IconWidget { id: icon;
                icon: QIcon("server-database");
                anchors.verticalCenter: parent.verticalCenter
                Behavior on opacity { PropertyAnimation{} }
            }

            // Add a column to the right of the icon
            Column {
                spacing: 5
                width: parent.width - (icon.opacity > 0.0 ? icon.width : 0) - parent.spacing
                move: Transition { NumberAnimation { properties: "x,y" } }

                // Add a project title
                Text { id: title;
                    text: "GTFS Database for " + project.data.id
                    font { pointSize: 20; bold: true }
                    width: parent.width
                    wrapMode: Text.WordWrap
                }

                // Add other information strings about the project's state
                Text {
                    font.bold: project.gtfsDatabaseState == Project.GtfsDatabaseImportRunning
                    width: title.width; wrapMode: Text.WordWrap
                    text: {
                        switch ( project.gtfsDatabaseState ) {
                        case Project.GtfsDatabaseImportFinished:
                            return i18nc("@info", "GTFS feed successfully imported");
                        case Project.GtfsDatabaseImportRunning:
                            return i18nc("@info %1 is a translated info message of the import job, " +
                                         "%2 is the percentage", "%1, %2% finished",
                                         project.gtfsFeedImportInfoMessage,
                                         project.gtfsFeedImportProgress);
                        case Project.GtfsDatabaseImportPending:
                            return i18nc("@info", "GTFS feed not imported",
                                            project.gtfsFeedImportProgress);
                        case Project.GtfsDatabaseError:
                            return project.gtfsDatabaseErrorString;
                        default:
                            return "Unknown";
                        }
                    }
                }

                PlasmaComponents.ProgressBar { id: progressBar
                    width: title.width
                    maximumValue: 100; value: project.gtfsFeedImportProgress
                    visible: project.gtfsDatabaseState == Project.GtfsDatabaseImportRunning
                }

//                 Text { font.bold: true
//                     text: importJob.isRunning
//                        ? i18nc("@info", "- Download and import GTFS feed into the database.") : ""
//                 }
            }
        } // titleBar

        // Show some buttons with project actions, surrounded by some SVG gradients
        PlasmaCore.Svg { id: svg; imagePath: svgFileName }
        PlasmaCore.SvgItem { width: titleBar.width; height: 20;
                             svg: svg; elementId: "gradientTop" }
        Flow { id: actions
            spacing: 5
            width: Math.max( 200, parent.width * 0.85 )
            anchors.horizontalCenter: parent.horizontalCenter
            move: Transition { NumberAnimation { properties: "x,y" } }

            ActionButton { action: project.projectAction(Project.ImportGtfsFeed) }
            ActionButton { action: project.projectAction(Project.DeleteGtfsDatabase) }
        }
        PlasmaCore.SvgItem { width: titleBar.width; height: 20;
                             svg: svg; elementId: "gradientBottom" }

        // Show the GTFS feed URL
        Text { id: lblFeedUrl; text: i18nc("@label", "GTFS Feed URL:");
            width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
        Text { id: feedUrl;
            text: project.data.feedUrl.length == 0
                ? "<a href='#'>Add GTFS feed URL in project settings</a>" : project.data.feedUrl;
            width: parent.fieldWidth; wrapMode: Text.Wrap
            onLinkActivated: project.showSettingsDialog()
        }

        // Show the GTFS feed last modified time
        Text { id: lblFeedLastModified; text: i18nc("@label", "GTFS Feed Last Modified:");
            width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
        Text { id: feedLastModified
            text: project.gtfsFeedSize == 0 ? i18nc("@info/plain", "(unknown)")
                                            : project.gtfsFeedLastModified.toString()
            width: parent.fieldWidth; wrapMode: Text.Wrap
        }

        // Show the GTFS feed size
        Text { id: lblFeedSize; text: i18nc("@label", "GTFS Feed Size (remote):");
            width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
        Text { id: feedSize
            text: project.gtfsFeedSize == 0 ? i18nc("@info/plain", "(unknown)")
                                            : project.gtfsFeedSizeString
            width: parent.fieldWidth; wrapMode: Text.Wrap
        }

        // Show the path of the GTFS database
        Text { id: lblDatabasePath; text: i18nc("@label", "GTFS Database Path:");
            width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true
            visible: project.gtfsDatabaseState == Project.GtfsDatabaseImportFinished }
        Text { id: databasePath; text: project.gtfsDatabasePath
            width: parent.fieldWidth; wrapMode: Text.Wrap
            visible: project.gtfsDatabaseState == Project.GtfsDatabaseImportFinished
        }

        // Show the GTFS database size
        Text { id: lblDatabaseSize; text: i18nc("@label", "GTFS Database Size (local):");
            width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true
            visible: project.gtfsDatabaseState == Project.GtfsDatabaseImportFinished }
        Text { id: databaseSize; text: project.gtfsDatabaseSizeString
            width: parent.fieldWidth; wrapMode: Text.Wrap
            visible: project.gtfsDatabaseState == Project.GtfsDatabaseImportFinished
        }
    } // container

    // Background gradient
    Rectangle { id: gradient
        anchors.centerIn: container
        width: Math.max(container.width, container.height) * 2; height: width
        gradient: Gradient {
            GradientStop { position: 0.0; color: "white"; }
            GradientStop { position: 1.0; color: "#d8e8c2"; } // Oxygen "forest green1"
        }
        rotation: -45.0
    }
}
