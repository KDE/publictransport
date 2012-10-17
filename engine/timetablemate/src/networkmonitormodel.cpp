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

// Header
#include "networkmonitormodel.h"

// Public Transport engine includes
#include <engine/global.h>

// KDE includes
#include <KLocalizedString>
#include <KDebug>
#include <KGlobal>
#include <KLocale>
#include <KUrl>
#include <KMimeType>
#include <KIO/NetAccess>
#include <KIO/Job>
#include <KIcon>
#include <KTemporaryFile>
#include <KGlobalSettings>

// Qt includes
#include <QNetworkRequest>
#include <QNetworkReply>
#include <qnetworkdiskcache.h>
#include <QTimer>
#include <QBuffer>
#include <QTextCodec>
#include <QImageReader>

NetworkMonitorModel::NetworkMonitorModel( QObject *parent ) : QAbstractListModel( parent )
{
}

NetworkMonitorModel::~NetworkMonitorModel()
{
    qDeleteAll( m_data );
}

bool NetworkMonitorModel::removeRows( int row, int count, const QModelIndex &parent )
{
    if ( parent.isValid() || row < 0 || row + count > m_data.count() ) {
        return false;
    }

    beginRemoveRows( parent, row, row + count - 1 );
    for ( int i = row + count - 1; i >= row; --i ) {
        delete m_data.takeAt( i );
    }
    endRemoveRows();
    return true;
}

int NetworkMonitorModel::rowCount( const QModelIndex &parent ) const
{
    if ( parent.isValid() ) {
        return 0;
    } else {
        return m_data.count();
    }
}

void NetworkMonitorModel::clear()
{
    beginRemoveRows( QModelIndex(), 0, rowCount() );
    m_data.clear();
    endRemoveRows();
}

QModelIndex NetworkMonitorModel::index( int row, int column, const QModelIndex &parent ) const
{
    if ( column < 0 || column >= ColumnCount || row < 0 ) {
        return QModelIndex();
    }

    if ( parent.isValid() ) {
        return QModelIndex();
    } else {
        return row >= m_data.count() ? QModelIndex() : createIndex(row, column, m_data[row]);
    }
}

QModelIndex NetworkMonitorModel::indexFromMonitorData( NetworkMonitorModelItem *data, int column ) const
{
    const int row = m_data.indexOf( data );
    if ( row == -1 ) {
        return QModelIndex();
    } else {
        return index( row, column );
    }
}

NetworkMonitorModelItem *NetworkMonitorModel::monitorDataFromIndex( const QModelIndex &index ) const
{
    return static_cast< NetworkMonitorModelItem* >( index.internalPointer() );
}

QString NetworkMonitorModel::encodeHtml( const QString &html )
{
    return QString(html).replace( "<", "&lt;" ).replace( ">", "&gt;" );
}

QString NetworkMonitorModel::limitLineCount( const QString &string, bool htmlFormat,
                                             int maxLineCount, int maxCharactersPerLine )
{
    QString result;
    QStringList lines = string.split( '\n' );
    foreach ( const QString &line, lines ) {
        if ( maxCharactersPerLine > 0 && line.length() > maxCharactersPerLine ) {
            QString currentLine = line;
            while ( !currentLine.isEmpty() ) {
                if ( !result.isEmpty() ) {
                    result += '\n';
                }
                result += currentLine.left( maxCharactersPerLine );
                currentLine = currentLine.mid( maxCharactersPerLine );
                if ( !currentLine.isEmpty() && !htmlFormat ) {
                    // Append a return symbol
                    result += QString::fromUtf8( "\342\217\216" );
                }
            }
        } else {
            if ( !result.isEmpty() ) {
                result += '\n';
            }
            result += line;
        }

        int lineCount = 0;
        int pos = -1;
        while ( (pos = result.indexOf('\n', pos + 1)) != -1 ) {
            ++lineCount;
            if ( lineCount >= maxLineCount ) {
                return result.left( pos ) + "...";
            }
        }
    }
    return result;
}

