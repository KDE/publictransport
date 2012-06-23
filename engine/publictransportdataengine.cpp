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
#include "serviceprovider.h"
#include "serviceproviderdata.h"
#include "serviceproviderglobal.h"
#include "serviceprovidergtfs.h"
#include "publictransportservice.h"
#include "global.h"
#include "request.h"

// KDE/Plasma includes
#include <Plasma/DataContainer>
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
        m_fileSystemWatcher(0), m_providerUpdateDelayTimer(0), m_sourceUpdateTimer(0)
{
    // We ignore any arguments - data engines do not have much use for them
    Q_UNUSED( args )

    m_lastJourneyCount = 0;
    m_lastStopNameCount = 0;

    // This prevents applets from setting an unnecessarily high update interval
    // and using too much CPU.
    // 60 seconds should be enough, departure / arrival times have minute precision (except for GTFS).
    setMinimumPollingInterval( 60000 );

    connect( this, SIGNAL(sourceRemoved(QString)), this, SLOT(slotSourceRemoved(QString)) );
}

PublicTransportEngine::~PublicTransportEngine()
{
    delete m_fileSystemWatcher;
}

QStringList PublicTransportEngine::sources() const
{
    QStringList sources = Plasma::DataEngine::sources();
    sources << sourceTypeKeyword(LocationsSource)
            << sourceTypeKeyword(ServiceProvidersSource)
            << sourceTypeKeyword(ErroneousServiceProvidersSource);
    sources.removeDuplicates();
    return sources;
}

bool PublicTransportEngine::isProviderUsed( const QString &serviceProviderId )
{
    for ( QVariantHash::ConstIterator it = m_dataSources.constBegin();
          it != m_dataSources.constEnd(); ++it )
    {
        const QVariantHash otherDataHash = it->toHash();
        if ( otherDataHash.contains("serviceProvider") ) {
            const QString otherProviderId = otherDataHash["serviceProvider"].toString().toLower();
            if ( otherProviderId == serviceProviderId ) {
                return true;
            }
        }
    }

    return false;
}

void PublicTransportEngine::slotSourceRemoved( const QString& name )
{
    const QString nonAmbiguousName = name.toLower();
    kDebug() << "Running" << m_runningSources.removeOne( nonAmbiguousName );

    const QVariant data = m_dataSources.take( nonAmbiguousName );
    kDebug() << "Cached" << data.isValid();
    kDebug() << "Source" << name << "removed, still cached data sources" << m_dataSources.count();

    // If a provider was used by the source, remove the provider if it is not used in another source
    const QVariantHash dataHash = data.toHash();
    if ( dataHash.contains("serviceProvider") ) {
        const QString providerId = dataHash["serviceProvider"].toString().toLower();
        if ( !providerId.isEmpty() && !isProviderUsed(providerId) ) {
            if ( m_providers.remove(providerId) > 0 ) {
                kDebug() << "Removed unused provider" << providerId;
            }
        }
    }
}

QVariantHash PublicTransportEngine::serviceProviderData( const ServiceProvider *provider )
{
    Q_ASSERT( provider );
    return serviceProviderData( *(provider->data()), provider );
}

