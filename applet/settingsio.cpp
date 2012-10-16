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
#include "settingsio.h"

// KDE+Plasma includes
#include <Plasma/Theme>
#include <Plasma/DataEngine>
#include <KLocale>

Settings SettingsIO::readSettings( KConfigGroup cg, KConfigGroup cgGlobal,
                                   Plasma::DataEngine *publictransportEngine )
{
    Settings settings;
    if ( !cg.hasKey("departureTimeFlags") && (cg.hasKey("showRemainingMinutes") ||
         cg.hasKey("showDepartureTime") || cg.hasKey("displayTimeBold")) )
    {
        // DEPRECATED This reads settings stored in old format (version < 0.11)
        kDebug() << "Reading settings in old format, will be converted to new format";
//         settings.m_departureTimeFlags = Settings::DoNotShowDepartureTime;
        settings.setShowRemainingTime( cg.readEntry("showRemainingMinutes", true) );
        cg.deleteEntry( "showRemainingMinutes" );


        settings.setDisplayDepartureTimeBold( cg.readEntry("displayTimeBold", true) );
        cg.deleteEntry( "displayTimeBold" );

        cg.writeEntry( "departureTimeFlags", static_cast<int>(settings.departureTimeFlags()) );
        cg.sync();
    } else {
        settings.setDepartureTimeFlags( static_cast< Settings::DepartureTimeFlags >(
                cg.readEntry("departureTimeFlags", static_cast<int>(Settings::DefaultDepartureTimeFlags))) );
    }

    const Settings::AdditionalDataRequestType requestType = static_cast< Settings::AdditionalDataRequestType >(
            cg.readEntry("additionalDataRequestType", static_cast<int>(Settings::DefaultAdditionalDataRequestType)));
    settings.setAdditionalDataRequestType( requestType );

    // Read stop settings TODO: Store in config groups like filters
    StopSettingsList stopSettingsList;
    int stopSettingCount = cgGlobal.readEntry( "stopSettings", 1 );
    QString test = "location";
    int i = 1;
    while ( cgGlobal.hasKey(test) ) {
        StopSettings stopSettings;
        QString suffix = i == 1 ? QString() : '_' + QString::number( i );
        stopSettings.set( LocationSetting, cgGlobal.readEntry("location" + suffix, "showAll") );
        stopSettings.set( ServiceProviderSetting,
                          cgGlobal.readEntry("serviceProvider" + suffix, "de_db") );
        stopSettings.set( CitySetting, cgGlobal.readEntry("city" + suffix, QString()) );
        stopSettings.setStops( cgGlobal.readEntry("stop" + suffix, QStringList()),
                               cgGlobal.readEntry("stopID" + suffix, QStringList()) );
        stopSettings.set( TimeOffsetOfFirstDepartureSetting,
                          cgGlobal.readEntry("timeOffsetOfFirstDeparture" + suffix, 0) );
        stopSettings.set( TimeOfFirstDepartureSetting,
                          QTime::fromString(cgGlobal.readEntry("timeOfFirstDepartureCustom" + suffix,
                                                               "12:00"), "hh:mm") );
        stopSettings.set( FirstDepartureConfigModeSetting,
                          cgGlobal.readEntry("firstDepartureConfigMode" + suffix,
                                             static_cast<int>(RelativeToCurrentTime)) );
        stopSettings.set( AlarmTimeSetting, cgGlobal.readEntry("alarmTime" + suffix, 5) );

        // Read favorite/recent journey search items for the current stop settings
        QByteArray journeySearchesData =
                cgGlobal.readEntry( "journeySearches" + suffix, QByteArray() );
        stopSettings.set( Settings::JourneySearchSetting, QVariant::fromValue(
                SettingsIO::decodeJourneySearchItems(&journeySearchesData)) );
        stopSettingsList << stopSettings;

        ++i;
        test = "location_" + QString::number( i );
        if ( i > stopSettingCount ) {
            break;
        }
    }

    settings.setCurrentStop( cg.readEntry("currentStopIndex", 0) );

    // Add initial stop settings when no settings are available
    if ( stopSettingsList.isEmpty() ) {
        kDebug() << "Stop settings list in settings is empty";
        if ( publictransportEngine ) {
            QString countryCode = KGlobal::locale()->country();
            Plasma::DataEngine::Data locationData = publictransportEngine->query( "Locations" );
            QString defaultServiceProviderId =
                    locationData[countryCode].toHash()["defaultProvider"].toString();

            StopSettings stopSettings;
            if ( defaultServiceProviderId.isEmpty() ) {
                stopSettings.set( LocationSetting, "showAll" );
            } else {
                stopSettings.set( LocationSetting, countryCode );
                stopSettings.set( ServiceProviderSetting, defaultServiceProviderId );
            }
            stopSettings.setStop( QString() );
            // TODO: Get initial stop names using StopFinder

            stopSettingsList << stopSettings;
        } else {
            stopSettingsList << StopSettings();
        }

    }
    settings.setStops( stopSettingsList );

    if ( settings.currentStopIndex() < 0 ) {
        settings.setCurrentStop( 0 ); // For compatibility with versions < 0.7
    } else if ( settings.currentStopIndex() >= settings.stops().count() ) {
        kDebug() << "Current stop index in settings invalid";
        settings.setCurrentStop( settings.stops().count() - 1 );
    }

    settings.setMaximalNumberOfDepartures( cg.readEntry("maximalNumberOfDepartures", 50) );
    settings.setLinesPerRow( cg.readEntry("linesPerRow", 2) );
    settings.setSizeFactor( Settings::sizeFactorFromSize(cg.readEntry("size", 2)) );
    settings.setDepartureArrivalListType( static_cast<DepartureArrivalListType>(
            cg.readEntry("departureArrivalListType", static_cast<int>(DepartureList))) );

    settings.setDrawShadows( cg.readEntry("drawShadows", true) );
    settings.setHideTargetColumn( cg.readEntry("hideColumnTarget", false) );
    settings.setColorize( cg.readEntry("colorize", true) );

    QString fontFamily = cg.readEntry( "fontFamily", QString() );
    settings.setUseThemeFont( fontFamily.isEmpty() );
    if ( !settings.useThemeFont() ) {
        settings.setFont( QFont(fontFamily) );
    } else {
        settings.setFont( Plasma::Theme::defaultTheme()->font(Plasma::Theme::DefaultFont) );
    }

    // ***************** DEPRECATED BEGIN *****************************************************
    // *** Used for migration of filter settings from versions prior to version 0.10 RC1 ******
    FilterSettingsList filtersList;
    if ( cgGlobal.hasKey("filterConfigurationList") ) {
        kDebug() << "DEPRECATED Filter settings will be restructured for new version";
        QStringList filterConfigurationList =
                cgGlobal.readEntry( "filterConfigurationList", QStringList() );
        for ( int i = filterConfigurationList.count() - 1; i >= 0; --i ) {
            const QString &filterConfiguration = filterConfigurationList[ i ];
            if ( filterConfiguration.isEmpty() ) {
                filterConfigurationList.removeAt( i );
            }
        }

        kDebug() << "Config group list" << cgGlobal.groupList();
        kDebug() << "Filter config list:" << filterConfigurationList;

        // Read old filter settings
        foreach( const QString &filterConfiguration, filterConfigurationList ) {
            FilterSettings filters =
                    readFilterConfig( cgGlobal.group("filterConfig_" + filterConfiguration) );
            filters.name = filterConfiguration;
            filtersList << filters;
        }
    // ***************** DEPRECATED END *******************************************************
    } else {
        // New filter storage structure
        int filterCount = cgGlobal.readEntry( "filterCount", 0 );
        test = "filterConfig_1";
        i = 1;
        while ( i <= filterCount && cgGlobal.hasGroup(test) ) {
            FilterSettings filters = readFilterConfig( cgGlobal.group(test) );
            if ( filters.name.isEmpty() ) {
                kDebug() << "Filter settings without a name found!";
                filters.name = "Unnamed" + QString::number( i );
            }
            filtersList << filters;

            ++i;
            test = "filterConfig_" + QString::number( i );
        }
    }
    settings.setFilters( filtersList );

    // Read alarm settings
    //   TODO Store in config groups like filters
    AlarmSettingsList alarms;
    int alarmCount = cgGlobal.readEntry( "alarmCount", 0 );
    test = "alarmType";
    i = 1;
    while ( i <= alarmCount && cgGlobal.hasKey(test) ) {
        AlarmSettings alarm;
        QString suffix = i == 1 ? QString() : '_' + QString::number( i );
        alarm.type = static_cast<AlarmType>(
                cgGlobal.readEntry("alarmType" + suffix, static_cast<int>(AlarmRemoveAfterFirstMatch)) );
        alarm.affectedStops = cgGlobal.readEntry( "alarmStops" + suffix, QList<int>() );
        alarm.enabled = cgGlobal.readEntry( "alarmEnabled" + suffix, true );
        alarm.name = cgGlobal.readEntry( "alarmName" + suffix, "Unnamed" );
        alarm.lastFired = cgGlobal.readEntry( "alarmLastFired" + suffix, QDateTime() );
        alarm.autoGenerated = cgGlobal.readEntry( "alarmAutogenerated" + suffix, false );
        QByteArray baAlarmFilter = cgGlobal.readEntry( "alarmFilter" + suffix, QByteArray() );
        alarm.filter.fromData( baAlarmFilter );

        // Use date and time of one-time alarms to check if they are expired
        if ( alarm.isOneTimeAlarm() && alarm.isExpired() ) {
            kDebug() << "Removing one expired one-time alarm";
        } else {
            alarms << alarm;
        }

        ++i;
        test = "alarmType_" + QString::number( i );
    }
    settings.setAlarms( alarms );

    return settings;
}

