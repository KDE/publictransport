/*
 *   Copyright 2013 Friedrich Pülz <fpuelz@gmx.de>
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

/** @file
* @brief This file contains the timetable service.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLESERVICE_HEADER
#define TIMETABLESERVICE_HEADER

// Own includes
#include "enums.h"

// Plasma includes
#include <Plasma/Service>
#include <Plasma/ServiceJob>

namespace Plasma {
    class DataEngine;
}

/**
 * @brief Base class for timetable service jobs.
 * Only stores a pointer to the publictransport data engine.
 **/
class TimetableServiceJob : public Plasma::ServiceJob {
    Q_OBJECT

public:
    TimetableServiceJob( Plasma::DataEngine *engine, const QString &destination,
                         const QString &operation, const QMap< QString, QVariant > &parameters,
                         QObject *parent = 0 )
            : ServiceJob(destination, operation, parameters, parent), m_engine(engine) {};

protected:
    Plasma::DataEngine *m_engine;
};

/**
 * @brief Requests additional data for a data source.
 *
 * The job actually only invokes a slot in the data engine to start the request and waits
 * (non-blocking) for a signal which gets emitted when the request has finished.
 * The data engine may execute the request in another thread, eg. for script provider plugins.
 *
 *
 **/
class RequestAdditionalDataJob : public TimetableServiceJob {
    Q_OBJECT

public:
    RequestAdditionalDataJob( Plasma::DataEngine *engine, const QString &destination,
                              const QString &operation, const QMap< QString, QVariant > &parameters,
                              QObject *parent = 0 );
    virtual ~RequestAdditionalDataJob() {};

    /** @brief Starts the additional data request. */
    virtual void start();

protected slots:
    void additionalDataRequestFinished( const QString &sourceName, int item,
                                        bool success, const QString &errorMessage );
private:
    int m_updateItem;
    int m_updateItemEnd;
    int m_itemsDone;
    int m_itemsFailed;
    QString m_errorMessage;
};

/**
 * @brief Requests a manual update of a data source.
 *
 * The job actually only invokes a slot in the data engine to start the request and waits
 * (non-blocking) for a signal which gets emitted when the request has finished.
 * The data engine may execute the request in another thread, eg. for script provider plugins.
 * It may also refuse to update the data source, because the last update was not long enough ago.
 **/
class UpdateRequestJob : public TimetableServiceJob {
    Q_OBJECT

public:
    UpdateRequestJob( Plasma::DataEngine *engine, const QString &destination,
                      const QString &operation, const QMap< QString, QVariant > &parameters,
                      QObject *parent = 0 );
    virtual ~UpdateRequestJob() {};

    /** @brief Starts the update request. */
    virtual void start();

protected slots:
    void updateRequestFinished( const QString &sourceName,
                                bool success = true, const QString &errorMessage = QString() );
};

/**
 * @brief Requests more items for a (journey) data source.
 *
 * The job actually only invokes a slot in the data engine to start the request and waits
 * (non-blocking) for a signal which gets emitted when the request has finished.
 * The data engine may execute the request in another thread, eg. for script provider plugins.
 *
 * This only works with journey data sources, where usually only a few items are available.
 * For departure/arrival data sources the number of items is usually much bigger and to make
 * sharing of such data not unnecessarily hard, the start time is fixed (or always relative to the
 * current time) for departure/arrival sources. In other words to get earlier or later
 * departures/arrivals connect to another data source with earlier/later timetable items.
 *
 * The new journeys will not replace the old ones, but will be added to the list of journeys in
 * the data source.
 **/
class RequestMoreItemsJob : public TimetableServiceJob {
    Q_OBJECT

public:
    RequestMoreItemsJob( Plasma::DataEngine *engine, const QString &destination,
                         Enums::MoreItemsDirection direction, const QString &operation,
                         const QMap< QString, QVariant > &parameters, QObject *parent = 0 );
    virtual ~RequestMoreItemsJob() {};

    /** @brief Starts the more items request. */
    virtual void start();

protected slots:
    void moreItemsRequestFinished( const QString &sourceName, Enums::MoreItemsDirection direction,
                                   bool success = true, const QString &errorMessage = QString() );

private:
    const Enums::MoreItemsDirection m_direction;
};

/**
 * @brief A service for timetable data sources of the PublicTransport data engine.
 *
 * The service provides an operation "requestAdditionalData" to get additional data for a
 * timetable item (eg. a departure) in an existing data source of the engine. This can be eg.
 * route data.
 *
 * To manually request an update of a timetable data source there is a "requestUpdate" operation.
 *
 * For journey data sources, earlier or later journeys can be requested using the
 * "requestEarlierItems" or "requestLaterItems" operations.
 **/
class TimetableService : public Plasma::Service {
    Q_OBJECT

public:
    enum ErrorCode {
        InvalidErrorCode = Plasma::ServiceJob::UserDefinedError,
        UnknownError
    };

    explicit TimetableService( Plasma::DataEngine *engine, const QString &name,
                               QObject *parent = 0 );

protected:
    /**
     * @brief Creates a new job for the given @p operation with the given @p parameters.
     *
     * @param operation The operation to create a job for.
     *   Currently supported is "requestAdditionalData".
     * @param parameters Parameters for the operation.
     * @return A pointer to the newly created job or 0 if the @p operation is unsupported.
     **/
    virtual Plasma::ServiceJob* createJob( const QString &operation,
                                           QMap< QString, QVariant > &parameters );

private:
    Plasma::DataEngine *m_engine;
};

#endif // Multiple inclusion guard