QVariantHash PublicTransportEngine::serviceProviderData( const ServiceProviderData &data,
                                                         const ServiceProvider *provider )
{
    QVariantHash dataServiceProvider;
    dataServiceProvider.insert( "id", data.id() );
    dataServiceProvider.insert( "fileName", data.fileName() );
    dataServiceProvider.insert( "type", ServiceProvider::typeName(data.type()) );
    if ( data.type() == GtfsProvider ) {
        dataServiceProvider.insert( "feedUrl", data.feedUrl() );

        const QString databasePath = GeneralTransitFeedDatabase::databasePath( data.id() );
        dataServiceProvider.insert( "gtfsDatabasePath", databasePath );
        dataServiceProvider.insert( "gtfsDatabaseSize", QFileInfo(databasePath).size() );
    } else {
        dataServiceProvider.insert( "scriptFileName", data.scriptFileName() );
    }
    dataServiceProvider.insert( "name", data.name() );
    dataServiceProvider.insert( "url", data.url() );
    dataServiceProvider.insert( "shortUrl", data.shortUrl() );
    dataServiceProvider.insert( "country", data.country() );
    dataServiceProvider.insert( "cities", data.cities() );
    dataServiceProvider.insert( "credit", data.credit() );
    dataServiceProvider.insert( "useSeparateCityValue", data.useSeparateCityValue() );
    dataServiceProvider.insert( "onlyUseCitiesInList", data.onlyUseCitiesInList() );
//     dataServiceProvider.insert( "features", data.features() );
//     dataServiceProvider.insert( "featuresLocalized", data.featuresLocalized() );
    dataServiceProvider.insert( "author", data.author() );
    dataServiceProvider.insert( "shortAuthor", data.shortAuthor() );
    dataServiceProvider.insert( "email", data.email() );
    dataServiceProvider.insert( "description", data.description() );
    dataServiceProvider.insert( "version", data.version() );

    QStringList changelog;
    foreach ( const ChangelogEntry &entry, data.changelog() ) {
        changelog << QString( "%2 (%1): %3" ).arg( entry.version ).arg( entry.author ).arg( entry.description );
    }
    dataServiceProvider.insert( "changelog", changelog );

    // To get the list of features, ServiceProviderData is not enough
    // A given ServiceProvider or cached data gets used if available. Otherwise the ServiceProvider
    // gets created just to get the list of features
    if ( provider ) {
        kDebug() << "Use given provider to get feature info";
        dataServiceProvider.insert( "features", provider->features() );
        dataServiceProvider.insert( "featuresLocalized", provider->featuresLocalized() );
    } else {
        const QString fileName = ServiceProviderGlobal::cacheFileName();
        const bool cacheExists = QFile::exists( fileName );
        bool dataFound = false;
        if ( cacheExists ) {
            const KConfig config( fileName, KConfig::SimpleConfig );
            const KConfigGroup group = config.group( data.id() );
            const bool cacheFilled = group.readEntry("cacheFilled", false);
            if ( cacheFilled ) {
                const QStringList features = group.readEntry("features", QStringList());
                dataServiceProvider.insert( "features", features );
                dataServiceProvider.insert( "featuresLocalized",
                                            ServiceProvider::localizeFeatures(features) );
                dataFound = true;
            }
        }

        if ( !dataFound ) {
            kDebug() << "No cached feature data was found for provider" << data.id();
            // No cached feature data was found for the provider, create the provider to get the
            // feature list. Caching the feature data is up to the provider.
            bool newlyCreated;
            ProviderPointer _provider = providerFromId( data.id(), &newlyCreated );
            const QStringList features = _provider->features();
            const QStringList featuresLocalized = _provider->featuresLocalized();

            // Remove provider from the list again to delete it
            if ( newlyCreated ) {
                m_providers.remove( data.id() );
            }

            dataServiceProvider.insert( "features", features );
            dataServiceProvider.insert( "featuresLocalized", featuresLocalized );

            // Write features to cache
            KConfig config( fileName, KConfig::SimpleConfig );
            KConfigGroup group = config.group( data.id() );
            group.writeEntry( "features", features );
            group.writeEntry( "cacheFilled", true );
        }
    }

    return dataServiceProvider;
}