QVariant NetworkMonitorModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    switch ( role ) {
    case Qt::DisplayRole:
        if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
            switch ( static_cast<Column>(section) ) {
            case TypeColumn:
                return i18nc("@title:column", "Type");
            case ContentTypeColumn:
                return i18nc("@title:column", "Content");
            case TimeColumn:
                return i18nc("@title:column", "Time");
            case UrlColumn:
                return i18nc("@title:column", "URL");
            case DataColumn:
                return i18nc("@title:column", "Data");
            default:
                return QVariant();
            }
        } else {
            return QVariant();
        }

    default:
        return QVariant();
    }
}

QString NetworkMonitorModel::decodeHtml( const QByteArray &data ) const
{
    QTextCodec *textCodec = QTextCodec::codecForHtml( data,
            QTextCodec::codecForUtfText(data, QTextCodec::codecForName("UTF-8")) );
    return textCodec ? textCodec->toUnicode(data) : data;
}

QVariant NetworkMonitorModel::data( const QModelIndex &index, int role ) const
{
    if ( !index.isValid() ) {
        return QVariant();
    }

    NetworkMonitorModelItem *data = monitorDataFromIndex( index );
    if ( !data ) {
        return QVariant();
    }

    switch ( role ) {
    case Qt::DisplayRole:
        switch ( index.column() ) {
        case TypeColumn:
            return NetworkMonitorModelItem::nameFromType( data->type() );
        case ContentTypeColumn:
            return NetworkMonitorModelItem::nameFromContentType( data->contentType() );
        case TimeColumn:
            return KGlobal::locale()->formatLocaleTime( data->time() );
        case UrlColumn:
            return data->url();
        case DataColumn: {
            const bool htmlFormat = data->type() == NetworkMonitorModelItem::Reply &&
                    (data->contentType() == NetworkMonitorModelItem::HtmlData ||
                     data->contentType() == NetworkMonitorModelItem::XmlData);
            const bool textData = htmlFormat || data->contentType() == NetworkMonitorModelItem::CssData ||
                     data->contentType() == NetworkMonitorModelItem::ScriptData ||
                     data->contentType() == NetworkMonitorModelItem::UnknownTextData;
            if ( data->type() == NetworkMonitorModelItem::Reply && !textData ) {
                return data->mimeType();
            }

            QString result = data->type() == NetworkMonitorModelItem::Reply
                    ? decodeHtml(data->data()) : data->data(); // TODO fallback charset in ServiceProviderData
            // Remove carriage return characters, they would get drawn in views for some reason
            result = result.replace( '\r', QString() );
            return limitLineCount( result.trimmed() );
        }
        default:
            return QVariant();
        }

    case Qt::EditRole:
        switch ( index.column() ) {
        case UrlColumn:
            return data->url();
        case DataColumn:
            if ( data->contentType() == NetworkMonitorModelItem::ImageData && data->imageData() ) {
                // Read image from temporary file and convert to a pixmap
                // Shouldn't be used to often, eg. for copy to clipboard actions
                // The pixmap isn't stored in memory to save space
                return QPixmap::fromImage( QImage(data->imageData()->tempFile->fileName()) );
            } else if ( data->type() == NetworkMonitorModelItem::Reply ) {
                return decodeHtml( data->data() ); // TODO fallback charset in ServiceProviderData
            } else {
                return data->data();
            }
        default:
            return QVariant();
        }

    case Qt::ToolTipRole:
        switch ( index.column() ) {
        case UrlColumn:
            return data->url();
        case DataColumn: {
            const QString limited = limitLineCount( data->data(), true, 15 );
            switch ( data->contentType() ) {
            case NetworkMonitorModelItem::ImageData:
                if ( data->imageData() && data->imageData()->tempFile ) {
                    const int maxImageDimension =
                            KGlobalSettings::desktopGeometry(QPoint(0, 0)).height() / 2;
                    int width = data->imageData()->size.width();
                    int height = data->imageData()->size.height();
                    if ( width > height ) {
                        if ( width > maxImageDimension ) {
                            height *= maxImageDimension / width;
                            width = maxImageDimension;
                        }
                    } else {
                        if ( height > maxImageDimension ) {
                            width *= maxImageDimension / height;
                            height = maxImageDimension;
                        }
                    }
                    return QString("%6, %2x%3:<br />"
                            "<img src='%1' width='%4' height='%5' style='padding:2px;' />")
                            .arg(data->imageData()->tempFile->fileName())
                            .arg(data->imageData()->size.width())
                            .arg(data->imageData()->size.height())
                            .arg(width).arg(height).arg(data->mimeType());
                } else {
                    return QVariant();
                }
            case NetworkMonitorModelItem::HtmlData:
            case NetworkMonitorModelItem::XmlData:
                return encodeHtml( limited );
            default:
                return limited;
            }
        }
        default:
            return QVariant();
        }

    case Qt::DecorationRole:
        switch ( index.column() ) {
        case TypeColumn:
            switch ( data->type() ) {
            case NetworkMonitorModelItem::GetRequest:
            case NetworkMonitorModelItem::PostRequest:
                return KIcon("download");
            case NetworkMonitorModelItem::Reply:
                return KIcon("go-up");
            default:
                return QVariant();
            }

        case ContentTypeColumn:
            switch ( data->contentType() ) {
            case NetworkMonitorModelItem::HtmlData:
                return KIcon("text-html");
            case NetworkMonitorModelItem::XmlData:
                return KIcon("text-xml");
            case NetworkMonitorModelItem::CssData:
                return KIcon("text-css");
            case NetworkMonitorModelItem::ScriptData:
                return KIcon("text-x-script");
            case NetworkMonitorModelItem::UnknownTextData:
                return KIcon("text-plain");
            case NetworkMonitorModelItem::ImageData:
                return KIcon("image-x-generic");
            case NetworkMonitorModelItem::RetrievingContentType:
                return KIcon("task-ongoing");
            default:
                return QVariant();
            }

        case DataColumn:
            switch ( data->contentType() ) {
            case NetworkMonitorModelItem::ImageData:
                return data->imageData() ? data->imageData()->icon : QVariant();
            default:
                return QVariant();
            }

        default:
            return QVariant();
        }

    case DataTypeRole:
        return data->type();
    case ContentTypeRole:
        return data->contentType();

    default:
        return QVariant();
    }
}

