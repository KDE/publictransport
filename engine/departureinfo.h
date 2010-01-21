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
* @see JourneyInfo
* @see DepartureInfo
*/
class PublicTransportInfo {
    public:
	/** Constructs a new PublicTransportInfo object. */
	PublicTransportInfo() {};
	
	/** Contructs a new PublicTransportInfo object based on the information given
	* with @p data.
	* @param data A hash that contains values for TimetableInformations. */
	PublicTransportInfo( const QHash<TimetableInformation, QVariant> &data );
	
	/** Constructs a new PublicTransportInfo object.
	* @param departure The date and time of the departure / arrival of the vehicle. */
	PublicTransportInfo( const QDateTime &departure, const QString &operatorName = QString() ) {
	    m_data[ DepartureDate ] = departure.date();
	    m_data[ DepartureYear ] = departure.date().year();
	    m_data[ DepartureHour ] = departure.time().hour();
	    m_data[ DepartureMinute ] = departure.time().minute();
	    m_data[ Operator ] = operatorName;
	};

	virtual ~PublicTransportInfo() {};

	/** Wheather or not this PublicTransportInfo object is valid.
	* @return true if the PublicTransportInfo object is valid.
	* @return false if the PublicTransportInfo object is invalid. */
	virtual bool isValid() const { return m_isValid; };

	/** Gets the date and time of the departure or arrival. */
	QDateTime departure() const {
	    if ( m_data.contains(DepartureDate) ) {
		return QDateTime( m_data[DepartureDate].toDate(),
				  QTime(m_data[DepartureHour].toInt(),
					m_data[DepartureMinute].toInt()) );
	    } else
		return QDateTime(); };

	/** Get the company that is responsible for this departure / arrival. */
	QString operatorName() const { return m_data.contains(Operator)
		? m_data[Operator].toString() : QString(); };

	/** Gets a list of stops of the departure / arrival to it's destination 
	* stop or a list of stops of the journey from it's start to it's 
	* destination stop.
	* @note: If @ref routeStops and @ref routeTimes are both set,
	* they contain the same number of elements. And elements with equal 
	* indices are associated (the times at which the vehicle is at the 
	* stops).
	* @see routeTimes 
	* @see routeTimesVariant */
	QStringList routeStops() const { return m_data.contains(RouteStops)
		? m_data[RouteStops].toStringList() : QStringList(); };

	/** The number of exact route stops. The route stop list isn't complete 
	* from the last exact route stop. */
	int routeExactStops() const { return m_data.contains(RouteExactStops)
		? m_data[RouteExactStops].toInt() : 0; };
		
	/** Parses the given string for a vehicle type.
	* @param sLineType The string to be parsed (e.g. "STR", "ICE", "RB", ...).
	* @return The type of vehicle that was parsed or VehicleType::Unknown if
	* it couldn't be parsed. */
	static VehicleType getVehicleTypeFromString( const QString &sLineType );
	
	/** Gets the operator for the given vehicle type string.
	* @param sLineType The string to get the operator for (e.g. "ME", "ERB", "NWB", ...).
	* @return The operator for the given vehicle type string or QString() if
	* it couldn't be determined. */
	static QString operatorFromVehicleTypeString( const QString &sLineType );

    protected:
	bool m_isValid;
	QHash< TimetableInformation, QVariant > m_data;
	//QString m_operator; /**< Stores the name of the company that is responsibe for this departure / arrival / journey. */
	//QDateTime m_departure; /**< Stores a departure time. Can be used by derived classes. */
};

/** @class JourneyInfo
* @brief This class stores information about journeys with public transport.
* @see DepartureInfo
* @see PublicTransportInfo
*/
class JourneyInfo : public PublicTransportInfo {
    public:
	/** Contructs a new JourneyInfo object based on the information given
	* with @p data.
	* @param data A hash that contains values for at least the required
	* TimetableInformations (TransportLine, Target, DepartureHour, DepartureMinute). */
	JourneyInfo( const QHash<TimetableInformation, QVariant> &data );
	
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
	JourneyInfo( const QList<VehicleType> &vehicleTypes,
		     const QString &startStopName, const QString &targetStopName,
		     const QDateTime &departure, const QDateTime &arrival,
		     int duration, int changes = 0, const QString &pricing = QString(),
		     const QString &journeyNews = QString(),
		     const QString &operatorName = QString() );

