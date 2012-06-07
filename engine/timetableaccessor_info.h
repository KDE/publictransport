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

/**
 * @brief Provides information about how to download and parse documents from service providers.
 *
 * Can be read from "accessor info XML" documents using AccessorInfoXmlReader. There is also an
 * AccessorInfoXmlWriter implemented in TimetableMate (not needed in the engine).
 *
 * Each TimetableAccessor uses one constant instance of this class to know how to request/parse
 * documents from the different service providers.
 *
 * To get a non-const copy of the constant TimetableAccessorInfo object returned by
 * TimetableAccessor::info(), use TimetableAccessorInfo::clone().
 **/
class TimetableAccessorInfo : public QObject {
    Q_OBJECT
    friend class AccessorInfoXmlReader; // Because AccessorInfoXmlReader needs to set values when reading xml files

    // Properties are declared as constant, they are intended to be used with constant
    // TimetableAccessorInfo instances as returned by eg. TimetableAccessor::info().
    // Otherwise QML would give warnings because NOTIFY signals are missing, although the object
    // itself is constant.
    Q_PROPERTY( QString name READ name CONSTANT )
    Q_PROPERTY( QString description READ description CONSTANT )
    Q_PROPERTY( QString version READ version CONSTANT )
    Q_PROPERTY( QString fileVersion READ fileVersion CONSTANT )
    Q_PROPERTY( QString serviceProvider READ serviceProvider CONSTANT )
    Q_PROPERTY( QString author READ author CONSTANT )
    Q_PROPERTY( QString shortAuthor READ shortAuthor CONSTANT )
    Q_PROPERTY( QString email READ email CONSTANT )
    Q_PROPERTY( QString url READ url CONSTANT )
    Q_PROPERTY( QString shortUrl READ shortUrl CONSTANT )
    Q_PROPERTY( QString country READ country CONSTANT )
    Q_PROPERTY( QStringList cities READ cities CONSTANT )
    Q_PROPERTY( QString credit READ credit CONSTANT )
    Q_PROPERTY( QByteArray charsetForUrlEncoding READ charsetForUrlEncoding CONSTANT )
    Q_PROPERTY( QByteArray fallbackCharset READ fallbackCharset CONSTANT )
    Q_PROPERTY( int minFetchWait READ minFetchWait CONSTANT )
    Q_PROPERTY( bool useSeparateCityValue READ useSeparateCityValue CONSTANT )
    Q_PROPERTY( bool onlyUseCitiesInList READ onlyUseCitiesInList CONSTANT )
    Q_PROPERTY( QString fileName READ fileName CONSTANT )
    Q_PROPERTY( QString scriptFileName READ scriptFileName CONSTANT )
    Q_PROPERTY( QStringList scriptExtensions READ scriptExtensions CONSTANT )
    Q_PROPERTY( QString type READ typeString CONSTANT )
    Q_PROPERTY( QStringList sampleStopNames READ sampleStopNames CONSTANT )
    Q_PROPERTY( QString sampleCity READ sampleCity CONSTANT )
    Q_PROPERTY( QList<ChangelogEntry> changelog READ changelog CONSTANT )
    Q_PROPERTY( QString lastChangelogAuthor READ lastChangelogAuthor CONSTANT )
    Q_PROPERTY( QString lastChangelogVersion READ lastChangelogVersion CONSTANT )
    Q_PROPERTY( QString lastChangelogDescription READ lastChangelogDescription CONSTANT )

public:
    /**
     * @brief Creates a new TimetableAccessorInfo object.
     *
     * @param accessorType The type of the accessor.
     * @param serviceProviderID The service provider for which this accessor
     *   is designed for.
     *
     * @see AccessorType
     **/
    explicit TimetableAccessorInfo( const AccessorType& accessorType = NoAccessor,
                                    const QString& serviceProviderID = QString(),
                                    QObject *parent = 0 );

    TimetableAccessorInfo( const AccessorType &accessorType, const QString &serviceProviderID,
            const QHash<QString, QString> &names, const QHash<QString, QString> &descriptions,
            const QString &version, const QString &fileVersion, bool useSeparateCityValue,
            bool onlyUseCitiesInList, const QString &url, const QString &shortUrl,
            int minFetchWait, const QString &author, const QString &email,
            VehicleType defaultVehicleType, const QList<ChangelogEntry> &changelog,
            const QStringList &cities,
            const QHash<QString, QString> &cityNameToValueReplacementHash,
            QObject *parent = 0 );

    TimetableAccessorInfo( const TimetableAccessorInfo &info, QObject *parent = 0 );

    virtual ~TimetableAccessorInfo();

    TimetableAccessorInfo *clone( QObject *parent = 0 ) const {
            return new TimetableAccessorInfo(*this, parent); };

    TimetableAccessorInfo &operator =( const TimetableAccessorInfo &info );

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

    /** @brief Get the name of this accessor in the local language if available. */
    QString name() const;
    /** @brief The description in the local language if available of the service provider. */
    QString description() const;
    /** @brief Get the names of this accessor, sorted by language. */
    QHash<QString, QString> names() const { return m_name; };
    /** @brief Description of the service provider, sorted by language. */
    QHash<QString, QString> descriptions() const { return m_description; };
    /** @brief Type of the accessor (HTML, XML) */
    AccessorType accessorType() const { return m_accessorType; };
    /** @brief The version of the accessor information. */
    QString version() const { return m_version; };
    /** @brief The file version of the accessor information. */
    QString fileVersion() const { return m_fileVersion; };
    /** @brief The ID of the service provider this accessor is designed for. */
    QString serviceProvider() const { return m_serviceProviderID; };

