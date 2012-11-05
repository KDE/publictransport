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

/** @brief Contains data for a data source. */
struct DataSource {
    DataSource( const QString &dataSource = QString(), const QVariantHash &data = QVariantHash() );
    virtual ~DataSource();

    /** @brief The data source name. */
    QString name;

    /** @brief Data stored for the data source. */
    QVariantHash data;
};

/** @brief Contains data for a timetable data source, ie. for departures, arrivals or journeys. */
class TimetableDataSource : public DataSource {
public:
    TimetableDataSource( const QString &dataSource = QString(),
                         const QVariantHash &data = QVariantHash() );
    virtual ~TimetableDataSource();

    void addUsingDataSource( const QSharedPointer<AbstractRequest> &request,
                             const QString &sourceName, const QDateTime &dateTime, int maxCount );
    void removeUsingDataSource( const QString &sourceName );
    int usageCount() const { return m_dataSources.count(); };
    QStringList usingDataSources() const { return m_dataSources.keys(); };

    QDateTime lastUpdate() const {
        return !data.contains("updated") ? QDateTime() : data["updated"].toDateTime();
    };

    bool hasConstantTime() const { return name.contains(QLatin1String("date=")); };
    UpdateFlags updateFlags() const;

    QVariantList timetableItems() const;
    void setTimetableItems( const QVariantList &items );

    /**
     * @brief Contains already downloaded additional data.
     * Additional data gets stored by a hash value for the associated timetable item.
     **/
    QHash< uint, TimetableData > additionalData() const { return m_additionalData; };
    void setAdditionalData( const QHash< uint, TimetableData > &additionalData ) {
        m_additionalData = additionalData;
    };
    TimetableData additionalData( uint departureHash ) const {
        return m_additionalData[ departureHash ];
    };
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

    QString timetableItemKey() const;

    QHash< uint, TimetableData > m_additionalData;
    QTimer *m_updateTimer;
    QTimer *m_updateAdditionalDataDelayTimer;
    QDateTime m_nextDownloadTimeProposal;
    QHash< QString, SourceData > m_dataSources; // Connected data sources ("ambiguous" ones)
};

#endif // Multiple inclusion guard
