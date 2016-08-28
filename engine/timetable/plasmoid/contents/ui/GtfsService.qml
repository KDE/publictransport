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

Item {
    id: gtfsServiceRoot
    anchors.fill: parent

    property var isGtfsImportInProgress: false

    // TODO: Handle the case when more than one service providers are installed
    property var providerId: (function getServiceProviderId() {
        var data = mainDataSource.data["ServiceProviders"]
        var serviceproviders = Object.keys(data)
        return serviceproviders[0]
    })

    property var importDb: (function startGtfsImport() {
        var service = mainDataSource.serviceForSource("GTFS")
        var op = service.operationDescription("importGtfsDatabase")
        op.serviceProviderId = providerId
        service.startOperationCall(op)
    })

    property var updateFeed: (function updateGtfsFeedInfo() {
        var service = mainDataSource.serviceForSource("GTFS")
        var op = service.operationDescription("importGtfsDatabase")
        op.serviceProviderId = providerId
        service.startOperationCall(op)
    })

    property var updateDb: (function startGtfsUpdate() {
        var service = mainDataSource.serviceForSource("GTFS")
        var op = service.operationDescription("updateGtfsDatabase")
        op.serviceProviderId = providerId
        service.startOperationCall(op)
    })

    property var deleteDb: (function deleteGtfsDatabase() {
        var service = mainDataSource.serviceForSource("GTFS")
        var op = service.operationDescription("deleteGtfsFeedInfo")
        op.serviceProviderId = providerId
        service.startOperationCall(op)
    })

    Repeater {
        id: gtfsOperationsDisplay
        anchors.fill: parent
        model: 1
        delegate: GtfsServiceDelegate {
            importGtfsDatabase: importDb
            updateGtfsDatabase: updateDb
            deleteGtfsDatabase: deleteDb
            updateGtfsFeedInfo: updateFeed
            gtfsImportRunning : isGtfsImportInProgress
        }
    }
}
