/*
 *   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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
* @brief This file contains the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef PUBLICTRANSPORTSERVICE_HEADER
#define PUBLICTRANSPORTSERVICE_HEADER

// Own includes
// #include "enums.h"
#include "generaltransitfeed_importer.h"

// Plasma includes
#include <Plasma/Service>
#include <Plasma/ServiceJob>

namespace KIO {
    class Job;
}

class TimetableAccessorInfo;
class QNetworkReply;

/**
 * @brief Imports a GTFS feed into a database.
 *
 * This is also the base class of @ref UpdateGtfsToDatabaseJob, which does produce an error if it
 * gets used without an initial import of the GTFS feed.
 *
 * Depending on the size of the GTFS feed, reading and importing it into the database can take some
 * time. Progress gets reported using the Plasma::ServiceJob API, just like this job supports
 * suspend/resume and kill.
 **/
class ImportGtfsToDatabaseJob : public Plasma::ServiceJob {
    Q_OBJECT

public:
    ImportGtfsToDatabaseJob( const QString &destination, const QString &operation,
                             const QMap< QString, QVariant > &parameters, QObject *parent = 0 );
    virtual ~ImportGtfsToDatabaseJob();

    /** @brief Starts the GTFS feed download and import. */
    virtual void start();

    inline const TimetableAccessorInfo *info() const { return m_info; };

protected slots:
    void statFeedFinished( QNetworkReply *reply );
    void downloadProgress( KJob *job, ulong percent );
    void mimeType( KIO::Job *job, const QString &type );
    void totalSize( KJob *job, qulonglong size );
    void feedReceived( KJob *job );

    void importerProgress( qreal progress );
    void importerFinished( GeneralTransitFeedImporter::State state, const QString &errorText );

protected:
    void statFeed();
    void downloadFeed();

    virtual bool doKill();
    virtual bool doSuspend();
    virtual bool doResume();

private:
    enum State {
        Initializing = 0,
        StatingFeed,
        DownloadingFeed,
        ReadingFeed,
        KillingJob,

        Ready,

        ErrorDownloadingFeed = 10,
        ErrorReadingFeed,
        ErrorInDatabase
    };

    /** A value between 0.0 and 1.0 indicating the amount of the total progress for downloading. */
    static const qreal PROGRESS_PART_FOR_FEED_DOWNLOAD = 0.1;

    State m_state; // Current state
    qreal m_progress;
    qulonglong m_size;
    TimetableAccessorInfo *m_info;
    GeneralTransitFeedImporter *m_importer;
    QString m_lastRedirectUrl;
};

/**
 * @brief Updates an already imported GTFS feed if there is a new version.
 *
 * The base class of this class is @ref ImportGtfsToDatabaseJob. This class changes it's behaviour
 * by producing an error if it gets used without an initial import of the GTFS feed.
 *
 * Depending on the size of the GTFS feed, reading and importing it into the database can take some
 * time. Progress gets reported using the Plasma::ServiceJob API, just like this job supports
 * suspend/resume and kill.
 **/
class UpdateGtfsToDatabaseJob : public ImportGtfsToDatabaseJob {
    Q_OBJECT

public:
    UpdateGtfsToDatabaseJob( const QString &destination, const QString &operation,
                             const QMap< QString, QVariant > &parameters, QObject *parent = 0 );

    /**
     * @brief Starts the GTFS feed update or produces an error if there was no initial import.
     *
     * The error that gets produced, if the GTFS feed was never completely imported, has the error
     * code -7.
     **/
    virtual void start();
};

/**
 * @brief Deletes a GTFS database for a specific service provider.
 **/
class DeleteGtfsDatabaseJob : public Plasma::ServiceJob {
    Q_OBJECT

public:
    DeleteGtfsDatabaseJob( const QString &destination, const QString &operation,
                           const QMap< QString, QVariant > &parameters, QObject *parent = 0 );

    /**
     * @brief Starts the GTFS database deletion or produces an error if there was no database.
     **/
    virtual void start();

private:
    QString m_serviceProviderId;
};

/**
 * @brief A service for the Public Transport data engine, which can import/update GTFS feeds.
 *
 * This service has an operation "UpdateGtfsFeed", which only updates already imported GTFS feeds
 * if there is a new version (job @ref UpdateGtfsToDatabaseJob). This operation gets called by
 * the GTFS accessor @ref TimetableAccessorGeneralTransitFeed to make sure the GTFS data is up to
 * date. To import a new GTFS feed for the first time the operation "ImportGtfsFeed" should be
 * used (job @ref ImportGtfsToDatabaseJob). That operation does @em not get called automatically
 * by the GTFS accessor. This is because importing GTFS feeds can require quite a lot disk space
 * and importing can take some time.
 *
 * If there is no imported data every request to the accessor (using the data engine) results in
 * an error with the error code 3 (see the field "errorCode" in the data returned from the data
 * engine). The user should then be asked to import a new GTFS feed and then the "ImportGtfsFeed"
 * operation should be called.
 *
 * To call the "ImportGtfsFeed" operation from a Plasma::Applet, code like this can be used:
 * @code
    Plasma::Service *service = engine("publictransport")->serviceForSource( QString() );
    KConfigGroup op = service->operationDescription("importGtfsFeed");
    op.writeEntry( "serviceProviderId", "us_trimet" );
    Plasma::ServiceJob *importJob = service->startOperationCall( op );
    connect( importJob, SIGNAL(finished(KJob*)), this, SLOT(importFinished(KJob*)) );
    connect( importJob, SIGNAL(percent(KJob*,ulong)), this, SLOT(importProgress(KJob*,ulong)) );
   @endcode
 * Replace @em us_trimet with the ID of the service provider, which GTFS feed should be imported.
 *
 * To delete a GTFS database for a service provider use the "DeleteGtfsDatabase" operation (job
 * @ref DeleteGtfsDatabaseJob). You can query the size of the GTFS database for a given service
 * provider by using the "ServiceProvider <em>\<ID\></em>" data source of the Public Transport
 * data engine. Replace <em>\<ID\></em> with the ID of the service provider. For GTFS accessors
 * the returned data object contains a field "gtfsDatabaseSize" and contains the database size
 * in bytes. The database sizes should be shown to the user, because they may be quite big, eg.
 * ~300MB.
 **/
class PublicTransportService : public Plasma::Service {
    Q_OBJECT

public:
    explicit PublicTransportService( const QString &name, QObject *parent = 0 );

protected:
    /**
     * @brief Creates a new job for the given @p operation with the given @p parameters.
     *
     * @param operation The operation to create a job for. Currently supported are
     *   "UpdateGtfsFeed", "ImportGtfsFeed" and "DeleteGtfsDatabase".
     * @param parameters Parameters for the operation.
     * @return A pointer to the newly created job or 0 if the @p operation is unsupported.
     **/
    virtual Plasma::ServiceJob* createJob( const QString &operation,
                                           QMap< QString, QVariant > &parameters );

private:
    QString m_name;
};

#endif // Multiple inclusion guard
