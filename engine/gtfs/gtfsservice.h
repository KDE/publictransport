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
* @brief This file contains the GTFS service.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef GTFSSERVICE_HEADER
#define GTFSSERVICE_HEADER

// Own includes
#include "gtfsimporter.h"

// Plasma includes
#include <Plasma/Service>
#include <Plasma/ServiceJob>

namespace KIO {
    class Job;
}

class ServiceProviderData;
class QNetworkReply;

/**
 * @brief Base class for jobs that access the GTFS database.
 *
 * Subclasses should overwrite work() instead of start(). The default implementation of start()
 * calls work() from the event loop if the job can be started. Therefore you can call emitResult()
 * or setResult() from work(), which can cause problems in start().
 *
 * Before work() is called, it gets tested if the provider ID is valid (ie. there is a provider
 * with the given ID) and no other GTFS database job is currently running or
 * isAccessingGtfsDatabase() returns @c false. When the job is finished you can use the
 * canAccessGtfsDatabase property to check if work() was called to access the database.
 **/
class AbstractGtfsDatabaseJob : public Plasma::ServiceJob {
    Q_OBJECT
    Q_PROPERTY( QString serviceProviderId READ serviceProviderId )
    Q_PROPERTY( bool canAccessGtfsDatabase READ canAccessGtfsDatabase )
    Q_PROPERTY( bool isAccessingGtfsDatabase READ isAccessingGtfsDatabase )

public:
    AbstractGtfsDatabaseJob( const QString &destination, const QString &operation,
                             const QMap< QString, QVariant > &parameters, QObject *parent = 0 );
    virtual QString serviceProviderId() const = 0;

    /**
     * @brief Whether or not the data engine has allowed access to the GTFS database for this job.
     * If isAccessingGtfsDatabase() returns @c false, this function also returns @c false.
     **/
    bool canAccessGtfsDatabase() const {
        return isAccessingGtfsDatabase() ? m_canAccessGtfsDatabase : false;
    };

    /**
     * @brief Whether or not this job needs access to the GTFS database.
     * Overwrite and return @c false if the derived job does not need database access.
     **/
    virtual bool isAccessingGtfsDatabase() const { return true; };

    /** @brief Overwritten to call tryToWork() from the event loop. */
    virtual void start();

protected slots:
    /**
     * @brief Calls work() if the job can be started.
     * If there are errors, eg. an invalid provider ID or if a GTFS feed import is already running
     * work() is not called and an error gets set. To test if an import job is running
     * canImportGtfsFeed() get called, which calls the tryToStartGtfsFeedImportJob() function
     * of the PublicTransport data engine.
     * Overwrite to test for other errors before asking the data engine if the job can be started.
     **/
    virtual void tryToWork();

protected:
    /** @brief Should be overwritten instead of start(). */
    virtual void work() = 0;

    bool canImportGtfsFeed();

private:
    bool m_canAccessGtfsDatabase;
};

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
class ImportGtfsToDatabaseJob : public AbstractGtfsDatabaseJob {
    Q_OBJECT

public:
    ImportGtfsToDatabaseJob( const QString &destination, const QString &operation,
                             const QMap< QString, QVariant > &parameters, QObject *parent = 0 );
    virtual ~ImportGtfsToDatabaseJob();

    inline const ServiceProviderData *data() const { return m_data; };
    virtual QString serviceProviderId() const;

    void setOnlyGetInformation( bool onlyGetInformation ) {
        m_onlyGetInformation = onlyGetInformation;
    };

    /**
     * @brief Whether or not this job needs access to the GTFS database.
     * If this job stops after GTFS feed information was retrieved, it does not need access to
     * the database. This is the case when setOnlyGetInformation() was called with @c true and
     * this function will then return @c false.
     **/
    virtual bool isAccessingGtfsDatabase() const { return !m_onlyGetInformation; };

signals:
    void logChanged();

protected slots:
    void statFeedFinished( QNetworkReply *reply );
    void downloadProgress( KJob *job, ulong percent );
    void mimeType( KIO::Job *job, const QString &type );
    void totalSize( KJob *job, qulonglong size );
    void speed( KJob *job, ulong speed );
    void feedReceived( KJob *job );

    void importerProgress( qreal progress, const QString &currentTableName = QString() );
    void importerFinished( GtfsImporter::State state, const QString &errorText );
    void logMessage( const QString &message );

protected:
    virtual void work();
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
    static const qreal PROGRESS_PART_FOR_FEED_DOWNLOAD;

    State m_state; // Current state
    qreal m_progress;
    qulonglong m_size;
    ServiceProviderData *m_data;
    GtfsImporter *m_importer;
    QString m_lastRedirectUrl;
    QString m_lastTableName;
    bool m_onlyGetInformation;
};

/**
 * @brief Updates an already imported GTFS feed if there is a new version.
 *
 * The base class of this class is @ref ImportGtfsToDatabaseJob. This class changes it's behaviour
 * by producing an error if it gets used without an initial import of the GTFS feed
 * (error code GtfsFeedNotImported).
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

protected slots:
    /**
     * @brief Overwritten to test if the GTFS feed was imported.
     * This test needs to be done before calling canImportGtfsFeed(), otherwise the data engine
     * will set the provider state to "importing_gtfs_feed", although this updating import job
     * will not be run. */
    virtual void tryToWork();

protected:
    virtual void work();
};

/**
 * @brief Deletes a GTFS database for a specific service provider.
 **/
class DeleteGtfsDatabaseJob : public AbstractGtfsDatabaseJob {
    Q_OBJECT

public:
    DeleteGtfsDatabaseJob( const QString &destination, const QString &operation,
                           const QMap< QString, QVariant > &parameters, QObject *parent = 0 );

    virtual QString serviceProviderId() const { return m_serviceProviderId; };

protected:
    virtual void work();

private:
    QString m_serviceProviderId;
};

/**
 * @brief A service to control GTFS feed import/update and GTFS database deletion.
 *
 * This service has an operation "updateGtfsFeed", which only updates already imported GTFS feeds
 * if there is a new version (job @ref UpdateGtfsToDatabaseJob). This operation gets called by
 * the GTFS provider @ref ServiceProviderGtfs to make sure the GTFS data is up to date. To import
 * a new GTFS feed for the first time the operation "importGtfsFeed" should be used (job
 * @ref ImportGtfsToDatabaseJob). That operation does @em not get called automatically by the
 * GTFS provider. This is because importing GTFS feeds can require quite a lot disk space and
 * importing can take some time.
 *
 * If there is no imported data every request to the provider (using the data engine) results in
 * an error with the error code GtfsErrorFeedImportRequired (see the field "errorCode" in the data
 * returned from the data engine). The user should then be asked to import a new GTFS feed and
 * then the "importGtfsFeed" operation should be called.
 *
 * To call the "importGtfsFeed" operation from a Plasma::Applet, code like this can be used:
 * @code
    Plasma::Service *service = engine("publictransport")->serviceForSource( "GTFS" );
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
 * data engine. Replace <em>\<ID\></em> with the ID of the service provider. For GTFS providers
 * the returned data object contains a field "gtfsDatabaseSize" in the field "stateData" and
 * contains the database size in bytes. The database sizes should be shown to the user, because
 * they may be quite big, eg. ~300MB.
 **/
class GtfsService : public Plasma::Service {
    Q_OBJECT

public:
    explicit GtfsService( const QString &name, QObject *parent = 0 );

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
