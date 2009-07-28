/*
 *   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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
* @brief This file contains classes to store departure / arrival / journey information.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef DEPARTUREINFO_HEADER
#define DEPARTUREINFO_HEADER

// Qt includes
#include <QTime>
#include <QDebug>

// Own includes
#include "enums.h"
#include <QVariant>

/** LineService-Flags. @see LineService */
Q_DECLARE_FLAGS( LineServices, LineService )


/** @class PublicTransportInfo
* @brief This is the abstract base class of all other timetable information classes.
* @see JourneyInfo
* @see DepartureInfo
*/
class PublicTransportInfo {
    public:
	/** Constructs a new PublicTransportInfo object. */
	PublicTransportInfo() {};

	/** Constructs a new PublicTransportInfo object.
	* @param departure The date and time of the departure / arrival of the vehicle. */
	PublicTransportInfo( const QDateTime &departure, const QString &operatorName = QString() ) {
	    m_departure = departure;
	    m_operator = operatorName;
	};

	virtual ~PublicTransportInfo() {};

	/** Wheather or not this PublicTransportInfo object is valid.
	* @return true if the PublicTransportInfo object is valid.
	* @return false if the PublicTransportInfo object is invalid. */
	virtual bool isValid() const = 0;

	/** Gets the date and time of the departure or arrival. */
	QDateTime departure() const { return m_departure; };

	/** Get the company that is responsible for this departure / arrival. */
	QString operatorName() const { return m_operator; };

	/** Parses the given string for a vehicle type.
	* @param sLineType The string to be parsed (e.g. "STR", "ICE", "RB", ...).
	* @return The type of vehicle that was parsed or VehicleType::Unknown if
	* it couldn't be parsed. */
	static VehicleType getVehicleTypeFromString( const QString &sLineType );

    protected:
	QString m_operator; /**< Stores the name of the company that is responsibe for this departure / arrival / journey. */
	QDateTime m_departure; /**< Stores a departure time. Can be used by derived classes. */
};

/** @class JourneyInfo
* @brief This class stores information about journeys with public transport.
* @see DepartureInfo
* @see PublicTransportInfo
*/
class JourneyInfo : public PublicTransportInfo {
    public:
	/** Constructs a new JourneyInfo object.
        * @param vehicleTypes The types of vehicle used in the journey.
        * @param startStopName The stop name at which the journey starts.
        * @param targetStopName The stop name of the target of the journey.
	* @param departure The date and time of the departure from the starting stop.
	* @param arrival The date and time of the arrival at the journey target.
	* @param duration The duration in minutes of the journey.
        * @param changes How many changes between different vehicles are needed.
	* @param pricing Information about the pricing of the journey.
	* @param journeyNews News for the journey, such as "platform changed".
	* @param operatorName The company that is responsible for this departure / arrival. */
	JourneyInfo( const QList<VehicleType> &vehicleTypes, const QString &startStopName, const QString &targetStopName, const QDateTime &departure, const QDateTime &arrival, int duration, int changes = 0, const QString &pricing = QString(), const QString &journeyNews = QString(), const QString &operatorName = QString() );

	/** Wheather or not the journey is valid.
	* @return true if the journey is valid.
	* @return false if the journey is invalid. */
	virtual bool isValid() const { return m_changes >= 0; };

	/** Gets information about the pricing of the journey. */
	QString pricing() const { return m_pricing; };
	/** Gets news for the journey, such as "platform changed". */
	QString journeyNews() const { return m_journeyNews; };
	/** Gets the stop name at which the journey starts */
	QString startStopName() const { return m_startStopName; };
	/** Gets the stop name of the target of the journey */
	QString targetStopName() const { return m_targetStopName; };
	/** Gets the date and time of the arrival at the journey target */
	QDateTime arrival() const { return m_arrival; };
	/** Gets the duration in minutes of the journey. */
	int duration() const { return m_duration; };
	/** Gets the types of vehicle used in the journey. */
	QList<VehicleType> vehicleTypes() const { return m_vehicleTypes; };
	/** Gets the types of vehicle used in the journey as QVariantList to be stored
	* in an Plasma::DataEngine::Data object. It contains the vehicle types casted
	* from VehicleType to int using "static_cast<int>". */
	QVariantList vehicleTypesVariant() const {
	    QVariantList listVariant;
	    foreach ( VehicleType vehicleType, m_vehicleTypes )
		listVariant.append( static_cast<int>(vehicleType) );
	    return listVariant;
	};
	/** Gets how many changes between different vehicles are needed */
	int changes() const { return m_changes; };

    private:
	/** Initializes this JourneyInfo object. This is used by the constructors.
	* @param vehicleTypes The types of vehicle used in the journey.
	* @param startStopName The stop name at which the journey starts.
	* @param targetStopName The stop name of the target of the journey.
	* @param departure The date and time of the departure from the starting stop.
	* @param arrival The date and time of the arrival at the journey target.
	* @param duration The duration in minutes of the journey.
	* @param changes How many changes between different vehicles are needed.
	* @param pricing Information about the pricing of the journey.
	* @param journeyNews News for the journey, such as "platform changed". */
	void init( const QList<VehicleType> &vehicleTypes, const QString &startStopName, const QString &targetStopName, const QDateTime &departure, const QDateTime &arrival, int duration, int changes = 0, const QString &pricing = QString(), const QString &journeyNews = QString() );

