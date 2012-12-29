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

property int maxCompactWidth: 400
property int maxNoFrameWidth: 600
property int padding: 0

state: "Default"
states: [
    // No jumps allowed between "Default" state and "Compact" state
    State { id: defaultState; name: "Default"
        PropertyChanges { target: root; padding: background.offset }
        PropertyChanges { target: background; opacity: 1.0 }
    },
    State { id: noFrameState; name: "NoFrame"
        PropertyChanges { target: root; padding: background.offset / 2 }
        PropertyChanges { target: projectInfo; wrap: false }
        PropertyChanges { target: background; opacity: 0.0 }
        PropertyChanges { target: icon; opacity: 1.0 }
    },
    State { id: compactState; name: "Compact"
        PropertyChanges { target: root; padding: 3 }
        PropertyChanges { target: projectInfo; wrap: true }
        PropertyChanges { target: icon; opacity: 0.0 }
    }
]

Component.onCompleted: state = width > maxNoFrameWidth ? "Default"
        : (width > maxCompactWidth ? "NoFrame" : "Compact")

// Switch to different layout states for different widths
onWidthChanged: {
    switch ( state ) {
    case "Compact":
        if ( width > maxCompactWidth + 10 )
            state = "NoFrame"
        break;
    case "NoFrame":
        if ( width > maxNoFrameWidth )
            state = "Default"
        else if ( width < maxCompactWidth )
            state = "Compact"
        break;
    case "Default":
    default:
        if ( width < maxNoFrameWidth )
            state = "NoFrame"
        break;
    }
}

// A toolbar with some project actions
PlasmaComponents.ToolBar { id: mainToolBar
    anchors { left: root.left; right: root.right; top: root.top }
    tools: Flow { id: mainToolBarLayout
        Component.onCompleted: {
            // Turn on text for all child ToolButtons
            for ( i = 0; i < children.length - 1; ++i ) {
                if ( children[i].hasOwnProperty("nativeWidget") )
                    children[i].nativeWidget.toolButtonStyle = Qt.ToolButtonTextBesideIcon;
            }
        }
        ActionToolButton { action: project.projectAction(Project.ShowProjectSettings) }
        ActionToolButton { action: project.projectAction(Project.Save) }
        ActionToolButton { action: project.projectAction(Project.RunAllTests) }
        ActionToolButton { action: project.projectAction(Project.Close) }
        Item { width: 10; height: 10 }
    }

    Behavior on opacity { PropertyAnimation{} }

    state: "Shown"
    states: [
        State { name: "Shown";
            PropertyChanges { target: mainToolBar; opacity: 1 }
            PropertyChanges { target: flickable; anchors.top: mainToolBar.bottom }
        },
        State { name: "Hidden";
            PropertyChanges { target: mainToolBar; opacity: 0 }
            PropertyChanges { target: flickable; anchors.top: root.top }
        }
    ]
}

