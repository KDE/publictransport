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
* @brief This file contains TimetableAccessorInfo, which is the base class of all service
*   provider information classes.
*
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_INFO_HEADER
#define TIMETABLEACCESSOR_INFO_HEADER

#include "accessorinfoxmlreader.h"
#include "enums.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QHash>

/** @class TimetableAccessorInfo
 * @brief Provides information about how to download and parse documents from service providers.
 *
 * This is the base class of all service provider information classes. It is used by
 * TimetableAccessor.
 *
 * The values in this class can be read from XML files using AccessorInfoXmlReader.
 **/
class TimetableAccessorInfo {
    friend class AccessorInfoXmlReader; // Because AccessorInfoXmlReader needs to set values when reading xml files

public:
    /** @brief Creates a new TimetableAccessorInfo object. */
    explicit TimetableAccessorInfo();

    /** @brief Simple destructor. */
    virtual ~TimetableAccessorInfo() {};

    /** @brief Gets the name of this accessor, which can be displayed by the visualization. */
    inline QString name() const { return m_name; };

    /** @brief A description of the service provider. */
    inline QString description() const { return m_description; };

    /** @brief Type of the accessor. */
    inline AccessorType accessorType() const { return m_accessorType; };

    /** @brief The version of the accessor information. */
    inline QString version() const { return m_version; };

    /** @brief The ID of the service provider this accessor is designed for. */
    inline QString serviceProvider() const { return m_serviceProviderID; };

    /** @brief The author of the accessor information to be used by the accessor. */
    inline QString author() const { return m_author; };

    /** @brief An abbreviation of the authors name. */
    inline QString shortAuthor() const { return m_shortAuthor; };

    /** @brief The email address of the author. */
    inline QString email() const { return m_email; };

    /** @brief The main/home URL of the service provider. */
    inline QString url() const { return m_url; };

    /** @brief A short version of the URL without protocol to be displayed in links. */
    inline QString shortUrl() const { return m_shortUrl; };

    /**
     * @brief Gets the template "raw" URL to a document containing a departure/arrival list.
     *
     * The returned string contains placeholders for data needed by the service provider, eg.
     * the stop name gets a placeholder <em>"{stop}"</em>. If useSeparateCityValue() returns true,
     * a city value is also needed and has <em>"{city}"</em> as placeholder. Other placeholders
     * are <em>"{time}"</em> (the time in "hh:mm" format of the first departure/arrival),
     * <em>{timestamp}</em> (a time_t timestamp), <em>{maxCount}</em> (maximum number of
     * departures/arrivals to request), <em>{dataType}</em> (the type of data to request, ie.
     * "departures" or "arrivals"), <em>{date:yyyy-MM-dd}</em> (the date in the given format of
     * the first departure/arrival, the format string gets passed to QDate::toString()).
     **/
    inline QString departureRawUrl() const { return m_departureRawUrl; };

    /**
     * @brief Gets the template "raw" URL to a document containing a journey list.
     *
     * The returned string contains placeholders for data needed by the service provider, eg.
     * the origin stop name gets a placeholder <em>"{startStop}"</em> and the target stop name gets
     * a placeholder <em>"{targetStop}"</em>. If useSeparateCityValue() returns true, a city value
     * is also needed and has <em>"{city}"</em> as placeholder. Other placeholders are
     * <em>"{time}"</em> (the time in "hh:mm" format of the first journey), <em>{maxCount}</em>
     * (maximum number of journeys to request), <em>{dataType}</em> (the type of data to request,
     * ie. "journeys"), <em>{date:yyyy-MM-dd}</em> (the date in the given format of the first
     * journey, the format string gets passed to QDate::toString()).
     **/
    inline QString journeyRawUrl() const { return m_journeyRawUrl; };

