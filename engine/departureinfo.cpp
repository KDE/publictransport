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

#include "departureinfo.h"
#include <KDebug>

PublicTransportInfo::PublicTransportInfo( const QHash< TimetableInformation, QVariant >& data )
{
	m_data = data;
	m_isValid = false;

	if ( !m_data.contains( Delay ) ) {
		m_data.insert( Delay, -1 );
	}

	if ( m_data.contains( RouteTimes ) ) {
		QVariantList times;
		if ( m_data[RouteTimes].canConvert( QVariant::StringList ) ) {
			QStringList strings = m_data[ RouteTimes ].toStringList();
			foreach( const QString &str, strings ) {
				times << QTime::fromString( str.trimmed(), "hh:mm" );
			}
		} else if ( m_data[RouteTimes].canConvert( QVariant::List ) ) {
			QVariantList vars = m_data[ RouteTimes ].toList();
			foreach( const QVariant &var, vars ) {
				times << var.toTime();
			}
		}

		m_data[ RouteTimes ] = times;
	}

	if ( m_data.contains( TypeOfVehicle ) && m_data[TypeOfVehicle].canConvert( QVariant::String ) ) {
		QString sVehicleType = m_data[TypeOfVehicle].toString();
		m_data[ TypeOfVehicle ] = getVehicleTypeFromString( sVehicleType );

		if ( !m_data.contains( Operator )
				|| ( m_data[Operator].canConvert( QVariant::String )
					 && m_data[Operator].toString().isEmpty() ) ) {
			QString sOperator = operatorFromVehicleTypeString( sVehicleType );
			if ( !sOperator.isNull() ) {
				m_data[ Operator ] = sOperator;
			}
		}
	}


	if ( m_data.contains( TransportLine ) ) {
		QString sVehicleType, sTransportLine = m_data[TransportLine].toString();

		if ( !m_data.contains( Operator )
				|| ( m_data[Operator].canConvert( QVariant::String )
					 && m_data[Operator].toString().isEmpty() ) ) {
			QRegExp rx( "^[abcdefghijklmnopqrstuvwxyz]+", Qt::CaseInsensitive );
			if ( rx.indexIn( sTransportLine ) != -1 ) {
				QString sOperator = operatorFromVehicleTypeString( rx.cap() );
				if ( !sOperator.isNull() ) {
					m_data[ Operator ] = sOperator;
				}
			}
		}

		QRegExp rx = QRegExp( "^(bus|tro|tram|str|s|u|m)\\s*", Qt::CaseInsensitive );
		rx.indexIn( sTransportLine );
		sVehicleType = rx.cap();
		if ( !m_data.contains( TypeOfVehicle ) ) {
			m_data.insert( TypeOfVehicle, sVehicleType );
		} else if ( m_data.value( TypeOfVehicle ) == Unknown ) {
			m_data[TypeOfVehicle] = sVehicleType;
		}

		sTransportLine = sTransportLine.trimmed();
		sTransportLine = sTransportLine.remove( QRegExp( "^(bus|tro|tram|str)",
		                                        Qt::CaseInsensitive ) );
		sTransportLine = sTransportLine.replace( QRegExp( "\\s{2,}" ), " " );

		if ( sTransportLine.contains( "F&#228;hre" ) ) {
			m_data.insert( TypeOfVehicle, Ferry );
		}

		m_data.insert( TransportLine, sTransportLine.trimmed() );
	}

	if ( m_data.contains( DepartureDate ) && ( !m_data[DepartureDate].canConvert( QVariant::Date )
				|| !m_data[DepartureDate].toDate().isValid() ) ) {
		if ( m_data[DepartureDate].canConvert( QVariant::String ) ) {
			QString sDate = m_data[DepartureDate].toString();
			m_data[ DepartureDate ] = QDate::fromString( sDate, "dd.MM.yyyy" );

			if ( !m_data[DepartureDate].toDate().isValid() ) {
				int currentYear = QDate::currentDate().year();
				QDate date = QDate::fromString( sDate, "dd.MM.yy" );
				if ( date.year() <= currentYear - 99 ) {
					date.setDate( currentYear, date.month(), date.day() );
				}

				m_data[ DepartureDate ] = date;
			}
		} else {
			kDebug() << "Departure date is in wrong format:" << m_data[ DepartureDate ];
			m_data.remove( DepartureDate );
		}
	}

	if ( m_data.contains( DepartureHour ) && m_data.contains( DepartureMinute )
				&& !m_data.contains( DepartureDate ) ) {
		QTime departureTime = QTime( m_data[DepartureHour].toInt(),
		                             m_data[DepartureMinute].toInt() );
		if ( departureTime < QTime::currentTime().addSecs( -5 * 60 ) ) {
			// This could produce wrong dates (better give DepartureDate in scripts)
			kDebug() << "Guessed DepartureDate as tomorrow";
			m_data[ DepartureDate ] = QDate::currentDate().addDays( 1 );
		} else {
			kDebug() << "Guessed DepartureDate as today";
			m_data[ DepartureDate ] = QDate::currentDate();
		}
		m_data.insert( DepartureYear, m_data[DepartureDate].toDate().year() );
	}
}

