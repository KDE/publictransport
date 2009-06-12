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
#include <KLocale>

DepartureInfo::DepartureInfo ()
{
}

DepartureInfo::DepartureInfo ( DepartureInfo::LineType lineType, int line, bool nightLine, const QString& target, const QTime &departure )
{
    m_lineType = lineType;
    m_line = line;
    m_nightLine = nightLine;
    m_target = target;
    m_departure = departure;
}

QString DepartureInfo::getDurationString() const
{
    int totalSeconds = QTime::currentTime().secsTo(m_departure);
    int seconds = totalSeconds % 60;
    totalSeconds -= seconds;
    if (seconds > 0)
	totalSeconds += 60;
    if (totalSeconds < 0)
	return i18n("depart is in the past");

    int minutes = (totalSeconds / 60) % 60;
    int hours = totalSeconds / 3600;
    QString str;

    if (hours != 0)
    {
	str += i18n("in <b>%1</b> hours",hours);
	if (minutes != 0)
	    str += i18n(" and ");
    }
    if (minutes != 0)
	str += i18n("in <b>%1</b> minutes",minutes);
    if (hours == 0 && minutes == 0)
	str = i18n("now");

    return str;
}

