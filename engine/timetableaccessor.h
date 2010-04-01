/*
 *   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#include <KUrl>
#include "kio/scheduler.h"
#include "kio/jobclasses.h"

#include <QHash>

#include "departureinfo.h"
#include "enums.h"
#include "timetableaccessor_htmlinfo.h"

/** @class TimetableAccessor
* @brief Gets timetable information for public transport from different service providers.
* Gets timetable information for public transport from different service providers.
* The easiest way to implement support for a new service provider is to derive a new class
* from TimetableAccessorInfo and use it with TimetableAccessorHtml.
* To implement support for a new class of service providers create a new class based on
* TimetableAccessor or it's derivates and overwrite
*	- serviceProvider()
* 	- country(), cities()
* 	- rawUrl(), parseDocument() */
class TimetableAccessor : public QObject {
    Q_OBJECT

    public:
	/** Constructs a new TimetableAccessor object. You should use getSpecificAccessor()
	* to get an accessor that can download and parse documents from the given service
	* provider. */
	TimetableAccessor() {};
        virtual ~TimetableAccessor() {};

	/** Gets a timetable accessor that is able to parse results from the given service provider. */
	static TimetableAccessor *getSpecificAccessor( const QString &serviceProvider );

	/** Gets the AccessorType enumerable for the given string. */
	static AccessorType accessorTypeFromString( const QString &sAccessorType );

	/** Gets the TimetableInformation enumerable for the given string. */
	static TimetableInformation timetableInformationFromString(
		const QString &sTimetableInformation );

	/** Gets the VehicleType enumerable for the given string. */
	static VehicleType vehicleTypeFromString( QString sVehicleType );
	
	/** Gets the service provider the accessor is designed for. */
	virtual QString serviceProvider() const { return m_info.serviceProvider(); };
	
	/** Gets the minimum seconds to wait between two data-fetches from the
	* service provider. */
	virtual int minFetchWait() const { return m_info.minFetchWait(); };

	/** Gets a list of features that this accessor supports. */
	virtual QStringList features() const;

	/** Gets a list of short localized strings describing the supported features. */
	QStringList featuresLocalized() const;

	/** Gets a list of features that this accessor supports through a script. */
	virtual QStringList scriptFeatures() const { return QStringList(); };

	/** The country for which the accessor returns results. */
	virtual QString country() const { return m_info.country(); };

	/** A list of cities for which the accessor returns results. */
	virtual QStringList cities() const { return m_info.cities(); };

	QString credit() const { return m_info.credit(); };

	/** Requests a list of departures / arrivals. When the departure / arrival list
	* is completely received departureListReceived() is emitted. */
	KIO::StoredTransferJob *requestDepartures( const QString &sourceName,
					     const QString &city, const QString &stop,
					     int maxDeps, const QDateTime &dateTime,
					     const QString &dataType = "departures",
					     bool useDifferentUrl = false );

	KIO::StoredTransferJob *requestStopSuggestions( const QString &sourceName,
						  const QString &city,
						  const QString &stop );

	/** Requests a list of journeys. When the journey list is completely received
	* journeyListReceived() is emitted. */
	KIO::StoredTransferJob *requestJourneys( const QString &sourceName,
					   const QString &city,
					   const QString &startStopName,
					   const QString &targetStopName,
					   int maxDeps, const QDateTime &dateTime,
					   const QString &dataType = "journeys",
					   bool useDifferentUrl = false );
					   
	KIO::StoredTransferJob *requestJourneys( const KUrl &url );

	/** Gets the information object used by this accessor. */
	const TimetableAccessorInfo &timetableAccessorInfo() const { return m_info; };

	/** Wheather or not the city should be put into the "raw" url. */
	virtual bool useSeparateCityValue() const { return m_info.useSeparateCityValue(); };

	/** Wheather or not cities may be chosen freely.
	* @return true if only cities in the list returned by cities()  are valid.
	* @return false (default) if cities may be chosen freely, but may be invalid. */
	virtual bool onlyUseCitiesInList() const { return m_info.onlyUseCitiesInList(); };

	/** Wheather or not to use the url returned by differentRawUrl() instead of the
	* one returned by rawUrl().
	* @see differentRawUrl() */
	bool hasSpecialUrlForStopSuggestions() const {
		return !m_info.stopSuggestionsRawUrl().isEmpty(); };

