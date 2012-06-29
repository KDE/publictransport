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
 * @brief This file contains an xml reader that reads service provider data XML files.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SERVICEPROVIDERDATAREADER_HEADER
#define SERVICEPROVIDERDATAREADER_HEADER

// Own includes
#include "enums.h"

// Qt includes
#include <QXmlStreamReader> // Base class

class ServiceProvider;
class ServiceProviderData;

/**
 * @brief Store information about a single changelog entry.
 **/
struct ChangelogEntry {
    explicit ChangelogEntry( const QString &version = "1.0" ) : version(version) {};

    QString author; /**< The author who implemented the change. */
    QString version; /**< The version of the service provider data file where this change was applied. */
    QString engineVersion; /**< The version of the PublicTransport engine where this
            * change was integrated. */
    QString description; /**< A description of the change. */
};
Q_DECLARE_METATYPE(ChangelogEntry);
Q_DECLARE_METATYPE(QList<ChangelogEntry>);

/**
 * @brief Read service provider data XML files.
 *
 * In a service provider data XML it's properties are described, like the name of the service
 * provider, a used script file, raw urls, etc. See @ref pageServiceProviders for more information
 * about the XML structure.
 **/
class ServiceProviderDataReader : public QXmlStreamReader {
    friend class ServiceProviderData; // Because ServiceProviderDataReader needs to set values
            // in ServiceProviderData when reading xml files

public:
    enum ErrorAcceptance {
        OnlyReadCorrectFiles,
        ReadErrorneousFiles
    };

    /** @brief Creates a new service provider data reader. */
    ServiceProviderDataReader() : QXmlStreamReader() {};

    /**
     * @brief Reads the XML file for the given @p serviceProvider.
     *
     * If the provider XML file can be found but has errors, error information gets written to the
     * cache to prevent reading the file again as long as it does not change.
     *
     * @param providerId The ID of the service provider which XML file should be read.
     *   The ID starts with a country code, followed by an underscore and it's name.
     *   If it's empty, the default service provider for the users country will
     *   be used, if there is any.
     * @param errorMessage A pointer to a QString to fill with an error message if there was an
     *   error. Can be 0.
     *
     * @return A pointer to the read ServiceProviderData object or 0 if there was an error reading
     *   the data.
     **/
    static ServiceProviderData *read( const QString &providerId, QString *errorMessage = 0 );

    /**
     * @brief Reads service provider data from @p device.
     *
     * @param device The QIODevice to read the xml data from.
     * @param serviceProvider The service provider ID for the service provider to read.
     * @param fileName The filename of the xml file.
     * @param country The country the service provider operates in.
     *
     * @return A ServiceProviderData object or NULL on error.
     **/
    ServiceProviderData* read( QIODevice *device, const QString &serviceProvider,
                               const QString &fileName, const QString &country,
                               ErrorAcceptance errorAcceptance = OnlyReadCorrectFiles,
                               QObject *parent = 0 );

    ServiceProviderData* read( QIODevice *device, const QString &fileName,
                               ErrorAcceptance errorAcceptance = OnlyReadCorrectFiles,
                               QObject *parent = 0 );

private:
    void readUnknownElement();
    ServiceProviderData *readProviderData( const QString &serviceProvider,
                                           const QString &fileName, const QString &country,
                                           ErrorAcceptance errorAcceptance, QObject *parent );
    QString readLocalizedTextElement( QString *lang );
    bool readBooleanElement();
    void readAuthor( QString *fullname, QString *shortName, QString *email );
    void readCities( QStringList *cities, QHash<QString, QString> *cityNameReplacements );
    void readSamples( QStringList *stops, QString *city );
    QList<ChangelogEntry> readChangelog();
};

#endif // Multiple inclusion guard
