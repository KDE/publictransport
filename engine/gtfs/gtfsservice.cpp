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

// Own includes
#include "gtfsservice.h"
#include "serviceprovider.h"
#include "serviceproviderdata.h"
#include "serviceproviderglobal.h"
#include "serviceprovidergtfs.h"

// KDE includes
#include <KTemporaryFile>
#include <KMimeType>
#include <KDebug>
#include <KFileItem>
#include <KIO/Job>
#include <Plasma/DataEngine>
#include <kjobtrackerinterface.h>

// Qt includes
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

const qreal ImportGtfsToDatabaseJob::PROGRESS_PART_FOR_FEED_DOWNLOAD = 0.1;

AbstractGtfsDatabaseJob::AbstractGtfsDatabaseJob( const QString &destination,
        const QString &operation, const QMap< QString, QVariant > &parameters, QObject *parent )
        : ServiceJob(destination, operation, parameters, parent)
{
    Q_ASSERT( qobject_cast<GtfsService*>(parent) );
    Q_ASSERT( qobject_cast<Plasma::DataEngine*>(parent->parent()) );
}

ImportGtfsToDatabaseJob::ImportGtfsToDatabaseJob( const QString &destination,
        const QString &operation, const QMap< QString, QVariant > &parameters, QObject *parent )
        : AbstractGtfsDatabaseJob(destination, operation, parameters, parent),
          m_state(Initializing), m_data(0), m_importer(0), m_onlyGetInformation(false)
{
    setCapabilities( Suspendable | Killable );

    QString errorMessage;
    m_data = ServiceProviderDataReader::read( parameters["serviceProviderId"].toString(),
                                              &errorMessage );
    if ( !m_data ) {
        setError( GtfsErrorInvalidProviderId );
        setErrorText( errorMessage );
        return;
    }

    if ( m_data->type() != Enums::GtfsProvider ) {
        setError( GtfsErrorWrongProviderType );
        setErrorText( i18nc("@info/plain", "Not a GTFS provider") );
    }
}

UpdateGtfsToDatabaseJob::UpdateGtfsToDatabaseJob( const QString& destination,
        const QString &operation, const QMap< QString, QVariant > &parameters, QObject *parent )
        : ImportGtfsToDatabaseJob(destination, operation, parameters, parent)
{
}

ImportGtfsToDatabaseJob::~ImportGtfsToDatabaseJob()
{
    if ( m_importer ) {
        m_importer->quit();
        kDebug() << "Wait 10 second for the import thread to quit...";
        m_importer->wait( 10000 );
        delete m_importer;
    }
}

DeleteGtfsDatabaseJob::DeleteGtfsDatabaseJob( const QString &destination,
        const QString &operation, const QMap< QString, QVariant > &parameters, QObject *parent )
        : AbstractGtfsDatabaseJob(destination, operation, parameters, parent)
{
    m_serviceProviderId = parameters["serviceProviderId"].toString();
}

bool AbstractGtfsDatabaseJob::canImportGtfsFeed()
{
    GtfsService *service = qobject_cast< GtfsService* >( parent() );
    Plasma::DataEngine *engine = qobject_cast< Plasma::DataEngine* >( service->parent() );
    Q_ASSERT( engine );

    // Get the QMetaObject of the engine
    const QMetaObject *meta = engine->metaObject();

    // Find the slot of the engine to start the request
    const int slotIndex = meta->indexOfSlot( "tryToStartGtfsFeedImportJob(Plasma::ServiceJob*)" );
    Q_ASSERT( slotIndex != -1 );

    meta->method( slotIndex ).invoke( engine, Qt::DirectConnection,
                                      Q_RETURN_ARG(bool, m_canAccessGtfsDatabase),
                                      Q_ARG(const Plasma::ServiceJob*, this) );

    // If an import is already running for the provider, the operation cannot be executed now,
    // only updateGtfsFeedInfo can be executed while importing a GTFS feed
    if ( !m_canAccessGtfsDatabase ) {
        kWarning() << "The GTFS feed already gets imported";
        return false;
    }

    return true;
}

void AbstractGtfsDatabaseJob::start()
{
    QTimer::singleShot( 0, this, SLOT(tryToWork()) );
}