JourneyInfo::JourneyInfo( const QHash< TimetableInformation, QVariant >& data )
		: PublicTransportInfo( data )
{
	if ( m_data.contains( TypesOfVehicleInJourney ) ) {
		QVariantList vehicleTypes;

		if ( m_data[TypesOfVehicleInJourney].canConvert( QVariant::StringList ) ) {
			QStringList strings = m_data[ TypesOfVehicleInJourney ].toStringList();
			foreach( const QString &str, strings ) {
				int vehicleType = static_cast<int>( getVehicleTypeFromString( str ) );
				if ( !vehicleTypes.contains( vehicleType ) ) {
					vehicleTypes << vehicleType;
				}
			}
		} else if ( m_data[TypesOfVehicleInJourney].canConvert( QVariant::List ) ) {
			QVariantList vars = m_data[ TypesOfVehicleInJourney ].toList();
			foreach( const QVariant &var, vars ) {
				if ( var.canConvert( QVariant::Int ) ) {
					int vehicleType = var.toInt();
					if ( !vehicleTypes.contains( vehicleType ) ) {
						vehicleTypes << vehicleType;
					}
				}
			}
		}

		m_data[ TypesOfVehicleInJourney ] = vehicleTypes;
	}

	if ( m_data.contains( RouteTypesOfVehicles ) ) {
		QVariantList vehicleTypes;

		if ( m_data[RouteTypesOfVehicles].canConvert( QVariant::StringList ) ) {
			QStringList strings = m_data[ RouteTypesOfVehicles ].toStringList();
			foreach( const QString &str, strings ) {
				vehicleTypes << static_cast<int>( getVehicleTypeFromString( str ) );
			}
		} else if ( m_data[RouteTypesOfVehicles].canConvert( QVariant::List ) ) {
			QVariantList vars = m_data[ RouteTypesOfVehicles ].toList();
			foreach( const QVariant &var, vars ) {
				if ( var.canConvert( QVariant::Int ) ) {
					vehicleTypes << var.toInt();
				}
			}
		}

		m_data[ RouteTypesOfVehicles ] = vehicleTypes;
	}

	if ( m_data.contains( ArrivalDate ) && ( !m_data[ArrivalDate].canConvert( QVariant::Date )
				|| !m_data[ArrivalDate].toDate().isValid() ) ) {
		if ( m_data[ArrivalDate].canConvert( QVariant::String ) ) {
			QString sDate = m_data[ArrivalDate].toString();

			m_data[ ArrivalDate ] = QDate::fromString( sDate, "dd.MM.yyyy" );

			if ( !m_data[ArrivalDate].toDate().isValid() ) {
				int currentYear = QDate::currentDate().year();
				QDate date = QDate::fromString( sDate, "dd.MM.yy" );
				if ( date.year() <= currentYear - 99 ) {
					date.setDate( currentYear, date.month(), date.day() );
				}

				m_data[ ArrivalDate ] = date;
			}
		} else {
			kDebug() << "Arrival date is in wrong format:" << m_data[ ArrivalDate ];
			m_data.remove( ArrivalDate );
		}
	}

	if ( m_data.contains( ArrivalHour ) && m_data.contains( ArrivalMinute )
			&& !m_data.contains( ArrivalDate ) ) {
		QTime arrivalTime = QTime( m_data[ArrivalHour].toInt(),
								   m_data[ArrivalMinute].toInt() );
		if ( arrivalTime < QTime::currentTime().addSecs( -5 * 60 ) ) {
			m_data[ ArrivalDate ] = QDate::currentDate().addDays( 1 );
		} else {
			m_data[ ArrivalDate ] = QDate::currentDate();
		}
	}

	if ( m_data.contains( Duration ) && m_data[Duration].toInt() <= 0 ) {
		if ( m_data[Duration].canConvert( QVariant::String ) ) {
			QString duration = m_data[ Duration ].toString();
			QTime timeDuration = QTime::fromString( duration, "hh:mm" );
			if ( timeDuration.isValid() ) {
				int minsDuration = QTime( 0, 0 ).secsTo( timeDuration ) / 60;
				m_data[Duration] = minsDuration;
			}
		}
	}
	if ( m_data[Duration].toInt() <= 0
			&& m_data.contains( DepartureDate )
			&& m_data.contains( DepartureHour ) && m_data.contains( DepartureMinute )
			&& m_data.contains( ArrivalDate )
			&& m_data.contains( ArrivalHour ) && m_data.contains( ArrivalMinute ) ) {
		QDateTime departure( m_data[DepartureDate].toDate(),
							 QTime( m_data[DepartureHour].toInt(),
									m_data[DepartureMinute].toInt() ) );
		QDateTime arrival( m_data[ArrivalDate].toDate(),
						   QTime( m_data[ArrivalHour].toInt(),
								  m_data[ArrivalMinute].toInt() ) );
		int minsDuration = departure.secsTo( arrival ) / 60;
		m_data[Duration] = minsDuration;
	}

	if ( m_data.contains( RouteTimesDeparture ) ) {
		QVariantList times;
		if ( m_data[RouteTimesDeparture].canConvert( QVariant::StringList ) ) {
			QStringList strings = m_data[ RouteTimesDeparture ].toStringList();
			foreach( const QString &str, strings ) {
				times << QTime::fromString( str.trimmed(), "hh:mm" );
			}
		} else if ( m_data[RouteTimesDeparture].canConvert( QVariant::List ) ) {
			QVariantList vars = m_data[ RouteTimesDeparture ].toList();
			foreach( const QVariant &var, vars ) {
				times << var.toTime();
			}
		}

		m_data[ RouteTimesDeparture ] = times;
	}

	if ( m_data.contains( RouteTimesArrival ) ) {
		QVariantList times;
		if ( m_data[RouteTimesArrival].canConvert( QVariant::StringList ) ) {
			QStringList strings = m_data[ RouteTimesArrival ].toStringList();
			foreach( const QString &str, strings ) {
				times << QTime::fromString( str.trimmed(), "hh:mm" );
			}
		} else if ( m_data[RouteTimesArrival].canConvert( QVariant::List ) ) {
			QVariantList vars = m_data[ RouteTimesArrival ].toList();
			foreach( const QVariant &var, vars ) {
				times << var.toTime();
			}
		}

		m_data[ RouteTimesArrival ] = times;
	}

	m_isValid = m_data.contains( DepartureHour ) && m_data.contains( DepartureMinute )
				&& m_data.contains( StartStopName ) && m_data.contains( TargetStopName )
				&& m_data.contains( ArrivalHour ) && m_data.contains( ArrivalMinute );
}

