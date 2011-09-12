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

/** @file
 * @brief This file contains classes to store departure / arrival / journey information.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef DEPARTUREINFO_HEADER
#define DEPARTUREINFO_HEADER

// Qt includes
#include <QVariant>
#include <QTime>
#include <QStringList>
#include <QDebug>

// Own includes
#include "enums.h"

/** LineService-Flags. @see LineService */
Q_DECLARE_FLAGS( LineServices, LineService )


/** @class PublicTransportInfo
 * @brief This is the abstract base class of all other timetable information classes.
 *
 * @see JourneyInfo
 * @see DepartureInfo
 */
class PublicTransportInfo : public QHash<TimetableInformation, QVariant> {
public:
	/** @brief Constructs a new PublicTransportInfo object. */
	PublicTransportInfo() : QHash<TimetableInformation, QVariant>() {};

	/**
	 * @brief Contructs a new PublicTransportInfo object based on the information given
	 *   with @p data.
	 *
	 * @param data A hash that contains values for TimetableInformations. */
	explicit PublicTransportInfo( const QHash<TimetableInformation, QVariant> &data );

	virtual ~PublicTransportInfo() {};

	/**
	 * @brief Wheather or not this PublicTransportInfo object is valid.
	 *
	 * @return true if the PublicTransportInfo object is valid.
	 * 
	 * @return false if the PublicTransportInfo object is invalid. */
	virtual bool isValid() const { return m_isValid; };

	/**
	 * @brief Gets the date and time of the departure or arrival. */
	QDateTime departure() const {
	    if ( contains(DepartureDate) ) {
			return QDateTime( value(DepartureDate).toDate(), value(DepartureTime).toTime() );
	    } else {
			return QDateTime( QDate::currentDate(), value(DepartureTime).toTime() );
		}
	};

	/** @brief Get the company that is responsible for this departure / arrival. */
	QString operatorName() const { 
			return contains(Operator) ? value(Operator).toString() : QString(); };

	/**
	 * @brief Gets a list of stops of the departure/arrival to it's destination
	 *   stop or a list of stops of the journey from it's start to it's
	 *   destination stop.
	 *
	 * @note: If data for both @ref RouteStops and @ref RouteTimes is set,
	 *   they contain the same number of elements. And elements with equal
	 *   indices are associated (the times at which the vehicle is at the stops).
	 * 
	 * @see routeTimes
	 * @see routeTimesVariant */
	QStringList routeStops() const { 
			return contains(RouteStops) ? value(RouteStops).toStringList() : QStringList(); };

	/**
	 * @brief The number of exact route stops. The route stop list isn't complete
	 *   from the last exact route stop. */
	int routeExactStops() const { 
			return contains(RouteExactStops) ? value(RouteExactStops).toInt() : 0; };

	/**
	 * @brief Parses the given string for a vehicle type.
	 *
	 * @param sLineType The string to be parsed (e.g. "STR", "ICE", "RB", ...).
	 * 
	 * @return The type of vehicle that was parsed or VehicleType::Unknown if
	 *   it couldn't be parsed. */
	static VehicleType getVehicleTypeFromString( const QString &sLineType );

	/**
	 * @brief Gets the operator for the given vehicle type string.
	 *
	 * @param sLineType The string to get the operator for (e.g. "ME", "ERB", "NWB", ...).
	 * 
	 * @return The operator for the given vehicle type string or QString() if
	 *   it couldn't be determined. */
	static QString operatorFromVehicleTypeString( const QString &sLineType );

protected:
	bool m_isValid;
};

/** @class JourneyInfo
 * @brief This class stores information about journeys with public transport.
 * 
 * @see DepartureInfo
 * @see PublicTransportInfo
 */
class JourneyInfo : public PublicTransportInfo {
public:
	/**
	 * @brief Contructs a new JourneyInfo object based on the information given with @p data.
	 *
	 * @param data A hash that contains values for at least the required
	 *   TimetableInformations (TransportLine, Target, DepartureHour, DepartureMinute). */
	explicit JourneyInfo( const QHash<TimetableInformation, QVariant> &data );

