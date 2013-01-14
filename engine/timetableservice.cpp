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
#include "timetableservice.h"

// KDE includes
#include <Plasma/DataEngine>
#include <KDebug>

// Qt incluse
#include <QMetaMethod>
#include <qmath.h>

RequestAdditionalDataJob::RequestAdditionalDataJob( Plasma::DataEngine *engine,
        const QString &destination, const QString &operation,
        const QMap< QString, QVariant > &parameters, QObject *parent )
        : TimetableServiceJob(engine, destination, operation, parameters, parent),
          m_itemsDone(0), m_itemsFailed(0)
{
    if ( parameters.contains("itemnumberbegin") && parameters.contains("itemnumberend") ) {
        m_updateItem = parameters["itemnumberbegin"].toInt();
        m_updateItemEnd = parameters["itemnumberend"].toInt();
        Q_ASSERT( m_updateItemEnd >= m_updateItem );
    } else {
        m_updateItem = m_updateItemEnd = parameters["itemnumber"].toInt();
    }
}

void RequestAdditionalDataJob::start()
{
    // Find the slot of the engine to start the request
    const QMetaObject *meta = m_engine->metaObject();
    const int slotIndex = meta->indexOfSlot( "requestAdditionalData(QString,int,int)" );
    Q_ASSERT( slotIndex != -1 );

    // Connect to the finished signal of the engine for finished additional data requests
    // NOTE This signal is emitted for each finished request, not for all requests,
    //   if there are multiple requests
    connect( m_engine, SIGNAL(additionalDataRequestFinished(QString,int,bool,QString)),
             this, SLOT(additionalDataRequestFinished(QString,int,bool,QString)) );

    // Invoke the slot to request additional data for each item
    meta->method( slotIndex ).invoke( m_engine, Qt::QueuedConnection,
                                      Q_ARG(QString, destination()),
                                      Q_ARG(int, m_updateItem),
                                      Q_ARG(int, m_updateItemEnd - m_updateItem + 1) );
}

void RequestAdditionalDataJob::additionalDataRequestFinished( const QString &sourceName,
                                                              int item, bool success,
                                                              const QString &errorMessage )
{
    if ( sourceName != destination() || item < m_updateItem || item > m_updateItemEnd ) {
        // The finished() signal from the data engine was emitted for another service job
        return;
    }

    ++m_itemsDone;
    if ( !success ) {
        ++m_itemsFailed;
        if ( m_errorMessage.isEmpty() ) {
            m_errorMessage = errorMessage;
        }
    }
    const int totalItemsToUpdate = m_updateItemEnd - m_updateItem + 1;
    const int itemsFailed = m_itemsFailed;
    const bool isFinished = m_itemsDone == totalItemsToUpdate;
    const long unsigned int percent = qCeil((100 * m_itemsDone) / qreal(totalItemsToUpdate));
    QString firstErrorMessage = m_errorMessage;
    Q_ASSERT( totalItemsToUpdate > 0 );

    // Set the current percentage of requests that are done
    setPercent( percent );

    if ( isFinished ) {
        // Last item is done set error message, if any
        if ( itemsFailed >= 1 ) {
            // Use a simple error message if multiple items were requested, otherwise use the error
            // message for the failed item. The error messages for all failed items are set in the
            // target data source in "additionalDataError" fields.
            if ( totalItemsToUpdate > 1 ) {
                firstErrorMessage = i18nc("@info/plain", "%1 of %2 items failed",
                                          itemsFailed, totalItemsToUpdate);
            }
            setError( KJob::UserDefinedError );
            setErrorText( firstErrorMessage );
        }

        // Set the result and emit finished(). This should only be done when the job is really
        // finished, it. there should be no more requests of multiple requests to be done for
        // this job. It will not crash in the service
        // NOTE The finished() signal may be connected to the deleteLater() slot of the service,
        // which is the parent of this job, ie. this may delete this job (later)
        setResult( success );
    }
}

UpdateRequestJob::UpdateRequestJob( Plasma::DataEngine *engine, const QString &destination,
        const QString &operation, const QMap< QString, QVariant > &parameters, QObject *parent )
        : TimetableServiceJob( engine, destination, operation, parameters, parent )
{
}

