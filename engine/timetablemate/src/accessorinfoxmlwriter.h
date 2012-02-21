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

/** @file
* @brief This file contains an xml reader that reads accessor info xml files.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef ACCESSORINFOXMLWRITER_HEADER
#define ACCESSORINFOXMLWRITER_HEADER

// Includes from the PublicTransport data engine
#include <engine/timetableaccessor.h>

// Qt includes
#include <QXmlStreamWriter>
#include <QHash>
#include <QStringList>

class AccessorInfoXmlWriter : public QXmlStreamWriter
{
public:
    AccessorInfoXmlWriter() : QXmlStreamWriter() {};

    bool write( QIODevice *device, const TimetableAccessor *accessor );

private:
    TimetableAccessor writeAccessorInfo();
    void writeAuthor( QString *fullname, QString *email );
    void writeCities( QStringList *cities,
                      QHash<QString, QString> *cityNameReplacements );
    void writeRawUrls( QString *rawUrlDepartures,
                       QString *rawUrlStopSuggestions, QString *rawUrlJourneys );
    void writeChangelog( const QList<ChangelogEntry> &changelog );
};

#endif // ACCESSORINFOXMLREADER_HEADER
// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