StopInfo::StopInfo()
{
	m_isValid = false;
}

StopInfo::StopInfo(const QHash< TimetableInformation, QVariant >& data)
{
	m_data = data;
	m_isValid = m_data.contains( StopName );
}

StopInfo::StopInfo(const QString& name, const QString& id, int weight, const QString& city, 
				   const QString& countryCode)
{
	m_data[StopName] = name;
	if ( !id.isNull() ) {
		m_data[StopID] = id;
	}
	if ( !city.isNull() ) {
		m_data[StopCity] = city;
	}
	if ( !countryCode.isNull() ) {
		m_data[StopCountryCode] = countryCode;
	}
	if ( weight != -1 ) {
		m_data[StopWeight] = weight;
	}

	m_isValid = !name.isEmpty();
}


JourneyInfo::JourneyInfo( const QList< VehicleType >& vehicleTypes,
                          const QString& startStopName,
                          const QString& targetStopName,
                          const QDateTime& departure, const QDateTime& arrival,
                          int duration, int changes, const QString& pricing,
                          const QString &journeyNews, const QString &operatorName )
		: PublicTransportInfo( departure, operatorName )
{
	init( vehicleTypes, startStopName, targetStopName, arrival,
		  duration, changes, pricing, journeyNews );
}

void JourneyInfo::init( const QList< VehicleType >& vehicleTypes,
                        const QString& startStopName,
                        const QString& targetStopName,
                        const QDateTime& arrival, int duration, int changes,
                        const QString& pricing, const QString &journeyNews )
{
	m_isValid = changes >= 0;

	QVariantList vehicleTypeList;
	foreach( VehicleType vehicleType, vehicleTypes ) {
		vehicleTypeList << static_cast<int>( vehicleType );
	}
	m_data.insert( TypesOfVehicleInJourney, vehicleTypeList );
	m_data.insert( StartStopName, startStopName );
	m_data.insert( TargetStopName, targetStopName );
	m_data.insert( ArrivalDate, arrival.date() );
	m_data.insert( ArrivalHour, arrival.time().hour() );
	m_data.insert( ArrivalMinute, arrival.time().minute() );
	m_data.insert( Duration, duration );
	m_data.insert( Changes, changes );
	m_data.insert( Pricing, pricing );
	m_data.insert( JourneyNews, journeyNews );
}