	/** @brief Gets information about the pricing of the journey. */
	QString pricing() const { 
			return contains(Pricing) ? value(Pricing).toString() : QString(); };
	/** @brief Gets news for the journey, such as "platform changed". */
	QString journeyNews() const { 
			return contains(JourneyNews) ? value(JourneyNews).toString() : QString(); };
	/** @brief Gets the stop name at which the journey starts */
	QString startStopName() const { 
			return contains(StartStopName) ? value(StartStopName).toString() : QString(); };
	/** @brief Gets the stop name of the target of the journey */
	QString targetStopName() const { 
			return contains(TargetStopName) ? value(TargetStopName).toString() : QString(); };
	/** @brief Gets the date and time of the arrival at the journey target */
	QDateTime arrival() const {
		if ( contains(ArrivalDate) ) {
			return QDateTime( value(ArrivalDate).toDate(), value(ArrivalTime).toTime() );
	    } else if ( contains(ArrivalTime) ) {
			return QDateTime( QDate::currentDate(), value(ArrivalTime).toTime() );
		} else {
			return QDateTime();
		}
	};
	/** @brief Gets the duration in minutes of the journey. */
	int duration() const { return contains(Duration) ? value(Duration).toInt() : -1; };
	/** @brief Gets the types of vehicle used in the journey. */
	QList<VehicleType> vehicleTypes() const {
		if ( contains(TypesOfVehicleInJourney) ) {
			QVariantList listVariant = value(TypesOfVehicleInJourney).toList();
			QList<VehicleType> listRet;
			foreach ( QVariant vehicleType, listVariant ) {
				listRet.append( static_cast<VehicleType>(vehicleType.toInt()) );
			}
			return listRet;
		} else {
			return QList<VehicleType>();
		}
	};

	QStringList vehicleIconNames() const {
		if ( !contains(TypesOfVehicleInJourney) ) {
			return QStringList();
		}
		QVariantList vehicles = value(TypesOfVehicleInJourney).toList();
		QStringList iconNames;
		foreach ( QVariant vehicle, vehicles ) {
			iconNames << Global::vehicleTypeToIcon( static_cast<VehicleType>(vehicle.toInt()) );
		}
		return iconNames;
	};

	QStringList vehicleNames(bool plural = false) const {
		if ( !contains(TypesOfVehicleInJourney) ) {
			return QStringList();
		}
		QVariantList vehicles = value(TypesOfVehicleInJourney).toList();
		QStringList names;
		foreach ( QVariant vehicle, vehicles ) {
			names << Global::vehicleTypeToString( static_cast<VehicleType>(vehicle.toInt()), plural );
		}
		return names;
	};

	/**
	 * @brief Gets the types of vehicle used in the journey as QVariantList to be stored
	 *   in a Plasma::DataEngine::Data object. */
	QVariantList vehicleTypesVariant() const { 
		return contains(TypesOfVehicleInJourney)
				? value(TypesOfVehicleInJourney).toList() : QVariantList(); 
	};

	/**
	 * @brief Gets the types of vehicle used for each "sub-journey" in the journey as
	 *   QVariantList to be stored in a Plasma::DataEngine::Data object. */
	QVariantList routeVehicleTypesVariant() const { 
		return contains(RouteTypesOfVehicles)
				? value(RouteTypesOfVehicles).toList() : QVariantList(); 
	};

	/** @brief Gets the transport line used for each "sub-journey" in the journey. */
	QStringList routeTransportLines() const {
		return contains(RouteTransportLines)
				? value(RouteTransportLines).toStringList() : QStringList(); 
	};

	/** @brief Gets the platform of the departure used for each stop in the journey. */
	QStringList routePlatformsDeparture() const {
		return contains(RoutePlatformsDeparture)
				? value(RoutePlatformsDeparture).toStringList() : QStringList(); 
	};

	/** @brief Gets the platform of the arrival used for each stop in the journey. */
	QStringList routePlatformsArrival() const {
		return contains(RoutePlatformsArrival)
				? value(RoutePlatformsArrival).toStringList() : QStringList(); 
	};

	/** @brief Gets how many changes between different vehicles are needed */
	int changes() const {
		return contains(Changes)
				? value(Changes).toInt() : -1; 
	};

