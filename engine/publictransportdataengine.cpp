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

// Header
#include "publictransportdataengine.h"

// Own includes
#include "timetableaccessor.h"
#include "timetableaccessor_info.h"
#include "global.h"
#include "request.h"

// KDE/Plasma includes
#include <Plasma/DataContainer>
#include <KStandardDirs>

// Qt includes
#include <QFileSystemWatcher>
#include <QFileInfo>

const int PublicTransportEngine::MIN_UPDATE_TIMEOUT = 120; // in seconds
const int PublicTransportEngine::MAX_UPDATE_TIMEOUT_DELAY = 5 * 60; // if delays are available
const int PublicTransportEngine::DEFAULT_TIME_OFFSET = 0;

PublicTransportEngine::PublicTransportEngine( QObject* parent, const QVariantList& args )
        : Plasma::DataEngine( parent, args ),
        m_fileSystemWatcher(0), m_accessorUpdateDelayTimer(0), m_sourceUpdateTimer(0)
{
    // We ignore any arguments - data engines do not have much use for them
    Q_UNUSED( args )

    m_lastJourneyCount = 0;
    m_lastStopNameCount = 0;

    // This prevents applets from setting an unnecessarily high update interval
    // and using too much CPU.
    // 60 seconds should be enough, departure / arrival times have minute precision (except for GTFS).
    setMinimumPollingInterval( 60000 );

    // Add service provider source, so when using
    // dataEngine("publictransport").sources() in an applet it at least returns this
//     setData( sourceTypeKeyword(ServiceProviders), DataEngine::Data() );
//     setData( sourceTypeKeyword(ErroneousServiceProviders), DataEngine::Data() );
//     setData( sourceTypeKeyword(Locations), DataEngine::Data() );
//     TODO return these source names in the virtual method ...

    connect( this, SIGNAL(sourceRemoved(QString)), this, SLOT(slotSourceRemoved(QString)) );
}

PublicTransportEngine::~PublicTransportEngine()
{
    qDeleteAll( m_accessors.values() );
    delete m_fileSystemWatcher;
}

void PublicTransportEngine::slotSourceRemoved( const QString& name )
{
    kDebug() << "____________________________________________________________________________";
    kDebug() << "Source" << name << "removed";

    const QString nonAmbiguousName = name.toLower();
    kDebug() << "Running" << m_runningSources.removeOne( nonAmbiguousName );
    kDebug() << "Cached" << m_dataSources.remove( nonAmbiguousName );
    kDebug() << "Still cached data sources" << m_dataSources.count();

}

QHash< QString, QVariant > PublicTransportEngine::serviceProviderInfo(
    const TimetableAccessor *&accessor )
{
    Q_ASSERT( accessor != NULL );

    QVariantHash dataServiceProvider;
    dataServiceProvider.insert( "id", accessor->serviceProvider() );
    dataServiceProvider.insert( "fileName", accessor->timetableAccessorInfo().fileName() );
    dataServiceProvider.insert( "scriptFileName", accessor->timetableAccessorInfo().scriptFileName() );
    dataServiceProvider.insert( "name", accessor->timetableAccessorInfo().name() );
    dataServiceProvider.insert( "url", accessor->timetableAccessorInfo().url() );
    dataServiceProvider.insert( "shortUrl", accessor->timetableAccessorInfo().shortUrl() );
    dataServiceProvider.insert( "country", accessor->country() );
    dataServiceProvider.insert( "cities", accessor->cities() );
    dataServiceProvider.insert( "credit", accessor->credit() );
    dataServiceProvider.insert( "useSeparateCityValue", accessor->useSeparateCityValue() );
    dataServiceProvider.insert( "onlyUseCitiesInList", accessor->onlyUseCitiesInList() );
    dataServiceProvider.insert( "features", accessor->features() );
    dataServiceProvider.insert( "featuresLocalized", accessor->featuresLocalized() );
    dataServiceProvider.insert( "author", accessor->timetableAccessorInfo().author() );
    dataServiceProvider.insert( "shortAuthor", accessor->timetableAccessorInfo().shortAuthor() );
    dataServiceProvider.insert( "email", accessor->timetableAccessorInfo().email() );
    dataServiceProvider.insert( "description", accessor->timetableAccessorInfo().description() );
    dataServiceProvider.insert( "version", accessor->timetableAccessorInfo().version() );

    QStringList changelog;
    foreach ( const ChangelogEntry &entry, accessor->timetableAccessorInfo().changelog() ) {
        changelog << QString( "%2 (%1): %3" ).arg( entry.version ).arg( entry.author ).arg( entry.description );
    }
    dataServiceProvider.insert( "changelog", changelog );

    return dataServiceProvider;
}