	/** Encodes the url in @p str using the charset in @p charset. Then it is
	* percent encoded.
	* @see charsetForUrlEncoding() */
	static QString toPercentEncoding( QString str, QByteArray charset );

    protected:
	/** Parses the contents of a document that was requested using requestJourneys()
	* and puts the results into @p journeys..
	* @param journeys A pointer to a list of departure/arrival or journey information.
	* The results of parsing the document is stored in @p journeys.
	* @param parseDocumentMode The mode of parsing, e.g. parse for
	* departures/arrivals or journeys.
	* @return true, if there were no errors and the data in @p journeys is valid.
	* @return false, if there were an error parsing the document.
	* @see parseDocumentPossibleStops() */
	virtual bool parseDocument( const QByteArray &document,
				    QList<PublicTransportInfo*> *journeys,
				    GlobalTimetableInfo *globalInfo,
				    ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals );

	/** Override this method to parse the contents of a received document for
	* an url to a document containing later journeys. The default implementation 
	* returns a null string.
	* @return The parsed url. */
	virtual QString parseDocumentForLaterJourneysUrl( const QByteArray &document ) {
		Q_UNUSED( document );
		return QString(); };
	
	/** Override this method to parse the contents of a received document for
	* an url to a document containing detailed journey information. The default
	* implementation returns a null string.
	* @return The parsed url. */
	virtual QString parseDocumentForDetailedJourneysUrl( const QByteArray &document ) {
		Q_UNUSED( document );
		return QString(); };
				    
	/** Parses the contents of a received document for a list of possible stop names
	* and puts the results into @p stops.
	* @param stops A pointer to a string list, where the stop names are stored.
	* @param stopToStopId A pointer to a map, where the keys are stop names
	* and the values are stop IDs.
	* @return true, if there were no errors.
	* @return false, if there were an error parsing the document.
	* @see parseDocument() */
	virtual bool parseDocumentPossibleStops( const QByteArray &document,
						 QStringList *stops,
						 QHash<QString,QString> *stopToStopId,
						 QHash<QString,int> *stopToStopWeight );

	/** Gets the "raw" url with placeholders for the city ("%1") and the stop ("%2")
	* or only for the stop ("%1") if putCityIntoUrl() returns false. */
	virtual QString departuresRawUrl() const;

	/** Gets a second "raw" url with placeholders for the city ("%1") and the stop ("%2")
	* or only for the stop ("%1") if putCityIntoUrl() returns false. */
	virtual QString stopSuggestionsRawUrl() const;

	/** Gets the charset used to encode urls before percent-encoding them. Normally
	* this charset is UTF-8. But that doesn't work for sites that require parameters
	* in the url (..&param=x) to be encoded in that specific charset.
	* @see TimetableAccessor::toPercentEncoding() */
	virtual QByteArray charsetForUrlEncoding() const;

	/** Constructs an url to the departure / arrival list by combining the "raw"
	* url with the needed information. */
	KUrl getUrl( const QString &city, const QString &stop, int maxDeps,
		     const QDateTime &dateTime,
		     const QString &dataType = "departures",
		     bool useDifferentUrl = false ) const;

	/** Constructs an url to a page containing stop suggestions by combining 
	* the "raw" url with the needed information. */
	KUrl getStopSuggestionsUrl( const QString &city, const QString &stop );
		     
	/** Constructs an url to the journey list by combining the "raw" url with the
	* needed information. */
	KUrl getJourneyUrl( const QString &city, const QString &startStopName,
			    const QString &targetStopName, int maxDeps,
			    const QDateTime &dateTime,
			    const QString &dataType = "departures",
			    bool useDifferentUrl = false ) const;

	QString m_curCity; /**< Stores the currently used city. */
	TimetableAccessorInfo m_info; /**< Stores service provider specific information that is used to parse the html pages. */

    signals:
	/** Emitted when a new departure or arrival list has been received and parsed.
	* @param accessor The accessor that was used to download and parse the
	* departures / arrivals.
	* @param requestUrl The url used to request the information.
	* @param journeys A list of departures / arrivals that were received.
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source for which the departures /
	* arrivals have been downloaded and parsed.
	* @param city The city the stop is in. May be empty if the service provider
	* doesn't need a separate city value.
	* @param stop The stop name for which the departures / arrivals have been received.
	* @param dataType "departures" or "arrivals".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeperateCityValue() */
	void departureListReceived( TimetableAccessor *accessor,
				    const QUrl &requestUrl,
				    const QList<DepartureInfo*> &journeys,
				    const GlobalTimetableInfo &globalInfo,
				    const QString &serviceProvider,
				    const QString &sourceName, const QString &city,
				    const QString &stop, const QString &dataType,
				    ParseDocumentMode parseDocumentMode );

