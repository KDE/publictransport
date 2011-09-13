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
 * Use @p startImport to import a GTFS feed with a given filename. The progress is reported by
 * emitting @p progress. If importing is finished @p finished gets emitted, also if it was not
 * succesful.
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
    QString sourceFileName() const { return m_fileName; };

    /** @brief The current state of the importer. */
    State state() const { return m_state; };

    /** @brief Whether or not the importer is currently running. */
    inline bool isRunning() const { return m_state == Importing; };

    /** @brief Whether or not there was an error. */
    inline bool hasError() const { return m_state == FinishedWithErrors || m_state == FatalError; };

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
    /**
     * @brief Cancel a running import process.
     **/
    void quit();

protected:
    virtual void run();

private:
    bool writeGtfsDataToDatabase( const QString &fileName, const QStringList &requiredFields,
                                  int minimalRecordCount, qint64 totalFilePosition,
                                  qint64 totalFileSize );

    bool readHeader( const QString &header, QStringList *fieldNames,
                     const QStringList &requiredFields );
    bool readFields( const QString &line, QVariantList *fieldValues,
                     const QList<GeneralTransitFeedDatabase::FieldType> &types,
                     int expectedFieldCount );

    void setError( State errorState, const QString &errorText );

    State m_state;
    QString m_fileName;
    QString m_errorString;
    bool m_quit;
    QMutex m_mutex;
};

#endif // Multiple inclusion guard
