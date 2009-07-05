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


DepartureInfo::DepartureInfo() {
    m_line = -1;
    m_delay = -1;
    m_platform = "";
    m_delayReason = "";
}

DepartureInfo::DepartureInfo( const QString& line, const VehicleType& typeOfVehicle, const QString& target, const QDateTime& departure, bool nightLine, bool expressLine, const QString& platform, int delay, const QString& delayReason, const QString& sJourneyNews ) {
    init( line, typeOfVehicle, target, departure, nightLine, expressLine, platform, delay, delayReason, sJourneyNews );
}

DepartureInfo::DepartureInfo( const QString& line, const VehicleType& typeOfVehicle, const QString& target, const QTime &requestTime, const QTime& departureTime, bool nightLine, bool expressLine, const QString& platform, int delay, const QString& delayReason, const QString& sJourneyNews ) {
    QDate departureDate;
    if ( departureTime < requestTime.addSecs(-5 * 60) )
	departureDate = QDate::currentDate().addDays(1);
    else
	departureDate = QDate::currentDate();

    init( line, typeOfVehicle, target, QDateTime(departureDate, departureTime), nightLine, expressLine, platform, delay, delayReason, sJourneyNews );
}

void DepartureInfo::init ( const QString& line, const VehicleType& typeOfVehicle, const QString& target, const QDateTime& departure, bool nightLine, bool expressLine, const QString& platform, int delay, const QString& delayReason, const QString& sJourneyNews ) {
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
    m_journeyNews = sJourneyNews;
}

VehicleType DepartureInfo::getVehicleTypeFromString( const QString& sLineType ) {
//     qDebug() << "DepartureInfo::getLineTypeFromString" << sLineType;

    QString sLineTypeLower = sLineType.trimmed().toLower();
    if ( sLineTypeLower == "u-bahn" ||
	sLineTypeLower == "ubahn" ||
	sLineTypeLower == "u" )
	return Subway;

    else if ( sLineTypeLower == "s-bahn" ||
	sLineTypeLower == "sbahn" ||
	sLineTypeLower == "s" )
	return TrainInterurban;

    else if ( sLineTypeLower == "tram" ||
	sLineTypeLower == "straßenbahn" ||
	sLineTypeLower == "str" ||
	sLineTypeLower == "dm_train" ||
	sLineTypeLower == "streetcar (tram)" ) // for imhd.sk
	return Tram;

    else if ( sLineTypeLower == "bus" ||
	sLineTypeLower == "dm_bus" ||
	sLineTypeLower == "bsv" || // for nasa.de
	sLineTypeLower == "express bus" || // for imhd.sk
	sLineTypeLower == "night line - bus" ) // for imhd.sk
	return Bus;

    else if ( sLineTypeLower == "rb" ||
	sLineTypeLower == "dpn" || // for rozklad-pkp.pl (TODO: Check if this is the right train type)
	sLineTypeLower == "t84" || // for rozklad-pkp.pl (TODO: Check if this is the right train type)
	sLineTypeLower == "r84" ) // for rozklad-pkp.pl (TODO: Check if this is the right train type)
	return TrainRegional;

    else if ( sLineTypeLower == "re" )
	return TrainRegionalExpress;

    else if ( sLineTypeLower == "ir" )
	return TrainInterregio;

    else if ( sLineTypeLower == "ec_ic" ||
	sLineTypeLower == "ec" ||
	sLineTypeLower == "ic" )
	return TrainIntercityEurocity;

    else if ( sLineTypeLower == "ice" )
	return TrainIntercityExpress;

    else
	return Unknown;
}
