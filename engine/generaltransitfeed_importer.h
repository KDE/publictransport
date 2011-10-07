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

/** @file
* @brief This file contains a class to import data from GTFS feeds.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef GENERALTRANSITFEEDIMPORTER_HEADER
#define GENERALTRANSITFEEDIMPORTER_HEADER

#include "generaltransitfeed_database.h" // For GeneralTransitFeedDatabase::FieldType

#include <QThread>
#include <QString>
#include <QMutex>
#include <QVariant>

class QSqlRecord;

/**
 * @brief Imports data from GTFS feeds in a separate thread.
 *
 * Use @ref startImport to import a GTFS feed (zip file) with a given filename.
 * The progress is reported by emitting @ref progress. If importing is finished the @ref finished
 * signal gets emitted, also if it was not succesful. Use @ref hasError to see whether importing
 * was successful or not. @ref lastError returns a string explaining the error, @ref state
 * has more differentiation for errors, ie. @ref FatalError or @ref FinishedWithErrors.
 *
 * GTFS (General Transit Feed Specification) defines the following files:
 * @li @em cre.txt (Required): This file contains information about one or more transit
 *     agencies that provide the data in this feed.
 * @li @em stops.txt (Required): This file contains information about individual locations where
 *     vehicles pick up or drop off passengers.
 * @li @em routes.txt (Required): This file contains information about a transit organization's
 *     routes. A route is a group of trips that are displayed to riders as a single service.
 * @li @em trips.txt (Required): This file lists all trips and their routes. A trip is a sequence
 *     of two or more stops that occurs at specific time.
 * @li @em stop_times.txt (Required): This file lists the times that a vehicle arrives at and
 *     departs from individual stops for each trip.
 * @li @em calendar.txt (Required): This file defines dates for service IDs using a weekly
 *     schedule. Specify when service starts and ends, as well as days of the week where service
 *     is available.
 * @li @em calendar_dates.txt (Optional): This file lists exceptions for the service IDs defined
 *     in the calendar.txt file. If calendar_dates.txt includes ALL dates of service, this file
 *     may be specified instead of calendar.txt.
 * @li @em fare_attributes.txt (Optional): This file defines fare information for a transit
 *     organization's routes.
 * @li @em fare_rules.txt (Optional): This file defines the rules for applying fare information
 *     for a transit organization's routes.
 * @li @em shapes.txt (Optional): This file defines the rules for drawing lines on a map to
 *     represent a transit organization's routes.
 * @li @em frequencies.txt (Optional): This file defines the headway (time between trips) for
 *     routes with variable frequency of service.
 * @li @em transfers.txt (Optional): This file defines the rules for making connections at
 *     transfer points between routes.
 *
 * @see http://code.google.com/intl/de-DE/transit/spec/transit_feed_specification.html#transitFeedFiles
 *
 * All files are imported into a database with one table for each file. Most fields in the
 * database are also the same as in the source files (in CSV format). Instead of string IDs, which
 * are allowed in GTFS, hash values of these string IDs are used for performance reasons.
 * The fields "monday", "tuesday", ..., "sunday" in @em calendar.txt are combines into one field
 * "weekdays", which gets stored as a string of 7 characters, each '0' or '1'. The values get
 * concatenated beginning with sunday.
 * The @em shapes.txt file currently is not imported.
 **/
class GeneralTransitFeedImporter : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief States of the importer.
     **/
    enum State {
        Initializing = 0, /**< Currently initializing the database. */
        Initialized, /**< Initialized and waiting for a call to @p startImport. */
        Importing, /**< Currently importing a GTFS feed. */
        ImportingSuspended, /**< Current import is suspended. */

        FinishedSuccessfully = 10, /**< Finished importing a GTFS feed successfully. */
        FinishedWithErrors, /**< Finished importing a GTFS feed with non-fatal error(s). */
        FatalError /**< Importing a GTFS feed was aborted because of a fatal error. */
    };

    /**
     * @brief Creates a new GTFS importer for the given @p providerName.
     *
     * @param providerName The name of the provider for which a GTFS database should be opened.
     **/
    explicit GeneralTransitFeedImporter( const QString &providerName );

    virtual ~GeneralTransitFeedImporter();

    /**
     * @brief Starts importing the GTFS feed at the given @p fileName.
     *
     * It is guaranteed that @ref finished gets emitted after calling this method.
     *
     * @param fileName The name of the GTFS feed zip file to import.
     **/
    void startImport( const QString &fileName );

    /**
     * @brief The filename of the source GTFS feed.
     *
     * This is the argument given to @p startImport.
     **/
    QString sourceFileName() {
        QMutexLocker locker(&m_mutex);
        return m_fileName; };

    /** @brief The current state of the importer. */
    State state() {
        QMutexLocker locker(&m_mutex);
        return m_state; };

    /** @brief Whether or not there was an error. */
    inline bool hasError() {
        QMutexLocker locker(&m_mutex);
        return m_state == FinishedWithErrors || m_state == FatalError; };

    /** @brief A string explaining the last error. */
    QString lastError() const { return m_errorString; };

signals:
    /**
     * @brief Emitted if importing a GTFS feed was finished or aborted.
     *
     * This signal is guaranteed to be emitted after calling @p startImport.
     *
     * @param state The resulting state of the importer.
     * @param errorText A string explaining an error. If there was no error this is an empty string.
     **/
    void finished( GeneralTransitFeedImporter::State state, const QString &errorText = QString() );

    /**
     * @brief Gets emitted from time to time to report the progress of the importer.
     *
     * @param completed A value between 0 (just started) and 1 (fully completed) indicating
     *   the progress.
     **/
    void progress( qreal completed );

public slots:
    /** @brief Cancel a running import process. */
    void quit();

    /** @brief Suspend a running import process. */
    void suspend();

    /** @brief Resume a suspended import process. */
    void resume();

protected:
    virtual void run();

private:
    bool writeGtfsDataToDatabase( QSqlDatabase database, const QString &fileName,
                                  const QStringList &requiredFields, int minimalRecordCount,
                                  qint64 totalFilePosition, qint64 totalFileSize );

    bool readHeader( const QString &header, QStringList *fieldNames,
                     const QStringList &requiredFields );
    bool readFields( const QString &line, QVariantList *fieldValues,
                     const QList<GeneralTransitFeedDatabase::FieldType> &types,
                     int expectedFieldCount );

    void setError( State errorState, const QString &errorText );

    State m_state;
    QString m_providerName;
    QString m_fileName;
    QString m_errorString;
    bool m_quit;
    QMutex m_mutex;
};

#endif // Multiple inclusion guard
