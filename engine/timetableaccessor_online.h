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
* @brief This file contains the base class for accessors that download documents and then parse them.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_ONLINE_HEADER
#define TIMETABLEACCESSOR_ONLINE_HEADER

#include "timetableaccessor.h"

/**
 * @brief Abstract base class for accessors that need to download documents and then parse them.
 *
 * It implements requestDepartures(), requestJourneys(), requestStopSuggestions() and
 * requestSessionKey() and requests associated source documents from the service provider.
 * The request...() functions do not block, ie. return immediately and wait for the requested data
 * in the background. When a document is downloaded parseDocument() or
 * parseDocumentForStopSuggestions() gets called with the content of the document as a QByteArray.
 * Once the timetable data is available parseDocument(), parseDocumentForStopSuggestions() or
 * parseDocumentForSessionKey() gets called. If the parsing function returns true (success)
 * departureListReceived(), journeyListReceived() or stopListReceived() gets emitted.
 * On errors errorParsing() gets emitted instead of the ...Received signal.
 * A session key gets requested automatically by this class if needed. Once it is available the
 * real request gets started using the session key.
 *
 * This class supports requests using GET and POST methods. What method gets used is specified in
 * the accessor XML file.
 *
 * Use info() to get a pointer to a TimetableAccessorInfo object with information about the service
 * provider. The information in that object is read from an XML file.
 *
 * @note If a derived accessor class would only be used for one service provider consider using
 *   TimetableAccessorScript instead and only implement the parsing code in a separate script.
 **/
class TimetableAccessorOnline : public TimetableAccessor {
    Q_OBJECT

public:
    /**
     * @brief Constructs a new TimetableAccessorOnline object.
     *
     * You should use createAccessor() to get an accessor for a given service provider ID.
     *
     * @param info An object containing information about the service provider.
     * @param parent The parent QObject.
     **/
    explicit TimetableAccessorOnline( TimetableAccessorInfo *info, QObject *parent = 0 );

    /**
     * @brief Requests a list of departures/arrivals.
     *
     * When the departure/arrival document is completely received parseDocument() gets called.
     * If true gets returned by parseDocument() the signal departureListReceived() gets then
     * emitted.
     *
     * @param requestInfo Information about the departure/arrival request.
     **/
    virtual void requestDepartures( const DepartureRequestInfo &requestInfo );

    /**
     * @brief Requests a list of journeys.
     *
     * When the journey document is completely received parseDocument() gets called. If true gets
     * returned by parseDocument() the signal journeyListReceived() gets then emitted.
     *
     * @param requestInfo Information about the journey request.
     **/
    virtual void requestJourneys( const JourneyRequestInfo &requestInfo );

    /**
     * @brief Requests a list of stop suggestions.
     *
     * When the stop suggestion document is completely received parseDocumentForStopSuggestions()
     * gets called. If true gets returned by parseDocumentForStopSuggestions() the signal
     * stopListReceived() gets then emitted.
     *
     * @param requestInfo Information about the stop suggestion request.
     **/
    virtual void requestStopSuggestions( const StopSuggestionRequestInfo &requestInfo );

    /**
     * @brief Requests a session key. May be needed for some service providers to work properly.
     *
     * When the session key document is completely received parseDocumentForSessionKey() gets
     * called. If true gets returned by parseDocumentForStopSuggestions() the signal
     * sessionKeyReceived() gets emitted and the real request gets started using the just
     * received session key.
     *
     * @param requestInfo Information about a request, that gets executed when the session key
     *   is known. If for example requestInfo has information about a departure request, after
     *   getting the session key, @p requestDepartures gets called with this requestInfo object.
     **/
    virtual void requestSessionKey( const RequestInfo *requestInfo );

protected slots:
    /** @brief All data of a journey list has been received. */
    void result( KJob* job );

    /**
     * @brief Clears the session key.
     *
     * This gets called from time to time by a timer, to prevent using expired session keys.
     **/
    void clearSessionKey();

protected:
    /** @brief Contains information about a timetable data request job. */
    struct JobInfos {
        // Mainly for QHash
        JobInfos() : requestInfo(0)
        {
        };

