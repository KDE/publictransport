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

#include "gtfsimporter.h"
#include "gtfsdatabase.h"

#include <KZip>
#include <KStandardDirs>
#include <KDebug>
#include <KLocalizedString>

#include <QDir>
#include <QVariant>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlDriver>

GtfsImporter::GtfsImporter( const QString &providerName )
        : m_state(Initializing), m_providerName(providerName), m_quit(false)
{
    // Register state enum to be able to use it in queued connections
    qRegisterMetaType< GtfsImporter::State >( "GtfsImporter::State" );

    QMutexLocker locker( &m_mutex );
    if ( !GtfsDatabase::initDatabase(providerName, &m_errorString) ) {
        m_state = FatalError;
        kDebug() << m_errorString;
    } else {
        m_state = Initialized;
    }
}

GtfsImporter::~GtfsImporter()
{
    QMutexLocker locker( &m_mutex );
    m_quit = true;
}

void GtfsImporter::quit()
{
    QMutexLocker locker( &m_mutex );
    if ( m_state == Importing ) {
        kDebug() << "Quits at next checkpoint";
    }
    m_quit = true;
}

void GtfsImporter::suspend()
{
    QMutexLocker locker( &m_mutex );
    if ( m_state == Importing ) {
        m_state = ImportingSuspended;
        kDebug() << "Suspend";
    }
}

void GtfsImporter::resume()
{
    QMutexLocker locker( &m_mutex );
    if ( m_state == ImportingSuspended ) {
        m_state = Importing;
    }
}

void GtfsImporter::startImport( const QString &fileName )
{
    m_mutex.lock();
    m_fileName = fileName;
    m_mutex.unlock();

    start( LowPriority );
}

void GtfsImporter::setError( GtfsImporter::State errorState,
                                           const QString &errorText )
{
    m_mutex.lock();
    if ( errorState <= m_state ) {
        // A more fatal error is already recorded
        m_mutex.unlock();
        return;
    }
    m_state = errorState;
    m_errorString = errorText;
    m_mutex.unlock();

    kDebug() << errorText;
    if ( errorState == FatalError ) {
        emit logMessage( i18nc("@info/plain GTFS feed import logbook entry",
                               "Fatal error: <message>%1</message>", errorText) );
        emit finished( errorState, errorText );
    } else {
        emit logMessage( i18nc("@info/plain GTFS feed import logbook entry",
                               "Error: <message>%1</message>", errorText) );
    }
}

