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

/** @file
* @brief This file contains the base class for all accessors used by the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_HEADER
#define TIMETABLEACCESSOR_HEADER

// Own includes
#include "enums.h"
#include "departureinfo.h"

// KDE includes
#include <KUrl> // Member variable

// Qt includes
#include <QHash>
#include <QStringList>

class QScriptEngine;
class KJob;

class AbstractRequest;
class DepartureRequest;
class ArrivalRequest;
class StopSuggestionRequest;
class JourneyRequest;
class ChangelogEntry;
class StopInfo;
class DepartureInfo;
class JourneyInfo;
class TimetableAccessorInfo;

/**
 * @brief Gets timetable information for public transport from different service providers.
 *
 * This class can be instantiated to create an invalid timetable accessor. To create a valid
 * accessor instantiate one of the derivates of this class.
 * The easiest way to implement support for a new service provider is to add an XML file describing
 * the service provider and a script to parse timetable documents.
 * If that's not enough a new class can be derived from TimetableAccessor or it's derivates.
 * These functions should then be overwritten:
 *   @li requestDepartures()
 *   @li requestArrivals()
 *   @li requestStopSuggestions()
 *   @li requestJourneys()
 * If one of these functions isn't overridden, the associated timetable data can not be accessed
 * from the service provider with the accessor.
 **/
class TimetableAccessor : public QObject {
    Q_OBJECT
    Q_PROPERTY( QString serviceProvider READ serviceProvider )
    Q_PROPERTY( QStringList features READ features )
    Q_PROPERTY( QStringList featuresLocalized READ featuresLocalized )
    Q_PROPERTY( QString country READ country )
    Q_PROPERTY( QStringList cities READ cities )
    Q_PROPERTY( QString credit READ credit )
    Q_PROPERTY( int minFetchWait READ minFetchWait )
    Q_PROPERTY( bool useSeparateCityValue READ useSeparateCityValue )
    Q_PROPERTY( bool onlyUseCitiesInList READ onlyUseCitiesInList )

public:
    /**
     * @brief Constructs a new TimetableAccessor object. You should use getSpecificAccessor()
     *   to get an accessor that can download and parse documents from the given service provider.
     **/
    explicit TimetableAccessor( const TimetableAccessorInfo *info = 0, QObject *parent = 0 );

    /** @brief Destructor. */
    virtual ~TimetableAccessor();

    /**
     * @brief Gets a timetable accessor that is able to parse results from the given service provider.
     *
     * @param serviceProvider The ID of the service provider to get an accessor for.
     *   The ID starts with a country code, followed by an underscore and it's name.
     *   If it's empty, the default service provider for the users country will
     *   be used, if there is any.
     **/
    static TimetableAccessor *getSpecificAccessor( const QString &serviceProvider = QString(),
                                                   QObject *parent = 0 );

    /** @brief Gets the AccessorType enumerable for the given string. */
    static AccessorType accessorTypeFromString( const QString &sAccessorType );

    /** @brief Gets the service provider ID for the given accessor XML file name. */
    static QString serviceProviderIdFromFileName( const QString &accessorXmlFileName );

    /** @brief Gets the file path of the default service provider XML for the given @p location. */
    static QString defaultServiceProviderForLocation( const QString &location,
                                                      const QStringList &dirs = QStringList() );

    /** @brief Gets the service provider ID the accessor is designed for. */
    QString serviceProvider() const;

    /** @brief Gets the minimum seconds to wait between two data-fetches from the service provider. */
    virtual int minFetchWait() const;

    /**
     * @brief Gets a list of features that this accessor supports.
     * The default implementation calls scriptFeatures() and removes duplicates.
     **/
    virtual QStringList features() const;

    /** @brief Gets a list of short localized strings describing the supported features. */
    QStringList featuresLocalized() const;

    /** @brief Gets a list of features that this accessor supports through a script. */
    virtual QStringList scriptFeatures() const { return QStringList(); };

    /** @brief The country for which the accessor returns results. */
    QString country() const;

    /** @brief A list of cities for which the accessor returns results. */
    QStringList cities() const;

    QString credit() const;

    const TimetableAccessorInfo *info() const { return m_info; };

    /**
     * @brief Requests a list of departures.
     *
     * When the departure list is completely received @ref departureListReceived gets emitted.
     * The default implementation does nothing.
     * @param request Information about the departure request.
     **/
    virtual void requestDepartures( const DepartureRequest &request );

    /**
     * @brief Requests a list of arrivals.
     *
     * When the arrival list is completely received @ref departureListReceived gets emitted.
     * The default implementation does nothing.
     * @todo TODO use arrivalListReceived()
     * @param request Information about the arrival request.
     **/
    virtual void requestArrivals( const ArrivalRequest &request );

