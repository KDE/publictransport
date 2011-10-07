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
* @brief This file contains the base class for all accessors used by the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_HEADER
#define TIMETABLEACCESSOR_HEADER

#include "departureinfo.h"
#include "enums.h"
#include "timetableaccessor_info.h"

#include <KUrl>
#include <KIO/Scheduler>
#include <KIO/JobClasses>

#include <QHash>

/** @class TimetableAccessor
 * @brief Gets timetable information for public transport from different service providers.
 *
 * The easiest way to implement support for a new service provider is to add an XML file describing
 * the service provider and a script to parse timetable documents.
 * If that's not enough a new class can be derived from TimetableAccessor or it's derivates. These
 * functions should then be overwritten:
 *   - serviceProvider()
 *   - country(), cities()
 *   - rawUrl(), parseDocument() */
class TimetableAccessor : public QObject {
	Q_OBJECT

public:
	/**
	 * @brief Constructs a new TimetableAccessor object. You should use createAccessor()
	 *   to get an accessor that can download and parse documents from the given service provider. */
	explicit TimetableAccessor( TimetableAccessorInfo *info = new TimetableAccessorInfo() );
	virtual ~TimetableAccessor();

	/**
	 * @brief Gets a timetable accessor that is able to parse results from the given service provider.
	 *
	 * @param serviceProvider The ID of the service provider to get an accessor for.
	 *   The ID starts with a country code, followed by an underscore and it's name.
	 *   If it's empty, the default service provider for the users country will
	 *   be used, if there is any. */
	static TimetableAccessor *createAccessor( const QString &serviceProvider = QString() );

    /**
     * @brief Reads the XML file for the given @p serviceProvider.
     *
     * @param serviceProvider The ID of the service provider which XML file should be read.
     *   The ID starts with a country code, followed by an underscore and it's name.
     *   If it's empty, the default service provider for the users country will
     *   be used, if there is any. */
    static TimetableAccessorInfo *readAccessorInfo( const QString &serviceProvider = QString() );

	/** @brief Gets the AccessorType enumerable for the given string. */
	static AccessorType accessorTypeFromString( const QString &accessorType );

    /** @brief Gets the name for the given AccessorType enumerable. */
    static QString accessorTypeName( AccessorType accessorType );

	/** @brief Gets the TimetableInformation enumerable for the given string. */
	static TimetableInformation timetableInformationFromString( const QString &sTimetableInformation );

	/** @brief Gets the VehicleType enumerable for the given string. */
	static VehicleType vehicleTypeFromString( QString sVehicleType );

    /**
     * @brief Gets the name of the cache file for accessor infomation.
     *
     * The cache file can be used by accessors to store information about themselves that might
     * take some time to get if not stored. For example a network request might be needed to get
     * the information.
     * The cache file can also be used to store other information for the accessor, eg. the last
     * modified time of a file that gets only downloaded again, if it's last modified time is
     * higher.
     * KConfig is used to read from / write to the file.
     **/
    static QString accessorCacheFileName();

    /**
     * @brief Gets the AccessorType of this accessor.
     *
     * The default implementation returns @ref NoAccessor. Derived classes should overwrite this
     * to return the appropriate type.
     **/
    virtual AccessorType type() const { return NoAccessor; };

	/** @brief Gets the service provider ID the accessor is designed for. */
	virtual QString serviceProvider() const { return m_info->serviceProvider(); };

	/** @brief Gets the minimum seconds to wait between two data-fetches from the service provider. */
	virtual int minFetchWait() const { return m_info->minFetchWait(); };

	/** @brief Gets a list of features that this accessor supports. */
	virtual QStringList features() const;

	/** @brief Gets a list of short localized strings describing the supported features. */
	QStringList featuresLocalized() const;

	/** @brief Gets a list of features that this accessor supports through a script. */
	virtual QStringList scriptFeatures() const { return QStringList(); };

	/** @brief The country for which the accessor returns results. */
	virtual QString country() const { return m_info->country(); };

	/** @brief A list of cities for which the accessor returns results. */
	virtual QStringList cities() const { return m_info->cities(); };

	QString credit() const { return m_info->credit(); };