void GtfsImporter::run()
{
    m_mutex.lock();
    m_state = Importing;
    const QString fileName = m_fileName;
    const QString providerName = m_providerName;
    QSqlDatabase database = GtfsDatabase::database( m_providerName );
    m_mutex.unlock();

    emit logMessage( i18nc("@info/plain GTFS feed import logbook entry",
                         "Start import of GTFS feed for %1", providerName) );

    // stop_times.txt is the biggest, importing it takes most time
    QStringList requiredFiles;
    requiredFiles << "agency.txt" << "stops.txt" << "routes.txt" << "trips.txt" << "stop_times.txt";

    KZip gtfsZipFile( fileName );
    if ( !gtfsZipFile.open(QIODevice::ReadOnly) ) {
        setError( FatalError, "Can not open file " + fileName + ": " +
                              gtfsZipFile.device()->errorString() );
        return;
    }
    const QString tmpGtfsDir = KGlobal::dirs()->saveLocation( "tmp",
            QFileInfo(fileName).fileName() + "_dir/" );

    // Cast away constness, to be able to set directory to another directory (but not changing it)
    KArchiveDirectory *directory = const_cast<KArchiveDirectory*>( gtfsZipFile.directory() );
    QStringList directoryEntries = directory->entries();
    QStringList missingFiles;
    bool feedSubDirectoryFound = false;
    forever {
        QStringList currentMissingFiles = requiredFiles;
        foreach ( const QString &directoryEntry, directoryEntries ) {
            currentMissingFiles.removeOne( directoryEntry );
        }

        feedSubDirectoryFound = currentMissingFiles.isEmpty();
        if ( feedSubDirectoryFound ) {
            // Required files found or no more directories to look into
            missingFiles = currentMissingFiles;
            break;
        } else {
            // Use first sub directory in current directory for new directory
            bool subDirectoryFound = false;
            foreach ( const QString &directoryEntry, directoryEntries ) {
                const KArchiveEntry *entry = directory->entry( directoryEntry );
                if ( entry->isDirectory() ) {
                    kDebug() << "Going into subdirectory of the zip file:" << entry->name();
                    directory = const_cast<KArchiveDirectory*>(
                            dynamic_cast<const KArchiveDirectory*>(entry) );
                    directoryEntries = directory->entries();
                    subDirectoryFound = true;
                    break;
                }
            }

            if ( !subDirectoryFound ) {
                // Required files not found, also not in (first) sub directories
                kDebug() << "Required files not found, also not in (first) sub directories";
                break;
            }
        }
    }

    if ( !missingFiles.isEmpty() || !feedSubDirectoryFound ) {
        kDebug() << "Required file(s) missing in GTFS feed: " << missingFiles.join(", ");
        setError( FatalError, "Required file(s) missing in GTFS feed: " + missingFiles.join(", ") ); // TODO i18nc
        return;
    }

    directory->copyTo( tmpGtfsDir );
    gtfsZipFile.close();

    // Calculate total file size (for progress calculations)
    QFileInfoList fileInfos = QDir( tmpGtfsDir ).entryInfoList( QDir::Files | QDir::NoDotAndDotDot );
    qint64 totalFileSize = 0;
    foreach ( const QFileInfo &fileInfo, fileInfos ) {
        totalFileSize += fileInfo.size();
    }

    QString errorText;
    if ( !GtfsDatabase::createDatabaseTables(&errorText, database) ) {
        setError( FatalError, "Error initializing tables in the database: " + errorText );
        return;
    }

    bool errors = false;
    qint64 totalFilePosition = 0;
    foreach ( const QFileInfo &fileInfo, fileInfos ) {
        QStringList requiredFields;
        int minimalRecordCount = 0;
        if ( fileInfo.fileName() == "agency.txt" ) {
            requiredFields << "agency_name" << "agency_url" << "agency_timezone";
        } else if ( fileInfo.fileName() == "stops.txt" ) {
            requiredFields << "stop_id" << "stop_name" << "stop_lat" << "stop_lon";
            minimalRecordCount = 1;
        } else if ( fileInfo.fileName() == "routes.txt" ) {
            requiredFields << "route_id" << "route_short_name" << "route_long_name" << "route_type";
            minimalRecordCount = 1;
        } else if ( fileInfo.fileName() == "trips.txt" ) {
            requiredFields << "trip_id" << "route_id" << "service_id";
            minimalRecordCount = 1;
        } else if ( fileInfo.fileName() == "stop_times.txt" ) {
            requiredFields << "trip_id" << "arrival_time" << "departure_time" << "stop_id"
                           << "stop_sequence";
            minimalRecordCount = 1;
        } else if ( fileInfo.fileName() == "calendar.txt" ) {
            requiredFields << "service_id" << "monday" << "tuesday" << "wednesday" << "thursday"
                           << "friday" << "saturday" << "sunday" << "start_date" << "end_date";
        } else if ( fileInfo.fileName() == "calendar_dates.txt" ) {
            requiredFields << "service_id" << "date" << "exception_type";
        } else if ( fileInfo.fileName() == "fare_attributes.txt" ) {
            requiredFields << "fare_id" << "price" << "currency_type" << "payment_method"
                           << "transfers";
        } else if ( fileInfo.fileName() == "fare_rules.txt" ) {
            requiredFields << "fare_id";
        } else if ( fileInfo.fileName() == "shapes.txt" ) {
            emit logMessage( i18nc("@info/plain GTFS feed import logbook entry",
                                 "Skip <filename>shapes.txt</filename>, data is unused") );
            // requiredFields << "shape_id" << "shape_pt_lat" << "shape_pt_lon" << "shape_pt_sequence";
            totalFilePosition += fileInfo.size();
            continue;
        } else if ( fileInfo.fileName() == "frequencies.txt" ) {
            requiredFields << "trip_id" << "start_time" << "end_time" << "headway_secs";
        } else if ( fileInfo.fileName() == "transfers.txt" ) {
            requiredFields << "from_stop_id" << "to_stop_id" << "transfer_type";
        } else {
            kDebug() << "Unexpected filename:" << fileInfo.fileName();
            emit logMessage( i18nc("@info/plain GTFS feed import logbook entry",
                                 "Unexpected filename: %1</filename>", fileInfo.fileName()) );
            totalFilePosition += fileInfo.size();
            continue;
        }

        if ( !writeGtfsDataToDatabase(database, fileInfo.filePath(), requiredFields,
                                      minimalRecordCount, totalFilePosition, totalFileSize) )
        {
            errors = true;
        }
        totalFilePosition += fileInfo.size();
        emit progress( qreal(totalFilePosition) / qreal(totalFileSize), fileInfo.baseName() );

        m_mutex.lock();
        if ( m_quit ) {
            m_mutex.unlock();
            setError( FatalError, "Importing was cancelled" );
            return;
        }
        m_mutex.unlock();
    }

    m_mutex.lock();
    m_state = errors ? FinishedWithErrors : FinishedSuccessfully;
    kDebug() << "Importer finished" << m_providerName;
    if ( errors ) {
        emit logMessage( i18nc("@info/plain GTFS feed import logbook entry",
                             "Import finished with error <message>%1</message>", m_errorString) );
    } else {
        emit logMessage( i18nc("@info/plain GTFS feed import logbook entry",
                             "Import finished successfully") );
    }
    m_mutex.unlock();

    emit finished( errors ? FinishedWithErrors : FinishedSuccessfully );
    return;
}

