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

import QtQuick 1.0
import org.kde.plasma.core 0.1 as PlasmaCore
import org.kde.plasma.graphicswidgets 0.1 as PlasmaWidgets
import org.kde.plasma.components 0.1 as PlasmaComponents
import TimetableMate 1.0

// Root element, size set to viewport size (in C++, QDeclarativeView)
Item { id: root

// A toolbar with some project actions
PlasmaComponents.ToolBar { id: mainToolBar
    anchors { left: root.left; right: root.right; top: root.top }
    height: mainToolBarLayout.implicitHeight
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
        State { name: "Shown"; PropertyChanges { target: mainToolBar; opacity: 1 } },
        State { name: "Hidden"; PropertyChanges { target: mainToolBar; opacity: 0 } }
    ]
}

Flickable { id: flickable
    boundsBehavior: Flickable.StopAtBounds // More desktop-like behavior
    z: 100
    clip: true // Hide contents under the toolbar (not blurred => hard to read)
    anchors { left: root.left;
              right: verticalScrollBar.left
              top: /*mainToolBar.state == "Shown" ?*/ mainToolBar.bottom /*: root.top*/;
              bottom: horizontalScrollBar.top
              margins: 5 }
    contentWidth: container.width
    contentHeight: container.height

    // The main element, containing a title with an icon, a set of buttons connected to
    // project actions and a set of informations about the projects
    Column { id: container
        spacing: 5
        anchors.margins: 5
        width: allowedWidth( flickable.width, 0 ) // Math.max( minWidth, Math.min(maxWidth, root.width - 20) )
        x: Math.max( 0, (flickable.width - width) / 2 ) // anchors.centerIn does not work in Flickable
        y: Math.max( 0, (flickable.height - height) / 2 )
        move: Transition { NumberAnimation { properties: "x,y" } }

        property bool compact: false
        property int maxCompactWidth: 400
        property int minWidth: 200
        property int maxWidth: 600

        // Switch to compact layout, when width gets too small
        onWidthChanged: compact = width <= maxCompactWidth

        // Toggle icon display when compact mode changed
        onCompactChanged: icon.opacity = compact ? 0.0 : 1.0

        function allowedWidth( width, offset ) {
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
            ActionButton { action: project.projectAction(Project.AbortDebugger) }
            ActionButton { action: project.projectAction(Project.AbortRunningTests) }

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
            property bool wrap: parent.compact

            // When wrapped, align text in labels left, otherwise right
            onWrapChanged: {
                var labels = [ lblDescription, lblVersion, lblChangelogEntry, lblUrl, lblAuthor,
                               lblFileName, lblScriptFileName, lblScriptExtensions ]
                var alignment = wrap ? Text.AlignLeft : Text.AlignRight;
                for ( var i = 0; i < labels.length; ++i ) {
                    labels[i].horizontalAlignment = alignment
                }
            }

            // Compute the maximum implicit width of all labels, ie. the label columns implicit width
            function implicitLabelWidth() {
                var labels = [ lblDescription, lblVersion, lblChangelogEntry, lblUrl, lblAuthor,
                               lblFileName, lblScriptFileName, lblScriptExtensions ]
                var implicitWidth = 0
                for ( var i = 0; i < labels.length; ++i )
                    implicitWidth = Math.max( implicitWidth, labels[i].implicitWidth );
                return implicitWidth
            }

            // Show the description for the project
            Text { id: lblDescription; text: i18nc("@label", "Description:");
                   width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: description; text: info.description;
                   width: parent.fieldWidth; wrapMode: Text.Wrap }

            // A link to the service providers home page
            Text { id: lblUrl; text: i18nc("@label", "URL:");
                   width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: url; text: "<a href='#'>" + info.url + "</a>";
                   width: parent.fieldWidth; elide: Text.ElideMiddle
                   onLinkActivated: project.showWebTab()
            }

            // The version of the project
            Text { id: lblVersion; text: i18nc("@label", "Version:");
                   width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: version; text: info.version; width: parent.fieldWidth }

            // Information about the last changelog entry
            Text { id: lblChangelogEntry; text: i18nc("@label", "Last Changelog Entry:");
                   width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: changelogEntry;
                width: parent.fieldWidth; wrapMode: Text.Wrap
                text: info.lastChangelogAuthor.length == 0
                    ? i18nc("@info", "Version %1, <message>%2</message>", info.lastChangelogVersion,
                            cutString(info.lastChangelogDescription, 100))
                    : i18nc("@info", "Version %1, <message>%2</message> (by %3)",
                            info.lastChangelogVersion, cutString(info.lastChangelogDescription, 100),
                            info.lastChangelogAuthor)
                function cutString( string, maxChars ) {
                    return string.length > maxChars ? string.substring(0, maxChars) + "..." : string;
                }
            }

            // Author (and email)
            Text { id: lblAuthor; text: i18nc("@label", "Author:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: author;
                text: info.author + (info.email.length == 0
                    ? "" : " (<a href='mailto:" + info.email + "'>" + info.email + "</a>)")
                wrapMode: Text.Wrap; width: parent.fieldWidth
                onLinkActivated: Qt.openUrlExternally(link)
            }

            // Strip the path from a file path to get the file name
            function fileNameFromPath( filePath ) {
                var pos = filePath.lastIndexOf('/');
                return pos == -1 ? filePath : filePath.substring(pos + 1);
            }

            // A link to the project source XML document, opens the document in a tab
            Text { id: lblFileName; text: i18nc("@label", "File Name:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: fileName;
                text: "<a href='#'>" + projectInfo.fileNameFromPath(info.fileName) + "</a>";
                width: parent.fieldWidth; elide: Text.ElideMiddle
                onLinkActivated: project.showAccessorTab()
            }

            // A link to the script document, opens the document in a tab
            // If no script was created for the project, a button to do so gets created
            Component { id: scriptInfoText
                Text { text: "<a href='#'>" + projectInfo.fileNameFromPath(info.scriptFileName) + "</a>"
                    width: projectInfo.fieldWidth; elide: Text.ElideMiddle
                    onLinkActivated: project.showScriptTab()
                }
            }
            Component { id: createScriptButton
                ActionButton { action: project.projectAction(Project.ShowScriptTab); }
            }
            Text { id: lblScriptFileName; text: i18nc("@label", "Script File Name:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Loader { id: scriptFileName;
                sourceComponent: info.scriptFileName.length == 0 ? createScriptButton : scriptInfoText }

            // Used script extensions
            Text { id: lblScriptExtensions; text: i18nc("@label", "Script Extensions:");
                width: parent.labelWidth; wrapMode: Text.WordWrap; font.bold: true }
            Text { id: scriptExtensions;
                text: info.scriptExtensions.length == 0
                    ? i18nc("@info", "(none)") : info.scriptExtensions.join(", ")
                width: parent.fieldWidth; elide: Text.ElideMiddle }
        }
    }
} // Flickable

// Horizontal/vertical scroll bars
PlasmaComponents.ScrollBar { id: horizontalScrollBar
    flickableItem: flickable; orientation: Qt.Horizontal
    height: visible ? implicitHeight : 0
    anchors { left: flickable.left; right: flickable.right
              bottom: root.bottom; topMargin: 5 }
}
PlasmaComponents.ScrollBar { id: verticalScrollBar
    flickableItem: flickable; orientation: Qt.Vertical
    width: visible ? implicitWidth : 0
    anchors { top: flickable.top; bottom: flickable.bottom
              right: root.right; leftMargin: 5 }
}

} // root Item
