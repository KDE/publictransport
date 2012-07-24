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

#ifndef POPUPICON_HEADER
#define POPUPICON_HEADER

/** @file
 * @brief This file contains the PopupIcon class.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#include <QObject> // Base class

class DeparturePainter;
class DepartureModel;
class DepartureItem;
class ItemBase;
class KIcon;
class QSize;
class QTimer;
class QPropertyAnimation;

typedef QList< DepartureItem* > DepartureGroup;
typedef QList< DepartureGroup > DepartureGroupList;

/**
 * @brief Holds data for the popup icon and handles animations.
 *
 * Use @ref createPopupIcon to create a KIcon with the current state of the popup icon, that can
 * be used with eg. Plasma::Applet::setPopupIcon. This class uses the DeparturePainter given in
 * the constructor to draw the popup icon.
 *
 * This class constructs (@ref createDepartureGroups) and stores a departure group list
 * (@ref departureGroups), ie. a QList of QLists of DepartureItems. It can animate between these
 * departure groups and the departures in the current group, using @ref animateToNextGroup,
 * @ref animateToPreviousGroup, @ref animateToAlarm (handled as special group) and
 * @ref fadeToNextDepartureInGroup. Animations between groups can go forward and backward, while
 * animations between departures in the current group always go forward.
 **/
class PopupIcon : public QObject {
    Q_OBJECT

    /**
     * @brief The index of the current departure group.
     *
     * @note If a transition animation between departure groups is running, this property holds a
     *   non integer value. If you set it manually this may also be a non integer value. This
     *   shouldn't need to be set manually. It gets used for the transition animation between
     *   departure groups.
     **/
    Q_PROPERTY( qreal DepartureGroupIndex READ departureGroupIndex WRITE setDepartureGroupIndex )

    /**
     * @brief The index of the current departure in the current group.
     *
     * @note If a fade animation between departures in the current group is running, this property
     *   holds a non integer value. If you set it manually this may also be a non integer value.
     *   This shouldn't need to be set manually. It gets used for the fade animation between
     *   departures in the current group.
     **/
    Q_PROPERTY( qreal DepartureIndex READ departureIndex WRITE setDepartureIndex )

public:
    /**
     * @brief Creates a new PopupIcon object, using the given @p departurePainter.
     **/
    explicit PopupIcon( DeparturePainter *departurePainter, QObject *parent = 0 );

    /** @brief The maximum number of departure groups (at the same time) to cycle through in the
     * popup icon. */
    static const int POPUP_ICON_DEPARTURE_GROUP_COUNT = 15;

    /** @brief Duration of the animation which does the transition between departure groups. */
    static const uint ANIMATION_DEPARTURE_GROUP_TRANSITION_DURATION = 500;

    /** @brief Duration of the animation which does the transition between departures in one group. */
    static const uint ANIMATION_DEPARTURE_TRANSITION_DURATION = 750;

    /** @brief Pause between animations which do the transition between departures in one group. */
    static const uint ANIMATION_DEPARTURE_TRANSITION_PAUSE = 1500;

    /** @brief Creates the popup icon with information about the next departure / alarm. */
    KIcon createPopupIcon( const QSize &size );

    /** @brief Start animation to the next departure group, if any. */
    inline void animateToNextGroup() { return animate(1); };

    /**
     * @brief Start animation to the previous departure group, if any.
     *
     * If the current departure group is the first one and the model has a pending alarm, this
     * animates to the alarm.
     **/
    inline void animateToPreviousGroup() { return animate(-1); };

    /** @brief Start animation to the next pending alarm, if any. */
    void animateToAlarm();

    /**
     * @brief The index of the current departure group.
     *
     * @note If a transition animation between departure groups is running, this returns a non
     *   integer value. If you call @ref setDepartureGroupIndex manually this may also be a non
     *   integer value.
     **/
    qreal departureGroupIndex() const { return m_currentDepartureGroupIndexStep; };

    /**
     * @brief Sets the index of the current departure group.
     *
     * @param departureGroupIndex The index of the new departure group.
     *
     * @note This shouldn't need to be called manually. It gets used for the transition animation
     *   between departure groups.
     **/
    void setDepartureGroupIndex( qreal departureGroupIndex ) {
        const int groupIndexBefore = currentDepartureGroupIndex();
        m_currentDepartureGroupIndexStep = departureGroupIndex;
        const int groupIndexAfter = currentDepartureGroupIndex();

        emit currentDepartureGroupIndexChanged( departureGroupIndex );
        if ( groupIndexBefore != groupIndexAfter ) {
            emit currentDepartureGroupChanged( groupIndexAfter );
        }
    };

