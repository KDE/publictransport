import QtQuick 2.0

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.plasma.components 2.0 as PlasmaComponents

// TODO: import org.kde.plasma.publictransport.engine 1.0

Column {
    id: timetableRoot
    width: parent.width

    property var alternativeSourceExists: false
    property var testArrivalSource: "Arrivals no_ruter|stop=Oslo Bussterminal"
    property var testDepartureSource: "Departures no_ruter|stop=Oslo Bussterminal"

    property var arrivalSource: ( function isArrivalSource(source) {
        return source.contains("Arrivals")
    })
    property var departureSource: ( function isDepartureSource(source) {
        return source.contains("Departures")
    })
    property var testSource: ( function connectToTestSource(sourceType) {
        arrivalSource(sourceType) 
            ? timetableSource.connectSource(testArrivalSource)
            : timetableSource.connectSource(testDepartureSource)
    })

    ListModel {
        id: timetableData
    }

    PlasmaCore.DataSource {
        id: timetableSource

        engine: "publictransport"
        interval: 0

        onSourceAdded: {
            if (arrivalSource(source) || departureSource(source)) {
                console.log("Connecting to new timetable source")
            }
            connectSource(source)
            timetableData.append({name: source})
        }

        onSourceRemoved: {
            // Remove source from model
            for (var i = 0; i < timetableData.count; i++) {
                if (timetableData.get(i).name === source) {
                    timetableData.remove();
                    break;
                }
            }

            // Check if any valid sources exist
            for (var i = 0; i < sources.count; i++) {
                if (arrivalSource(sources[i]) || departureSource(sources[i]) ) {
                    alternativeSourceExists: true
                }
            }

            if (!alternativeSourceExists) {
                console.error("No timetable sources found !")
            }
        }

        onNewData: {
            // TODO
        }

        onDataChanged: {
            // TODO
        }

        Component.onCompleted: {
            connectedSources = sources
        }
    }

    Repeater {
        model: timetableData
        delegate: TimetableDelegate {}
    }
}