/*
*   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef GLOBAL_HEADER
#define GLOBAL_HEADER

// Own includes
#include "enums.h"

// Qt includes
#include <QString>

class Global {
public:
    Global() {};

    /** @brief Gets the VehicleType enumerable for the given string. */
    static VehicleType vehicleTypeFromString( QString sVehicleType );

    /** Gets the name of the given type of vehicle. */
    static QString vehicleTypeToString( const VehicleType &vehicleType, bool plural = false );

    /** Gets an icon for the given type of vehicle. */
    static QString vehicleTypeToIcon( const VehicleType &vehicleType );

    /**
     * @brief Gets the TimetableInformation enumerable for the given string.
     * If no TimetableInformation matches @p sTimetableInformation Nothing is returned.
     **/
    static TimetableInformation timetableInformationFromString( const QString& sTimetableInformation );

    /** @brief Gets a string for the given @p timetableInformation. */
    static QString timetableInformationToString( TimetableInformation timetableInformation );

    static bool checkTimetableInformation( TimetableInformation info, const QVariant &value );

    /** @brief Decodes HTML entities in @p html, e.g. "&nbsp;" is replaced by " ". */
    static QString decodeHtmlEntities( const QString& html );

    /** @brief Encodes HTML entities in @p html, e.g. "<" is replaced by "&lt;". */
    static QString encodeHtmlEntities( const QString& html );

    /**
     * @brief Decodes the given HTML document.
     *
     * First it tries QTextCodec::codecForHtml().
     * If that doesn't work, it parses the document for the charset in a meta-tag.
     **/
    static QString decodeHtml( const QByteArray& document,
                               const QByteArray& fallbackCharset = QByteArray() );
};

#endif // Multiple inclusion guard
