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
* @brief This file contains ServiceProviderData, which is the base class of all service
*   provider information classes.
*
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SERVICEPROVIDERDATA_HEADER
#define SERVICEPROVIDERDATA_HEADER

// Own includes
#include "serviceproviderdatareader.h"
#include "enums.h"

// Qt includes
#include <QList>
#include <QString>
#include <QStringList>
#include <QHash>

/**
 * @brief Provides information about how to download and parse documents from service providers.
 *
 * Can be read from service provider plugin XML files using ServiceProviderDataReader. There is also an
 * ServiceProviderDataWriter implemented in TimetableMate (not needed by the engine).
 *
 * Each ServiceProvider uses one constant instance of this class to know how to request/parse
 * documents from the different service providers.
 *
 * To get a non-const copy of the constant ServiceProviderData object returned by
 * ServiceProvider::data(), use ServiceProviderData::clone().
 **/
class ServiceProviderData : public QObject {
    Q_OBJECT
    friend class ServiceProviderDataReader; // Because ServiceProviderDataReader needs to set values when reading xml files

    // Properties are declared as constant, they are intended to be used with constant
    // ServiceProviderData instances as returned by eg. ServiceProvider::data().
    // Otherwise QML would give warnings because NOTIFY signals are missing, although the object
    // itself is constant.
    Q_PROPERTY( Enums::ServiceProviderType type READ type CONSTANT )
    Q_PROPERTY( QString typeString READ typeString CONSTANT )
    Q_PROPERTY( QString typeName READ typeName CONSTANT )
    Q_PROPERTY( QString name READ name CONSTANT )
    Q_PROPERTY( QString description READ description CONSTANT )
    Q_PROPERTY( QString version READ version CONSTANT )
    Q_PROPERTY( QString fileFormatVersion READ fileFormatVersion CONSTANT )
    Q_PROPERTY( QString id READ id CONSTANT )
    Q_PROPERTY( QString author READ author CONSTANT )
    Q_PROPERTY( QString shortAuthor READ shortAuthor CONSTANT )
    Q_PROPERTY( QString email READ email CONSTANT )
    Q_PROPERTY( QString url READ url CONSTANT )
    Q_PROPERTY( QString shortUrl READ shortUrl CONSTANT )
    Q_PROPERTY( QString country READ country CONSTANT )
    Q_PROPERTY( QStringList cities READ cities CONSTANT )
    Q_PROPERTY( Enums::VehicleType defaultVehicleType READ defaultVehicleType CONSTANT )
    Q_PROPERTY( QString credit READ credit CONSTANT )
    Q_PROPERTY( QByteArray charsetForUrlEncoding READ charsetForUrlEncoding CONSTANT )
    Q_PROPERTY( QByteArray fallbackCharset READ fallbackCharset CONSTANT )
    Q_PROPERTY( int minFetchWait READ minFetchWait CONSTANT )
    Q_PROPERTY( bool useSeparateCityValue READ useSeparateCityValue CONSTANT )
    Q_PROPERTY( bool onlyUseCitiesInList READ onlyUseCitiesInList CONSTANT )
    Q_PROPERTY( QString fileName READ fileName CONSTANT )
    Q_PROPERTY( QStringList sampleStopNames READ sampleStopNames CONSTANT )
    Q_PROPERTY( QString sampleCity READ sampleCity CONSTANT )
    Q_PROPERTY( qreal sampleLongitude READ sampleLongitude CONSTANT )
    Q_PROPERTY( qreal sampleLatitude READ sampleLatitude CONSTANT )
    Q_PROPERTY( QList<ChangelogEntry> changelog READ changelog CONSTANT )
    Q_PROPERTY( QString lastChangelogAuthor READ lastChangelogAuthor CONSTANT )
    Q_PROPERTY( QString lastChangelogVersion READ lastChangelogVersion CONSTANT )
    Q_PROPERTY( QString lastChangelogDescription READ lastChangelogDescription CONSTANT )
    Q_PROPERTY( QString notes READ notes CONSTANT )

    // For ScriptedProvider
    Q_PROPERTY( QString scriptFileName READ scriptFileName CONSTANT )
    Q_PROPERTY( QStringList scriptExtensions READ scriptExtensions CONSTANT )

