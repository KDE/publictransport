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

#include "departureinfo.h"
#include <KDebug>

PublicTransportInfo::PublicTransportInfo( const QHash< TimetableInformation, QVariant >& data ) {
    m_data = data;
    m_isValid = false;

    if ( !m_data.contains(Delay) )
	m_data.insert( Delay, -1 );

    if ( m_data.contains(RouteTimes) ) {
	QVariantList times;
	if ( m_data[RouteTimes].canConvert(QVariant::StringList) ) {
	    QStringList strings = m_data[ RouteTimes ].toStringList();
	    foreach ( QString str, strings )
		times << QTime::fromString( str.trimmed(), "hh:mm" );
	} else if ( m_data[RouteTimes].canConvert(QVariant::List) ) {
	    QVariantList vars = m_data[ RouteTimes ].toList();
	    foreach ( QVariant var, vars )
		times << var.toTime();
	}

	m_data[ RouteTimes ] = times;
    }

    if ( m_data.contains(TypeOfVehicle) && m_data[TypeOfVehicle].canConvert(QVariant::String) ) {
	m_data[ TypeOfVehicle ] = getVehicleTypeFromString( m_data[TypeOfVehicle].toString() );
    }

    if ( m_data.contains(TransportLine) ) {
	QString sVehicleType, sTransportLine = m_data[TransportLine].toString();
	QRegExp rx = QRegExp("^(bus|tro|tram|str|s|u|m)\\s*", Qt::CaseInsensitive);
	rx.indexIn( sTransportLine );
	sVehicleType = rx.cap();
	if ( !m_data.contains(TypeOfVehicle) )
	    m_data.insert( TypeOfVehicle, sVehicleType );
	else if ( m_data.value(TypeOfVehicle) == Unknown )
	    m_data[TypeOfVehicle] = sVehicleType;

	sTransportLine = sTransportLine.trimmed();
	sTransportLine = sTransportLine.remove( QRegExp("^(bus|tro|tram|str)",
							Qt::CaseInsensitive) );
	sTransportLine = sTransportLine.replace( QRegExp("\\s{2,}"), " " );

	if ( sTransportLine.contains( "F&#228;hre" ) )
	    m_data.insert( TypeOfVehicle, Ferry );

	m_data.insert( TransportLine, sTransportLine.trimmed() );
    }
    
    if ( m_data.contains(DepartureDate) && (!m_data[DepartureDate].canConvert(QVariant::Date)
		|| !m_data[DepartureDate].toDate().isValid()) ) {
	if ( m_data[DepartureDate].canConvert(QVariant::String) ) {
	    QString sDate = m_data[DepartureDate].toString();
	    
	    m_data[ DepartureDate ] = QDate::fromString( sDate, "dd.MM.yyyy" );
	    
	    if ( !m_data[DepartureDate].toDate().isValid() ) {
		int currentYear = QDate::currentDate().year();
		QDate date = QDate::fromString( sDate, "dd.MM.yy" );
		if ( date.year() <= currentYear - 99 )
		    date.setDate( currentYear, date.month(), date.day() );
		
		m_data[ DepartureDate ] = date;
	    }
	} else {
	    kDebug() << "Departure date is in wrong format:" << m_data[ DepartureDate ];
	    m_data.remove( DepartureDate );
	}
    }
    
    if ( m_data.contains(DepartureHour) && m_data.contains(DepartureMinute)
		&& !m_data.contains(DepartureDate) ) {
	QTime departureTime = QTime( m_data[DepartureHour].toInt(),
				     m_data[DepartureMinute].toInt() );
	if ( departureTime < QTime::currentTime().addSecs(-5 * 60) )
	    m_data[ DepartureDate ] = QDate::currentDate().addDays( 1 );
	else
	    m_data[ DepartureDate ] = QDate::currentDate();
	m_data.insert( DepartureYear, m_data[DepartureDate].toDate().year() );
    }
}