void AbstractGtfsDatabaseJob::tryToWork()
{
    if ( error() != NoGtfsError ) {
        kDebug() << error();
        // Error found in constructor, eg. no provider with the given ID found or no GTFS provider
        setResult( false );
        return;
    }

    if ( isAccessingGtfsDatabase() && !canImportGtfsFeed() ) {
        // Cannot start another GTFS database accessing job
        kDebug() << "Import is already running";
        setError( GtfsErrorFeedImportAlreadyRunning );
        setErrorText( i18nc("@info/plain", "The GTFS feed already gets imported.") );
        setResult( false );
        return;
    }

    // No problems, do the work now
    work();
}

void ImportGtfsToDatabaseJob::registerAtJobTracker()
{
    KIO::getJobTracker()->registerJob( this );
    emitDescription();
}

void ImportGtfsToDatabaseJob::emitDescription()
{
    if ( error() != NoGtfsError || !data() ) {
        kDebug() << error();
        // Error found in constructor, eg. no provider with the given ID found or no GTFS provider
        emit description( this, errorString() );
        return;
    }

    // Emit a description about what's done in this job
    const QPair< QString, QString > field1 =
            qMakePair( i18nc("@info/plain Label for GTFS service provider", "Service Provider"),
                       data()->name() );
    const QPair< QString, QString > field2 =
            qMakePair( i18nc("@info/plain Label for GTFS feed source URLs", "Source"),
                       m_data->feedUrl() );
    if ( m_onlyGetInformation ) {
        emit description( this, i18nc("@info", "Update GTFS feed info"), field1, field2 );
    } else {
        emit description( this, i18nc("@info", "Import GTFS feed"), field1, field2 );
    }
}

void UpdateGtfsToDatabaseJob::emitDescription()
{
    if ( error() != NoGtfsError || !data() ) {
        kDebug() << error();
        // Error found in constructor, eg. no provider with the given ID found or no GTFS provider
        emit description( this, errorString() );
        return;
    }

    emit description( this, i18nc("@info", "Updating GTFS feed"),
                      qMakePair(i18nc("@info/plain Label for GTFS service provider",
                                      "Service Provider"), data()->name()),
                      qMakePair(i18nc("@info/plain Label for GTFS feed source URLs", 
                                      "Source"), data()->feedUrl()) );
}

void ImportGtfsToDatabaseJob::work()
{
    Q_ASSERT( m_data );

    emitDescription();

    // Start the job by first requesting GTFS feed information
    statFeed();
}

void UpdateGtfsToDatabaseJob::tryToWork()
{
    QString errorMessage;
    if ( ServiceProviderGtfs::isGtfsFeedImportFinished(data()->id(), data()->feedUrl(),
                ServiceProviderGlobal::cache(), &errorMessage) )
    {
        AbstractGtfsDatabaseJob::tryToWork();
    } else {
        setError( GtfsErrorFeedImportRequired );
        setErrorText( errorMessage );
        setResult( false );
    }
}

void UpdateGtfsToDatabaseJob::work()
{
    QString errorMessage;
    if ( ServiceProviderGtfs::isGtfsFeedImportFinished(data()->id(), data()->feedUrl(),
                ServiceProviderGlobal::cache(), &errorMessage) )
    {
        // Emit a description about what's done in this job
        emitDescription();
        setCapabilities( Suspendable | Killable );

        // Start the job by first requesting GTFS feed information
        statFeed();
    } else {
        setError( GtfsErrorFeedImportRequired );
        setErrorText( errorMessage );
        setResult( false );
    }
}

void DeleteGtfsDatabaseJob::work()
{
    // Close the database before deleting it,
    // otherwise a newly created database won't get opened,
    // because the already opened database connection gets used instead
    GtfsDatabase::closeDatabase( m_serviceProviderId );

    // Delete the database file
    const QString databasePath = GtfsDatabase::databasePath( m_serviceProviderId );
    if ( !QFile::remove(databasePath) ) {
        kDebug() << "Failed to delete GTFS database";
        setError( GtfsErrorCannotDeleteDatabase );
        setErrorText( i18nc("@info/plain", "The GTFS database could not be deleted.") );
        setResult( false );
        return;
    }
    kDebug() << "Finished deleting GTFS database of" << m_serviceProviderId;

    // Update the accessor cache file to indicate that the GTFS feed needs to be imported again
    KConfig config( ServiceProviderGlobal::cacheFileName(), KConfig::SimpleConfig );
    KConfigGroup group = config.group( m_serviceProviderId );
    KConfigGroup gtfsGroup = group.group( "gtfs" );
    gtfsGroup.writeEntry( "feedImportFinished", false );

    // Write to disk now, important for the data engine to get the correct state
    // directly after this job has finished
    gtfsGroup.sync();

    // Finished successfully
    setResult( true );
}

