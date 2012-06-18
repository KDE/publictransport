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

// Own includes
#include "settings.h"

Settings::Settings()
{
    m_currentStopIndex = 0;
}

Settings::Settings( const Settings& other )
{
    *this = other;
}

Settings::~Settings()
{
}

void Settings::favorJourneySearch( const QString &journeySearch )
{
    QList< JourneySearchItem > journeySearches = currentJourneySearches();
    for ( int i = 0; i < journeySearches.count(); ++i ) {
        if ( journeySearches[i].journeySearch() == journeySearch ) {
            journeySearches[i].setFavorite( true );
            setCurrentJourneySearches( journeySearches );
            break;
        }
    }
}

void Settings::removeJourneySearch( const QString &journeySearch )
{
    QList< JourneySearchItem > journeySearches = currentJourneySearches();
    for ( int i = 0; i < journeySearches.count(); ++i ) {
        if ( journeySearches[i].journeySearch() == journeySearch ) {
            journeySearches.removeAt( i );
            setCurrentJourneySearches( journeySearches );
            break;
        }
    }
}

void Settings::addRecentJourneySearch( const QString &journeySearch )
{
    QList< JourneySearchItem > journeySearches = currentJourneySearches();
    for ( int i = 0; i < journeySearches.count(); ++i ) {
        if ( journeySearches[i].journeySearch() == journeySearch ) {
            // Do not add already existing journey search strings
            return;
        }
    }

    // The given journeySearch string is not already in journeySearches, add it
    journeySearches << JourneySearchItem( journeySearch );
    setCurrentJourneySearches( journeySearches );
}

bool AlarmSettings::isOneTimeAlarm() const
{
    return type == AlarmRemoveAfterFirstMatch || filter.isOneTimeFilter();
}

bool AlarmSettings::isExpired() const
{
    if ( !isOneTimeAlarm() ) {
        return false;
    }

    return filter.isExpired();
}

QStringList AlarmSettingsList::names() const {
    QStringList ret;
    foreach ( const AlarmSettings &alarm, *this ) {
        ret << alarm.name;
    }
    return ret;
}

bool AlarmSettingsList::hasName( const QString& name ) const {
    foreach ( const AlarmSettings &alarm, *this ) {
        if ( alarm.name == name ) {
            return true;
        }
    }
    return false; // No alarm with the given name found
}

AlarmSettings AlarmSettingsList::byName( const QString& name ) const {
    foreach ( const AlarmSettings &alarm, *this ) {
        if ( alarm.name == name ) {
            return alarm;
        }
    }
    return AlarmSettings(); // No alarm with the given name found
}

void AlarmSettingsList::removeByName( const QString& name ) {
    for ( int i = 0; i < count(); ++i ) {
        if ( operator[](i).name == name ) {
            removeAt( i );
            return;
        }
    }

    kDebug() << "No alarm with the given name found:" << name;
    kDebug() << "Available names are:" << names();
}

void AlarmSettingsList::set( const AlarmSettings& newAlarmSettings ) {
    for ( int i = 0; i < count(); ++i ) {
        if ( operator[](i).name == newAlarmSettings.name ) {
            operator[]( i ) = newAlarmSettings;
            return;
        }
    }

    // No alarm with the given name found, add newAlarmSettings to this list
    *this << newAlarmSettings;
}

bool AlarmSettingsList::removeAlarm( const AlarmSettings &alarm )
{
    for ( int i = 0; i < count(); ++i ) {
        const AlarmSettings currentAlarm = operator[]( i );
        if ( currentAlarm.equals(alarm) ) {
            removeAt( i );
            return true;
        }
    }
    return false;
}

bool operator ==( const AlarmSettings &l, const AlarmSettings &r )
{
    return l.name == r.name && l.enabled == r.enabled && l.type == r.type
        && l.affectedStops == r.affectedStops && l.filter == r.filter
        && l.lastFired == r.lastFired;
}

bool operator==( const ColorGroupSettings &l, const ColorGroupSettings &r )
{
    return l.color == r.color && l.filters == r.filters && l.filterOut == r.filterOut
        && l.lastCommonStopName == r.lastCommonStopName;
}