QHash< QString, QVariant > PublicTransportEngine::locations()
{
    QVariantHash ret;
    const QStringList accessors = KGlobal::dirs()->findAllResources( "data",
            "plasma_engine_publictransport/accessorInfos/*_*.xml" );
    const QStringList dirs = KGlobal::dirs()->findDirs( "data",
            "plasma_engine_publictransport/accessorInfos" );

    // Update ServiceProviders source to fill m_erroneousAccessors
    updateServiceProviderSource();

    foreach( const QString &accessor, accessors ) {
        if ( QFileInfo(accessor).isSymLink() ) {
            // Accessor XML file is a symlink for a default accessor, skip it
            continue;
        }

        const QString accessorFileName = QFileInfo( accessor ).fileName();
        const QString accessorId =
                TimetableAccessor::serviceProviderIdFromFileName( accessorFileName );
        if ( m_erroneousAccessors.contains(accessorId) ) {
            // Accessor is erroneous
            continue;
        }

        const int pos = accessorFileName.indexOf('_');
        if ( pos > 0 ) {
            // Found an underscore (not the first character)
            // Cut location code from the accessors XML filename
            const QString location = accessorFileName.mid( 0, pos ).toLower();
            if ( !ret.contains(location) ) {
                // Location is not already added to [ret]
                // Get the filename of the default accessor for the current location
                const QString defaultAccessorFileName =
                        TimetableAccessor::defaultServiceProviderForLocation( location, dirs );

                // Extract service provider ID from the filename
                const QString defaultAccessorId =
                        TimetableAccessor::serviceProviderIdFromFileName( defaultAccessorFileName );

                // Store location values in a hash and insert it into [ret]
                QVariantHash locationHash;
                locationHash.insert( "name", location );
                if ( location == "international" ) {
                    locationHash.insert( "description", i18n("International providers. "
                                         "There is one for getting flight departures/arrivals.") );
                } else {
                    locationHash.insert( "description", i18n("Service providers for %1.",
                            KGlobal::locale()->countryCodeToName(location)) );
                }
                locationHash.insert( "defaultAccessor", defaultAccessorId );
                ret.insert( location, locationHash );
            }
        }
    }

    return ret;
}

bool PublicTransportEngine::sourceRequestEvent( const QString &name )
{
    if ( isDataRequestingSourceType(sourceTypeFromName(name)) ) {
        setData( name, DataEngine::Data() ); // Create source, TODO: check if [name] is valid
    }

//     m_sourceUpdateTimer
//     TODO call forceImmediateUpdateOfAllVisualizations() after the data is available
    return updateSourceEvent( name );
}

bool PublicTransportEngine::updateServiceProviderForCountrySource( const QString& name )
{
    QString accessorId;
    if ( name.contains('_') ) {
        // Seems that a service provider ID is given
        QStringList s = name.split( ' ', QString::SkipEmptyParts );
        if ( s.count() < 2 ) {
            return false;
        }

        accessorId = s[1];
    } else {
        // Assume a country code in name
        if ( !updateServiceProviderSource() || !updateLocationSource() ) {
            return false;
        }

        QStringList s = name.split( ' ', QString::SkipEmptyParts );
        if ( s.count() < 2 ) {
            return false;
        }

        QString countryCode = s[1];
        QVariantHash locations = m_dataSources[ sourceTypeKeyword(Locations) ].toHash();
        QVariantHash locationCountry = locations[ countryCode.toLower() ].toHash();
        QString defaultAccessor = locationCountry[ "defaultAccessor" ].toString();
        if ( defaultAccessor.isEmpty() ) {
            return false;
        }

        accessorId = defaultAccessor;
    }

//     kDebug() << "Check accessor" << accessorId; // TODO
    const TimetableAccessor *accessor = TimetableAccessor::getSpecificAccessor( accessorId );
    if ( accessor ) {
        setData( name, serviceProviderInfo(accessor) );
        delete accessor;
    } else {
        if ( !m_erroneousAccessors.contains(accessorId) ) {
            m_erroneousAccessors << accessorId;
        }
        return false;
    }

    return true;
}

bool PublicTransportEngine::updateServiceProviderSource()
{
    const QString name = sourceTypeKeyword( ServiceProviders );
    QVariantHash dataSource;
    if ( m_dataSources.contains(name) ) {
        // kDebug() << "Data source" << name << "is up to date";
        dataSource = m_dataSources[ name ].toHash();
    } else {
        if ( !m_fileSystemWatcher ) {
            QStringList dirList = KGlobal::dirs()->findDirs( "data",
                                  "plasma_engine_publictransport/accessorInfos" );
            m_fileSystemWatcher = new QFileSystemWatcher( dirList );
            connect( m_fileSystemWatcher, SIGNAL(directoryChanged(QString)),
                     this, SLOT(accessorInfoDirChanged(QString)) );
        }

        QStringList fileNames = KGlobal::dirs()->findAllResources( "data",
                                "plasma_engine_publictransport/accessorInfos/*.xml" );
        if ( fileNames.isEmpty() ) {
            kDebug() << "Couldn't find any service provider information XML files";
            return false;
        }

        QStringList loadedAccessors;
        m_erroneousAccessors.clear();
        foreach( const QString &fileName, fileNames ) {
            if ( QFileInfo(fileName).isSymLink() && fileName.endsWith(QLatin1String("_default.xml")) ) {
                // Don't use symlinks to default service providers
                continue;
            }

            QString s = KUrl( fileName ).fileName().remove( QRegExp( "\\..*$" ) ); // Remove file extension
            const TimetableAccessor *accessor = TimetableAccessor::getSpecificAccessor( s );
            if ( accessor ) {
                dataSource.insert( accessor->timetableAccessorInfo().name(),
                                   serviceProviderInfo(accessor) );
                loadedAccessors << s;
                delete accessor;
            } else {
                m_erroneousAccessors << s;
            }
        }

        kDebug() << "Loaded" << loadedAccessors.count() << "accessors";
        if ( !m_erroneousAccessors.isEmpty() ) {
            kDebug() << "Erroneous accessor info XMLs, that couldn't be loaded:" << m_erroneousAccessors;
        }

        m_dataSources.insert( name, dataSource );
    }

    for ( QVariantHash::const_iterator it = dataSource.constBegin();
            it != dataSource.constEnd(); ++it ) {
        setData( name, it.key(), it.value() );
    }
    return true;
}