	const TimetableAccessorInfo *info() const { return m_info; };

	/** @brief Requests a list of departures/arrivals. When the departure/arrival list
	 *   is completely received @ref departureListReceived is emitted. */
	virtual void requestDepartures( const QString &sourceName,
			const QString &city, const QString &stop,
			int maxCount, const QDateTime &dateTime, const QString &dataType = "departures",
			bool useDifferentUrl = false );

	/**
	 * @brief Requests a session key. May be needed for some service providers to work properly.
	 *
	 * When the session key has been received @ref sessionKeyReceived is emitted.
	 *
	 * @param parseMode Can be ParseForSessionKeyThenStopSuggestions (calls
	 *   @ref requestStopSuggestions after the session key has been retrieved) or
	 *   ParseForSessionKeyThenDepartures (calls @ref requestDepartures after the session key has
	 *   been retrieved).
	 *
	 * @param url The url to a document which contains the session key.
	 *
	 * @param sourceName The source name in the data engine.
	 *
	 * @param city After the session key has been parsed, @ref requestDepartures is called
	 *   with @p city.
	 *
	 * @param stop After the session key has been parsed, @ref requestDepartures is called
	 *   with @p stop.
	 *
	 * @param maxCount After the session key has been parsed, @ref requestDepartures is called
	 *   with @p maxCount. Default is 99.
	 *
	 * @param dateTime After the session key has been parsed, @ref requestDepartures is called
	 *   with @p dateTime. Default is QDateTime::currentDateTime().
	 *
	 * @param dataType After the session key has been parsed, @ref requestDepartures is called
	 *   with @p dataType. Default is QString().
	 *
	 * @param usedDifferentUrl After the session key has been parsed, @ref requestDepartures is
	 *   called with @p usedDifferentUrl. Default is false.
	 **/
	virtual void requestSessionKey( ParseDocumentMode parseMode, const KUrl &url,
			const QString &sourceName, const QString &city, const QString &stop, int maxCount = 99,
			const QDateTime &dateTime = QDateTime::currentDateTime(),
			const QString &dataType = QString(), bool usedDifferentUrl = false );

	/**
	 * @brief Requests a list of stop suggestions. When the stop list is completely received
	 *   @ref stopListReceived is emitted.
	 *
	 * @param sourceName The source name in the data engine.
	 *
	 * @param city The city to get stop suggestions for (only needed if @ref useSeparateCityValue
	 *   returns true).
	 *
	 * @param stop The stop name (or a part of it) to get suggestions for.
	 *
	 * @param parseMode Can be ParseForStopSuggestions or ParseForStopIdThenDepartures (then the
	 *   arguments @p maxCount, @p dateTime, @p dataType, @p usedDifferentUrl are also needed and
	 *   used for the departure request once a stop suggestion has arrived).
	 *   Default is ParseForStopSuggestions.
	 *
	 * @param maxCount Only used if @p parseMode is ParseForStopIdThenDepartures. After stop
	 *   suggestions are retrieved, @ref requestDepartures is called with @p maxCount. Default is 1,
	 *   because only the best suggestion is used.
	 *
	 * @param dateTime Only used if @p parseMode is ParseForStopIdThenDepartures. After stop
	 *   suggestions are retrieved, @ref requestDepartures is called with @p dateTime. Default is
	 *   QDateTime::currentDateTime().
	 *
	 * @param dataType Only used if @p parseMode is ParseForStopIdThenDepartures. After stop
	 *   suggestions are retrieved, @ref requestDepartures is called with @p dataType. Default is
	 *   QString().
	 *
	 * @param usedDifferentUrl Only used if @p parseMode is ParseForStopIdThenDepartures. After stop
	 *   suggestions are retrieved, @ref requestDepartures is called with @p usedDifferentUrl.
	 *   Default is false.
	 **/
	virtual void requestStopSuggestions( const QString &sourceName,
			const QString &city, const QString &stop,
			ParseDocumentMode parseMode = ParseForStopSuggestions, int maxCount = 1,
			const QDateTime &dateTime = QDateTime::currentDateTime(),
			const QString &dataType = QString(), bool usedDifferentUrl = false );

