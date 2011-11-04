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

// Header
#include "publictransportdataengine.h"

// Own includes
#include "publictransportservice.h"
#include "timetableaccessor.h"
#include "timetableaccessor_info.h"
#include "timetableaccessor_generaltransitfeed.h"
#include "departureinfo.h"

// KDE includes
#include <KStandardDirs>

// Qt includes
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QTimer>

const int PublicTransportEngine::MIN_UPDATE_TIMEOUT = 120; // in seconds
const int PublicTransportEngine::MAX_UPDATE_TIMEOUT_DELAY = 5 * 60; // if delays are available
const int PublicTransportEngine::DEFAULT_TIME_OFFSET = 0;

Plasma::Service* PublicTransportEngine::serviceForSource( const QString &name )
{
    PublicTransportService *service = new PublicTransportService( name, this );
    service->setDestination( name );
    return service;
}

PublicTransportEngine::PublicTransportEngine( QObject* parent, const QVariantList& args )
        : Plasma::DataEngine( parent, args ),
        m_fileSystemWatcher(0), m_timer(0)
{
    // We ignore any arguments - data engines do not have much use for them
    Q_UNUSED( args )

    m_lastJourneyCount = 0;
    m_lastStopNameCount = 0;

    // This prevents applets from setting an unnecessarily high update interval
    // and using too much CPU.
    // 60 seconds should be enough, departure/arrival times have minute precision (except for GTFS,
    // but updating every 60 seconds is sufficient nevertheless).
    setMinimumPollingInterval( 60000 );
}

PublicTransportEngine::~PublicTransportEngine()
{
}

QHash< QString, QVariant > PublicTransportEngine::serviceProviderInfo(
        const TimetableAccessor *&accessor )
{
    return serviceProviderInfo( *accessor->info(), accessor );
}