        JobInfos( const KUrl &url, RequestInfo *requestInfo )
                : requestInfo(requestInfo)
        {
            this->url = url;
        };

        ~JobInfos()
        {
        };

        KUrl url;
        QSharedPointer<RequestInfo> requestInfo;
    };

    /**
     * @brief Parses the contents of a document and puts the results into @p journeys.
     *
     * This function is used by TimetableAccessorOnline's implementations of
     * @ref requestDepartures, @ref requestJourneys, etc.
     * The default implementation does nothing and returns false.
     * Override this function to parse @p document for departures/arrivals/journeys. The timetable
     * items should be put into @p journeys.
     *
     * @param document The contents of the document that should be parsed.
     * @param journeys A pointer to a list of departure/arrival or journey information.
     *   The results of parsing the document is stored in @p journeys.
     * @param globalInfo Information for all items get stored in this object.
     * @param parseDocumentMode The mode of parsing, e.g. parse for
     *   departures/arrivals or journeys.
     * @return true, if there were no errors and the data in @p journeys is valid.
     * @return false, if there were an error parsing the document.
     *
     * @see parseDocumentForStopSuggestions()
     **/
    virtual bool parseDocument( const QByteArray &document,
            QList<PublicTransportInfo*> *journeys, GlobalTimetableInfo *globalInfo,
            ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals ) = 0;

    /**
     * @brief Parses @p document for a list of stop suggestions.
     *
     * The default implementation returns false.
     * Override this function if the service provider offers stop suggestions. The parsed stop
     * suggestions should be put into @p stops.
     *
     * @param document The contents of the document that should be parsed.
     * @param stops A pointer to a list of @ref StopInfo objects.
     * @return true, if there were no errors.
     * @return false, if there were an error parsing the document.
     *
     * @see parseDocument()
     **/
    virtual bool parseDocumentForStopSuggestions( const QByteArray &document,
                                                  QList<StopInfo*> *stops ) = 0;

    /**
     * @brief Parses @p document for an URL to a document containing later journeys.
     *
     * The default implementation returns an empty string.
     * Override this function if the service provider uses another URL for later journey.
     *
     * @param document The contents of the document that should be parsed.
     * @return The parsed URL.
     **/
    virtual QString parseDocumentForLaterJourneysUrl( const QByteArray &document ) {
        Q_UNUSED( document );
        return QString();
    };

    /**
     * @brief Parses @p document for an URL to a document containing detailed journey information.
     *
     * The default implementation returns an empty string.
     * Override this function if the service provider uses another URL for more journey details.
     *
     * @param document The contents of the document that should be parsed.
     * @return The parsed url.
     **/
    virtual QString parseDocumentForDetailedJourneysUrl( const QByteArray &document ) {
        Q_UNUSED( document );
        return QString();
    };

    /**
     * @brief Parses @p document for a session key.
     *
     * The default implementation returns an empty string.
     * Override this function if the service provider needs a session key.
     *
     * @param document The contents of the document that should be parsed.
     * @return The parsed session key.
     **/
    virtual QString parseDocumentForSessionKey( const QByteArray &document ) {
        Q_UNUSED( document );
        return QString();
    };

    /**
     * @brief Constructs an URL to a document containing a departure/arrival list
     *
     * Uses the template "raw" URL for departures and replaces placeholders with the needed
     * information.
     * This class first calls TimetableAccessor::departureUrl() and then additionaly replaces the
     * <em>"{sessionKey}"</em> placeholder with the session key, if needed.
     *
     * @param requestInfo Information about the departure/arrival request to get a source URL for.
     **/
    virtual KUrl departureUrl( const DepartureRequestInfo &requestInfo ) const;

    /** @brief Stores a session key, if it's needed by the accessor. */
    QString m_sessionKey;

    /** @brief Stores the time at which the session key was last updated. */
    QTime m_sessionKeyGetTime;

    /** @brief Stores information about currently running download jobs. */
    QHash< KJob*, JobInfos > m_jobInfos;

    /** @brief Stores whether or not a stop ID was requested, awaiting the result. */
    bool m_stopIdRequested;

    /** @brief Stores the currently used city. */
    QString m_curCity;
};

#endif // Multiple inclusion guard
