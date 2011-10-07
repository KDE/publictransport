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

class ImportGtfsToDatabaseJob : public Plasma::ServiceJob {
    Q_OBJECT

public:
    ImportGtfsToDatabaseJob( const QString &destination, const QString &operation,
                             const QMap< QString, QVariant > &parameters, QObject *parent = 0 );
    virtual ~ImportGtfsToDatabaseJob();

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

class UpdateGtfsToDatabaseJob : public ImportGtfsToDatabaseJob {
    Q_OBJECT

public:
    UpdateGtfsToDatabaseJob( const QString &destination, const QString &operation,
                             const QMap< QString, QVariant > &parameters, QObject *parent = 0 );

    virtual void start();
};

class PublicTransportService : public Plasma::Service {
    Q_OBJECT

public:
    explicit PublicTransportService( const QString &name, QObject *parent = 0 );

protected:
    virtual Plasma::ServiceJob* createJob( const QString &operation,
                                           QMap< QString, QVariant > &parameters );

private:
    QString m_name;
};

#endif // Multiple inclusion guard