SettingsIO::ChangedFlags SettingsIO::writeSettings( const Settings &settings,
        const Settings &oldSettings, KConfigGroup cg, KConfigGroup cgGlobal )
{
    ChangedFlags changed = NothingChanged;

    if ( settings.currentStopIndex() != oldSettings.currentStopIndex() ) {
        cg.writeEntry( "currentStopIndex", settings.currentStopIndex() );
        changed |= IsChanged | ChangedCurrentStop | ChangedCurrentStopSettings;
    }

    // Write stop settings
    if ( settings.stops() != oldSettings.stops() ) {
        changed |= IsChanged | ChangedStopSettings;

        // Get current stop settings and compare journey search lists
        const StopSettings stopSettings = settings.currentStop();
        const StopSettings oldStopSettings = oldSettings.currentStop();
        if ( stopSettings.get< QList<JourneySearchItem> >(Settings::JourneySearchSetting) !=
             oldStopSettings.get< QList<JourneySearchItem> >(Settings::JourneySearchSetting) )
        {
            kDebug() << "Changed journey search list";
            changed |= ChangedCurrentJourneySearchLists;
        }

        // Get QHash with the current stop settings, remove values that do not require
        // timetable data to be requested again and compare the remaining settings
        QHash<int, QVariant> stopSettingsRequireRequest = stopSettings.settings();
        QHash<int, QVariant> oldStopSettingsRequireRequest = oldStopSettings.settings();
        stopSettingsRequireRequest.remove( AlarmTimeSetting );
        oldStopSettingsRequireRequest.remove( AlarmTimeSetting );
        stopSettingsRequireRequest.remove( Settings::JourneySearchSetting );
        oldStopSettingsRequireRequest.remove( Settings::JourneySearchSetting );
        if ( stopSettingsRequireRequest != oldStopSettingsRequireRequest ) {
            changed |= ChangedCurrentStopSettings;
        }

        int i = 1;
        cgGlobal.writeEntry( "stopSettings", settings.stops().count() ); // Not needed if deleteEntry/Group works, don't know what's wrong (sync() and Plasma::Applet::configNeedsSaving() doesn't help)

        foreach( const StopSettings &stopSettings, settings.stops() ) {
            QString suffix = i == 1 ? QString() : '_' + QString::number( i );
            cgGlobal.writeEntry( "location" + suffix,
                    stopSettings.get<QString>(LocationSetting) );
            cgGlobal.writeEntry( "serviceProvider" + suffix,
                    stopSettings.get<QString>(ServiceProviderSetting) );
            cgGlobal.writeEntry( "city" + suffix, stopSettings.get<QString>(CitySetting) );
            cgGlobal.writeEntry( "stop" + suffix, stopSettings.stops() );
            cgGlobal.writeEntry( "stopID" + suffix, stopSettings.stopIDs() );
            cgGlobal.writeEntry( "timeOffsetOfFirstDeparture" + suffix,
                    stopSettings.get<int>(TimeOffsetOfFirstDepartureSetting) );
            cgGlobal.writeEntry( "timeOfFirstDepartureCustom" + suffix,
                    stopSettings.get<QTime>(TimeOfFirstDepartureSetting).toString("hh:mm") );
            cgGlobal.writeEntry( "firstDepartureConfigMode" + suffix,
                    stopSettings.get<int>(FirstDepartureConfigModeSetting) );
            cgGlobal.writeEntry( "alarmTime" + suffix, stopSettings.get<int>(AlarmTimeSetting) );

            // Write journey search items in encoded form
            const QByteArray journeySearchesData = SettingsIO::encodeJourneySearchItems(
                    stopSettings.get< QList<JourneySearchItem> >(Settings::JourneySearchSetting) );
            cgGlobal.writeEntry( "journeySearches" + suffix, journeySearchesData );
            ++i;
        }

        // Delete old stop settings entries
        QString test = "location_" + QString::number( i );
        while ( cgGlobal.hasKey( test ) ) {
            QString suffix = '_' + QString::number( i );
            cgGlobal.deleteEntry( "location" + suffix );
            cgGlobal.deleteEntry( "serviceProvider" + suffix );
            cgGlobal.deleteEntry( "city" + suffix );
            cgGlobal.deleteEntry( "stop" + suffix );
            cgGlobal.deleteEntry( "stopID" + suffix );
            cgGlobal.deleteEntry( "timeOffsetOfFirstDeparture" + suffix );
            cgGlobal.deleteEntry( "timeOfFirstDepartureCustom" + suffix );
            cgGlobal.deleteEntry( "firstDepartureConfigMode" + suffix );
            cgGlobal.deleteEntry( "alarmTime" + suffix );
            cgGlobal.deleteEntry( "journeySearches" + suffix );
            ++i;
            test = "location_" + QString::number( i );
        }
    }

    if ( settings.settingsFlags() != oldSettings.settingsFlags() ) {
        if ( settings.drawShadows() != oldSettings.drawShadows() ) {
            cg.writeEntry( "drawShadows", settings.drawShadows() );
            changed |= IsChanged | ChangedShadows;
        }
        if ( settings.hideTargetColumn() != oldSettings.hideTargetColumn() ) {
            cg.writeEntry( "hideColumnTarget", settings.hideTargetColumn() );
            changed |= IsChanged | ChangedTargetColumn;
        }
        if ( settings.useThemeFont() != oldSettings.useThemeFont() ||
            (!settings.useThemeFont() && settings.font() != oldSettings.font()) )
        {
            cg.writeEntry( "fontFamily", settings.useThemeFont()
                           ? QString() : settings.font().family() );
            changed |= IsChanged | ChangedFont;
        }
        if ( settings.colorize() != oldSettings.colorize() ) {
            cg.writeEntry( "colorize", settings.colorize() );
            changed |= IsChanged | ChangedColorization;
        }
    }

    if ( settings.departureArrivalListType() != oldSettings.departureArrivalListType() ) {
        cg.writeEntry( "departureArrivalListType",
                        static_cast<int>(settings.departureArrivalListType()) );
        changed |= IsChanged | ChangedServiceProvider | ChangedDepartureArrivalListType;
    }

    if ( settings.departureTimeFlags() != oldSettings.departureTimeFlags() ) {
        cg.writeEntry( "departureTimeFlags", static_cast<int>(settings.departureTimeFlags()) );
        changed |= IsChanged | ChangedDepartureTimeSettings;
    }

    if ( settings.additionalDataRequestType() != oldSettings.additionalDataRequestType() ) {
        cg.writeEntry( "additionalDataRequestType", static_cast<int>(settings.additionalDataRequestType()) );
        changed |= IsChanged | ChangedServiceProvider;
    }

    if ( settings.maximalNumberOfDepartures() != oldSettings.maximalNumberOfDepartures() ) {
        cg.writeEntry( "maximalNumberOfDepartures", settings.maximalNumberOfDepartures() );
        changed |= IsChanged | ChangedServiceProvider;
    }

    if ( settings.linesPerRow() != oldSettings.linesPerRow() ) {
        cg.writeEntry( "linesPerRow", settings.linesPerRow() );
        changed |= IsChanged | ChangedLinesPerRow;
    }

    if ( settings.sizeFactor() != oldSettings.sizeFactor() ) {
        cg.writeEntry( "size", Settings::sizeFromSizeFactor(settings.sizeFactor()) );
        changed |= IsChanged | ChangedSizeFactor;
    }

    // ***************** DEPRECATED BEGIN *****************************************************
    // *** Used for migration of filter settings from versions prior to version 0.10 Beta 9 ***
    if ( cgGlobal.hasKey("filterConfigurationList") ) {
        // Read deprecated filter configuration names
        QStringList filterConfigurationList =
                cgGlobal.readEntry( "filterConfigurationList", QStringList() );
        for ( int i = filterConfigurationList.count() - 1; i >= 0; --i ) {
            const QString &filterConfiguration = filterConfigurationList[ i ];
            if ( filterConfiguration.isEmpty() ) {
                filterConfigurationList.removeAt( i );
            }
        }

        kDebug() << "Delete deprecated entry \"filterConfigurationList\"";
        cgGlobal.deleteEntry( "filterConfigurationList" );

        // Delete deprecated filter settings
        foreach( const QString &group, cgGlobal.groupList() ) {
            if ( !filterConfigurationList.contains(
                    group.mid(QString("filterConfig_").length())) )
            {
                kDebug() << "Delete deprecated group" << group;
                cgGlobal.deleteGroup( group );
            }
        }

        // Delete filter configuration names in stop settings
        const QString filterConfigurationKey = "filterConfiguration";
        QString currentFilterConfigurationKey = filterConfigurationKey;
        int i = 2;
        while ( cgGlobal.hasKey(currentFilterConfigurationKey) ) {
            kDebug() << "Delete deprecated filter using entry" << currentFilterConfigurationKey;
            cgGlobal.deleteEntry( currentFilterConfigurationKey );

            currentFilterConfigurationKey = filterConfigurationKey + '_' + QString::number(i);
            ++i;
        }

        cgGlobal.sync();
    }

    // ***************** DEPRECATED END *******************************************************

    // Write filter settings
    if ( settings.filters() != oldSettings.filters() ) {
        cgGlobal.writeEntry( "filterCount", settings.filters().count() );
        int i = 1;
        foreach ( const FilterSettings &filters, settings.filters() ) {
            if ( filters.name.isEmpty() ) {
                kDebug() << "Empty filter config name, can't write settings";
                continue;
            }

            if ( oldSettings.filters().hasName(filters.name) ) {
                SettingsIO::writeFilterConfig( filters,
                        oldSettings.filters().byName(filters.name),
                        cgGlobal.group("filterConfig_" + QString::number(i)) );
            } else {
                SettingsIO::writeFilterConfig( filters,
                        cgGlobal.group("filterConfig_" + QString::number(i)) );
            }
            ++i;
        }

        // Delete old filter settings groups
        while ( i <= oldSettings.filters().count() ) {
            cgGlobal.deleteGroup( "filterConfig_" + QString::number(i) );
            cgGlobal.sync();
            ++i;
        }

        changed |= IsChanged | ChangedFilterSettings;
    }

    // Compare (don't write) color group settings
    if ( settings.colorGroups() != oldSettings.colorGroups() ) {
        changed |= IsChanged | ChangedColorGroupSettings;
    }

    // Write alarm settings
    if ( settings.alarms() != oldSettings.alarms() ) {
        changed |= IsChanged | ChangedAlarmSettings;
        int i = 1;
        cgGlobal.writeEntry( "alarmCount", settings.alarms().count() );
        foreach( const AlarmSettings &alarm, settings.alarms() ) {
            QString suffix = i == 1 ? QString() : '_' + QString::number( i );
            cgGlobal.writeEntry( "alarmType" + suffix, static_cast<int>( alarm.type ) );
            cgGlobal.writeEntry( "alarmStops" + suffix, alarm.affectedStops );
            cgGlobal.writeEntry( "alarmFilter" + suffix, alarm.filter.toData() );
            cgGlobal.writeEntry( "alarmEnabled" + suffix, alarm.enabled );
            cgGlobal.writeEntry( "alarmName" + suffix, alarm.name );
            cgGlobal.writeEntry( "alarmLastFired" + suffix, alarm.lastFired );
            cgGlobal.writeEntry( "alarmAutogenerated" + suffix, alarm.autoGenerated );
            ++i;
        }

        // Delete old stop settings entries
        QString test = "alarmType" + QString::number( i );
        while ( cgGlobal.hasKey( test ) ) {
            QString suffix = i == 1 ? QString() : '_' + QString::number( i );
            cgGlobal.deleteEntry( "alarmType" + suffix );
            cgGlobal.deleteEntry( "alarmStops" + suffix );
            cgGlobal.deleteEntry( "alarmFilter" + suffix );
            cgGlobal.deleteEntry( "alarmEnabled" + suffix );
            cgGlobal.deleteEntry( "alarmName" + suffix );
            cgGlobal.deleteEntry( "alarmLastFired" + suffix );
            cgGlobal.deleteEntry( "alarmAutogenerated" + suffix );
            ++i;
            test = "alarmType_" + QString::number( i );
        }
    }

    return changed;
}

