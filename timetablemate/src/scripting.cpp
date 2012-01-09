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

#include "scripting.h"

void TimetableData::set( const QString &info, const QVariant& value ) {
    static QStringList validDepartureStrings = QStringList()
		<< "departuredatetime" << "departuredate" << "departuretime" << "typeofvehicle"
		<< "transportline" << "flightnumber" << "target" << "platform" << "delay" << "delayreason"
		<< "journeynews" << "journeynewsother" << "journeynewslink"
		<< "operator" << "status" << "routestops"
		<< "routetimes" << "routeexactstops" << "isnightline";
    static QStringList validJourneyStrings = QStringList()
		<< "departuredatetime" << "departuredate" << "departuretime"
		<< "duration" << "startstopname" << "startstopid" << "targetstopname" << "targetstopid"
		<< "arrivaldatetime" << "arrivaldate" << "arrivaltime" << "changes"
		<< "typesofvehicleinjourney" << "pricing" << "routetransportlines" << "routetypesofvehicles"
		<< "routeplatformsdeparture" << "routeplatformsarrival" << "routetimesdeparturedelay"
		<< "routetimesarrivaldelay" << "routetimesdeparture" << "routetimesarrival"
		<< "routestops" << "journeynews" << "journeynewsother" << "journeynewslink"
		<< "operator";
    static QStringList validStopSuggestionStrings = QStringList()
		<< "stopname" << "stopid" << "stopweight" << "stopcity" << "stopcountrycode";

    QString s = info.toLower();
    if ( (m_mode == "departures" && !validDepartureStrings.contains(s))
      || (m_mode == "journeys" && !validJourneyStrings.contains(s))
      || (m_mode == "stopsuggestions" && !validStopSuggestionStrings.contains(s)) )
    {
		kDebug() << "Unknown timetable information" << s
			<< "with value" << (value.isNull() ? "NULL" : value.toString());
		m_unknownTimetableInformationStrings.insert( info, value );
    } else if ( value.isNull() ) {
		kDebug() << "Value is NULL for" << s;
    } else {
		// Valid data
		if ( value.isValid() && value.canConvert(QVariant::String)
			&& (s == "stopname" || s == "target"
			|| s == "startStopName" || s == "targetstopname"
			|| s == "operator" || s == "transportline"
			|| s == "platform" || s == "delayreason"
			|| s == "status" || s == "pricing") )
		{
			m_values[ s ] = /*TimetableAccessorScript::decodeHtmlEntities(*/ value.toString() /*)*/; // TODO
		} else if ( value.isValid() && value.canConvert(QVariant::List) && s == "departuredate" ) {
			QVariantList date = value.toList();
			m_values[ s ] = date.length() == 3 ? QDate(date[0].toInt(), date[1].toInt(), date[2].toInt()) : value;
		} else {
			m_values[ s ] = value;
		}
    }
}
