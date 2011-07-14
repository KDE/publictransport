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

#include <qnamespace.h>
#include <QTime>
#include <KIcon>
#include "kdeversion.h"
#include <publictransporthelper/enums.h>
#include <QEvent>
#include <QAbstractTransition>
#include <qstate.h>
#include <QSignalTransition>
#include <QVariant>
#include <QAction>

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

/** The values of the enumerators shouldn't be changed because they are saved to the config file.
* @brief Types of departure / arrival lists. */
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

class ToPropertyTransition : public QSignalTransition
{
public:
    ToPropertyTransition( QObject *sender, const char *signal, QState *source,
                          QObject *propertyObject, const char *targetStateProperty )
        : QSignalTransition(sender, signal, source),
          m_propertyObject(propertyObject),
          m_property(targetStateProperty)
    {
        qRegisterMetaType<QState*>("QState*");
    };

    const QObject *propertyObject() const { return m_propertyObject; };
    const char *targetStateProperty() const { return m_property; };
    QState *currentTargetState() const {
        return qobject_cast<QState*>( qvariant_cast<QObject*>(m_propertyObject->property(m_property)) );
    };
    void setTargetStateProperty( const QObject *propertyObject, const char *property ) {
        m_propertyObject = propertyObject;
        m_property = property;
    };

protected:
    virtual bool eventTest( QEvent *event )
    {
        if ( !QSignalTransition::eventTest(event) ) {
            return false;
        }

        setTargetState( currentTargetState() );
//         qDebug() << targetState();
        return true;
    };

private:
    const QObject *m_propertyObject;
    const char *m_property;
};

/** @brief A set of flags for RouteStopMarkerGraphicsItem/RouteStopTextGraphicsItem. */
enum RouteItemFlag {
    RouteItemDefault        = 0x0000, /**< Use default settings. */
    RouteItemHighlighted    = 0x0001  /**< Draw the stop item highlighted. */
};
Q_DECLARE_FLAGS( RouteItemFlags, RouteItemFlag );
Q_DECLARE_OPERATORS_FOR_FLAGS( RouteItemFlags );

/** @brief A set of flags for stops in a route. */
enum RouteStopFlag {
    RouteStopIsIntermediate = 0x0001, /**< The route stop is an intermediate one (not the first
                                       * and not the last). Can't be used together with
                                       * RouteStopIsOrigin or RouteStopIsTarget. */
    RouteStopIsOrigin       = 0x0002, /**< The route stop is the origin of the route. Can't be used
                                       * together with RouteStopIsIntermediate or RouteStopIsTarget. */
    RouteStopIsTarget       = 0x0004, /**< The route stop is the target of the route. Can't be used
                                       * together with RouteStopIsIntermediate or RouteStopIsOrigin. */
    RouteStopIsHomeStop     = 0x0008  /**< The route stop is the currently selected home stop. */
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

class StopAction : public QAction
{
    Q_OBJECT
public:
    /** @brief Actions for intermediate stops, shown in RouteGraphicsItems. */
    enum Type {
        ShowDeparturesForStop,   /**< Show a departure list for the associated stop. */
        CreateFilterForStop,     /**< Create a filter via the associated stop. */
        CopyStopNameToClipboard, /**< Copy the name of the associated stop to the clipboard. */
        HighlightStop,           /**< Highlight the associated stop in all route items.
                                  * If the stop was already highlighted, it should
                                  * be unhighlighted. */
        RequestJourneysToStop,   /**< Request journeys to the associated stop. The origin stop
                                  * can be given as QVariant data argument to stop action requests. */
        RequestJourneysFromStop  /**< Request journeys from the associated stop. The target stop
                                  * can be given as QVariant data argument to stop action requests. */
    };

    StopAction( Type type, QObject* parent );

    Type type() const { return m_type; };

    QString stopName() const { return m_stopName; };
    void setStopName( const QString &stopName ) { m_stopName = stopName; };

signals:
    /**
     * @brief This signal gets fired when this QAction signals triggered(), but with more arguments.
     *
     * @param type The type of the triggered stop action.
     * @param stopName The name of the stop associated with the action.
     **/
    void stopActionTriggered( StopAction::Type type, const QString &stopName );

protected slots:
    void slotTriggered();

private:
    Q_DISABLE_COPY(StopAction)

    const Type m_type;
    QString m_stopName;
};

#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
namespace Plasma {
    class Animator;
    class Animation;
}
class QGraphicsWidget;
#endif

/** @class Global
  * @brief Contains global static methods. */
class GlobalApplet {
public:
#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    static Plasma::Animation *fadeAnimation( QGraphicsWidget *w, qreal targetOpacity );
    static void startFadeAnimation( QGraphicsWidget *w, qreal targetOpacity );
#endif
    
    static KIcon putIconIntoBiggerSizeIcon( const KIcon &icon,
                                            const QSize &iconSize, const QSize &resultingSize = QSize(32, 32) );

    /** Create an "international" icon with some flag icons */
    static KIcon internationalIcon();

    /** Creates an icon that has another icon as overlay on the bottom right. */
    static KIcon makeOverlayIcon( const KIcon &icon, const KIcon &overlayIcon,
                                  const QSize &overlaySize = QSize(10, 10), int iconExtend = 16 );

    /** Creates an icon that has another icon as overlay on the bottom right. */
    static KIcon makeOverlayIcon( const KIcon &icon, const QString &overlayIconName,
                                  const QSize &overlaySize = QSize(10, 10), int iconExtend = 16 );

    /** Creates an icon that has other icons as overlay on the bottom. */
    static KIcon makeOverlayIcon( const KIcon &icon, const QList<KIcon> &overlayIconsBottom,
                                  const QSize &overlaySize = QSize(10, 10), int iconExtend = 16 );

    /** Gets an icon for the given type of vehicle. */
    static KIcon vehicleTypeToIcon( const VehicleType &vehicleType,
                                    const QString &overlayIcon = QString() );

    /** Gets an icon containing the icons of all vehicle types in the given list. */
    static KIcon iconFromVehicleTypeList( const QList<VehicleType> &vehicleTypes, int extend = 32 );

    /** Gets a string like "25 minutes". */
    static QString durationString( int seconds );
};

#endif // Multiple inclusion guard