JourneyInfo::JourneyInfo( const QHash< TimetableInformation, QVariant >& data )
		: PublicTransportInfo(data) {
    if ( m_data.contains(TypesOfVehicleInJourney) ) {
	QVariantList vehicleTypes;
	
	if ( m_data[TypesOfVehicleInJourney].canConvert(QVariant::StringList) ) {
	    QStringList strings = m_data[ TypesOfVehicleInJourney ].toStringList();
	    foreach ( QString str, strings ) {
		int vehicleType = static_cast<int>( getVehicleTypeFromString(str) );
		if ( !vehicleTypes.contains(vehicleType) )
		vehicleTypes << vehicleType;
	    }
	} else if ( m_data[TypesOfVehicleInJourney].canConvert(QVariant::List) ) {
	    QVariantList vars = m_data[ TypesOfVehicleInJourney ].toList();
	    foreach ( QVariant var, vars ) {
		if ( var.canConvert(QVariant::Int) ) {
		    int vehicleType = var.toInt();
		    if ( !vehicleTypes.contains(vehicleType) )
			vehicleTypes << vehicleType;
		}
	    }
	}

	m_data[ TypesOfVehicleInJourney ] = vehicleTypes;
    }
    
    if ( m_data.contains(RouteTypesOfVehicles) ) {
	QVariantList vehicleTypes;
	
	if ( m_data[RouteTypesOfVehicles].canConvert(QVariant::StringList) ) {
	    QStringList strings = m_data[ RouteTypesOfVehicles ].toStringList();
	    foreach ( QString str, strings )
		vehicleTypes << static_cast<int>( getVehicleTypeFromString(str) );
	} else if ( m_data[RouteTypesOfVehicles].canConvert(QVariant::List) ) {
	    QVariantList vars = m_data[ RouteTypesOfVehicles ].toList();
	    foreach ( QVariant var, vars ) {
		if ( var.canConvert(QVariant::Int) )
		    vehicleTypes << var.toInt();
	    }
	}
	
	m_data[ RouteTypesOfVehicles ] = vehicleTypes;
    }

    if ( m_data.contains(ArrivalDate) && (!m_data[ArrivalDate].canConvert(QVariant::Date)
		|| !m_data[ArrivalDate].toDate().isValid()) ) {
	if ( m_data[ArrivalDate].canConvert(QVariant::String) ) {
	    QString sDate = m_data[ArrivalDate].toString();
	    
	    m_data[ ArrivalDate ] = QDate::fromString( sDate, "dd.MM.yyyy" );
	    
	    if ( !m_data[ArrivalDate].toDate().isValid() ) {
		int currentYear = QDate::currentDate().year();
		QDate date = QDate::fromString( sDate, "dd.MM.yy" );
		if ( date.year() <= currentYear - 99 )
		    date.setDate( currentYear, date.month(), date.day() );
		
		m_data[ ArrivalDate ] = date;
	    }
	} else {
	    kDebug() << "Arrival date is in wrong format:" << m_data[ ArrivalDate ];
	    m_data.remove( ArrivalDate );
	}
    }

    if ( m_data.contains(ArrivalHour) && m_data.contains(ArrivalMinute)
		&& !m_data.contains(ArrivalDate) ) {
	QTime arrivalTime = QTime( m_data[ArrivalHour].toInt(),
				   m_data[ArrivalMinute].toInt() );
	if ( arrivalTime < QTime::currentTime().addSecs(-5 * 60) )
	    m_data[ ArrivalDate ] = QDate::currentDate().addDays( 1 );
	else
	    m_data[ ArrivalDate ] = QDate::currentDate();
    }

    if ( m_data.contains(Duration) && m_data[Duration].toInt() <= 0 ) {
	if ( m_data[Duration].canConvert(QVariant::String) ) {
	    QString duration = m_data[ Duration ].toString();
	    QTime timeDuration = QTime::fromString( duration, "hh:mm" );
	    if ( timeDuration.isValid() ) {
		int minsDuration = QTime(0, 0).secsTo( timeDuration ) / 60;
		m_data[Duration] = minsDuration;
	    }
	}
    }
    if ( m_data[Duration].toInt() <= 0
		&& m_data.contains(DepartureDate)
		&& m_data.contains(DepartureHour) && m_data.contains(DepartureMinute)
		&& m_data.contains(ArrivalDate)
		&& m_data.contains(ArrivalHour) && m_data.contains(ArrivalMinute) ) {
	QDateTime departure( m_data[DepartureDate].toDate(),
			     QTime(m_data[DepartureHour].toInt(),
				   m_data[DepartureMinute].toInt()) );
	QDateTime arrival( m_data[ArrivalDate].toDate(),
			   QTime(m_data[ArrivalHour].toInt(),
				 m_data[ArrivalMinute].toInt()) );
	int minsDuration = departure.secsTo( arrival ) / 60;
	m_data[Duration] = minsDuration;
    }

    if ( m_data.contains(RouteTimesDeparture) ) {
	QVariantList times;
	if ( m_data[RouteTimesDeparture].canConvert(QVariant::StringList) ) {
	    QStringList strings = m_data[ RouteTimesDeparture ].toStringList();
	    foreach ( QString str, strings )
		times << QTime::fromString( str.trimmed(), "hh:mm" );
	} else if ( m_data[RouteTimesDeparture].canConvert(QVariant::List) ) {
	    QVariantList vars = m_data[ RouteTimesDeparture ].toList();
	    foreach ( QVariant var, vars )
		times << var.toTime();
	}

	m_data[ RouteTimesDeparture ] = times;
    }
    
    if ( m_data.contains(RouteTimesArrival) ) {
	QVariantList times;
	if ( m_data[RouteTimesArrival].canConvert(QVariant::StringList) ) {
	    QStringList strings = m_data[ RouteTimesArrival ].toStringList();
	    foreach ( QString str, strings )
		times << QTime::fromString( str.trimmed(), "hh:mm" );
	} else if ( m_data[RouteTimesArrival].canConvert(QVariant::List) ) {
	    QVariantList vars = m_data[ RouteTimesArrival ].toList();
	    foreach ( QVariant var, vars )
		times << var.toTime();
	}
	
	m_data[ RouteTimesArrival ] = times;
    }
    
    m_isValid = m_data.contains(DepartureHour) && m_data.contains(DepartureMinute)
	    && m_data.contains(StartStopName) && m_data.contains(TargetStopName)
	    && m_data.contains(ArrivalHour) && m_data.contains(ArrivalMinute);
}