QList< JourneySearchItem > SettingsIO::decodeJourneySearchItems( QByteArray *data )
{
    QDataStream stream( data, QIODevice::ReadOnly );
    if ( stream.atEnd() ) {
        return QList< JourneySearchItem >();
    }

    // Test for correct data structure by the stored version
    quint8 version;
    stream >> version;
    if ( version != 1 ) {
        kDebug() << "Wrong setting version" << version;
        return QList< JourneySearchItem >();
    }

    // Read number of items
    quint8 count;
    stream >> count;

    // Read count items
    QList< JourneySearchItem > journeySearches;
    for ( int i = 0; i < count; ++i ) {
        QString name;
        QString journeySearch;
        bool favorite;
        stream >> journeySearch;
        stream >> name;
        stream >> favorite;

        journeySearches << JourneySearchItem( journeySearch, name, favorite );
    }

    return journeySearches;
}

QByteArray SettingsIO::encodeJourneySearchItems( const QList< JourneySearchItem > &journeySearches )
{
    QByteArray data;
    QDataStream stream( &data, QIODevice::WriteOnly );

    // Store version of the data structure, needs to be incremented whenever it changes,
    // max version is 256
    stream << quint8(1);

    // Store number of items
    stream << quint8(journeySearches.count()); // Max 256 items

    // Store items
    foreach ( const JourneySearchItem &item, journeySearches ) {
        stream << item.journeySearch();
        stream << item.name();
        stream << item.isFavorite();
    }
    return data;
}