bool ImportGtfsToDatabaseJob::doKill()
{
    if ( m_state == ReadingFeed ) {
        m_importer->quit();
    }
    m_state = KillingJob;
    return true;
}

bool ImportGtfsToDatabaseJob::doSuspend()
{
    if ( m_state == ReadingFeed ) {
        m_importer->suspend();
    }
    return true;
}

bool ImportGtfsToDatabaseJob::doResume()
{
    if ( m_state == ReadingFeed ) {
        m_importer->resume();
    }
    return true;
}

void ImportGtfsToDatabaseJob::statFeed()
{
    if ( m_state == DownloadingFeed || m_state == ReadingFeed || m_state == StatingFeed ) {
        kDebug() << "Feed already gets downloaded / was downloaded and gets imported / gets stated";
        return;
    }

    if ( !m_data ) {
        // There was an error in the constructor, error already set
        setResult( false );
        return;
    }

    kDebug() << "Request GTFS feed information for" << m_data->id();
    emit infoMessage( this, i18nc("@info/plain", "Checking GTFS feed source") );
    m_state = StatingFeed;
    QNetworkAccessManager *manager = new QNetworkAccessManager( this );
    connect( manager, SIGNAL(finished(QNetworkReply*)),
             this, SLOT(statFeedFinished(QNetworkReply*)) );
    QNetworkRequest request( m_data->feedUrl() );
    manager->head( request );
}

void ImportGtfsToDatabaseJob::statFeedFinished( QNetworkReply *reply )
{
    if ( m_state == KillingJob || isSuspended() ) {
        return;
    }

    // Check if there is a valid redirection
    QString redirectUrl = reply->attribute( QNetworkRequest::RedirectionTargetAttribute ).toString();
    if ( reply->error() != QNetworkReply::NoError &&
         reply->header(QNetworkRequest::ContentLengthHeader).toInt() == 0 &&
         reply->url() != m_lastRedirectUrl )
    {
        // Headers were requested, empty result
        // Now download completely, using get instead of head
        kDebug() << "Possible redirection, requesting headers lead to an error reply" << reply->url();
        m_lastRedirectUrl = reply->url().toString();
        reply->manager()->get( QNetworkRequest(reply->url()) );
        reply->deleteLater();
        return;
    } else if( !redirectUrl.isEmpty() && redirectUrl != m_lastRedirectUrl ) {
        // Redirect to redirectUrl, store last redirection
        kDebug() << "Redirecting to" << redirectUrl;
        m_lastRedirectUrl = redirectUrl;
        reply->manager()->head( QNetworkRequest(redirectUrl) );
        reply->deleteLater();
        return;
    }

    // Not redirected anymore
    m_lastRedirectUrl.clear();

    if ( reply->error() == QNetworkReply::NoError ) {
        QString contentType = reply->header( QNetworkRequest::ContentTypeHeader ).toString();
        // If ';' is not included in contentType, QString::left() returns the whole string
        const KMimeType::Ptr mimeType =
                KMimeType::mimeType( contentType.left(contentType.indexOf(';')) );
        if ( mimeType && mimeType->isValid() ) {
            if ( !mimeType->is("application/zip") && !mimeType->is("application/octet-stream") ) {
                kDebug() << "Invalid mime type:" << reply->header(QNetworkRequest::ContentTypeHeader).toString();
                setError( GtfsErrorWrongFeedFormat );
                setErrorText( i18nc("@info/plain", "Wrong GTFS feed format: %1", mimeType->name()) );
                setResult( false );
                return;
            }
        } else {
            kDebug() << "Could not create KMimeType object for" << reply->header(QNetworkRequest::ContentTypeHeader).toString();
        }

        // Use KDateTime and UTC time to not get confused with different locales
        KDateTime newLastModified = KDateTime::fromString(
                reply->header(QNetworkRequest::LastModifiedHeader).toString() ).toUtc();
        qulonglong newSizeInBytes = reply->header( QNetworkRequest::ContentLengthHeader ).toULongLong();
//         emit size and lastModified?
//         qDebug() << ">>>>> GTFS feed was last modified (UTC):" << newLastModified
//                  << "and it's size is:" << (newSizeInBytes/qreal(1024*1024)) << "MB";
        // Read accessor information cache
        QSharedPointer<KConfig> cache = ServiceProviderGlobal::cache();
        QString errorMessage;
        const bool importFinished = ServiceProviderGtfs::isGtfsFeedImportFinished(
                data()->id(), data()->feedUrl(), cache, &errorMessage );
        KConfigGroup group = cache->group( data()->id() );
        KConfigGroup gtfsGroup = group.group( "gtfs" );
        KDateTime lastModified = KDateTime::fromString(
                gtfsGroup.readEntry("feedModifiedTime", QString()) );
        qulonglong sizeInBytes = gtfsGroup.readEntry( "feedSizeInBytes", qulonglong(-1) );

        gtfsGroup.writeEntry( "feedModifiedTime", newLastModified.toString() );
        gtfsGroup.writeEntry( "feedSizeInBytes", newSizeInBytes );
        gtfsGroup.writeEntry( "feedUrl", data()->feedUrl() );

        // Needed to have the GTFS feed information available directly after this job is finished
        gtfsGroup.sync();

        // Stop here for "updateGtfsFeedInfo" operation
        if ( m_onlyGetInformation ) {
            m_state = Ready;
            setResult( importFinished );
            return;
        }

        if ( !importFinished ) {
            qDebug() << errorMessage << m_data->id();
        }
//         qDebug() << "Old last modified:" << lastModified
//                  << "new size:" << (sizeInBytes/qreal(1024*1024)) << "MB";
        if ( !importFinished // GTFS import not finished or never started?
             || (newLastModified.isValid() && lastModified.isValid() && // GTFS feed modified?
                 newLastModified != lastModified)
             || (newSizeInBytes > 0 && newSizeInBytes != sizeInBytes) // Size changed?
             || (newSizeInBytes == 0 && !newLastModified.isValid() && // If both are not available...
                 lastModified.daysTo(KDateTime::currentUtcDateTime()) > 7) ) // ...upate weekly
        {
            kDebug() << "Download new GTFS feed version for" << m_data->id();

            // A newer GTFS feed is available or it was not imported / import did not finish
            m_state = Initializing;
            downloadFeed();
        } else {
            // Newest version of the GTFS feed is already downloaded and completely imported
            m_state = Ready;
            setResult( true );
        }
    } else {
        // Track this job to show the error
        registerAtJobTracker();

        kDebug() << "GTFS feed not available: " << m_data->feedUrl() << reply->errorString();
        m_state = ErrorDownloadingFeed;
        setError( GtfsErrorDownloadFailed );
        setErrorText( reply->errorString() ); // TODO
        setResult( false );
    }

    reply->manager()->deleteLater();
    reply->deleteLater();
}