void PublicTransportEngine::updateErroneousServiceProviderSource( const QString &name )
{
    setData( name, "names", m_erroneousAccessors );
}

bool PublicTransportEngine::updateLocationSource()
{
    const QString name = sourceTypeKeyword( Locations );
    QVariantHash dataSource;
    if ( m_dataSources.keys().contains(name) ) {
        dataSource = m_dataSources[name].toHash(); // locations already loaded
    } else {
        dataSource = locations();
    }
    m_dataSources.insert( name, dataSource );

    for ( QVariantHash::const_iterator it = dataSource.constBegin();
            it != dataSource.constEnd(); ++it ) {
        setData( name, it.key(), it.value() );
    }

    return true;
}

bool PublicTransportEngine::updateDepartureOrJourneySource( const QString &name )
{
    const QString nonAmbiguousName = name.toLower();
    bool containsDataSource = m_dataSources.contains( nonAmbiguousName );
    if ( containsDataSource && isSourceUpToDate(nonAmbiguousName) ) { // Data is stored in the map and up to date
        kDebug() << "Data source" << name << "is up to date";
        QVariantHash dataSource = m_dataSources[nonAmbiguousName].toHash();
        for ( QVariantHash::const_iterator it = dataSource.constBegin();
                it != dataSource.constEnd(); ++it ) {
            setData( name, it.key(), it.value() );
        }
    } else if ( m_runningSources.contains(nonAmbiguousName) ) {
        // Source gets already processed
        kDebug() << "Source already gets processed, please wait" << name;
    } else { // Request new data
        if ( containsDataSource ) {
            m_dataSources.remove( nonAmbiguousName ); // Clear old data
        }

        QStringList input;
        ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals;
        QString city, stop, targetStop, originStop, dataType;
        QDateTime dateTime;
        // Get 100 items by default to limit server requests (data is cached).
        // For fast results that are only needed once small numbers should be used.
        int maxCount = 100;

        QString parameters;
        SourceType sourceType = sourceTypeFromName( name );
        if ( sourceType == Departures ) {
            parameters = name.mid( sourceTypeKeyword(Departures).length() );
            parseDocumentMode = ParseForDeparturesArrivals;
            dataType = "departures";
        } else if ( sourceType == Arrivals ) {
            parameters = name.mid( sourceTypeKeyword(Arrivals).length() );
            parseDocumentMode = ParseForDeparturesArrivals;
            dataType = "arrivals";
        } else if ( sourceType == Stops ) {
            parameters = name.mid( sourceTypeKeyword(Stops).length() );
            parseDocumentMode = ParseForStopSuggestions;
            dataType = "stopSuggestions";
        } else if ( sourceType == JourneysDep ) {
            parameters = name.mid( sourceTypeKeyword(JourneysDep).length() );
            parseDocumentMode = ParseForJourneys;
            dataType = "journeysDep";
        } else if ( sourceType == JourneysArr ) {
            parameters = name.mid( sourceTypeKeyword(JourneysArr).length() );
            parseDocumentMode = ParseForJourneys;
            dataType = "journeysArr";
        } else if ( sourceType == Journeys ) {
            parameters = name.mid( sourceTypeKeyword(Journeys).length() );
            parseDocumentMode = ParseForJourneys;
            dataType = "journeysDep";
        } else {
            kDebug() << "Unknown source type" << sourceType;
            return false;
        }

        input = parameters.trimmed().split( '|', QString::SkipEmptyParts );
        QString serviceProvider;

        for ( int i = 0; i < input.length(); ++i ) {
            QString s = input.at( i );
            if ( s.startsWith(QLatin1String("city="), Qt::CaseInsensitive) ) {
                city = s.mid( QString("city=").length() ).trimmed();
            } else if ( s.startsWith(QLatin1String("stop="), Qt::CaseInsensitive) ) {
                stop = s.mid( QString("stop=").length() ).trimmed();
            } else if ( s.startsWith(QLatin1String("targetStop="), Qt::CaseInsensitive) ) {
                targetStop = s.mid( QString("targetStop=").length() ).trimmed();
            } else if ( s.startsWith(QLatin1String("originStop="), Qt::CaseInsensitive) ) {
                originStop = s.mid( QString("originStop=").length() ).trimmed();
            } else if ( s.startsWith(QLatin1String("timeoffset="), Qt::CaseInsensitive) ) {
                s = s.mid( QString("timeoffset=").length() ).trimmed();
                dateTime = QDateTime::currentDateTime().addSecs( s.toInt() * 60 );
            } else if ( s.startsWith(QLatin1String("time="), Qt::CaseInsensitive) ) {
                s = s.mid( QString("time=").length() ).trimmed();
                dateTime = QDateTime( QDate::currentDate(), QTime::fromString(s, "hh:mm") );
            } else if ( s.startsWith(QLatin1String("datetime="), Qt::CaseInsensitive) ) {
                s = s.mid( QString("datetime=").length() ).trimmed();
                dateTime = QDateTime::fromString( s );
            } else if ( s.startsWith(QLatin1String("maxCount="), Qt::CaseInsensitive) ) {
                bool ok;
                maxCount = s.mid( QString("maxCount=").length() ).trimmed().toInt( &ok );
                if ( !ok ) {
                    kDebug() << "Bad value for 'maxCount' in source name:" << s;
                    maxCount = 100;
                }
            } else if ( !s.isEmpty() && s.indexOf( '=' ) == -1 ) {
                // No parameter name given, assume the service provider ID
                serviceProvider = s.trimmed();
            } else {
                kDebug() << "Unknown argument" << s;
            }
        }

        if ( dateTime.isNull() ) {
            dateTime = QDateTime::currentDateTime().addSecs( DEFAULT_TIME_OFFSET * 60 );
        }

        if ( parseDocumentMode == ParseForDeparturesArrivals
                || parseDocumentMode == ParseForStopSuggestions ) {
            if ( stop.isEmpty() ) {
                kDebug() << "Stop name is missing in data source name" << name;
                return false; // wrong input
            }
        } else {
            if ( originStop.isEmpty() && !targetStop.isEmpty() ) {
                originStop = stop;
            } else if ( targetStop.isEmpty() && !originStop.isEmpty() ) {
                targetStop = stop;
            }
        }

        // Try to get the specific accessor from m_accessors (if it's not in there it is created)
        bool newlyCreated = false;
        TimetableAccessor *accessor;
        if ( !m_accessors.contains(serviceProvider) ) {
            accessor = TimetableAccessor::getSpecificAccessor( serviceProvider );
            m_accessors.insert( serviceProvider, accessor );
            newlyCreated = true;
        } else {
            accessor = m_accessors.value( serviceProvider );
        }

        if ( !accessor ) {
            kDebug() << QString( "Accessor %1 couldn't be created" ).arg( serviceProvider );
            return false; // accessor couldn't be created
        } else if ( accessor->useSeparateCityValue() && city.isEmpty() ) {
            kDebug() << QString( "Accessor %1 needs a separate city value. Add to "
                                 "source name '|city=X', where X stands for the city "
                                 "name." ).arg( serviceProvider );
            return false; // accessor needs a separate city value
        } else if ( parseDocumentMode == ParseForJourneys
                    && !accessor->features().contains("JourneySearch") )
        {
            kDebug() << QString( "Accessor %1 doesn't support journey searches." )
                        .arg( serviceProvider );
            return false; // accessor doesn't support journey searches
        }

        // Store source name as currently being processed, to not start another
        // request if there is already a running one
        m_runningSources << nonAmbiguousName;

        if ( newlyCreated ) {
            connect( accessor, SIGNAL(departureListReceived(TimetableAccessor*,QUrl,DepartureInfoList,GlobalTimetableInfo,DepartureRequest)),
                     this, SLOT(departureListReceived(TimetableAccessor*,QUrl,DepartureInfoList,GlobalTimetableInfo,DepartureRequest)) );
            connect( accessor, SIGNAL(journeyListReceived(TimetableAccessor*,QUrl,JourneyInfoList,GlobalTimetableInfo,JourneyRequest)),
                     this, SLOT(journeyListReceived(TimetableAccessor*,QUrl,JourneyInfoList,GlobalTimetableInfo,JourneyRequest)) );
            connect( accessor, SIGNAL(stopListReceived(TimetableAccessor*,QUrl,StopInfoList,StopSuggestionRequest)),
                     this, SLOT(stopListReceived(TimetableAccessor*,QUrl,StopInfoList,StopSuggestionRequest)) );
            connect( accessor, SIGNAL(errorParsing(TimetableAccessor*,ErrorCode,QString,QUrl,const AbstractRequest*)),
                     this, SLOT(errorParsing(TimetableAccessor*,ErrorCode,QString,QUrl,const AbstractRequest*)) );
        }

        if ( parseDocumentMode == ParseForDeparturesArrivals ) {
            if ( dataType == "arrivals" ) {
                accessor->requestArrivals( ArrivalRequest(name, stop, dateTime, maxCount,
                                           city, dataType, parseDocumentMode) );
            } else {
                accessor->requestDepartures( DepartureRequest(name, stop, dateTime, maxCount,
                                             city, dataType, parseDocumentMode) );
            }
        } else if ( parseDocumentMode == ParseForStopSuggestions ) {
            accessor->requestStopSuggestions( StopSuggestionRequest(name, stop,
                                              maxCount, city, parseDocumentMode) );
        } else { // if ( parseDocumentMode == ParseForJourneys )
            accessor->requestJourneys( JourneyRequest(name, originStop, targetStop,
                                       dateTime, maxCount, QString(), QString(), dataType) );
        }
    }

    return true;
}