	/** Gets information about the pricing of the journey. */
	QString pricing() const { return m_data.contains(Pricing)
		? m_data[Pricing].toString() : QString(); };
	/** Gets news for the journey, such as "platform changed". */
	QString journeyNews() const { return m_data.contains(JourneyNews)
		? m_data[JourneyNews].toString() : QString(); };
	/** Gets the stop name at which the journey starts */
	QString startStopName() const { return m_data.contains(StartStopName)
		? m_data[StartStopName].toString() : QString(); };
	/** Gets the stop name of the target of the journey */
	QString targetStopName() const { return m_data.contains(TargetStopName)
		? m_data[TargetStopName].toString() : QString(); };
	/** Gets the date and time of the arrival at the journey target */
	QDateTime arrival() const {
	    if ( m_data.contains(ArrivalDate) ) {
		return QDateTime( m_data[ArrivalDate].toDate(),
				  QTime(m_data[ArrivalHour].toInt(),
					m_data[ArrivalMinute].toInt()) );
	    } else if ( m_data.contains(ArrivalHour) && m_data.contains(ArrivalMinute) ) {
		return QDateTime( QDate::currentDate(),
				  QTime(m_data[ArrivalHour].toInt(),
					m_data[ArrivalMinute].toInt()) );
	    } else
		return QDateTime(); };
	/** Gets the duration in minutes of the journey. */
	int duration() const { return m_data.contains(Duration)
		? m_data[Duration].toInt() : -1; };
	/** Gets the types of vehicle used in the journey. */
	QList<VehicleType> vehicleTypes() const {
	    if ( m_data.contains(TypesOfVehicleInJourney) ) {
		QVariantList listVariant = m_data[TypesOfVehicleInJourney].toList();
		QList<VehicleType> listRet;
		foreach ( QVariant vehicleType, listVariant )
		    listRet.append( static_cast<VehicleType>(vehicleType.toInt()) );
		return listRet;
	    } else
		return QList<VehicleType>();
	};
	
	/** Gets the types of vehicle used in the journey as QVariantList to be stored
	* in a Plasma::DataEngine::Data object. */
	QVariantList vehicleTypesVariant() const { return m_data.contains(TypesOfVehicleInJourney)
		? m_data[TypesOfVehicleInJourney].toList() : QVariantList(); };

	/** Gets the types of vehicle used for each "sub-journey" in the journey as 
	* QVariantList to be stored in a Plasma::DataEngine::Data object. */
	QVariantList routeVehicleTypesVariant() const { return m_data.contains(RouteTypesOfVehicles)
		? m_data[RouteTypesOfVehicles].toList() : QVariantList(); };
		
	/** Gets the transport line used for each "sub-journey" in the journey. */
	QStringList routeTransportLines() const { return m_data.contains(RouteTransportLines)
		? m_data[RouteTransportLines].toStringList() : QStringList(); };
	
	/** Gets the platform of the departure used for each stop in the journey. */
	QStringList routePlatformsDeparture() const {
		return m_data.contains(RoutePlatformsDeparture)
		? m_data[RoutePlatformsDeparture].toStringList() : QStringList(); };

	/** Gets the platform of the arrival used for each stop in the journey. */
	QStringList routePlatformsArrival() const {
		return m_data.contains(RoutePlatformsArrival)
		? m_data[RoutePlatformsArrival].toStringList() : QStringList(); };
		
	/** Gets how many changes between different vehicles are needed */
	int changes() const { return m_data.contains(Changes)
		? m_data[Changes].toInt() : -1; };

	/** Gets a list of departure times of the journey. Use QVariant::toTime() 
	* to convert the QVariants in the list to QTime objects or just use 
	* @ref routeTimesDeparture to get a list with values converted to QTime.
	* @note: If @ref routeStops and @ref routeTimesDeparture are both set,
	* the latter contains one element less (because the last stop has no 
	* departure, only an arrival time). Elements with equal indices are
	* associated (the times at which the vehicle departs from the stops).
	* @see routeTimesDeparture
	* @see routeStops */
	QVariantList routeTimesDepartureVariant() const {
		return m_data.contains(RouteTimesDeparture)
		    ? m_data[RouteTimesDeparture].toList() : QVariantList(); };