	/** @brief Requests a list of journeys. When the journey list is completely received
	 * journeyListReceived() is emitted. */
	virtual void requestJourneys( const QString &sourceName,
			const QString &city, const QString &startStopName, const QString &targetStopName,
			int maxCount, const QDateTime &dateTime,
			const QString &dataType = "journeys", bool useDifferentUrl = false,
            const QString &urlToUse = QString(), int roundTrips = 0 );

	/** @brief Gets the information object used by this accessor. */
	const TimetableAccessorInfo &timetableAccessorInfo() const { return *m_info; };

	/** @brief Whether or not the city should be put into the "raw" url. */
	virtual bool useSeparateCityValue() const { return m_info->useSeparateCityValue(); };

	/**
	 * @brief Whether or not cities may be chosen freely.
	 *
	 * @return true if only cities in the list returned by cities()  are valid.
	 * @return false (default) if cities may be chosen freely, but may be invalid. */
	virtual bool onlyUseCitiesInList() const { return m_info->onlyUseCitiesInList(); };

	/**
	 * @brief Whether or not to use the url returned by differentRawUrl() instead of the
	 *   one returned by rawUrl().
	 * @see differentRawUrl() */
	bool hasSpecialUrlForStopSuggestions() const {
		return !m_info->stopSuggestionsRawUrl().isEmpty(); };

	/** @brief Returns a list of changelog entries.
	 *
	 * @see ChangelogEntry */
	QList<ChangelogEntry> changelog() const { return m_info->changelog(); };
	/** @brief Sets the list of changelog entries.
	 *
	 * @param changelog The new list of changelog entries.
	 *
	 * @see ChangelogEntry */
	void setChangelog( const QList<ChangelogEntry> &changelog ) {
		m_info->setChangelog( changelog );
	};

	/** @brief Encodes the url in @p str using the charset in @p charset. Then it is percent encoded.
	 *
	 * @see charsetForUrlEncoding() */
	static QString toPercentEncoding( const QString &str, const QByteArray &charset );

protected:
	/**
	 * @brief Parses the contents of a document and puts the results into @p journeys.
     *
     * This function is used by the default implementations of @ref requestDepartures,
     * @ref requestJourneys, etc.
     * The default implementation does nothing and returns false.
	 *
	 * @param journeys A pointer to a list of departure/arrival or journey information.
	 *   The results of parsing the document is stored in @p journeys.
	 * @param parseDocumentMode The mode of parsing, e.g. parse for
	 *   departures/arrivals or journeys.
	 * @return true, if there were no errors and the data in @p journeys is valid.
	 * @return false, if there were an error parsing the document.
     *
	 * @see parseDocumentPossibleStops()
     **/
	virtual bool parseDocument( const QByteArray &document,
			QList<PublicTransportInfo*> *journeys, GlobalTimetableInfo *globalInfo,
			ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals );

	/**
	 * @brief Override this method to parse the contents of a received document for
	 *   an url to a document containing later journeys. The default implementation
	 *   returns a null string.
	 * @return The parsed url.
     **/
	virtual QString parseDocumentForLaterJourneysUrl( const QByteArray &document ) {
		Q_UNUSED( document );
		return QString();
	};

	/**
	 * @brief Override this method to parse the contents of a received document for
	 *   an url to a document containing detailed journey information. The default
	 *   implementation returns a null string.
	 * @return The parsed url.
     **/
	virtual QString parseDocumentForDetailedJourneysUrl( const QByteArray &document ) {
		Q_UNUSED( document );
		return QString();
	};

	/**
	 * @brief Override this method to parse the contents of a received document for
	 *   a session key. The default implementation returns a null string.
	 * @return The parsed session key. */
	virtual QString parseDocumentForSessionKey( const QByteArray &document ) {
		Q_UNUSED( document );
		return QString();
	};

	/**
	 * @brief Parses the contents of a received document for a list of possible stop names
	 *   and puts the results into @p stops.
	 *
	 * @param stops A pointer to a list of @ref StopInfo objects.
	 * @return true, if there were no errors.
	 * @return false, if there were an error parsing the document.
     *
	 * @see parseDocument()
     **/
	virtual bool parseDocumentPossibleStops( const QByteArray &document, QList<StopInfo*> *stops );

