/*
 *   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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
* @brief This file contains TimetableAccessorInfo, which is the base class of all service provider information classes.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_HTMLINFO_HEADER
#define TIMETABLEACCESSOR_HTMLINFO_HEADER

#include "enums.h"
#include <QList>
#include <QString>
#include <QStringList>
#include <QHash>

/** @class TimetableRegExpSearch
* @brief Stores a regular expression and information about the meaning of the matches.
*  */
class TimetableRegExpSearch
{
    public:
	/** Creates a new TimetableRegExpSearch object with an empty regular expression. */
	TimetableRegExpSearch();

	/** Creates a new TimetableRegExpSearch object.
	* @param regExpSearch The regular expression of this search.
	* @param regExpInfos A list of meanings for each matched string of the regular expression. */
	TimetableRegExpSearch( QString regExpSearch, QList< TimetableInformation > regExpInfos );

	/** Wheather or not the regular expression is valid.
	* @return true, if the regular expression is valid.
	* @return false, if the regular expression is invalid. */
	bool isValid() const { return m_regExpSearch.isValid(); };

	/** Gets the regular expression of this search. */
	const QRegExp& regExp() const {
	    return m_regExpSearch;
	};

	/** Gets a list of meanings for each matched string of the regular expression. */
	QList< TimetableInformation > infos () const { return m_regExpInfos; };

    private:
	QRegExp m_regExpSearch;
	QList< TimetableInformation > m_regExpInfos;
};

/** @class TimetableAccessorInfo
* This is the base class of all service provider information classes. It is used
* by TimetableAccessor to downlaod and parse documents from different
* service providers.
* @brief Provides information about how to download and parse documents from service providers. */
class TimetableAccessorInfo
{
    friend class TimetableAccessor; // Because TimetableAccessor needs to set values when reading xml files

    public:
	/** Creates a new TimetableAccessorInfo object.
	* TODO: Don't use so many parameters in the constructor. The setters need to be called anyway.
	* @param name The name of the accessor.
	* @param shortUrl A short version of the url to the service provider home page. This can be used by the visualization as displayed text of links.
	* @param author The author of the accessor.
	* @param email The email address of the author given in @p author.
	* @param version The version of the accessor information.
	* @param serviceProviderID The service provider for which this accessor is designed for.
	* @param accessorType The type of the accessor.
	* @see AccessorType */
	TimetableAccessorInfo( const QString& name = QString(), const QString& shortUrl = QString(), const QString& author = QString(), const QString& email = QString(), const QString& version = QString(), const QString& serviceProviderID = QString(), const AccessorType& accessorType = NoAccessor );

// 	virtual ~TimetableAccessorInfo() {
// 	    if ( searchJourneysPre )
// 	    {
// 		delete searchJourneysPre;
// 		searchJourneysPre = NULL;
// 	    }
// 	};

	/** Gets the name of this accessor, which can be displayed by the visualization. */
	QString name() const;
	/** If empty, use unicode (QUrl::toPercentEncoding()), otherwise use own toPercentEncoding() with this charset. */
	QByteArray charsetForUrlEncoding() const;
	/** Type of the accessor (HTML, XML) */
	AccessorType accessorType() const;
	/** Raw url to an xml file for xml accessors */
	QString stopSuggestionsRawUrl() const;
	/** A description of the service provider. */
	QString description() const;
	/** The version of the accessor information. */
	QString version() const;
	/** The author of the accessor information to be used by the accessor. */
	QString author() const;
	/** The email address of the author. */
	QString email() const;
	/** The main/home url of the service provider. */
	QString url() const;
	/** A short version of the url without protocol or "www"  to be displayed in links. */
	QString shortUrl() const;
	/** A raw url that is used to get departures. */
	QString departureRawUrl() const;
	/** A raw url that is used to get journeys. */
	QString journeyRawUrl() const;

	/** The service provider this accessor is designed for. */
	QString serviceProvider() const;

