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

#ifndef DATASOURCEDATA_HEADER
#define DATASOURCEDATA_HEADER

// Own includes
#include "enums.h"
#include "request.h"

// Qt includes
#include <QStringList>

class QTimer;

/**
 * @brief Abstract base class for all data sources.
 *
 * This class only provides a virtual data() function to get the QVariantHash with the data
 * of the data source, but it does not store it in a member variable. This gives subclasses the
 * possibility to store the data in other structures, like ProvidersDataSource does.
 * SimpleDataSource on the other hand adds a QVariantHash member variable to store the data of the
 * data source and is used for most data sources.
 *
 * @see ProvidersDataSource
 * @see SimpleDataSource
 **/
class DataSource {
public:
    /** @brief Create a new data source object for a data source with the given @p sourceName. */
    DataSource( const QString &sourceName = QString() );
    virtual ~DataSource();

    /** @brief The data source name. */
    QString name() const { return m_name; };

    /** @brief Get data stored for the data source. */
    virtual QVariantHash data() const = 0;

    /** @brief Get data stored for the data source. */
    QVariant value( const QString &key ) const { return data()[key]; };

protected:
    QString m_name;
};

/**
 * @brief Simple data source class, containing data for a data source.
 *
 * This class overrides DataSource and adds a QVariantHash member variable to store the data
 * of the data source. It also adds some functions to give more control over the stored data.
 **/
class SimpleDataSource : public DataSource {
public:
    /** @brief Create a new data source object for a data source @p sourceName with @p data. */
    SimpleDataSource( const QString &sourceName = QString(),
                      const QVariantHash &data = QVariantHash() );

    /** @brief Get data stored for the data source. */
    virtual QVariantHash data() const { return m_data; };

    /** @brief Clear the data stored for the data source. */
    void clear() { m_data.clear(); };

    /** @brief Set data stored for the data source. */
    virtual void setData( const QVariantHash &data ) { m_data = data; };

    /** @brief Insert @p value as @p key into the data stored for the data source. */
    virtual void setValue( const QString &key, const QVariant &value ) {
        m_data.insert( key, value );
    };

protected:
    QVariantHash m_data;
};

/**
 * @brief Contains data for service provider data sources.
 *
 * The data() function returns the complete data for the "ServiceProviders" data source.
 * To get data for a single service provider, ie. a "ServiceProvider [providerId]" data source,
 * use the providerData() function.
 **/
class ProvidersDataSource : public DataSource {
public:
    /**
     * @brief Contains data for a single service provider.
     *
     * The data is separated into state data and non-state data. Non-state data does not change
     * as long as the provider plugin sources do not change, ie. the .pts file and eg. script files
     * for script provider plugins. This data contains values read from the .pts file and other
     * cached values that require the ServiceProvider object to be created, eg. the list of
     * supported features.
     *
     * State data contains information about the provider plugins current state. The state ID is
     * a short string identifying the providers state, it can be one of these: "ready", "error",
     * "gtfs_feed_import_pending" and "importing_gtfs_feed". State data always contains a field
     * "statusMessage" with a human readable description of the providers state. The may be
     * different status messages for the same state ID, providing some more information. Depending
     * on the state ID there may be more fields available in the state data. For example when the
     * state is "importing_gtfs_feed", there is a "progress" field with the progress percentage
     * of the import job.
     **/
    struct ProviderData {
        /**
         * @brief Construct a new ProviderData object.
         * @param data Normal data for the provider.
         * @param stateId The ID of the providers current state. Since ProviderData objects are
         *   only created when there is no operation running for the provider, dynamic states such
         *   as "importing_gtfs_feed" are replaced with static ones using
         *   ProvidersDataSource::toStaticState().
         * @param stateData Data for the providers current state. A field 'statusMessage' should
         *   always be contained.
         **/
        ProviderData( const QVariantHash &data = QVariantHash(), const QString &stateId = QString(),
                      const QVariantHash &stateData = QVariantHash() );

        /** @brief Combine provider data with state ID and state data. */
        QVariantHash data() const;

        QVariantHash dataWithoutState;
        QString stateId;
        QVariantHash stateData;
    };

    /**
     * @brief Create a new data source object for all service provider data sources.
     **/
    ProvidersDataSource( const QString &sourceName = QString(),
                         const QHash<QString, ProviderData> &data = QHash<QString, ProviderData>() );

    /**
     * @brief Replace dynamic states with static ones.
     *
     * Dynamic states are states that will change to another state after some time,
     * eg. "importing_gtfs_feed". When loading provider states from cache it makes no sense
     * to use dynamic states, because they are invalid then, most probably they stayed in the
     * cache after a crash. The operation described by the dynamic state cannot be continued
     * after a restart. Such dynamic states are replaced with static states here, that can be
     * used in initialization.
     * For example "importing_gtfs_feed" gets replaced by "gtfs_feed_import_pending".
     **/
    static QString toStaticState( const QString &dynamicStateId );

    /** @brief Add a provider with the given @p providerId and @p providerData. */
    void addProvider( const QString &providerId, const ProviderData &providerData ) {
        m_providerData.insert( providerId, providerData );
    };

    /** @brief Remove the provider with the given @p providerId. */
    void removeProvider( const QString &providerId ) {
        m_providerData.remove( providerId );
    };

    /** @brief Get all provider data, ie. the data for the "ServiceProviders" data source. */
    virtual QVariantHash data() const;

    /** @brief Get data for the provider with the given @p providerId. */
    QVariantHash providerData( const QString &providerId ) const {
        return m_providerData.value(providerId).data();
    };

