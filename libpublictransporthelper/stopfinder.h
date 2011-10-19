/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef STOPFINDER_HEADER
#define STOPFINDER_HEADER

#include "publictransporthelper_export.h"

#include <QObject>
#include <QQueue>
#include <Plasma/DataEngine>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

class StopFinderPrivate;
class StopSuggesterPrivate;

// TODO: Finish and use this
class PUBLICTRANSPORTHELPER_EXPORT StopSuggester : public QObject {
    Q_OBJECT

public:
    enum RunningRequestOptions {
        AbortRunningRequests,
        KeepRunningRequests
    };

    explicit StopSuggester( Plasma::DataEngine *publicTransportEngine, QObject* parent = 0 );
    virtual ~StopSuggester();

    void requestSuggestions( const QString &serviceProviderID, const QString &stopSubstring,
                             const QString &city = QString(),
                             RunningRequestOptions runningRequestOptions = AbortRunningRequests );
    bool isRunning() const;

Q_SIGNALS:
    void stopSuggestionsReceived( const QStringList &stopSuggestions,
                                  const QVariantHash &stopToStopID,
                                  const QHash< QString, int > &stopToStopWeight );

protected Q_SLOTS:
    /** @brief The data from the data engine was updated. */
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

protected:
    StopSuggesterPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( StopSuggester )
    Q_DISABLE_COPY( StopSuggester )
};

/**
 * @brief Finds stops near the users current position using three data engines 
 *   (geolocation, openstreetmap and publictransport).
 * 
 * Use @ref start to start searching.
 **/
class PUBLICTRANSPORTHELPER_EXPORT StopFinder : public QObject {
    Q_OBJECT

public:
    enum Mode {
        StopNamesFromOSM, /**< Get stop names for stops near the current position from OpenStreetMap */
        ValidatedStopNamesFromOSM /**< Get first suggested stop names from publicTransport engine
                * for stop names (for stops near the current position) from OpenStreetMap */
    };

    /**
     * @brief Errors that may occur.
     **/
    enum Error {
        NoStopsFound,
        NoServiceProviderForCurrentCountry,
        OpenStreetMapDataEngineNotAvailable
    };

    enum DeletionPolicy {
        DeleteWhenFinished,
        KeepWhenFinished
    };

    StopFinder( Mode mode, Plasma::DataEngine *publicTransportEngine,
                Plasma::DataEngine *osmEngine, Plasma::DataEngine *geolocationEngine,
                int resultLimit = 25, DeletionPolicy deletionPolicy = DeleteWhenFinished,
                QObject* parent = 0 );
    virtual ~StopFinder();

    Mode mode() const;

    /**
     * @brief Start to determine a list of stops near the users current position.
     * 
     * It first queries the <em>geolocation</em> data engine for the users current position.
     * That position is then send to the <em>openstreetmap</em> data engine to get a list of
     * stop names near that position. Once new stop names arrive from the <em>openstreetmap</em>
     * data engine, they are validated/corrected by the <em>publictransport</em> data engine, 
     * ie. the first suggested stop name for the stop name from openstreetmap gets reported
     * using @ref stopsFound.
     * 
     * If there is an error in this process, @ref error is emitted.
     **/
    void start();

Q_SIGNALS:
    void finished();

    /**
     * @brief An @p error occurred.
     *
     * @param error The type of the error.
     * 
     * @param errorMessage An error message.
     **/
    void error( StopFinder::Error error, const QString &errorMessage );

    /**
     * @brief A list of @p stops has been found.
     *
     * @param stops A list of stop names.
     * 
     * @param stopIDs A list of stop IDs for the found @p stops. May be empty, if stop IDs aren't
     *   available for the used service provider.
     * 
     * @param serviceProviderID The ID of the used service provider.
     **/
    void stopsFound( const QStringList &stops, const QStringList &stopIDs,
                     const QString &serviceProviderID );

    void geolocationData( const QString &countryCode, const QString &city,
                          qreal latitude, qreal longitude, int accuracy );

protected Q_SLOTS:
    /** @brief The data from the data engine was updated. */
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

protected:
    StopFinderPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( StopFinder )
    Q_DISABLE_COPY( StopFinder )
};

}; // namespace Timetable

#endif // Multiple inclusion guard
