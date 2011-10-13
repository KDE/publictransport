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

#include "publictransportservice.h"
#include "timetableaccessor.h"

// KDE includes
#include <KTemporaryFile>
#include <KMimeType>
#include <KDebug>
#include <KFileItem>

// Qt includes
#include <QNetworkAccessManager>
#include <QNetworkReply>

ImportGtfsToDatabaseJob::ImportGtfsToDatabaseJob( const QString &destination,
        const QString &operation, const QMap< QString, QVariant > &parameters, QObject *parent )
        : ServiceJob(destination, operation, parameters, parent), m_info(0), m_importer(0)
{
    m_info = TimetableAccessor::readAccessorInfo( parameters["serviceProviderId"].toString() );
    if ( !m_info ) {
        setError( -1 );
        setErrorText( i18nc("@info/plain", "Error while reading Accessor XML.") );
        return;
    }

    if ( m_info->accessorType() != GtfsAccessor ) {
        setError( -2 );
        setErrorText( i18nc("@info/plain", "Not a GTFS accessor") );
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
        : ServiceJob(destination, operation, parameters, parent)
{
    m_serviceProviderId = parameters["serviceProviderId"].toString();
}

void ImportGtfsToDatabaseJob::start()
{
    kDebug() << "Start import for" << m_info->serviceProvider();
//     downloadFeed(); // TODO Check datacache?
    statFeed();
}

void UpdateGtfsToDatabaseJob::start()
{
    KConfig cfg( TimetableAccessor::accessorCacheFileName(), KConfig::SimpleConfig );
    KConfigGroup grp = cfg.group( info()->serviceProvider() );
    bool importFinished = grp.readEntry( "feedImportFinished", false );
    if ( !importFinished ) {
        setError( -7 );
        setErrorText( i18nc("@info/plain", "GTFS feed not imported. Please import it explicitly first.") );
        emitResult();
    } else {
        ImportGtfsToDatabaseJob::start();
    }
}

void DeleteGtfsDatabaseJob::start()
{
    const QString databasePath = GeneralTransitFeedDatabase::databasePath( m_serviceProviderId );
    if ( !QFile::remove(databasePath) ) {
        setError( -1 );
        setErrorText( i18nc("@info/plain", "The GTFS database could not be deleted.") );
        kDebug() << "Failed to delete GTFS database";
    }
    kDebug() << "Finished deleting GTFS database";

    // Update the accessor cache file to indicate that the GTFS feed needs to be imported again
    KConfig cfg( TimetableAccessor::accessorCacheFileName(), KConfig::SimpleConfig );
    KConfigGroup grp = cfg.group( m_serviceProviderId );
    grp.writeEntry( "feedImportFinished", false );

    emitResult();
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

    if ( !m_info ) {
        // There was an error in the constructor, error already set
        emitResult();
        return;
    }

    kDebug() << "************* STARTING STAT FOR *****" << m_info->serviceProvider() << "*********";
    m_state = StatingFeed;
    QNetworkAccessManager *manager = new QNetworkAccessManager( this );
    connect( manager, SIGNAL(finished(QNetworkReply*)),
             this, SLOT(statFeedFinished(QNetworkReply*)) );
    QNetworkRequest request( m_info->feedUrl() );
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
        if ( mimeType && !mimeType->name().isEmpty() ) {
            if ( mimeType->name() != "application/zip" ) {
                kDebug() << "Invalid mime type:" << reply->header(QNetworkRequest::ContentTypeHeader).toString();
                setError( -3 );
                setErrorText( i18nc("@info/plain", "Wrong GTFS feed format: %1", mimeType->name()) ); // TODO
                emitResult();
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
        KConfig cfg( TimetableAccessor::accessorCacheFileName(), KConfig::SimpleConfig );
        KConfigGroup grp = cfg.group( m_info->serviceProvider() );
        bool importFinished = grp.readEntry( "feedImportFinished", false );
        KDateTime lastModified = KDateTime::fromString( grp.readEntry("feedLastModified", QString()) );
        qulonglong sizeInBytes = grp.readEntry( "feedSizeInBytes", qulonglong(-1) );

        grp.writeEntry( "feedLastModified", newLastModified.toString() );
        grp.writeEntry( "feedSizeInBytes", newSizeInBytes );

        if ( !importFinished ) {
            qDebug() << "Last GTFS feed import did not finish for" << m_info->serviceProvider();
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
            kDebug() << "Download new GTFS feed version for" << m_info->serviceProvider();

            // A newer GTFS feed is available or it was not imported / import did not finish
            m_state = Initializing;
            downloadFeed();
        } else {
            // Newest version of the GTFS feed is already downloaded and completely imported
            m_state = Ready;
            emitResult();
        }
    } else {
        kDebug() << "GTFS feed not available: " << m_info->feedUrl() << reply->errorString();
        m_state = ErrorDownloadingFeed;
        setError( -4 );
        setErrorText( reply->errorString() ); // TODO
        emitResult();
    }

    reply->manager()->deleteLater();
    reply->deleteLater();
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

    KTemporaryFile tmpFile;
    if ( tmpFile.open() ) {
        kDebug() << "Downloading GTFS feed from" << m_info->feedUrl() << "to" << tmpFile.fileName();
        m_state = DownloadingFeed;
        tmpFile.setAutoRemove( false ); // Do not remove the target file while downloading

        // Set progress to 0
        m_progress = 0.0;
        emitPercent( 0, 1000 );

//         KFileItem gtfsFeed( m_info->feedUrl(), QString(), S_IFREG | S_IFSOCK );
//         kDebug() << "LAST MODIFIED:" << gtfsFeed.timeString( KFileItem::ModificationTime );

        // Update accessor information cache
        const QString fileName = TimetableAccessor::accessorCacheFileName();
        const bool cacheExists = QFile::exists( fileName );
        KConfig cfg( fileName, KConfig::SimpleConfig );
        KConfigGroup grp = cfg.group( m_info->serviceProvider() );
        grp.writeEntry( "feedImportFinished", false );

        KIO::FileCopyJob *job = KIO::file_copy( m_info->feedUrl(), KUrl(tmpFile.fileName()), -1,
                                                KIO::Overwrite | KIO::HideProgressInfo );
        connect( job, SIGNAL(result(KJob*)), this, SLOT(feedReceived(KJob*)) );
        connect( job, SIGNAL(percent(KJob*,ulong)), this, SLOT(downloadProgress(KJob*,ulong)) );
        connect( job, SIGNAL(mimetype(KIO::Job*,QString)), this, SLOT(mimeType(KIO::Job*,QString)) );
        connect( job, SIGNAL(totalSize(KJob*,qulonglong)), this, SLOT(totalSize(KJob*,qulonglong)) );
    } else {
        kDebug() << "Could not create a temporary file to download the GTFS feed";
//         TODO emit error...
    }
}

void ImportGtfsToDatabaseJob::mimeType( KIO::Job *job, const QString &type )
{
    if ( !type.endsWith(QLatin1String("zip")) && !type.endsWith(QLatin1String("zip-compressed")) ) {
        job->kill();
        setError( -10 );
        setErrorText( "GTFS feed in wrong format: " + type /*TODO i18nc("@info/plain", "")*/ );
        emitResult();
    }
}

void ImportGtfsToDatabaseJob::totalSize( KJob* job, qulonglong size )
{
    KConfig cfg( TimetableAccessor::accessorCacheFileName(), KConfig::SimpleConfig );
    KConfigGroup grp = cfg.group( m_info->serviceProvider() );
//     qulonglong sizeInBytes = grp.readEntry( "feedSizeInBytes", qulonglong(-1) );
//     if ( size == sizeInBytes ) {
//         // The file that is currently downloaded by job did not change in size
//         kDebug() << "Size of the GTFS feed did not change, don't update";
//         job->kill();
//         emitResult();
//     } else {
        grp.writeEntry( "feedSizeInBytes", size );
//     }
}

void ImportGtfsToDatabaseJob::downloadProgress( KJob *job, ulong percent )
{
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
        m_state = ErrorDownloadingFeed;
        if ( !QFile::remove(tmpFilePath) ) {
            kDebug() << "Could not remove the temporary GTFS feed file";
        }

        setError( -5 );
        setErrorText( job->errorString() ); // TODO
        emitResult();
        return;
    }

    kDebug() << "GTFS feed received at" << tmpFilePath;

    // Read feed and write data into the DB
    m_state = ReadingFeed;
    m_importer = new GeneralTransitFeedImporter( m_info->serviceProvider() );
    connect( m_importer, SIGNAL(progress(qreal)), this, SLOT(importerProgress(qreal)) );
    connect( m_importer, SIGNAL(finished(GeneralTransitFeedImporter::State,QString)),
             this, SLOT(importerFinished(GeneralTransitFeedImporter::State,QString)) );
    m_importer->startImport( tmpFilePath );
}

void ImportGtfsToDatabaseJob::importerProgress( qreal importerProgress )
{
    m_progress = PROGRESS_PART_FOR_FEED_DOWNLOAD * (1.0 - importerProgress) + importerProgress;
    emitPercent( m_progress * 1000, 1000 );
}

void ImportGtfsToDatabaseJob::importerFinished(
        GeneralTransitFeedImporter::State state, const QString &errorText )
{
    // Emit progress with 1.0, ie. finsihed
    m_progress = 1.0;
    emitPercent( 1000, 1000 );
    kDebug() << "FINISHED";

    // Remove temporary file
    if ( m_importer && !QFile::remove(m_importer->sourceFileName()) ) {
        kDebug() << "Could not remove the temporary GTFS feed file";
    }

    // Ignore GeneralTransitFeedImporter::FinishedWithErrors
    if ( state == GeneralTransitFeedImporter::FatalError ) {
        m_state = ErrorReadingFeed;
        kDebug() << "There was an error importing the GTFS feed into the database" << errorText;
    } else {
        m_state = Ready;
    }

    if ( m_importer ) {
        m_importer->quit();
        m_importer->wait();
        delete m_importer;
        m_importer = 0;
    }

    if ( m_state == Ready ) {
        // Update accessor information cache
//         const bool cacheExists = QFile::exists( fileName );
        KConfig cfg( TimetableAccessor::accessorCacheFileName(), KConfig::SimpleConfig );
        KConfigGroup grp = cfg.group( m_info->serviceProvider() );
        grp.writeEntry( "feedImportFinished", true );
    } else {
        setError( -6 );
        setErrorText( errorText ); // TODO
    }

    emitResult();
}

PublicTransportService::PublicTransportService( const QString &name, QObject *parent )
        : Service(parent), m_name(name)
{
    // This associates the service with the "publictransport.operations" file
    setName( "publictransport" );
}

Plasma::ServiceJob* PublicTransportService::createJob(
        const QString &operation, QMap< QString, QVariant > &parameters )
{
    if ( operation == "updateGtfsFeed" ) {
        return new UpdateGtfsToDatabaseJob( "PublicTransport", operation, parameters, this );
    } else if ( operation == "importGtfsFeed" ) {
        return new ImportGtfsToDatabaseJob( "PublicTransport", operation, parameters, this );
    } else if ( operation == "deleteGtfsDatabase" ) {
        return new DeleteGtfsDatabaseJob( "PublicTransport", operation, parameters, this );
    } else {
        kWarning() << "Operation" << operation << "not supported";
        return 0;
    }
}
