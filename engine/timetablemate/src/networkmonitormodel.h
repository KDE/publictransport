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

#ifndef NETWORKMONITORMODEL_H
#define NETWORKMONITORMODEL_H

// KDE includes
#include <KIcon>

// Qt inlcudes
#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QAbstractNetworkCache>
#include <QNetworkAccessManager>
#include <QTime>
#include <QUrl>

class NetworkMonitorModel;
class KJob;
class KUrl;
class KTemporaryFile;
class QPixmap;
namespace KIO
{
    class Job;
}

/** @brief Represents an item in a NetworkMonitorModel. */
class NetworkMonitorModelItem : public QObject {
    Q_OBJECT
    friend class NetworkMonitorModel;

public:
    /** @brief Available types of NetworkMonitorModel items. */
    enum Type {
        Invalid         = 0x0000, /**< An invalid item. */

        GetRequest      = 0x0001, /**< A GET request item. */
        PostRequest     = 0x0002, /**< A POST request item. */
        Reply           = 0x0004, /**< A reply item. */

        AllTypes = GetRequest | PostRequest | Reply
    };
    Q_DECLARE_FLAGS( Types, Type );

    /**
     * @brief Available content types of NetworkMonitorModel items.
     * More specific type information can be retrieved using NetworkMonitorData::mimeType.
     **/
    enum ContentType {
        NoData          = 0x0000, /**< No data, eg. for POST requests. */
        RetrievingContentType
                        = 0x0001, /**< The content type is currently being retrieved and will
                                    * change once it is available. */
        UnknownData     = 0x0002, /**< Unknown data, ie. none of the other types. */

        UnknownTextData = 0x0004, /**< Unknown text data, ie. text data which is not HtmlData,
                                    * XmlData, CssData or ScriptData. */

        HtmlData        = 0x0008, /**< HTML data. */
        XmlData         = 0x0010, /**< XML data. */

        CssData         = 0x0020, /**< CSS data. */
        ScriptData      = 0x0040, /**< Script data. */
        ImageData       = 0x0080, /**< Image data. */

        InterestingData = HtmlData | XmlData | UnknownTextData,
                                  /**< Content types that are interesting in TimetableMate. */
        UninterestingData = RetrievingContentType | ImageData | CssData | ScriptData,
        AllData =  InterestingData | UninterestingData | UnknownData | NoData
    };
    Q_DECLARE_FLAGS( ContentTypes, ContentType );

    /** @brief Contains additional data for items with content of type ImageData. */
    struct AdditionalImageData {
        AdditionalImageData() : tempFile(0) {};
        KIcon icon; /**< A small (32px height) version of the image as KIcon. */
        KTemporaryFile *tempFile; /**< The temporary file containing the image data. */
        QSize size;
    };

    /** @brief Constructor. */
    NetworkMonitorModelItem( Type type, const QString &url,
                             const QByteArray &data = QByteArray(), QObject *parent = 0 );

    /** @brief Destructor. */
    virtual ~NetworkMonitorModelItem();

    /** @brief Get a name for @p type, to be displayed to the user. */
    static QString nameFromType( Type type );

    /** @brief Get a name for @p contentType, to be displayed to the user. */
    static QString nameFromContentType( ContentType contentType );

    /** @brief Get the Type for @p operation. */
    static Type typeFromOperation( QNetworkAccessManager::Operation operation );

    /** @brief Get the ContentType for @p mimeType. */
    static ContentType contentTypeFromMimeType( const QString &mimeType );

    /** @brief Get the type of this item. */
    Type type() const { return m_type; };

    /** @brief Get the type of the data contents of this item, use data() to get the data. */
    ContentType contentType() const { return m_contentType; };

    /** @brief The time when this request was sent or this reply was received. */
    QTime time() const { return m_time; };

    /** @brief The url of the request/reply. */
    QString url() const { return m_url; };

    /** @brief Data sent/received with this request/reply. */
    QByteArray data() const { return m_data; };

    /** @brief The mime type of the data of this item, use data() to get the data. */
    QString mimeType() const { return m_mimeType; };

    /** @brief Additional data for items of type ImageData. */
    AdditionalImageData *imageData() const { return m_imageData; };

