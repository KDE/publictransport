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

#ifndef GLOBAL_HEADER
#define GLOBAL_HEADER

/** @file
 * This file contains enumerations and Global used by the public transport applet.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#include <KIcon>
#include "kdeversion.h"

#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
namespace Plasma {
    class Animation;
}
class QGraphicsWidget;
#endif

/** @brief Icons to be displayed by the Plasma::IconWidget in the applet's top left corner. */
enum MainIconDisplay {
    DepartureListErrorIcon,
    ArrivalListErrorIcon = DepartureListErrorIcon,

    DepartureListOkIcon,
    ArrivalListOkIcon = DepartureListOkIcon,

    AbortJourneySearchIcon,
    GoBackIcon,

    JourneyListErrorIcon,
    JourneyListOkIcon
};

/**
 * @brief Types of departure / arrival lists.
 *
 * The values of the enumerators shouldn't be changed because they are saved to the config file.
 **/
enum DepartureArrivalListType {
    _UseCurrentDepartureArrivalListType = 999, /**< Only for use as default parameter
            * to use the settings from PublicTransportSettings */

    DepartureList = 0, /**< A list of departures from the home stop */
    ArrivalList = 1 /**< A list of arrivals at the home stop */
};

/** @brief Types of the title of the applet. */
enum TitleType {
    ShowDepartureArrivalListTitle = 0, /**< Shows an icon, the stop name and additional information */
    ShowSearchJourneyLineEdit = 1, /**< Shows a line edit for journey search requests */
    ShowSearchJourneyLineEditDisabled = 2, /**< Shows a disabled line edit for journey search requests */
    ShowJourneyListTitle = 3, /**< Shows an icon, a title and additional information */
    ShowIntermediateDepartureListTitle = 4, /**< Shows a back icon, the stop name and additional information */
};

/** @brief A set of flags for route stops in the DepartureModel/JourneyModel. */
enum RouteItemFlag {
    RouteItemDefault        = 0x0000, /**< Default route stop settings. */
    RouteItemHighlighted    = 0x0001, /**< The stop item is currently highlighted. */
    RouteItemHomeStop       = 0x0002  /**< The stop item is the currently selected home stop. */
};
Q_DECLARE_FLAGS( RouteItemFlags, RouteItemFlag );
Q_DECLARE_OPERATORS_FOR_FLAGS( RouteItemFlags );

/** @brief A set of flags for stops in a route. */
enum RouteStopFlag {
    RouteStopDefault        = 0x0000, /**< The route stop has no special settings. */
    RouteStopIsIntermediate = 0x0001, /**< The route stop is an intermediate one (not the first
                                       * and not the last). Can't be used together with
                                       * RouteStopIsOrigin or RouteStopIsTarget. */
    RouteStopIsOrigin       = 0x0002, /**< The route stop is the origin of the route. Can't be used
                                       * together with RouteStopIsIntermediate or RouteStopIsTarget. */
    RouteStopIsTarget       = 0x0004, /**< The route stop is the target of the route. Can't be used
                                       * together with RouteStopIsIntermediate or RouteStopIsOrigin. */
    RouteStopIsHomeStop     = 0x0008, /**< The route stop is the currently selected home stop. */
    RouteStopIsHighlighted  = 0x0010  /**< The route stop is the currently highlighted stop. */
};
Q_DECLARE_FLAGS( RouteStopFlags, RouteStopFlag );
Q_DECLARE_OPERATORS_FOR_FLAGS( RouteStopFlags );

/** @brief Different states of alarm. */
enum AlarmState {
    NoAlarm                 = 0x0000, /**< No alarm is set. */

    AlarmPending            = 0x0001, /**< An alarm is set and pending. */
    AlarmFired              = 0x0002, /**< An alarm has been fired. */

    AlarmIsAutoGenerated    = 0x0004, /**< There is an alarm setting with the
                                       * same settings that are used to autogenerate alarms for departures
                                       * using the context menu. Items with this alarm state can remove
                                       * their alarm. */
    AlarmIsRecurring        = 0x0008  /**< There is a recurring alarm that matches the departure. */
};
Q_DECLARE_FLAGS( AlarmStates, AlarmState )
Q_DECLARE_OPERATORS_FOR_FLAGS( AlarmStates )

/** @class GlobalApplet
 * @brief Contains global static methods. */
class GlobalApplet {
public:
#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    static Plasma::Animation *fadeAnimation( QGraphicsWidget *w, qreal targetOpacity );
    static void startFadeAnimation( QGraphicsWidget *w, qreal targetOpacity );
#endif

    static KIcon stopIcon( RouteStopFlags routeStopFlags );

    /** @brief Creates an icon that has another icon as overlay on the bottom right. */
    static KIcon makeOverlayIcon( const KIcon &icon, const KIcon &overlayIcon,
                                  const QSize &overlaySize = QSize(10, 10), int iconExtend = 16 );

    /** @brief Creates an icon that has another icon as overlay on the bottom right. */
    static KIcon makeOverlayIcon( const KIcon &icon, const QString &overlayIconName,
                                  const QSize &overlaySize = QSize(10, 10), int iconExtend = 16 );

    /** @brief Creates an icon that has other icons as overlay on the bottom. */
    static KIcon makeOverlayIcon( const KIcon &icon, const QList<KIcon> &overlayIconsBottom,
                                  const QSize &overlaySize = QSize(10, 10), int iconExtend = 16 );
};

#endif // Multiple inclusion guard