	/**
	 * @brief Gets the "raw" url with placeholders for the city ("%1") and the stop ("%2")
	 *   or only for the stop ("%1") if putCityIntoUrl() returns false. */
	virtual QString departuresRawUrl() const;

	/**
	 * @brief Gets a second "raw" url with placeholders for the city ("%1") and the stop ("%2")
	 *   or only for the stop ("%1") if putCityIntoUrl() returns false. */
	virtual QString stopSuggestionsRawUrl() const;

	/**
	 * @brief Gets the charset used to encode urls before percent-encoding them. Normally
	 *   this charset is UTF-8. But that doesn't work for sites that require parameters
	 *   in the url (..&param=x) to be encoded in that specific charset.
	 *
	 * @see TimetableAccessor::toPercentEncoding() */
	virtual QByteArray charsetForUrlEncoding() const;

	/**
	 * @brief Constructs an url to the departure / arrival list by combining the "raw"
	 *   url with the needed information. */
	KUrl getUrl( const QString &city, const QString &stop, int maxCount, const QDateTime &dateTime,
			const QString &dataType = "departures", bool useDifferentUrl = false ) const;

	/**
	 * @brief Constructs an url to a page containing stop suggestions by combining
	 *   the "raw" url with the needed information. */
	KUrl getStopSuggestionsUrl( const QString &city, const QString &stop );

	/**
	 * @brief Constructs an url to the journey list by combining the "raw" url with the
	 *   needed information. */
	KUrl getJourneyUrl( const QString &city, const QString &startStopName,
			const QString &targetStopName, int maxCount, const QDateTime &dateTime,
			const QString &dataType = "departures", bool useDifferentUrl = false ) const;


	QString m_curCity; /**< @brief Stores the currently used city. */
	TimetableAccessorInfo *m_info; /**< @brief Stores service provider specific information that is
									 * used to parse the documents from the service provider. */

signals:
	/**
	 * @brief Emitted when a new departure or arrival list has been received and parsed.
	 *
	 * @param accessor The accessor that was used to download and parse the departures/arrivals.
	 * @param requestUrl The url used to request the information.
	 * @param journeys A list of departures / arrivals that were received.
	 * @param serviceProvider The service provider the data came from.
	 * @param sourceName The name of the data source for which the departures/arrivals
	 *   have been downloaded and parsed.
	 * @param city The city the stop is in. May be empty if the service provider
	 *   doesn't need a separate city value.
	 * @param stop The stop name for which the departures / arrivals have been received.
	 * @param dataType "departures" or "arrivals".
	 * @param parseDocumentMode What has been parsed from the document.
	 * @see TimetableAccessor::useSeperateCityValue() */
	void departureListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
			const QList<DepartureInfo*> &journeys, const GlobalTimetableInfo &globalInfo,
			const QString &serviceProvider, const QString &sourceName, const QString &city,
			const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode );

	/**
	 * @brief Emitted when a new journey list has been received and parsed.
	 *
	 * @param accessor The accessor that was used to download and parse the journeys.
	 * @param requestUrl The url used to request the information.
	 * @param journeys A list of journeys that were received.
	 * @param serviceProvider The service provider the data came from.
	 * @param sourceName The name of the data source for which the journeys have
	 *   been downloaded and parsed.
	 * @param city The city the stop is in. May be empty if the service provider
	 *   doesn't need a separate city value.
	 * @param stop The stop name for which the departures / arrivals have been received.
	 * @param dataType "journeys".
	 * @param parseDocumentMode What has been parsed from the document.
	 * @see TimetableAccessor::useSeperateCityValue() */
	void journeyListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
			const QList<JourneyInfo*> &journeys, const GlobalTimetableInfo &globalInfo,
			const QString &serviceProvider, const QString &sourceName,
			const QString &city, const QString &stop,
			const QString &dataType, ParseDocumentMode parseDocumentMode );

	/**
	 * @brief Emitted when a list of stop names has been received and parsed.
	 *
	 * @param accessor The accessor that was used to download and parse the stops.
	 * @param requestUrl The url used to request the information.
	 * @param stops A pointer to a list of @ref StopInfo objects.
	 * @param serviceProvider The service provider the data came from.
	 * @param sourceName The name of the data source for which the stops have been
	 *   downloaded and parsed.
	 * @param city The city the (ambiguous) stop is in. May be empty if the service provider
	 *   doesn't need a separate city value.
	 * @param stop The (ambiguous) stop name for which the stop list has been received.
	 * @param dataType "stopList".
	 * @param parseDocumentMode What has been parsed from the document.
	 * @see TimetableAccessor::useSeperateCityValue() */
	void stopListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
			const QList<StopInfo*> &stops,
			const QString &serviceProvider, const QString &sourceName, const QString &city,
			const QString &stop, const QString &dataType, ParseDocumentMode parseDocumentMode );

	/**
	 * @brief Emitted when a session key has been received and parsed.
	 *
	 * @param accessor The accessor that was used to download and parse the session key.
	 * @param sessionKey The parsed session key. */
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
	 * @param serviceProvider The service provider the data came from.
	 * @param sourceName The name of the data source.
	 * @param city The city the stop is in. May be empty if the service provider
	 *   doesn't need a separate city value.
	 * @param stop The stop name for which the error occurred.
	 * @param dataType "nothing".
	 * @param parseDocumentMode What has been parsed from the document.
	 * @see TimetableAccessor::useSeperateCityValue() */
	void errorParsing( TimetableAccessor *accessor, ErrorCode errorCode, const QString &errorString,
			const QUrl &requestUrl, const QString &serviceProvider,
			const QString &sourceName, const QString &city, const QString &stop,
			const QString &dataType, ParseDocumentMode parseDocumentMode );

    /**
     * @brief Reports progress of the accessor in performing an action.
     *
     * TODO
     *
     * @param progress A value between 0 (just started) and 1 (completed) indicating progress.
     **/
    void progress( TimetableAccessor *accessor, qreal progress, const QString &jobDescription,
            const QUrl &requestUrl, const QString &serviceProvider,
            const QString &sourceName, const QString &city, const QString &stop,
            const QString &dataType, ParseDocumentMode parseDocumentMode );

