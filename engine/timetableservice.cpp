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

// Own includes
#include "timetableservice.h"

// KDE includes
#include <Plasma/DataEngine>
#include <KDebug>

// Qt incluse
#include <QMetaMethod>

RequestAdditionalDataJob::RequestAdditionalDataJob( const QString &destination,
        const QString &operation, const QMap< QString, QVariant > &parameters, QObject *parent )
        : ServiceJob(destination, operation, parameters, parent), m_itemsDone(0)
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
    TimetableService *service = qobject_cast< TimetableService* >( parent() );
    Plasma::DataEngine *engine = qobject_cast< Plasma::DataEngine* >( service->parent() );
    Q_ASSERT( engine );

    // Get the QMetaObject of the engine
    const QMetaObject *meta = engine->metaObject();

    // Find the slot of the engine to start the request
    const int slotIndex = meta->indexOfSlot( "requestAdditionalData(QString,int)" );
    Q_ASSERT( slotIndex != -1 );

    // Find the signal of the engine which gets emitted when the request has finished
    connect( engine, SIGNAL(additionalDataRequestFinished(QVariantHash,bool,QString)),
             this, SLOT(additionalDataRequestFinished(QVariantHash,bool,QString)) );

    // Invoke the slot to request additional data
    for ( int item = m_updateItem; item <= m_updateItemEnd; ++item ) {
        meta->method( slotIndex ).invoke( engine, Qt::QueuedConnection,
                                          Q_ARG(QString, destination()), Q_ARG(int, item) );
    }
}

UpdateRequestJob::UpdateRequestJob( const QString &destination, const QString &operation,
                                    const QMap< QString, QVariant > &parameters, QObject *parent )
        : ServiceJob( destination, operation, parameters, parent )
{
}

void UpdateRequestJob::start()
{
    TimetableService *service = qobject_cast< TimetableService* >( parent() );
    Plasma::DataEngine *engine = qobject_cast< Plasma::DataEngine* >( service->parent() );
    Q_ASSERT( engine );

    // Get the QMetaObject of the engine
    const QMetaObject *meta = engine->metaObject();

    // Find the slot of the engine to start the request
    const int slotIndex = meta->indexOfMethod( "requestUpdate(QString,QString*)" );
    Q_ASSERT( slotIndex != -1 );

    bool success;
    QString errorMessage;
    meta->method( slotIndex ).invoke( engine, Qt::DirectConnection,
                                      Q_RETURN_ARG(bool, success),
                                      Q_ARG(QString, destination()),
                                      Q_ARG(QString*, &errorMessage) );
    setResult( success );
    if ( !success ) {
        setError( TimetableService::UnknownError );
        setErrorText( errorMessage );
    }
}

void RequestAdditionalDataJob::additionalDataRequestFinished( const QVariantHash &newData,
                                                              bool success,
                                                              const QString &errorMessage )
{
    Q_UNUSED( newData );
    if ( !success ) {
        setError( KJob::UserDefinedError );
        setErrorText( errorMessage );
    }
    ++m_itemsDone;
    const int totalItemsToUpdate = m_updateItemEnd - m_updateItem + 1;
    setPercent( m_itemsDone / totalItemsToUpdate );

    if ( m_itemsDone == totalItemsToUpdate ) {
        // Last item is done
        setResult( success );
    }
}

TimetableService::TimetableService( const QString &name, QObject *parent )
        : Service(parent), m_name(name)
{
    // This associates the service with the "timetable.operations" file
    setName( "timetable" );
}

Plasma::ServiceJob* TimetableService::createJob(
        const QString &operation, QMap< QString, QVariant > &parameters )
{
    if ( operation == "requestAdditionalData" || operation == "requestAdditionalDataRange" ) {
        return new RequestAdditionalDataJob( m_name, operation, parameters, this );
    } else if ( operation == "requestUpdate" ) {
        return new UpdateRequestJob( m_name, operation, parameters, this );
    } else {
        kWarning() << "Operation" << operation << "not supported";
        return 0;
    }
}
