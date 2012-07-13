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

// Header
#include "serviceproviderdata.h"
#include "serviceproviderglobal.h"

// KDE includes
#include <KLocalizedString>
#include <KDebug>
#include <KStandardDirs>
#include <KLocale>
#include <QUrl>

class ChangelogEntryGreaterThan
{
public:
    inline bool operator()( const ChangelogEntry& l, const ChangelogEntry& r ) const {
        int comparison = ServiceProviderData::compareVersions( l.version, r.version );
        if ( comparison == 0 ) {
            // Versions are equal, compare authors
            return l.author.compare( r.author, Qt::CaseInsensitive ) < 0;
        } else {
            // If comparison is 1, the left version is bigger than the right one,
            // if it is -1, the right one is bigger
            return comparison > 0;
        }
    };
};

ServiceProviderData::ServiceProviderData( const Enums::ServiceProviderType& type,
                                          const QString& id, QObject *parent )
        : QObject(parent)
{
    m_serviceProviderType = type;
    m_id = id;
    m_version = "1.0";
    m_fileFormatVersion = "1.1";
    m_useSeparateCityValue = false;
    m_onlyUseCitiesInList = false;
    m_defaultVehicleType = Enums::UnknownVehicleType;
    m_minFetchWait = 0;

    qRegisterMetaType< Enums::ServiceProviderType >( "Enums::ServiceProviderType" );
}

ServiceProviderData::ServiceProviderData( const Enums::ServiceProviderType &type,
        const QString &id, const QHash< QString, QString > &names,
        const QHash< QString, QString >& descriptions, const QString &version,
        const QString &fileVersion, bool useSeparateCityValue, bool onlyUseCitiesInList,
        const QString &url, const QString &shortUrl, int minFetchWait, const QString &author,
        const QString &email, Enums::VehicleType defaultVehicleType,
        const QList< ChangelogEntry >& changelog, const QStringList &cities,
        const QHash< QString, QString >& cityNameToValueReplacementHash,
        QObject *parent )
        : QObject(parent)
{
    m_serviceProviderType = type;
    m_id = id;
    m_name = names;
    m_description = descriptions;
    m_version = version;
    m_fileFormatVersion = fileVersion;
    m_useSeparateCityValue = useSeparateCityValue;
    m_onlyUseCitiesInList = onlyUseCitiesInList;
    m_url = url;
    m_shortUrl = shortUrl;
    m_minFetchWait = minFetchWait;
    m_author = author;
    m_email = email;
    m_defaultVehicleType = defaultVehicleType;
    m_changelog = changelog;
    m_cities = cities;
    m_hashCityNameToValue = cityNameToValueReplacementHash;
}

ServiceProviderData::ServiceProviderData( const ServiceProviderData &data, QObject *parent )
        : QObject(parent)
{
    // Use assignment operator for initialization
    operator=( data );
}

ServiceProviderData::~ServiceProviderData()
{
}

ServiceProviderData &ServiceProviderData::operator=( const ServiceProviderData &data )
{
    m_serviceProviderType = data.m_serviceProviderType;
    m_id = data.m_id;
    m_name = data.m_name;
    m_description = data.m_description;
    m_version = data.m_version;
    m_fileFormatVersion = data.m_fileFormatVersion;
    m_useSeparateCityValue = data.m_useSeparateCityValue;
    m_onlyUseCitiesInList = data.m_onlyUseCitiesInList;
    m_url = data.m_url;
    m_shortUrl = data.m_shortUrl;
    m_minFetchWait = data.m_minFetchWait;
    m_author = data.m_author;
    m_shortAuthor = data.m_shortAuthor;
    m_email = data.m_email;
    m_defaultVehicleType = data.m_defaultVehicleType;
    m_changelog = data.m_changelog;
    m_country = data.m_country;
    m_cities = data.m_cities;
    m_credit = data.m_credit;
    m_hashCityNameToValue = data.m_hashCityNameToValue;
    m_fileName = data.m_fileName;
    m_charsetForUrlEncoding = data.m_charsetForUrlEncoding;
    m_fallbackCharset = data.m_fallbackCharset;
    m_sampleStopNames = data.m_sampleStopNames;
    m_sampleCity = data.m_sampleCity;
    m_notes = data.m_notes;

    // For ScriptedProvider
    m_scriptFileName = data.m_scriptFileName;
    m_scriptExtensions = data.m_scriptExtensions;

    // For GtfsProvider
    m_feedUrl = data.m_feedUrl;
    m_tripUpdatesUrl = data.m_tripUpdatesUrl;
    m_alertsUrl = data.m_alertsUrl;
    m_timeZone = data.m_timeZone;
    return *this;
}

bool ServiceProviderData::operator ==( const ServiceProviderData &data ) const
{
    return m_serviceProviderType == data.m_serviceProviderType &&
           m_id == data.m_id &&
           m_name == data.m_name &&
           m_description == data.m_description &&
           m_version == data.m_version &&
           m_fileFormatVersion == data.m_fileFormatVersion &&
           m_useSeparateCityValue == data.m_useSeparateCityValue &&
           m_onlyUseCitiesInList == data.m_onlyUseCitiesInList &&
           m_url == data.m_url &&
           m_shortUrl == data.m_shortUrl &&
           m_minFetchWait == data.m_minFetchWait &&
           m_author == data.m_author &&
           m_shortAuthor == data.m_shortAuthor &&
           m_email == data.m_email &&
           m_defaultVehicleType == data.m_defaultVehicleType &&
           m_changelog == data.m_changelog &&
           m_country == data.m_country &&
           m_cities == data.m_cities &&
           m_credit == data.m_credit &&
           m_hashCityNameToValue == data.m_hashCityNameToValue &&
           m_fileName == data.m_fileName &&
           m_charsetForUrlEncoding == data.m_charsetForUrlEncoding &&
           m_fallbackCharset == data.m_fallbackCharset &&
           m_sampleStopNames == data.m_sampleStopNames &&
           m_sampleCity == data.m_sampleCity &&
           m_notes == data.m_notes &&

           // For ScriptedProvider
           m_scriptFileName == data.m_scriptFileName &&
           m_scriptExtensions == data.m_scriptExtensions &&

           // For GtfsProvider
           m_feedUrl == data.m_feedUrl &&
           m_tripUpdatesUrl == data.m_tripUpdatesUrl &&
           m_alertsUrl == data.m_alertsUrl &&
           m_timeZone == data.m_timeZone;
}