void ImportGtfsToDatabaseJob::slotSpeed( KJob *job, ulong speed )
{
    Q_UNUSED( job );
    emitSpeed( speed );
}

void ImportGtfsToDatabaseJob::downloadFeed()
{
    if ( m_state == DownloadingFeed || m_state == ReadingFeed || m_state == StatingFeed ) {
        kDebug() << "Feed already gets downloaded / was downloaded and gets imported / gets stated";
        return;
    }
    if ( m_state == KillingJob || isSuspended() ) {
        return;
    }

    // Track this job at least from now on,
    // because the download/import can take some time
    registerAtJobTracker();

    kDebug() << "Start GTFS feed import for" << m_data->id();
    KTemporaryFile tmpFile;
    if ( tmpFile.open() ) {
        kDebug() << "Downloading GTFS feed from" << m_data->feedUrl() << "to" << tmpFile.fileName();
        emit infoMessage( this, i18nc("@info/plain", "Downloading GTFS feed") );
        m_state = DownloadingFeed;
        tmpFile.setAutoRemove( false ); // Do not remove the target file while downloading

        // Set progress to 0
        m_progress = 0.0;
        emitPercent( 0, 1000 );

//         KFileItem gtfsFeed( m_info->feedUrl(), QString(), S_IFREG | S_IFSOCK );
//         kDebug() << "LAST MODIFIED:" << gtfsFeed.timeString( KFileItem::ModificationTime );

        // Update provider cache
        KConfig config( ServiceProviderGlobal::cacheFileName(), KConfig::SimpleConfig );
        KConfigGroup group = config.group( data()->id() );
        KConfigGroup gtfsGroup = group.group( "gtfs" );
        gtfsGroup.writeEntry( "feedImportFinished", false );

        KIO::FileCopyJob *job = KIO::file_copy( m_data->feedUrl(), KUrl(tmpFile.fileName()), -1,
                                                KIO::Overwrite | KIO::HideProgressInfo );
        connect( job, SIGNAL(result(KJob*)), this, SLOT(feedReceived(KJob*)) );
        connect( job, SIGNAL(percent(KJob*,ulong)), this, SLOT(downloadProgress(KJob*,ulong)) );
        connect( job, SIGNAL(mimetype(KIO::Job*,QString)), this, SLOT(mimeType(KIO::Job*,QString)) );
        connect( job, SIGNAL(totalSize(KJob*,qulonglong)), this, SLOT(totalSize(KJob*,qulonglong)) );
        connect( job, SIGNAL(speed(KJob*,ulong)), this, SLOT(slotSpeed(KJob*,ulong)) );
    } else {
        kDebug() << "Could not create a temporary file to download the GTFS feed";
//         TODO emit error...
    }
}