Qt::ItemFlags NetworkMonitorModel::flags( const QModelIndex &index ) const
{
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

NetworkMonitorModelItem::NetworkMonitorModelItem( NetworkMonitorModelItem::Type type, const QString &url,
                                        const QByteArray &data, QObject *parent )
            : QObject(parent), m_model(0), m_type(type),
              m_contentType(NetworkMonitorModelItem::RetrievingContentType),
              m_time(QTime::currentTime()), m_url(url), m_data(data), m_imageData(0)
{
    const KUrl _url( url );
    if ( !data.isEmpty() ) {
        m_contentType = contentTypeFromContent( data, _url );
        if ( m_contentType == NetworkMonitorModelItem::UnknownData ) {
            if ( type == Reply ) {
                m_contentType = contentTypeFromUrl( _url );
            } else {
                m_contentType = UnknownTextData;
            }
        }

        if ( m_contentType == ImageData ) {
            prepareAdditionalImageData();
        }
    } else {
        m_contentType = contentTypeFromUrl( _url );
    }
}

NetworkMonitorModelItem::~NetworkMonitorModelItem()
{
    delete m_imageData;
}

void NetworkMonitorModelItem::prepareAdditionalImageData()
{
    // Delete old image data, if any
    if ( m_imageData ) {
        delete m_imageData;
    }

    // Read image data
    QImage image = QImage::fromData( m_data );
    if ( image.isNull() ) {
        // Cannot read image from data, maybe empty data
        return;
    }

    // Create new object with additional data for items containing image data
    m_imageData = new AdditionalImageData();
    m_imageData->size = image.size();
    QPixmap smallPixmap = QPixmap::fromImage( image.scaledToHeight(32, Qt::SmoothTransformation) );
    if ( smallPixmap.isNull() ) {
        // Scaling produced an empty pixmap (very high and narrow or very wide and little height)
        // Create an empty 16x16 pixmap
        smallPixmap = QPixmap( 32, 32 );
        smallPixmap.fill( Qt::transparent );
    }
    m_imageData->icon = KIcon( smallPixmap );

    // Write image data to a temporary file, for display in a QToolTip using an HTML <img> tag
    m_imageData->tempFile = new KTemporaryFile();
    m_imageData->tempFile->setAutoRemove( true );
    m_imageData->tempFile->setPrefix( KUrl(m_url).fileName() );
    if ( m_imageData->tempFile->open() ) {
        m_imageData->tempFile->write( m_data );
        m_imageData->tempFile->close();
    }
}

NetworkMonitorModelItem::ContentType NetworkMonitorModelItem::contentTypeFromUrl( const KUrl &url )
{
    KMimeType::Ptr mimeType = KMimeType::findByUrl( url );
    if ( mimeType == KMimeType::defaultMimeTypePtr() ) {
        // Mime type not found, use KIO to get the mime type asynchronously
        KIO::MimetypeJob *job = KIO::mimetype( url, KIO::HideProgressInfo );
        connect( job, SIGNAL(mimetype(KIO::Job*,QString)), this, SLOT(mimetype(KIO::Job*,QString)) );
        connect( job, SIGNAL(finished(KJob*)), this, SLOT(mimetypeJobFinished(KJob*)) );
        job->start();
        return RetrievingContentType;
    } else {
        // Mime type found
        m_mimeType = mimeType->name();
        return contentTypeFromMimeType( mimeType->name() );
    }
}

NetworkMonitorModelItem::ContentType NetworkMonitorModelItem::contentTypeFromContent(
        const QByteArray &content, const KUrl &url )
{
    int accuracy;
    KMimeType::Ptr mimeType = KMimeType::findByContent( content, &accuracy );
    if ( (mimeType == KMimeType::defaultMimeTypePtr() || accuracy < 70) && url.isValid() ) {
        // No accurate mime type found, find mime type from URL
        mimeType = KMimeType::findByUrl( url );
    }
    m_mimeType = mimeType->name();
    return contentTypeFromMimeType( mimeType->name() );
}

QString NetworkMonitorModelItem::nameFromType( NetworkMonitorModelItem::Type type )
{
    switch ( type ) {
    case GetRequest:
        return i18nc("@info/plain", "Request (GET)");
    case PostRequest:
        return i18nc("@info/plain", "Request (POST)");
    case Reply:
        return i18nc("@info/plain", "Reply");
    default:
        kDebug() << "Unknown type" << type;
        return QString();
    }
}

QString NetworkMonitorModelItem::nameFromContentType( NetworkMonitorModelItem::ContentType contentType )
{
    switch ( contentType ) {
    case HtmlData:
        return i18nc("@info/plain", "HTML");
    case XmlData:
        return i18nc("@info/plain", "XML");
    case ImageData:
        return i18nc("@info/plain", "Image");
    case CssData:
        return i18nc("@info/plain", "CSS");
    case ScriptData:
        return i18nc("@info/plain", "Script");
    case UnknownTextData:
        return i18nc("@info/plain", "Text");
    case RetrievingContentType:
        return i18nc("@info/plain", "(wait)");
    case UnknownData:
    default:
        return i18nc("@info/plain", "Unknown");
    }
}

NetworkMonitorModelItem::Type NetworkMonitorModelItem::typeFromOperation(
            QNetworkAccessManager::Operation operation )
{
    switch ( operation ) {
    case QNetworkAccessManager::HeadOperation:
    case QNetworkAccessManager::GetOperation:
        return GetRequest;
    case QNetworkAccessManager::PostOperation:
        return PostRequest;
    default:
        return Invalid;
    }
}

NetworkMonitorModelItem::ContentType NetworkMonitorModelItem::contentTypeFromMimeType( const QString &type )
{
    if ( type.startsWith(QLatin1String("image")) ) {
        return ImageData;
    } else if ( type.contains(QLatin1String("html")) ) {
        return HtmlData;
    } else if ( type.endsWith(QLatin1String("/xml")) ) {
        return XmlData;
    } else if ( type == QLatin1String("text/css") ) {
        return CssData;
    } else if ( type.endsWith(QLatin1String("script")) ) {
        return ScriptData;
    } else if ( type.startsWith(QLatin1String("text/")) ) {
        return UnknownTextData;
    } else {
        return UnknownData;
    }
}

void NetworkMonitorModelItem::mimetype( KIO::Job *job, const QString &type )
{
    Q_UNUSED( job );
    m_mimeType = type;
    m_contentType = contentTypeFromMimeType( type );
    if ( m_contentType == ImageData ) {
        prepareAdditionalImageData();
    }
    if ( m_model ) {
        m_model->slotDataChanged( this );
    }
}

void NetworkMonitorModelItem::mimetypeJobFinished( KJob *job )
{
    Q_UNUSED( job );
    if ( m_contentType == RetrievingContentType ) {
        // Mimetype not found
        kDebug() << "Mimetype not found";
        m_contentType = UnknownData;
        if ( m_model ) {
            m_model->slotDataChanged( this );
        }
    }
}

void NetworkMonitorModel::slotDataChanged( NetworkMonitorModelItem *data )
{
    emit dataChanged( indexFromMonitorData(data, 0), indexFromMonitorData(data, ColumnCount - 1) );
}

void NetworkMonitorModel::requestCreated( NetworkMonitorModelItem::Type type, const QString &url,
                                          const QByteArray &data, QNetworkReply *reply )
{
    // Create new item for the request
    NetworkMonitorModelItem *requestItem = new NetworkMonitorModelItem( type, url, data, this );
    requestItem->setModel( this );

    // Insert request item
    beginInsertRows( QModelIndex(), 0, 0 );
    m_data.prepend( requestItem );
    endInsertRows();

    if ( reply ) {
        // Connect to the finished signal to add a reply item when the reply has finished
        connect( reply, SIGNAL(finished()), this, SLOT(replyFinished()) );
    }
}

void NetworkMonitorModel::replyFinished()
{
    QNetworkReply *reply = qobject_cast< QNetworkReply* >( sender() );
    if ( reply ) {
        // Get data from cache
        QAbstractNetworkCache *cache = reply->manager()->cache();
        QIODevice *cachedData = cache->data( reply->request().url() );
        if ( !cachedData ) {
            cachedData = cache->data( reply->url() );
        }
        QByteArray data;
        if ( cachedData ) {
            data = cachedData->readAll();
            delete cachedData;
        }

        // Insert reply item
        beginInsertRows( QModelIndex(), 0, 0 );
        NetworkMonitorModelItem *newData = new NetworkMonitorModelItem(
                NetworkMonitorModelItem::Reply, reply->url().toString(), data, this );
        newData->setModel( this );
        m_data.prepend( newData );
        endInsertRows();
    }
}

bool NetworkMonitorFilterModel::filterAcceptsRow( int sourceRow, const QModelIndex &sourceParent ) const
{
    if ( sourceParent.isValid() ) {
        return false;
    }

    // Filter by type
    const QModelIndex sourceIndex = sourceModel()->index( sourceRow, 0, sourceParent );
    const NetworkMonitorModelItem::Type type = static_cast< NetworkMonitorModelItem::Type >(
            sourceModel()->data(sourceIndex, NetworkMonitorModel::DataTypeRole).toInt() );
    if ( !m_types.testFlag(type) ) {
        return false;
    }

    // Filter by content type
    const NetworkMonitorModelItem::ContentType contentType = static_cast< NetworkMonitorModelItem::ContentType >(
            sourceModel()->data(sourceIndex, NetworkMonitorModel::ContentTypeRole).toInt() );
    return m_contentTypes.testFlag( contentType );
}

void NetworkMonitorFilterModel::setTypeFilter( NetworkMonitorModelItem::Types types )
{
    m_types = types;
    invalidateFilter();
}

void NetworkMonitorFilterModel::setContentTypeFilter( NetworkMonitorModelItem::ContentTypes contentTypes )
{
    m_contentTypes = contentTypes;
    invalidateFilter();
}

NetworkMemoryCache::~NetworkMemoryCache()
{
    clear();
}

void NetworkMemoryCache::clear()
{
    qDeleteAll( m_orderedData );
    m_orderedData.clear();
    m_data.clear();
}

QIODevice *NetworkMemoryCache::prepare( const QNetworkCacheMetaData &metaData )
{
    if ( !metaData.isValid() || !metaData.url().isValid() ) {
        return 0;
    }

    QBuffer *buffer = new QBuffer( this );
    if ( !buffer->open(QIODevice::ReadWrite) ) {
        delete buffer;
        return 0;
    }
    m_prepared.insert( buffer, metaData );
    return buffer;
}

void NetworkMemoryCache::insert( QIODevice *device )
{
    if ( !m_prepared.contains(device) ) {
        kWarning() << "Call prepare() first";
        return;
    }

    // Get prepared meta data object and insert it with the data read from device
    const QNetworkCacheMetaData metaData = m_prepared.take( device );
    device->reset(); // Go to beginning (after writing to device it is at the end)
    const QByteArray data = device->readAll();
    CacheData *cacheData = new CacheData( metaData, data );
    m_data.insert( metaData.url(), cacheData );
    m_orderedData << cacheData;
    device->close();
    delete device;

    QTimer::singleShot( TIMEOUT, this, SLOT(removeOldestCacheData()) );
}

void NetworkMemoryCache::removeOldestCacheData()
{
    if ( m_orderedData.isEmpty() ) {
        return;
    }

    CacheData *cacheData = m_orderedData.first();
    if ( !remove(cacheData->metaData.url()) ) {
        kWarning() << "Could not remove old cache item" << cacheData->metaData.url();
        m_orderedData.removeFirst();
    }
}

bool NetworkMemoryCache::remove( const QUrl &url )
{
    if ( m_data.contains(url) ) {
        // Data cached for url, remove it
        CacheData *cacheData = m_data.take( url );
        m_orderedData.removeOne( cacheData );
        delete cacheData;
        return true;
    } else {
        return false;
    }
}

void NetworkMemoryCache::updateMetaData( const QNetworkCacheMetaData &metaData )
{
    if ( !m_data.contains(metaData.url()) ) {
        return;
    }

    CacheData *data = m_data[ metaData.url() ];
    data->metaData = metaData;
}

QIODevice *NetworkMemoryCache::data( const QUrl &url )
{
    if ( !m_data.contains(url) ) {
        return 0;
    }

    CacheData *data = m_data[ url ];
    QBuffer *buffer = new QBuffer( this );
    buffer->setData( data->data );
    if ( !buffer->open(QIODevice::ReadOnly) ) {
        delete buffer;
        return 0;
    } else {
        return buffer;
    }
}

QNetworkCacheMetaData NetworkMemoryCache::metaData( const QUrl &url )
{
    if ( !m_data.contains(url) ) {
        return QNetworkCacheMetaData();
    }

    CacheData *data = m_data[ url ];
    return data->metaData;
}

NetworkMemoryCache::CacheData::CacheData( const QNetworkCacheMetaData &metaData,
                                          const QByteArray &data )
        : metaData(metaData), data(data)
{
}

QNetworkReply *MonitorNetworkAccessManager::createRequest( QNetworkAccessManager::Operation op,
                                                           const QNetworkRequest &request,
                                                           QIODevice *outgoingData )
{
    QByteArray data;
    QNetworkReply *reply;
    QNetworkRequest _request( request );
    _request.setAttribute( QNetworkRequest::CacheSaveControlAttribute, true );
    if ( outgoingData ) {
        QBuffer *buffer = qobject_cast< QBuffer* >( outgoingData );
        if ( buffer ) {
            // Read from buffer without changing the position in outgoingData
            data = buffer->buffer();
            reply = QNetworkAccessManager::createRequest( op, _request, outgoingData );
        } else if ( !outgoingData->isSequential() ) {
            // Read from random access device and reset to the start position
            data = outgoingData->readAll();
            outgoingData->reset();
            reply = QNetworkAccessManager::createRequest( op, _request, outgoingData );
        } else {
            // Read from sequential device and create a new device after reading,
            // because sequential devices can only be read once and would be empty when send to
            // QNetworkAccessManager::createRequest()
            buffer = new QBuffer( outgoingData->parent() );
            buffer->setData( outgoingData->readAll() );
            data = buffer->buffer();
            outgoingData->close();
            outgoingData->deleteLater();
            reply = QNetworkAccessManager::createRequest( op, _request, buffer );
        }
    } else {
        reply = QNetworkAccessManager::createRequest( op, _request );
    }

    emit requestCreated( NetworkMonitorModelItem::typeFromOperation(op),
                         _request.url().toString(), data, reply );
    return reply;
}
