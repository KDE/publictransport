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
 * @brief This file contains classes for managing settings of the PublicTransport applet.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SETTINGS_HEADER
#define SETTINGS_HEADER

// Own includes
#include "global.h"
#include "journeysearchitem.h"

// libpublictransporthelper includes
#include <filter.h>
#include <stopsettings.h>

// Qt includes
#include <QFont>

using namespace PublicTransport;

/** @brief Different types of alarms. */
enum AlarmType {
    AlarmRemoveAfterFirstMatch = 0, /**< The alarm will get removed once fired. */
    AlarmApplyToNewDepartures /**< The alarm won't be removed automatically,
            * ie. it's a recurring alarm. */
};

/** @brief Contains information about an alarm. */
struct AlarmSettings {
    AlarmSettings( const QString &name = "<unnamed>", bool autoGenerated = false ) {
        this->name = name;
        this->autoGenerated = autoGenerated;
        this->enabled = true;
        this->type = AlarmRemoveAfterFirstMatch;
    };

    QString name; /**< The name of this alarm. */
    bool enabled; /**< Whether or not this alarm is enabled. */
    bool autoGenerated; /**< Whether or not this alarm is auto generated.
            * Alarms that get changed are no longer treated as auto generated. */
    Filter filter; /**< The filter to match alarm departures/arrivals. */
    AlarmType type; /**< The type of this alarm. */
    QList< int > affectedStops; /**< A list of stop settings indices for which
            * this alarm should be applied. */
    QDateTime lastFired; /**< The time when this alarm was last fired. */

    /**
     * @brief Compares this alarm with @p other without checking the name or target constraints.
     *
     * Can be used to find an autogenerated alarm in an AlarmSettingsList.
     * @param other The other alarm to compare with.
     * @return bool True, if the alarms are "equal", ie. both autogenerated and equal except
     *   for the name/affectedStops/lastFired settings and without target constraints in the
     *   filter of this alarm.
     **/
    bool equals( const AlarmSettings &other ) const {
        return autoGenerated == other.autoGenerated && type == other.type &&
               enabled == other.enabled && filter == other.filter;
    };

    bool isOneTimeAlarm() const;
    bool isExpired() const; // only if one time alarm
};
bool operator ==( const AlarmSettings &l, const AlarmSettings &r );

/** @brief A QList of AlarmSettings with some convenience methods. */
class AlarmSettingsList : public QList< AlarmSettings > {
public:
    /** @brief Get a list of the names of all alarm settings in this list. */
    QStringList names() const;

    /** @brief Checks if there is an alarm settings object with the given @p name in this list. */
    bool hasName( const QString &name ) const;

    /**
     * @brief Gets the alarm settings object with the given @p name from this list.
     *
     * If there is no such alarm settings object, a default constructed AlarmSettings object
     * gets returned.
     **/
    AlarmSettings byName( const QString &name ) const;

    /** @brief Removes the alarm settings object with the given @p name from this list. */
    void removeByName( const QString &name );

    /** @brief Adds the given @p newAlarmSettings to this list or changes an existing one with
     * the same name, if there is already one in this list. */
    void set( const AlarmSettings &newAlarmSettings );

    /** @brief Remove one alarm which is equal to @p alarm according to AlarmSettings::equals(). */
    bool removeAlarm( const AlarmSettings &alarm );
};

/** @brief Contains information about a color group configuration, ie. it's color. */
struct ColorGroupSettings {
    /**
     * @brief A list of filters for this color group configuration.
     *
     * Filters are OR combined while constraints are AND combined.
     * Departures/arrivals that match these filters are colored using the
     * coor of this ColorGroupSettings.
     **/
    FilterList filters;

    /**< @brief The color of this color group. */
    QColor color;

    /**< @brief Whether or not departures in this color group should be filtered out. */
    bool filterOut;

    QString target;
    QString displayText;

    ColorGroupSettings( const QColor &color = Qt::transparent ) {
        this->color = color;
        this->filterOut = false;
    };

    /** @brief Applies the filters of this color group configuration
      * on the given @p departureInfo. */
    bool matches( const DepartureInfo& departureInfo ) const {
        return filters.match( departureInfo );
    };
};
bool operator ==( const ColorGroupSettings &l, const ColorGroupSettings &r );

