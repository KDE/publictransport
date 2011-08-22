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

#include "popupicon.h"

#include "departuremodel.h"
#include "departurepainter.h"

#include <QPropertyAnimation>
#include <QTimer>
#include <qmath.h>

PopupIcon::PopupIcon( DeparturePainter *departurePainter, QObject* parent )
        : QObject(parent), m_model(0), m_departurePainter(departurePainter),
          m_transitionAnimation(0), m_fadeAnimation(0), m_fadeBetweenDeparturesInGroupTimer(0)
{
    m_currentDepartureGroupIndexStep = 0.0;
    m_currentDepartureIndexStep = 0.0;
    m_startGroupIndex = 0;
    m_endGroupIndex = 0;

    m_fadeBetweenDeparturesInGroupTimer = new QTimer( this );
    m_fadeBetweenDeparturesInGroupTimer->setInterval( ANIMATION_DEPARTURE_TRANSITION_DURATION +
                                                      ANIMATION_DEPARTURE_TRANSITION_PAUSE );
    connect( m_fadeBetweenDeparturesInGroupTimer, SIGNAL(timeout()),
             this, SLOT(fadeToNextDepartureInGroup()) );
}

KIcon PopupIcon::createPopupIcon( const QSize &size )
{
    KIcon icon;
    QPixmap pixmap;
    if ( m_model->isEmpty() || m_departureGroups.isEmpty() ) {
        pixmap = m_departurePainter->createMainIconPixmap( size );
    } else {
        pixmap = m_departurePainter->createPopupIcon( this, m_model, size );
    }
    icon.addPixmap( pixmap );
    return icon;
}

bool PopupIcon::hasAlarms() const
{
    return m_model->hasAlarms();
}

void PopupIcon::createDepartureGroups()
{
    m_departureGroups.clear();

    // Create departure groups (maximally PopupIconData::POPUP_ICON_DEPARTURE_GROUP_COUNT groups)
    QDateTime lastTime;
    for ( int row = 0; row < m_model->rowCount(); ++row ) {
        DepartureItem *item = dynamic_cast<DepartureItem*>( m_model->item(row) );
        const DepartureInfo *info = item->departureInfo();

        QDateTime time = info->predictedDeparture();
        if ( m_departureGroups.count() == PopupIcon::POPUP_ICON_DEPARTURE_GROUP_COUNT
             && time != lastTime )
        {
            // Maximum group count reached and all groups filled
            break;
        } else if ( time == lastTime ) {
            // Add item to the last group
            m_departureGroups.last().append( item );
        } else {
            // Create a new group
            m_departureGroups << (DepartureGroup() << item);
            lastTime = time;
        }
    }

    qDebug() << "Created" << m_departureGroups.count() << "new departure groups";
    applyDepartureIndexLimit();
    startFadeTimerIfMultipleDepartures();
}

void PopupIcon::applyDepartureIndexLimit()
{
    qreal maxDepartureIndex = currentDepartureGroup().count();
    if ( m_currentDepartureIndexStep > maxDepartureIndex ) {
        if ( m_fadeAnimation ) {
            stopDepartureFadeAnimation();
        }
        m_currentDepartureIndexStep = maxDepartureIndex;
    }
}

void PopupIcon::stopDepartureFadeAnimation()
{
    if ( m_fadeAnimation ) {
        m_fadeAnimation->stop();
        fadeAnimationFinished();
    }
}

void PopupIcon::departuresAboutToBeRemoved( QList< ItemBase* > departures )
{
    // Update departure groups
    DepartureGroupList::iterator it = m_departureGroups.begin();
    int index = 0;
    while ( it != m_departureGroups.end() ) {
        // Remove all departures in the current group
        // that are inside the given list of departures to be removed
        DepartureGroup::iterator sub_it = it->begin();
        while ( sub_it != it->end() ) {
            if ( departures.contains(*sub_it) ) {
//                 departureRemoved( departureIndex ); TODO
                sub_it = it->erase( sub_it );
            } else {
                ++sub_it;
            }
        }

        // Remove the group if all it's departures have been removed
        if ( it->isEmpty() ) {
            it = m_departureGroups.erase( it );
            departureGroupRemoved( index );
        } else {
            ++it;
        }
        ++index;
    }
}