	/**
	 * @brief Gets a list of departure times of the journey. Use QVariant::toTime()
	 *   to convert the QVariants in the list to QTime objects or just use
	 *
	 * @ref routeTimesDeparture to get a list with values converted to QTime.
	 * @note: If @ref routeStops and @ref routeTimesDeparture are both set,
	 *   the latter contains one element less (because the last stop has no
	 *   departure, only an arrival time). Elements with equal indices are
	 *   associated (the times at which the vehicle departs from the stops).
	 * @see routeTimesDeparture
	 * @see routeStops */
	QVariantList routeTimesDepartureVariant() const {
		return contains(RouteTimesDeparture)
				? value(RouteTimesDeparture).toList() : QVariantList(); 
	};

	/**
	 * @brief Gets a list of times of the journey to it's destination stop.
	 *
	 * @note: If @ref routeStops and @ref routeTimesDeparture are both set,
	 *   the latter contains one element less (because the last stop has no
	 *   departure, only an arrival time). Elements with equal indices are
	 *   associated (the times at which the vehicle departs from the stops).
	 * @see routeTimesDepartureVariant
	 * @see routeStops */
	QList<QTime> routeTimesDeparture() const {
		if ( contains(RouteTimesDeparture) ) {
			QList<QTime> ret;
			QVariantList times = value(RouteTimesDeparture).toList();
			foreach ( QVariant time, times ) {
				ret << time.toTime();
			}
			return ret;
		} else {
			return QList<QTime>();
		}
	};

	/**
	 * @brief Gets a list of arrival times of the journey. Use QVariant::toTime()
	 *   to convert the QVariants in the list to QTime objects or just use
	 *   @ref routeTimesArrival to get a list with values converted to QTime.
	 *
	 * @note: If @ref routeStops and @ref routeTimesArrival are both set,
	 *   the latter contains one element less (because the first stop has no
	 *   arrival, only a departure time). Elements with equal indices should
	 *   be associated (the times at which the vehicle arrives at the stops).
	 * @see routeTimesArrival
	 * @see routeStops */
	QVariantList routeTimesArrivalVariant() const {
		return contains(RouteTimesArrival)
				? value(RouteTimesArrival).toList() : QVariantList(); 
	};

	/**
	 * @brief Gets a list of arrival times of the journey.
	 *
	 * @note: If @ref routeStops and @ref routeTimesArrival are both set,
	 *   the latter contains one element less (because the first stop has no
	 *   arrival, only a departure time). Elements with equal indices should
	 *   be associated (the times at which the vehicle arrives at the stops).
	 * @see routeTimesArrivalVariant
	 * @see routeStops */
	QList<QTime> routeTimesArrival() const {
		if ( contains(RouteTimesArrival) ) {
			QList<QTime> ret;
			QVariantList times = value(RouteTimesArrival).toList();
			foreach ( QVariant time, times ) {
				ret << time.toTime();
			}
			return ret;
		} else {
			return QList<QTime>();
		}
	};

	QVariantList routeTimesDepartureDelay() const {
		if ( contains(RouteTimesDepartureDelay) ) {
			return value(RouteTimesDepartureDelay).toList();
		} else {
			return QVariantList();
		}
	};

	QVariantList routeTimesArrivalDelay() const {
		if ( contains(RouteTimesArrivalDelay) ) {
			return value(RouteTimesArrivalDelay).toList();
		} else {
			return QVariantList();
		}
	};

private:
};

/** @class DepartureInfo
 * @brief This class stores information about departures / arrivals with public transport.
 *
 * @see JourneyInfo
 * @see PublicTransportInfo
 */
class DepartureInfo : public PublicTransportInfo {
public:
	/** @brief Constructs an invalid DepartureInfo object. */
	DepartureInfo();

	/**
	 * @brief Contructs a new DepartureInfo object based on the information given with @p data.
	 *
	 * @param data A hash that contains values for at least the required
	 *   TimetableInformations (TransportLine, Target, DepartureHour, DepartureMinute). */
	explicit DepartureInfo( const QHash<TimetableInformation, QVariant> &data );