/** @brief A list of ColorGroupSettings. */
class ColorGroupSettingsList : public QList< ColorGroupSettings > {
public:
    /** @brief Find a ColorGroupSettings object with the given @p color in the list. */
    ColorGroupSettings byColor( const QColor &color );

    void set( const ColorGroupSettings& newColorGroupSettings );

    /** @brief Checks if there is a color group settings object with the given @p color in this list. */
    bool hasColor( const QColor &color ) const;

    /** @brief Removes the color group settings object with the given @p color from this list. */
    bool removeColor( const QColor &color );

    /** @brief Enables/disables the ColorGroupSettings object with the given @p color in the list. */
    void enableColorGroup( const QColor &color, bool enable = true );

    /**
     * @brief Applies the filters of the color group configurations in the list to @p departureInfo.
     *
     * @param departureInfo The departure to test.
     * @returns True, if the given @p departureInfo matches a filtered out color group in the list.
     *   False, otherwise.
     **/
    bool filterOut( const DepartureInfo& departureInfo ) const;
};

inline uint qHash( const QStringList &key )
{
    uint result = 1;
    foreach ( const QString &k, key ) {
        result += qHash( k );
    }
    return result;
}

/**
 * @brief Contains all settings of the PublicTransport applet.
 *
 * Use SettingsIO to read/write settings from/to disk, use SettingsUiManager to synchronize and
 * connect the settings in this class with the widget states in the configuration dialog.
 **/
class Settings {
public:
    /** @brief Named user settings for use in PublicTransport::StopSettings. */
    enum ExtendedStopSetting {
        JourneySearchSetting = UserSetting /**< A list of journey searches,
                                              * QList\<JourneySearchItem\> */
    };

    /** @brief Global boolean settings. */
    enum SettingsFlag {
        NoSettingsFlags         = 0x0000,

        ColorizeDepartureGroups = 0x0001,
        DrawShadows             = 0x0004,
        HideTargetColumn        = 0x0008,
        UseThemeFont            = 0x0010,

        DefaultSettingsFlags    = ColorizeDepartureGroups | DrawShadows | UseThemeFont
    };
    Q_DECLARE_FLAGS( SettingsFlags, SettingsFlag )

    /** @brief Boolean flags controlling the departure time display. */
    enum DepartureTimeFlag {
        DoNotShowDepartureTime   = 0x00,

        ShowDepartureTime        = 0x01,
        ShowRemainingTime        = 0x02,
        DisplayDepartureTimeBold = 0x04,

        ShowTimeAndRemainingTime = ShowDepartureTime | ShowRemainingTime,
        DefaultDepartureTimeFlags = ShowTimeAndRemainingTime | DisplayDepartureTimeBold
    };
    Q_DECLARE_FLAGS( DepartureTimeFlags, DepartureTimeFlag )

    /** @brief Different ways to request additional timetable data. */
    enum AdditionalDataRequestType {
        NeverRequestAdditionalData,

        RequestAdditionalDataWhenNeeded,
        RequestAdditionalDataDirectly,

        DefaultAdditionalDataRequestType = RequestAdditionalDataWhenNeeded
    };

    /**
     * @brief Gets the size factor to be used for the given @p size value.
     *
     * @param size An integer value, ie. configurable using a slider widget. Smallest value is null.
     * @return The size factor associated with the given @p size value.
     **/
    static inline qreal sizeFactorFromSize( int size ) { return (size + 3) * 0.2; };

    /**
     * @brief Gets the integer size value to be used for the given @p sizeFactor.
     *
     * @param sizeFactor The zoom factor, to get the integer size value for.
     * @return The size associated with the given @p sizeFactor.
     **/
    static inline int sizeFromSizeFactor( qreal sizeFactor ) { return qRound(sizeFactor / 0.2) - 3; };

    /** @brief Creates a new Settings object. */
    Settings();

    /** @brief Copy constructor. */
    Settings( const Settings &other );

    /** @brief Destructor. */
    virtual ~Settings();

    /**
     * @brief Get the stop settings with the given @p stopIndex.
     * @see currentStop()
     **/
    inline const StopSettings stop( int stopIndex ) const {
            return m_stops[stopIndex]; };

    /**
     * @brief Get the filter settings with the given @p filterIndex.
     * @see currentFilters()
     **/
    inline const FilterSettings filter( int filterIndex ) const {
            return m_filters[filterIndex]; };

