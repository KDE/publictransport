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

/** @file
* @brief This file contains an xml reader that reads accessor info xml files.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef ACCESSORINFOXMLREADER_HEADER
#define ACCESSORINFOXMLREADER_HEADER

#include <QXmlStreamReader>
#include <QHash>
#include <QStringList>

/**
 * @brief Stores information about a single changelog entry.
 **/
struct ChangelogEntry {
    QString author; /**< The author who implemented the change. */
    QString version; /**< The version where this change was applied. */
    QString releasedWith; /**< The version of the publictransport data engine, this change was released with. */
    QString description; /**< A description of the change. */
};

enum AccessorType {
    InvalidAccessor = 0,
    ScriptedAccessor, XmlAccessor
};

struct TimetableAccessor {
    TimetableAccessor() {
        type = InvalidAccessor;
        useCityValue = false;
        onlyUseCitiesInList = false;
        fileVersion = "1.0";
        defaultVehicleType = "";
        minFetchWait = 2;
    };

    bool isValid() const { return type != InvalidAccessor; };

    QHash<QString, QString> name; /**< Name of the service provider, for different country codes. */
    QHash<QString, QString> description; /**< A description for this accessor, 
                        * for different country codes. */
    QString scriptFile; /**< The script file used to parse timetable documents. */
    QString version; /**< The version of this accessor. */
    QString fileVersion; /**< The version of the XML accessor file type. */
    QString url; /**< An URL to the service provider. */
    QString shortUrl; /**< A short version of the URL to be displayed in the applet. */
    QString rawDepartureUrl; /**< An URL to a departure board. */
    QString rawJourneyUrl; /**< An URL to a journey timetable. */
    QString rawStopSuggestionsUrl; /**< An URL to a stop suggestions document. */
    QString credit; /**< Credit information for the service provider. */
    QString author; /**< Name of the author of the accessor. */
    QString shortAuthor; /**< Short name of the author of the accessor. */
    QString email; /**< E-mail of the author of the accessor. */
    QString defaultVehicleType; /**< The vehicle type to be used if it couldn't 
        * be read for a departure/arrival. */
    QString charsetForUrlEncoding, fallbackCharset;
    QStringList cities;
    QHash< QString, QString > cityNameReplacements;
    bool useCityValue, onlyUseCitiesInList;
    int minFetchWait; /**< Minimum wait time in minutes between two data fetches. */
    QList<ChangelogEntry> changelog;
    AccessorType type;
};

class AccessorInfoXmlReader : public QXmlStreamReader {
public:
    AccessorInfoXmlReader() : QXmlStreamReader() {};

    TimetableAccessor read( QIODevice *device );

private:
    void readUnknownElement();
    TimetableAccessor readAccessorInfo();
    QString readLocalizedTextElement( QString *lang );
    bool readBooleanElement();
    void readAuthor( QString *fullname, QString *shortName, QString *email );
    void readCities( QStringList *cities,
             QHash<QString, QString> *cityNameReplacements );
    void readRawUrls( QString *rawUrlDepartures,
              QString *rawUrlStopSuggestions, QString *rawUrlJourneys );
    QList<ChangelogEntry> readChangelog();
};

class AccessorInfoXmlWriter : public QXmlStreamWriter {
    public:
    AccessorInfoXmlWriter() : QXmlStreamWriter() {};

    bool write( QIODevice *device, const TimetableAccessor &accessor );
    
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
