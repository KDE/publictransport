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
* @brief This file contains a class to handle a GTFS database.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef GENERALTRANSITFEEDDATABASE_HEADER
#define GENERALTRANSITFEEDDATABASE_HEADER

#include <qsql.h>
#include <QSqlDatabase>

class QString;
class QVariant;

/**
 * @brief Provides static methods to handle a GTFS database.
 *
 * @warning Before using any other method, @ref initDatabase must be called to open a connection
 *   to the correct database for a specific provider.
 **/
class GeneralTransitFeedDatabase {
public:
    /**
     * @brief Types of fields in the database tables.
     **/
    enum FieldType {
        HashId, /**< The qHash value of the source value is stored in the database. Used for IDs
                * which can be strings in GTFS feeds. For performance reasons integers are much
                * better in the database. */
        Integer, /**< The source value is converted to an integer before storing it in the
                * database, using QString::toInt. */
        Double, /**< The source value is converted to a double before storing it in the database,
                * using QString::toDouble. */
        SecondsSinceMidnight, /**< The source value is converted to the number of seconds that
                * have passed since midnight at the given Date. */
        Date, /**< The source value is converted to a QDate before storing it in the database,
                * using QDate::fromString. */
        String, /**< The source value is stored as it is in the database. */
        Color, /**< The source value is converted to a QColor before storing it in the database,
                * using the source value as named color by prepending a '#'. */
        Url /**< The source value is converted to a QUrl before storing it in the database. */
    };

    /**
     * @brief Initializes the database
     *
     * @param providerName The name of the provider for which a GTFS database should be opened.
     * @param errorText Gets set to a string explaining an error, if this returns false.
     *
     * @returns True, if the database could be initialized successfully. False, otherwise.
     **/
    static bool initDatabase( const QString &providerName, QString *errorText );

    /**
     * @brief Creates all needed tables in the database, if they did not already exist.
     *
     * @param errorText Gets set to a string explaining an error, if this returns false.
     * @param database The database to use.
     *
     * @returns True, if the database tables could be created successfully. False, otherwise.
     **/
    static bool createDatabaseTables( QString *errorText, QSqlDatabase database = QSqlDatabase() );

    /**
     * @brief Gets the full path to the SQLite database file for the given @p providerName.
     *
     * @param providerName The name of the provider for which the path to the database should be
     *   returned.
     **/
    static QString databasePath( const QString &providerName );

    /**
     * @brief Gets the target type in the database of the GTFS field with the given @p fieldName.
     *
     * @param fieldName The name of the GTFS field, which target type in the database should be
     *   returned.
     **/
    static FieldType typeOfField( const QString &fieldName );

    /**
     * @brief Converts the given source @p fieldValue to the given target @p type.
     *
     * @param fieldValue The source value from a GTFS feed file (CSV).
     * @param type The target type to convert @p fieldValue to.
     *
     * @return A QVariant with @p fieldValue converted to @p type.
     **/
    static QVariant convertFieldValue( const QString &fieldValue, FieldType type );
};

#endif // Multiple inclusion guard