    // For GtfsProvider
    Q_PROPERTY( QString feedUrl READ feedUrl CONSTANT )
    Q_PROPERTY( QString realtimeTripUpdateUrl READ realtimeTripUpdateUrl CONSTANT )
    Q_PROPERTY( QString realtimeAlertsUrl READ realtimeAlertsUrl CONSTANT )
    Q_PROPERTY( QString timeZone READ timeZone CONSTANT )

public:
    /**
     * @brief Creates a new ServiceProviderData object.
     *
     * @param type The type of the service provider plugin.
     * @param id The ID of the service provider for which the plugin is designed for.
     *
     * @see ServiceProviderType
     **/
    explicit ServiceProviderData( const Enums::ServiceProviderType& type = Enums::InvalidProvider,
                                  const QString& id = QString(), QObject *parent = 0 );

    ServiceProviderData( const Enums::ServiceProviderType &type, const QString &id,
            const QHash<QString, QString> &names, const QHash<QString, QString> &descriptions,
            const QString &version, const QString &fileFormatVersion, bool useSeparateCityValue,
            bool onlyUseCitiesInList, const QString &url, const QString &shortUrl,
            int minFetchWait, const QString &author, const QString &email,
            Enums::VehicleType defaultVehicleType, const QList<ChangelogEntry> &changelog,
            const QStringList &cities,
            const QHash<QString, QString> &cityNameToValueReplacementHash,
            QObject *parent = 0 );

    ServiceProviderData( const ServiceProviderData &data, QObject *parent = 0 );

    virtual ~ServiceProviderData();

    ServiceProviderData *clone( QObject *parent = 0 ) const {
            return new ServiceProviderData(*this, parent); };

    ServiceProviderData &operator =( const ServiceProviderData &data );
    bool operator ==( const ServiceProviderData &data ) const;

    /**
     * @brief Compare version strings in @p version1 and @p version2.
     * @returns 0, if version1 equals version2. 1, if version1 is bigger than version2.
     *   -1, if version1 is smaller than version2. Similiar to QString::compare()
     **/
    static int compareVersions( const QString &version1, const QString &version2 );

    /** @brief Get a short version of @p url without scheme, query, port, path, fragment. */
    static QString shortUrlFromUrl( const QString &url );

    /** @brief Get a short version of @p authorName, only first characters of prenames. */
    static QString shortAuthorFromAuthor( const QString &authorName );

    /** @brief Get the name of the service provider (in the local language, if available). */
    QString name() const;

    /** @brief A description of the service provider (in the local language, if available). */
    QString description() const;

    /** @brief Get the names of this service provider, sorted by language. */
    QHash<QString, QString> names() const { return m_name; };

    /** @brief Description of the service provider, sorted by language. */
    QHash<QString, QString> descriptions() const { return m_description; };

    /** @brief The type of the service provider (scripted, GTFS, ...) */
    Enums::ServiceProviderType type() const { return m_serviceProviderType; };

    /** @brief Whether or not this object contains valid data. */
    bool isValid() const { return m_serviceProviderType != Enums::InvalidProvider; };

    /** @brief Convenience function that uses ServiceProviderGlobal::typeToString(). */
    QString typeString() const;

    /** @brief Convenience function that uses ServiceProviderGlobal::typeName(). */
    QString typeName() const;

    /** @brief The version of the service provider plugin. */
    QString version() const { return m_version; };

    /** @brief The version of the file format of the service provider XML. */
    QString fileFormatVersion() const { return m_fileFormatVersion; };

    /** @brief The ID of the service provider plugin. */
    QString id() const { return m_id; };

    /** @brief The author of the service provider plugin. */
    QString author() const { return m_author; };

    /** @brief An abbreviation of the authors name. */
    QString shortAuthor() const { return m_shortAuthor; };

    /** @brief The email address of the author. */
    QString email() const { return m_email; };

    /** @brief The main/home url of the service provider. */
    QString url() const { return m_url; };

    /** @brief A short version of the url without protocol or "www" to be displayed in links. */
    QString shortUrl() const { return m_shortUrl; };

    /** @brief The country for which the service provider has data. */
    QString country() const { return m_country; };

    /** @brief A list of cities for which the service provider has data. */
    QStringList cities() const { return m_cities; };

    QString credit() const { return m_credit; };
    Enums::VehicleType defaultVehicleType() const { return m_defaultVehicleType; };

    /**
     * @brief If empty, use unicode (QUrl::toPercentEncoding()), otherwise use
     *   own toPercentEncoding() with this charset.
     **/
    QByteArray charsetForUrlEncoding() const { return m_charsetForUrlEncoding; };
    QByteArray fallbackCharset() const { return m_fallbackCharset; };