	QString m_pricing, m_startStopName, m_targetStopName, m_journeyNews;
	QDateTime m_arrival;
	QList<VehicleType> m_vehicleTypes;
	int m_duration, m_changes;
};

/** @class DepartureInfo
* @brief This class stores information about departures / arrivals with public transport.
* @see JourneyInfo
* @see PublicTransportInfo
*/
class DepartureInfo : public PublicTransportInfo {
    public:
	/** Constructs an invalid DepartureInfo object. */
	DepartureInfo();

	/** Constructs a new DepartureInfo object.
	* @param line The name of the public transport line (e.g. "6", "3S", "S 4", "RB 24122").
	* @param typeOfVehicle The type of vehicle used.
	* @param target The target / origin of the vehicle.
	* @param departure The date and time of the departure / arrival of the vehicle.
	* @param nightLine Wheather or not the public transport line is a night line.
	* @param expressLine Wheather or not the public transport line is an express line.
	* @param platform The platform from/at which the vehicle departs/arrives.
	* @param delay The delay in minutes of the vehicle. -1 means that no delay information is available.
	* @param delayReason The reason of a delay.
	* @param journeyNews News for the departure / arrival, such as "platform changed".
	* @param operatorName The company that is responsible for this departure / arrival. */
	DepartureInfo( const QString &line, const VehicleType &typeOfVehicle, const QString &target, const QDateTime &departure, bool nightLine = false, bool expressLine = false, const QString &platform = QString(), int delay = -1, const QString &delayReason = QString(), const QString &journeyNews = QString(), const QString &operatorName = QString() );

	/** Constructs a new DepartureInfo object.
	* @param line The name of the public transport line (e.g. "6", "3S", "S 4", "RB 24122").
	* @param typeOfVehicle The type of vehicle used.
	* @param target The target / origin of the vehicle.
	* @param requestTime The time at which the departure / arrival was requested from the service provider.
	* @param departureTime The date and time of the departure / arrival of the vehicle.
	* @param nightLine Wheather or not the public transport line is a night line.
	* @param expressLine Wheather or not the public transport line is an express line.
	* @param platform The platform from/at which the vehicle departs/arrives.
	* @param delay The delay in minutes of the vehicle. -1 means that no delay information is available.
	* @param delayReason The reason of a delay.
	* @param journeyNews News for the departure / arrival, such as "platform changed".
	* @param operatorName The company that is responsible for this departure / arrival. */
	DepartureInfo( const QString &line, const VehicleType &typeOfVehicle, const QString &target, const QTime &requestTime, const QTime &departureTime, bool nightLine = false, bool expressLine = false, const QString &platform = QString(), int delay = -1, const QString &delayReason = QString(), const QString &journeyNews = QString(), const QString &operatorName = QString() );

	/** Wheather or not the departure / arrival is valid.
	* @return true if the departure / arrival is valid.
        * @return false if the departure / arrival is invalid. */
	virtual bool isValid() const { return !m_line.isEmpty(); };

	/** Gets the target / origin of the departing / arriving vehicle. */
	QString target() const { return m_target; };
	/** Gets the line name of the departing / arriving vehicle. */
	QString line() const { return m_line; };
	/** Gets the type of the departing / arriving vehicle. */
	VehicleType vehicleType() const { return m_vehicleType; };
	/** Wheather or not the departing / arriving vehicle is a night line. */
	bool isNightLine() const { return m_lineServices.testFlag( NightLine ); };
	/** Wheather or not the departing / arriving vehicle is an express line. */
	bool isExpressLine() const { return m_lineServices.testFlag( ExpressLine ); };
	/** Gets the platform from/at which the vehicle departs/arrives. */
	QString platform() const { return m_platform; };
	/** Gets the delay in minutes of the vehicle. -1 means that no delay information is available. */
	int delay() const { return m_delay; };
	/** Gets the delay reason. */
	QString delayReason() const { return m_delayReason; };
	/** Gets news for the departure / arrival, such as "platform changed". */
	QString journeyNews() const { return m_journeyNews; };

    private:
	/** Initializes this DepartureInfo object. This is used by the constructors.
	* @param line The name of the public transport line (e.g. "6", "3S", "S 4", "RB 24122").
	* @param typeOfVehicle The type of vehicle used.
	* @param target The target / origin of the vehicle.
	* @param departure The date and time of the departure / arrival of the vehicle.
	* @param nightLine Wheather or not the public transport line is a night line.
	* @param expressLine Wheather or not the public transport line is an express line.
	* @param platform The platform from/at which the vehicle departs/arrives.
	* @param delay The delay in minutes of the vehicle. -1 means that no delay information is available.
	* @param delayReason The reason of a delay.
	* @param journeyNews News for the departure / arrival, such as "platform changed". */
	void init( const QString &line, const VehicleType &typeOfVehicle, const QString &target, const QDateTime &departure, bool nightLine = false, bool expressLine = false, const QString &platform = QString(), int delay = -1, const QString &delayReason = QString(), const QString &sJourneyNews = QString() );

	QString m_target, m_line, m_platform, m_delayReason, m_journeyNews;
	int m_delay;
	VehicleType m_vehicleType;
	LineServices m_lineServices;
};


#endif // DEPARTUREINFO_HEADER