    /** @brief The NetworkMonitorModel into which this item was inserted. */
    NetworkMonitorModel *model() const { return m_model; };

protected slots:
    void mimetype( KIO::Job *job, const QString &type );
    void mimetypeJobFinished( KJob *job );

protected:
    void setModel( NetworkMonitorModel *model ) { m_model = model; };
    void prepareAdditionalImageData();
    ContentType contentTypeFromUrl( const KUrl &url );
    ContentType contentTypeFromContent( const QByteArray &content, const KUrl &url );

private:
    NetworkMonitorModel *m_model;
    Type m_type;
    ContentType m_contentType;
    QTime m_time;
    QString m_url;
    QByteArray m_data;
    QString m_mimeType;
    AdditionalImageData *m_imageData;
};

/**
 * @brief Model to monitor network requests and replies.
 **/
class NetworkMonitorModel : public QAbstractListModel {
    Q_OBJECT
    friend class NetworkMonitorModelItem;

public:
    /** @brief Available columns. */
    enum Column {
        TypeColumn = 0, /**< Shows the names for the Type's of items. */
        TimeColumn, /**< Shows the times at which the items requests were sent
                * or replies were received. */
        ContentTypeColumn, /**< Shows the names for the ContentType's of items. */
        UrlColumn, /**< Shows the URL of items. */
        DataColumn, /**< Shows the data items. */

        ColumnCount /**< @internal */
    };

    /** @brief Additional roles for this model. */
    enum Roles {
        DataTypeRole = Qt::UserRole, /**< Of type NetworkMonitorData::Type. */
        ContentTypeRole = Qt::UserRole + 1 /**< Of type NetworkMonitorData::ContentType. */
    };

    /** @brief Constructor. */
    explicit NetworkMonitorModel( QObject *parent = 0 );

    /** @brief Destructor. */
    virtual ~NetworkMonitorModel();

    static QString encodeHtml( const QString &html );
    static QString limitLineCount( const QString &string, bool htmlFormat = false,
                                   int maxLineCount = 3, int maxCharactersPerLine = 100 );

    virtual int columnCount( const QModelIndex &parent = QModelIndex() ) const {
            Q_UNUSED(parent); return ColumnCount; };
    virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual QModelIndex index( int row, int column, const QModelIndex &parent = QModelIndex() ) const;
    virtual QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const;
    virtual QVariant headerData( int section, Qt::Orientation orientation,
                                 int role = Qt::DisplayRole ) const;
    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;
    virtual bool removeRows( int row, int count, const QModelIndex &parent = QModelIndex() );

    QString decodeHtml( const QByteArray &data ) const;

    /** @brief Clear monitor data. */
    void clear();

    /** @brief Get the QModelIndex for @p monitorData. */
    QModelIndex indexFromMonitorData( NetworkMonitorModelItem *data, int column = 0 ) const;

    /** @brief Get the item for @p index. */
    NetworkMonitorModelItem *monitorDataFromIndex( const QModelIndex &index ) const;

protected slots:
    void requestCreated( NetworkMonitorModelItem::Type type, const QString &url,
                         const QByteArray &data = QByteArray(), QNetworkReply *reply = 0 );
    void slotDataChanged( NetworkMonitorModelItem *data );
    void replyFinished();

private:
    QList< NetworkMonitorModelItem* > m_data;
};

/**
 * @brief Filters rows of a NetworkMonitorModel by Type and ContentType.
 *
 * The type of a row is read from the QModelIndex at the first column, in the
 * NetworkMonitorModel::DataTypeRole. The content type is read from the same index, in the
 * NetworkMonitorModel::ContentTypeRole.
 * Items of specific types can be filtered using setTypeFilter(). Specific content types can be
 * filtered using setContentTypeFilter().
 **/
class NetworkMonitorFilterModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    /**
     * @brief Construct a new NetworkMonitorFilterModel, use setSourceModel() to connect to a model.
     *
     * @param types The Types to use as initial type filter, use setTypeFilter() to change it later.
     * @param contentTypes The ContentTypes to use as initial content type filter, use
     *   setContentTypeFilter() to change it later.
     * @param parent The parent of this model.
     **/
    NetworkMonitorFilterModel( NetworkMonitorModelItem::Types types = NetworkMonitorModelItem::AllTypes,
            NetworkMonitorModelItem::ContentTypes contentTypes = NetworkMonitorModelItem::InterestingData,
            QObject *parent = 0 )
            : QSortFilterProxyModel(parent), m_types(types), m_contentTypes(contentTypes) {};

    /**
     * @brief Construct a new NetworkMonitorFilterModel, use setSourceModel() to connect to a model.
     *
     * @param parent The parent of this model.
     **/
    NetworkMonitorFilterModel( QObject *parent = 0 )
            : QSortFilterProxyModel(parent), m_types(NetworkMonitorModelItem::AllTypes),
              m_contentTypes(NetworkMonitorModelItem::InterestingData) {};

    /** @brief Set the Types to filter from the source model into this proxy model. */
    void setTypeFilter( NetworkMonitorModelItem::Types types = NetworkMonitorModelItem::AllTypes );

    /** @brief Set the ContentTypes to filter from the source model into this proxy model. */
    void setContentTypeFilter( NetworkMonitorModelItem::ContentTypes contentTypes =
                                    NetworkMonitorModelItem::InterestingData );

    /** @brief Get the Types which get filtered from the source model into this proxy model. */
    NetworkMonitorModelItem::Types typeFilter() const { return m_types; };

    /** @brief Get the ContentTypes which get filtered from the source model into this proxy model. */
    NetworkMonitorModelItem::ContentTypes contentTypeFilter() const { return m_contentTypes; };

protected:
    /** @brief Overriden to filter rows by Type and ContentType. */
    virtual bool filterAcceptsRow( int sourceRow, const QModelIndex &sourceParent ) const;

private:
    NetworkMonitorModelItem::Types m_types;
    NetworkMonitorModelItem::ContentTypes m_contentTypes;
};

/**
 * @brief Caches data retrieved in network replies for a short time.
 *
 * Because QNetworkAccessManager provides access to reply data in a sequential QIODevice, it can
 * be read only once and should be shared by the application afterwards. This cache shares the data
 * in memory. Can be used to read reply data without having empty data sent to a connected QWebView.
 *
 * After new data has been inserted into the cache a timeout gets started to remove the data from
 * the cache again. This is done to save memory and because only a connected QWebView and
 * NetworkMonitorModel need to read the data, which is done shortly after the new data has been
 * inserted.
 **/
class NetworkMemoryCache : public QAbstractNetworkCache {
    Q_OBJECT

public:
    /** @brief The time in milliseconds after which new data gets removed from the cache again. */
    static const int TIMEOUT = 5000;

    /** @brief Constructor. */
    explicit NetworkMemoryCache( QObject *parent = 0 ) : QAbstractNetworkCache(parent) {};

    virtual ~NetworkMemoryCache();

    virtual QIODevice *data( const QUrl &url );
    virtual QNetworkCacheMetaData metaData( const QUrl &url );
    virtual qint64 cacheSize() const { // TODO
            return m_data.count() * (sizeof(QUrl) + sizeof(CacheData*)); };

    virtual QIODevice *prepare( const QNetworkCacheMetaData &metaData );
    virtual void updateMetaData( const QNetworkCacheMetaData &metaData );
    virtual void insert( QIODevice *device );
    virtual bool remove( const QUrl &url );
    virtual void clear();

protected slots:
    virtual void removeOldestCacheData();

private:
    struct CacheData {
        CacheData( const QNetworkCacheMetaData &metaData, const QByteArray &data = QByteArray() );

        QNetworkCacheMetaData metaData;
        QByteArray data;
    };

    QHash< QUrl, CacheData* > m_data;
    QHash< QIODevice*, QNetworkCacheMetaData > m_prepared;
    QList< CacheData* > m_orderedData; // Ordered by insert time
};

/**
 * @brief A QNetworkAccessManager that emits a requestCreated() signal for each created request.
 **/
class MonitorNetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT

public:
    /** @brief Constructor. */
    explicit MonitorNetworkAccessManager( QObject *parent = 0 ) : QNetworkAccessManager(parent) {};

signals:
    /**
     * @brief Emitted, when a new request was created.
     *
     * @param type The type of the request.
     * @param url The requested URL.
     * @param data Data sent with the request (if @p type is NetworkMonitorModelItem::PostRequest).
     * @param reply A pointer to the QNetworkReply object created for the request.
     **/
    void requestCreated( NetworkMonitorModelItem::Type type, const QString &url,
                         const QByteArray &data = QByteArray(), QNetworkReply *reply = 0 );

protected:
    /** @brief Overridden to emit requestCreated() and read data in @p outgoingData. */
    virtual QNetworkReply *createRequest( Operation op, const QNetworkRequest &request,
                                          QIODevice *outgoingData = 0 );
};

#endif // Multiple inclusion guard
