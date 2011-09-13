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

#include "generaltransitfeed_importer.h"
#include "generaltransitfeed_database.h"

#include <KZip>
#include <KStandardDirs>
#include <KDebug>

#include <QDir>
#include <QVariant>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlDriver>

GeneralTransitFeedImporter::GeneralTransitFeedImporter( const QString &providerName )
        : m_state(Initializing), m_quit(false)
{
    // Register state enum to be able to use it in queued connections
    qRegisterMetaType< GeneralTransitFeedImporter::State >( "GeneralTransitFeedImporter::State" );

    QMutexLocker locker( &m_mutex );
    if ( !GeneralTransitFeedDatabase::initDatabase(providerName, &m_errorString) ) {
        m_state = FatalError;
        kDebug() << m_errorString;
    }
}

GeneralTransitFeedImporter::~GeneralTransitFeedImporter()
{
    QMutexLocker locker( &m_mutex );
    m_quit = true;
}

void GeneralTransitFeedImporter::quit()
{
    if ( isRunning() ) {
        QMutexLocker locker( &m_mutex );
        m_quit = true;
    }
}

void GeneralTransitFeedImporter::startImport( const QString &fileName )
{
    m_mutex.lock();
    m_fileName = fileName;
    m_mutex.unlock();

    start();
}

void GeneralTransitFeedImporter::setError( GeneralTransitFeedImporter::State errorState,
                                           const QString &errorText )
{
    m_mutex.lock();
    m_state = errorState;
    m_errorString = errorText;
    kDebug() << errorText;
    m_mutex.unlock();

    if ( errorState == FatalError ) {
        emit finished( errorState, errorText );
    }
}

// The GTFS (GeneralTransitFeedSpecification) specification defines the following files:
//-----------------------------------------------------------------------------------------------
// agency.txt - Required. This file contains information about one or more transit agencies that
//          provide the data in this feed.
// stops.txt - Required. This file contains information about individual locations where vehicles
//          pick up or drop off passengers.
// routes.txt - Required. This file contains information about a transit organization's routes.
//          A route is a group of trips that are displayed to riders as a single service.
// trips.txt - Required. This file lists all trips and their routes. A trip is a sequence of two
//          or more stops that occurs at specific time.
// stop_times.txt - Required. This file lists the times that a vehicle arrives at and departs
//          from individual stops for each trip.
// calendar.txt - Required. This file defines dates for service IDs using a weekly schedule.
//          Specify when service starts and ends, as well as days of the week where service
//          is available.
// calendar_dates.txt - Optional. This file lists exceptions for the service IDs defined in the
//          calendar.txt file. If calendar_dates.txt includes ALL dates of service, this file may
//          be specified instead of calendar.txt.
// fare_attributes.txt - Optional. This file defines fare information for a transit
//          organization's routes.
// fare_rules.txt - Optional. This file defines the rules for applying fare information for a
//          transit organization's routes.
// shapes.txt - Optional. This file defines the rules for drawing lines on a map to represent
//          a transit organization's routes.
// frequencies.txt - Optional. This file defines the headway (time between trips) for routes with
//          variable frequency of service.
// transfers.txt - Optional. This file defines the rules for making connections at transfer
//          points between routes.
//
// (see http://code.google.com/intl/de-DE/transit/spec/transit_feed_specification.html#transitFeedFiles)
// bool GeneralTransitFeedImporter::writeGtfsDataToDatabase( const QString &fileName )
// {
void GeneralTransitFeedImporter::run()
{
    m_mutex.lock();
    m_state = Importing;
    const QString fileName = m_fileName;
    m_mutex.unlock();

    QString errorText;
    if ( !GeneralTransitFeedDatabase::createDatabaseTables(&errorText) ) {
        setError( FatalError, "Error initializing tables in the database: " + errorText );
        return;
    }

    KZip gtfsZipFile( fileName );
    if ( !gtfsZipFile.open(QIODevice::ReadOnly) ) {
        setError( FatalError, "Can not open file " + fileName + ": " +
                              gtfsZipFile.device()->errorString() );
        return;
    }
    const QString tmpGtfsDir = KGlobal::dirs()->saveLocation( "tmp",
            QFileInfo(fileName).fileName() + "_dir/" );
    gtfsZipFile.directory()->copyTo( tmpGtfsDir );
    gtfsZipFile.close();

    QFileInfoList fileInfos = QDir( tmpGtfsDir ).entryInfoList( QDir::Files | QDir::NoDotAndDotDot );
    if ( fileInfos.isEmpty() ) {
        setError( FatalError, "Empty GTFS feed: " + fileName );
        kDebug() << "Extracted to" << tmpGtfsDir;
        return;
    }

    // Check required files in the feed and calculate total file size (for progress calculations)
    QStringList requiredFiles;
    requiredFiles << "agency.txt" << "stops.txt" << "routes.txt" << "trips.txt" << "stop_times.txt";
    qint64 totalFileSize = 0;
    foreach ( const QFileInfo &fileInfo, fileInfos ) {
        requiredFiles.removeOne( fileInfo.fileName() );
        totalFileSize += fileInfo.size();
    }
    if ( !requiredFiles.isEmpty() ) {
        setError( FatalError, "Required file(s) missing in GTFS feed: " + requiredFiles.join(", ") );
        return;
    }

    bool errors = false;
    qint64 totalFilePosition = 0;
    foreach ( const QFileInfo &fileInfo, fileInfos ) {
        QStringList requiredFields;
        int minimalRecordCount = 0;
        if ( fileInfo.fileName() == "agency.txt" ) {
            requiredFields << "agency_name" << "agency_url" << "agency_timezone";
            minimalRecordCount = 1;
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
            kDebug() << "Skipping 'shapes.txt', data is unused";
//             requiredFields << "shape_id" << "shape_pt_lat" << "shape_pt_lon" << "shape_pt_sequence";
            totalFilePosition += fileInfo.size();
            continue;
        } else if ( fileInfo.fileName() == "frequencies.txt" ) {
            requiredFields << "trip_id" << "start_time" << "end_time" << "headway_secs";
        } else if ( fileInfo.fileName() == "transfers.txt" ) {
            requiredFields << "from_stop_id" << "to_stop_id" << "transfer_type";
        } else {
            kDebug() << "Filename unexpected:" << fileInfo.fileName();
            totalFilePosition += fileInfo.size();
            continue;
        }

        if ( !writeGtfsDataToDatabase(fileInfo.filePath(), requiredFields, minimalRecordCount,
                                      totalFilePosition, totalFileSize) )
        {
            errors = true;
        }
        totalFilePosition += fileInfo.size();
        emit progress( qreal(totalFilePosition) / qreal(totalFileSize) );

        m_mutex.lock();
        if ( m_quit ) {
            m_mutex.unlock();
            setError( FatalError, "Importing was cancelled" );
            return;
        }
        m_mutex.unlock();
    }
// TODO Test required files...

    m_mutex.lock();
    m_state = errors ? FinishedWithErrors : FinishedSuccessfully;
    emit finished( m_state );
    m_mutex.unlock();
    kDebug() << "Importer finished";
    return;
}