void PublicTransportEngine::forceUpdate()
{
    kDebug() << "FORCE UPDATE -------------------------------------------------------------------";
    forceImmediateUpdateOfAllVisualizations();
}

QString PublicTransportEngine::stripDateAndTimeValues( const QString& sourceName )
{
    QString ret = sourceName;
    QRegExp rx( "(time=[^\\|]*|datetime=[^\\|]*)", Qt::CaseInsensitive );
    rx.setMinimal( true );
    ret.replace( rx, QChar() );
    return ret;
}

void PublicTransportEngine::accessorInfoDirChanged( const QString &path )
{
    Q_UNUSED( path )

    // Use a timer to prevent loading all accessors again and again, for every changed file in a
    // possibly big list of files. It reloads the accessors maximally every 250ms.
    // Otherwise it can freeze plasma for a while if eg. all accessor files are changed at once.
    if ( !m_accessorUpdateDelayTimer ) {
        m_accessorUpdateDelayTimer = new QTimer( this );
        connect( m_accessorUpdateDelayTimer, SIGNAL(timeout()), this, SLOT(reloadAllAccessors()) );
    }

    m_accessorUpdateDelayTimer->start( 250 );
}

void PublicTransportEngine::reloadAllAccessors()
{
    kDebug() << "Reload accessors (the accessor dir changed)";

    delete m_accessorUpdateDelayTimer;
    m_accessorUpdateDelayTimer = NULL;

    // Remove all accessors (could have been changed)
    qDeleteAll( m_accessors );
    m_accessors.clear();

    // Clear all cached data (use the new accessor to parse the data again)
    // TODO: Only clear data for changed accessors (change modified time of a script or the XML)
    QStringList cachedSources = m_dataSources.keys();
    foreach( const QString &cachedSource, cachedSources ) {
        SourceType sourceType = sourceTypeFromName( cachedSource );
        if ( isDataRequestingSourceType(sourceType) ) {
            m_dataSources.remove( cachedSource );
        }
    }

    // Remove cached service provider source
    const QString serviceProvidersKey = sourceTypeKeyword( ServiceProviders );
    if ( m_dataSources.keys().contains(serviceProvidersKey) ) {
        m_dataSources.remove( serviceProvidersKey );
    }

    updateServiceProviderSource();
}