    /**
     * @brief Gets the template "raw" URL to a document containing stop suggestions.
     *
     * The returned string contains placeholders for data needed by the service provider, eg.
     * the stop name gets a placeholder <em>"{stop}"</em>. If useSeparateCityValue() returns true,
     * a city value is also needed and has <em>"{city}"</em> as placeholder. Another placeholder
     * that may be needed is <em>{timestamp}</em> (a time_t timestamp).
     **/
    inline QString stopSuggestionsRawUrl() const { return m_stopSuggestionsRawUrl; };

    /** @brief An URL that is used to download a (GTFS) feed. */
    inline QString feedUrl() const { return m_departureRawUrl; };

    /** @brief Filename of the (GTFS) database. */
    inline QString databaseFileName() const { return m_journeyRawUrl; };

    /** @brief An URL where realtime trip update data gets downloaded (for delays). */
    inline QString realtimeTripUpdateUrl() const { return m_realtimeTripUpdateUrl; };

    /** @brief An URL where realtime alerts data gets downloaded (for journey news). */
    inline QString realtimeAlertsUrl() const { return m_realtimeAlertsUrl; };

    /** @brief The timezone of the area in which the service provider operates or an empty string. */
    inline QString timeZone() const { return m_timeZone; };

    inline QHash<QString, QString> attributesForStopSuggestions() const { return m_attributesForStopSuggestions; };
    inline QHash<QString, QString> attributesForDepatures() const { return m_attributesForDepatures; };
    inline QHash<QString, QString> attributesForJourneys() const { return m_attributesForJourneys; };

    /** @brief The country for which the service provider has data. */
    inline QString country() const { return m_country; };

    /** @brief A list of cities for which the service provider has data. */
    inline QStringList cities() const { return m_cities; };

    /**
     * @brief A courtesy string that must be shown to the user when showing the timetable data.
     *
     * If this function returns a not empty string it must be shown to the user. This is because
     * of various license agreements with service providers that require special strings to be
     * shown.
     *
     * If no string is returned the service provider should nevertheless be presented to the user,
     * but in a shorter form if needed, eg. "data by: www.link.com".
     **/
    inline QString credit() const { return m_credit; };

    /** @brief Gets the default vehicle type to be used instead of VehicleType::Unknown. */
    inline VehicleType defaultVehicleType() const { return m_defaultVehicleType; };

    /**
     * @brief Gets the charset used to encode urls before percent-encoding them.
     *
     * Normally this charset is UTF-8. But that doesn't work for sites that require parameters
     * in the URL (..&param=x) to be encoded in that specific charset.
     *
     * @see TimetableAccessor::toPercentEncoding()
     **/
    inline QByteArray charsetForUrlEncoding() const { return m_charsetForUrlEncoding; };

    inline QByteArray fallbackCharset() const { return m_fallbackCharset; };

    /** @brief Gets the URL to get a session key. */
    inline QString sessionKeyUrl() const { return m_sessionKeyUrl; };

    /**
     * @brief Gets the place, where to put the session key in a request.
     *
     * @see SessionKeyPlace
     **/
    inline SessionKeyPlace sessionKeyPlace() const { return m_sessionKeyPlace; };

    /** @brief Gets the data to POST with requests. */
    inline QString sessionKeyData() const { return m_sessionKeyData; };

    /**
     * @brief Gets the minimum seconds to wait between two data-fetches from the service provider.
     **/
    inline int minFetchWait() const { return m_minFetchWait; };

    /** @brief Wheather or not the service provider needs a separate city value. */
    inline bool useSeparateCityValue() const { return m_useSeparateCityValue; };

    /**
     * @brief Wheather or not cities may be chosen freely.
     *
     * @return true if only cities in the list returned by cities()  are valid.
     * @return false (default) if cities may be chosen freely, but may be invalid.
     **/
    inline bool onlyUseCitiesInList() const { return m_onlyUseCitiesInList; };