DepartureGroup PopupIcon::currentDepartureGroup() const
{
    int groupIndex;
    if ( m_transitionAnimation ) {
        if ( qFloor(m_currentDepartureGroupIndexStep) == m_startGroupIndex ) {
            // Animation has just started, use start group
            groupIndex = m_startGroupIndex;
        } else {
            // Animation is running, use end group
            groupIndex = m_endGroupIndex;
        }
    } else {
        groupIndex = qFloor( m_currentDepartureGroupIndexStep );
    }
    return groupIndex < 0 ? DepartureGroup() : m_departureGroups[ groupIndex ];
}

DepartureItem* PopupIcon::currentDeparture() const
{
    return currentDepartureGroup()[ qCeil(m_currentDepartureIndexStep) ];
}

void PopupIcon::startFadeTimerIfMultipleDepartures()
{
    if ( currentDepartureGroup().count() > 1 ) {
        if ( !m_fadeBetweenDeparturesInGroupTimer->isActive() ) {
            m_fadeBetweenDeparturesInGroupTimer->start();
        }
    } else if ( m_fadeBetweenDeparturesInGroupTimer->isActive() ) {
        m_fadeBetweenDeparturesInGroupTimer->stop();
    }
}

void PopupIcon::fadeToNextDepartureInGroup()
{
    if ( currentDepartureGroup().count() <= 1 ) {
        kDebug() << "Need at least two departures in the current group to fade between";
        stopDepartureFadeAnimation();
        return;
    }

    // Create animation
    if ( !m_fadeAnimation ) {
        m_fadeAnimation = new QPropertyAnimation( this, "DepartureIndex", this );
        m_fadeAnimation->setEasingCurve( QEasingCurve(QEasingCurve::OutQuart) );
        m_fadeAnimation->setDuration( ANIMATION_DEPARTURE_TRANSITION_DURATION );
        connect( m_fadeAnimation, SIGNAL(finished()), this, SLOT(fadeAnimationFinished()) );
    }

    m_fadeAnimation->setStartValue( m_currentDepartureIndexStep );
    m_fadeAnimation->setEndValue( (qCeil(m_currentDepartureIndexStep) + 1)
                                  % currentDepartureGroup().count() );
    m_fadeAnimation->start();
}

void PopupIcon::transitionAnimationFinished()
{
    delete m_transitionAnimation;
    m_transitionAnimation = 0;
    startFadeTimerIfMultipleDepartures();
}

void PopupIcon::fadeAnimationFinished()
{
    delete m_fadeAnimation;
    m_fadeAnimation = 0;
    DepartureGroup group = currentDepartureGroup();
    if ( !group.isEmpty() ) {
        m_currentDepartureIndexStep = qMax( minimalDepartureGroupIndex(),
                                            qCeil(m_currentDepartureIndexStep) % group.count() );
    }
}