const QString PublicTransportEngine::sourceTypeKeyword( SourceType sourceType )
{
    switch ( sourceType ) {
    case ServiceProvider:
        return "ServiceProvider";
    case ServiceProviders:
        return "ServiceProviders";
    case ErroneousServiceProviders:
        return "ErroneousServiceProviders";
    case Locations:
        return "Locations";
    case Departures:
        return "Departures";
    case Arrivals:
        return "Arrivals";
    case Stops:
        return "Stops";
    case Journeys:
        return "Journeys";
    case JourneysDep:
        return "JourneysDep";
    case JourneysArr:
        return "JourneysArr";

    default:
        return "";
    }
}

PublicTransportEngine::SourceType PublicTransportEngine::sourceTypeFromName(
    const QString& sourceName ) const
{
    if ( sourceName.startsWith(sourceTypeKeyword(ServiceProvider) + ' ', Qt::CaseInsensitive) ) {
        return ServiceProvider;
    } else if ( sourceName.compare(sourceTypeKeyword(ServiceProviders), Qt::CaseInsensitive) == 0 ) {
        return ServiceProviders;
    } else if ( sourceName.compare(sourceTypeKeyword(ErroneousServiceProviders),
                                   Qt::CaseInsensitive) == 0 ) {
        return ErroneousServiceProviders;
    } else if ( sourceName.compare(sourceTypeKeyword(Locations), Qt::CaseInsensitive) == 0 ) {
        return Locations;
    } else if ( sourceName.startsWith(sourceTypeKeyword(Departures), Qt::CaseInsensitive) ) {
        return Departures;
    } else if ( sourceName.startsWith(sourceTypeKeyword(Arrivals), Qt::CaseInsensitive) ) {
        return Arrivals;
    } else if ( sourceName.startsWith(sourceTypeKeyword(Stops), Qt::CaseInsensitive) ) {
        return Stops;
    } else if ( sourceName.startsWith(sourceTypeKeyword(JourneysDep), Qt::CaseInsensitive) ) {
        return JourneysDep;
    } else if ( sourceName.startsWith(sourceTypeKeyword(JourneysArr), Qt::CaseInsensitive) ) {
        return JourneysArr;
    } else if ( sourceName.startsWith(sourceTypeKeyword(Journeys), Qt::CaseInsensitive) ) {
        return Journeys;
    } else {
        return InvalidSourceName;
    }
}

bool PublicTransportEngine::updateSourceEvent( const QString &name )
{
    bool ret = true;
    SourceType sourceType = sourceTypeFromName( name );
    switch ( sourceType ) {
    case ServiceProvider:
        ret = updateServiceProviderForCountrySource( name );
        break;
    case ServiceProviders:
        ret = updateServiceProviderSource();
        break;
    case ErroneousServiceProviders:
        updateErroneousServiceProviderSource( name );
        break;
    case Locations:
        ret = updateLocationSource();
        break;
    case Departures:
    case Arrivals:
    case Stops:
    case Journeys:
    case JourneysArr:
    case JourneysDep:
        ret = updateDepartureOrJourneySource( name );
        break;
    default:
        kDebug() << "Source name incorrect" << name;
        ret = false;
        break;
    }

    return ret;
}