bool GtfsImporter::writeGtfsDataToDatabase( QSqlDatabase database,
        const QString &fileName, const QStringList &requiredFields, int minimalRecordCount,
        qint64 totalFilePosition, qint64 totalFileSize )
{
    // Open the file
    QFile file( fileName );
    if ( !file.open(QIODevice::ReadOnly) ) {
        setError( FatalError, "Cannot open file " + fileName );
        return false;
    }

    // Check if the file is empty
    if ( file.atEnd() ) {
        file.close();
        if ( minimalRecordCount == 0 ) {
            return true;
        } else {
            setError( FatalError, "Empty file " + fileName );
            return false;
        }
    }

    // Open the database
    if ( !database.isValid() || !database.isOpen() ) {
        file.close();
        setError( FatalError, "Can not open database" );
        return false;
    }

    const QString tableName = QFileInfo(file).baseName();
    emit logMessage( i18nc("@info/plain GTFS feed import logbook entry",
                         "Import GTFS data for table %1", tableName) );

    // Read first line from file (header with used field names)
    QStringList fieldNames;
    if ( file.atEnd() || !readHeader(decode(file.readLine()), &fieldNames, requiredFields) ) {
        file.close();
        return false; // Error in header
    }
    // Get types of the fields
    QList<GtfsDatabase::FieldType> fieldTypes;
    foreach ( const QString &fieldName, fieldNames ) {
        fieldTypes << GtfsDatabase::typeOfField( fieldName );
    }

    QSqlRecord table = database.record( tableName );
    QStringList unavailableFieldNames; // Field names not used in the database
    QList<int> unavailableFieldIndices;
    for ( int i = fieldNames.count() - 1; i >= 0; --i ) {
        const QString fieldName = fieldNames[i];
        if ( !table.contains(fieldName) && fieldName != "monday" && fieldName != "tuesday" &&
             fieldName != "wednesday" && fieldName != "thursday" && fieldName != "friday" &&
             fieldName != "saturday" && fieldName != "sunday" )
        {
            // The current field name is not available in the database, skip it's value in each row
            unavailableFieldNames << fieldNames[i];
            unavailableFieldIndices << i;
            fieldNames.removeAt( i );
        }
    }
    if ( !unavailableFieldNames.isEmpty() ) {
        kDebug() << "Not all used fields are available in the database:" << unavailableFieldNames
                 << "table:" << tableName;
        emit logMessage( i18nc("@info/plain GTFS feed import logbook entry",
                             "Not all used fields are available in table %1: %2",
                             tableName, unavailableFieldNames.join(", ")) );
    }

    QStringList dbFieldNames = fieldNames;
    if ( tableName == QLatin1String("calendar") ) {
        dbFieldNames.removeOne( "monday" );
        dbFieldNames.removeOne( "tuesday" );
        dbFieldNames.removeOne( "wednesday" );
        dbFieldNames.removeOne( "thursday" );
        dbFieldNames.removeOne( "friday" );
        dbFieldNames.removeOne( "saturday" );
        dbFieldNames.removeOne( "sunday" );
        dbFieldNames.append( "weekdays" );
    }
    QString placeholder( '?' );
    for ( int i = 1; i < dbFieldNames.count(); ++i ) {
        placeholder += ",?";
    }

    // Simple benchmark, prints the time it took until the Block got destructed
    KDebug::Block insertBlock( ("Import GTFS table " + tableName).toUtf8() );

    // Create a query for the database
    QSqlQuery query( database );

    // Performance optimization
    if( !query.exec("PRAGMA synchronous=OFF;") ) {
        qDebug() << query.lastError();
        emit logMessage( query.lastError().text() );
    }

    // Disable journal completely, because it's much faster
    // and there is nothing to loose, if the import crashes the database will
    // very likely go corrupt, but the import can be simply restarted
    if( !query.exec("PRAGMA journal_mode=OFF;") ) {
        qDebug() << query.lastError();
        emit logMessage( query.lastError().text() );
    }

    // Begin transaction
    if ( !database.driver()->beginTransaction() ) {
        qDebug() << database.lastError();
        emit logMessage( database.lastError().text() );
    }

    // Prepare an INSERT query to be used for each dataset to be inserted
    query.prepare( QString("INSERT OR REPLACE INTO %1 (%2) VALUES (%3)")
                .arg(tableName, dbFieldNames.join(","), placeholder) );

    int counter = 0;
    while ( !file.atEnd() ) {
        const QByteArray line = file.readLine();
        QVariantList fieldValues;
        if ( readFields(line, &fieldValues, fieldTypes, fieldNames.count()) ) {
            // Remove values for fields that do not exist in the database
            for ( int i = 0; i < unavailableFieldIndices.count(); ++i ) {
                fieldValues.removeAt( unavailableFieldIndices[i] );
            }

            // Add current field values to the prepared query and execute it
            if ( tableName == "calendar" ) {
                QString weekdays = "0000000";
                for ( int i = 0; i < fieldNames.count(); ++i ) {
                    const QString fieldName = fieldNames[i];
                    if ( fieldValues[i].toInt() > 0 ) {
                        if ( fieldName == "sunday" ) {
                            weekdays[0] = '1';
                        } else if ( fieldName == "monday" ) {
                            weekdays[1] = '1';
                        } else if ( fieldName == "tuesday" ) {
                            weekdays[2] = '1';
                        } else if ( fieldName == "wednesday" ) {
                            weekdays[3] = '1';
                        } else if ( fieldName == "thursday" ) {
                            weekdays[4] = '1';
                        } else if ( fieldName == "friday" ) {
                            weekdays[5] = '1';
                        } else if ( fieldName == "saturday" ) {
                            weekdays[6] = '1';
                        } else {
                            query.addBindValue( fieldValues[i] );
                        }
                    } else if ( fieldName != "monday" && fieldName != "tuesday" &&
                        fieldName != "wednesday" && fieldName != "thursday" &&
                        fieldName != "friday" && fieldName != "saturday" && fieldName != "sunday" )
                    {
                        query.addBindValue( fieldValues[i] );
                    }
                }
                query.addBindValue( weekdays );
            } else if ( tableName == "stop_times" ) {
                // If only one of "departure_time" and "arrival_time" is set,
                // copy the value to both fields
                int departureTime = -1;
                int arrivalTime = -1;
                int departureTimeIndex = -1;
                int arrivalTimeIndex = -1;
                for ( int i = 0; i < fieldNames.count(); ++i ) {
                    if ( fieldNames[i] == "departure_time" ) {
                        departureTime = fieldValues[i].toInt();
                        departureTimeIndex = i;
                    } else if ( fieldNames[i] == "arrival_time" ) {
                        arrivalTime = fieldValues[i].toInt();
                        arrivalTimeIndex = i;
                    }
                }
                if ( departureTimeIndex >= 0 ) {
                    arrivalTime = departureTime;
                    fieldValues[ arrivalTimeIndex ] = departureTime;
                } else if ( departureTime < 0 ) {
                    departureTime = arrivalTime;
                    fieldValues[ departureTimeIndex ] = arrivalTime;
                }
                foreach ( const QVariant &fieldValue, fieldValues ) {
                    query.addBindValue( fieldValue );
                }
            } else {
                foreach ( const QVariant &fieldValue, fieldValues ) {
                    query.addBindValue( fieldValue );
                }
            }
            if ( !query.exec() ) {
                QMutexLocker locker( &m_mutex );
                emit logMessage( query.lastError().text() );
                kDebug() << query.lastError();
                kDebug() << "With this query:" << query.executedQuery();
                continue;
            }

            // New row has been inserted into the DB successfully
            ++counter; // counter is now >= 1

            // Start a new transaction after 50000 INSERTs
            if ( counter % 50000 == 0 ) {
                if ( !database.driver()->commitTransaction() ) {
                    qDebug() << database.lastError();
                    emit logMessage( database.lastError().text() );
                }
                if ( !database.driver()->beginTransaction() ) {
                    qDebug() << database.lastError();
                    emit logMessage( database.lastError().text() );
                }
            }

            // Report progress and check for quit/suspend after each 500 INSERTs
            if ( counter % 500 == 0 ) {
                // Report progress
                emit progress( qreal(totalFilePosition + file.pos()) / qreal(totalFileSize),
                                tableName );

                // Check if the job should be cancelled
                m_mutex.lock();
                if ( m_quit ) {
                    m_mutex.unlock();
                    setError( FatalError, "Importer was cancelled" );
                    return false;
                }

                // Check if the job should be suspended
                if ( m_state == ImportingSuspended ) {
                    // Commit before going to sleep for suspension
                    if ( !database.driver()->commitTransaction() ) {
                        qDebug() << database.lastError();
                        emit logMessage( database.lastError().text() );
                    }

                    do {
                        // Do not lock while sleeping, otherwise ::resume() results in a deadlock
                        m_mutex.unlock();

                        // Suspend import for one second
                        sleep( 1 );

                        // Lock mutex again, to check if m_state is still ImportingSuspended
                        m_mutex.lock();
                        kDebug() << "Next check for suspended state" << m_state;
                    } while ( m_state == ImportingSuspended );

                    // Start a new transaction
                    if ( !database.driver()->beginTransaction() ) {
                        qDebug() << database.lastError();
                        emit logMessage( database.lastError().text() );
                    }
                }

                // Unlock mutex again
                m_mutex.unlock();
            }
        }
    }

    // End transaction, restore synchronous=FULL
    if ( !database.driver()->commitTransaction() ) {
        qDebug() << database.lastError();
        emit logMessage( database.lastError().text() );
    }
    if( !query.exec("PRAGMA synchronous=FULL;") ) {
        qDebug() << query.lastError();
        emit logMessage( query.lastError().text() );
    }

    // Close the file again
    file.close();

    // Return true (success) if at least one stop has been read
    if ( counter >= minimalRecordCount ) {
        return true;
    } else {
        setError( FatalError, "Not enough records found in " + tableName );
        kDebug() << "Minimal record count is" << minimalRecordCount << "but only" << counter
                 << "records were added";
        return false;
    }
}