    /**
     * @brief Gets a value for the given city that is used by the service provider.
     *
     * @returns Either the value for the given city if it exists, or @p city itself. */
    QString mapCityNameToValue( const QString &city ) const;

    /** @brief The name of the XML file that was parsed to get this accessor information object. */
    inline QString fileName() const { return m_fileName; };

    /** @brief The file name of the script file to parse HTML pages. */
    inline QString scriptFileName() const { return m_scriptFileName; };

    /**
     * @brief Returns a list of changelog entries.
     *
     * @see ChangelogEntry
     **/
    inline QList<ChangelogEntry> changelog() const { return m_changelog; };

    /** @brief Wheather or not this accessor supports stop name autocompletion. TODO: Remove? */
    virtual bool supportsStopAutocompletion() const {
        return false; };

    /** @brief Wheather or not this accessor supports the given TimetableInformation. */
    virtual bool supportsTimetableAccessorInfo( const TimetableInformation &info ) const {
        Q_UNUSED(info)
        return false; };

    const QHash<QString, QString> &cityNameToValueReplacementHash() const {
        return m_hashCityNameToValue; };

protected:
    /**
     * @brief Finishes the data given by the setters.
     *
     * Should be called after all values have been set.
     * This generates a short author name from the author name if none was given,
     * sorts the changelog and sets the short url to the url if no short url was given.
     **/
    void finish();

    /**
     * @brief Adds a replacement for the city name @p city. Before a city name is inserted
     *   into a raw url it is checked if there are replacements for the city name.
     *
     * @param city The name of a city to be replaced by @p value.
     *
     * @param value The replacement value for @p city.
     **/
    inline void addCityNameToValueReplacement( const QString &city, const QString &value ) {
        m_hashCityNameToValue.insert( city, value ); };

    /**
     * @brief Sets the hash, that replaces city names that are keys in the hash with it's
     *   values, before the city name is inserted into a raw url.
     *
     * @param hash The new replacement hash.
     **/
    inline void setCityNameToValueReplacementHash( const QHash<QString, QString> &hash ) {
        m_hashCityNameToValue = hash; };

    /**
     * @brief Sets the name of the XML file that was parsed to get this accessor information object.
     *
     * If @p fileName is a symlink the real file name gets retrieved (using
     * KStandardDirs::realFilePath, eg. for the default service providers ending with "_default.xml").
     **/
    void setFileName( const QString &fileName );

    /** @brief Sets the file name of the script file to parse html pages. */
    inline void setScriptFile( const QString &scriptFileName ) {
        m_scriptFileName = scriptFileName; };

    /**
     * @brief Sets the name of this accessor. The name is displayed in the config dialog's
     *   service provider combobox.
     *
     * @param name The new name of this accessor.
     **/
    inline void setName( const QString &name ) { m_name = name; };

    /**
     * @brief Sets the ID of the service provider this accessor is designed for.
     *
     * @param serviceProvider The ID of the service provider.
     **/
    inline void setServiceProvider( const QString &serviceProvider ) {
        m_serviceProviderID = serviceProvider; };

    /**
     * @brief Sets the type of the accessor.
     *
     * @param type The type of the accessor.
     **/
    inline void setType( AccessorType type ) { m_accessorType = type; };

    /**
     * @brief Sets the charset used to encode documents from the service provider.
     *
     * @param charsetForUrlEncoding The charset used for encoding.
     **/
    inline void setCharsetForUrlEncoding( const QByteArray &charsetForUrlEncoding ) {
        m_charsetForUrlEncoding = charsetForUrlEncoding; };

    /**
     * @brief Sets the charset used to encode documents where it couldn't be determined
     *   automatically.
     *
     * @param fallbackCharset The charset used if it couldn't be determined.
     **/
    inline void setFallbackCharset( const QByteArray &fallbackCharset ) {
        m_fallbackCharset = fallbackCharset; };