bool GeneralTransitFeedImporter::writeGtfsDataToDatabase( const QString &fileName,
        const QStringList &requiredFields, int minimalRecordCount,
        qint64 totalFilePosition, qint64 totalFileSize )
{
    // Open the file
    QFile file( fileName );
    if ( !file.open(QIODevice::ReadOnly) ) {
        setError( FatalError, "Cannot open file " + fileName );
        return false;
    }

    // Create a text stream
    QTextStream stream( &file );
    if ( stream.atEnd() ) {
        kDebug() << "Empty file" << fileName;
        if ( minimalRecordCount == 0 ) {
            return true;
        } else {
            setError( FatalError, "Empty file " + fileName );
            return false;
        }
    }

    // Open the database
    QSqlDatabase db = QSqlDatabase::database();
    if ( !db.isValid() ) {
        setError( FatalError, "Can not open database" );
        return false;
    }

    const QString tableName = QFileInfo(file).baseName();
    kDebug() << "Read GTFS data for table" << tableName;

    // Read first line from file (header with used field names)
    QStringList fieldNames;
    if ( stream.atEnd() || !readHeader(stream.readLine(), &fieldNames, requiredFields) ) {
        return false; // Error in header
    }
    // Get types of the fields
    QList<GeneralTransitFeedDatabase::FieldType> fieldTypes;
    foreach ( const QString &fieldName, fieldNames ) {
        fieldTypes << GeneralTransitFeedDatabase::typeOfField( fieldName );
    }

    QStringList availableFields;
    QSqlRecord table = db.record( tableName );
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
        kDebug() << "Not all used fields are available in the database:" << unavailableFieldNames;
    }

    // Performance optimization
    QSqlQuery query;
    if( !query.exec("PRAGMA synchronous=OFF;") ) {
        qDebug() << query.lastError();
    }
    if( !query.exec("PRAGMA journal_mode=WAL;") ) {
        qDebug() << query.lastError();
    }

    // Begin transaction
    if ( !db.driver()->beginTransaction() ) {
        qDebug() << db.lastError();
    }

    QStringList dbFieldNames = fieldNames;
    if ( tableName == "calendar" ) {
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

    // Prepare an INSERT query to be used for each dataset to be inserted
    query.prepare( QString("INSERT OR REPLACE INTO %1 (%2) VALUES (%3)")
                   .arg(tableName, dbFieldNames.join(","), placeholder) );

    int counter = 0;
    while ( !stream.atEnd() ) {
        QString line = stream.readLine();
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
            } else {
                foreach ( const QVariant &fieldValue, fieldValues ) {
                    query.addBindValue( fieldValue );
                }
            }
            if ( !query.exec() ) {
                kDebug() << query.lastError();
                kDebug() << "With this query:" << query.executedQuery();
                continue;
            }

            // New row has been inserted into the DB successfully
            ++counter;
            if ( counter > 0 && counter % 50000 == 0 ) {
                // Start a new transaction after 50000 INSERTs
                if ( !db.driver()->commitTransaction() ) {
                    qDebug() << db.lastError();
                }
                if ( !db.driver()->beginTransaction() ) {
                    qDebug() << db.lastError();
                }

                // Report progress TODO
//                 emit progress( qreal(file.pos()) / qreal(file.size()) );
                emit progress( qreal(totalFilePosition + file.pos()) / qreal(totalFileSize) );

                // Check if the job should be cancelled
                m_mutex.lock();
                if ( m_quit ) {
                    setError( FatalError, "Importer was cancelled" );
                    return false;
                }
                m_mutex.unlock();
            }
        } else {
            continue;
        }
    }
    file.close();

    // End transaction, restore synchronous=FULL
    if ( !db.driver()->commitTransaction() ) {
        qDebug() << db.lastError();
    }
    if( !query.exec("PRAGMA synchronous=FULL;") ) {
        qDebug() << query.lastError();
    }

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