bool GtfsImporter::readHeader( const QString &header, QStringList *fieldNames,
                                             const QStringList &requiredFields )
{
    QStringList names = header.simplified().split(',');
    *fieldNames = QStringList();
    if ( names.isEmpty() ) {
        setError( FatalError, "No field names found in header: " + header );
        return false;
    }

    // Only allow alphanumerical characters as field names (and prevent SQL injection).
    QRegExp regExp( "[^A-Z0-9_]", Qt::CaseInsensitive );
    foreach ( const QString &fieldName, names ) {
        QString name = fieldName;
        if ( (name.startsWith('"') && name.endsWith('"')) ||
             (name.startsWith('\'') && name.endsWith('\'')) )
        {
            name = name.mid( 1, name.length() - 2 );
        }
        if ( regExp.indexIn(name) != -1 ) {
            emit logMessage( i18nc("@info", "Field name <emphasis>%1</emphasis> contains "
                                   "a disallowed character <emphasis>%2</emphasis>' at %3",
                                   fieldName, regExp.cap(), regExp.pos()) );
        } else {
            fieldNames->append( name );
        }
    }

    // Check required fields
    foreach ( const QString &requiredField, requiredFields ) {
        if ( !fieldNames->contains(requiredField) ) {
            emit logMessage( i18nc("@info", "Required field '%1' is missing", requiredField) );
            kDebug() << "Required field missing:" << requiredField;

            if ( requiredField == "agency_timezone" ) {
                kDebug() << "Will use default timezone";
                fieldNames->append( "agency_timezone" );
            } else {
                setError( FatalError, "Required field missing: " + requiredField );
                kDebug() << "in this header line:" << header;
                return false;
            }
        }
    }

    return true;
}