	/** Gets a list of times of the departure / arrival to it's destination
	* stop.
	* @note: If @ref routeStops and @ref routeTimesDeparture are both set,
	* the latter contains one element less (because the last stop has no
	* departure, only an arrival time). Elements with equal indices are
	* associated (the times at which the vehicle departs from the stops).
	* @see routeTimesDepartureVariant
	* @see routeStops */
	QList<QTime> routeTimesDeparture() const {
		if ( m_data.contains(RouteTimesDeparture) ) {
		    QList<QTime> ret;
		    QVariantList times = m_data[RouteTimesDeparture].toList();
		    foreach ( QVariant time, times )
			ret << time.toTime();
		    return ret;
		} else
		    return QList<QTime>(); };
		
	/** Gets a list of arrival times of the journey. Use QVariant::toTime()
	* to convert the QVariants in the list to QTime objects or just use
	* @ref routeTimesArrival to get a list with values converted to QTime.
	* @note: If @ref routeStops and @ref routeTimesArrival are both set,
	* the latter contains one element less (because the first stop has no 
	* arrival, only a departure time). Elements with equal indices should 
	* be associated (the times at which the vehicle arrives at the stops).
	* @see routeTimesArrival
	* @see routeStops */
	QVariantList routeTimesArrivalVariant() const {
		return m_data.contains(RouteTimesArrival)
		    ? m_data[RouteTimesArrival].toList() : QVariantList(); };

	/** Gets a list of arrival times of the journey.
	* @note: If @ref routeStops and @ref routeTimesArrival are both set,
	* the latter contains one element less (because the first stop has no
	* arrival, only a departure time). Elements with equal indices should
	* be associated (the times at which the vehicle arrives at the stops).
	* @see routeTimesArrivalVariant
	* @see routeStops */
	QList<QTime> routeTimesArrival() const {
		if ( m_data.contains(RouteTimesArrival) ) {
		    QList<QTime> ret;
		    QVariantList times = m_data[RouteTimesArrival].toList();
		    foreach ( QVariant time, times )
			ret << time.toTime();
		    return ret;
		} else
		    return QList<QTime>(); };

	QVariantList routeTimesDepartureDelay() const {
		if ( m_data.contains(RouteTimesDepartureDelay) )
		    return m_data[RouteTimesDepartureDelay].toList();
		else
		    return QVariantList(); };
		
	QVariantList routeTimesArrivalDelay() const {
		if ( m_data.contains(RouteTimesArrivalDelay) )
		    return m_data[RouteTimesArrivalDelay].toList();
		else
		    return QVariantList(); };
		
    private:
	/** Initializes this JourneyInfo object. This is used by the constructors.
	* @param vehicleTypes The types of vehicle used in the journey.
	* @param startStopName The stop name at which the journey starts.
	* @param targetStopName The stop name of the target of the journey.
	* @param arrival The date and time of the arrival at the journey target.
	* @param duration The duration in minutes of the journey.
	* @param changes How many changes between different vehicles are needed.
	* @param pricing Information about the pricing of the journey.
	* @param journeyNews News for the journey, such as "platform changed". */
	void init( const QList<VehicleType> &vehicleTypes, const QString &startStopName,
		   const QString &targetStopName, const QDateTime &arrival,
		   int duration, int changes = 0, const QString &pricing = QString(),
		   const QString &journeyNews = QString() );

// 	QString m_pricing, m_startStopName, m_targetStopName, m_journeyNews;
// 	QDateTime m_arrival;
// 	QList<VehicleType> m_vehicleTypes;
// 	int m_duration, m_changes;
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
	