JourneyInfo::JourneyInfo( const QList< VehicleType >& vehicleTypes,
			  const QString& startStopName,
			  const QString& targetStopName,
			  const QDateTime& departure, const QDateTime& arrival,
			  int duration, int changes, const QString& pricing,
			  const QString &journeyNews, const QString &operatorName )
			  : PublicTransportInfo(departure, operatorName) {
    init( vehicleTypes, startStopName, targetStopName, arrival,
	  duration, changes, pricing, journeyNews );
}

void JourneyInfo::init( const QList< VehicleType >& vehicleTypes,
			const QString& startStopName,
			const QString& targetStopName,
			const QDateTime& arrival, int duration, int changes,
			const QString& pricing, const QString &journeyNews ) {
    m_isValid = changes >= 0;
    
//     m_vehicleTypes = vehicleTypes;
//     m_startStopName = startStopName;
//     m_targetStopName = targetStopName;
    QVariantList vehicleTypeList;
    foreach ( VehicleType vehicleType, vehicleTypes )
	vehicleTypeList << static_cast<int>( vehicleType );
    m_data.insert( TypesOfVehicleInJourney, vehicleTypeList );
    m_data.insert( StartStopName, startStopName );
    m_data.insert( TargetStopName, targetStopName );

//     m_arrival = arrival;
    m_data.insert( ArrivalDate, arrival.date() );
    m_data.insert( ArrivalHour, arrival.time().hour() );
    m_data.insert( ArrivalMinute, arrival.time().minute() );
    
//     m_duration = duration;
//     m_changes = changes;
//     m_pricing = pricing;
//     m_journeyNews = journeyNews;
    m_data.insert( Duration, duration );
    m_data.insert( Changes, changes );
    m_data.insert( Pricing, pricing );
    m_data.insert( JourneyNews, journeyNews );
}


DepartureInfo::DepartureInfo() : PublicTransportInfo() {
    m_isValid = false;
}

DepartureInfo::DepartureInfo( const QString& line, const VehicleType& typeOfVehicle,
			      const QString& target, const QDateTime& departure,
			      bool nightLine, bool expressLine,
			      const QString& platform, int delay,
			      const QString& delayReason, const QString& journeyNews,
			      const QString &operatorName )
			      : PublicTransportInfo(departure, operatorName) {
    init( line, typeOfVehicle, target, nightLine, expressLine,
	  platform, delay, delayReason, journeyNews );
}