void ImportGtfsToDatabaseJob::mimeType( KIO::Job *job, const QString &type )
{
    Q_UNUSED( job );
    const KMimeType::Ptr mimeType = KMimeType::mimeType( type );
    if ( mimeType && mimeType->isValid() ) {
        if ( !mimeType->is("application/zip") && !mimeType->is("application/octet-stream") ) {
            kDebug() << "Invalid mime type:" << type;
            setError( GtfsErrorWrongFeedFormat );
            setErrorText( i18nc("@info/plain", "Wrong GTFS feed format: %1", mimeType->name()) );
            setResult( false );
            return;
        }
    } else {
        kDebug() << "Could not create KMimeType object for" << type;
    }
}

void ImportGtfsToDatabaseJob::totalSize( KJob* job, qulonglong size )
{
    Q_UNUSED( job );
    KConfig config( ServiceProviderGlobal::cacheFileName(), KConfig::SimpleConfig );
    KConfigGroup group = config.group( data()->id() );
    KConfigGroup gtfsGroup = group.group( "gtfs" );
    gtfsGroup.writeEntry( "feedSizeInBytes", size );
}

void ImportGtfsToDatabaseJob::downloadProgress( KJob *job, ulong percent )
{
    Q_UNUSED( job );
    m_progress = (qreal(percent) / 100.0) * PROGRESS_PART_FOR_FEED_DOWNLOAD; // TODO Remove m_progress
    emitPercent( percent * 10 * PROGRESS_PART_FOR_FEED_DOWNLOAD, 1000 );
}

void ImportGtfsToDatabaseJob::feedReceived( KJob *job )
{
    if ( m_state == KillingJob || isSuspended() ) {
        return;
    }

    // Emit progress for finished download
    m_progress = PROGRESS_PART_FOR_FEED_DOWNLOAD;
    emitPercent( 1000 * PROGRESS_PART_FOR_FEED_DOWNLOAD, 1000 );

    KIO::FileCopyJob *fileCopyJob = qobject_cast<KIO::FileCopyJob*>( job );
    QString tmpFilePath = fileCopyJob->destUrl().path();
    if ( job->error() != 0 ) {
        kDebug() << "Error downloading GTFS feed:" << job->errorString();
        emit infoMessage( this, i18nc("@info/plain", "Error downloading GTFS feed: "
                          "<message>%1</message>", job->errorString()) );
        m_state = ErrorDownloadingFeed;
        if ( !QFile::remove(tmpFilePath) ) {
            kDebug() << "Could not remove the temporary GTFS feed file";
        }

        setError( GtfsErrorDownloadFailed );
        setErrorText( job->errorString() ); // TODO
        setResult( false );
        return;
    }

    kDebug() << "GTFS feed received at" << tmpFilePath;

    // Read feed and write data into the DB
    m_state = ReadingFeed;
    emit infoMessage( this, i18nc("@info/plain", "Importing GTFS feed") );
    m_importer = new GtfsImporter( m_data->id() );
    connect( m_importer, SIGNAL(logMessage(QString)), this, SLOT(logMessage(QString)) );
    connect( m_importer, SIGNAL(progress(qreal,QString)),
             this, SLOT(importerProgress(qreal,QString)) );
    connect( m_importer, SIGNAL(finished(GtfsImporter::State,QString)),
             this, SLOT(importerFinished(GtfsImporter::State,QString)) );
    m_importer->startImport( tmpFilePath );
}