	/** Contructs a new DepartureInfo object based on the information given
	* with @p data.
	* @param data A hash that contains values for at least the required 
	* TimetableInformations (TransportLine, Target, DepartureHour, DepartureMinute). */
	DepartureInfo( const QHash<TimetableInformation, QVariant> &data );

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
	DepartureInfo( const QString &line, const VehicleType &typeOfVehicle,
		       const QString &target, const QDateTime &departure,
		       bool nightLine = false, bool expressLine = false,
		       const QString &platform = QString(), int delay = -1,
		       const QString &delayReason = QString(),
		       const QString &journeyNews = QString(),
		       const QString &operatorName = QString() );

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
	DepartureInfo( const QString &line, const VehicleType &typeOfVehicle,
		       const QString &target, const QTime &requestTime,
		       const QTime &departureTime, bool nightLine = false,
		       bool expressLine = false, const QString &platform = QString(),
		       int delay = -1, const QString &delayReason = QString(),
		       const QString &journeyNews = QString(),
		       const QString &operatorName = QString() );

	/** Gets the target / origin of the departing / arriving vehicle. */
	QString target() const { return m_data.contains(Target)
		? m_data[Target].toString() : QString(); };
	/** Gets the line name of the departing / arriving vehicle. */
	QString line() const { return m_data.contains(TransportLine)
		? m_data[TransportLine].toString() : QString(); };
	/** Gets the type of the departing / arriving vehicle. */
	VehicleType vehicleType() const { return m_data.contains(TypeOfVehicle)
		? static_cast<VehicleType>( m_data[TypeOfVehicle].toInt() ) : Unknown; };
	/** Wheather or not the departing / arriving vehicle is a night line. */
	bool isNightLine() const { return m_lineServices.testFlag( NightLine ); };
	/** Wheather or not the departing / arriving vehicle is an express line. */
	bool isExpressLine() const { return m_lineServices.testFlag( ExpressLine ); };
	/** Gets the platform from/at which the vehicle departs/arrives. */
	QString platform() const { return m_data.contains(Platform)
		? m_data[Platform].toString() : QString(); };
	/** Gets the delay in minutes of the vehicle. -1 means that no delay information is available. */
	int delay() const { return m_data.contains(Delay)
		? m_data[Delay].toInt() : -1; };
	/** Gets the delay reason. */
	QString delayReason() const { return m_data.contains(DelayReason)
		? m_data[DelayReason].toString() : QString(); };
	/** Gets news for the departure / arrival, such as "platform changed". */
	QString journeyNews() const { return m_data.contains(JourneyNews)
		? m_data[JourneyNews].toString() : QString(); };

	/** Gets a list of times of the departure / arrival to it's destination
	* stop. Use QVariant::toTime() to convert the QVariants in the list to 
	* QTime objects or just use @ref routeTimes to get a list with values
	* converted to QTime.
	* @note: If @ref routeStops and @ref routeTimes are both set,
	* they contain the same number of elements. And elements with equal
	* indices are associated (the times at which the vehicle is at the
	* stops).
	* @see routeTimes
	* @see routeStops */
	QVariantList routeTimesVariant() const { return m_data.contains(RouteTimes)
		? m_data[RouteTimes].toList() : QVariantList(); };

	/** Gets a list of times of the departure / arrival to it's destination
	* stop.
	* @note: If @ref routeStops and @ref routeTimes are both set,
	* they contain the same number of elements. And elements with equal
	* indices are associated (the times at which the vehicle is at the
	* stops).
	* @see routeTimesVariant
	* @see routeStops */
	QList<QTime> routeTimes() const {
		if ( m_data.contains(RouteTimes) ) {
		    QList<QTime> ret;
		    QVariantList times = m_data[RouteTimes].toList();
		    foreach ( QVariant time, times )
			ret << time.toTime();
		    return ret;
		} else
		    return QList<QTime>(); };
		
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
	void init( const QString &line, const VehicleType &typeOfVehicle,
		   const QString &target, 
		   bool nightLine = false, bool expressLine = false,
		   const QString &platform = QString(), int delay = -1,
		   const QString &delayReason = QString(),
		   const QString &sJourneyNews = QString() );

// 	QString m_target, m_line, m_platform, m_delayReason, m_journeyNews;
// 	int m_delay;
// 	VehicleType m_vehicleType;
	LineServices m_lineServices;
};


#endif // DEPARTUREINFO_HEADER