DepartureInfo::DepartureInfo() : PublicTransportInfo()
{
	m_isValid = false;
}

DepartureInfo::DepartureInfo( const QString& line, const VehicleType& typeOfVehicle,
                              const QString& target, const QDateTime& departure,
                              bool nightLine, bool expressLine,
                              const QString& platform, int delay,
                              const QString& delayReason, const QString& journeyNews,
                              const QString &operatorName, const QString &status )
		: PublicTransportInfo( departure, operatorName )
{
	init( line, typeOfVehicle, target, nightLine, expressLine,
		  platform, delay, delayReason, journeyNews, status );
}

DepartureInfo::DepartureInfo( const QString& line, const VehicleType& typeOfVehicle,
                              const QString& target, const QTime &requestTime,
                              const QTime& departureTime, bool nightLine,
                              bool expressLine, const QString& platform,
                              int delay, const QString& delayReason,
                              const QString& journeyNews, const QString &operatorName,
							  const QString &status )
		: PublicTransportInfo()
{
	// Guess date TODO
	Q_UNUSED( requestTime );
	// Interprete as tomorrow, if the time is more than 12 hours ago/in the future.
// 	if ( departureTime < requestTime.addSecs(-12 * 60 * 60) ) {
// 		kDebug() << "Guessed DepartureDate as tomorrow, departure is at" 
// 				 << departureTime << "requested was" << requestTime;
// 		m_data.insert( DepartureDate, QDate::currentDate().addDays(1) );
// 	} else {
		kDebug() << "Guessed DepartureDate as today";
		m_data.insert( DepartureDate, QDate::currentDate() );
// 	}
	m_data.insert( DepartureYear, m_data[DepartureDate].toDate().year() );

	m_data.insert( DepartureHour, departureTime.hour() );
	m_data.insert( DepartureMinute, departureTime.minute() );

	m_data[ DepartureHour ] = departureTime.hour();
	m_data[ DepartureMinute ] = departureTime.minute();

	init( line, typeOfVehicle, target, nightLine, expressLine, platform, delay,
		  delayReason, journeyNews );

	m_data[ Operator ] = operatorName;
	m_data[ Status ] = status;
}

