/*
 *   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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
* This file contains enumerations and Global used by the public transport applet.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef ENUMS_HEADER
#define ENUMS_HEADER

#include <qnamespace.h>
#include <KLocalizedString>

#include <KIcon>
#include <KIconEffect>
#include <KIconLoader>
#include <QPainter>

/** Columns of the tree view containing the timetable information */
enum TimetableColumn {
    LineStringColumn, /**< Column containing line strings and vehicle type icons */
    TargetColumn, /**< Column containing targets / origins and an info icon for departures / arrivals with journey news */
    DepartureColumn, /**< Column containing departure / arrival times, remaining minutes and delays */
    ArrivalColumn, /**< Column containing arrival times for journeys to or from the home stop */
    JourneyInfoColumn, /**< Column containing additional information for journeys to or from the home stop */
    VehicleTypeListColumn /**< Column containing icons of the used vehicle types for journeys */
};

/** Icons to be displayed by the Plasma::IconWidget in the applet's top left corner. */
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

/** Types of departure / arrival lists.
* The values of the enumerators shouldn't be changed because they are saved to the config file. */
enum DepartureArrivalListType {
    _UseCurrentDepartureArrivalListType = 999, /**< Only for use as default parameter to use the settings from PublicTransportSettings */

    DepartureList = 0, /**< A list of departures from the home stop */
    ArrivalList = 1 /**< A list of arrivals at the home stop */
};

/** Types of the title of the applet. */
enum TitleType {
    ShowDepartureArrivalListTitle = 0, /**< Shows an icon, the stop name and additional information */
    ShowSearchJourneyLineEdit = 1, /**< Shows a line edit for journey search requests */
    ShowSearchJourneyLineEditDisabled = 2, /**< Shows a disabled line edit for journey search requests */
    ShowJourneyListTitle = 3 /**< Shows an icon, a title and additional information */
};

/** Global states of the applet. */
enum AppletState {
    NoState 				= 0x000000, /**< No state. */

    ShowingDepartureArrivalList 	= 0x000001, /**< The applet is currently
						    * showing a departure / arrival list */
    ShowingJourneyList 			= 0x000002, /**< The applet is currently 
						    * showing a journey list */
    ShowingJourneySearch 		= 0x000004, /**< The applet is currently
						    * showing the journey search interface */
    ShowingJourneysNotSupported 	= 0x000008, /**< The applet is currently showing 
						    * an info, that journey searches aren't 
						    * supported by the current service provider. */

    WaitingForDepartureData 		= 0x000100, /**< The applet is waiting for 
						    * departure data from the data engine */
    ReceivedValidDepartureData 		= 0x000200, /**< The applet received valid departure
						    * data from the data engine */
    ReceivedErroneousDepartureData 	= 0x000400, /**< The applet received erroneous
						    * departure data from the data engine */

    WaitingForJourneyData 		= 0x001000, /**< The applet is waiting for journey
						    * data from the data engine */
    ReceivedValidJourneyData 		= 0x002000, /**< The applet received valid journey
						    * data from the data engine */
    ReceivedErroneousJourneyData 	= 0x004000, /**< The applet received erroneous
						    * journey data from the data engine */

    ConfigDialogShown 			= 0x010000, /**< The configuration dialog is
						    * currently shown */
    AccessorInfoDialogShown 		= 0x020000, /**< The accessor information
						    * dialog is currently shown */

    SettingsJustChanged 		= 0x040000, /**< The settings have just changed
						    * and dataUpdated() hasn't been called
						    * since that */
    ServiceProviderSettingsJustChanged 	= 0x080000 /**< Settings were just changed that
						    * require a new data request */
};
Q_DECLARE_FLAGS( AppletStates, AppletState )
Q_DECLARE_OPERATORS_FOR_FLAGS( AppletStates )

/** Different states of alarm. */
enum AlarmState {
    NoAlarm, /**< No alarm is set */
    AlarmPending, /**< An alarm is set and pending */
    AlarmFired /**< An alarm has been fired */
};

/** Indicates the information that an item displays (a cell in the tree view). */
enum ItemInformation {
    LineNameItem, /**< The item contains the line name */
    TargetItem, /**< The item contains the target / origin */
    DepartureItem, /**< The item contains the departure time or the arrival time if the item is an arrival */
    PlatformItem, /**< The item contains platform */
    JourneyNewsItem, /**< The item contains the journey news */
    DelayItem, /**< The item contains the delay */
    OperatorItem, /**< The item contains the operator name */
    RouteItem, /**< The item contains a list of stops in the route (to the destination for departures / arrivals) */

    VehicleTypeListItem, /**< The item contains the vehicle types of a journey */
    ArrivalItem, /**< The item contains the arrival time of a journey */
    DurationItem, /**< The items contains the duration in minutes of a journey */
    StartStopNameItem, /**< The item contains the start stop of a journey */
    TargetStopNameItem, /**< The item contains the target stop of a journey */
    ChangesItem, /**< The item contains the number of changes of a journey */
    PricingItem, /**< The item contains the pricing of a journey */
    JourneyInfoItem /**< The item contains information about a journey */
};