FilterSettings SettingsIO::readFilterConfig( const KConfigGroup &cgGlobal )
{
    FilterSettings filters;
    filters.name = cgGlobal.readEntry( "Name", QString() );
    filters.filterAction = static_cast< FilterAction >(
            cgGlobal.readEntry("FilterAction", static_cast<int>(ShowMatching)) );
    filters.affectedStops = cgGlobal.readEntry( "AffectedStops", QList<int>() ).toSet();

    QByteArray baFilters = cgGlobal.readEntry( "Filters", QByteArray() );
    filters.filters.fromData( baFilters );
    return filters;
}

bool SettingsIO::writeFilterConfig( const FilterSettings &filters,
        const FilterSettings &oldFilterSettings, KConfigGroup cgGlobal )
{
    bool changed = false;

    if ( filters.name != oldFilterSettings.name ) {
        cgGlobal.writeEntry( "Name", filters.name );
        changed = true;
    }

    if ( filters.filters != oldFilterSettings.filters ) {
        cgGlobal.writeEntry( "Filters", filters.filters.toData() );
        changed = true;
    }

    if ( filters.filterAction != oldFilterSettings.filterAction ) {
        cgGlobal.writeEntry( "FilterAction", static_cast<int>(filters.filterAction) );
        changed = true;
    }

    if ( filters.affectedStops != oldFilterSettings.affectedStops ) {
        cgGlobal.writeEntry( "AffectedStops", filters.affectedStops.toList() );
        changed = true;
    }

    return changed;
}

void SettingsIO::writeFilterConfig( const FilterSettings& filters,
                                    KConfigGroup cgGlobal )
{
    cgGlobal.writeEntry( "Name", filters.name );
    cgGlobal.writeEntry( "Filters", filters.filters.toData() );
    cgGlobal.writeEntry( "FilterAction", static_cast<int>( filters.filterAction ) );
    cgGlobal.writeEntry( "AffectedStops", filters.affectedStops.toList() );
}
