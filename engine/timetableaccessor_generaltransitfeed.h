/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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
* @brief This file contains a class to access data from Google Transit using libgtfs.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_GOOGLETRANSIT_HEADER
#define TIMETABLEACCESSOR_GOOGLETRANSIT_HEADER

#include "timetableaccessor.h"
#include "generaltransitfeed_importer.h"

class KTimeZone;
/** @class TimetableAccessorGeneralTransitFeed
* @brief This class uses a database similiar to the GTFS structure to access public transport data.
*
* To fill the database with data from a GeneralTransitFeedSpecification feed (zip file) use
* @ref GeneralTransitFeedImporter.
*
* This class does not use the default functions in @ref TimetableAccessor to request data. Instead
* these functions are overwritten to query a database for results and directly emitting a received
* signal.
*/
class TimetableAccessorGeneralTransitFeed : public TimetableAccessor
{
    Q_OBJECT
public:
    struct AgencyInformation {
        AgencyInformation() : timezone(0) {};
        virtual ~AgencyInformation();

        /** @brief Gets the offset in seconds for the agency's timezone. */
        int timeZoneOffset() const;

        QString name;
        QString phone;
        QString language;
        KUrl url;
        KTimeZone *timezone;
    };
    typedef QHash<uint, AgencyInformation*> AgencyInformations;

    /** @brief The maximum of stop suggestion to return. */
    static const int STOP_SUGGESTION_LIMIT = 100;

    explicit TimetableAccessorGeneralTransitFeed(
            TimetableAccessorInfo *info = new TimetableAccessorInfo() );
    virtual ~TimetableAccessorGeneralTransitFeed();

    /** @brief Gets a list of features that this accessor supports. */
    virtual QStringList features() const;

protected slots:
    void downloadProgress( KJob *job, ulong percent );
    void feedReceived( KJob *job );

    void importerProgress( qreal progress );
    void importerFinished( GeneralTransitFeedImporter::State state, const QString &errorText );

protected:
    virtual void requestDepartures( const QString &sourceName, const QString &city,
            const QString &stop, int maxCount, const QDateTime &dateTime,
            const QString &dataType = "departures", bool useDifferentUrl = false );

    virtual void requestStopSuggestions( const QString &sourceName, const QString &city,
            const QString &stop, ParseDocumentMode parseMode = ParseForStopSuggestions,
            int maxCount = 1, const QDateTime& dateTime = QDateTime::currentDateTime(),
            const QString &dataType = QString(), bool usedDifferentUrl = false );

    void downloadFeed();
    bool checkState( const QString &sourceName,
            const QString &city, const QString &stop, int maxCount, const QDateTime &dateTime,
            const QString &dataType, bool useDifferentUrl, ParseDocumentMode parseMode );

private:
    enum State {
        Initializing = 0,
        DownloadingFeed,
        ReadingFeed,
        Ready,

        ErrorDownloadingFeed = 10,
        ErrorReadingFeed,
        ErrorInDatabase
    };

    /**
     * @brief Converts a GTFS route_type value to a matching VehicleType.
     *
     * @see http://code.google.com/intl/en-US/transit/spec/transit_feed_specification.html#routes_txt___Field_Definitions
     **/
    VehicleType vehicleTypeFromGtfsRouteType( int gtfsRouteType ) const;

    QTime timeFromSecondsSinceMidnight( int secondsSinceMidnight, QDate *date = 0 ) const;

    void loadAgencyInformation();

    State m_state;
    AgencyInformations m_agencyCache; // Cache contents of the "agency" DB table, usally small, eg. only one agency

    // Stores information about currently running download jobs
    QHash<QString, JobInfos> m_jobInfos;
    GeneralTransitFeedImporter *m_importer;
};

#endif // Multiple inclusion guard