	/** The country for which the service provider has data. */
	QString country() const;
	/** A list of cities for which the service provider has data. */
	QStringList cities() const;
	VehicleType defaultVehicleType() const;
	/** Gets the TimetableRegExpSearch object used to search for departures / arrivals. */
	TimetableRegExpSearch searchDepartures() const;
	/** Gets the TimetableRegExpSearch object used to search for journeys. */
	TimetableRegExpSearch searchJourneys() const;
	/** Gets a pointer to the TimetableRegExpSearch object used to preparse the
	* document, before it gets parsed with the TimetableRegExpSearch object returned
	* by searchDepartures() or searchJourneys().
	* @return NULL if there is no regular expression defined for preparsing the document.*/
	TimetableRegExpSearch *searchDeparturesPre() const;
	/** Gets a pointer to the TimetableRegExpSearch object used to parse the
	* document for split points, before it gets parsed with the TimetableRegExpSearch object returned
	* by searchDepartures() or searchJourneys(). TODO: Documentation
	* @return NULL if there is no regular expression defined for preparsing the document.*/
	TimetableRegExpSearch *searchDepartureGroupTitles() const;
	/** Gets a list of regular expression patterns used to search for a substring
	* in the document which contains a list of possible stops. */
	QStringList regExpSearchPossibleStopsRanges() const;
	/** Gets a list of TimetableRegExpSearch objects used to parse a list of
	* possible stops from a string matched by one of the regular expressions
	* returned by regExpSearchPossibleStopsRanges(). */
	QList<TimetableRegExpSearch> searchPossibleStops() const;
	/** Gets a list of TimetableRegExpSearch objects used to parse additional
	* information from a field with the meaning TimetableInformation::JourneyNews. */
	QList<TimetableRegExpSearch> searchJourneyNews() const;
	/** Wheather or not the service provider needs a seperate city value. */
	bool useSeperateCityValue() const;
	/** Wheather or not cities may be chosen freely.
	* @return true if only cities in the list returned by cities()  are valid.
	* @return false (default) if cities may be chosen freely, but may be invalid. */
	bool onlyUseCitiesInList() const;
	/** Gets a value for the given city that is used by the service provider.
	* @returns Either the value for the given city if it exists, or @p city itself. */
	QString mapCityNameToValue( const QString &city ) const;
	/** The name of the XML file that was parsed to get this accessor information object. */
	QString fileName() const;
	/** A list of features this accessor supports. */
	QStringList features() const;
	/** Wheather or not this accessor supports stop name autocompletion. */
	bool supportsStopAutocompletion() const;
	/** Wheather or not this accessor supports the given TimetableInformation. */
	bool supportsTimetableAccessorInfo( const TimetableInformation &info ) const;

    protected:
	/** Sets a regular expression for parsing an html document for a list of
	* departures / arrivals. A list of TimetableInformation values is needed to
	* know what's the meaning of each matched string of the regular expression.
	* @note You can also set another regular expression to be executed before
	* executing the actual regular expression. The data matched by this one is
	* then used if the actual one doesn't provide it. It needs two TimetableInformation
	* values, one is used as the key and one as the value, to create a map where
	* the keys (known from regExpSearch) point to values (not known from
	* regExpSearch). That map is then used to fill in missing data.
	* @param regExpSearch The regular expression used to parse a document for
	* departures / arrivals.
	* @param regExpInfos A list of TimetableInformation values that describe what
	* each match of the regular expression in @p regExpSearch means.
	* @param regExpSearchPre A regular expression that is used to parse the
	* document before really parsing it. This is used to create a map of values
	* from one type of information to the other. This map can then be used when
	* parsing the document using @p regExpSearch to get missing values.
	* @param regExpInfoKeyPre TODO: documentation
	* @param regExpInfoValuePre */
	void setRegExpDepartures( const QString &regExpSearch,
				  const QList< TimetableInformation > &regExpInfos,
				  const QString &regExpSearchPre = QString(),
				  TimetableInformation regExpInfoKeyPre = Nothing,
				  TimetableInformation regExpInfoValuePre = Nothing );

	// TODO: Documentation
	void setRegExpDepartureGroupTitles( const QString &regExpSearch,
				    const QList< TimetableInformation > &regExpInfos );

	/** Sets a regular expression for parsing an html document for a list of journeys.
	* A list of TimetableInformation values is needed to know what's the meaning
	* of each matched string of the regular expression.
	* @param regExpSearch The regular expression used to parse a document for journeys.
	* @param regExpInfos A list of TimetableInformation values that describe what
	* each match of the regular expression in @p regExpSearch means. */
	void setRegExpJourneys( const QString &regExpSearch,
				const QList< TimetableInformation > &regExpInfos );

	/** Adds a regular expression for parsing an html document for a list of possible stops.
	* @param regExpRange A regular expression for getting a subset of the html
	* document containing the list of stops.
	* @param regExpSearch A regular expression for getting the stop information from
	* the list matched by @p regExpRange.
	* @param regExpInfos A list of TimetableInformation values that describe what
	* each match of the regular expression in @p regExpSearch means. */
	void addRegExpPossibleStops( const QString &regExpRange, const QString &regExpSearch,
				     const QList< TimetableInformation > &regExpInfos = QList< TimetableInformation >() << StopName );

	/** Adds a regular expression for parsing a string matched by the regular
	* expression set by setRegExpJourneys() that is assocciated with the
	* TimetableInformation value JourneyNews. You can add more such regular
	* expressions for different meanings.
	* @param regExpSearch A regular expression used to parse a document for journey news.
	* @param regExpInfos A list of TimetableInformation values that describe what
	* each match of the regular expression in @p regExpSearch means.
	* @see setRegExpJourneys() */
	void addRegExpJouneyNews( const QString &regExpSearch, const QList< TimetableInformation > &regExpInfos );

	/** Adds a replacement for the city name @p city. Before a city name is inserted
	* into a raw url it is checked if there are replacements for the city name.
	* @param city The name of a city to be replaced by @p value.
	* @param value The replacement value for @p city. */
	void addCityNameToValueReplacement( const QString &city, const QString &value );