void ImportGtfsToDatabaseJob::logMessage( const QString &message )
{
    emit warning( this, message );
}

void ImportGtfsToDatabaseJob::importerProgress( qreal importerProgress,
                                                const QString &currentTableName )
{
    m_progress = PROGRESS_PART_FOR_FEED_DOWNLOAD * (1.0 - importerProgress) + importerProgress;
    emitPercent( m_progress * 1000, 1000 );

    if ( currentTableName != m_lastTableName ) {
        emit infoMessage( this, i18nc("@info/plain", "Importing GTFS feed (%1)",
                                      currentTableName) );
        m_lastTableName = currentTableName;
    }
}

void ImportGtfsToDatabaseJob::importerFinished(
        GtfsImporter::State state, const QString &errorText )
{
    // Remove temporary file
    if ( m_importer && !QFile::remove(m_importer->sourceFileName()) ) {
        kWarning() << "Could not remove the temporary GTFS feed file";
    }

    // Update 'feedImportFinished' field in the cache
    KConfig config( ServiceProviderGlobal::cacheFileName(), KConfig::SimpleConfig );
    KConfigGroup group = config.group( data()->id() );
    KConfigGroup gtfsGroup = group.group( "gtfs" );
    gtfsGroup.writeEntry( "feedImportFinished", state != GtfsImporter::FatalError );

    // Write to disk now, important for the data engine to get the correct state
    // directly after this job has finished
    gtfsGroup.sync();

    // Emit progress with 1.0, ie. finsihed
    m_progress = 1.0;
    emitPercent( 1000, 1000 );
    kDebug() << "Finished" << state << errorText;

    // Ignore GtfsImporter::FinishedWithErrors
    if ( state == GtfsImporter::FatalError ) {
        m_state = ErrorReadingFeed;
        kDebug() << "There was an error importing the GTFS feed into the database" << errorText;
        emit infoMessage( this, errorText );
    } else {
        m_state = Ready;
        emit infoMessage( this, i18nc("@info/plain", "GTFS feed has been successfully imported") );
    }

    if ( m_importer ) {
        m_importer->quit();
        m_importer->wait();
        delete m_importer;
        m_importer = 0;
    }

    if ( m_state == Ready ) {
        // Update accessor information cache
        setResult( true );
    } else {
        setError( GtfsErrorImportFailed );
        setErrorText( errorText );
        setResult( false );
    }
}

QString ImportGtfsToDatabaseJob::serviceProviderId() const
{
    return m_data ? m_data->id() : QString();
}

GtfsService::GtfsService( const QString &name, QObject *parent )
        : Service(parent), m_name(name)
{
    // This associates the service with the "publictransport.operations" file
    setName( "publictransport" );
}

Plasma::ServiceJob* GtfsService::createJob(
        const QString &operation, QMap< QString, QVariant > &parameters )
{
    // Check if a valid provider ID is available in the parameters
    const QString providerId = parameters["serviceProviderId"].toString();
    if ( providerId.isEmpty() ) {
        kWarning() << "No 'serviceProviderId' parameter given to GTFS service operation";
        return 0;
    }

    if ( operation == QLatin1String("updateGtfsDatabase") ) {
        UpdateGtfsToDatabaseJob *updateJob =
                new UpdateGtfsToDatabaseJob( "PublicTransport", operation, parameters, this );
        return updateJob;
    } else if ( operation == QLatin1String("importGtfsFeed") ) {
        ImportGtfsToDatabaseJob *importJob =
                new ImportGtfsToDatabaseJob( "PublicTransport", operation, parameters, this );
        // Directly register import jobs, ie. also show "Check Feed Source"
        importJob->registerAtJobTracker();
        return importJob;
    } else if ( operation == QLatin1String("deleteGtfsDatabase") ) {
        DeleteGtfsDatabaseJob *deleteJob =
                new DeleteGtfsDatabaseJob( "PublicTransport", operation, parameters, this );
        return deleteJob;
    } else if ( operation == QLatin1String("updateGtfsFeedInfo") ) {
        ImportGtfsToDatabaseJob *updateFeedInfoJob =
                new ImportGtfsToDatabaseJob( "PublicTransport", operation, parameters, this );
        updateFeedInfoJob->setOnlyGetInformation( true );
        return updateFeedInfoJob;
    } else {
        kWarning() << "Operation" << operation << "not supported";
        return 0;
    }
}