protected slots:
	/** @brief All data of a journey list has been received. */
	void result( KJob* job );

	/** @brief Clears the session key. This gets called from time to time by a timer, to enforce
	 * the download of a new session key the next it's needed. This is done to prevent usage of
	 * expired session keys. */
	void clearSessionKey();

protected:
	// Stores a session key, if it's needed by the accessor
	QString m_sessionKey;
	QTime m_sessionKeyGetTime;

	struct JobInfos {
        // Mainly for QHash
	    JobInfos() {
            this->parseDocumentMode = ParseInvalid;
            this->maxCount = -1;
            this->usedDifferentUrl = false;
            this->roundTrips = 0;
		};

	    JobInfos( ParseDocumentMode parseDocumentMode, const QString &sourceName,
				  const QString &city, const QString &stop, const QUrl &url,
				  const QString &dataType = QString(), int maxCount = -1,
				  const QDateTime &dateTime = QDateTime(), bool useDifferentUrl = false,
				  const QString &targetStop = QString(), int roundTrips = 0 ) {
			this->parseDocumentMode = parseDocumentMode;
			this->sourceName = sourceName;
			this->city = city;
			this->stop = stop;
			this->url = url;
			this->dataType = dataType;
			this->maxCount = maxCount;
			this->dateTime = dateTime;
			this->usedDifferentUrl = useDifferentUrl;
			this->targetStop = targetStop;
			this->roundTrips = roundTrips;
	    };

	    ParseDocumentMode parseDocumentMode;
	    QString sourceName;
	    QString city;
	    QString stop;
	    QString dataType;
	    QUrl url;
	    int maxCount;
	    QDateTime dateTime;
	    bool usedDifferentUrl;
	    QString targetStop;
	    int roundTrips;
	};

private:
    static QString gethex( ushort decimal );

	bool m_idAlreadyRequested;

    // Stores information about currently running download jobs
    QHash< KJob*, JobInfos > m_jobInfos;

//     qint64 m_lastUsed; TODO
};

#endif // TIMETABLEACCESSOR_HEADER