DepartureInfo::DepartureInfo( const QString& line, const VehicleType& typeOfVehicle,
			      const QString& target, const QTime &requestTime,
			      const QTime& departureTime, bool nightLine,
			      bool expressLine, const QString& platform,
			      int delay, const QString& delayReason,
			      const QString& journeyNews, const QString &operatorName ) : PublicTransportInfo() {
    // Guess date
    if ( departureTime < requestTime.addSecs(-5 * 60) )
	m_data.insert( DepartureDate, QDate::currentDate().addDays(1) );
    else
	m_data.insert( DepartureDate, QDate::currentDate() );
    m_data.insert( DepartureYear, m_data[DepartureDate].toDate().year() );
    
    m_data.insert( DepartureHour, departureTime.hour() );
    m_data.insert( DepartureMinute, departureTime.minute() );
    
    m_data[ DepartureHour ] = departureTime.hour();
    m_data[ DepartureMinute ] = departureTime.minute();

    init( line, typeOfVehicle, target, nightLine, expressLine, platform, delay,
	  delayReason, journeyNews );
	  
    m_data[ Operator ] = operatorName;
}

DepartureInfo::DepartureInfo( const QHash<TimetableInformation, QVariant> &data )
	    : PublicTransportInfo( data ) {
    m_isValid = m_data.contains(TransportLine) && m_data.contains(Target)
	    && m_data.contains(DepartureHour) && m_data.contains(DepartureMinute);
}

void DepartureInfo::init( const QString& line, const VehicleType& typeOfVehicle,
			  const QString& target,
			  bool nightLine, bool expressLine, const QString& platform,
			  int delay, const QString& delayReason,
			  const QString& journeyNews ) {
    m_isValid = true;
//     m_line = line;
//     m_vehicleType = typeOfVehicle;
//     m_target = target;
    m_data.insert( TransportLine, line );
    m_data.insert( TypeOfVehicle, static_cast<int>(typeOfVehicle) );
    m_data.insert( Target, target );
    
    m_lineServices = nightLine ? NightLine : NoLineService;
    if ( expressLine )
	m_lineServices |= ExpressLine;
//     m_platform = platform;
    m_data.insert( Platform, platform );
//     m_delay = delay;
    m_data.insert( Delay, delay );
//     m_delayReason = delayReason;
    m_data.insert( DelayReason, delayReason );
//     m_journeyNews = journeyNews;
    m_data.insert( JourneyNews, journeyNews );
}