    /**
     * @brief Sets session key data.
     *
     * @param sessionKeyUrl An url to a document containing the session key.
     *
     * @param sessionKeyPlace The place where to put the session key in requests.
     *
     * @param data Data to POST with requests.
     **/
    inline void setSessionKeyData( const QString &sessionKeyUrl, SessionKeyPlace sessionKeyPlace,
                                   const QString &data )
    {
        m_sessionKeyUrl = sessionKeyUrl;
        m_sessionKeyPlace = sessionKeyPlace;
        m_sessionKeyData = data;
    };

    /**
     * @brief Sets the description of this accessor.
     *
     * @param description A description of this accessor.
     **/
    inline void setDescription( const QString &description ) { m_description = description; };

    /**
     * @brief Sets the author of this accessor. You can also set the email of the author.
     *
     * @param author The author of this accessor.
     *
     * @param shortAuthor An abbreviation of the authors name.
     *
     * @param email The email address of the author.
     **/
    void setAuthor( const QString &author, const QString &shortAuthor, const QString &email = QString() );

    /**
     * @brief Sets the version of this accessor.
     *
     * @param version The version of this accessor.
     **/
    inline void setVersion( const QString &version ) { m_version = version; };

    /**
     * @brief Sets the url to the home page of this service provider.
     *
     * @param url The url to the home page of the service provider.
     **/
    inline void setUrl( const QString &url ) { m_url = url; };

    /**
     * @brief Sets the short version of the url to the service provider. The short url
     *   can be used to display short links to the service provider (the real url of
     *   the link should then be url()).
     *
     * @param shortUrl The short url to be set.
     *
     * @see url() @see setUrl()
     **/
    inline void setShortUrl( const QString &shortUrl ) { m_shortUrl = shortUrl; };

    inline void setMinFetchWait( int minFetchWait ) { m_minFetchWait = minFetchWait; };

    inline void setDefaultVehicleType( VehicleType vehicleType ) {
        m_defaultVehicleType = vehicleType; };

    /** @brief Sets the template "raw" URL to documents containing departures/arrivals. */
    inline void setDepartureRawUrl( const QString &departureRawUrl ) {
        m_departureRawUrl = departureRawUrl; };

    /** @brief Sets the template "raw" URL to documents containing journeys. */
    inline void setJourneyRawUrl( const QString &journeyRawUrl ) {
        m_journeyRawUrl = journeyRawUrl; };

    /** @brief Sets the template "raw" URL to documents containing stop suggestions. */
    inline void setStopSuggestionsRawUrl( const QString &stopSuggestionsRawUrl ) {
        m_stopSuggestionsRawUrl = stopSuggestionsRawUrl; };

    /** @brief Sets the URL to a (GTFS) feed. TODO  */
    inline void setFeedUrl( const QString &feedUrl ) { setDepartureRawUrl(feedUrl); };

    /** @brief Sets the filename of the (GTFS) database. TODO  */
    inline void setDatabaseFileName( const QString &databaseFileName ) {
        setJourneyRawUrl(databaseFileName); };

    inline void setRealtimeTripUpdateUrl( const QString &realtimeTripUpdateUrl ) {
            m_realtimeTripUpdateUrl = realtimeTripUpdateUrl; };
    inline void setRealtimeAlertsUrl( const QString &realtimeAlertsUrl ) {
            m_realtimeAlertsUrl = realtimeAlertsUrl; };

    inline void setAttributesForStopSuggestions( const QHash<QString, QString> &attributesForStopSuggestions ) {
        m_attributesForStopSuggestions = attributesForStopSuggestions; };
    inline void setAttributesForDepatures( const QHash<QString, QString> &attributesForDepartures ) {
        m_attributesForDepatures = attributesForDepartures; };
    inline void setAttributesForJourneys( const QHash<QString, QString> &attributesForJourneys ) {
        m_attributesForJourneys = attributesForJourneys; };