void ServiceProviderData::finish()
{
    // Generate a short URL if none is given
    if ( m_shortUrl.isEmpty() ) {
        m_shortUrl = shortUrlFromUrl( m_url );
    }

    // Generate a short author name if none is given
    if ( m_shortAuthor.isEmpty() && !m_author.isEmpty() ) {
        m_shortAuthor = shortAuthorFromAuthor( m_author );
    }

    // Use script author as author of the change entry if no one else was set
    for ( int i = 0; i < m_changelog.count(); ++i ) {
        if ( m_changelog[i].author.isEmpty() ) {
            m_changelog[i].author = m_shortAuthor;
        }
    }
    qStableSort( m_changelog.begin(), m_changelog.end(), ChangelogEntryGreaterThan() );
}

QString ServiceProviderData::typeString() const
{
    return ServiceProviderGlobal::typeToString( m_serviceProviderType );
}

QString ServiceProviderData::typeName( ) const
{
    return ServiceProviderGlobal::typeName( m_serviceProviderType );
}

int ServiceProviderData::versionNumberFromString( const QString &version, int *startPos ) {
    bool ok;
    int versionNumber;
    int nextPointPos = version.indexOf( '.', *startPos );
    if ( nextPointPos <= 0 ) {
        // No point found in version after startPos, get last number until the end of the string
        versionNumber = version.mid( *startPos ).toInt( &ok );
        *startPos = -1;
    } else {
        // Found a point after startPos in version, extract the number from version
        versionNumber = version.mid( *startPos, nextPointPos - *startPos ).toInt( &ok );
        *startPos = nextPointPos + 1;
    }

    if ( ok ) {
        return versionNumber;
    } else {
        kDebug() << "Version is invalid:" << version;
        return -1;
    }
}

int ServiceProviderData::compareVersions( const QString &version1, const QString &version2 )
{
    int pos1 = 0;
    int pos2 = 0;
    forever {
        int versionNumber1 = versionNumberFromString( version1, &pos1 );
        int versionNumber2 = versionNumberFromString( version2, &pos2 );
        if ( versionNumber1 < 0 || versionNumber2 < 0 ) {
            // Invalid version strings
            return 0;
        }

        if ( versionNumber1 < versionNumber2 ) {
            return -1; // Version 1 is smaller than version 2
        } else if ( versionNumber1 > versionNumber2 ) {
            return 1; // Version 1 is bigger than version 2
        }

        if ( pos1 == -1 && pos2 == -1 ) {
            return 0; // No more version numbers in both version
        } else if ( pos1 == -1 ) {
            return -1; // No more version numbers in version1, but in version2, which is therefore bigger
        } else if ( pos2 == -1 ) {
            return 1; // No more version numbers in version2, but in version1, which is therefore bigger
        }

        // pos1 and pos2 are both >= 0 here
    }

    return 0;
}

QString ServiceProviderData::shortUrlFromUrl( const QString &url )
{
    QString shortUrl = QUrl( url ).toString(
            QUrl::RemoveScheme | QUrl::RemovePort | QUrl::RemovePath |
            QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::StripTrailingSlash );
    while ( shortUrl.startsWith('/') ) {
        shortUrl = shortUrl.mid( 1 );
    }
    return shortUrl;
}

QString ServiceProviderData::shortAuthorFromAuthor( const QString &authorName )
{
    const QStringList names = authorName.toLower().split( ' ', QString::SkipEmptyParts );
    if ( !names.isEmpty() ) {
        // Add first character of all prenames
        QString shortAuthor;
        for ( int i = 0; i < names.count() - 1; ++i ) {
            shortAuthor += names[i][0];
        }

        // Add family name completely
        shortAuthor += names.last();
        return shortAuthor;
    } else {
        return QString();
    }
}

QString ServiceProviderData::name() const
{
    const QString lang = KGlobal::locale()->country();
    return m_name.contains(lang) ? m_name[lang] : m_name["en"];
}

QString ServiceProviderData::description() const
{
    const QString lang = KGlobal::locale()->country();
    return m_description.contains(lang) ? m_description[lang] : m_description["en"];
}

QString ServiceProviderData::notes() const
{
    return m_notes;
}

void ServiceProviderData::setUrl( const QString &url, const QString &shortUrl )
{
    m_url = url;
    m_shortUrl = shortUrl.isEmpty() ? shortUrlFromUrl(url) : shortUrl;
}

void ServiceProviderData::setAuthor( const QString& author, const QString &shortAuthor,
                                       const QString &email )
{
    m_author = author;
    m_shortAuthor = shortAuthor;
    m_email = email;
}

void ServiceProviderData::setFileName(const QString& fileName)
{
    m_fileName = KStandardDirs::realFilePath( fileName );
}

QString ServiceProviderData::mapCityNameToValue( const QString &city ) const
{
    if ( m_hashCityNameToValue.contains( city.toLower() ) ) {
        return m_hashCityNameToValue[city.toLower()];
    } else {
        return city;
    }
}