    /** @brief Get the current state of the provider with the given @p providerId. */
    QString providerState( const QString &providerId ) const {
        return m_providerData.value(providerId).stateId;
    };

    /** @brief Get state data of the provider with the given @p providerId. */
    QVariantHash providerStateData( const QString &providerId ) const {
        return m_providerData.value(providerId).stateData;
    };

    /** @brief Set the state of the provider with the given @p providerId to @p stateId. */
    void setProviderState( const QString &providerId, const QString &stateId,
                           const QVariantHash &stateData = QVariantHash() )
    {
        m_providerData[ providerId ].stateId = stateId;
        m_providerData[ providerId ].stateData = stateData;
    };

    /** @brief Set the state data of the provider with the given @p providerId to @p stateData. */
    void setProviderStateData( const QString &providerId, const QVariantHash &stateData ) {
        m_providerData[ providerId ].stateData = stateData;
    };

private:
    QHash< QString, ProviderData > m_providerData;
};

/** @brief Contains data for a timetable data source, ie. for departures, arrivals or journeys. */
class TimetableDataSource : public SimpleDataSource {
public:
    /** @brief Create a new timetable data source object for a data source @p sourceName. */
    TimetableDataSource( const QString &dataSource = QString(),
                         const QVariantHash &data = QVariantHash() );
    virtual ~TimetableDataSource();

    void addUsingDataSource( const QSharedPointer<AbstractRequest> &request,
                             const QString &sourceName, const QDateTime &dateTime, int maxCount );
    void removeUsingDataSource( const QString &sourceName );
    int usageCount() const { return m_dataSources.count(); };
    QStringList usingDataSources() const { return m_dataSources.keys(); };

    /** @brief Get the date and time of the last timetable data update. */
    QDateTime lastUpdate() const {
        return !m_data.contains("updated") ? QDateTime() : m_data["updated"].toDateTime();
    };

    /** @brief Get the ID of the provider used to get the timetable data. */
    QString providerId() const { return m_data["serviceProvider"].toString(); };

    /** @brief Whether or not this data source always contains timetable items for the same time. */
    bool hasConstantTime() const { return m_name.contains(QLatin1String("date=")); };

    /** @brief Get flags for this data source, see UpdateFlags. */
    UpdateFlags updateFlags() const;

    /**
     * @brief Get the key under which timetable items are stored in the data source.
     * @see timetableItems(), setTimetableItems()
     **/
    QString timetableItemKey() const;

    /**
     * @brief Get the list of timetable items, which are stored in this data source.
     * This is equivalent to:
     * @code value( timetableItemKey() ); @endcode
     * @see timetableItemKey()
     **/
    QVariantList timetableItems() const { return m_data[ timetableItemKey() ].toList(); };

    /**
     * @brief Set the list of timetable items, which are stored in this data source.
     * This is equivalent to:
     * @code setValue( timetableItemKey(), items ); @endcode
     * @see timetableItemKey()
     **/
    void setTimetableItems( const QVariantList &items );

    /**
     * @brief Get all additional data of this data source.
     * Additional data gets stored by a hash value for the associated timetable item.
     **/
    QHash< uint, TimetableData > additionalData() const { return m_additionalData; };

    /**
     * @brief Set all additional data to @p additionalData.
     * This replaces all previously set additional timetable data.
     **/
    void setAdditionalData( const QHash< uint, TimetableData > &additionalData ) {
        m_additionalData = additionalData;
    };

    /** @brief Get additional data for the item with @p departureHash. */
    TimetableData additionalData( uint departureHash ) const {
        return m_additionalData[ departureHash ];
    };

    /** @brief Set additional data for the item with @p departureHash to @p additionalData. */
    void setAdditionalData( uint departureHash, const TimetableData &additionalData ) {
        m_additionalData[ departureHash ] = additionalData;
    };

    /** @brief Timer to update the data source periodically. */
    QTimer *updateTimer() const { return m_updateTimer; };

    void setUpdateTimer( QTimer *timer );
    void stopUpdateTimer();

    /** @brief Timer to delay updates to additional timetable data of timetable items. */
    QTimer *updateAdditionalDataDelayTimer() const { return m_updateAdditionalDataDelayTimer; };

    void setUpdateAdditionalDataDelayTimer( QTimer *timer );

    /**
     * @brief The time at which new downloads will have sufficient changes.
     * Sufficient means enough timetable items are in the past or there may be changed delays.
     **/
    QDateTime nextDownloadTimeProposal() const { return m_nextDownloadTimeProposal; };
    void setNextDownloadTimeProposal( const QDateTime &nextDownloadTime ) {
        m_nextDownloadTimeProposal = nextDownloadTime;
    };

    bool enoughDataAvailable( const QDateTime &dateTime, int count );

    QSharedPointer< AbstractRequest > request( const QString &sourceName ) const;

private:
    struct SourceData {
        SourceData( const QSharedPointer<AbstractRequest> &request = QSharedPointer<AbstractRequest>(),
                    const QDateTime &dateTime = QDateTime(), int maxCount = 1 )
                : request(request), dateTime(dateTime), maxCount(maxCount) {}

        QSharedPointer< AbstractRequest > request;
        QDateTime dateTime;
        int maxCount;
    };

    QVariantHash m_data;
    QHash< uint, TimetableData > m_additionalData;
    QTimer *m_updateTimer;
    QTimer *m_updateAdditionalDataDelayTimer;
    QDateTime m_nextDownloadTimeProposal;
    QHash< QString, SourceData > m_dataSources; // Connected data sources ("ambiguous" ones)
};

#endif // Multiple inclusion guard