	/** @brief Gets the target / origin of the departing / arriving vehicle. */
	QString target() const { return contains(Target)
		? value(Target).toString() : QString(); };
	/** @brief Gets the line name of the departing / arriving vehicle. */
	QString line() const { return contains(TransportLine)
		? value(TransportLine).toString() : QString(); };
	/** @brief Gets the type of the departing / arriving vehicle. */
	VehicleType vehicleType() const { return contains(TypeOfVehicle)
		? static_cast<VehicleType>( value(TypeOfVehicle).toInt() ) : Unknown; };
	/** @brief Wheather or not the departing / arriving vehicle is a night line. */
	bool isNightLine() const { return m_lineServices.testFlag( NightLine ); };
	/** @brief Wheather or not the departing / arriving vehicle is an express line. */
	bool isExpressLine() const { return m_lineServices.testFlag( ExpressLine ); };
	/** @brief Gets the platform from/at which the vehicle departs/arrives. */
	QString platform() const { return contains(Platform)
		? value(Platform).toString() : QString(); };
	/** @brief Gets the delay in minutes of the vehicle. -1 means that no delay information is available. */
	int delay() const { return contains(Delay)
		? value(Delay).toInt() : -1; };
	/** @brief Gets the delay reason. */
	QString delayReason() const { return contains(DelayReason)
		? value(DelayReason).toString() : QString(); };
	/** @brief Gets news for the departure/arrival, such as "platform changed". */
	QString journeyNews() const { return contains(JourneyNews)
		? value(JourneyNews).toString() : QString(); };
		
	/** @brief Gets the status of the departure/arrival, such as "departing". */
	QString status() const { return contains(Status)
		? value(Status).toString() : QString(); };

	/**
	 * @brief Gets a list of times of the departure / arrival to it's destination
	 *   stop. Use QVariant::toTime() to convert the QVariants in the list to
	 *   QTime objects or just use @ref routeTimes to get a list with values
	 *   converted to QTime.
	 *
	 * @note: If @ref routeStops and @ref routeTimes are both set,
	 *   they contain the same number of elements. And elements with equal
	 *   indices are associated (the times at which the vehicle is at the stops).
	 * @see routeTimes
	 * @see routeStops */
	QVariantList routeTimesVariant() const { return contains(RouteTimes)
		? value(RouteTimes).toList() : QVariantList(); };

	/**
	 * @brief Gets a list of times of the departure / arrival to it's destination stop.
	 *
	 * @note: If @ref routeStops and @ref routeTimes are both set,
	 *   they contain the same number of elements. And elements with equal
	 *   indices are associated (the times at which the vehicle is at the
	 *   stops).
	 * @see routeTimesVariant
	 * @see routeStops */
	QList<QTime> routeTimes() const {
		if ( contains(RouteTimes) ) {
			QList<QTime> ret;
			QVariantList times = value(RouteTimes).toList();
			foreach ( QVariant time, times ) {
				ret << time.toTime();
			}
			return ret;
		} else {
			return QList<QTime>();
		}
	};

private:
	LineServices m_lineServices;
};

/**
 * @brief Stores information about a stop. Used for stop suggestions.
 * 
 * @see DepartureInfo
 * @see JourneyInfo
 **/
class StopInfo : public QHash<TimetableInformation, QVariant> {
public:
	/** @brief Constructs an invalid StopInfo object. */
	StopInfo();

	/**
	 * @brief Contructs a new StopInfo object based on the information given with @p data.
	 *
	 * @param data A hash that contains values for at least the required TimetableInformations 
	 *   (StopName). */
	StopInfo( const QHash<TimetableInformation, QVariant> &data );

	/**
	 * @brief Constructs a new StopInfo object.
	 * 
	 * @param name The name of the stop.
	 * 
	 * @param id The ID for the stop @p name, if available.
	 * 
	 * @param weight The weight of this stop suggestion, if available. Higher values are set for 
	 *   more important / better matching stops.
	 * 
	 * @param city The city in which the stop is, if available.
	 * 
	 * @param countryCode The code of the country in which the stop is, if available.
	 */
	StopInfo( const QString &name, const QString &id = QString(), int weight = -1,
			  const QString &city = QString(), const QString &countryCode = QString() );
	
	/** @brief Gets the name of the stop. */
	QString name() const { return value(StopName).toString(); };
	
	/** @brief Gets the ID for the stop, if available. */
	QString id() const { return value(StopID).toString(); };
	
	/** @brief Gets the weight of the stop. */
	QString weight() const { return value(StopWeight).toString(); };
	
	/** @brief Gets the city in which the stop is. */
	QString city() const { return value(StopCity).toString(); };
	
	/** @brief Gets the code of the country in which the stop is. */
	QString countryCode() const { return value(StopCountryCode).toString(); };
	
private:
	bool m_isValid;
};

#endif // DEPARTUREINFO_HEADER