    /**
     * @brief Get the minimum seconds to wait between two data-fetches from the service provider.
     **/
    int minFetchWait() const { return m_minFetchWait; };

    QStringList sampleStopNames() const { return m_sampleStopNames; };
    QString sampleCity() const { return m_sampleCity; };
    qreal sampleLongitude() const { return m_sampleLongitude; };
    qreal sampleLatitude() const { return m_sampleLatitude; };
    bool hasSampleCoordinates() const {
        return !qFuzzyIsNull(m_sampleLongitude) && !qFuzzyIsNull(m_sampleLatitude);
    };

    /** @brief Wheather or not the service provider needs a separate city value. */
    bool useSeparateCityValue() const { return m_useSeparateCityValue; };

    /**
     * @brief Wheather or not cities may be chosen freely.
     *
     * @return true if only cities in the list returned by cities()  are valid.
     * @return false (default) if cities may be chosen freely, but may be invalid.
     **/
    bool onlyUseCitiesInList() const { return m_onlyUseCitiesInList; };

    /**
     * @brief Get a value for the given city that is used by the service provider.
     *
     * @returns Either the value for the given city if it exists, or @p city itself.
     **/
    QString mapCityNameToValue( const QString &city ) const;

    /**
     * @brief The name of the service provider plugin XML file.
     *
     * The values provided by this class can be read from this file.
     **/
    QString fileName() const { return m_fileName; };

    /** @brief The file name of the script file to parse downloaded documents. */
    QString scriptFileName() const { return m_scriptFileName; };

    /** @brief A list of QScript extensions to import when executing the script. */
    QStringList scriptExtensions() const { return m_scriptExtensions; };

    /** @brief An URL that is used to download a (GTFS) feed. */
    QString feedUrl() const { return m_feedUrl; };

    /** @brief An URL where realtime GTFS trip update data gets downloaded (for delays). */
    QString realtimeTripUpdateUrl() const { return m_tripUpdatesUrl; };

    /** @brief An URL where realtime GTFS alerts data gets downloaded (for journey news). */
    QString realtimeAlertsUrl() const { return m_alertsUrl; };

    /** @brief The timezone of the area in which the service provider operates or an empty string. */
    QString timeZone() const { return m_timeZone; };

    QList<ChangelogEntry> changelog() const { return m_changelog; };
    QString lastChangelogAuthor() const {
        return m_changelog.isEmpty() ? QString() : m_changelog.first().author; };
    QString lastChangelogVersion() const {
        return m_changelog.isEmpty() ? QString() : m_changelog.first().version; };
    QString lastChangelogDescription() const {
        return m_changelog.isEmpty() ? QString() : m_changelog.first().description; };

    const QHash<QString, QString> &cityNameToValueReplacementHash() const {
        return m_hashCityNameToValue; };

    /** @brief Custom notes for the project, eg. a list of todo items. */
    QString notes() const;

// Where appropriate "const ServiceProviderData*" gets used, so that setters are not available
// protected:
    void setChangelog( const QList<ChangelogEntry> &changelog ) {
        m_changelog = changelog;
    };

    /**
     * @brief Finishes the data given by the setters.
     *
     * Should be called after all values have been set.
     * This eg. generates a short author name/short URL from the complete author name/URL if none
     * was given, sorts the changelog.
     **/
    void finish();

    /**
     * @brief Adds a replacement for the city name @p city. Before a city name is inserted
     *   into a raw url it is checked if there are replacements for the city name.
     *
     * @param city The name of a city to be replaced by @p value.
     * @param value The replacement value for @p city.
     **/
    void addCityNameToValueReplacement( const QString &city, const QString &value ) {
        m_hashCityNameToValue.insert( city, value ); };

    /**
     * @brief Set the hash, that replaces city names that are keys in the hash with it's
     *   values, before the city name is inserted into a raw url.
     *
     * @param hash The new replacement hash.
     **/
    void setCityNameToValueReplacementHash( const QHash<QString, QString> &hash ) {
        m_hashCityNameToValue = hash; };

    /**
     * @brief Set the name of the XML file that was parsed to get this ServiceProviderData object.
     *
     * If @p fileName is a symlink the real file name gets retrieved (using
     * KStandardDirs::realFilePath, eg. for the default service providers ending with
     * "_default.pts" or "_default.xml").
     **/
    void setFileName( const QString &fileName );