void UpdateRequestJob::start()
{
    // Find the slot of the engine to start the request
    const QMetaObject *meta = m_engine->metaObject();
    const int slotIndex = meta->indexOfMethod( "requestUpdate(QString)" );
    Q_ASSERT( slotIndex != -1 );

    // Connect to the finished signal of the engine for finished update requests
    connect( m_engine, SIGNAL(updateRequestFinished(QString,bool,QString)),
             this, SLOT(updateRequestFinished(QString,bool,QString)) );

    // Invoke the slot to request the update
    meta->method( slotIndex ).invoke( m_engine, Qt::QueuedConnection,
                                      Q_ARG(QString, destination()) );
}

void UpdateRequestJob::updateRequestFinished( const QString &sourceName, bool success,
                                              const QString &errorMessage)
{
    if ( sourceName != destination() ) {
        // The finished() signal from the data engine was emitted for another service job
        return;
    }

    setResult( success );
    if ( !success ) {
        setError( TimetableService::UnknownError );
        setErrorText( errorMessage );
    }
}

RequestMoreItemsJob::RequestMoreItemsJob( Plasma::DataEngine *engine,
        const QString &destination, Enums::MoreItemsDirection direction, const QString &operation,
        const QMap< QString, QVariant > &parameters, QObject *parent )
        : TimetableServiceJob(engine, destination, operation, parameters, parent),
          m_direction(direction)
{
    qRegisterMetaType< Enums::MoreItemsDirection >( "Enums::MoreItemsDirection" );
}

void RequestMoreItemsJob::start()
{
    // Find the slot of the engine to start the request
    const QMetaObject *meta = m_engine->metaObject();
    const int slotIndex = meta->indexOfMethod( "requestMoreItems(QString,Enums::MoreItemsDirection)" );
    Q_ASSERT( slotIndex != -1 );

    // Connect to the finished signal of the engine for finished requests for more items
    connect( m_engine, SIGNAL(moreItemsRequestFinished(QString,Enums::MoreItemsDirection,bool,QString)),
             this, SLOT(moreItemsRequestFinished(QString,Enums::MoreItemsDirection,bool,QString)) );

    // Invoke the slot to request more items
    meta->method( slotIndex ).invoke( m_engine, Qt::QueuedConnection,
                                      Q_ARG(QString, destination()),
                                      Q_ARG(Enums::MoreItemsDirection, m_direction) );
}

void RequestMoreItemsJob::moreItemsRequestFinished( const QString &sourceName,
        Enums::MoreItemsDirection direction, bool success, const QString &errorMessage)
{
    if ( sourceName != destination() || direction == m_direction ) {
        // The finished() signal from the data engine was emitted for another service job
        return;
    }

    if ( !success ) {
        setError( TimetableService::UnknownError );
        setErrorText( errorMessage );
    }
    setResult( success );
}

TimetableService::TimetableService( Plasma::DataEngine *engine, const QString &name,
                                    QObject *parent )
        : Service(parent), m_engine(engine)
{
    // This associates the service with the "timetable.operations" file
    setName( "timetable" );
    setDestination( name );
}

Plasma::ServiceJob* TimetableService::createJob(
        const QString &operation, QMap< QString, QVariant > &parameters )
{
    if ( operation == "requestAdditionalData" || operation == "requestAdditionalDataRange" ) {
        return new RequestAdditionalDataJob( m_engine, destination(), operation, parameters, this );
    } else if ( operation == "requestUpdate" ) {
        return new UpdateRequestJob( m_engine, destination(), operation, parameters, this );
    } else if ( operation == "requestEarlierItems" ) {
        return new RequestMoreItemsJob( m_engine, destination(), Enums::EarlierItems,
                                        operation, parameters, this );
    } else if ( operation == "requestLaterItems" ) {
        return new RequestMoreItemsJob( m_engine, destination(), Enums::LaterItems,
                                        operation, parameters, this );
    } else {
        kWarning() << "Operation" << operation << "not supported";
        return 0;
    }
}