    /** @brief Get the alarm settings with the given @p alarmIndex. */
    inline const AlarmSettings alarm( int alarmIndex ) const {
            return m_alarms[alarmIndex]; };

    /**
     * @brief Get color group settings with the given @p stopIndex.
     * @note If colorize() returns false these color groups won't be used.
     * @see currentColorGroups()
     * @see colorGroups()
     **/
    inline const ColorGroupSettingsList colorGroup( int stopIndex ) const {
            return m_colorGroups[stopIndex]; };

    /**
     * @brief A list of all stop settings.
     * @see stop()
     * @see currentStop()
     **/
    inline const StopSettingsList stops() const { return m_stops; };

    /**
     * @brief A list of all filter settings.
     * @note FilterSettings objects returned in the list may not apply to the current stop.
     *   To get a list of filter settings that apply to the current stop use currentFilters().
     * @see filters()
     * @see currentFilters()
     **/
    inline const FilterSettingsList filters() const { return m_filters; };

    /**
     * @brief A list of all alarm settings.
     * @note AlarmSettings objects returned in the list may not apply to the current stop.
     *   To get a list of alarm settings that apply to the current stop use currentAlarms().
     * @see alarm()
     **/
    inline const AlarmSettingsList alarms() const { return m_alarms; };

    /**
     * @brief A list of all color group settings lists (one list for each stop).
     * @note If colorize() returns false these color groups won't be used.
     * @note Color group settings returned in the list may not apply to the current stop.
     *   To get a list of color groups that apply to the current stop use currentColorGroups().
     * @see colorGroup()
     **/
    inline const QList<ColorGroupSettingsList> colorGroups() const {
            return m_colorGroups; };

    /**
     * @brief Gets the currently used stop settings.
     *
     * If the current stop settings index is invalid an empty StopSettings object gets returned.
     * @see isCurrentStopIndexValid
     **/
    const StopSettings currentStop() const {
        if ( !isCurrentStopIndexValid() ) {
            kDebug() << "Current stop index invalid" << m_currentStopIndex
                     << "Stop settings count:" << m_stops.count();
            return StopSettings();
        }
        return m_stops[ m_currentStopIndex ];
    };

    /**
     * @brief Gets a modifyable reference to the currently used stop settings.
     * @warning This crashes with invalid stop settings index.
     * @see isCurrentStopIndexValid
     **/
    StopSettings &currentStop() {
        Q_ASSERT_X( isCurrentStopIndexValid(), "StopSettings::currentStop",
                    QString("There's no stop settings with index %1 to get a "
                            "reference to").arg(m_currentStopIndex).toLatin1() );
        return m_stops[ m_currentStopIndex ];
    };

    /** @brief Get a list of all filters that apply to the current stop. */
    FilterSettingsList currentFilters() const;

    /** @brief Get a list of all alarms that apply to the current stop. */
    AlarmSettingsList currentAlarms() const;

    /** @brief Get a list of all color groups that apply to the current stop. */
    ColorGroupSettingsList currentColorGroups() const;

    /** @brief Whether or not the index of the currently used stop is valid. */
    bool isCurrentStopIndexValid() const {
        return m_currentStopIndex >= 0 &&
               m_currentStopIndex < m_stops.count();
    };

    /** @brief Remove all intermediate stops. */
    void removeIntermediateStops() { m_stops.removeIntermediateStops(); };

    /**
     * @brief Append @p stop to the list of stops.
     * @see stops()
     **/
    void appendStop( const StopSettings &stop ) { m_stops << stop; };

    /**
     * @brief Append @p filter to the list of filters.
     * @see filters()
     **/
    void appendFilter( const FilterSettings &filter ) { m_filters << filter; };

    /**
     * @brief Append @p alarm to the list of alarms.
     * @see alarms()
     **/
    void appendAlarm( const AlarmSettings &alarm ) { m_alarms << alarm; };

    /**
     * @brief Remove @p alarm from the list of alarms.
     * @see alarms()
     **/
    void removeAlarm( const AlarmSettings &alarm ) { m_alarms.removeAlarm(alarm); };

    /**
     * @brief The index of the currently used stop settings.
     *
     * Use currentStop() to get the StopSettings object, this index is pointing at. If
     * isCurrentStopIndexValid() returns false, this index is not in the range of
     * available stop settings.
     *
     * @see currentStop
     * @see isCurrentStopIndexValid
     **/
    inline int currentStopIndex() const { return m_currentStopIndex; };

