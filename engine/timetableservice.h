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

/** @file
* @brief This file contains the timetable service.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLESERVICE_HEADER
#define TIMETABLESERVICE_HEADER

// Plasma includes
#include <Plasma/Service>
#include <Plasma/ServiceJob>

/**
 * @brief Requests additional data for a data source.
 *
 * The job actually only invokes a slot in the data engine to start the request and waits for
 * a signal which gets emitted when the request has finished.
 **/
class RequestAdditionalDataJob : public Plasma::ServiceJob {
    Q_OBJECT

public:
    RequestAdditionalDataJob( const QString &destination, const QString &operation,
                              const QMap< QString, QVariant > &parameters, QObject *parent = 0 );
    virtual ~RequestAdditionalDataJob();

    /** @brief Starts the additional data request. */
    virtual void start();

protected slots:
    void additionalDataRequestFinished( const QVariantHash &newData,
                                        bool success, const QString &errorMessage );

private:
    int m_updateItem;
    int m_updateItemEnd;
    int m_itemsDone;
};

/**
 * @brief A service for timetable data sources of the PublicTransport data engine.
 *
 * The service provides an operation "requestAdditionalData" to get additional data for a
 * timetable item (eg. a departure) in an existing data source of the engine.
 **/
class TimetableService : public Plasma::Service {
    Q_OBJECT

public:
    explicit TimetableService( const QString &name, QObject *parent = 0 );

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
    QString m_name; // The data source name, used as destination for jobs
};

#endif // Multiple inclusion guard