    /**
     * @brief Sets the country for which the service provider has data.
     *
     * @param country The country to be set.
     **/
    inline void setCountry( const QString &country ) { m_country = country; };

    /**
     * @brief Sets the cities for which the service provider has data.
     *
     * @see setOnlyUseCitiesInList()
     **/
    inline void setCities( const QStringList &cities ) { m_cities = cities; };

    inline void setCredit( const QString &credit ) { m_credit = credit; };

    /**
     * @brief Sets the list of changelog entries.
     *
     * @param changelog The new list of changelog entries.
     * @see ChangelogEntry
     **/
    inline void setChangelog( const QList<ChangelogEntry> &changelog ) {
        m_changelog = changelog;
    };

    /** @brief Sets the timezone of the area in which the service provider operates */
    inline void setTimeZone( const QString &timeZone ) { m_timeZone = timeZone; };

    /**
     * @brief Sets wheather or not the service provider needs a separate city value. */
    inline void setUseSeparateCityValue( bool useSeparateCityValue ) {
        m_useSeparateCityValue = useSeparateCityValue; };

    /**
     * @brief Sets wheather or not cities may be freely chosen.
     *
     * @param onlyUseCitiesInList true if only cities in the list returned by cities()  are valid.
     *   false (default) if cities may be freely chosen, but may be invalid.
     **/
    inline void setOnlyUseCitiesInList( bool onlyUseCitiesInList ) {
        m_onlyUseCitiesInList = onlyUseCitiesInList; };

protected:
    virtual bool supportsByJourneyNewsParsing( const TimetableInformation &info ) const {
        Q_UNUSED(info)
        return false; };

    // The name of the XML file that was parsed to get this accessor information object
    QString m_fileName;
    // The file name of the script file to parse html pages
    QString m_scriptFileName;
    // The name of this accessor, which can be displayed by the visualization
    QString m_name;
    // A short version of the url without protocol or "www"  to be displayed in links
    QString m_shortUrl;
    // A description of the service provider
    QString m_description;
    // The author of the accessor information to be used by the accessor
    QString m_author;
    // An abbreviation of the authors name
    QString m_shortAuthor;
    // The email address of the author
    QString m_email;
    // The version of the accessor information
    QString m_version;
    // The main/home url of the service provider
    QString m_url;
    // If empty, use unicode (QUrl::toPercentEncoding()), otherwise use own
    // toPercentEncoding() with this charset
    QByteArray m_charsetForUrlEncoding, m_fallbackCharset;

    // Session key data
    QString m_sessionKeyUrl;
    SessionKeyPlace m_sessionKeyPlace;
    QString m_sessionKeyData;

    // Raw url to a site containing a list of stop name suggestions
    QString m_stopSuggestionsRawUrl;
    // A raw url that is used to get departures/arrivals
    QString m_departureRawUrl;
    // A raw url that is used to get journeys
    QString m_journeyRawUrl;

    QString m_realtimeTripUpdateUrl;
    QString m_realtimeAlertsUrl;

    // Keys are versions, where the change entries occurred (values)
    QList<ChangelogEntry> m_changelog;

    QHash<QString, QString> m_attributesForStopSuggestions;
    QHash<QString, QString> m_attributesForDepatures;
    QHash<QString, QString> m_attributesForJourneys;

    // Type of the accessor (ScriptedAccessor, XmlAccessor, GtfsAccessor)
    AccessorType m_accessorType;
    VehicleType m_defaultVehicleType;
    int m_minFetchWait;
    QString m_serviceProviderID;
    QString m_country;
    QStringList m_cities;
    QString m_credit;
    bool m_useSeparateCityValue;
    bool m_onlyUseCitiesInList;
    QHash<QString, QString> m_hashCityNameToValue; // The city value is used for the url (e.g. "ba" for city name "bratislava").
    QString m_timeZone;
};

#endif // TIMETABLEACCESSOR_INFO_HEADER