    /**
     * @brief Set the file name of the script file to parse downloaded documents.
     *
     * @param scriptFileName The file name of the script.
     **/
    void setScriptFile( const QString &scriptFileName ) {
        m_scriptFileName = scriptFileName;
    };

    /**
     * @brief Set the file name of the script file to parse downloaded documents.
     *
     * @param scriptFileName The file name of the script.
     * @param extensions A list of QScript extensions to import when executing the script.
     **/
    void setScriptFile( const QString &scriptFileName, const QStringList &extensions ) {
        m_scriptFileName = scriptFileName;
        m_scriptExtensions = extensions;
    };

    /**
     * @brief Set the name of this service provider plugin.
     * @param names The new name.
     **/
    void setNames( const QHash<QString, QString> &names ) { m_name = names; };

    /**
     * @brief Set the ID of the service provider.
     * @param id The ID of the service provider.
     **/
    void setId( const QString &id ) { m_id = id; };

    /**
     * @brief Set the type of the service provider.
     * @param type The new type of the service provider.
     **/
    void setType( Enums::ServiceProviderType type ) { m_serviceProviderType = type; };

    /**
     * @brief Set the charset used to encode documents from the service provider.
     *
     * @param charsetForUrlEncoding The charset used for encoding.
     **/
    void setCharsetForUrlEncoding( const QByteArray &charsetForUrlEncoding ) {
        m_charsetForUrlEncoding = charsetForUrlEncoding; };

    /**
     * @brief Set the charset used to encode documents where it couldn't be determined
     *   automatically.
     *
     * @param fallbackCharset The charset used if it couldn't be determined.
     **/
    void setFallbackCharset( const QByteArray &fallbackCharset ) {
        m_fallbackCharset = fallbackCharset; };

    /**
     * @brief Set the description of this service provider (plugin).
     *
     * @param descriptions The description in different languages. Keys of the hash are country
     *   codes, values are descriptions translated to the language associated with that country code.
     **/
    void setDescriptions( const QHash<QString, QString> &descriptions ) {
        m_description = descriptions; };

    /**
     * @brief Set the author of the service provider plugin.
     *
     * You can also set the email of the author.
     * @param author The author of this service provider plugin.
     * @param shortAuthor An abbreviation of the authors name.
     * @param email The email address of the author.
     **/
    void setAuthor( const QString &author, const QString &shortAuthor, const QString &email = QString() );

    /**
     * @brief Set the version of the service provider plugin.
     *
     * The version of the service provider plugin should be updated whenever its values or
     * parsing implementations gets changed.
     *
     * @param version The version of the service provider plugin.
     **/
    void setVersion( const QString &version ) { m_version = version; };

    /**
     * @brief Set the file format version of this service provider plugin.
     *
     * The "file version" names the used version of the PublicTransport engine plugin interface.
     * Currently only version 1.1 is supported.
     *
     * @note Service provider plugins written for version 1.0 need to be updated. But that mostly
     *   means adding a 'getTimetable()' function to download a timetable document and calling the
     *   old 'parseTimetable()' function if the download is finished. The same can be done with
     *   'getStopsuggestions()' and 'getJourneys()'. The return values now need to be JavaScript
     *   objects containing properties named after the enumerables in TimetableInformation.
     *
     * @param fileFormatVersion The file format version of this service provider plugin.
     **/
    void setFileFormatVersion( const QString &fileFormatVersion ) {
        m_fileFormatVersion = fileFormatVersion; };

    /**
     * @brief Set the URL to the home page of this service provider.
     *
     * @param url The URL to the home page of the service provider.
     * @param shortUrl A short version of the URL to be displayed to the user. If an empty string
     *   is given, the short URL is generated from @p url.
     **/
    void setUrl( const QString &url, const QString &shortUrl = QString() );

    /**
     * @brief Set the short version of the URL to the service provider.
     *
     * The short URL can be used as text shown for links to the service provider home page
     * (the complete URL of the link should then be url()).
     *
     * @param shortUrl The short URL to be set.
     * @see setUrl()
     **/
    void setShortUrl( const QString &shortUrl ) { m_shortUrl = shortUrl; };

    void setMinFetchWait( int minFetchWait ) { m_minFetchWait = minFetchWait; };

    void setDefaultVehicleType( Enums::VehicleType vehicleType ) {
        m_defaultVehicleType = vehicleType; };

