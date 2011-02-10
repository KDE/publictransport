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

#ifndef ENUMS_HEADER
#define ENUMS_HEADER

/** @file
* @brief This file contains enumerations used by the public transport helper library.
* 
* @author Friedrich Pülz <fpuelz@gmx.de> */

#include "publictransporthelper_export.h"
#include <qnamespace.h>

/**
 * @brief Contains keys for different stop settings.
 * 
 * Indices beginning at UserSetting may be used to store custom data.
 * 
 * For each enumerable a data type is defined, which can be used for example in 
 * @ref StopSettings::get. If you want to get the value of eg. <em>TimeOfFirstDepartureSetting</em>
 * you can write something like this:
 * @code
 * StopSettings stopSettings;
 * stopSettings.set( TimeOfFirstDepartureSetting, QTime(12, 0) );
 * QTime time = stopSettings.get<QTime>( TimeOfFirstDepartureSetting );
 * QTime time2 = stopSettings[ TimeOfFirstDepartureSetting ].toTime();
 * @endcode
 * The last two lines do the same thing and <em>time</em> and <em>time2</em> are equal. You can
 * use the function you prefer to get the value. The data type <em>QTime</em> can be retrieved
 * from the documentation for the enumerable <em>TimeOfFirstDepartureSetting</em> (At the end
 * in parantheses).
 **/
enum StopSetting {
	NoSetting = 0, /**< Don't use any setting. */
	
	LocationSetting = 1, /**< The location of the stop, eg. a country (QString). */
	ServiceProviderSetting = 2, /**< The ID of the service provider of the stop (QString). */
	CitySetting = 3, /**< The city of the stop, if used by the service provider (QString). */
	StopNameSetting = 4, /**< The stop name (Stop). */
	
	FilterConfigurationSetting = 10, /**< The filter configuration to be used for the stop (QString). */
	AlarmTimeSetting = 11, /** The time in minutes before the departure 
			* at which the alarm should be fired (int). */
	FirstDepartureConfigModeSetting = 12, /**< The config mode for the time 
			* of the first departure (enum FirstDepartureConfigMode, int). */
	TimeOffsetOfFirstDepartureSetting = 13, /**< The offset in minutes from the current time 
			* until the first departure (int). */
	TimeOfFirstDepartureSetting = 14, /**< A custom time for the first departure (QTime). */
	
	UserSetting = 100 /**< The first index to be used for custom data (QVariant). Widgets
			* to edit custom settings can be created for the @ref StopSettingsDialog 
			* using a @ref StopSettingsWidgetFactory. */
};

/** @brief Different config modes for the time of the first departure. */
enum PUBLICTRANSPORTHELPER_EXPORT FirstDepartureConfigMode {
	RelativeToCurrentTime = 0, /**< Uses the current date and time and adds an offset. */
	AtCustomTime = 1 /**< Uses a custom time, but the current date. */
};

/** @brief Indicates what is saved in a model item's data. */
enum PUBLICTRANSPORTHELPER_EXPORT ModelDataRoles {
	SortRole = Qt::UserRole, /**< Used to store sorting data */
	ServiceProviderDataRole = Qt::UserRole + 8, /**< For the service provider combo box */
	DepartureInfoRole = Qt::UserRole + 10, /**< Used to store the departure */
	LocationCodeRole = Qt::UserRole + 12, /**< Used to store the location code
			* (country code or other) in the location model. */
	TimetableItemHashRole = Qt::UserRole + 13, /**< Used to store a hash for the current
			* timetable item in the model. */
	ServiceProviderIdRole = Qt::UserRole + 14, /** Used to store the service provider ID. */

	// Additional data roles used by DepartureModel / JourneyModel.
	FormattedTextRole = Qt::UserRole + 500, /**< Used to store formatted text.
			* The text of an item should not contain html tags, if used in a combo box. */
	DecorationPositionRole = Qt::UserRole + 501,
	DrawAlarmBackgroundRole = Qt::UserRole + 502,
	AlarmColorIntensityRole = Qt::UserRole + 503,
	JourneyRatingRole = Qt::UserRole + 504, /**< Stores a value between 0 and 1.
			* 0 for the journey with the biggest duration, 1 for the smallest duration. */
	LinesPerRowRole = Qt::UserRole + 505, /**< Used to change the number of lines for a row. */
	IconSizeRole = Qt::UserRole + 506 /**< Used to set a specific icon size for an element. */
};

/** @brief The position of the decoration. */
enum PUBLICTRANSPORTHELPER_EXPORT DecorationPosition {
	DecorationLeft, /**< Show the decoration on the left side. */
	DecorationRight /**< Show the decoration on the right side. */
};

/**
 * @brief A more general vehicle type than @ref VehicleType.
 * 
 * For example @ref VehicleType has special enumerables for different types of trains, 
 * where @ref GerenalVehicleType combines them into one enumerable.
 */
enum PUBLICTRANSPORTHELPER_EXPORT GeneralVehicleType {
	UnknownVehicle = 0,
	LocalPublicTransport = 1,
	Train = 2,
	WaterVehicle = 3,
	AirVehicle = 4
};

/**
 * @brief The type of the vehicle used for a public transport line.
 *
 * The numbers here match the ones used by the data engine.
 */
enum PUBLICTRANSPORTHELPER_EXPORT VehicleType {
	Unknown = 0, /**< The type of the vehicle is unknown. */

	Tram = 1, /**< A tram / streetcar. */
	Bus = 2, /**< A bus. */
	Subway = 3, /**< A subway. */
	TrainInterurban = 4, /**< An interurban train. @deprecated Use InterurbanTrain instead. */
	InterurbanTrain = 4, /**< An interurban train. */
	Metro = 5, /**< A metro. */
	TrolleyBus = 6, /**< A trolleybus (also known as trolley bus, trolley coach,
			* trackless trolley, trackless tram or trolley) is an electric bus that draws its
			* electricity from overhead wires (generally suspended from roadside posts)
			* using spring-loaded trolley poles. */

	TrainRegional = 10, /**< A regional train. @deprecated Use RegionalTrain instead. */
	TrainRegionalExpress = 11, /**< A regional express train. @deprecated Use RegionalExpressTrain instead. */
	TrainInterregio = 12, /**< An inter-regional train. @deprecated Use InterregionalTrain instead. */
	TrainIntercityEurocity = 13, /**< An intercity / eurocity train. @deprecated Use IntercityTrain instead. */
	TrainIntercityExpress = 14, /**< An intercity express. @deprecated Use HighSpeedTrain instead. */
	
	RegionalTrain = 10, /**< A regional train. Stops at many small stations, slow. */
	RegionalExpressTrain = 11, /**< A regional express train. Stops at less small stations than 
			* RegionalTrain but it faster. */
	InterregionalTrain = 12, /**< An inter-regional train. Higher distances and faster than 
			* RegionalTrain and RegionalExpressTrain. */
	IntercityTrain = 13, /**< An intercity / eurocity train. Connects cities. */
	HighSpeedTrain = 14, /**< A highspeed train, eg. an intercity express (ICE). 
			* Trains at > 250 km/h, high distances. */

	Feet = 50, /**< By feet, ie. no vehicle. Used for journeys, eg. from platform A to
			* platform B when changing the vehicle. */

	Ferry = 100, /**< A ferry. */
	Ship = 101, /**< A ship. */

	Plane = 200, /**< An aeroplane. */

	Spacecraft = 300, /**< A spacecraft. */
};

#endif // Multiple inclusion guard