ColorGroupSettings ColorGroupSettingsList::byColor( const QColor &color ) {
    foreach ( const ColorGroupSettings &colorSettings, *this ) {
        if ( colorSettings.color == color ) {
            return colorSettings;
        }
    }

    // No color group with the given color found, return an "empty" object
    return ColorGroupSettings();
}

void ColorGroupSettingsList::set( const ColorGroupSettings &newColorGroupSettings ) {
    for ( int i = 0; i < count(); ++i ) {
        if ( operator[](i).color == newColorGroupSettings.color ) {
            operator[]( i ) = newColorGroupSettings;
            return;
        }
    }

    // No color group with the given color found, add newColorGroupSettings to this list
    *this << newColorGroupSettings;
}

bool ColorGroupSettingsList::hasColor( const QColor &color ) const {
    for ( int i = 0; i < count(); ++i ) {
        if ( operator[](i).color == color ) {
            return true;
        }
    }

    return false;
}

bool ColorGroupSettingsList::removeColor( const QColor &color ) {
    for ( int i = 0; i < count(); ++i ) {
        if ( operator[](i).color == color ) {
            removeAt( i );
            return true;
        }
    }

    return false;
}

void ColorGroupSettingsList::enableColorGroup( const QColor &color, bool enable ) {
    for ( int i = 0; i < count(); ++i ) {
        if ( operator[](i).color == color ) {
            operator[](i).filterOut = !enable;
            return;
        }
    }
}

bool ColorGroupSettingsList::filterOut( const Timetable::DepartureInfo &departureInfo ) const {
    foreach ( const ColorGroupSettings &colorSettings, *this ) {
        if ( colorSettings.filterOut && colorSettings.matches(departureInfo) ) {
            return true;
        }
    }

    return false;
}

void Settings::adjustColorGroupSettingsCount()
{
    while ( m_colorGroups.count() < m_stops.count() ) {
        m_colorGroups << ColorGroupSettingsList();
    }
    while ( m_colorGroups.count() > m_stops.count() ) {
        m_colorGroups.removeLast();
    }
}

FilterSettingsList Settings::currentFilters() const
{
    // Construct filter settings per stop (constructed of multiple filter configs)
    FilterSettingsList activeFilterSettings;
    foreach( const FilterSettings &filters, m_filters ) {
        if ( filters.affectedStops.contains(m_currentStopIndex) ) {
            activeFilterSettings << filters;
        }
    }
    return activeFilterSettings;
}

ColorGroupSettingsList Settings::currentColorGroups() const
{
    if ( m_currentStopIndex >= 0 &&
         m_currentStopIndex < m_colorGroups.count() )
    {
        return m_colorGroups[ m_currentStopIndex ];
    } else {
        return ColorGroupSettingsList();
    }
}

QFont Settings::sizedFont() const
{
    QFont f = m_font;
    if ( f.pointSize() == -1 ) {
        int pixelSize = f.pixelSize() * m_sizeFactor;
        f.setPixelSize( pixelSize > 0 ? pixelSize : 1 );
    } else {
        int pointSize = f.pointSize() * m_sizeFactor;
        f.setPointSize( pointSize > 0 ? pointSize : 1 );
    }
    return f;
}

bool Settings::checkConfig()
{
    // TODO: Check when adding stops in StopSettingsDialog
    if ( m_stops.isEmpty() ) {
        return false;
    } else {
        foreach ( const StopSettings &stop, m_stops ) {
            if ( stop.stops().isEmpty() ) {
                return false;
            } else {
                foreach ( const QString &stop, stop.stops() ) {
                    if ( stop.isEmpty() ) {
                        return false;
                    }
                }
            }
        }
    }
    return true;

    //     if ( m_useSeparateCityValue && (m_city.isEmpty()
    //         || m_stops.isEmpty() || m_stops.first().isEmpty()) )
    //     emit configurationRequired(true, i18n("Please set a city and a stop."));
    //     else if ( m_stops.isEmpty() || m_stops.first().isEmpty() )
    //     emit configurationRequired(true, i18n("Please set a stop."));
    //     else if ( m_serviceProvider == "" )
    //     emit configurationRequired(true, i18n("Please select a service provider."));
    //     else {
    //     emit configurationRequired(false);
    //     return true;
    //     }
    //
    //     return false;
}