DepartureInfo::DepartureInfo( const QHash<TimetableInformation, QVariant> &data )
		: PublicTransportInfo( data )
{
	m_isValid = m_data.contains( TransportLine ) && m_data.contains( Target )
				&& m_data.contains( DepartureHour ) && m_data.contains( DepartureMinute );
}

void DepartureInfo::init( const QString& line, const VehicleType& typeOfVehicle,
                          const QString& target,
                          bool nightLine, bool expressLine, const QString& platform,
                          int delay, const QString& delayReason,
                          const QString& journeyNews, const QString &status )
{
	m_isValid = true;
	m_data.insert( TransportLine, line );
	m_data.insert( TypeOfVehicle, static_cast<int>(typeOfVehicle) );
	m_data.insert( Target, target );

	m_lineServices = nightLine ? NightLine : NoLineService;
	if ( expressLine ) {
		m_lineServices |= ExpressLine;
	}
	m_data.insert( Platform, platform );
	m_data.insert( Delay, delay );
	m_data.insert( DelayReason, delayReason );
	m_data.insert( JourneyNews, journeyNews );
	m_data.insert( Status, status );
}

VehicleType PublicTransportInfo::getVehicleTypeFromString( const QString& sLineType )
{
	QString sLineTypeLower = sLineType.trimmed().toLower();

	// See http://en.wikipedia.org/wiki/Train_categories_in_Europe
	if ( sLineTypeLower == "u-bahn" ||
			sLineTypeLower == "ubahn" ||
			sLineTypeLower == "u" ||
			sLineTypeLower == "subway" ||
			sLineTypeLower == "rt" ||
			sLineTypeLower.toInt() == static_cast<int>(Subway) ) { // regio tram
		return Subway;
	} else if ( sLineTypeLower == "s-bahn" ||
			sLineTypeLower == "sbahn" ||
			sLineTypeLower == "s_bahn" ||
			sLineTypeLower == "s" ||
			sLineTypeLower == "s1" || // ch_sbb
			sLineTypeLower == "rsb" ||
			sLineTypeLower.toInt() == static_cast<int>(InterurbanTrain) ) { // "regio-s-bahn", au_oebb
		return InterurbanTrain;
	} else if ( sLineTypeLower == "tram" ||
			sLineTypeLower == "straßenbahn" ||
			sLineTypeLower == "str" ||
			sLineTypeLower == "ntr" || // for ch_sbb
			sLineTypeLower == "tra" || // for ch_sbb
			sLineTypeLower == "stb" || // "stadtbahn", germany
			sLineTypeLower == "dm_train" ||
			sLineTypeLower == "streetcar (tram)" ||
			sLineTypeLower.toInt() == static_cast<int>(Tram) ) { // for sk_imhd
		return Tram;
	} else if ( sLineTypeLower == "bus" ||
			sLineTypeLower == "dm_bus" ||
			sLineTypeLower == "au" || // it_cup2000, "autobus"
			sLineTypeLower == "nbu" || // for ch_sbb
			sLineTypeLower == "bsv" || // for de_nasa
			sLineTypeLower == "express bus" || // for sk_imhd
			sLineTypeLower == "night line - bus" ||
			sLineTypeLower.toInt() == static_cast<int>(Bus) ) { // for sk_imhd
		return Bus;
	} else if ( sLineTypeLower == "metro" ||
			sLineTypeLower == "m" ||
			sLineTypeLower.toInt() == static_cast<int>(Metro) ) {
		return Metro;
	} else if ( sLineTypeLower == "tro" ||
			sLineTypeLower == "trolleybus" ||
			sLineTypeLower == "trolley bus" ||
			sLineTypeLower.startsWith(QLatin1String("trolleybus")) ||
			sLineTypeLower.toInt() == static_cast<int>(TrolleyBus) ) { // for sk_imhd
		return TrolleyBus;
	} else if ( sLineTypeLower == "feet" ||
			sLineTypeLower == "by feet" ||
			sLineTypeLower == "fu&#223;weg" ||
			sLineTypeLower == "fu&szlig;weg" ||
			sLineTypeLower == "fussweg" ||
			sLineTypeLower == "zu fu&#223;" ||
			sLineTypeLower == "zu fu&szlig;" ||
			sLineTypeLower == "zu fuss" ||
			sLineTypeLower == "&#220;bergang" ||
			sLineTypeLower == "uebergang" ||
			sLineTypeLower == "&uuml;bergang" ||
			sLineTypeLower.toInt() == static_cast<int>(Feet) ) {
		return Feet;
	} else if ( sLineTypeLower == "ferry" ||
			sLineTypeLower == "faehre" ||
			sLineTypeLower.toInt() == static_cast<int>(Ferry) ) {
		return Ferry;
	} else if ( sLineTypeLower == "ship" ||
			sLineTypeLower == "boat" ||
			sLineTypeLower == "schiff" ||
			sLineTypeLower.toInt() == static_cast<int>(Ship) ) {
		return Ship;
	} else if ( sLineTypeLower == "plane" ||
			sLineTypeLower == "air" ||
			sLineTypeLower == "aeroplane" ||
			sLineTypeLower.toInt() == static_cast<int>(Plane) ) {
		return Plane;
	} else if ( sLineTypeLower == "rb" || // Regional, "RegionalBahn", germany
			sLineTypeLower == "me" || // "Metronom", germany
			sLineTypeLower == "mer" || // "Metronom", germany
			sLineTypeLower == "mr" || // "Märkische Regiobahn", germany
			sLineTypeLower == "erb" || // "EuroBahn", germany
			sLineTypeLower == "wfb" || // "WestfalenBahn", germany
			sLineTypeLower == "nwb" || // "NordWestBahn", germany
			sLineTypeLower == "osb" || // "Ortenau-S-Bahn GmbH" (no interurban train), germany
			sLineTypeLower == "regional" ||
			sLineTypeLower == "r" || // austria, switzerland
			sLineTypeLower == "os" || // czech, "Osobní vlak", basic local (stopping) trains, Regionalbahn
			sLineTypeLower == "dpn" || // for rozklad-pkp.pl (TODO: Check if this is the right train type)
			sLineTypeLower == "t84" || // for rozklad-pkp.pl (TODO: Check if this is the right train type)
			sLineTypeLower == "r84" || // for rozklad-pkp.pl (TODO: Check if this is the right train type)
			sLineTypeLower.toInt() == static_cast<int>(RegionalTrain) ) {
		return RegionalTrain;
	} else if ( sLineTypeLower == "re" || // RegionalExpress
			sLineTypeLower == "rer" || // france, Reseau Express Regional
			sLineTypeLower == "sp" || // czech, "Spěšný vlak", semi-fast trains (Eilzug)
			sLineTypeLower == "rex" || // austria, local train stopping at few stations; semi fast
			sLineTypeLower == "ez" || // austria ("erlebniszug"), local train stopping at few stations; semi fast
			sLineTypeLower == "zr" || // slovakia, "Zrýchlený vlak", train serving almost all stations en route fast
			sLineTypeLower == "regional express" ||
			sLineTypeLower == "regional express trains" || // used by gares-en-mouvement.com (france)
			sLineTypeLower.toInt() == static_cast<int>(RegionalExpressTrain) ) {
		return RegionalExpressTrain;
	} else if ( sLineTypeLower == "ir" || // InterRegio
			sLineTypeLower == "d" || // schnellzug, swiss
			sLineTypeLower == "ire" ||  // almost a local train (Nahverkehrszug) stopping at few stations; semi-fast
			sLineTypeLower == "er" ||  // border-crossing local train stopping at few stations; semi-fast
			sLineTypeLower == "ex" || // czech, express trains with no supplementary fare, similar to the German Interregio or also Regional-Express
			sLineTypeLower == "express" ||
			sLineTypeLower == "intercity and regional train" || // used by gares-en-mouvement.com (france)
			sLineTypeLower.toInt() == static_cast<int>(InterregionalTrain) ) {
		return InterregionalTrain;
	} else if ( sLineTypeLower == "ec_ic" || // Eurocity / Intercity
			sLineTypeLower == "ec" || // Eurocity
			sLineTypeLower == "ic" || // Intercity
			sLineTypeLower == "intercity" || // Intercity
			sLineTypeLower == "eurocity" || // Intercity
			sLineTypeLower == "cnl" || // CityNightLine
			sLineTypeLower == "en" || // EuroNight
			sLineTypeLower == "nz" || // "Nachtzug"
			sLineTypeLower == "oec" || // austria
			sLineTypeLower == "oic" || // austria
			sLineTypeLower == "icn" || // national long-distance train with tilting technology
			sLineTypeLower.toInt() == static_cast<int>(IntercityTrain) ) { 
		return IntercityTrain;
	} else if ( sLineTypeLower == "ice" || // germany
			sLineTypeLower == "rj" ||  // "railjet", austria
			sLineTypeLower == "tgv" ||  // france
			sLineTypeLower == "tha" ||  // thalys
			sLineTypeLower == "hst" || // great britain
			sLineTypeLower == "est" || // eurostar
			sLineTypeLower == "es" || // eurostar, High-speed, tilting trains for long-distance services
			sLineTypeLower == "high-speed train" || // used by gares-en-mouvement.com (france)
			sLineTypeLower.toInt() == static_cast<int>(HighSpeedTrain) ) { 
		return HighSpeedTrain;
	} else {
		return Unknown;
	}
}