Flickable { id: flickable
    boundsBehavior: Flickable.StopAtBounds // More desktop-like behavior
    z: 100
    clip: true // Hide contents under the toolbar (not blurred => hard to read)
    anchors { left: root.left
              right: verticalScrollBar.left
              top: mainToolBar.bottom // FIXME anchor loop, if mainToolBar changes it's height
              bottom: horizontalScrollBar.top
              margins: 3 }
    contentWidth: container.width + 2 * root.padding
    contentHeight: container.height + 2 * root.padding

    Item { id: content
    PlasmaCore.FrameSvgItem { id: background
        imagePath: "widgets/translucentbackground"
        x: container.x - (visible ? offset : 0)
        y: container.y - (visible ? offset : 0)
        z: 0 // Put to the background
        width: container.width + (visible ? 2 * offset : 0)
        height: container.height + (visible ? 2 * offset : 0)

        property int offset: 35
        Behavior on opacity { PropertyAnimation{} }
    }
    // The main element, containing a title with an icon, a set of buttons connected to
    // project actions and a set of informations about the projects
    Column { id: container
        spacing: 5
        z: 50
        clip: true // clips eg. long links (links don't break)
        move: Transition { NumberAnimation { properties: "x,y" } }
        width: container.allowedWidth( flickable.width - 2 * root.padding )
        x: Math.max( root.padding, (flickable.width - container.width) / 2 )
        y: Math.max( root.padding, (flickable.height - container.height) / 2 )

        Behavior on x { PropertyAnimation{} }
        Behavior on y { PropertyAnimation{} }

        property int minWidth: 200
        property int maxWidth: 600

        function allowedWidth( width, offset ) {
            if ( offset === undefined ) offset = 0 // Default value for offset argument
            return Math.max( minWidth - offset, Math.min(maxWidth - offset, width) );
        }

        PlasmaComponents.Button { id: toolBarToggle
            z: 1000 // Put over flickable, otherwise it is not clickable
            checkable: true
            checked: false // Initialize with false here, see below
            text: i18nc("@info/plain", "Show Toolbar")
            anchors { right: parent.right; rightMargin: 10 }
            onCheckedChanged: mainToolBar.state = checked ? "Shown" : "Hidden"
//             onCheckedChanged: mainToolBar.opacity = checked ? 1 : 0

            // Change checked value to true, if not changed here but initialized with
            // true instead, the button will not appear pressed
            Component.onCompleted: checked = true
        }

        // Show a title bar
        Row { id: titleBar
            spacing: 10
            width: Math.max( 200, parent.width )
            anchors.horizontalCenter: parent.horizontalCenter
            move: Transition { NumberAnimation { properties: "x,y" } }

            // Add an icon (invisible in compact mode)
            PlasmaWidgets.IconWidget { id: icon;
                icon: project.icon;
                anchors.verticalCenter: parent.verticalCenter
                Behavior on opacity { PropertyAnimation{} }
            }

            // Add a column to the right of the icon
            Column {
                width: parent.width - (icon.opacity > 0.0 ? icon.width : 0) - parent.spacing
                move: Transition { NumberAnimation { properties: "x,y" } }

                // Add a project title
                Text { id: title;
                    text: project.name
                    font { pointSize: 20; bold: true }
                    width: parent.width
                    wrapMode: Text.WordWrap
                }

                // Add an informational string below the title, saying where the project
                // is saved and if it is installed globally or locally
                Text { text: project.savePathInfoString; width: title.width; wrapMode: Text.Wrap }

                // Add other information strings about the project's state
                Text { font.bold: project.isModified
                    text: project.isModified ? i18nc("@info", "- Project was modified")
                                             : i18nc("@info", "- No modifications")
                }
                Text { font.bold: true
                    text: project.isDebuggerRunning
                       ? i18nc("@info", "- The script of this project currently gets executed in a debugger.") : ""
                }
                Text { font.bold: true
                    text: project.isTestRunning
                       ? i18nc("@info", "- A test for this project is currently running.") : ""
                }

                // Add a checkbox to make the project active
                PlasmaComponents.CheckBox { id: activeProjectCheckBox
                    text: project.projectAction(Project.SetAsActiveProject).text
                    checked: project.projectAction(Project.SetAsActiveProject).checked
                    onCheckedChanged: {
                        enabled = !checked; // Disable, if checked, only allow to check
                        if ( checked )
                            project.setAsActiveProject();
                    }
//                     tooltip: i18nc("@info", "Main window docks and actions are associated " +
//                                    "with the active project.")
                }
            }
        }

        // Show some buttons with project actions, surrounded by some SVG gradients
        PlasmaCore.Svg { id: svg; imagePath: svgFileName }
        PlasmaCore.SvgItem { width: titleBar.width; height: 20;
                             svg: svg; elementId: "gradientTop" }
        Flow { id: actions
            spacing: 5
            width: Math.max( 200, parent.width * 0.85 )
            anchors.horizontalCenter: parent.horizontalCenter
            move: Transition { NumberAnimation { properties: "x,y" } }

            ActionButton { action: project.projectAction(Project.ShowPlasmaPreview) }
//             ActionButton { action: project.projectAction(Project.DebugGetTimetable) }
//             ActionButton { action: project.projectAction(Project.DebugGetStopSuggestions) }
//             ActionButton { action: project.projectAction(Project.DebugGetJourneys) }
            ActionButton { action: project.projectAction(Project.AbortDebugger)
                visible: project.data.type == PublicTransport.ScriptedProvider
            }
            ActionButton { action: project.projectAction(Project.ShowGtfsDatabase)
                visible: project.data.type == PublicTransport.GtfsProvider
            }
            ActionButton { action: project.projectAction(Project.AbortRunningTests) }

            ActionButton { action: project.projectAction(Project.Publish) }
            ActionButton { action: project.projectAction(Project.Install) }
            ActionButton { action: project.projectAction(Project.InstallGlobally) }
            ActionButton { action: project.projectAction(Project.Uninstall)
                enabled: project.isInstalledLocally }
            ActionButton { action: project.projectAction(Project.UninstallGlobally)
                enabled: project.isInstalledGlobally }
        }
        PlasmaCore.SvgItem { width: titleBar.width; height: 20;
                             svg: svg; elementId: "gradientBottom" }

        // Show some properties of the project, each with a label and a field item
        // If wrap is false, the layout uses two columns, otherwise it uses only one column
        Grid { id: projectInfo
            columns: wrap ? 1 : 2 // This handles the wrapping, one column or two
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 5
            move: Transition { NumberAnimation { properties: "x,y" } }

            // Define the widths of labels and fields
            property int labelWidth: wrap ? parent.width
                    : Math.min( container.minWidth - 50, implicitLabelWidth() )
            property int fieldWidth: wrap ? parent.width
                    : container.allowedWidth( parent.width - spacing - labelWidth, 50 )

            // Whether or not fields get wrapped to the next row after their labels
            property bool wrap: false // parent.compact

            // When wrapped, align text in labels left, otherwise right
            onWrapChanged: {
                var labels = [ lblDescription, lblVersion, lblChangelogEntry, lblUrl, lblAuthor,
                               lblFileName, lblNotes, lblType,
                               lblScriptFileName, lblScriptExtensions,
                               lblFeedUrl, lblTripUpdatesUrl, lblAlertsUrl, lblTimeZone ]
                var alignment = wrap ? Text.AlignLeft : Text.AlignRight;
                for ( var i = 0; i < labels.length; ++i ) {
                    labels[i].horizontalAlignment = alignment
                }
            }

            // Compute the maximum implicit width of all labels, ie. the label columns implicit width
            function implicitLabelWidth() {
                var labels = [ lblDescription, lblVersion, lblChangelogEntry, lblUrl, lblAuthor,
                               lblFileName, lblNotes, lblType,
                               lblScriptFileName, lblScriptExtensions,
                               lblFeedUrl, lblTripUpdatesUrl, lblAlertsUrl, lblTimeZone ]
                var implicitWidth = 0
                for ( var i = 0; i < labels.length; ++i )
                    implicitWidth = Math.max( implicitWidth, labels[i].implicitWidth );
                return implicitWidth
            }

            // Strip the path from a file path to get the file name
            function fileNameFromPath( filePath ) {
                var pos = filePath.lastIndexOf('/');
                return pos == -1 ? filePath : filePath.substring(pos + 1);
            }

            function updateLayoutToProviderType( type ) {
                var isScriptProvider = type == PublicTransport.ScriptedProvider;
                var isGtfsProvider = type == PublicTransport.GtfsProvider;

                lblScriptFileName.visible = isScriptProvider;
                scriptFileName.visible = isScriptProvider;
                lblScriptExtensions.visible = isScriptProvider;
                scriptExtensions.visible = isScriptProvider;

                lblFeedUrl.visible = isGtfsProvider;
                feedUrl.visible = isGtfsProvider;
                lblTripUpdatesUrl.visible = isGtfsProvider;
                tripUpdatesUrl.visible = isGtfsProvider;
                lblAlertsUrl.visible = isGtfsProvider;
                alertsUrl.visible = isGtfsProvider;
                lblTimeZone.visible = isGtfsProvider;
                timeZone.visible = isGtfsProvider;
            }

            Component.onCompleted: projectInfo.updateLayoutToProviderType(project.data.type)

            // Show / hide type specific settings (ScriptedProvider, GtfsProvider, ...)
            Connections {
                target: project
                onDataChanged: projectInfo.updateLayoutToProviderType(project.data.type)
            }

            // Show the description for the project
            Text { id: lblDescription; text: i18nc("@label", "Description:");
                   width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: description;
                text: project.data.description.length == 0
                    ? "<a href='#'>Add description in project settings</a>" : project.data.description;
                width: parent.fieldWidth; wrapMode: Text.Wrap
                onLinkActivated: project.showSettingsDialog()
            }

            // A link to the service providers home page
            Text { id: lblUrl; text: i18nc("@label", "URL:");
                   width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: url;
                text: project.data.url.length == 0 ? "<a href='#'>Add URL in project settings</a>"
                    : "<a href='#'>" + project.data.url + "</a>";
                width: parent.fieldWidth; elide: Text.ElideMiddle
                onLinkActivated: project.data.url.length == 0
                    ? project.showSettingsDialog() : project.showWebTab()
            }

            // The version of the project
            Text { id: lblVersion; text: i18nc("@label", "Version:");
                   width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: version; text: project.data.version; width: parent.fieldWidth }

            // Information about the last changelog entry
            Text { id: lblChangelogEntry; text: i18nc("@label", "Last Changelog Entry:");
                   width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: changelogEntry;
                width: parent.fieldWidth; wrapMode: Text.Wrap
                text: project.data.lastChangelogVersion.length == 0 || project.data.lastChangelogDescription.length == 0
                    ? ("<a href='#'>" + i18nc("@info", "Add changelog entries in project settings") + "</a>") :
                    (project.data.lastChangelogAuthor.length == 0
                     ? i18nc("@info", "Version %1, <message>%2</message>", project.data.lastChangelogVersion,
                             cutString(project.data.lastChangelogDescription, 100))
                     : i18nc("@info", "Version %1, <message>%2</message> (by %3)",
                             project.data.lastChangelogVersion, cutString(project.data.lastChangelogDescription, 100),
                             project.data.lastChangelogAuthor))
                onLinkActivated: project.showSettingsDialog()
                function cutString( string, maxChars ) {
                    return string.length > maxChars ? string.substring(0, maxChars) + "..." : string;
                }
            }

            // Author (and email)
            Text { id: lblAuthor; text: i18nc("@label", "Author:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: author;
                text: project.data.author.length == 0
                    ? "<a href='#'>Add author information in project settings</a>"
                    : (project.data.author + (project.data.email.length == 0
                       ? "" : " (<a href='mailto:" + project.data.email + "'>" + project.data.email + "</a>)"))
                wrapMode: Text.Wrap; width: parent.fieldWidth
                onLinkActivated: project.data.author.email.length == 0
                    ? project.showSettingsDialog() : Qt.openUrlExternally(link)
            }

            // A link to the project source XML document, opens the document in a tab
            Text { id: lblFileName; text: i18nc("@label", "File Name:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: fileName;
                text: "<a href='#'>" + (project.data.fileName.length == 0
                        ? i18nc("@info", "Open source file")
                        : projectInfo.fileNameFromPath(project.data.fileName)) + "</a>";
                width: parent.fieldWidth; elide: Text.ElideMiddle
                onLinkActivated: project.showProjectSourceTab()
            }

            Text { id: lblType; text: i18nc("@label", "Type:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: type
                color: project.data.type == PublicTransport.InvalidProvider ? "red" : "black"
                text: project.data.typeName
                width: parent.fieldWidth; elide: Text.ElideMiddle }

            // A link to the script document, opens the script tab
            Text { id: lblScriptFileName; text: i18nc("@label", "Script File Name:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: scriptFileName
                text: "<a href='#'>" + (projectInfo.fileNameFromPath(project.data.scriptFileName).length == 0
                        ? i18nc("@info", "Create script file")
                        : projectInfo.fileNameFromPath(project.data.scriptFileName)) + "</a>"
                width: projectInfo.fieldWidth; elide: Text.ElideMiddle
                onLinkActivated: project.showScriptTab()
            }

            // Used script extensions
            Text { id: lblScriptExtensions; text: i18nc("@label", "Script Extensions:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: scriptExtensions;
                text: project.data.scriptExtensions.length == 0
                    ? i18nc("@info", "(none)") : project.data.scriptExtensions.join(", ")
                width: parent.fieldWidth; elide: Text.ElideMiddle }

            // GTFS feed URL
            Text { id: lblFeedUrl; text: i18nc("@label", "GTFS Feed URL:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: feedUrl;
                text: project.data.feedUrl.length == 0 ? i18nc("@info", "(none)") : project.data.feedUrl
                width: parent.fieldWidth; elide: Text.ElideMiddle }

            // GTFS trip updates URL
            Text { id: lblTripUpdatesUrl; text: i18nc("@label", "TripUpdates URL:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: tripUpdatesUrl;
                text: project.data.realtimeTripUpdateUrl.length == 0
                    ? i18nc("@info", "(none)") : project.data.realtimeTripUpdateUrl
                width: parent.fieldWidth; elide: Text.ElideMiddle }

            // GTFS alerts URL
            Text { id: lblAlertsUrl; text: i18nc("@label", "Alerts URL:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: alertsUrl;
                text: project.data.realtimeAlertsUrl.length == 0
                    ? i18nc("@info", "(none)") : project.data.realtimeAlertsUrl
                width: parent.fieldWidth; elide: Text.ElideMiddle }

            // Default time zone
            Text { id: lblTimeZone; text: i18nc("@label", "Timezone:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: timeZone;
                text: project.data.timeZone.length == 0 ? i18nc("@info", "(none)") : project.data.timeZone
                width: parent.fieldWidth; elide: Text.ElideMiddle }

            // Notes
            Text { id: lblNotes; text: i18nc("@label", "Notes:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: notes;
                text: project.data.notes.length == 0 ? "(none)" : project.data.notes;
                width: parent.fieldWidth; wrapMode: Text.WordWrap }
//             PlasmaComponents.TextArea { id: notes;
//                 readOnly: false // TODO
//                 text: project.data.notes; width: parent.fieldWidth }
        }

    } // container
    }
} // flickable

// Background gradient
Rectangle { id: gradient
    anchors.centerIn: flickable
    width: Math.max(flickable.width, flickable.height) * 1.5; height: width
    gradient: Gradient {
        GradientStop { position: 0.0; color: "white"; }
        GradientStop { position: 1.0; color: "#d8e8c2"; } // Oxygen "forest green1"
    }
    rotation: -45.0
}

// Vertical scroll bar
PlasmaComponents.ScrollBar { id: verticalScrollBar
    flickableItem: flickable; orientation: Qt.Vertical
    width: visible ? implicitWidth : 0
    anchors { top: flickable.top; bottom: flickable.bottom
              right: root.right; leftMargin: 5 }
}

// Horizontal scroll bar (only visible on very small width in compact mode)
PlasmaComponents.ScrollBar { id: horizontalScrollBar
    flickableItem: flickable; orientation: Qt.Horizontal
    height: visible ? implicitHeight : 0
    anchors { left: flickable.left; right: flickable.right
              bottom: root.bottom; topMargin: 5 }
}

} // root Item