bool GeneralTransitFeedImporter::readHeader( const QString &header, QStringList *fieldNames,
                                             const QStringList &requiredFields )
{
    *fieldNames = header.split(',');

    if ( fieldNames->isEmpty() ) {
        setError( FatalError, "No field names found in header: " + header );
        return false;
    }

    // Only allow alphanumerical characters as field names (and prevent SQL injection).
    foreach ( const QString &fieldName, *fieldNames ) {
        if ( fieldName.contains(QRegExp("[^A-Z0-9_]", Qt::CaseInsensitive)) ) {
            setError( FatalError, "Field name contains disallowed characters: " + fieldName );
            return false;
        }
    }

    // Check required fields
    foreach ( const QString &requiredField, requiredFields ) {
        if ( !fieldNames->contains(requiredField) ) {
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

bool GeneralTransitFeedImporter::readFields( const QString& line, QVariantList *fieldValues,
        const QList<GeneralTransitFeedDatabase::FieldType> &fieldTypes, int expectedFieldCount )
{
    int pos = 0;
    QList<GeneralTransitFeedDatabase::FieldType>::ConstIterator fieldType = fieldTypes.constBegin();
    while ( pos < line.length() && fieldType != fieldTypes.constEnd() ) {
        int endPos = pos;
        QString newField;
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
                kDebug() << "Didn't find field end, wrong file format";
                return false; // Didn't find field end, wrong file format
            }

            // Add field value without the quotation marks around it
            // and doubled quotation marks replaced with single ones
            newField = line.mid( pos + 1, endPos - pos - 1 )
                    .replace( QLatin1String("\"\""), QLatin1String("\"") );
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
        fieldValues->append( GeneralTransitFeedDatabase::convertFieldValue(newField, *fieldType) );
        ++fieldType;

        if ( pos == line.length() && line[pos - 1] == ',' ) {
            // The current line ends after a ','. Add another empty field:
            fieldValues->append( GeneralTransitFeedDatabase::convertFieldValue(QString(), *fieldType) );
            ++fieldType;
        }
    }

    if ( fieldValues->count() < expectedFieldCount ) {
        kDebug() << "Header contains" << expectedFieldCount << "fields, but a line was read with only"
                 << fieldValues->count() << "field values. Using empty/default values:";
        kDebug() << "Values: " << *fieldValues;
        while ( fieldValues->count() < expectedFieldCount ) {
            fieldValues->append( QVariant() );
        }
//         return false; Error is non-fatal
    }

    return true;
}

#include "generaltransitfeed_importer.moc"