void PublicTransportEngine::departureListReceived( TimetableAccessor *accessor,
        const QUrl &requestUrl, const DepartureInfoList &departures,
        const GlobalTimetableInfo &globalInfo, const DepartureRequest &request,
        bool deleteDepartureInfos )
{
    Q_UNUSED( accessor );
    const QString sourceName = request.sourceName;
    kDebug() << departures.count() << "departures / arrivals received" << sourceName;

    int i = 0;
    const QString nonAmbiguousName = sourceName.toLower();
    m_dataSources.remove( nonAmbiguousName );
    m_runningSources.removeOne( nonAmbiguousName );
    QVariantHash dataSource;
    foreach( const DepartureInfoPtr &departureInfo, departures ) {
//     if ( !departureInfo->isValid() ) {
//         kDebug() << "Departure isn't valid" << departureInfo->line();
//         continue;
//     }
        QVariantHash data;
        data.insert( "line", departureInfo->line() );
        data.insert( "target", departureInfo->target() );
        data.insert( "targetShortened",
                     departureInfo->target(PublicTransportInfo::UseShortenedStopNames) );
        data.insert( "departure", departureInfo->departure() );
        data.insert( "vehicleType", static_cast<int>( departureInfo->vehicleType() ) );
        data.insert( "vehicleIconName", Global::vehicleTypeToIcon(departureInfo->vehicleType()) );
        data.insert( "vehicleName", Global::vehicleTypeToString(departureInfo->vehicleType()) );
        data.insert( "vehicleNamePlural", Global::vehicleTypeToString(departureInfo->vehicleType(), true) );
        data.insert( "nightline", departureInfo->isNightLine() );
        data.insert( "expressline", departureInfo->isExpressLine() );
        data.insert( "platform", departureInfo->platform() );
        data.insert( "delay", departureInfo->delay() );
        data.insert( "delayReason", departureInfo->delayReason() );
        if ( !departureInfo->status().isEmpty() ) {
            data.insert( "status", departureInfo->status() );
        }
        data.insert( "journeyNews", departureInfo->journeyNews() );
        data.insert( "operator", departureInfo->operatorName() );
        data.insert( "routeStops", departureInfo->routeStops() );
        data.insert( "routeStopsShortened",
                     departureInfo->routeStops(PublicTransportInfo::UseShortenedStopNames) );
        data.insert( "routeTimes", departureInfo->routeTimesVariant() );
        data.insert( "routeExactStops", departureInfo->routeExactStops() );

        QString sKey = QString( "%1" ).arg( i );
        setData( sourceName, sKey, data );
        dataSource.insert( sKey, data );
        ++i;
    }
    int departureCount = departures.count();
    QDateTime first, last;
    if ( departureCount > 0 ) {
        first = departures.first()->departure();
        last = departures.last()->departure();
    } else {
        first = last = QDateTime::currentDateTime();
    }
    if ( deleteDepartureInfos ) {
        kDebug() << "Delete" << departures.count() << "departures";
//         qDeleteAll( departures );
    }

    // Remove old jouneys
    for ( ; i < m_lastJourneyCount; ++i ) {
        removeData( sourceName, QString( "%1" ).arg( i ) );
    }
    m_lastJourneyCount = departures.count();

    // Remove old stop suggestions
    for ( i = 0 ; i < m_lastStopNameCount; ++i ) {
        removeData( sourceName, QString( "stopName %1" ).arg( i ) );
    }
    m_lastStopNameCount = 0;

    // Store a proposal for the next download time
    int secs = QDateTime::currentDateTime().secsTo( last ) / 3;
    QDateTime downloadTime = QDateTime::currentDateTime().addSecs( secs );
    m_nextDownloadTimeProposals[ stripDateAndTimeValues( sourceName )] = downloadTime;
//     kDebug() << "Set next download time proposal:" << downloadTime;

//     setData( sourceName, "serviceProvider", serviceProvider );TODO
    setData( sourceName, "count", departureCount );
    setData( sourceName, "delayInfoAvailable", globalInfo.delayInfoAvailable );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", "departures" );
    setData( sourceName, "receivedData", "departures" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
//     dataSource.insert( "serviceProvider", serviceProvider );
    dataSource.insert( "count", departureCount );
    dataSource.insert( "delayInfoAvailable", globalInfo.delayInfoAvailable );
    dataSource.insert( "requestUrl", requestUrl );
    dataSource.insert( "parseMode", "departures" );
    dataSource.insert( "receivedData", "departures" );
    dataSource.insert( "receivedPossibleStopList", false );
    dataSource.insert( "error", false );
    dataSource.insert( "updated", QDateTime::currentDateTime() );
    m_dataSources.insert( sourceName.toLower(), dataSource );
}


void PublicTransportEngine::journeyListReceived( TimetableAccessor* accessor,
        const QUrl &requestUrl, const JourneyInfoList &journeys,
        const GlobalTimetableInfo &globalInfo,
        const JourneyRequest &request,
        bool deleteJourneyInfos )
{
    Q_UNUSED( accessor );
    const QString sourceName = request.sourceName;
    kDebug() << journeys.count() << "journeys received" << sourceName;

    int i = 0;
    const QString nonAmbiguousName = sourceName.toLower();
    m_dataSources.remove( nonAmbiguousName );
    m_runningSources.removeOne( nonAmbiguousName );
    QVariantHash dataSource;
    foreach( const JourneyInfoPtr &journeyInfo, journeys ) {
        if ( !journeyInfo->isValid() ) {
            continue;
        }

        QVariantHash data;
        data.insert( "vehicleTypes", journeyInfo->vehicleTypesVariant() );
        data.insert( "vehicleIconNames", journeyInfo->vehicleIconNames() );
        data.insert( "vehicleNames", journeyInfo->vehicleNames() );
        data.insert( "vehicleNamesPlural", journeyInfo->vehicleNames( true ) );
        data.insert( "arrival", journeyInfo->arrival() );
        data.insert( "departure", journeyInfo->departure() );
        data.insert( "duration", journeyInfo->duration() );
        data.insert( "changes", journeyInfo->changes() );
        data.insert( "pricing", journeyInfo->pricing() );
        data.insert( "journeyNews", journeyInfo->journeyNews() );
        data.insert( "startStopName", journeyInfo->startStopName() );
        data.insert( "targetStopName", journeyInfo->targetStopName() );
        data.insert( "Operator", journeyInfo->operatorName() );
        data.insert( "routeStops", journeyInfo->routeStops() );
        data.insert( "routeStopsShortened",
                     journeyInfo->routeStops(PublicTransportInfo::UseShortenedStopNames) );
        data.insert( "routeTimesDeparture", journeyInfo->routeTimesDepartureVariant() );
        data.insert( "routeTimesArrival", journeyInfo->routeTimesArrivalVariant() );
        data.insert( "routeExactStops", journeyInfo->routeExactStops() );
        data.insert( "routeVehicleTypes", journeyInfo->routeVehicleTypesVariant() );
        data.insert( "routeTransportLines", journeyInfo->routeTransportLines() );
        data.insert( "routePlatformsDeparture", journeyInfo->routePlatformsDeparture() );
        data.insert( "routePlatformsArrival", journeyInfo->routePlatformsArrival() );
        data.insert( "routeTimesDepartureDelay", journeyInfo->routeTimesDepartureDelay() );
        data.insert( "routeTimesArrivalDelay", journeyInfo->routeTimesArrivalDelay() );

        QString sKey = QString( "%1" ).arg( i++ );
        setData( sourceName, sKey, data );
        dataSource.insert( sKey, data );
        //     kDebug() << "setData" << sourceName << data;
    }
    int journeyCount = journeys.count();
    QDateTime first, last;
    if ( journeyCount > 0 ) {
        first = journeys.first()->departure();
        last = journeys.last()->departure();
    } else {
        first = last = QDateTime::currentDateTime();
    }
    if ( deleteJourneyInfos ) {
//         qDeleteAll( journeys ); TODO
    }

    // Remove old journeys
    for ( ; i < m_lastJourneyCount; ++i ) {
        removeData( sourceName, QString( "%1" ).arg( i ) );
    }
    m_lastJourneyCount = journeys.count();

    // Remove old stop suggestions
    for ( i = 0 ; i < m_lastStopNameCount; ++i ) {
        removeData( sourceName, QString( "stopName %1" ).arg( i ) );
    }
    m_lastStopNameCount = 0;

    // Store a proposal for the next download time
    int secs = ( journeyCount / 3 ) * first.secsTo( last );
    QDateTime downloadTime = QDateTime::currentDateTime().addSecs( secs );
    m_nextDownloadTimeProposals[ stripDateAndTimeValues( sourceName )] = downloadTime;

//     setData( sourceName, "serviceProvider", serviceProvider );TODO
    setData( sourceName, "count", journeyCount );
    setData( sourceName, "delayInfoAvailable", globalInfo.delayInfoAvailable );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", "journeys" );
    setData( sourceName, "receivedData", "journeys" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
//     dataSource.insert( "serviceProvider", serviceProvider );
    dataSource.insert( "count", journeyCount );
    dataSource.insert( "delayInfoAvailable", globalInfo.delayInfoAvailable );
    dataSource.insert( "requestUrl", requestUrl );
    dataSource.insert( "parseMode", "journeys" );
    dataSource.insert( "receivedData", "journeys" );
    dataSource.insert( "receivedPossibleStopList", false );
    dataSource.insert( "error", false );
    dataSource.insert( "updated", QDateTime::currentDateTime() );
    m_dataSources.insert( sourceName.toLower(), dataSource );
}

void PublicTransportEngine::stopListReceived( TimetableAccessor *accessor,
        const QUrl &requestUrl, const StopInfoList &stops,
        const StopSuggestionRequest &request, bool deleteStopInfos )
{
    Q_UNUSED( accessor );
    Q_UNUSED( deleteStopInfos );

    const QString sourceName = request.sourceName;
    m_runningSources.removeOne( sourceName );

    int i = 0;
    foreach( const StopInfoPtr &stopInfo, stops ) {
        QVariantHash data;
        data.insert( "stopName", stopInfo->name() );
        if ( stopInfo->contains(StopID) ) {
            data.insert( "stopID", stopInfo->id() );
        }

        if ( stopInfo->contains(StopWeight) ) {
            data.insert( "stopWeight", stopInfo->weight() );
        }

        if ( stopInfo->contains(StopCity) ) {
            data.insert( "stopCity", stopInfo->city() );
        }

        if ( stopInfo->contains(StopCountryCode) ) {
            data.insert( "stopCountryCode", stopInfo->countryCode() );
        }

        setData( sourceName, QString("stopName %1").arg(i++), data );
    }

    // Remove values from an old possible stop list
    for ( i = stops.count(); i < m_lastStopNameCount; ++i ) {
        removeData( sourceName, QString("stopName %1").arg(i) );
    }
    m_lastStopNameCount = stops.count();

//     setData( sourceName, "serviceProvider", serviceProvider );TODO
    setData( sourceName, "count", stops.count() );
    setData( sourceName, "requestUrl", requestUrl );
    if ( request.parseMode == ParseForDeparturesArrivals ) {
        setData( sourceName, "parseMode", "departures" );
    } else if ( request.parseMode == ParseForJourneys ) {
        setData( sourceName, "parseMode", "journeys" );
    } else if ( request.parseMode == ParseForStopSuggestions ) {
        setData( sourceName, "parseMode", "stopSuggestions" );
    }
    setData( sourceName, "receivedData", "stopList" );
    setData( sourceName, "receivedPossibleStopList", true );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

//     if ( deleteStopInfos ) {
//         qDeleteAll( stops ); TODO
//     }

    kDebug() << "DONE" << request.sourceName;
//     kDebug() << m_runningSources.count() << "running sources" << m_runningSources;
//     kDebug() << m_dataSources.count() << "data sources" << m_dataSources;

    // TODO: add to m_dataSources?
}

void PublicTransportEngine::errorParsing( TimetableAccessor *accessor,
        ErrorCode errorType, const QString &errorString,
        const QUrl &requestUrl, const AbstractRequest *request )
{
    Q_UNUSED( accessor );
    kDebug() << "Error while parsing" << requestUrl //<< request->serviceProvider
             << "\n  sourceName =" << request->sourceName << request->dataType << request->parseMode;
    kDebug() << errorType << errorString;

    // Remove erroneous source from running sources list
    m_runningSources.removeOne( request->sourceName );

    const QString sourceName = request->sourceName;
//     setData( sourceName, "serviceProvider", request->serviceProvider );
    setData( sourceName, "count", 0 );
    setData( sourceName, "requestUrl", requestUrl );
    if ( request->parseMode == ParseForDeparturesArrivals ) {
        setData( sourceName, "parseMode", "departures" );
    } else if ( request->parseMode == ParseForJourneys ) {
        setData( sourceName, "parseMode", "journeys" );
    } else if ( request->parseMode == ParseForStopSuggestions ) {
        setData( sourceName, "parseMode", "stopSuggestions" );
    }
    setData( sourceName, "receivedData", "nothing" );
    setData( sourceName, "error", true );
    setData( sourceName, "errorCode", errorType );
    setData( sourceName, "errorString", errorString );
    setData( sourceName, "updated", QDateTime::currentDateTime() );
}

bool PublicTransportEngine::isSourceUpToDate( const QString& name )
{
    const QString nonAmbiguousName = name.toLower();
    if ( !m_dataSources.contains(nonAmbiguousName.toLower()) ) {
        return false;
    }

    // Data source stays up to date for max(UPDATE_TIMEOUT, minFetchWait) seconds
    QVariantHash dataSource = m_dataSources[ nonAmbiguousName ].toHash();

    TimetableAccessor *accessor;
    QString serviceProvider = dataSource[ "serviceProvider" ].toString();
    if ( !m_accessors.contains(serviceProvider) ) {
        accessor = TimetableAccessor::getSpecificAccessor( serviceProvider );
        m_accessors.insert( serviceProvider, accessor );
    } else {
        accessor = m_accessors.value( serviceProvider );
    }

    QDateTime downloadTime = m_nextDownloadTimeProposals[ stripDateAndTimeValues(nonAmbiguousName) ];
    int minForSufficientChanges = downloadTime.isValid()
            ? QDateTime::currentDateTime().secsTo( downloadTime ) : 0;
    int minFetchWait;

    // If delays are available set maximum fetch wait
    if ( accessor->features().contains("Delay") && dataSource["delayInfoAvailable"].toBool() ) {
        minFetchWait = qBound((int)MIN_UPDATE_TIMEOUT, minForSufficientChanges,
                              (int)MAX_UPDATE_TIMEOUT_DELAY );
    } else {
        minFetchWait = qMax( minForSufficientChanges, MIN_UPDATE_TIMEOUT );
    }

    minFetchWait = qMax( minFetchWait, accessor->minFetchWait() );
    kDebug() << "Wait time until next download:"
             << ((minFetchWait - dataSource["updated"].toDateTime().secsTo(
                 QDateTime::currentDateTime())) / 60 ) << "min";

    return dataSource["updated"].toDateTime().secsTo(
            QDateTime::currentDateTime() ) < minFetchWait;
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE( publictransport, PublicTransportEngine )

// this is needed since PublicTransportEngine is a QObject
#include "build/publictransportdataengine.moc"