    /** @brief How many lines each row in the departure/arrival view should have. */
    inline int linesPerRow() const { return m_linesPerRow; };

    /** @brief The maximal number of displayed departures. */
    inline int maximalNumberOfDepartures() const { return m_maximalNumberOfDepartures; };

    /**
     * @brief A zoom factor to use for item/font sizes.
     *
     * This value gets stored and configured in the dialog as integer. This integer gets converted
     * to the size factor using sizeFactorFromSize. To convert back sizeFromSizeFactor is used.
     **/
    inline float sizeFactor() const { return m_sizeFactor; };

    /**
     * @brief The type of data to be shown in the default timetable view.
     *
     * The default timetable view can show either departures or arrivals.
     **/
    inline DepartureArrivalListType departureArrivalListType() const {
            return m_departureArrivalListType; };

    /** @brief Whether or not shadows should be drawn in the applet. */
    inline bool drawShadows() const { return m_settingsFlags.testFlag(DrawShadows); };

    /** @brief Whether or not the target/origin column should be shown in the departure view. */
    inline bool hideTargetColumn() const { return m_settingsFlags.testFlag(HideTargetColumn); };

    /** @brief Whether or not the default plasma theme's font is used. */
    inline bool useThemeFont() const { return m_settingsFlags.testFlag(UseThemeFont); };

    /** @brief Whether or not departures should be colorized by groups. */
    inline bool colorize() const { return m_settingsFlags.testFlag(ColorizeDepartureGroups); };

    /** @brief Whether or not the departure time should be shown in the timetable. */
    inline bool showDepartureTime() const {
            return m_departureTimeFlags.testFlag(ShowDepartureTime); };

    /** @brief Whether or not the remaining time should be shown in the timetable. */
    inline bool showRemainingTime() const {
            return m_departureTimeFlags.testFlag(ShowRemainingTime); };

    /** @brief Whether or not the departure time should be displayed bold in the timetable. */
    inline bool displayDepartureTimeBold() const {
            return m_departureTimeFlags.testFlag(DisplayDepartureTimeBold); };

    /**
     * @brief Contains some global boolean settings.
     * @since 0.11
     **/
    inline SettingsFlags settingsFlags() const { return m_settingsFlags; };

    /**
     * @brief Flags for the display of the departure/arrival time.
     *
     * Before version 0.11, there were three boolean settings "showRemainingMinutes",
     * "showDepartureTime" and "displayTimeBold", now replaced with the DepartureTimeFlags
     * enumeration.
     * @since 0.11
     **/
    inline DepartureTimeFlags departureTimeFlags() const { return m_departureTimeFlags; };

    inline AdditionalDataRequestType additionalDataRequestType() const {
            return m_additionalDataRequestType; };

    /**
     * @brief The font to be used in the applet.
     *
     * This font gets only used, if useThemeFont() returns false.
     * @note If the font size is smaller than the font size of KGlobalSettings::smallestReadableFont
     *   that smallest readable font gets used instead to ensure best possible readability.
     * @see sizedFont()
     **/
    QFont font() const { return m_font; };

    /** @brief Gets font with the size zoomed by sizeFactor. */
    QFont sizedFont() const;

    /** @brief Get a list of JourneySearchItem's for the current stop settings. */
    QList<JourneySearchItem> currentJourneySearches() const {
        return currentStop().get< QList<JourneySearchItem> >( JourneySearchSetting );
    };

    /** @brief Whether or not shadows should be drawn in the applet. */
    void setDrawShadows( bool drawShadows ) {
        if ( m_settingsFlags.testFlag(DrawShadows) != drawShadows )
            m_settingsFlags ^= DrawShadows;
    };

    /** @brief Whether or not the target/origin column should be shown in the departure view. */
    void setHideTargetColumn( bool hideTargetColumn ) {
        if ( m_settingsFlags.testFlag(HideTargetColumn) != hideTargetColumn )
            m_settingsFlags ^= HideTargetColumn;
    };

    /** @brief Whether or not the default plasma theme's font is used. */
    void setUseThemeFont( bool useThemeFont ) {
        if ( m_settingsFlags.testFlag(UseThemeFont) != useThemeFont )
            m_settingsFlags ^= UseThemeFont;
    };

