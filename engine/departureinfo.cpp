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
#include <QDebug>

JourneyInfo::JourneyInfo ( const QList< VehicleType >& vehicleTypes, const QString& startStopName, const QString& targetStopName, const QDateTime& departure, const QDateTime& arrival, int duration, int changes, const QString& pricing, const QString &journeyNews, const QString &operatorName ) : PublicTransportInfo (departure, operatorName) {
    init( vehicleTypes, startStopName, targetStopName, departure, arrival, duration, changes, pricing, journeyNews );
}

void JourneyInfo::init( const QList< VehicleType >& vehicleTypes, const QString& startStopName, const QString& targetStopName, const QDateTime& departure, const QDateTime& arrival, int duration, int changes, const QString& pricing, const QString &journeyNews ) {
    m_vehicleTypes = vehicleTypes;
    m_startStopName = startStopName;
    m_targetStopName = targetStopName;
    m_departure = departure;
    m_arrival = arrival;
    m_duration = duration;
    m_changes = changes;
    m_pricing = pricing;
    m_journeyNews = journeyNews;
}


DepartureInfo::DepartureInfo() : PublicTransportInfo() {
    m_line = -1;
    m_delay = -1;
    m_platform = "";
    m_delayReason = "";
}

DepartureInfo::DepartureInfo( const QString& line, const VehicleType& typeOfVehicle, const QString& target, const QDateTime& departure, bool nightLine, bool expressLine, const QString& platform, int delay, const QString& delayReason, const QString& journeyNews, const QString &operatorName ) : PublicTransportInfo(departure, operatorName) {
    init( line, typeOfVehicle, target, departure, nightLine, expressLine, platform, delay, delayReason, journeyNews );
}

DepartureInfo::DepartureInfo( const QString& line, const VehicleType& typeOfVehicle, const QString& target, const QTime &requestTime, const QTime& departureTime, bool nightLine, bool expressLine, const QString& platform, int delay, const QString& delayReason, const QString& journeyNews, const QString &operatorName ) : PublicTransportInfo() {
    QDate departureDate;
    if ( departureTime < requestTime.addSecs(-5 * 60) )
	departureDate = QDate::currentDate().addDays(1);
    else
	departureDate = QDate::currentDate();

    init( line, typeOfVehicle, target, QDateTime(departureDate, departureTime), nightLine, expressLine, platform, delay, delayReason, journeyNews );
    m_operator = operatorName;
}

void DepartureInfo::init ( const QString& line, const VehicleType& typeOfVehicle, const QString& target, const QDateTime& departure, bool nightLine, bool expressLine, const QString& platform, int delay, const QString& delayReason, const QString& journeyNews ) {
    m_line = line;
    m_vehicleType = typeOfVehicle;
    m_target = target;
    m_departure = departure;
    m_lineServices = nightLine ? NightLine : NoLineService;
    if ( expressLine )
	m_lineServices |= ExpressLine;
    m_platform = platform;
    m_delay = delay;
    m_delayReason = delayReason;
    m_journeyNews = journeyNews;
}

VehicleType PublicTransportInfo::getVehicleTypeFromString( const QString& sLineType ) {
//     qDebug() << "DepartureInfo::getLineTypeFromString" << sLineType;

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

    else if ( sLineTypeLower == "ferry" )
	return Ferry;

    else if ( sLineTypeLower == "rb" ||
	sLineTypeLower == "mer" || // "metronom", geramny
	sLineTypeLower == "r" || // austria, switzerland
	sLineTypeLower == "os" || // czech, "Osobní vlak", basic local (stopping) trains, Regionalbahn
	sLineTypeLower == "dpn" || // for rozklad-pkp.pl (TODO: Check if this is the right train type)
	sLineTypeLower == "t84" || // for rozklad-pkp.pl (TODO: Check if this is the right train type)
	sLineTypeLower == "r84" ) // for rozklad-pkp.pl (TODO: Check if this is the right train type)
	return TrainRegional;

    else if ( sLineTypeLower == "re" ||
	sLineTypeLower == "sp" || // czech, "Spěšný vlak", semi-fast trains (Eilzug)
	sLineTypeLower == "rex" || // austria, local train stopping at few stations; semi fast
	sLineTypeLower == "ez" || // austria ("erlebniszug"), local train stopping at few stations; semi fast
	sLineTypeLower == "zr" ) // slovakia, "Zrýchlený vlak", train serving almost all stations en route
	return TrainRegionalExpress;

    else if ( sLineTypeLower == "ir" ||
	sLineTypeLower == "ire" ||  // almost a local train (Nahverkehrszug) stopping at few stations; semi-fast
	sLineTypeLower == "er" ||  // border-crossing local train stopping at few stations; semi-fast
	sLineTypeLower == "ex" )// czech, express trains with no supplementary fare, similar to the German Interregio or also Regional-Express
	return TrainInterregio;

    else if ( sLineTypeLower == "ec_ic" ||
	sLineTypeLower == "ec" ||
	sLineTypeLower == "ic" ||
	sLineTypeLower == "oec" || // austria
	sLineTypeLower == "oic" || // austria
	sLineTypeLower == "icn" ) // national long-distance train with tilting technology
	return TrainIntercityEurocity;

    else if ( sLineTypeLower == "ice" || // germany
	sLineTypeLower == "tgv" ||  // france
	sLineTypeLower == "tha" ||  // thalys
	sLineTypeLower == "hst" || // great britain
	sLineTypeLower == "es" ) // eurostar italia, High-speed, tilting trains for long-distance services
// 	sLineTypeLower == "" ) // spain, High speed trains, speeds up to 300 km/h... missing value
	return TrainIntercityExpress;

    else
	return Unknown;
}