    /**
     * @brief Requests a list of journeys.
     *
     * When the journey list is completely received @ref journeyListReceived() gets emitted.
     * The default implementation does nothing.
     * @param request Information about the journey request.
     **/
    virtual void requestJourneys( const JourneyRequest &request );

    /**
     * @brief Requests a list of stop suggestions.
     *
     * When the stop list is completely received @ref stopListReceived gets emitted.
     * The default implementation does nothing.
     * @param request Information about the stop suggestion request.
     **/
    virtual void requestStopSuggestions( const StopSuggestionRequest &request );

    /** @brief Gets the information object used by this accessor. */
    const TimetableAccessorInfo &timetableAccessorInfo() const { return *m_info; };

    /** @brief Whether or not the city should be put into the "raw" url. */
    virtual bool useSeparateCityValue() const;

    /**
     * @brief Whether or not cities may be chosen freely.
     *
     * @return true if only cities in the list returned by cities()  are valid.
     * @return false (default) if cities may be chosen freely, but may be invalid.
     **/
    virtual bool onlyUseCitiesInList() const;

    /**
     * @brief Encodes the url in @p str using the charset in @p charset. Then it is percent encoded.
     *
     * @see charsetForUrlEncoding()
     **/
    static QString toPercentEncoding( const QString &str, const QByteArray &charset );

protected:
    /**
     * @brief Gets the charset used to encode urls before percent-encoding them. Normally
     *   this charset is UTF-8. But that doesn't work for sites that require parameters
     *   in the url (..&param=x) to be encoded in that specific charset.
     *
     * @see TimetableAccessor::toPercentEncoding()
     **/
    virtual QByteArray charsetForUrlEncoding() const;


    QString m_curCity; /**< @brief Stores the currently used city. */
    const TimetableAccessorInfo *const m_info; /**< @brief Stores service provider information.
            * This TimetableAccessorInfo object contains all static information needed by
            * TimetableAccessor. TimetableAccessor uses this object to request/receive
            * the correct data and execute the correct script for a specific service provider.
            * To get a non-const copy of this object, use TimetableAccessorInfo::clone(). */

signals:
    /**
     * @brief Emitted when a new departure or arrival list has been received and parsed.
     *
     * @param accessor The accessor that was used to download and parse the departures/arrivals.
     * @param requestUrl The url used to request the information.
     * @param journeys A list of departures / arrivals that were received.
     * @param request Information about the request for the just received departure/arrival list.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void departureListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
            const DepartureInfoList &journeys, const GlobalTimetableInfo &globalInfo,
            const DepartureRequest &request );

    /**
     * @brief Emitted when a new journey list has been received and parsed.
     *
     * @param accessor The accessor that was used to download and parse the journeys.
     * @param requestUrl The url used to request the information.
     * @param journeys A list of journeys that were received.
     * @param request Information about the request for the just received journey list.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void journeyListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
            const JourneyInfoList &journeys, const GlobalTimetableInfo &globalInfo,
            const JourneyRequest &request );

    /**
     * @brief Emitted when a list of stop names has been received and parsed.
     *
     * @param accessor The accessor that was used to download and parse the stops.
     * @param requestUrl The url used to request the information.
     * @param stops A pointer to a list of @ref StopInfo objects.
     * @param request Information about the request for the just received stop list.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void stopListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
            const StopInfoList &stops, const StopSuggestionRequest &request );

    /**
     * @brief Emitted when a session key has been received and parsed.
     *
     * @param accessor The accessor that was used to download and parse the session key.
     * @param sessionKey The parsed session key.
     **/
    void sessionKeyReceived( TimetableAccessor *accessor, const QString &sessionKey );

    /**
     * @brief Emitted when an error occurred while parsing.
     *
     * @param accessor The accessor that was used to download and parse information
     *   from the service provider.
     * @param errorCode The error code or NoError if there was no error.
     * @param errorString If @p errorCode isn't NoError this contains a
     *   description of the error.
     * @param requestUrl The url used to request the information.
     * @param request Information about the request that resulted in the error.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void errorParsing( TimetableAccessor *accessor, ErrorCode errorCode, const QString &errorString,
            const QUrl &requestUrl, const AbstractRequest *request );

    void forceUpdate();

protected:
    struct JobInfos {
        // Mainly for QHash
        JobInfos() : request(0) {
        };

        JobInfos( const KUrl &url, AbstractRequest *request ) : request(request) {
            this->url = url;
        };

        ~JobInfos();

        KUrl url;
        AbstractRequest *request;
    };

private:
    static QString gethex( ushort decimal );

    // Stores information about currently running download jobs
    QHash< KJob*, JobInfos > m_jobInfos;

    bool m_idAlreadyRequested;
};

#endif // TIMETABLEACCESSOR_HEADER