VehicleType PublicTransportInfo::getVehicleTypeFromString( const QString& sLineType ) {
//     kDebug() << sLineType;

    QString sLineTypeLower = sLineType.trimmed().toLower();

    // See http://en.wikipedia.org/wiki/Train_categories_in_Europe
    if ( sLineTypeLower == "u-bahn" ||
	sLineTypeLower == "ubahn" ||
	sLineTypeLower == "u" ||
	sLineTypeLower == "rt" ) // regio tram
	return Subway;

    else if ( sLineTypeLower == "s-bahn" ||
	sLineTypeLower == "sbahn" ||
	sLineTypeLower == "s" ||
	sLineTypeLower == "s1" || // for sbb.ch
	sLineTypeLower == "rsb" ) // "regio-s-bahn", austria
	return TrainInterurban;

    else if ( sLineTypeLower == "tram" ||
	sLineTypeLower == "straßenbahn" ||
	sLineTypeLower == "str" ||
	sLineTypeLower == "ntr" || // for sbb.ch
	sLineTypeLower == "tra" || // for sbb.ch
	sLineTypeLower == "stb" || // "stadtbahn", germany
	sLineTypeLower == "dm_train" ||
	sLineTypeLower == "streetcar (tram)" ) // for imhd.sk
	return Tram;

    else if ( sLineTypeLower == "bus" ||
	sLineTypeLower == "dm_bus" ||
	sLineTypeLower == "nbu" || // for sbb.ch
	sLineTypeLower == "bsv" || // for nasa.de
	sLineTypeLower == "express bus" || // for imhd.sk
	sLineTypeLower == "night line - bus" ) // for imhd.sk
	return Bus;

    else if ( sLineTypeLower == "metro" ||
	sLineTypeLower == "m" )
	return Metro;

    else if ( sLineTypeLower == "tro" ||
	sLineTypeLower == "trolleybus" ||
	sLineTypeLower == "trolley bus" )
	return TrolleyBus;

    else if ( sLineTypeLower == "feet" ||
	sLineTypeLower == "by feet" ||
	sLineTypeLower == "fu&#223;weg" ||
	sLineTypeLower == "fu&szlig;weg" ||
	sLineTypeLower == "fussweg" ||
	sLineTypeLower == "zu fu&#223;" ||
	sLineTypeLower == "zu fu&szlig;" ||
	sLineTypeLower == "zu fuss" ||
	sLineTypeLower == "&#220;bergang" ||
	sLineTypeLower == "uebergang" ||
	sLineTypeLower == "&uuml;bergang" )
	return Feet;

    else if ( sLineTypeLower == "ferry" ||
	sLineTypeLower == "faehre" )
	return Ferry;

    else if ( sLineTypeLower == "ship" ||
	sLineTypeLower == "schiff" )
	return Ship;

    else if ( sLineTypeLower == "rb" || // Regional, "RegionalBahn", germany
	sLineTypeLower == "me" || // "Metronom", germany
	sLineTypeLower == "mer" || // "Metronom", germany
	sLineTypeLower == "erb" || // "EuroBahn", germany
	sLineTypeLower == "wfb" || // "WestfalenBahn", germany
	sLineTypeLower == "nwb" || // "NordWestBahn", germany
	sLineTypeLower == "osb" || // "Ortenau-S-Bahn GmbH" (no interurban train), germany
	sLineTypeLower == "r" || // austria, switzerland
	sLineTypeLower == "os" || // czech, "Osobní vlak", basic local (stopping) trains, Regionalbahn
	sLineTypeLower == "dpn" || // for rozklad-pkp.pl (TODO: Check if this is the right train type)
	sLineTypeLower == "t84" || // for rozklad-pkp.pl (TODO: Check if this is the right train type)
	sLineTypeLower == "r84" ) // for rozklad-pkp.pl (TODO: Check if this is the right train type)
	return TrainRegional;

    else if ( sLineTypeLower == "re" ||
	sLineTypeLower == "rer" || // france, Reseau Express Regional
	sLineTypeLower == "sp" || // czech, "Spěšný vlak", semi-fast trains (Eilzug)
	sLineTypeLower == "rex" || // austria, local train stopping at few stations; semi fast
	sLineTypeLower == "ez" || // austria ("erlebniszug"), local train stopping at few stations; semi fast
	sLineTypeLower == "zr" ) // slovakia, "Zrýchlený vlak", train serving almost all stations en route
	return TrainRegionalExpress;

    else if ( sLineTypeLower == "ir" ||
	sLineTypeLower == "d" || // schnellzug, swiss
	sLineTypeLower == "ire" ||  // almost a local train (Nahverkehrszug) stopping at few stations; semi-fast
	sLineTypeLower == "er" ||  // border-crossing local train stopping at few stations; semi-fast
	sLineTypeLower == "ex" ) // czech, express trains with no supplementary fare, similar to the German Interregio or also Regional-Express
	return TrainInterregio;

    else if ( sLineTypeLower == "ec_ic" || // Eurocity / Intercity
	sLineTypeLower == "ec" || // Eurocity
	sLineTypeLower == "ic" || // Intercity
	sLineTypeLower == "cnl" || // CityNightLine
	sLineTypeLower == "en" || // EuroNight
	sLineTypeLower == "nz" || // "Nachtzug"
	sLineTypeLower == "oec" || // austria
	sLineTypeLower == "oic" || // austria
	sLineTypeLower == "icn" ) // national long-distance train with tilting technology
	return TrainIntercityEurocity;

    else if ( sLineTypeLower == "ice" || // germany
	sLineTypeLower == "rj" ||  // "railjet", austria
	sLineTypeLower == "tgv" ||  // france
	sLineTypeLower == "tha" ||  // thalys
	sLineTypeLower == "hst" || // great britain
	sLineTypeLower == "est" || // eurostar
	sLineTypeLower == "es" ) // eurostar, High-speed, tilting trains for long-distance services
	return TrainIntercityExpress;

    else
	return Unknown;
}