    /**
     * @brief Set the country for which the service provider has data.
     *
     * @param country The country to be set.
     **/
    void setCountry( const QString &country ) { m_country = country; };

    /**
     * @brief Set the cities for which the service provider has data.
     *
     * @param cities A list of cities for which the service provider has data.
     * @see setOnlyUseCitiesInList()
     **/
    void setCities( const QStringList &cities ) {
        m_cities = cities; };

    void setCredit( const QString &credit ) { m_credit = credit; };

    /** @brief Set wheather or not the service provider needs a separate city value. */
    void setUseSeparateCityValue( bool useSeparateCityValue ) {
        m_useSeparateCityValue = useSeparateCityValue; };

    /**
     * @brief Set wheather or not cities may be freely chosen.
     *
     * @param onlyUseCitiesInList true if only cities in the list returned by cities()  are valid.
     *   false (default) if cities may be freely chosen, but may be invalid.
     **/
    void setOnlyUseCitiesInList( bool onlyUseCitiesInList ) {
        m_onlyUseCitiesInList = onlyUseCitiesInList; };

    void setSampleStops( const QStringList &sampleStopNames )
            { m_sampleStopNames = sampleStopNames; }; // For journeys at least two stop names are required
    void setSampleCity( const QString &sampleCity ) { m_sampleCity = sampleCity; };
    void setSampleCoordinates( qreal longitude, qreal latitude ) {
        m_sampleLongitude = longitude;
        m_sampleLatitude = latitude;
    };

    void setNotes( const QString &notes ) { m_notes = notes; };

    void setFeedUrl( const QString &feedUrl ) { m_feedUrl = feedUrl; };
    void setRealtimeTripUpdateUrl( const QString &tripUpdatedUrl ) { m_tripUpdatesUrl = tripUpdatedUrl; };
    void setRealtimeAlertsUrl( const QString &alertsUrl ) { m_alertsUrl = alertsUrl; };
    void setTimeZone( const QString &timeZone ) { m_timeZone = timeZone; };

protected:
    virtual bool supportsByJourneyNewsParsing( const Enums::TimetableInformation &info ) const {
        Q_UNUSED(info)
        return false; };

    static int versionNumberFromString( const QString &version, int *startPos );

    // The name of the XML file that was parsed to get this service provider data object
    QString m_fileName;
    // The file name of the script file to parse downloaded documents
    QString m_scriptFileName;
    // A list of QScript extensions to import when executing the script
    QStringList m_scriptExtensions;
    // The names of this service provider, sorted by language, which can be displayed by the visualization
    QHash<QString, QString> m_name;
    // A short version of the url without protocol or "www"  to be displayed in links
    QString m_shortUrl;
    // A description of the service provider, sorted by language
    QHash<QString, QString> m_description;
    // The author of the service provider plugin
    QString m_author;
    // An abbreviation of the authors name
    QString m_shortAuthor;
    // The email address of the author
    QString m_email;
    // The version of the service provider plugin
    QString m_version;
    // The version of the PublicTransport engine plugin interface
    QString m_fileFormatVersion;
    // The main/home url of the service provider
    QString m_url;
    // If empty, use unicode (QUrl::toPercentEncoding()), otherwise use own
    // toPercentEncoding() with this charset
    QByteArray m_charsetForUrlEncoding, m_fallbackCharset;

    QString m_feedUrl;
    QString m_tripUpdatesUrl;
    QString m_alertsUrl;
    QString m_timeZone;

    // Keys are versions, where the change entries occurred (values)
    QList<ChangelogEntry> m_changelog;

    // Type of the service provider
    Enums::ServiceProviderType m_serviceProviderType;
    Enums::VehicleType m_defaultVehicleType;
    int m_minFetchWait;
    QString m_id;
    QString m_country;
    QStringList m_cities;
    QString m_credit;
    bool m_useSeparateCityValue;
    bool m_onlyUseCitiesInList;
    QHash<QString, QString> m_hashCityNameToValue; // The city value is used for the url (e.g. "ba" for city name "bratislava").

    // Sample data, used to test service provider plugins
    QStringList m_sampleStopNames; // For journeys at least two stop names are required
    QString m_sampleCity;
    qreal m_sampleLongitude;
    qreal m_sampleLatitude;

    QString m_notes;
};

Q_DECLARE_METATYPE( ServiceProviderData );

#endif // Multiple inclusion guard