QString PublicTransportInfo::operatorFromVehicleTypeString( const QString& sLineType )
{
	QString sLineTypeLower = sLineType.trimmed().toLower();

	if ( sLineTypeLower == "me" ) {
		return "metronom Eisenbahngesellschaft mbH";
	} else if ( sLineTypeLower == "mer" ) {
		return "metronom regional";
	} else if ( sLineTypeLower == "arr" ) {
		return "Arriva";
	} else if ( sLineTypeLower == "abg" ) {
		return "Anhaltische Bahn Gesellschaft mbH";
	} else if ( sLineTypeLower == "abr" ) {
		return "ABELLIO Rail NRW GmbH";
	} else if ( sLineTypeLower == "akn" ) {
		return "AKN Eisenbahn AG";
	} else if ( sLineTypeLower == "alx" ) {
		return "alex (Vogtlandbahn GmbH)";
	} else if ( sLineTypeLower == "bsb" ) {
		return "Breisgau-S-Bahn GmbH";
	} else if ( sLineTypeLower == "byb" ) {
		return "BayernBahn GmbH";
	} else if ( sLineTypeLower == "cb" ) {
		return "City Bahn Chemnitz GmbH";
	} else if ( sLineTypeLower == "cx" ) {
		return "Connex";
	} else if ( sLineTypeLower == "dab" ) {
		return "Daadetalbahn, Züge der Westerwaldbahn GmbH";
	} else if ( sLineTypeLower == "eb" ) {
		return "Erfurter Bahn GmbH";
	} else if ( sLineTypeLower == "erb" ) {
		return "eurobahn Rhenus Keolis GmbH & Co. KG";
	} else if ( sLineTypeLower == "evb" ) {
		return "Eisenbahnen und Verkehrsbetriebe Elbe-Weser GmbH";
	} else if ( sLineTypeLower == "feg" ) {
		return "Freiberger Eisenbahngesellschaft mbH";
	} else if ( sLineTypeLower == "hex" ) {
		return "HarzElbeExpress";
	} else if ( sLineTypeLower == "hlb" ) {
		return "Hessische Landesbahn GmbH, HLB Basis AG, HLB Hessenbahn GmbH";
	} else if ( sLineTypeLower == "hsb" ) {
		return "Harzer Schmalspurbahnen GmbH";
	} else if ( sLineTypeLower == "htb" ) {
		return "HellertalBahn GmbH";
	} else if ( sLineTypeLower == "hzl" ) {
		return "Hohenzollerische Landesbahn AG";
	} else if ( sLineTypeLower == "lb" ) {
		return "Lausitzbahn";
	} else if ( sLineTypeLower == "lx" ) {
		return "Lausitz-Express";
	} else if ( sLineTypeLower == "mbb" ) {
		return "Mecklenburgische Bäderbahn „Molli“ GmbH";
	} else if ( sLineTypeLower == "mel" ) {
		return "Museums-Eisenbahn-Club Losheim";
	} else if ( sLineTypeLower == "mr" ) {
		return "Märkische Regiobahn";
	} else if ( sLineTypeLower == "mrb" ) {
		return "Mitteldeutsche Regiobahn";
	} else if ( sLineTypeLower == "msb" ) {
		return "Mainschleifenbahn";
	} else if ( sLineTypeLower == "nbe" ) {
		return "nordbahn Eisenbahngesellschaft mbH & Co KG";
	} else if ( sLineTypeLower == "neb" ) {
		return "NEB Betriebsgesellschaft mbH";
	} else if ( sLineTypeLower == "neg" ) {
		return "Norddeutsche Eisenbahn Gesellschaft Niebüll GmbH";
	} else if ( sLineTypeLower == "nob" ) {
		return "Nord-Ostsee-Bahn GmbH";
	} else if ( sLineTypeLower == "nwb" ) {
		return "NordWestBahn";
	} else if ( sLineTypeLower == "oe" ) {
		return "Ostdeutsche Eisenbahn GmbH";
	} else if ( sLineTypeLower == "ola" ) {
		return "Ostseeland Verkehr GmbH";
	} else if ( sLineTypeLower == "osb" ) {
		return "Ortenau-S-Bahn GmbH";
	} else if ( sLineTypeLower == "pre" ) {
		return "Eisenbahn-Bau- und Betriebsgesellschaft Pressnitztalbahn mbH";
	} else if ( sLineTypeLower == "peg" ) {
		return "Prignitzer Eisenbahn GmbH";
	} else if ( sLineTypeLower == "rnv" ) {
		return "Rhein-Neckar-Verkehr GmbH";
	} else if ( sLineTypeLower == "rt" ) {
		return "RegioTram KVG Kasseler Verkehrsgesellschaft mbH";
	} else if ( sLineTypeLower == "rtb" ) {
		return "Rurtalbahn GmbH";
	} else if ( sLineTypeLower == "sbb" ) {
		return "SBB GmbH";
	} else if ( sLineTypeLower == "sbe" ) {
		return "Sächsisch-Böhmische Eisenbahngesellschaft mbH";
	} else if ( sLineTypeLower == "sdg" ) {
		return "Sächsische Dampfeisenbahngesellschaft mbH";
	} else if ( sLineTypeLower == "shb" ) {
		return "Schleswig-Holstein-Bahn GmbH";
	} else if ( sLineTypeLower == "soe" ) {
		return "Sächsisch-Oberlausitzer Eisenbahngesellschaft mbH";
	} else if ( sLineTypeLower == "ssb" ) {
		return "Elektrische Bahnen der Stadt Bonn und des Rhein-Sieg-Kreises";
	} else if ( sLineTypeLower == "swb" ) {
		return "Stadtwerke Bonn Verkehrs-GmbH";
	} else if ( sLineTypeLower == "swe" ) {
		return "Südwestdeutsche Verkehrs-AG";
	} else if ( sLineTypeLower == "ubb" ) {
		return "Usedomer Bäderbahn GmbH";
	} else if ( sLineTypeLower == "vbg" ) {
		return "Vogtlandbahn GmbH";
	} else if ( sLineTypeLower == "vec" ) {
		return "vectus Verkehrsgesellschaft mbH";
	} else if ( sLineTypeLower == "via" ) {
		return "VIAS GmbH, Frankfurt/Main";
	} else if ( sLineTypeLower == "vx" ) {
		return "Vogtland-Express, Express-Zug der Vogtlandbahn-GmbH";
	} else if ( sLineTypeLower == "weg" ) {
		return "Württembergische Eisenbahn-Gesellschaft mbH";
	} else if ( sLineTypeLower == "wfb" ) {
		return "WestfalenBahn";
	} else if ( sLineTypeLower == "x" ) {
		return "InterConnex";
	} else if ( sLineTypeLower == "can" ) {
		return "cantus Verkehrsgesellschaft mbH";
	} else if ( sLineTypeLower == "erb" ) {
		return "EuroBahn";
	} else {
		return QString();
	}
}