	/** Emitted when a new journey list has been received and parsed.
	* @param accessor The accessor that was used to download and parse the journeys.
	* @param requestUrl The url used to request the information.
	* @param journeys A list of journeys that were received.
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source for which the journeys have
	* been downloaded and parsed.
	* @param city The city the stop is in. May be empty if the service provider
	* doesn't need a separate city value.
	* @param stop The stop name for which the departures / arrivals have been received.
	* @param dataType "journeys".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeperateCityValue() */
	void journeyListReceived( TimetableAccessor *accessor,
				  const QUrl &requestUrl,
				  const QList<JourneyInfo*> &journeys,
				  const GlobalTimetableInfo &globalInfo,
				  const QString &serviceProvider,
				  const QString &sourceName,
				  const QString &city, const QString &stop,
				  const QString &dataType,
				  ParseDocumentMode parseDocumentMode );

	/** Emitted when a list of stop names has been received and parsed.
	* @param accessor The accessor that was used to download and parse the stops.
	* @param requestUrl The url used to request the information.
	* @param stops A string list containing the received stop names.
	* @param stopToStopId A QHash containing the received stop names as keys 
	* and the stop IDs as values (stop IDs may be empty).
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source for which the stops have been
	* downloaded and parsed.
	* @param city The city the (ambiguous) stop is in. May be empty if the service provider
	* doesn't need a separate city value.
	* @param stop The (ambiguous) stop name for which the stop list has been received.
	* @param dataType "stopList".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeperateCityValue() */
	void stopListReceived( TimetableAccessor *accessor,
			       const QUrl &requestUrl,
			       const QStringList &stops,
			       const QHash<QString, QString> &stopToStopId,
			       const QHash<QString, int> &stopToStopWeight,
			       const QString &serviceProvider,
			       const QString &sourceName, const QString &city,
			       const QString &stop, const QString &dataType,
			       ParseDocumentMode parseDocumentMode );

	/** Emitted when an error occurred while parsing.
	* @param accessor The accessor that was used to download and parse information
	* from the service provider.
	* @param errorType The type of error or NoError if there was no error.
	* @param errorString If @p errorType isn't NoError this contains a 
	* description of the error.
	* @param requestUrl The url used to request the information.
	* @param serviceProvider The service provider the data came from.
	* @param sourceName The name of the data source.
	* @param city The city the stop is in. May be empty if the service provider
	* doesn't need a separate city value.
	* @param stop The stop name for which the error occured.
	* @param dataType "nothing".
	* @param parseDocumentMode What has been parsed from the document.
	* @see TimetableAccessor::useSeperateCityValue() */
	void errorParsing( TimetableAccessor *accessor,
			   ErrorType errorType,
			   const QString &errorString,
			   const QUrl &requestUrl,
			   const QString &serviceProvider,
			   const QString &sourceName, const QString &city,
			   const QString &stop, const QString &dataType,
			   ParseDocumentMode parseDocumentMode );

    public slots:
	/** All data of a journey list has been received. */
	void result( KJob* job );

    private:
	static QString gethex( ushort decimal );

	struct JobInfos {
	    JobInfos() {
	    };
	    
	    JobInfos( ParseDocumentMode parseDocumentMode,
		      const QString &sourceName, const QString &city,
		      const QString &stop, const QUrl &url,
		      const QString &dataType = QString(),
		      int maxDeps = -1, const QDateTime &dateTime = QDateTime(),
		      bool useDifferentUrl = false,
		      const QString &targetStop = QString(), int roundTrips = 0 ) {
		this->parseDocumentMode = parseDocumentMode;
		this->sourceName = sourceName;
		this->city = city;
		this->stop = stop;
		this->url = url;
		this->dataType = dataType;
		this->maxDeps = maxDeps;
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
	    int maxDeps;
	    QDateTime dateTime;
	    bool usedDifferentUrl;
	    QString targetStop;
	    int roundTrips;
	};
	
	// Stores information about currently running download jobs
	QHash< KJob*, JobInfos > m_jobInfos;
};

#endif // TIMETABLEACCESSOR_HEADER