void PopupIcon::animate( int delta )
{
    Q_ASSERT( delta != 0 );
    const int oldGroupSpan = qAbs( m_endGroupIndex - m_startGroupIndex );
    const int oldStartGroupIndex = m_startGroupIndex;
    const int oldEndGroupIndex = m_endGroupIndex;
    if ( delta > 0 ) {
        // Animate to next departure group
        if ( m_endGroupIndex + 1 < m_departureGroups.count() ) {
            if ( m_transitionAnimation ) {
                // Already animating
                if ( m_endGroupIndex < m_startGroupIndex ) {
                    // Direction was reversed
                    m_startGroupIndex = m_endGroupIndex;
                }

                // Increase index of the departure group where the animation should end
                ++m_endGroupIndex;
            } else {
                m_startGroupIndex = qFloor( m_currentDepartureGroupIndexStep );
                m_endGroupIndex = m_startGroupIndex + 1;
            }
        } else {
            // Max departure group already reached
            return;
        }
    } else {
        // Animate to previous departure group
        if ( m_endGroupIndex > minimalDepartureGroupIndex() ) {
            if ( m_transitionAnimation ) {
                // Already animating
                if ( m_endGroupIndex > m_startGroupIndex ) {
                    // Direction was reversed
                    m_startGroupIndex = m_endGroupIndex;
                }

                // Decrease index of the departure group where the animation should end
                --m_endGroupIndex;
            } else {
                m_startGroupIndex = qFloor( m_currentDepartureGroupIndexStep );
                m_endGroupIndex = m_startGroupIndex - 1;
            }
        } else {
            // Min departure group or alarm departure already reached
            return;
        }
    }

    if ( m_transitionAnimation ) {
        // Compute new starting index for the running animation
        // animationPartDone is a value from 0 (not started) to 1 (old animation already finished)
        const qreal animationPartDone = qAbs(m_currentDepartureGroupIndexStep - oldStartGroupIndex)
                                        / oldGroupSpan;
        if ( animationPartDone > 0.5 ) {
            // Running animation visually almost finished (actually 50%, but the easing curve 
            // slows the animation down at the end)
            // With this check, the possibility gets lowered, that the animation is spanned over 
            // more than one group
            m_startGroupIndex = oldEndGroupIndex;
            m_transitionAnimation->stop();
            m_transitionAnimation->setStartValue( m_startGroupIndex );
        } else {
            const int newGroupSpan = /*qAbs(*/ m_endGroupIndex - m_startGroupIndex;
            const qreal startDepartureIndex = m_startGroupIndex + animationPartDone * newGroupSpan;
            m_transitionAnimation->stop();
            m_transitionAnimation->setStartValue( startDepartureIndex );
        }
    } else {
        // Create animation
        m_transitionAnimation = new QPropertyAnimation( this, "DepartureGroupIndex", this );
        m_transitionAnimation->setEasingCurve( QEasingCurve(QEasingCurve::OutQuart) );
        m_transitionAnimation->setDuration( ANIMATION_DEPARTURE_GROUP_TRANSITION_DURATION );
        m_transitionAnimation->setStartValue( m_startGroupIndex );
        connect( m_transitionAnimation, SIGNAL(finished()),
                 this, SLOT(transitionAnimationFinished()) );
    }

    applyDepartureIndexLimit();

    m_transitionAnimation->setEndValue( m_endGroupIndex );
    m_transitionAnimation->start();
}

void PopupIcon::departureGroupRemoved( int index )
{
    if ( index <= m_currentDepartureGroupIndexStep ) {
        // The currently shown departure group or a group before it has been removed,
        // update group / departure index
        int minimalGroupIndex = minimalDepartureGroupIndex();
        if ( m_currentDepartureGroupIndexStep > minimalGroupIndex ) {
            // Decrement current departure group index if possible, to stay at the same group
            if ( m_transitionAnimation ) {
                if ( m_startGroupIndex > minimalGroupIndex && m_endGroupIndex > minimalGroupIndex ) {
                    // Update animation indices to point to the same groups as before the removal
                    --m_currentDepartureGroupIndexStep;
                    --m_startGroupIndex;
                    --m_endGroupIndex;
                } else {
                    // Stop running group transition animation, 
                    // the start or end group has been removed
                    m_transitionAnimation->stop();
                    transitionAnimationFinished();
                }
            } else {
                setDepartureGroupIndex( m_currentDepartureGroupIndexStep - 1 );
            }
        }

        if ( index == m_currentDepartureGroupIndexStep ) {
            if ( m_fadeAnimation ) {
                // Stop running fade animation between two departures in the removed group
                stopDepartureFadeAnimation();
            }
            m_currentDepartureIndexStep = 0.0; // New current departure is the first of the new group
        }
    }
}

void PopupIcon::animateToAlarm()
{
    if ( !hasAlarms() ) {
        return;
    }

    if ( m_transitionAnimation ) {
        m_transitionAnimation->stop();
        m_transitionAnimation->setStartValue( m_currentDepartureGroupIndexStep );
    } else {
        m_transitionAnimation = new QPropertyAnimation( this, "DepartureGroupIndex", this );
        m_transitionAnimation->setStartValue( m_startGroupIndex );
        connect( m_transitionAnimation, SIGNAL(finished()),
                 this, SLOT(transitionAnimationFinished()) );
    }

    m_transitionAnimation->setEndValue( -1 );
    m_transitionAnimation->start();
}