QHash< QString, QVariant > PublicTransportEngine::serviceProviderInfo(
    const TimetableAccessorInfo &accessorInfo, const TimetableAccessor *accessor )
{
    QVariantHash dataServiceProvider;
    dataServiceProvider.insert( "id", accessorInfo.serviceProvider() );
    dataServiceProvider.insert( "type", TimetableAccessor::accessorTypeName(accessorInfo.accessorType()) );
    dataServiceProvider.insert( "fileName", accessorInfo.fileName() );
    dataServiceProvider.insert( "name", accessorInfo.name() );
    dataServiceProvider.insert( "url", accessorInfo.url() );
    dataServiceProvider.insert( "shortUrl", accessorInfo.shortUrl() );
    if ( accessorInfo.accessorType() == GtfsAccessor ) {
        dataServiceProvider.insert( "feedUrl", accessorInfo.feedUrl() );
        dataServiceProvider.insert( "gtfsDatabaseSize",
                QFileInfo(GeneralTransitFeedDatabase::databasePath(accessorInfo.serviceProvider()))
                .size() );
    } else {
        dataServiceProvider.insert( "scriptFileName", accessorInfo.scriptFileName() );
    }
    dataServiceProvider.insert( "country", accessorInfo.country() );
    dataServiceProvider.insert( "cities", accessorInfo.cities() );
    dataServiceProvider.insert( "credit", accessorInfo.credit() );
    dataServiceProvider.insert( "useSeparateCityValue", accessorInfo.useSeparateCityValue() );
    dataServiceProvider.insert( "onlyUseCitiesInList", accessorInfo.onlyUseCitiesInList() );
    dataServiceProvider.insert( "author", accessorInfo.author() );
    dataServiceProvider.insert( "shortAuthor", accessorInfo.shortAuthor() );
    dataServiceProvider.insert( "email", accessorInfo.email() );
    dataServiceProvider.insert( "description", accessorInfo.description() );
    dataServiceProvider.insert( "version", accessorInfo.version() );

    if ( accessor ) {
        kDebug() << "Use given accessor to get feature info";
        dataServiceProvider.insert( "features", accessor->features() );
        dataServiceProvider.insert( "featuresLocalized", accessor->featuresLocalized() );
    } else {
        const QString fileName = TimetableAccessor::accessorCacheFileName();
        const bool cacheExists = QFile::exists( fileName );
        bool dataFound = false;
        if ( cacheExists ) {
//             TODO compare last changed value, maybe the accessor was changed to have more (or less) features
            KConfig cfg( fileName, KConfig::SimpleConfig );
            KConfigGroup grp = cfg.group( accessorInfo.serviceProvider() );
            QStringList features = grp.readEntry("features", QStringList());
            if ( !features.isEmpty() ) {
                dataServiceProvider.insert( "features", features );
                dataServiceProvider.insert( "featuresLocalized",
                                            TimetableAccessor::localizedFeatureNames(features) );
                dataFound = true;
            }
        }

        if ( !dataFound ) {
            // No cached feature data was found for the accessor, create the accessor to get the
            // feature list. Caching the feature data is up to the accessor.
            TimetableAccessor *_accessor = getSpecificAccessor( accessorInfo.serviceProvider() );
            dataServiceProvider.insert( "features", _accessor->features() );
            dataServiceProvider.insert( "featuresLocalized", _accessor->featuresLocalized() );
        }
    }

    QStringList changelog;
    foreach ( const ChangelogEntry &entry, accessorInfo.changelog() ) {
        changelog << QString( "%2 (%1): %3" )
                .arg( entry.since_version ).arg( entry.author ).arg( entry.description );
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
                        TimetableAccessor::defaultServiceProviderXmlPathForLocation( location, dirs );

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
    return updateSourceEvent( name );
}

bool PublicTransportEngine::updateServiceProviderForCountrySource( const QString& name )
{
    QString serviceProvider;
    if ( name.contains('_') ) {
        // Seems that a service provider ID is given
        QStringList s = name.split( ' ', QString::SkipEmptyParts );
        if ( s.count() < 2 ) {
            return false;
        }

        serviceProvider = s[1];
    } else {
        // Assume a country code in name
        if ( !updateServiceProviderSource() || !updateLocationSource() ) {
            return false;
        }

        // name is expected to contain two words, the first is the ServiceProvider keyword, the
        // second is the location (ie. "international" or a two letter country code).
        QStringList s = name.split( ' ', QString::SkipEmptyParts );
        if ( s.count() < 2 ) {
            // No location found in name
            return false;
        }

        // Get the ID of the default service provider for the given location
        serviceProvider = TimetableAccessor::defaultServiceProviderIdForLocation( s[1].toLower() );
        if ( serviceProvider.isEmpty() ) {
            return false;
        }
    }

    // Try to get information about the accessor
    if ( m_accessors.contains(serviceProvider) ) {
        const TimetableAccessorInfo *accessorInfo = m_accessors.value( serviceProvider )->info();
        setData( name, serviceProviderInfo(*accessorInfo) );
    } else {
        const TimetableAccessorInfo *accessorInfo =
                TimetableAccessor::readAccessorInfo( serviceProvider );
        if ( accessorInfo ) {
            setData( name, serviceProviderInfo(*accessorInfo) );
            delete accessorInfo;
        } else {
            if ( !m_erroneousAccessors.contains( serviceProvider) ) {
                m_erroneousAccessors << serviceProvider;
            }
            return false;
        }
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
            m_fileSystemWatcher = new QFileSystemWatcher( dirList, this );
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

            // Remove file extension
            QString serviceProvider = KUrl( fileName ).fileName().remove( QRegExp( "\\..*$" ) );

            // Try to get information about the current service provider
            if ( m_accessors.contains(serviceProvider) ) {
                // The accessor is already created, use it's TimetableAccessorInfo object
                const TimetableAccessorInfo *accessorInfo = m_accessors.value( serviceProvider )->info();
                dataSource.insert( accessorInfo->name(), serviceProviderInfo(*accessorInfo) );
                loadedAccessors << serviceProvider;
            } else {
                // Check the accessor cache file for errors with the current service provider
                const QString fileName = TimetableAccessor::accessorCacheFileName();
                if ( QFile::exists(fileName) ) {
                    KConfig cfg( fileName, KConfig::SimpleConfig );
                    KConfigGroup grp = cfg.group( serviceProvider );
                    if ( grp.readEntry("hasErrors", false) ) {
                        m_erroneousAccessors << serviceProvider;
                        continue;
                    }
                }

                // The accessor is not created already, read it's XML file
                const TimetableAccessorInfo *accessorInfo =
                        TimetableAccessor::readAccessorInfo( serviceProvider );
                if ( accessorInfo ) {
                    dataSource.insert( accessorInfo->name(), serviceProviderInfo(*accessorInfo) );
                    loadedAccessors << serviceProvider;
                    delete accessorInfo;
                } else {
                    // The accessors XML file could not be read
                    m_erroneousAccessors << serviceProvider;
                    continue;
                }
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

bool PublicTransportEngine::updateErroneousServiceProviderSource()
{
    const QLatin1String name = sourceTypeKeyword( ErroneousServiceProviders );
    setData( name, "names", m_erroneousAccessors );
    return true;
}

bool PublicTransportEngine::updateLocationSource()
{
    const QLatin1String name = sourceTypeKeyword( Locations );
    QVariantHash dataSource;
    if ( m_dataSources.keys().contains(name) ) {
        dataSource = m_dataSources[name].toHash(); // locations already loaded
    } else {
        dataSource = locations();
    }
    m_dataSources.insert( name, dataSource );

    for ( QVariantHash::const_iterator it = dataSource.constBegin();
          it != dataSource.constEnd(); ++it )
    {
        setData( name, it.key(), it.value() );
    }

    return true;
}

bool PublicTransportEngine::updateTimetableSource( const QString &name )
{
    bool containsDataSource = m_dataSources.contains( name );
    if ( containsDataSource && isSourceUpToDate(name) ) { // Data is stored in the map and up to date
        kDebug() << "Data source" << name << "is up to date";
        QVariantHash dataSource = m_dataSources[name].toHash();
        for ( QVariantHash::const_iterator it = dataSource.constBegin();
                it != dataSource.constEnd(); ++it ) {
            setData( name, it.key(), it.value() );
        }
    } else { // Request new data
        if ( containsDataSource ) {
            m_dataSources.remove( name ); // Clear old data
        }

        ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals;
        QString serviceProvider, city, stop, targetStop, originStop, dataType;
        QDateTime dateTime;
        // Get 100 items by default to limit server requests (data is cached).
        // For fast results that are only needed once small numbers should be used.
        int maxCount = 100;

        SourceType sourceType = sourceTypeFromName( name );
        switch ( sourceType ) {
        case Departures:
            parseDocumentMode = ParseForDeparturesArrivals;
            dataType = "departures";
            break;
        case Arrivals:
            parseDocumentMode = ParseForDeparturesArrivals;
            dataType = "arrivals";
            break;
        case Stops:
            parseDocumentMode = ParseForStopSuggestions;
            dataType = "stopSuggestions";
            break;
        case JourneysDep:
        case Journeys:
            parseDocumentMode = ParseForJourneys;
            dataType = "journeysDep";
            break;
        case JourneysArr:
            parseDocumentMode = ParseForJourneys;
            dataType = "journeysArr";
            break;
        default:
            return false;
        }

        // Extract parameters, which follow after the source type keyword in name
        // and are delimited with '|'
        QStringList parameters = name.mid( QString(sourceTypeKeyword(sourceType)).length() )
                .trimmed().split( '|', QString::SkipEmptyParts );

        // Read parameters
        for ( int i = 0; i < parameters.length(); ++i ) {
            QString s = parameters.at( i );
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
            // No date/time value given, use default offset from now
            dateTime = QDateTime::currentDateTime().addSecs( DEFAULT_TIME_OFFSET * 60 );
        }

        if ( parseDocumentMode == ParseForDeparturesArrivals ||
             parseDocumentMode == ParseForStopSuggestions )
        {
            // Check if the stop name is missing
            if ( stop.isEmpty() ) {
                kDebug() << "Stop name is missing in data source name" << name;
                return false;
            }
        } else { // if ( parseDocumentMode == ParseForJourneys )
            if ( originStop.isEmpty() && !targetStop.isEmpty() ) {
                originStop = stop;
            } else if ( targetStop.isEmpty() && !originStop.isEmpty() ) {
                targetStop = stop;
            }
        }

        // Try to get the specific accessor
        TimetableAccessor *accessor = getSpecificAccessor( serviceProvider );
        if ( !accessor ) {
            // The accessor could not be loaded
            return false;
        }

        // Check if a city value is required but not available
        if ( accessor->info()->useSeparateCityValue() && city.isEmpty() ) {
            kDebug() << QString( "Accessor %1 needs a separate city value. Add to "
                                 "source name '|city=X', where X stands for the city "
                                 "name." ).arg( serviceProvider );
            return false; // accessor needs a separate city value
        }

        // Check if journeys get requested but not supported by the accessor
        if ( parseDocumentMode == ParseForJourneys &&
             !accessor->features().contains("JourneySearch") )
        {
            kDebug() << QString( "Accessor %1 doesn't support journey searches." )
                        .arg( serviceProvider );
            return false; // accessor doesn't support journey searches
        }

        // Start the request using the accessor for the service provider
        if ( parseDocumentMode == ParseForDeparturesArrivals ) {
            accessor->requestDepartures( DepartureRequestInfo(name, stop, dateTime, maxCount,
                                         dataType, false, city, parseDocumentMode) );
        } else if ( parseDocumentMode == ParseForStopSuggestions ) {
            accessor->requestStopSuggestions( StopSuggestionRequestInfo(name, stop, dateTime,
                                              maxCount, dataType, false, city, parseDocumentMode) );
        } else { // if ( parseDocumentMode == ParseForJourneys )
            accessor->requestJourneys( JourneyRequestInfo(name, originStop, targetStop,
                                       dateTime, maxCount, QString(), dataType) );
        }
    }

    return true;
}

TimetableAccessor* PublicTransportEngine::getSpecificAccessor( const QString &serviceProvider )
{
    // Try to get the specific accessor from m_accessors
    if ( !m_accessors.contains(serviceProvider) ) {
        // Accessor not already created, do it now
        TimetableAccessor *accessor = TimetableAccessor::createAccessor( serviceProvider, this );
        if ( !accessor ) {
            // Accessor could not be created
            kDebug() << QString( "Accessor %1 couldn't be created" ).arg( serviceProvider );
            return 0;
        }

        // Connect accessor signals
        connect( accessor, SIGNAL(departureListReceived(TimetableAccessor*,QUrl,QList<DepartureInfo*>,GlobalTimetableInfo,const RequestInfo*)),
                 this, SLOT(departureListReceived(TimetableAccessor*,QUrl,QList<DepartureInfo*>,GlobalTimetableInfo,const RequestInfo*)) );
        connect( accessor, SIGNAL(journeyListReceived(TimetableAccessor*,QUrl,QList<JourneyInfo*>,GlobalTimetableInfo,const RequestInfo*)),
                 this, SLOT(journeyListReceived(TimetableAccessor*,QUrl,QList<JourneyInfo*>,GlobalTimetableInfo,const RequestInfo*)) );
        connect( accessor, SIGNAL(stopListReceived(TimetableAccessor*,QUrl,QList<StopInfo*>,const RequestInfo*)),
                 this, SLOT(stopListReceived(TimetableAccessor*,QUrl,QList<StopInfo*>,const RequestInfo*)) );
        connect( accessor, SIGNAL(errorParsing(TimetableAccessor*,ErrorCode,QString,QUrl,const RequestInfo*)),
                 this, SLOT(errorParsing(TimetableAccessor*,ErrorCode,QString,QUrl,const RequestInfo*)) );
        connect( accessor, SIGNAL(progress(TimetableAccessor*,qreal,QString,QUrl,const RequestInfo*)),
                 this, SLOT(progress(TimetableAccessor*,qreal,QString,QUrl,const RequestInfo*)) );

        // Cache the accessor object and return it
        m_accessors.insert( serviceProvider, accessor );
        return accessor;
    } else {
        // Use already created accessor
        return m_accessors.value( serviceProvider );
    }
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
    if ( !m_timer ) {
        m_timer = new QTimer( this );
        connect( m_timer, SIGNAL(timeout()), this, SLOT(reloadAllAccessors()) );
    }

    m_timer->start( 250 );
}

void PublicTransportEngine::reloadAllAccessors()
{
    kDebug() << "Reload accessors (the contents of the accessor directory changed)";

    delete m_timer;
    m_timer = 0;

    // Remove all accessors (could have been changed)
    qDeleteAll( m_accessors );
    m_accessors.clear();

    // Clear all cached data (use the new accessor to parse the data again)
    QStringList cachedSources = m_dataSources.keys();
    foreach( const QString &cachedSource, cachedSources ) {
        SourceType sourceType = sourceTypeFromName( cachedSource );
        if ( isDataRequestingSourceType(sourceType) ) {
            m_dataSources.remove( cachedSource );
        }
    }

    // Remove cached service providers data source and update it
    m_dataSources.remove( sourceTypeKeyword(ServiceProviders) );
    updateServiceProviderSource();
}

const QLatin1String PublicTransportEngine::sourceTypeKeyword( SourceType sourceType )
{
    switch ( sourceType ) {
    case ServiceProvider:
        return QLatin1String("ServiceProvider");
    case ServiceProviders:
        return QLatin1String("ServiceProviders");
    case ErroneousServiceProviders:
        return QLatin1String("ErroneousServiceProviders");
    case Locations:
        return QLatin1String("Locations");
    case Departures:
        return QLatin1String("Departures");
    case Arrivals:
        return QLatin1String("Arrivals");
    case Stops:
        return QLatin1String("Stops");
    case Journeys:
        return QLatin1String("Journeys");
    case JourneysDep:
        return QLatin1String("JourneysDep");
    case JourneysArr:
        return QLatin1String("JourneysArr");

    default:
        return QLatin1String("");
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
        ret = updateErroneousServiceProviderSource();
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
        ret = updateTimetableSource( name );
        break;
    default:
        kDebug() << "Source name incorrect" << name;
        ret = false;
        break;
    }

    return ret;
}

void PublicTransportEngine::departureListReceived( TimetableAccessor *accessor,
        const QUrl &requestUrl, const QList<DepartureInfo*> &departures,
        const GlobalTimetableInfo &globalInfo, const RequestInfo *requestInfo )
{
    const QString sourceName = requestInfo->sourceName;
    kDebug() << departures.count() << "departures / arrivals received" << sourceName;

    int i = 0;
    m_dataSources.remove( sourceName );
    QVariantHash dataSource;
    foreach( DepartureInfo *departureInfo, departures ) {
        QVariantHash data;
        data.insert( "line", departureInfo->line() );
        data.insert( "target", departureInfo->target() );
        data.insert( "departure", departureInfo->departure() );
        data.insert( "vehicleType", static_cast<int>(departureInfo->vehicleType()) );
        data.insert( "vehicleIconName", Global::vehicleTypeToIcon(departureInfo->vehicleType()) );
        data.insert( "vehicleName", Global::vehicleTypeToString(departureInfo->vehicleType()) );
        data.insert( "vehicleNamePlural", Global::vehicleTypeToString(
                                          departureInfo->vehicleType(), true) );
        data.insert( "nightline", departureInfo->isNightLine() );
        data.insert( "expressline", departureInfo->isExpressLine() );
        data.insert( "delay", departureInfo->delay() );
        data.insert( "routeExactStops", departureInfo->routeExactStops() );
        if ( !departureInfo->platform().isEmpty() ) {
            data.insert( "platform", departureInfo->platform() );
        }
        if ( !departureInfo->delayReason().isEmpty() ) {
            data.insert( "delayReason", departureInfo->delayReason() );
        }
        if ( !departureInfo->journeyNews().isEmpty() ) {
            data.insert( "journeyNews", departureInfo->journeyNews() );
        }
        if ( !departureInfo->operatorName().isEmpty() ) {
            data.insert( "operator", departureInfo->operatorName() );
        }
        if ( !departureInfo->routeStops().isEmpty() ) {
            data.insert( "routeStops", departureInfo->routeStops() );
        }
        if ( !departureInfo->routeTimesVariant().isEmpty() ) {
            data.insert( "routeTimes", departureInfo->routeTimesVariant() );
        }
        if ( !departureInfo->pricing().isEmpty() ) {
            data.insert( "pricing", departureInfo->pricing() );
        }
        if ( !departureInfo->status().isEmpty() ) {
            data.insert( "status", departureInfo->status() );
        }

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
    qDeleteAll( departures );

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

    setData( sourceName, "serviceProvider", accessor->info()->serviceProvider() );
    setData( sourceName, "count", departureCount );
    setData( sourceName, "delayInfoAvailable", globalInfo.delayInfoAvailable );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", "departures" );
    setData( sourceName, "receivedData", "departures" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "serviceProvider", accessor->info()->serviceProvider() );
    dataSource.insert( "count", departureCount );
    dataSource.insert( "delayInfoAvailable", globalInfo.delayInfoAvailable );
    dataSource.insert( "requestUrl", requestUrl );
    dataSource.insert( "parseMode", "departures" );
    dataSource.insert( "receivedData", "departures" );
    dataSource.insert( "receivedPossibleStopList", false );
    dataSource.insert( "error", false );
    dataSource.insert( "updated", QDateTime::currentDateTime() );
    m_dataSources.insert( sourceName, dataSource );
}


void PublicTransportEngine::journeyListReceived( TimetableAccessor* accessor,
        const QUrl &requestUrl, const QList<JourneyInfo*> &journeys,
        const GlobalTimetableInfo &globalInfo, const RequestInfo *requestInfo )
{
    const QString sourceName = requestInfo->sourceName;
    kDebug() << journeys.count() << "journeys received" << sourceName;

    int i = 0;
    m_dataSources.remove( sourceName );
    QVariantHash dataSource;
    foreach( JourneyInfo *journeyInfo, journeys ) {
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
        data.insert( "operator", journeyInfo->operatorName() );
        data.insert( "routeExactStops", journeyInfo->routeExactStops() );
        if ( !journeyInfo->routeStops().isEmpty() ) {
            data.insert( "routeStops", journeyInfo->routeStops() );
        }
        if ( !journeyInfo->routeTimesDepartureVariant().isEmpty() ) {
            data.insert( "routeTimesDeparture", journeyInfo->routeTimesDepartureVariant() );
        }
        if ( !journeyInfo->routeTimesArrivalVariant().isEmpty() ) {
            data.insert( "routeTimesArrival", journeyInfo->routeTimesArrivalVariant() );
        }
        if ( !journeyInfo->routeVehicleTypesVariant().isEmpty() ) {
            data.insert( "routeVehicleTypes", journeyInfo->routeVehicleTypesVariant() );
        }
        if ( !journeyInfo->routeTransportLines().isEmpty() ) {
            data.insert( "routeTransportLines", journeyInfo->routeTransportLines() );
        }
        if ( !journeyInfo->routePlatformsDeparture().isEmpty() ) {
            data.insert( "routePlatformsDeparture", journeyInfo->routePlatformsDeparture() );
        }
        if ( !journeyInfo->routePlatformsArrival().isEmpty() ) {
            data.insert( "routePlatformsArrival", journeyInfo->routePlatformsArrival() );
        }
        if ( !journeyInfo->routeTimesDepartureDelay().isEmpty() ) {
            data.insert( "routeTimesDepartureDelay", journeyInfo->routeTimesDepartureDelay() );
        }
        if ( !journeyInfo->routeTimesArrivalDelay().isEmpty() ) {
            data.insert( "routeTimesArrivalDelay", journeyInfo->routeTimesArrivalDelay() );
        }

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
    qDeleteAll( journeys );

    // Remove old journeys
    for ( ; i < m_lastJourneyCount; ++i ) {
        removeData( sourceName, QString("%1").arg(i) );
    }
    m_lastJourneyCount = journeys.count();

    // Remove old stop suggestions
    for ( i = 0 ; i < m_lastStopNameCount; ++i ) {
        removeData( sourceName, QString("stopName %1").arg(i) );
    }
    m_lastStopNameCount = 0;

    // Store a proposal for the next download time
    int secs = ( journeyCount / 3 ) * first.secsTo( last );
    QDateTime downloadTime = QDateTime::currentDateTime().addSecs( secs );
    m_nextDownloadTimeProposals[ stripDateAndTimeValues(sourceName) ] = downloadTime;

    setData( sourceName, "serviceProvider", accessor->info()->serviceProvider() );
    setData( sourceName, "count", journeyCount );
    setData( sourceName, "delayInfoAvailable", globalInfo.delayInfoAvailable );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", "journeys" );
    setData( sourceName, "receivedData", "journeys" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "serviceProvider", accessor->info()->serviceProvider() );
    dataSource.insert( "count", journeyCount );
    dataSource.insert( "delayInfoAvailable", globalInfo.delayInfoAvailable );
    dataSource.insert( "requestUrl", requestUrl );
    dataSource.insert( "parseMode", "journeys" );
    dataSource.insert( "receivedData", "journeys" );
    dataSource.insert( "receivedPossibleStopList", false );
    dataSource.insert( "error", false );
    dataSource.insert( "updated", QDateTime::currentDateTime() );
    m_dataSources.insert( sourceName, dataSource );
}

void PublicTransportEngine::stopListReceived( TimetableAccessor *accessor,
        const QUrl &requestUrl, const QList<StopInfo*> &stops, const RequestInfo *requestInfo )
{
    const QString sourceName = requestInfo->sourceName;
    int i = 0;
    foreach( const StopInfo *stopInfo, stops ) {
        QVariantHash data;
        data.insert( "stopName", stopInfo->name() );

        if ( stopInfo->contains(StopID) &&
            (!accessor->info()->attributesForDepatures().contains(QLatin1String("requestStopIdFirst")) ||
            accessor->info()->attributesForDepatures()[QLatin1String("requestStopIdFirst")] == "false") )
        {
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

        setData( sourceName, QString( "stopName %1" ).arg( i++ ), data );
    }

    // Remove values from an old possible stop list
    for ( i = stops.count(); i < m_lastStopNameCount; ++i ) {
        removeData( sourceName, QString( "stopName %1" ).arg( i ) );
    }
    m_lastStopNameCount = stops.count();

    setData( sourceName, "serviceProvider", accessor->info()->serviceProvider() );
    setData( sourceName, "count", stops.count() );
    setData( sourceName, "requestUrl", requestUrl );
    if ( requestInfo->parseMode == ParseForDeparturesArrivals ) {
        setData( sourceName, "parseMode", "departures" );
    } else if ( requestInfo->parseMode == ParseForJourneys ) {
        setData( sourceName, "parseMode", "journeys" );
    } else if ( requestInfo->parseMode == ParseForStopSuggestions ) {
        setData( sourceName, "parseMode", "stopSuggestions" );
    }
    setData( sourceName, "receivedData", "stopList" );
    setData( sourceName, "receivedPossibleStopList", true );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );
}

void PublicTransportEngine::errorParsing( TimetableAccessor *accessor,
        ErrorCode errorCode, const QString &errorString,
        const QUrl &requestUrl, const RequestInfo *requestInfo )
{
    const QString sourceName = requestInfo->sourceName;
    kDebug() << "Error while parsing" << requestUrl << accessor->info()->serviceProvider()
             << "\n  sourceName =" << requestInfo->sourceName << requestInfo->dataType
             << requestInfo->parseMode;
    kDebug() << errorCode << errorString;

    setData( sourceName, "serviceProvider", accessor->info()->serviceProvider() );
    setData( sourceName, "count", 0 );
    setData( sourceName, "requestUrl", requestUrl );
    if ( requestInfo->parseMode == ParseForDeparturesArrivals ) {
        setData( sourceName, "parseMode", "departures" );
    } else if ( requestInfo->parseMode == ParseForJourneys ) {
        setData( sourceName, "parseMode", "journeys" );
    } else if ( requestInfo->parseMode == ParseForStopSuggestions ) {
        setData( sourceName, "parseMode", "stopSuggestions" );
    }
    setData( sourceName, "receivedData", "nothing" );
    setData( sourceName, "error", true );
    setData( sourceName, "errorCode", errorCode );
    setData( sourceName, "errorString", errorString );
    setData( sourceName, "updated", QDateTime::currentDateTime() );
}

void PublicTransportEngine::progress( TimetableAccessor *accessor, qreal progress,
        const QString &jobDescription, const QUrl &requestUrl, const RequestInfo *requestInfo )
{
    const QString sourceName = requestInfo->sourceName;
    setData( sourceName, "serviceProvider", accessor->info()->serviceProvider() );
    setData( sourceName, "count", 0 );
    setData( sourceName, "progress", progress );
    setData( sourceName, "jobDescription", jobDescription );
    setData( sourceName, "requestUrl", requestUrl );
    if ( requestInfo->parseMode == ParseForDeparturesArrivals ) {
        setData( sourceName, "parseMode", "departures" );
    } else if ( requestInfo->parseMode == ParseForJourneys ) {
        setData( sourceName, "parseMode", "journeys" );
    } else if ( requestInfo->parseMode == ParseForStopSuggestions ) {
        setData( sourceName, "parseMode", "stopSuggestions" );
    }
    setData( sourceName, "receivedData", "nothing" );
    setData( sourceName, "updated", QDateTime::currentDateTime() );
}

bool PublicTransportEngine::isSourceUpToDate( const QString& name )
{
    if ( !m_dataSources.contains(name) ) {
        return false;
    }

    // Data source stays up to date for max(UPDATE_TIMEOUT, minFetchWait) seconds
    const QVariantHash dataSource = m_dataSources[ name ].toHash();

    const QString serviceProvider = dataSource[ "serviceProvider" ].toString();
    TimetableAccessor *accessor = getSpecificAccessor( serviceProvider );

    QDateTime downloadTime = m_nextDownloadTimeProposals[ stripDateAndTimeValues( name )];
    int minForSufficientChanges = downloadTime.isValid()
            ? QDateTime::currentDateTime().secsTo( downloadTime ) : 0;
    int minFetchWait;
    const int secsSinceLastUpdate = dataSource["updated"].toDateTime().secsTo(
                                    QDateTime::currentDateTime() );
    if ( accessor->type() == GtfsAccessor ) {
        // Update GTFS accessors once a week
        // TODO: Check for an updated GTFS feed every X seconds, eg. once an hour
        TimetableAccessorGeneralTransitFeed *gtfsAccessor =
                qobject_cast<TimetableAccessorGeneralTransitFeed*>( accessor );
        kDebug() << "Wait time until next update from GTFS accessor:"
                 << KGlobal::locale()->prettyFormatDuration(1000 *
                    (gtfsAccessor->isRealtimeDataAvailable()
                     ? 60 - secsSinceLastUpdate : 60 * 60 * 24 - secsSinceLastUpdate));
        return gtfsAccessor->isRealtimeDataAvailable()
                ? secsSinceLastUpdate < 60 // Update maximally once a minute if realtime data is available
                : secsSinceLastUpdate < 60 * 60 * 24; // Update GTFS feed once a day without realtime data
    } else {
        // If delays are available set maximum fetch wait
        if ( accessor->features().contains("Delay") && dataSource["delayInfoAvailable"].toBool() ) {
            minFetchWait = qBound((int)MIN_UPDATE_TIMEOUT, minForSufficientChanges,
                                (int)MAX_UPDATE_TIMEOUT_DELAY );
        } else {
            minFetchWait = qMax( minForSufficientChanges, MIN_UPDATE_TIMEOUT );
        }

        minFetchWait = qMax( minFetchWait, accessor->info()->minFetchWait() );
        kDebug() << "Wait time until next download:"
                 << ((minFetchWait - secsSinceLastUpdate) / 60) << "min";

        return secsSinceLastUpdate < minFetchWait;
    }
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE( publictransport, PublicTransportEngine )

// this is needed since PublicTransportEngine is a QObject
#include "build/publictransportdataengine.moc"
