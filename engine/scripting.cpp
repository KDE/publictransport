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

void TimetableData::set( TimetableInformation info, const QVariant& value )
{
	if ( info == Nothing ) {
		kDebug() << "Unknown timetable information" << info
				 << "with value" << ( value.isNull() ? "NULL" : value.toString() );
	} else if ( value.isNull() ) {
		kDebug() << "Value is NULL for" << info;
	} else {
		if ( value.isValid() && value.canConvert(QVariant::String)
				&& (info == StopName || info == Target
					|| info == StartStopName || info == TargetStopName
					|| info == Operator || info == TransportLine
					|| info == Platform || info == DelayReason
					|| info == Status || info == Pricing) ) {
			m_values[ info ] = TimetableAccessorHtml::decodeHtmlEntities( value.toString() );
		} else {
			m_values[ info ] = value;
		}
	}
}