    QString typeString() const { return m_accessorType == ScriptedAccessor
            ? "ScriptedAccessor" : "Unknown"; };

    /** @brief The author of the accessor information to be used by the accessor. */
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
    VehicleType defaultVehicleType() const { return m_defaultVehicleType; };

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
    /** @brief The name of the XML file that was parsed to get this accessor information object. */
    QString fileName() const { return m_fileName; };

    /** @brief The file name of the script file to parse downloaded documents. */
    QString scriptFileName() const { return m_scriptFileName; };
    /** @brief A list of QScript extensions to import when executing the script. */
    QStringList scriptExtensions() const { return m_scriptExtensions; };

    QList<ChangelogEntry> changelog() const { return m_changelog; };
    QString lastChangelogAuthor() const {
        return m_changelog.isEmpty() ? QString() : m_changelog.first().author; };
    QString lastChangelogVersion() const {
        return m_changelog.isEmpty() ? QString() : m_changelog.first().version; };
    QString lastChangelogDescription() const {
        return m_changelog.isEmpty() ? QString() : m_changelog.first().description; };

    const QHash<QString, QString> &cityNameToValueReplacementHash() const {
        return m_hashCityNameToValue; };

// Where appropriate "const TimetableAccessorInfo*" gets used, so that setters are not available
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
     * @brief Set the name of the XML file that was parsed to get this accessor information object.
     *
     * If @p fileName is a symlink the real file name gets retrieved (using
     * KStandardDirs::realFilePath, eg. for the default service providers ending with "_default.xml").
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
     * @brief Set the name of this accessor. The name is displayed in the config dialog's
     *   service provider combobox.
     *
     * @param names The new name of this accessor.
     **/
    void setNames( const QHash<QString, QString> &names ) { m_name = names; };

    /**
     * @brief Set the ID of the service provider this accessor is designed for.
     *
     * @param serviceProvider The ID of the service provider.
     **/
    void setServiceProvider( const QString &serviceProvider ) {
        m_serviceProviderID = serviceProvider; };

    /**
     * @brief Set the type of the accessor.
     *
     * @param type The type of the accessor.
     **/
    void setType( AccessorType type ) { m_accessorType = type; };

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
     * @brief Set the description of this accessor.
     *
     * @param descriptions A description of this accessor. */
    void setDescriptions( const QHash<QString, QString> &descriptions ) {
        m_description = descriptions; };

    /**
     * @brief Set the author of this accessor. You can also set the email of the author.
     *
     * @param author The author of this accessor.
     * @param shortAuthor An abbreviation of the authors name.
     * @param email The email address of the author.
     **/
    void setAuthor( const QString &author, const QString &shortAuthor, const QString &email = QString() );

    /**
     * @brief Set the version of this accessor.
     *
     * The version of the accessor should be updated whenever its values or parsing implementations
     * gets changed.
     *
     * @param version The version of this accessor.
     **/
    void setVersion( const QString &version ) { m_version = version; };

    /**
     * @brief Set the file version of this accessor.
     *
     * The "file version" names the used version of the PublicTransport engine plugin interface.
     * Currently only version 1.1 is supported.
     * Accessors written for version 1.0 need to be
     * updated. But that mostly means adding a 'getTimetable()' function to download a timetable
     * document and calling the old 'parseTimetable()' function if the download is finished. The
     * same can be done with 'getStopsuggestions()' and 'getJourneys()'. The return values now need
     * to be JavaScript objects containing properties named after the enumerables in
     * TimetableInformation.
     *
     * @param fileVersion The file version of this accessor.
     **/
    void setFileVersion( const QString &fileVersion ) { m_fileVersion = fileVersion; };

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

    void setDefaultVehicleType( VehicleType vehicleType ) {
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

protected:
    virtual bool supportsByJourneyNewsParsing( const TimetableInformation &info ) const {
        Q_UNUSED(info)
        return false; };

    static int versionNumberFromString( const QString &version, int *startPos );

    // The name of the XML file that was parsed to get this accessor information object
    QString m_fileName;
    // The file name of the script file to parse downloaded documents
    QString m_scriptFileName;
    // A list of QScript extensions to import when executing the script
    QStringList m_scriptExtensions;
    // The names of this accessor, sorted by language, which can be displayed by the visualization
    QHash<QString, QString> m_name;
    // A short version of the url without protocol or "www"  to be displayed in links
    QString m_shortUrl;
    // A description of the service provider, sorted by language
    QHash<QString, QString> m_description;
    // The author of the accessor information to be used by the accessor
    QString m_author;
    // An abbreviation of the authors name
    QString m_shortAuthor;
    // The email address of the author
    QString m_email;
    // The version of the accessor information
    QString m_version;
    // The version of the PublicTransport engine plugin interface
    QString m_fileVersion;
    // The main/home url of the service provider
    QString m_url;
    // If empty, use unicode (QUrl::toPercentEncoding()), otherwise use own
    // toPercentEncoding() with this charset
    QByteArray m_charsetForUrlEncoding, m_fallbackCharset;

    // Keys are versions, where the change entries occurred (values)
    QList<ChangelogEntry> m_changelog;

    // Type of the accessor (currently only HTML, GTFS is available in separate GIT branch)
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

    // Sample data, used to test accessors
    QStringList m_sampleStopNames; // For journeys at least two stop names are required
    QString m_sampleCity;
};

Q_DECLARE_METATYPE( TimetableAccessorInfo );

#endif // TIMETABLEACCESSOR_INFO_HEADER
