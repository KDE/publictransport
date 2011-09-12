/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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
* @brief This file contains a class to import data from GTFS feeds.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef GENERALTRANSITFEEDIMPORTER_HEADER
#define GENERALTRANSITFEEDIMPORTER_HEADER

#include <QString>
#include <QVariant>

class QSqlRecord;

class GeneralTransitFeedImporter
{
public:
    explicit GeneralTransitFeedImporter( const QString &providerName );

    static QString databasePath( const QString &providerName );

    static bool writeGtfsDataToDatabase( const QString &fileName );

    /** @brief Initializes the database */
    static bool initDatabase( const QString &providerName );

    /** @brief Creates all needed tables in the database, if they did not already exist. */
    static bool createDatabaseTables();

private:
    static bool writeGtfsDataToDatabase( const QString &fileName,
                                         const QStringList &requiredFields );

    static bool readHeader( const QString &header, QStringList *fieldNames,
                            const QStringList &requiredFields );
    static bool readFields( const QString &line, QVariantList *fieldValues,
                            const QList<QVariant::Type> &types, int expectedFieldCount );

    static QVariant::Type typeOfField( const QString &fieldName );
    static QVariant convertFieldValue( const QString &fieldValue, QVariant::Type type );

};

#endif // Multiple inclusion guard