    /**
     * @brief The index of the current departure in the current group.
     *
     * @note If a fade animation between departures in the current group is running, this returns a
     *   non integer value. If you call @ref setDepartureIndex manually this may also be a non
     *   integer value.
     **/
    qreal departureIndex() const { return m_currentDepartureIndexStep; };

    /**
     * @brief Sets the index of the current departure in the current group.
     *
     * @param departureIndex The index of the new departure.
     *
     * @note This shouldn't need to be called manually. It gets used for the fade animation
     *   between departures in the current group.
     **/
    void setDepartureIndex( qreal departureIndex ) {
        m_currentDepartureIndexStep = departureIndex;
        emit currentDepartureIndexChanged( departureIndex );
    };

    /**
     * @brief Gets the index of the group, where a group transition animation started.
     *
     * @note If no transition animation between groups is running, the returned value is undefined.
     **/
    int startDepartureGroupIndex() const { return m_startGroupIndex; };

    /**
     * @brief Gets the index of the group, where a group transition animation ends.
     *
     * @note If no transition animation between groups is running, the returned value is undefined.
     **/
    int endDepartureGroupIndex() const { return m_endGroupIndex; };

    /** @brief Gets the minimal departure group index. This can be -1, if there are pending alarms. */
    inline int minimalDepartureGroupIndex() const { return hasAlarms() ? -1 : 0; };

    /**
     * @brief Sets the used @ref DepartureModel to @p model.
     *
     * This gets used to check if there is a pending alarm.
     **/
    void setModel( DepartureModel *model ) { m_model = model; };

    /** @brief Checks if there is a pending alarm. */
    bool hasAlarms() const;

    bool currentGroupIsAlarmGroup() const;

    /**
     * @brief Gets a pointer to the current list of departure groups.
     *
     * Departures get grouped by their departure time in @ref createDepartureGroups.
     */
    const DepartureGroupList *departureGroups() const { return &m_departureGroups; };

    /** @brief Gets the current departure group, ie. a QList of departures in the current group. */
    DepartureGroup currentDepartureGroup() const;

    /** @brief Gets the current departure in the current group. */
    DepartureItem *currentDeparture() const;

    /**
     * @brief Creates a new list for the first departures that are shown in the popup icon.
     *
     * Each group can contain multiple departures if they depart at the same time. The number of
     * departure groups that can be shown in the popup icon is limited to
     * @ref POPUP_ICON_DEPARTURE_GROUP_COUNT.
     **/
    void createDepartureGroups();

signals:
    /** @brief Gets emitted if the current departure group has changed. */
    void currentDepartureGroupChanged( int departureGroupIndex );

    /** @brief Gets emitted if the current departure group index has changed. */
    void currentDepartureGroupIndexChanged( qreal departureGroupIndex );

    /** @brief Gets emitted if the current departure index in the current group has changed. */
    void currentDepartureIndexChanged( qreal departureIndex );

public slots:
    /** @brief Starts the fade animation to the next departure in the current group. */
    void fadeToNextDepartureInGroup();

    /**
     * @brief Removes the given @p departures from the current groups.
     *
     * If a departure group is empty, after removing @p departures, the group gets also removed.
     **/
    void departuresAboutToBeRemoved( QList< ItemBase* > departures );

protected slots:
    /** @brief The transition animation between two departure groups in the has finished. */
    void transitionAnimationFinished();

    /** @brief The fade animation between two departures in the current group in the has finished. */
    void fadeAnimationFinished();

private:
    void animate( int delta ); // Animate between departure groups
    void startFadeTimerIfMultipleDepartures(); // Starts/stops m_fadeBetweenDeparturesInGroupTimer
    void applyDepartureIndexLimit(); // Limit departure index to correct values in the current group
    void stopDepartureFadeAnimation(); // Stop running fade animation between two departures
    void departureGroupRemoved( int index ); // The group with the given index has been removed
    int currentDepartureGroupIndex() const; // The integer index of the current group (not qreal)

    DepartureModel *m_model;
    DeparturePainter *m_departurePainter;
    int m_startGroupIndex; // Index of the group where a running transition animation started
    int m_endGroupIndex; // Index of the group where a running transition animation ends
    qreal m_currentDepartureGroupIndexStep; // qreal for transition animations between groups
    qreal m_currentDepartureIndexStep; // qreal for fade animations between departures
    QPropertyAnimation *m_transitionAnimation; // Animates between departure groups
    QPropertyAnimation *m_fadeAnimation; // Animates between departures in the current group
    QTimer *m_fadeBetweenDeparturesInGroupTimer; // Periodically calls fadeToNextDepartureInGroup()
    DepartureGroupList m_departureGroups; // Groups the first few departures by departure time.
};

#endif // Multiple inclusion guard
