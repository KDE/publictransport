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

#include "enums.h"

#include <KLocale>

QString Global::vehicleTypeToString( const VehicleType& vehicleType, bool plural )
{
    switch ( vehicleType ) {
    case Tram:
        return plural ? i18nc( "@info/plain", "trams" )
               : i18nc( "@info/plain", "tram" );
    case Bus:
        return plural ? i18nc( "@info/plain", "buses" )
               : i18nc( "@info/plain", "bus" );
    case Subway:
        return plural ? i18nc( "@info/plain", "subways" )
               : i18nc( "@info/plain", "subway" );
    case TrainInterurban:
        return plural ? i18nc( "@info/plain", "interurban trains" )
               : i18nc( "@info/plain", "interurban train" );
    case Metro:
        return plural ? i18nc( "@info/plain", "metros" )
               : i18nc( "@info/plain", "metro" );
    case TrolleyBus:
        return plural ? i18nc( "@info/plain", "trolley buses" )
               : i18nc( "@info/plain", "trolley bus" );

//     case TrainRegional: // DEPRECATED
    case RegionalTrain:
        return plural ? i18nc( "@info/plain", "regional trains" )
               : i18nc( "@info/plain", "regional train" );
//     case TrainRegionalExpress: // DEPRECATED
    case RegionalExpressTrain:
        return plural ? i18nc( "@info/plain", "regional express trains" )
               : i18nc( "@info/plain", "regional express train" );
//     case TrainInterregio: // DEPRECATED
    case InterregionalTrain:
        return plural ? i18nc( "@info/plain", "interregional trains" )
               : i18nc( "@info/plain", "interregional train" );
//     case TrainIntercityEurocity: // DEPRECATED
    case IntercityTrain:
        return plural ? i18nc( "@info/plain", "intercity / eurocity trains" )
               : i18nc( "@info/plain", "intercity / eurocity train" );
//     case TrainIntercityExpress: // DEPRECATED
    case HighSpeedTrain:
        return plural ? i18nc( "@info/plain", "intercity express trains" )
               : i18nc( "@info/plain", "intercity express train" );

    case Feet:
        return i18nc( "@info/plain", "Footway" );

    case Ferry:
        return plural ? i18nc( "@info/plain", "ferries" )
               : i18nc( "@info/plain", "ferry" );
    case Ship:
        return plural ? i18nc( "@info/plain", "ships" )
               : i18nc( "@info/plain", "ship" );
    case Plane:
        return plural ? i18nc( "@info/plain airplanes", "planes" )
               : i18nc( "@info/plain an airplane", "plane" );

    case Unknown:
    default:
        return i18nc( "@info/plain Unknown type of vehicle", "Unknown" );
    }
}

QString Global::vehicleTypeToIcon( const VehicleType& vehicleType )
{
    switch ( vehicleType ) {
    case Tram:
        return "vehicle_type_tram";
    case Bus:
        return "vehicle_type_bus";
    case Subway:
        return "vehicle_type_subway";
    case Metro:
        return "vehicle_type_metro";
    case TrolleyBus:
        return "vehicle_type_trolleybus";
    case Feet:
        return "vehicle_type_feet";

//     case TrainInterurban: // DEPRECATED
    case InterurbanTrain:
        return "vehicle_type_train_interurban";
//     case TrainRegional: // DEPRECATED Icon not done yet, using this for now
//     case TrainRegionalExpress: // DEPRECATED
    case RegionalTrain:
    case RegionalExpressTrain:
        return "vehicle_type_train_regional";
//     case TrainInterregio: // DEPRECATED
    case InterregionalTrain:
        return "vehicle_type_train_interregional";
//     case TrainIntercityEurocity: // DEPRECATED
    case IntercityTrain:
        return "vehicle_type_train_intercity";
//     case TrainIntercityExpress: // DEPRECATED
    case HighSpeedTrain:
        return "vehicle_type_train_highspeed";

    case Ferry:
    case Ship:
        return "vehicle_type_ferry";
    case Plane:
        return "vehicle_type_plane";

    case Unknown:
    default:
        return "status_unknown";
    }
}
