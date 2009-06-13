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

#ifndef TIMETABLEACCESSOR_NASA_HEADER
#define TIMETABLEACCESSOR_NASA_HEADER

#include "timetableaccessor_efa.h"

class TimetableAccessorNasa : public TimetableAccessorEfa
{
    public:
	virtual ServiceProvider serviceProvider() { return NASA; };

	virtual QString country() const { return "Germany"; };
	// TODO: add all fahrplaner.de-cities (Niedersachsen/Bremen)
	virtual QStringList cities() const { return QStringList() << "Leipzig" << "Halle" << "Magdeburg" << "Dessau" << "Wernigerode" << "Halberstadt" << "Sangerhausen" << "Merseburg" << "Weissenfels" << "Zeitz" << "Altenburg" << "Delitzsch" << "Wolfen" << "Aschersleben" << "Köthen (Anhalt)" << "Wittenberg" << "Schönebeck (Elbe)" << "Stendal"; };
	
    protected:
	virtual QString rawUrl(); // gets the "raw" url
	virtual QString regExpSearch(); // the regexp string to use
	virtual DepartureInfo getInfo(QRegExp rx);
};

#endif // TIMETABLEACCESSOR_NASA_HEADER