	/** Sets the hash, that replaces city names that are keys in the hash with it's
	* values, before the city name is inserted into a raw url.
	* @param hash The new replacement hash. */
	void setCityNameToValueReplacementHash( const QHash<QString, QString> &hash );

	/** Sets the name of the XML file that was parsed to get this accessor information object. */
	void setFileName( const QString &fileName );;

	/** Sets the name of this accessor. The name is displayed in the config dialog's
	* service provider combobox.
	* @param name The new name of this accessor. */
	void setName( const QString &name );

	/** Sets the charset used to encode documents from the service provider.
	* @param charsetForUrlEncoding The charset used for encoding. */
	void setCharsetForUrlEncoding( const QByteArray &charsetForUrlEncoding );

	/** Sets the description of this accessor.
	* @param description A description of this accessor. */
	void setDescription( const QString &description );

	/** Sets the author of this accessor. You can also set the email of the author.
	* @param author The author of this accessor.
	* @param email The email address of the author. */
	void setAuthor( const QString &author, const QString &email = QString() );

	/** Sets the version of this accesor.
	* @param version The version of this accessor. */
	void setVersion( const QString &version );

	/** Sets the url to the home page of this service provider.
	* @param url The url to the home page of the service provider. */
	void setUrl( const QString &url );

	/** Sets the short version of the url to the service provider. The short url
	* can be used to display short links to the service provider (the real url of
	* the link should then be url()).
	* @param shortUrl The short url to be set.
	* @see url() @see setUrl() */
	void setShortUrl( const QString &shortUrl );

	void setDefaultVehicleType( VehicleType vehicleType );

	/** Sets the raw url for xml files to an xml file containing departure / arrival lists.
	* @param rawUrlXml The url to an xml file containing departure / arrival lists. */
	void setStopSuggestionsRawUrl( const QString &stopSuggestionsRawUrl );

	/** Sets the raw url for departure / arrival lists to an html file containing
	* departure / arrival lists. */
	void setDepartureRawUrl( const QString &departureRawUrl );

	/** Sets the raw url for journey lists to an html file containing journey lists. */
	void setJourneyRawUrl( const QString &journeyRawUrl );

	/** Sets the country for which the service provider has data.
	* @param country The country to be set. */
	void setCountry( const QString &country );

	/** Sets the cities for which the service provider has data.
	* @param cities A list of cities for which the service provider has data.
	* @see setOnlyUseCitiesInList() */
	void setCities( const QStringList &cities );

	/** Sets wheather or not the service provider needs a seperate city value. */
	void setUseSeperateCityValue( bool useSeperateCityValue );
	/** Sets wheather or not cities may be freely chosen.
	* @param onlyUseCitiesInList true if only cities in the list returned by cities()  are valid.
	* false (default) if cities may be freely chosen, but may be invalid. */
	void setOnlyUseCitiesInList( bool onlyUseCitiesInList );

    private:
	bool supportsByJourneyNewsParsing( const TimetableInformation &info ) const;

	// The name of the XML file that was parsed to get this accessor information object
	QString m_fileName;
	// The name of this accessor, which can be displayed by the visualization
	QString m_name;
	// A short version of the url without protocol or "www"  to be displayed in links
	QString m_shortUrl;
	// A description of the service provider
	QString m_description;
	// The author of the accessor information to be used by the accessor
	QString m_author;
	// The email address of the author
	QString m_email;
	// The version of the accessor information
	QString m_version;
	// The main/home url of the service provider
	QString m_url;
	// If empty, use unicode (QUrl::toPercentEncoding()), otherwise use own toPercentEncoding() with this charset
	QByteArray m_charsetForUrlEncoding;
	// Raw url to a site containing a list of stop name suggestions
	QString m_stopSuggestionsRawUrl;
	// Type of the accessor (HTML, XML)
	AccessorType m_accessorType;
	// A raw url that is used to get journeys
	QString m_journeyRawUrl;
	VehicleType m_defaultVehicleType;
	QString m_serviceProviderID;
	QString m_departureRawUrl;
	QString m_country;
	TimetableRegExpSearch m_searchDepartures;
	TimetableRegExpSearch m_searchJourneys;
	TimetableRegExpSearch *m_searchDeparturesPre;
	TimetableRegExpSearch *m_searchDepartureGroupTitles;
	QStringList m_regExpSearchPossibleStopsRanges;
	QList<TimetableRegExpSearch> m_searchPossibleStops;
	QList<TimetableRegExpSearch> m_searchJourneyNews;
	QStringList m_cities;
	bool m_useSeperateCityValue;
	bool m_onlyUseCitiesInList;
	QHash<QString, QString> m_hashCityNameToValue; // The city value is used for the url (e.g. "ba" for city name "bratislava").
};

#endif // TIMETABLEACCESSOR_HTMLINFO_HEADER
