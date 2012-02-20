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
#include "timetableaccessor_info.h"

// KDE includes
#include <KLocalizedString>
#include <KDebug>
#include <KStandardDirs>
#include <KLocale>

class ChangelogEntryGreaterThan
{
public:
    inline bool operator()( const ChangelogEntry& l, const ChangelogEntry& r ) const {
        QStringList lVersionParts = l.since_version.split('.');
        QStringList rVersionParts = r.since_version.split('.');
        for ( int i = 0; i < lVersionParts.count() && i < rVersionParts.count(); ++i ) {
            int l = lVersionParts[i].toInt();
            int r = rVersionParts[i].toInt();
            if ( l > r ) {
                return true;
            } else if  ( l < r ) {
                return false;
            }
        }

        if ( lVersionParts.count() > rVersionParts.count() ) {
            return true;
        } else if ( lVersionParts.count() < rVersionParts.count() ) {
            return false;
        }

        return l.author.compare( r.author, Qt::CaseInsensitive ) < 0;
    };
};

TimetableAccessorInfo::TimetableAccessorInfo( const AccessorType& accessorType,
                                              const QString& serviceProviderID )
{
    m_accessorType = accessorType;
    m_serviceProviderID = serviceProviderID;
    m_version = "1.0";
    m_fileVersion = "1.1";
    m_useSeparateCityValue = false;
    m_onlyUseCitiesInList = false;
    m_defaultVehicleType = Unknown;
    m_minFetchWait = 0;
}

TimetableAccessorInfo::TimetableAccessorInfo( const AccessorType &accessorType,
        const QString &serviceProviderID, const QHash< QString, QString > &names,
        const QHash< QString, QString >& descriptions, const QString &version,
        const QString &fileVersion, bool useSeparateCityValue, bool onlyUseCitiesInList,
        const QString &url, const QString &shortUrl, int minFetchWait, const QString &author,
        const QString &email, VehicleType defaultVehicleType,
        const QList< ChangelogEntry >& changelog, const QStringList &cities,
        const QHash< QString, QString >& cityNameToValueReplacementHash )
{
    m_accessorType = accessorType;
    m_serviceProviderID = serviceProviderID;
    m_name = names;
    m_description = descriptions;
    m_version = version;
    m_fileVersion = fileVersion;
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

TimetableAccessorInfo::~TimetableAccessorInfo()
{
}

void TimetableAccessorInfo::finish()
{
    if ( m_shortUrl.isEmpty() ) {
        m_shortUrl = m_url;
    }

    // Generate a short name if none is given ("<first letter of prename><family name>")
    if ( m_shortAuthor.isEmpty() && !m_author.isEmpty() ) {
        int pos = m_author.indexOf(' ');
        if ( m_author.length() < 5 || pos == -1 ) {
            m_shortAuthor = m_author.remove(' ').toLower();
        } else {
            m_shortAuthor = m_author[0].toLower() + m_author.mid(pos + 1).toLower();
        }
    }

    // Use script author as author of the change entry if no one else was set
    for ( int i = 0; i < m_changelog.count(); ++i ) {
        if ( m_changelog[i].author.isEmpty() ) {
            m_changelog[i].author = m_shortAuthor;
        }
    }
    qSort( m_changelog.begin(), m_changelog.end(), ChangelogEntryGreaterThan() );
}

QString TimetableAccessorInfo::name() const
{
    const QString lang = KGlobal::locale()->country();
    return m_name.contains(lang) ? m_name[lang] : QString();
}

QString TimetableAccessorInfo::description() const
{
    const QString lang = KGlobal::locale()->country();
    return m_description.contains(lang) ? m_description[lang] : QString();
}

void TimetableAccessorInfo::setAuthor( const QString& author, const QString &shortAuthor,
                                       const QString &email )
{
    m_author = author;
    m_shortAuthor = shortAuthor;
    m_email = email;
}

void TimetableAccessorInfo::setFileName(const QString& fileName)
{
    m_fileName = KStandardDirs::realFilePath( fileName );
}

QString TimetableAccessorInfo::mapCityNameToValue( const QString &city ) const
{
    if ( m_hashCityNameToValue.contains( city.toLower() ) ) {
        return m_hashCityNameToValue[city.toLower()];
    } else {
        return city;
    }
}