QVariantHash PublicTransportEngine::locations()
{
    QVariantHash ret;
    const QStringList providers = ServiceProviderGlobal::installedProviders();
    const QStringList dirs = KGlobal::dirs()->findDirs( "data",
            ServiceProviderGlobal::installationSubDirectory() );

    // Update ServiceProviders source to fill m_erroneousProviders
    updateServiceProviderSource();

    foreach( const QString &provider, providers ) {
        if ( QFileInfo(provider).isSymLink() ) {
            // Service provider XML file is a symlink for a default service provider, skip it
            continue;
        }

        const QString providerFileName = QFileInfo( provider ).fileName();
        const QString providerId = ServiceProviderGlobal::idFromFileName( providerFileName );
        if ( m_erroneousProviders.contains(providerId) ) {
            // Service provider is erroneous
            continue;
        }

        const int pos = providerFileName.indexOf('_');
        if ( pos > 0 ) {
            // Found an underscore (not the first character)
            // Cut location code from the service providers XML filename
            const QString location = providerFileName.mid( 0, pos ).toLower();
            if ( !ret.contains(location) ) {
                // Location is not already added to [ret]
                // Get the filename of the default provider for the current location
                const QString defaultProviderFileName =
                        ServiceProviderGlobal::defaultProviderForLocation( location, dirs );

                // Extract service provider ID from the filename
                const QString defaultProviderId =
                        ServiceProviderGlobal::idFromFileName( defaultProviderFileName );

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
                locationHash.insert( "defaultProvider", defaultProviderId );
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

PublicTransportEngine::ProviderPointer PublicTransportEngine::providerFromId(
        const QString &id, bool *newlyCreated )
{
    if ( m_providers.contains(id) ) {
        kDebug() << "Provider" << id << "already created";
        if ( newlyCreated ) {
            *newlyCreated = false;
        }
        return m_providers[ id ];
    } else {
        kDebug() << "Create provider" << id;
        if ( newlyCreated ) {
            *newlyCreated = true;
        }
        ServiceProvider *provider = ServiceProvider::createProvider( id );
        if ( !provider ) {
            return ProviderPointer::create();
        }

        connect( provider, SIGNAL(departureListReceived(ServiceProvider*,QUrl,DepartureInfoList,GlobalTimetableInfo,DepartureRequest)),
                 this, SLOT(departureListReceived(ServiceProvider*,QUrl,DepartureInfoList,GlobalTimetableInfo,DepartureRequest)) );
        connect( provider, SIGNAL(journeyListReceived(ServiceProvider*,QUrl,JourneyInfoList,GlobalTimetableInfo,JourneyRequest)),
                 this, SLOT(journeyListReceived(ServiceProvider*,QUrl,JourneyInfoList,GlobalTimetableInfo,JourneyRequest)) );
        connect( provider, SIGNAL(stopListReceived(ServiceProvider*,QUrl,StopInfoList,StopSuggestionRequest)),
                 this, SLOT(stopListReceived(ServiceProvider*,QUrl,StopInfoList,StopSuggestionRequest)) );
        connect( provider, SIGNAL(errorParsing(ServiceProvider*,ErrorCode,QString,QUrl,const AbstractRequest*)),
                 this, SLOT(errorParsing(ServiceProvider*,ErrorCode,QString,QUrl,const AbstractRequest*)) );

        const ProviderPointer pointer( provider );
        m_providers.insert( id, pointer );
        return pointer;
    }
}

bool PublicTransportEngine::updateServiceProviderForCountrySource( const QString& name )
{
    QString providerId;
    if ( name.contains('_') ) {
        // Seems that a service provider ID is given
        QStringList s = name.split( ' ', QString::SkipEmptyParts );
        if ( s.count() < 2 ) {
            return false;
        }

        providerId = s[1];
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

        QString countryCode = s[1];
        QVariantHash locations = m_dataSources[ sourceTypeKeyword(LocationsSource) ].toHash();
        QVariantHash locationCountry = locations[ countryCode.toLower() ].toHash();
        QString defaultProvider = locationCountry[ "defaultProvider" ].toString();
        if ( defaultProvider.isEmpty() ) {
            return false;
        }

        providerId = defaultProvider;
    }

    // Read provider data from the XML file
    QString errorMessage;
    ServiceProviderData *data = ServiceProvider::readProviderData( providerId, &errorMessage );
    if ( data ) {
        setData( name, serviceProviderData(*data) );
        setData( name, "error", false );
        delete data;
        return true;
    } else {
        setData( name, "error", true );
        setData( name, "errorMessage", errorMessage );
        if ( !m_erroneousProviders.contains(providerId) ) {
            m_erroneousProviders.insert( providerId, errorMessage );
        }
        return true;
    }
}

bool PublicTransportEngine::updateServiceProviderSource()
{
    const QString name = sourceTypeKeyword( ServiceProvidersSource );
    QVariantHash dataSource;
    if ( m_dataSources.contains(name) ) {
        // kDebug() << "Data source" << name << "is up to date";
        dataSource = m_dataSources[ name ].toHash();
    } else {
        if ( !m_fileSystemWatcher ) {
            const QStringList dirs = KGlobal::dirs()->findDirs( "data",
                    ServiceProviderGlobal::installationSubDirectory() );
            m_fileSystemWatcher = new QFileSystemWatcher( dirs );
            connect( m_fileSystemWatcher, SIGNAL(directoryChanged(QString)),
                     this, SLOT(serviceProviderDirChanged(QString)) );
        }

        const QStringList providers = ServiceProviderGlobal::installedProviders();
        if ( providers.isEmpty() ) {
            kDebug() << "Could not find any service provider plugins";
            return false;
        }

        QStringList loadedProviders;
        m_erroneousProviders.clear();
        foreach( const QString &provider, providers ) {
            const QFileInfo fileInfo( provider );
            if ( fileInfo.isSymLink() && fileInfo.baseName().endsWith(QLatin1String("_default")) ) {
                // Don't use symlinks to default service providers
                continue;
            }

            QString serviceProviderId =
                    KUrl( provider ).fileName().remove( QRegExp( "\\..*$" ) ); // Remove file extension
            // Try to get information about the current service provider
            if ( m_providers.contains(serviceProviderId) ) {
                // The accessor is already created, use it's ServiceProviderData object
                const ProviderPointer provider = m_providers[ serviceProviderId ];
                dataSource.insert( provider->data()->name(), serviceProviderData(provider.data()) );
                loadedProviders << serviceProviderId;
            } else {
                // Check the provider cache file for errors with the current service provider
                const KConfig config( ServiceProviderGlobal::cacheFileName(), KConfig::SimpleConfig );
                const KConfigGroup group = config.group( serviceProviderId );
                QString errorMessage;
                const ServiceProvider::SourceFileValidity validity =
                        ServiceProvider::sourceFileValidity( serviceProviderId, &errorMessage,
                                                             &config );
//                         static_cast<ServiceProvider::SourceFileValidity>(
//                         group.readEntry("validity", static_cast<int>(ServiceProvider::SourceFileValidityCheckPending)) );
                if ( validity == ServiceProvider::SourceFileIsInvalid ||
                     group.group("script").readEntry("errors", false) )
                {
                    if ( errorMessage.isEmpty() ) {
                        errorMessage = group.readEntry( "errorMessage", QString() );
                    }
                    m_erroneousProviders.insert( serviceProviderId, errorMessage );
                    continue;
                }
                errorMessage.clear();

                // The provider is not created already, read it's XML file
                const ServiceProviderData *data =
                        ServiceProvider::readProviderData( serviceProviderId, &errorMessage );
                if ( data ) {
                    dataSource.insert( data->name(), serviceProviderData(*data) );
                    loadedProviders << serviceProviderId;
                    delete data;
                } else {
                    // The providers XML file could not be read
                    if ( errorMessage.isEmpty() ) {
                        errorMessage = group.readEntry( "errorMessage", QString() );
                    }
                    m_erroneousProviders.insert( serviceProviderId, errorMessage  );
                    continue;
                }
            }
        }

        kDebug() << "Loaded" << loadedProviders.count() << "service providers";
        if ( !m_erroneousProviders.isEmpty() ) {
            kDebug() << "Erroneous service provider plugins, that couldn't be loaded:"
                     << m_erroneousProviders;
        }

        m_dataSources.insert( name, dataSource );
    }

    for ( QVariantHash::const_iterator it = dataSource.constBegin();
          it != dataSource.constEnd(); ++it )
    {
        setData( name, it.key(), it.value() );
    }
    return true;
}

bool PublicTransportEngine::updateErroneousServiceProviderSource()
{
    const QLatin1String name = sourceTypeKeyword( ErroneousServiceProvidersSource );
    setData( name, static_cast<Data>(m_erroneousProviders) );
    return true;
}

bool PublicTransportEngine::updateLocationSource()
{
    const QLatin1String name = sourceTypeKeyword( LocationsSource );
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

bool PublicTransportEngine::updateTimetableDataSource( const QString &name )
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

        ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals;
        QString serviceProviderId, city, stop, targetStop, originStop, dataType;
        QDateTime dateTime;
        // Get 100 items by default to limit server requests (data is cached).
        // For fast results that are only needed once small numbers should be used.
        int maxCount = 100;

        SourceType sourceType = sourceTypeFromName( name );
        switch ( sourceType ) {
        case DeparturesSource:
            parseDocumentMode = ParseForDeparturesArrivals;
            dataType = "departures";
            break;
        case ArrivalsSource:
            parseDocumentMode = ParseForDeparturesArrivals;
            dataType = "arrivals";
            break;
        case StopsSource:
            parseDocumentMode = ParseForStopSuggestions;
            dataType = "stopSuggestions";
            break;
        case JourneysDepSource:
            parseDocumentMode = ParseForJourneys;
            dataType = "journeysDep";
            break;
        case JourneysArrSource:
            parseDocumentMode = ParseForJourneys;
            dataType = "journeysArr";
            break;
        case JourneysSource:
            parseDocumentMode = ParseForJourneys;
            dataType = "journeysDep";
            break;
        default:
            kDebug() << "Unknown source type" << sourceType;
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
                serviceProviderId = s.trimmed();
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

        // Try to get the specific provider from m_providers (if it's not in there it is created)
        const ProviderPointer provider = providerFromId( serviceProviderId );
        if ( provider.isNull() ) {
            return false; // Service provider couldn't be created
        } else if ( provider->useSeparateCityValue() && city.isEmpty() ) {
            kDebug() << QString( "Service provider %1 needs a separate city value. Add to "
                                 "source name '|city=X', where X stands for the city "
                                 "name." ).arg( serviceProviderId );
            return false; // Service provider needs a separate city value
        } else if ( parseDocumentMode == ParseForJourneys
                    && !provider->features().contains("JourneySearch") )
        {
            kDebug() << QString( "Service provider %1 doesn't support journey searches." )
                        .arg( serviceProviderId );
            return false; // Service provider doesn't support journey searches
        }

        // Store source name as currently being processed, to not start another
        // request if there is already a running one
        m_runningSources << nonAmbiguousName;

        if ( parseDocumentMode == ParseForDeparturesArrivals ) {
            if ( dataType == "arrivals" ) {
                provider->requestArrivals( ArrivalRequest(name, stop, dateTime, maxCount,
                                           city, dataType, parseDocumentMode) );
            } else {
                provider->requestDepartures( DepartureRequest(name, stop, dateTime, maxCount,
                                             city, dataType, parseDocumentMode) );
            }
        } else if ( parseDocumentMode == ParseForStopSuggestions ) {
            provider->requestStopSuggestions( StopSuggestionRequest(name, stop,
                                              maxCount, city, parseDocumentMode) );
        } else { // if ( parseDocumentMode == ParseForJourneys )
            provider->requestJourneys( JourneyRequest(name, originStop, targetStop,
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

// TODO
// TimetableAccessor* PublicTransportEngine::getSpecificAccessor( const QString &serviceProvider )
// {
//     // Try to get the specific accessor from m_accessors
//     if ( !m_accessors.contains(serviceProvider) ) {
//         // Accessor not already created, do it now
//         TimetableAccessor *accessor = TimetableAccessor::createAccessor( serviceProvider, this );
//         if ( !accessor ) {
//             // Accessor could not be created
//             kDebug() << QString( "Accessor %1 couldn't be created" ).arg( serviceProvider );
//             return 0;
//         }
//
//         // Connect accessor signals
//         connect( accessor, SIGNAL(departureListReceived(TimetableAccessor*,QUrl,QList<DepartureInfo*>,GlobalTimetableInfo,const RequestInfo*)),
//                  this, SLOT(departureListReceived(TimetableAccessor*,QUrl,QList<DepartureInfo*>,GlobalTimetableInfo,const RequestInfo*)) );
//         connect( accessor, SIGNAL(journeyListReceived(TimetableAccessor*,QUrl,QList<JourneyInfo*>,GlobalTimetableInfo,const RequestInfo*)),
//                  this, SLOT(journeyListReceived(TimetableAccessor*,QUrl,QList<JourneyInfo*>,GlobalTimetableInfo,const RequestInfo*)) );
//         connect( accessor, SIGNAL(stopListReceived(TimetableAccessor*,QUrl,QList<StopInfo*>,const RequestInfo*)),
//                  this, SLOT(stopListReceived(TimetableAccessor*,QUrl,QList<StopInfo*>,const RequestInfo*)) );
//         connect( accessor, SIGNAL(errorParsing(TimetableAccessor*,ErrorCode,QString,QUrl,const RequestInfo*)),
//                  this, SLOT(errorParsing(TimetableAccessor*,ErrorCode,QString,QUrl,const RequestInfo*)) );
//         connect( accessor, SIGNAL(progress(TimetableAccessor*,qreal,QString,QUrl,const RequestInfo*)),
//                  this, SLOT(progress(TimetableAccessor*,qreal,QString,QUrl,const RequestInfo*)) );
//
//         // Cache the accessor object and return it
//         m_accessors.insert( serviceProvider, accessor );
//         return accessor;
//     } else {
//         // Use already created accessor
//         return m_accessors.value( serviceProvider );
//     }
// }

QString PublicTransportEngine::stripDateAndTimeValues( const QString& sourceName )
{
    QString ret = sourceName;
    QRegExp rx( "(time=[^\\|]*|datetime=[^\\|]*)", Qt::CaseInsensitive );
    rx.setMinimal( true );
    ret.replace( rx, QChar() );
    return ret;
}

void PublicTransportEngine::serviceProviderDirChanged( const QString &path )
{
    Q_UNUSED( path )

    // Use a timer to prevent loading all service providers again and again, for every changed file
    // in a possibly big list of files. It reloads the providers maximally every 250ms.
    // Otherwise it can freeze plasma for a while if eg. all provider files are changed at once.
    if ( !m_providerUpdateDelayTimer ) {
        m_providerUpdateDelayTimer = new QTimer( this );
        connect( m_providerUpdateDelayTimer, SIGNAL(timeout()), this, SLOT(reloadAllProviders()) );
    }

    m_providerUpdateDelayTimer->start( 250 );
}

void PublicTransportEngine::reloadAllProviders()
{
    kDebug() << "Reload service providers (the service provider dir changed)";

    delete m_providerUpdateDelayTimer;
    m_providerUpdateDelayTimer = 0;

//     delete m_timer; TODO
//     m_timer = 0;

    // Remove all providers (could have been changed)
//     qDeleteAll( m_providers ); // TODO Remove
    m_providers.clear();

    // Clear all cached data (use the new provider to parse the data again)
    // TODO: Only clear data for changed providers (change modified time of a script or the XML)
    QStringList cachedSources = m_dataSources.keys();
    foreach( const QString &cachedSource, cachedSources ) {
        SourceType sourceType = sourceTypeFromName( cachedSource );
        if ( isDataRequestingSourceType(sourceType) ) {
            m_dataSources.remove( cachedSource );
        }
    }

    // Remove cached service provider source
    const QString serviceProvidersKey = sourceTypeKeyword( ServiceProvidersSource );
    if ( m_dataSources.keys().contains(serviceProvidersKey) ) {
        m_dataSources.remove( serviceProvidersKey );
    }

    updateServiceProviderSource();
}

const QLatin1String PublicTransportEngine::sourceTypeKeyword( SourceType sourceType )
{
    switch ( sourceType ) {
    case ServiceProviderSource:
        return QLatin1String("ServiceProvider");
    case ServiceProvidersSource:
        return QLatin1String("ServiceProviders");
    case ErroneousServiceProvidersSource:
        return QLatin1String("ErroneousServiceProviders");
    case LocationsSource:
        return QLatin1String("Locations");
    case DeparturesSource:
        return QLatin1String("Departures");
    case ArrivalsSource:
        return QLatin1String("Arrivals");
    case StopsSource:
        return QLatin1String("Stops");
    case JourneysSource:
        return QLatin1String("Journeys");
    case JourneysDepSource:
        return QLatin1String("JourneysDep");
    case JourneysArrSource:
        return QLatin1String("JourneysArr");

    default:
        return QLatin1String("");
    }
}

PublicTransportEngine::SourceType PublicTransportEngine::sourceTypeFromName(
    const QString& sourceName ) const
{
    if ( sourceName.startsWith(sourceTypeKeyword(ServiceProviderSource) + ' ', Qt::CaseInsensitive) ) {
        return ServiceProviderSource;
    } else if ( sourceName.compare(sourceTypeKeyword(ServiceProvidersSource), Qt::CaseInsensitive) == 0 ) {
        return ServiceProvidersSource;
    } else if ( sourceName.compare(sourceTypeKeyword(ErroneousServiceProvidersSource),
                                   Qt::CaseInsensitive) == 0 ) {
        return ErroneousServiceProvidersSource;
    } else if ( sourceName.compare(sourceTypeKeyword(LocationsSource), Qt::CaseInsensitive) == 0 ) {
        return LocationsSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(DeparturesSource), Qt::CaseInsensitive) ) {
        return DeparturesSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(ArrivalsSource), Qt::CaseInsensitive) ) {
        return ArrivalsSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(StopsSource), Qt::CaseInsensitive) ) {
        return StopsSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(JourneysDepSource), Qt::CaseInsensitive) ) {
        return JourneysDepSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(JourneysArrSource), Qt::CaseInsensitive) ) {
        return JourneysArrSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(JourneysSource), Qt::CaseInsensitive) ) {
        return JourneysSource;
    } else {
        return InvalidSourceName;
    }
}

bool PublicTransportEngine::updateSourceEvent( const QString &name )
{
    SourceType sourceType = sourceTypeFromName( name );
    switch ( sourceType ) {
    case ServiceProviderSource:
        return updateServiceProviderForCountrySource( name );
    case ServiceProvidersSource:
        return updateServiceProviderSource();
    case ErroneousServiceProvidersSource:
        return updateErroneousServiceProviderSource();
    case LocationsSource:
        return updateLocationSource();
    case DeparturesSource:
    case ArrivalsSource:
    case StopsSource:
    case JourneysSource:
    case JourneysArrSource:
    case JourneysDepSource:
        return updateTimetableDataSource( name );
    case InvalidSourceName:
    default:
        kDebug() << "Source name incorrect" << name;
        return false;
    }
}

void PublicTransportEngine::departureListReceived( ServiceProvider *provider,
        const QUrl &requestUrl, const DepartureInfoList &departures,
        const GlobalTimetableInfo &globalInfo, const DepartureRequest &request,
        bool deleteDepartureInfos )
{
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
        data.insert( "vehicleType", static_cast<int>(departureInfo->vehicleType()) );
        data.insert( "vehicleIconName", Global::vehicleTypeToIcon(departureInfo->vehicleType()) );
        data.insert( "vehicleName", Global::vehicleTypeToString(departureInfo->vehicleType()) );
        data.insert( "vehicleNamePlural", Global::vehicleTypeToString(departureInfo->vehicleType(), true) );
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
        removeData( sourceName, QString("%1").arg(i) );
    }
    m_lastJourneyCount = departures.count();

    // Remove old stop suggestions
    for ( i = 0 ; i < m_lastStopNameCount; ++i ) {
        removeData( sourceName, QString("stopName %1").arg(i) );
    }
    m_lastStopNameCount = 0;

    // Store a proposal for the next download time
    int secs = QDateTime::currentDateTime().secsTo( last ) / 3;
    QDateTime downloadTime = QDateTime::currentDateTime().addSecs( secs );
    m_nextDownloadTimeProposals[ stripDateAndTimeValues( sourceName )] = downloadTime;
//     kDebug() << "Set next download time proposal:" << downloadTime;

    setData( sourceName, "serviceProvider", provider->id() );
    setData( sourceName, "count", departureCount );
    setData( sourceName, "delayInfoAvailable", globalInfo.delayInfoAvailable );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", "departures" );
    setData( sourceName, "receivedData", "departures" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "serviceProvider", provider->id() );
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


void PublicTransportEngine::journeyListReceived( ServiceProvider* provider,
        const QUrl &requestUrl, const JourneyInfoList &journeys,
        const GlobalTimetableInfo &globalInfo,
        const JourneyRequest &request,
        bool deleteJourneyInfos )
{
    Q_UNUSED( provider );
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
        data.insert( "operator", journeyInfo->operatorName() );
        data.insert( "routeStops", journeyInfo->routeStops() );
        data.insert( "routeStopsShortened",
                     journeyInfo->routeStops(PublicTransportInfo::UseShortenedStopNames) );
        data.insert( "routeTimesDeparture", journeyInfo->routeTimesDepartureVariant() );
        data.insert( "routeTimesArrival", journeyInfo->routeTimesArrivalVariant() );
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
    if ( deleteJourneyInfos ) {
//         qDeleteAll( journeys ); TODO
    }

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

    setData( sourceName, "serviceProvider", provider->id() );
    setData( sourceName, "count", journeyCount );
    setData( sourceName, "delayInfoAvailable", globalInfo.delayInfoAvailable );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", "journeys" );
    setData( sourceName, "receivedData", "journeys" );
    setData( sourceName, "receivedPossibleStopList", false );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

    // Store received data in the data source map
    dataSource.insert( "serviceProvider", provider->id() );
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

void PublicTransportEngine::stopListReceived( ServiceProvider *provider,
        const QUrl &requestUrl, const StopInfoList &stops,
        const StopSuggestionRequest &request, bool deleteStopInfos )
{
    Q_UNUSED( provider );
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

    setData( sourceName, "serviceProvider", provider->id() );
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
}

void PublicTransportEngine::errorParsing( ServiceProvider *provider,
        ErrorCode errorCode, const QString &errorString,
        const QUrl &requestUrl, const AbstractRequest *request )
{
    Q_UNUSED( provider );
    kDebug() << "Error while parsing" << requestUrl //<< request->serviceProvider
             << "\n  sourceName =" << request->sourceName << request->dataType << request->parseMode;
    kDebug() << errorCode << errorString;

    // Remove erroneous source from running sources list
    m_runningSources.removeOne( request->sourceName );

    const QString sourceName = request->sourceName;
    setData( sourceName, "serviceProvider", provider->id() );
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
    setData( sourceName, "errorCode", errorCode );
    setData( sourceName, "errorString", errorString );
    setData( sourceName, "updated", QDateTime::currentDateTime() );
}

void PublicTransportEngine::progress( ServiceProvider *provider, qreal progress,
        const QString &jobDescription, const QUrl &requestUrl, const AbstractRequest *request )
{
    const QString sourceName = request->sourceName;
    setData( sourceName, "serviceProvider", provider->id() );
    setData( sourceName, "count", 0 );
    setData( sourceName, "progress", progress );
    setData( sourceName, "jobDescription", jobDescription );
    setData( sourceName, "requestUrl", requestUrl );
    if ( request->parseMode == ParseForDeparturesArrivals ) {
        setData( sourceName, "parseMode", "departures" );
    } else if ( request->parseMode == ParseForJourneys ) {
        setData( sourceName, "parseMode", "journeys" );
    } else if ( request->parseMode == ParseForStopSuggestions ) {
        setData( sourceName, "parseMode", "stopSuggestions" );
    }
    setData( sourceName, "receivedData", "nothing" );
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

    QString providerId = dataSource[ "serviceProvider" ].toString();
    if ( providerId.isEmpty() ) {
        kWarning() << "Internal error: Service provider unknown"; // Could get provider id from <name>
        return false;
    }
    const ProviderPointer provider = providerFromId( providerId );
    if ( provider.isNull() ) {
        return false;
    }

    QDateTime downloadTime = m_nextDownloadTimeProposals[ stripDateAndTimeValues(nonAmbiguousName) ];
    int minForSufficientChanges = downloadTime.isValid()
            ? QDateTime::currentDateTime().secsTo( downloadTime ) : 0;
    int minFetchWait;

    // If delays are available set maximum fetch wait
    const int secsSinceLastUpdate = dataSource["updated"].toDateTime().secsTo(
                                    QDateTime::currentDateTime() );
    if ( provider->type() == GtfsProvider ) {
        // Update GTFS accessors once a week
        // TODO: Check for an updated GTFS feed every X seconds, eg. once an hour
        const QSharedPointer<ServiceProviderGtfs> gtfsAccessor =
                provider.objectCast<ServiceProviderGtfs>();
        kDebug() << "Wait time until next update from GTFS provider:"
                 << KGlobal::locale()->prettyFormatDuration(1000 *
                    (gtfsAccessor->isRealtimeDataAvailable()
                     ? 60 - secsSinceLastUpdate : 60 * 60 * 24 - secsSinceLastUpdate));
        return gtfsAccessor->isRealtimeDataAvailable()
                ? secsSinceLastUpdate < 60 // Update maximally once a minute if realtime data is available
                : secsSinceLastUpdate < 60 * 60 * 24; // Update GTFS feed once a day without realtime data
    } else if ( provider->features().contains("Delay") && dataSource["delayInfoAvailable"].toBool() ) {
        minFetchWait = qBound((int)MIN_UPDATE_TIMEOUT, minForSufficientChanges,
                              (int)MAX_UPDATE_TIMEOUT_DELAY );
    } else {
        minFetchWait = qMax( minForSufficientChanges, MIN_UPDATE_TIMEOUT );
    }

    minFetchWait = qMax( minFetchWait, provider->minFetchWait() );
    kDebug() << "Wait time until next download:"
             << ((minFetchWait - secsSinceLastUpdate) / 60) << "min";
    return secsSinceLastUpdate < minFetchWait;
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE( publictransport, PublicTransportEngine )

// this is needed since PublicTransportEngine is a QObject
#include "build/publictransportdataengine.moc"