/** Indicates what is saved in a model item's data. */
enum ModelDataRoles {
    SortRole = Qt::UserRole, /**< Used to store sorting data */
    AlarmTimerRole = Qt::UserRole + 4, /**< Used to store the alarm timer */
    VehicleTypeRole = Qt::UserRole + 5, /**< Used to store the vehicle type */
    VehicleTypeListRole = Qt::UserRole + 6, /**< Used to store the vehicle type */
    OriginalBackgroundColorRole = Qt::UserRole + 7, /**< Used to store the original background color */
    ServiceProviderDataRole = Qt::UserRole + 8, /**< For the service provider combo box */
    RemainingMinutesRole = Qt::UserRole + 9, /**< Used to store an int with the remaining minutes until the predicted departure / arrival (= departure / arrival + delay) */
    DepartureInfoRole = Qt::UserRole + 10, /**< Used to store the departure */
    OperatorRole = Qt::UserRole + 11, /**< Used to store the operator name of the departure / arrival / journey. */
    LocationCodeRole = Qt::UserRole + 12 /**< Used to store the location code (country code or other) in the location model. */
};

/** The type of the vehicle used for a public transport line.
* The numbers here must match the ones in the data engine! */
enum VehicleType {
    Unknown = 0, /**< The type of the vehicle is unknown. */

    Tram = 1, /**< A tram / streetcar. */
    Bus = 2, /**< A bus. */
    Subway = 3, /**< A subway. */
    TrainInterurban = 4, /**< An interurban train. */
    Metro = 5, /**< A metro. */
    TrolleyBus = 6, /**< An electric bus. */

    TrainRegional = 10, /**< A regional train. */
    TrainRegionalExpress = 11, /**< A regional express train. */
    TrainInterregio = 12, /**< An inter-regional train. */
    TrainIntercityEurocity = 13, /**< An intercity / eurocity train. */
    TrainIntercityExpress = 14, /**< An intercity express. */

    Feet = 50, /**< By feet. */

    Ferry = 100, /**< A ferry. */
    Ship = 101, /**< A ship. */

    Plane = 200, /**< An aeroplane. */

    Spacecraft = 300, /**< A spacecraft. */
};

/** The type of services for a public transport line. */
enum LineService {
    NoLineService = 0, /**< The public transport line has no special services */

    NightLine = 1, /**< The public transport line is a night line */
    ExpressLine = 2 /**< The public transport line is an express line */
};

/** The type of filtering by target / origin. */
enum FilterType {
    ShowAll = 0, /**< Show all targets / origins */
    ShowMatching = 1, /**< Show only targets / origins that are in the list of filter targets / origins */
    HideMatching = 2 /**< Hide targets / origins that are in the list of filter targets / origins */
};

/** The type of the delay of a departure / arrival. */
enum DelayType {
    DelayUnknown = 0, /**< No information about delay available */
    OnSchedule = 1, /**< Vehicle will depart / arrive on schedule */
    Delayed = 2 /**< Vehicle will depart / arrive with delay */
};

/** Different config modes for the time of the first departure. */
enum FirstDepartureConfigMode {
    RelativeToCurrentTime = 0, /**< Uses the current date and time and adds an offset. */
    AtCustomTime = 1 /**< Uses a custom time, but the current date. */
};

/** @class Global
@brief Contains global static methods. */
class Global {
    public:
	static KIcon putIconIntoBiggerSizeIcon( const KIcon &icon, const QSize &iconSize, const QSize &resultingSize = QSize(32, 32) );

	/** Create an "international" icon with some flag icons */
	static KIcon internationalIcon();

	/** Creates an icon that has another icon as overlay on the bottom right. */
	static KIcon makeOverlayIcon( const KIcon &icon, const KIcon &overlayIcon, const QSize &overlaySize = QSize(10, 10), int iconExtend = 16 );

	/** Creates an icon that has another icon as overlay on the bottom right. */
	static KIcon makeOverlayIcon( const KIcon &icon, const QString &overlayIconName, const QSize &overlaySize = QSize(10, 10), int iconExtend = 16 );

	/** Creates an icon that has other icons as overlay on the bottom. */
	static KIcon makeOverlayIcon( const KIcon &icon, const QList<KIcon> &overlayIconsBottom, const QSize &overlaySize = QSize(10, 10), int iconExtend = 16 );

	/** Gets an icon for the given type of vehicle. */
	static KIcon iconFromVehicleType( const VehicleType &vehicleType, const QString &overlayIcon = QString() );

	/** Gets an icon containing the icons of all vehicle types in the given list. */
	static KIcon iconFromVehicleTypeList( const QList<VehicleType> &vehicleTypes,
					      int extend = 32 );


	/** Gets the name of the given type of vehicle. */
	static QString vehicleTypeToString( const VehicleType &vehicleType, bool plural = false );

	/** Gets a string like "25 minutes". */
	static QString durationString( int seconds );
};

#endif // ENUMS_HEADER