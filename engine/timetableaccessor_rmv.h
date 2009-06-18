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

#ifndef TIMETABLEACCESSOR_RMV_HEADER
#define TIMETABLEACCESSOR_RMV_HEADER

#include "timetableaccessor.h"

class TimetableAccessorRmv : public TimetableAccessor
{
    public:
	virtual ServiceProvider serviceProvider() { return RMV; };

	virtual QString country() const { return "Germany"; };
// 	virtual QStringList cities() const { return QStringList() << "?"; };
	
    protected:
	// parses the contents of the document at the url
	virtual bool parseDocument( QList<DepartureInfo> *journeys );
	virtual QString rawUrl(); // gets the "raw" url
};

#endif // TIMETABLEACCESSOR_RMV_HEADER
