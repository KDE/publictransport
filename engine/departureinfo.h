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

#ifndef DEPARTREINFO_HEADER
#define DEPARTREINFO_HEADER

#include <QTime>

class DepartureInfo
{
    public:
	enum LineType
	{
	    Unknown = 0,
	    Tram = 1,
	    Bus = 2,
	    Subway = 3
	};

	DepartureInfo();
	DepartureInfo(LineType lineType, int line, bool nightLine, QString target, QTime departure);
	
	QString getDurationString();
	
	LineType lineType() { return m_lineType; };
	int line() { return m_line; };
	QString lineString() { return m_sLine; };
	void setLineString(QString sLine) { m_sLine = sLine; };
	bool isNightLine() { return m_nightLine; };
	QString target() { return m_target; };
	QTime departure() { return m_departure; };

    private:
	LineType m_lineType;
	int m_line;
	QString m_sLine;
	bool m_nightLine;
	QString m_target;
	QTime m_departure;
};

#endif