    /** @brief Whether or not departures should be colorized by groups. */
    void setColorize( bool colorize ) {
        if ( m_settingsFlags.testFlag(ColorizeDepartureGroups) != colorize )
            m_settingsFlags ^= ColorizeDepartureGroups;
    };

    void setShowDepartureTime( bool showDepartureTime ) {
        if ( m_departureTimeFlags.testFlag(ShowDepartureTime) != showDepartureTime )
            m_departureTimeFlags ^= ShowDepartureTime;
    };

    void setShowRemainingTime( bool showRemainingTime ) {
        if ( m_departureTimeFlags.testFlag(ShowRemainingTime) != showRemainingTime )
            m_departureTimeFlags ^= ShowRemainingTime;
    };

    void setDisplayDepartureTimeBold( bool displayDepartureTimeBold ) {
        if ( m_departureTimeFlags.testFlag(DisplayDepartureTimeBold) != displayDepartureTimeBold )
            m_departureTimeFlags ^= DisplayDepartureTimeBold;
    };

    void setFont( const QFont &font ) { m_font = font; };

    void setSettingsFlags( SettingsFlags settingsFlags ) { m_settingsFlags = settingsFlags; };

    void setDepartureTimeFlags( DepartureTimeFlags departureTimeFlags ) {
        m_departureTimeFlags = departureTimeFlags; };

    void setAdditionalDataRequestType( AdditionalDataRequestType additionalDataRequestType ) {
        m_additionalDataRequestType = additionalDataRequestType; };

    /** @brief Sets a list of JourneySearchItem's for the current stop settings. */
    void setCurrentJourneySearches( const QList<JourneySearchItem> &journeySearches ) {
        StopSettings &stop = currentStop();
        stop.set( JourneySearchSetting, QVariant::fromValue(journeySearches) );
    };

    void setMaximalNumberOfDepartures( int maximalNumberOfDepartures ) {
            m_maximalNumberOfDepartures = maximalNumberOfDepartures; };

    void setLinesPerRow( int linesPerRow ) { m_linesPerRow = linesPerRow; };

    void setSizeFactor( float sizeFactor ) { m_sizeFactor = sizeFactor; };

    void setDepartureArrivalListType( DepartureArrivalListType type ) {
            m_departureArrivalListType = type; };

    void setCurrentStop( int stopIndex ) {
        m_currentStopIndex = qBound(0, stopIndex, m_stops.count()); };

    /** @brief A list of all stop settings. */
    void setStops( const StopSettingsList & stopList ) { m_stops = stopList; };

    /** @brief A list of all filter settings. */
    void setFilters( const FilterSettingsList &filters ) { m_filters = filters; };

    /** @brief A list of all alarm settings. */
    void setAlarms( const AlarmSettingsList &alarms ) { m_alarms = alarms; };

    void setColorGroups( const QList<ColorGroupSettingsList> &colorGroups ) {
            m_colorGroups = colorGroups; };

    /** @brief Ensures that there is one color group settings list for each stop setting. */
    void adjustColorGroupSettingsCount();

    /** @brief Favorize the given @p journeySearch.*/
    void favorJourneySearch( const QString &journeySearch );

    /** @brief Removes the given @p journeySearch from the list of favored/recent journey searches. */
    void removeJourneySearch( const QString &journeySearch );

    /**
     * @brief Add the given @p journeySearch to the list of recent journey searches.
     *
     * If @p journeySearch is a favored journey search, this function does nothing.
     **/
    void addRecentJourneySearch( const QString &journeySearch );

    bool checkConfig();

private:
    SettingsFlags m_settingsFlags;
    DepartureTimeFlags m_departureTimeFlags;
    AdditionalDataRequestType m_additionalDataRequestType;

    StopSettingsList m_stops;
    FilterSettingsList m_filters;
    AlarmSettingsList m_alarms;
    QList<ColorGroupSettingsList> m_colorGroups;

    DepartureArrivalListType m_departureArrivalListType;
    QFont m_font;
    int m_currentStopIndex;
    int m_linesPerRow;
    int m_maximalNumberOfDepartures;
    float m_sizeFactor;
};

Q_DECLARE_OPERATORS_FOR_FLAGS( Settings::DepartureTimeFlags )
Q_DECLARE_OPERATORS_FOR_FLAGS( Settings::SettingsFlags )

#endif // SETTINGS_HEADER