bool GtfsImporter::readFields( const QByteArray &line, QVariantList *fieldValues,
        const QList<GtfsDatabase::FieldType> &fieldTypes, int expectedFieldCount )
{
    int pos = 0;
    QList<GtfsDatabase::FieldType>::ConstIterator fieldType = fieldTypes.constBegin();
    while ( pos < line.length() && fieldType != fieldTypes.constEnd() ) {
        int endPos = pos;
        QByteArray newField;
        if ( line[pos] == '"' ) {
            // A field with a quotation mark in it must start and end with a quotation mark,
            // all other quotation marks must be preceded with another quotation mark
            ++endPos;
            while ( endPos < line.length() ) {
                if ( line[endPos] == '"' ) {
                    if ( endPos + 1 >= line.length() || line[endPos + 1] == ',' ) {
                        break; // At the end of the field / line
                    }

                    if ( line[endPos + 1] == '"' ) {
                        ++endPos; // Two quotation marks read, skip them
                    }
                }
                ++endPos;
            }

            if ( endPos >= line.length() || line[endPos] != '"' ) {
                kWarning() << "No field end delimiter found in line" << line;
                kWarning() << "for field starting with a delimiter at position" << pos;
                kWarning() << "Read until the end of the line";
                // Do not remove 1, because it gets done below,
                // there is no delimiter to remove at the end
                endPos = line.length();
            }

            // Add field value without the quotation marks around it
            // and doubled quotation marks replaced with single ones
            newField = line.mid( pos + 1, endPos - pos - 1 )
                    .replace( QByteArray("\"\""), QByteArray("\"") );
            pos = endPos + 2;
        } else if ( line[pos] == ',' ) {
            // Empty field, newField stays empty
            ++pos;

        } else {
            // Field without quotation marks, read until the next ','
            endPos = line.indexOf( ',', pos );
            if ( endPos < 0 ) {
                endPos = line.length();
            }

            // Add field value
            newField = line.mid( pos, endPos - pos );
            pos = endPos + 1;
        }

        // Append the new field value
        fieldValues->append( GtfsDatabase::convertFieldValue(newField, *fieldType) );
        ++fieldType;

        if ( pos == line.length() && line[pos - 1] == ',' ) {
            // The current line ends after a ','. Add another empty field:
            fieldValues->append( GtfsDatabase::convertFieldValue(QByteArray(), *fieldType) );
            ++fieldType;
        }
    }

    if ( fieldValues->isEmpty() ) {
        return false;
    } else if ( fieldValues->count() < expectedFieldCount ) {
        kWarning() << "Header contains" << expectedFieldCount << "fields, but a line was read "
                "with only" << fieldValues->count() << "field values. Using empty/default values:";
        kWarning() << "Values: " << *fieldValues;
        while ( fieldValues->count() < expectedFieldCount ) {
            fieldValues->append( QVariant() );
        }
    }

    return true;
}

#include "gtfsimporter.moc"
