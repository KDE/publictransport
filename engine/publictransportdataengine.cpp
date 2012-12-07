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
#include "serviceprovidertestdata.h"
#include "serviceproviderglobal.h"
#include "global.h"
#include "request.h"
#include "timetableservice.h"
#include "datasource.h"

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    #include "script/serviceproviderscript.h"
#endif
#ifdef BUILD_PROVIDER_TYPE_GTFS
    #include "gtfs/serviceprovidergtfs.h"
    #include "gtfs/gtfsservice.h"
#endif

// KDE/Plasma includes
#include <Plasma/DataContainer>
#include <KStandardDirs>

// Qt includes
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QTimer>
#include <QDBusConnection>

const int PublicTransportEngine::DEFAULT_TIME_OFFSET = 0;

Plasma::Service* PublicTransportEngine::serviceForSource( const QString &name )
{
#ifdef BUILD_PROVIDER_TYPE_GTFS
    // Return the GTFS service for "GTFS" or "gtfs" names
    if ( name.toLower() == QLatin1String("gtfs") ) {
        GtfsService *service = new GtfsService( name, this );
        service->setDestination( name );
        connect( service, SIGNAL(finished(Plasma::ServiceJob*)),
                 this, SLOT(gtfsServiceJobFinished(Plasma::ServiceJob*)) );
        return service;
    }
#endif

    // If the name of a data requesting source is given, return the timetable service
    const SourceType type = sourceTypeFromName( name );
    if ( isDataRequestingSourceType(type) ) {
        TimetableService *service = new TimetableService( name, this );
        service->setDestination( name );
        return service;
    }

    // No service for the given name found
    return 0;
}

void PublicTransportEngine::publishData( DataSource *dataSource )
{
    Q_ASSERT( dataSource );
    setData( dataSource->name(), dataSource->data() );

    ProvidersDataSource *providersSource = dynamic_cast< ProvidersDataSource* >( dataSource );
    if ( providersSource ) {
        // Update "ServiceProvider <id>" sources, which are also contained in the
        // DataSource object for the "ServiceProviders" data source.
        // Check which data sources of changed providers of that type are connected
        QStringList sources = Plasma::DataEngine::sources();
        const QStringList changedProviders = providersSource->takeChangedProviders();
        foreach ( const QString changedProviderId, changedProviders ) {
            const QString providerSource =
                    sourceTypeKeyword(ServiceProviderSource) + ' ' + changedProviderId;
            if ( sources.contains(providerSource) ) {
                // Found a data source for the current changed provider, clear it and set new data
                removeAllData( providerSource );
                setData( providerSource, providersSource->providerData(changedProviderId) );
            }
        }
    }
}

ProvidersDataSource *PublicTransportEngine::providersDataSource() const
{
    const QString name = sourceTypeKeyword( ServiceProvidersSource );
    ProvidersDataSource *dataSource = dynamic_cast< ProvidersDataSource* >( m_dataSources[name] );
    Q_ASSERT_X( dataSource, "PublicTransportEngine::providersDataSource()",
                "ProvidersDataSource is not available in m_dataSources!" );
    return dataSource;
}

#ifdef BUILD_PROVIDER_TYPE_GTFS
bool PublicTransportEngine::tryToStartGtfsFeedImportJob( Plasma::ServiceJob *job )
{
    Q_ASSERT( job );
    updateServiceProviderSource();
    ProvidersDataSource *dataSource = providersDataSource();
    Q_ASSERT( dataSource );
    const QString providerId = job->property( "serviceProviderId" ).toString();
    if ( dataSource->providerState(providerId) == QLatin1String("importing_gtfs_feed") ) {
        // GTFS feed already gets imported, cannot start another import job
        return false;
    }

    // Update provider state in service provider data source(s)
    QVariantHash stateData = dataSource->providerStateData( providerId );
    const QString databasePath = GtfsDatabase::databasePath( providerId );
    stateData[ "gtfsDatabasePath" ] = databasePath;
    stateData[ "gtfsDatabaseSize" ] = 0;
    stateData[ "progress" ] = 0;

    QString state;
    if ( job->operationName() == QLatin1String("importGtfsFeed") ) {
        state = "importing_gtfs_feed";
    } else if ( job->operationName() == QLatin1String("deleteGtfsDatabase") ) {
        state = "gtfs_feed_import_pending";
    } else {
        // The operations "updateGtfsDatabase" and "updateGtfsFeedInfo" can run in the background
        state = "ready";
    }
    dataSource->setProviderState( providerId, state, stateData );

    // Store the state in the cache
    QSharedPointer< KConfig > cache = ServiceProviderGlobal::cache();
    KConfigGroup group = cache->group( providerId );
    group.writeEntry( "state", state );
    KConfigGroup stateGroup = group.group( "stateData" );
    for ( QVariantHash::ConstIterator it = stateData.constBegin();
          it != stateData.constEnd(); ++it )
    {
        if ( isStateDataCached(it.key()) ) {
            stateGroup.writeEntry( it.key(), it.value() );
        }
    }

    publishData( dataSource );

    // Connect to messages of the job to update the provider state
    connect( job, SIGNAL(infoMessage(KJob*,QString,QString)),
             this, SLOT(gtfsImportJobInfoMessage(KJob*,QString,QString)) );
    connect( job, SIGNAL(percent(KJob*,ulong)),
             this, SLOT(gtfsImportJobPercent(KJob*,ulong)) );

    // The import job can be started
    return true;
}

void PublicTransportEngine::gtfsServiceJobFinished( Plasma::ServiceJob *job )
{
    // Disconnect messages of the job
    disconnect( job, SIGNAL(infoMessage(KJob*,QString,QString)),
                this, SLOT(gtfsImportJobInfoMessage(KJob*,QString,QString)) );
    disconnect( job, SIGNAL(percent(KJob*,ulong)),
                this, SLOT(gtfsImportJobPercent(KJob*,ulong)) );

    // Check that the job was not canceled because another database job was already running
    const bool canAccessGtfsDatabase = job->property("canAccessGtfsDatabase").toBool();
    const QString providerId = job->property( "serviceProviderId" ).toString();
    if ( !canAccessGtfsDatabase || providerId.isEmpty() ) {
        // Invalid job or cancelled, because another import job is already running
        // for the provider
        return;
    }

    // Reset state in "ServiceProviders", "ServiceProvider <id>" data sources
    QVariantHash stateData;
    const QString state = updateProviderState( providerId, &stateData, "GTFS", false );
    ProvidersDataSource *dataSource = providersDataSource();
    dataSource->setProviderState( providerId, state, stateData );

    publishData( dataSource );
}

void PublicTransportEngine::gtfsImportJobInfoMessage( KJob *job, const QString &plain,
                                                      const QString &rich )
{
    // Update "ServiceProviders", "ServiceProvider <id>" data sources
    const QString providerId = job->property( "serviceProviderId" ).toString();
    ProvidersDataSource *dataSource = providersDataSource();
    QVariantHash stateData = dataSource->providerStateData( providerId );
    stateData[ "statusMessage" ] = plain;
    if ( rich.isEmpty() ) {
        stateData.remove( "statusMessageRich" );
    } else {
        stateData[ "statusMessageRich" ] = rich;
    }
    dataSource->setProviderStateData( providerId, stateData );
    publishData( dataSource );
}

void PublicTransportEngine::gtfsImportJobPercent( KJob *job, ulong percent )
{
    // Update "ServiceProviders", "ServiceProvider <id>" data sources
    const QString providerId = job->property( "serviceProviderId" ).toString();
    ProvidersDataSource *dataSource = providersDataSource();
    QVariantHash stateData = dataSource->providerStateData( providerId );
    stateData[ "progress" ] = int( percent );
    dataSource->setProviderStateData( providerId, stateData );
    publishData( dataSource );
}
#endif // BUILD_PROVIDER_TYPE_GTFS

PublicTransportEngine::PublicTransportEngine( QObject* parent, const QVariantList& args )
        : Plasma::DataEngine( parent, args ),
        m_fileSystemWatcher(0), m_providerUpdateDelayTimer(0)
{
    // We ignore any arguments - data engines do not have much use for them
    Q_UNUSED( args )

    // This prevents applets from setting an unnecessarily high update interval
    // and using too much CPU.
    // 60 seconds should be enough, departure / arrival times have minute precision (except for GTFS).
    setMinimumPollingInterval( 60000 );

    connect( this, SIGNAL(sourceRemoved(QString)), this, SLOT(slotSourceRemoved(QString)) );

    // Get notified when the network state changes to update data sources,
    // which update timers were missed because of missing network connection
    QDBusConnection::sessionBus().connect( "org.kde.kded", "/modules/networkstatus",
                                           "org.kde.Solid.Networking.Client", "statusChanged",
                                           this, SLOT(networkStateChanged(uint)) );

    // Create "ServiceProviders" and "ServiceProvider [providerId]" data source object
    const QString name = sourceTypeKeyword( ServiceProvidersSource );
    m_dataSources.insert( name, new ProvidersDataSource(name) );
    updateServiceProviderSource();

    // Ensure the local provider installation directory exists in the users HOME and will not
    // get removed, by creating a small file in it so that the directory is never empty.
    // If an installation directory gets removed, the file system watcher would need to watch the
    // parent directory instead to get notified when it gets created again.
    const QString installationSubDirectory = ServiceProviderGlobal::installationSubDirectory();
    const QString saveDir = KGlobal::dirs()->saveLocation( "data", installationSubDirectory );
    QFile saveDirKeeper( saveDir + "Do not remove this directory" );
    saveDirKeeper.open( QIODevice::WriteOnly );
    saveDirKeeper.write( "If this directory gets removed, PublicTransport will not get notified "
            "about installed provider files in that directory." );
    saveDirKeeper.close();

    // Create a file system watcher for the provider plugin installation directories
    // to get notified about new/modified/removed providers
    const QStringList directories = KGlobal::dirs()->findDirs( "data", installationSubDirectory );
    m_fileSystemWatcher = new QFileSystemWatcher( directories );
    connect( m_fileSystemWatcher, SIGNAL(directoryChanged(QString)),
             this, SLOT(serviceProviderDirChanged(QString)) );
}

PublicTransportEngine::~PublicTransportEngine()
{
    delete m_fileSystemWatcher;
    delete m_providerUpdateDelayTimer;
    qDeleteAll( m_dataSources );
    m_dataSources.clear();
}

QStringList PublicTransportEngine::sources() const
{
    QStringList sources = Plasma::DataEngine::sources();
    sources << sourceTypeKeyword(LocationsSource)
            << sourceTypeKeyword(ServiceProvidersSource)
            << sourceTypeKeyword(ErroneousServiceProvidersSource)
            << sourceTypeKeyword(VehicleTypesSource);
    sources.removeDuplicates();
    return sources;
}

void PublicTransportEngine::networkStateChanged( uint state )
{
    if ( state != 4 ) {
        // 4 => Connected, see Solid::Networking::Status
        return;
    }

    // Network is connected again, check for missed update timers in connected data sources
    for ( QHash<QString, DataSource*>::ConstIterator it = m_dataSources.constBegin();
          it != m_dataSources.constEnd(); ++it )
    {
        // Check if the current data source is a timetable data source, without running update
        TimetableDataSource *dataSource = dynamic_cast< TimetableDataSource* >( *it );
        if ( !dataSource || m_runningSources.contains(it.key()) ) {
            continue;
        }

        // Check if the next automatic update time was missed (stored in the data source)
        const QDateTime nextAutomaticUpdate = dataSource->value("nextAutomaticUpdate").toDateTime();
        if ( nextAutomaticUpdate <= QDateTime::currentDateTime() ) {
            // Found a timetable data source that should have been updated already and
            // is not currently being updated (not in m_runningSources).
            // This happens if there was no network connection while an automatic update
            // was triggered. If the system is suspended the QTimer's for automatic updates
            // are not triggered at all.
            // Do now manually request updates for all connected sources, ie. do what should
            // have been done in updateTimeout().
            foreach ( const QString &sourceName, dataSource->usingDataSources() ) {
                updateTimetableDataSource( SourceRequestData(sourceName) );
            }
        }
    }
}

bool PublicTransportEngine::isProviderUsed( const QString &providerId )
{
    // Check if a request is currently running for the provider
    foreach ( const QString &runningSource, m_runningSources ) {
        if ( runningSource.contains(providerId) ) {
            return true;
        }
    }

    // Check if a data source is connected that uses the provider
    for ( QHash< QString, DataSource* >::ConstIterator it = m_dataSources.constBegin();
          it != m_dataSources.constEnd(); ++it )
    {
        Q_ASSERT( *it );
        TimetableDataSource *dataSource = dynamic_cast< TimetableDataSource* >( *it );
        if ( dataSource && dataSource->providerId() == providerId ) {
            return true;
        }
    }

    // The provider is not used any longer by the engine
    return false;
}

void PublicTransportEngine::slotSourceRemoved( const QString &sourceName )
{
    const QString nonAmbiguousName = disambiguateSourceName( sourceName );
    if ( m_dataSources.contains(nonAmbiguousName) ) {
        // If this is a timetable data source, which might be associated with multiple
        // ambiguous source names, check if this data source is still connected under other names
        TimetableDataSource *timetableDataSource =
                dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
        if ( timetableDataSource ) {
            timetableDataSource->removeUsingDataSource( sourceName );
            if ( timetableDataSource->usageCount() > 0 ) {
                // The TimetableDataSource object is still used by other connected data sources
                return;
            }
        }

        if ( sourceName == sourceTypeKeyword(ServiceProvidersSource) ) {
            // Do not remove ServiceProviders data source
            return;
        }

        // If a provider was used by the source,
        // remove the provider if it is not used by another source
        const DataSource *dataSource = m_dataSources.take( nonAmbiguousName );
        Q_ASSERT( dataSource );
        if ( dataSource->data().contains("serviceProvider") ) {
            const QString providerId = dataSource->value("serviceProvider").toString();
            if ( !providerId.isEmpty() && !isProviderUsed(providerId) ) {
                // Remove provider from the list,
                // if no other ProviderPointer to that provider exists, this deletes the provider
                m_providers.remove( providerId );
            }
        }

        // The data source is no longer used, delete it
        delete dataSource;
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
    dataServiceProvider.insert( "type", ServiceProviderGlobal::typeName(data.type()) );
#ifdef BUILD_PROVIDER_TYPE_GTFS
    if ( data.type() == Enums::GtfsProvider ) {
        dataServiceProvider.insert( "feedUrl", data.feedUrl() );
    }
#endif
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    if ( data.type() == Enums::ScriptedProvider ) {
        dataServiceProvider.insert( "scriptFileName", data.scriptFileName() );
    }
#endif
    dataServiceProvider.insert( "name", data.name() );
    dataServiceProvider.insert( "url", data.url() );
    dataServiceProvider.insert( "shortUrl", data.shortUrl() );
    dataServiceProvider.insert( "country", data.country() );
    dataServiceProvider.insert( "cities", data.cities() );
    dataServiceProvider.insert( "credit", data.credit() );
    dataServiceProvider.insert( "useSeparateCityValue", data.useSeparateCityValue() );
    dataServiceProvider.insert( "onlyUseCitiesInList", data.onlyUseCitiesInList() );
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
    const QSharedPointer<KConfig> cache = ServiceProviderGlobal::cache();
    KConfigGroup providerGroup = cache->group( data.id() );
    if ( provider ) {
        // Write features to the return value
        const QList< Enums::ProviderFeature > features = provider->features();
        const QStringList featureStrings = ServiceProviderGlobal::featureStrings( features );
        dataServiceProvider.insert( "features", featureStrings );
        dataServiceProvider.insert( "featureNames", ServiceProviderGlobal::featureNames(features) );

        // Make sure, features have been written to the cache
        providerGroup.writeEntry( "features", featureStrings );
    } else {
        // Check stored feature strings and re-read features if an invalid string was found
        bool ok;
        QStringList featureStrings = providerGroup.readEntry("features", QStringList());
        const bool featureListIsEmpty = featureStrings.removeOne("(none)");
        QList< Enums::ProviderFeature > features =
                ServiceProviderGlobal::featuresFromFeatureStrings( featureStrings, &ok );

        if ( (featureListIsEmpty || !featureStrings.isEmpty()) && ok ) {
            // Feature list could be read from cache
            dataServiceProvider.insert( "features", featureStrings );
            dataServiceProvider.insert( "featureNames",
                                        ServiceProviderGlobal::featureNames(features) );
        } else {
            kDebug() << "No cached feature data was found for provider" << data.id();
            // No cached feature data was found for the provider,
            // create the provider to get the feature list and store it in the cache
            bool newlyCreated;
            ProviderPointer _provider = providerFromId( data.id(), &newlyCreated );
            features = _provider->features();
            featureStrings = ServiceProviderGlobal::featureStrings( features );
            const QStringList featuresNames = ServiceProviderGlobal::featureNames( features );

            // Remove provider from the list again to delete it
            if ( newlyCreated ) {
                m_providers.remove( data.id() );
            }

            // If no features are supported write "(none)" to the cache
            // to indicate that features have been written to the cache
            if ( featureStrings.isEmpty() ) {
                featureStrings.append( "(none)" );
            }

            // Write features to the return value
            dataServiceProvider.insert( "features", featureStrings );
            dataServiceProvider.insert( "featureNames", featuresNames );

            // Write features to cache
            providerGroup.writeEntry( "features", featureStrings );
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

PublicTransportEngine::ProviderPointer PublicTransportEngine::providerFromId(
        const QString &id, bool *newlyCreated )
{
    if ( m_providers.contains(id) ) {
        // The provider with the given ID is already created
        if ( newlyCreated ) {
            *newlyCreated = false;
        }
        return m_providers[ id ];
    } else {
        // Provider not already created
        if ( newlyCreated ) {
            *newlyCreated = true;
        }

        // Try to create the provider
        ServiceProvider *provider = createProvider( id, this );
        if ( !provider ) {
            // Return an invalid ProviderPointer, when the provider could not be created
            return ProviderPointer::create();
        }

        // Check the state of the provider, it needs to be "ready"
        ProvidersDataSource *dataSource = providersDataSource();
        if ( dataSource ) {
            const QString state = dataSource->providerState( id );
            if ( state != QLatin1String("ready") ) {
                kWarning() << "Provider" << id << "is not ready, state is" << state;
                return ProviderPointer::create();
            }
        }

        // Connect provider, when it was created successfully
        connect( provider, SIGNAL(departuresReceived(ServiceProvider*,QUrl,DepartureInfoList,GlobalTimetableInfo,DepartureRequest)),
                 this, SLOT(departuresReceived(ServiceProvider*,QUrl,DepartureInfoList,GlobalTimetableInfo,DepartureRequest)) );
        connect( provider, SIGNAL(arrivalsReceived(ServiceProvider*,QUrl,ArrivalInfoList,GlobalTimetableInfo,ArrivalRequest)),
                 this, SLOT(arrivalsReceived(ServiceProvider*,QUrl,ArrivalInfoList,GlobalTimetableInfo,ArrivalRequest)) );
        connect( provider, SIGNAL(journeysReceived(ServiceProvider*,QUrl,JourneyInfoList,GlobalTimetableInfo,JourneyRequest)),
                 this, SLOT(journeysReceived(ServiceProvider*,QUrl,JourneyInfoList,GlobalTimetableInfo,JourneyRequest)) );
        connect( provider, SIGNAL(stopsReceived(ServiceProvider*,QUrl,StopInfoList,StopSuggestionRequest)),
                 this, SLOT(stopsReceived(ServiceProvider*,QUrl,StopInfoList,StopSuggestionRequest)) );
        connect( provider, SIGNAL(additionalDataReceived(ServiceProvider*,QUrl,TimetableData,AdditionalDataRequest)),
                 this, SLOT(additionalDataReceived(ServiceProvider*,QUrl,TimetableData,AdditionalDataRequest)) );
        connect( provider, SIGNAL(requestFailed(ServiceProvider*,ErrorCode,QString,QUrl,const AbstractRequest*)),
                 this, SLOT(requestFailed(ServiceProvider*,ErrorCode,QString,QUrl,const AbstractRequest*)) );

        // Create a ProviderPointer for the created provider and
        // add it to the list of currently used providers
        const ProviderPointer pointer( provider );
        m_providers.insert( id, pointer );
        return pointer;
    }
}

bool PublicTransportEngine::updateServiceProviderForCountrySource( const SourceRequestData &data )
{
    QString providerId;
    if ( data.defaultParameter.contains('_') ) {
        // Seems that a service provider ID is given
        providerId = data.defaultParameter;
    } else {
        // Assume a country code in name
        if ( !updateServiceProviderSource() || !updateLocationSource() ) {
            return false;
        }

        // The defaultParameter stored in data is a location code
        // (ie. "international" or a two letter country code)
        QVariantHash locations = m_dataSources[ sourceTypeKeyword(LocationsSource) ]->data();
        QVariantHash locationCountry = locations[ data.defaultParameter.toLower() ].toHash();
        QString defaultProvider = locationCountry[ "defaultProvider" ].toString();
        if ( defaultProvider.isEmpty() ) {
            return false;
        }

        providerId = defaultProvider;
    }

    // Test if the provider is valid
    QVariantHash providerData;
    QString errorMessage;
    if ( testServiceProvider(providerId, &providerData, &errorMessage) ) {
        // The provider is valid, update it's state
        QVariantHash stateData;
        const QString state = updateProviderState( providerId, &stateData,
                                                   providerData["type"].toString() );

        // Publish all provider information and add "state", "stateData" and "error"
        setData( data.name, providerData );
        setData( data.name, "state", state );
        setData( data.name, "stateData", stateData );
        setData( data.name, "error", false );
    } else {
        // The provider is erroneous, only return "id", "error" and "errorMessage"
        setData( data.name, "id", providerId );
        setData( data.name, "error", true );
        setData( data.name, "errorMessage", errorMessage );
    }
    return true;
}

bool PublicTransportEngine::updateServiceProviderSource()
{
    const QString name = sourceTypeKeyword( ServiceProvidersSource );
    ProvidersDataSource *providersSource = providersDataSource();
    if ( providersSource->isDirty() ) {
        const QStringList providers = ServiceProviderGlobal::installedProviders();
        if ( providers.isEmpty() ) {
            kWarning() << "Could not find any service provider plugins";
            return false;
        }

        QStringList loadedProviders;
        m_erroneousProviders.clear();
        QSharedPointer<KConfig> cache = ServiceProviderGlobal::cache();
        foreach( const QString &provider, providers ) {
            QString providerId = ServiceProviderGlobal::idFromFileName( KUrl(provider).fileName() );
            QVariantHash providerData;
            QString errorMessage;
            if ( testServiceProvider(providerId, &providerData, &errorMessage, cache) ) {
                QVariantHash stateData;
                const QString state = updateProviderState( providerId, &stateData,
                                                           providerData["type"].toString() );

                providerData["error"] = false;
                providersSource->addProvider( providerId,
                        ProvidersDataSource::ProviderData(providerData, state, stateData) );
                loadedProviders << providerId;
            } else {
                // Invalid provider, clear old data and only set id and error fields
                providerData.clear();
                providerData["id"] = providerId;
                providerData["error"] = true;
                providerData["errorMessage"] = errorMessage;

                providersSource->addProvider( providerId,
                        ProvidersDataSource::ProviderData(providerData, "error", QVariantHash()) );
            }
        }

        // Print information about loaded/erroneous providers
        kDebug() << "Loaded" << loadedProviders.count() << "service providers";
        if ( !m_erroneousProviders.isEmpty() ) {
            kWarning() << "Erroneous service provider plugins, that could not be loaded:"
                       << m_erroneousProviders;
        }

        // Insert the data source, providersData );
        m_dataSources.insert( name, providersSource );
    }

    // Remove all old data, some service providers may have been updated and are now erroneous
    removeAllData( name );
    publishData( providersSource );

    return true;
}

QString PublicTransportEngine::updateProviderState( const QString &providerId,
                                                    QVariantHash *stateData,
                                                    const QString &providerType,
                                                    bool readFromCache )
{
    Q_ASSERT( stateData );
    QSharedPointer< KConfig > cache = ServiceProviderGlobal::cache();
    KConfigGroup group = cache->group( providerId );
    const QString cachedState = readFromCache ? group.readEntry("state", QString()) : QString();
#ifdef BUILD_PROVIDER_TYPE_GTFS // Currently type is only used for GTFS
    const Enums::ServiceProviderType type = ServiceProviderGlobal::typeFromString( providerType );
#endif // BUILD_PROVIDER_TYPE_GTFS

    // Test if there is an error
    if ( m_erroneousProviders.contains(providerId) ) {
        stateData->insert( "statusMessage", m_erroneousProviders[providerId].toString() );
        return "error";
    }

    if ( !cachedState.isEmpty() ) {
        // State is stored in the cache,
        // also read state data from cache
        KConfigGroup stateGroup = group.group( "stateData" );
        foreach ( const QString &key, stateGroup.keyList() ) {
            stateData->insert( key, stateGroup.readEntry(key) );
        }

#ifdef BUILD_PROVIDER_TYPE_GTFS
        if ( type == Enums::GtfsProvider ) {
            // Update state and add dynamic state data
            const QString state = ServiceProviderGtfs::updateGtfsDatabaseState(
                    providerId, cache, stateData );
            if ( state != QLatin1String("ready") ) {
                // The database is invalid/deleted, but the cache says the import was finished
                deleteProvider( providerId );
            }
            return state;
        } else
#endif // BUILD_PROVIDER_TYPE_GTFS
        {
            if ( cachedState != QLatin1String("ready") ) {
                // Provider not ready, cannot use it
                deleteProvider( providerId );
            }

            // State of non-GTFS providers does not need more tests,
            // if there is an error only the fields "error" and "errorMessage" are available
            // in the data source, no fields "state" or "stateData"
            return cachedState;
        }
    } // !state.isEmpty()

    // State is not stored in the cache or is out of date
    QString state = "ready";

#ifdef BUILD_PROVIDER_TYPE_GTFS
    if ( type == Enums::GtfsProvider ) {
        state = ServiceProviderGtfs::updateGtfsDatabaseState( providerId, cache, stateData );
    }
#endif // BUILD_PROVIDER_TYPE_GTFS

    // Ensure a status message is given (or at least warn if not)
    if ( stateData->value("statusMessage").toString().isEmpty() ) {
        if ( state == QLatin1String("ready") ) {
            stateData->insert( "statusMessage",
                                i18nc("@info/plain", "The provider is ready to use") );
        } else {
            kWarning() << "Missing status message explaining why the provider" << providerId
                        << "is not ready";
        }
    }

    // Store the state in the cache
    group.writeEntry( "state", state );

    // Write state data to the cache
    KConfigGroup stateGroup = group.group( "stateData" );
    for ( QVariantHash::ConstIterator it = stateData->constBegin();
        it != stateData->constEnd(); ++it )
    {
        if ( isStateDataCached(it.key()) ) {
            stateGroup.writeEntry( it.key(), it.value() );
        }
    }
    return state;
}

bool PublicTransportEngine::isStateDataCached( const QString &stateDataKey )
{
    return stateDataKey != QLatin1String("progress") &&
           stateDataKey != QLatin1String("gtfsDatabasePath") &&
           stateDataKey != QLatin1String("gtfsDatabaseSize");
}

bool PublicTransportEngine::testServiceProvider( const QString &providerId,
                                                 QVariantHash *providerData, QString *errorMessage,
                                                 const QSharedPointer<KConfig> &_cache )
{
    if ( m_providers.contains(providerId) ) {
        // The provider is cached in the engine, ie. it is valid,
        // use it's ServiceProviderData object
        const ProviderPointer provider = m_providers[ providerId ];
        *providerData = serviceProviderData( provider.data() );
        errorMessage->clear();
        return true;
    }

    // Read cached data for the provider
    QSharedPointer<KConfig> cache = _cache.isNull() ? ServiceProviderGlobal::cache() : _cache;
    ServiceProviderTestData testData = ServiceProviderTestData::read( providerId, cache );

    // TODO Needs to be done for each provider sub class here
    foreach ( Enums::ServiceProviderType type, ServiceProviderGlobal::availableProviderTypes() ) {
        switch ( type ) {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        case Enums::ScriptedProvider:
            if ( !ServiceProviderScript::isTestResultUnchanged(providerId, cache) ) {
                kDebug() << "Script changed" << providerId;
                testData.setSubTypeTestStatus( ServiceProviderTestData::Pending );
                testData.write( providerId, cache );
            }
            break;
#endif
        case Enums::GtfsProvider:
            break;
        case Enums::InvalidProvider:
        default:
            kWarning() << "Provider type unknown" << type;
            break;
        }
    }

    if ( testData.status() == ServiceProviderTestData::Failed ) {
        // Tests are marked as failed in the cache
        kWarning() << "Tests are marked as failed in the cache" << providerId;
        providerData->clear();
        *errorMessage = testData.errorMessage();
        m_erroneousProviders.insert( providerId, testData.errorMessage() );
        updateErroneousServiceProviderSource();
        return false;
    }

    // Read provider data from the XML file
    QString _errorMessage;
    const QScopedPointer<ServiceProviderData> data(
            ServiceProviderDataReader::read(providerId, &_errorMessage) );
    if ( data.isNull() ) {
        // Could not read provider data
        if ( testData.isXmlStructureTestPending() ) {
            // Store error message in cache and do not reread unchanged XMLs everytime
            testData.setXmlStructureTestStatus( ServiceProviderTestData::Failed, _errorMessage );
            testData.write( providerId, cache );
        }

        providerData->clear();
        *errorMessage = _errorMessage;
        m_erroneousProviders.insert( providerId, _errorMessage );
        updateErroneousServiceProviderSource();
        return false;
    } else if ( !ServiceProviderGlobal::isProviderTypeAvailable(data->type()) ) {
        providerData->clear();
        _errorMessage = i18nc("@info/plain", "Support for provider type %1 is not available",
                              ServiceProviderGlobal::typeName(data->type(),
                                    ServiceProviderGlobal::ProviderTypeNameWithoutUnsupportedHint));
        *errorMessage = _errorMessage;
        m_erroneousProviders.insert( providerId, _errorMessage );
        updateErroneousServiceProviderSource();
        return false;
    } else if ( testData.isXmlStructureTestPending() ) {
        // Store error message in cache and do not reread unchanged XMLs everytime
        testData.setXmlStructureTestStatus( ServiceProviderTestData::Passed );
        testData.write( providerId, cache );
    }

    // XML file structure test is passed, run sub-type test if not done already
    if ( testData.isSubTypeTestPending() ) {
        // Need to create the provider to run tests in derived classes (in the constructor)
        const QScopedPointer<ServiceProvider> provider(
                createProviderForData(data.data(), this, cache) );
        data->setParent( 0 ); // Prevent deletion of data when the provider gets deleted

        // Read test data again, because it may have been changed in the provider constructor
        testData = ServiceProviderTestData::read( providerId, cache );

        // Run the sub type test if it is still pending
        testData = provider->runSubTypeTest( testData, cache );

        // Read test data again, updated in the ServiceProvider constructor
        if ( testData.results().testFlag(ServiceProviderTestData::SubTypeTestFailed) ) {
            // Sub-type test failed
            kWarning() << "Test failed for" << providerId << testData.errorMessage();
            providerData->clear();
            *errorMessage = testData.errorMessage();
            m_erroneousProviders.insert( providerId, testData.errorMessage() );
            updateErroneousServiceProviderSource();
            return false;
        }

        // The provider is already created, use it in serviceProviderData(), if needed
        *providerData = serviceProviderData( provider.data() );
    } else {
        *providerData = serviceProviderData( *data );
    }

    m_erroneousProviders.remove( providerId );
    const QLatin1String name = sourceTypeKeyword( ErroneousServiceProvidersSource );
    removeData( name, providerId );

    errorMessage->clear();
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
    if ( m_dataSources.contains(name) ) {
        setData( name, m_dataSources[name]->data() );
    } else {
        SimpleDataSource *dataSource = new SimpleDataSource( name, locations() );
        m_dataSources.insert( name, dataSource );
        setData( name, dataSource->data() );
    }
    return true;
}

QString PublicTransportEngine::providerIdFromSourceName( const QString &sourceName )
{
    const int pos = sourceName.indexOf( ' ' );
    if ( pos == -1 ) { //|| pos = sourceName.length() - 1 ) {
        return QString();
    }

    const int endPos = sourceName.indexOf( '|', pos + 1 );
    return fixProviderId( sourceName.mid(pos + 1, endPos - pos - 1).trimmed() );
}

ParseDocumentMode PublicTransportEngine::parseModeFromSourceType(
        PublicTransportEngine::SourceType type )
{
    switch ( type ) {
    case DeparturesSource:
        return ParseForDepartures;
    case ArrivalsSource:
        return ParseForArrivals;
    case StopsSource:
        return ParseForStopSuggestions;
    case JourneysDepSource:
        return ParseForJourneysByDepartureTime;
    case JourneysArrSource:
        return ParseForJourneysByArrivalTime;
    case JourneysSource:
        return ParseForJourneysByDepartureTime;
    default:
        return ParseInvalid;
    }
}

bool PublicTransportEngine::enoughDataAvailable( DataSource *dataSource,
                                                 const SourceRequestData &sourceData ) const
{
    TimetableDataSource *timetableDataSource = dynamic_cast< TimetableDataSource* >( dataSource );
    if ( !timetableDataSource ) {
        return true;
    }

    AbstractTimetableItemRequest *request = sourceData.request;
    return timetableDataSource->enoughDataAvailable( request->dateTime(), request->count() );
}

bool PublicTransportEngine::updateTimetableDataSource( const SourceRequestData &data )
{
    const QString nonAmbiguousName = disambiguateSourceName( data.name );
    bool containsDataSource = m_dataSources.contains( nonAmbiguousName );
    if ( containsDataSource && isSourceUpToDate(nonAmbiguousName) &&
         enoughDataAvailable(m_dataSources[nonAmbiguousName], data) )
    { // Data is stored in the map and up to date
        TimetableDataSource *dataSource =
                dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
        dataSource->addUsingDataSource( QSharedPointer<AbstractRequest>(data.request->clone()),
                                        data.name, data.request->dateTime(), data.request->count() );
        setData( data.name, dataSource->data() );
    } else if ( m_runningSources.contains(nonAmbiguousName) ) {
        // Source gets already processed
        kDebug() << "Source already gets processed, please wait" << data.name;
    } else if ( data.parseMode == ParseInvalid || !data.request ) {
        kWarning() << "Invalid source" << data.name;
        return false;
    } else { // Request new data
        TimetableDataSource *dataSource = containsDataSource
                ? dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] )
                : new TimetableDataSource(nonAmbiguousName);
        dataSource->clear();
        dataSource->addUsingDataSource( QSharedPointer<AbstractRequest>(data.request->clone()),
                                        data.name, data.request->dateTime(), data.request->count() );
        m_dataSources[ nonAmbiguousName ] = dataSource;

        // Start the request
        request( data );
    }

    return true;
}

void PublicTransportEngine::requestAdditionalData( const QString &sourceName, int itemNumber )
{
    // Try to get a pointer to the provider with the provider ID from the source name
    const QString providerId = providerIdFromSourceName( sourceName );
    const ProviderPointer provider = providerFromId( providerId );
    if ( provider.isNull() ) {
        emit additionalDataRequestFinished( QVariantHash(), false,
                QString("Service provider %1 could not be created").arg(providerId) );
        return; // Service provider couldn't be created
    }

    // Test if the source with the given name is cached
    const QString nonAmbiguousName = disambiguateSourceName( sourceName );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        emit additionalDataRequestFinished( QVariantHash(), false,
                "Data source to update not found: " + sourceName );
        return;
    }

    // Get the data list, currently only for departures/arrivals TODO: journeys
    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    if ( !dataSource ) {
        emit additionalDataRequestFinished( QVariantHash(), false,
                "Data source is not a timetable data source: " + sourceName );
        return;
    }

    QVariantList items = dataSource->timetableItems();
    if ( itemNumber >= items.count() || itemNumber < 0 ) {
        emit additionalDataRequestFinished( QVariantHash(), false,
                                            "Item to update not found in data source" );
        return;
    }

    // Get the timetable item stored in the data source at the given index
    QVariantHash item = items[ itemNumber ].toHash();

    // Check if additional data is already included or was already requested
    if ( item["IncludesAdditionalData"].toBool() ) {
        emit additionalDataRequestFinished( QVariantHash(), false,
                                            "Additional data is already included" );
        return;
    } else if ( item["WaitingForAdditionalData"].toBool() ) {
        emit additionalDataRequestFinished( QVariantHash(), false,
                                            "Additional data already was requested, please wait" );
        return;
    }

    // Check if the timetable item is valid,
    // extract values needed for the additional data request job
    const QDateTime dateTime = item[ "DepartureDateTime" ].toDateTime();
    const QString transportLine = item[ "TransportLine" ].toString();
    const QString target = item[ "Target" ].toString();
    const QString routeDataUrl = item[ "RouteDataUrl" ].toString();
    if ( routeDataUrl.isEmpty() &&
         (!dateTime.isValid() || transportLine.isEmpty() || target.isEmpty()) )
    {
        emit additionalDataRequestFinished( QVariantHash(), false,
                                            QString("Item to update is invalid: %1, %2, %3")
                                            .arg(dateTime.toString()).arg(transportLine, target) );
        return;
    }

    // Store state of additional data in the timetable item
    item["WaitingForAdditionalData"] = true;
    items[ itemNumber ] = item;
    dataSource->setTimetableItems( items );

    // Found data of the timetable item to update
    const SourceRequestData sourceData( sourceName );
    provider->requestAdditionalData( AdditionalDataRequest(sourceName, itemNumber,
            sourceData.request->stop(), dateTime, transportLine, target, sourceData.request->city(),
            routeDataUrl) );
}

QString PublicTransportEngine::fixProviderId( const QString &providerId )
{
    if ( !providerId.isEmpty() ) {
        return providerId;
    }

    // No service provider ID given, use the default one for the users country
    const QString country = KGlobal::locale()->country();

    // Try to find the XML filename of the default accessor for [country]
    const QString filePath = ServiceProviderGlobal::defaultProviderForLocation( country );
    if ( filePath.isEmpty() ) {
        return 0;
    }

    // Extract service provider ID from filename
    const QString defaultProviderId = ServiceProviderGlobal::idFromFileName( filePath );
    kDebug() << "No service provider ID given, using the default one for country"
             << country << "which is" << defaultProviderId;
    return defaultProviderId;
}

QString PublicTransportEngine::disambiguateSourceName( const QString &sourceName )
{
    // Remove count argument
    QString ret = sourceName;
    ret.remove( QRegExp("(count=[^\\|]+)") );

    // Round time parameter values to 15 minutes precision
    QRegExp rx( "(time=[^\\|]+|datetime=[^\\|]+)" );
    if ( rx.indexIn(ret) != -1 ) {
        // Get the time value
        const QString timeParameter = rx.cap( 1 );
        QDateTime time;
        if ( timeParameter.startsWith(QLatin1String("time=")) ) {
            time = QDateTime( QDate::currentDate(),
                              QTime::fromString(timeParameter.mid(5), "hh:mm") );
        } else { // startsWith "datetime="
            time = QDateTime::fromString( timeParameter.mid(9) );
        }

        // Round 15 minutes
        qint64 msecs = time.toMSecsSinceEpoch();
        time = QDateTime::fromMSecsSinceEpoch( msecs - msecs % (1000 * 60 * 15) );

        // Replace old time parameter with a new one
        ret.replace( rx.pos(), rx.matchedLength(), QLatin1String("datetime=") + time.toString() );
    }

    // Read parameters to reorder them afterwards
    const SourceType type = sourceTypeFromName( ret );
    const QString typeKeyword = sourceTypeKeyword( type );
    const QStringList parameterPairs = ret.mid( typeKeyword.length() )
            .trimmed().split( '|', QString::SkipEmptyParts );
    QString defaultParameter;
    QHash< QString, QString > parameters;
    for ( int i = 0; i < parameterPairs.length(); ++i ) {
        const QString parameter = parameterPairs.at( i ).trimmed();
        const int pos = parameter.indexOf( '=' );
        if ( pos == -1 ) {
            // No parameter name given, this is the default parameter, eg. the provider ID
            defaultParameter = fixProviderId( parameter );
        } else {
            // Only add parameters with non-empty parameter name and value,
            // make parameter value lower case (eg. stop names)
            const QString parameterName = parameter.left( pos );
            const QString parameterValue = parameter.mid( pos + 1 ).trimmed();
            if ( !parameterName.isEmpty() && !parameterValue.isEmpty() ) {
                parameters[ parameterName ] = parameterValue.toLower();
            }
        }
    }

    // Build non-ambiguous source name with standardized parameter order
    ret = typeKeyword + ' ' + defaultParameter;
    if ( parameters.contains(QLatin1String("city")) ) {
        ret += "|city=" + parameters["city"];
    }
    if ( parameters.contains(QLatin1String("stop")) ) {
        ret += "|stop=" + parameters["stop"];
    }
    if ( parameters.contains(QLatin1String("originstop")) ) {
        ret += "|originstop=" + parameters["originstop"];
    }
    if ( parameters.contains(QLatin1String("targetstop")) ) {
        ret += "|targetstop=" + parameters["targetstop"];
    }
    if ( parameters.contains(QLatin1String("timeoffset")) ) {
        ret += "|timeoffset=" + parameters["timeoffset"];
    }
    if ( parameters.contains(QLatin1String("time")) ) {
        ret += "|time=" + parameters["time"];
    }
    if ( parameters.contains(QLatin1String("datetime")) ) {
        ret += "|datetime=" + parameters["datetime"];
    }
    if ( parameters.contains(QLatin1String("count")) ) {
        ret += "|count=" + parameters["count"];
    }
    if ( parameters.contains(QLatin1String("longitude")) ) {
        ret += "|longitude=" + parameters["longitude"];
    }
    if ( parameters.contains(QLatin1String("latitude")) ) {
        ret += "|latitude=" + parameters["latitude"];
    }
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
        connect( m_providerUpdateDelayTimer, SIGNAL(timeout()), this, SLOT(reloadChangedProviders()) );
    }

    m_providerUpdateDelayTimer->start( 250 );
}

void PublicTransportEngine::deleteProvider( const QString &providerId )
{
    // Clear all cached data
    const QStringList cachedSources = m_dataSources.keys();
    foreach( const QString &cachedSource, cachedSources ) {
        const QString currentProviderId = providerIdFromSourceName( cachedSource );
        if ( currentProviderId == providerId ) {
            // Remove data source for the current provider
            // and remove the provider object (deletes it)
            delete m_dataSources.take( cachedSource );
            m_providers.remove( providerId );
            m_erroneousProviders.remove( providerId );
        }
    }
}

void PublicTransportEngine::reloadChangedProviders()
{
    kDebug() << "Reload service providers (the service provider dir changed)";

    delete m_providerUpdateDelayTimer;
    m_providerUpdateDelayTimer = 0;

    // Notify the providers data source about the changed provider directory.
    // Do not remove it here, so that it can track which providers have changed
    ProvidersDataSource *providersSource = providersDataSource();
    if ( providersSource ) {
        providersSource->providersHaveChanged();
    }

    // Remove cached locations source to have it updated
    delete m_dataSources.take( sourceTypeKeyword(LocationsSource) );

    // Clear all cached data (use the new provider to parse the data again)
    const QStringList cachedSources = m_dataSources.keys();
    const QSharedPointer< KConfig > cache = ServiceProviderGlobal::cache();
    foreach( const QString &cachedSource, cachedSources ) {
        const QString providerId = providerIdFromSourceName( cachedSource );
        if ( !providerId.isEmpty() &&
             (ServiceProviderGlobal::isSourceFileModified(providerId, cache)
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
             || !ServiceProviderScript::isTestResultUnchanged(providerId, cache)
#endif
            ) )
        {
            // Remove data source for the current provider
            // and remove the provider object (deletes it)
            delete m_dataSources.take( cachedSource );
            m_providers.remove( providerId );
            m_erroneousProviders.remove( providerId );
        }
    }

    updateLocationSource();
    updateServiceProviderSource();
    updateErroneousServiceProviderSource();
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
    case VehicleTypesSource:
        return QLatin1String("VehicleTypes");
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
        const QString &sourceName )
{
    // Get type of the source, do not match case insensitive, otherwise there can be multiple
    // sources with the same data but only different case
    if ( sourceName.startsWith(sourceTypeKeyword(ServiceProviderSource) + ' ') ) {
        return ServiceProviderSource;
    } else if ( sourceName.compare(sourceTypeKeyword(ServiceProvidersSource)) == 0 ) {
        return ServiceProvidersSource;
    } else if ( sourceName.compare(sourceTypeKeyword(ErroneousServiceProvidersSource)) == 0 ) {
        return ErroneousServiceProvidersSource;
    } else if ( sourceName.compare(sourceTypeKeyword(LocationsSource)) == 0 ) {
        return LocationsSource;
    } else if ( sourceName.compare(sourceTypeKeyword(VehicleTypesSource)) == 0 ) {
        return VehicleTypesSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(DeparturesSource)) ) {
        return DeparturesSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(ArrivalsSource)) ) {
        return ArrivalsSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(StopsSource)) ) {
        return StopsSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(JourneysDepSource)) ) {
        return JourneysDepSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(JourneysArrSource)) ) {
        return JourneysArrSource;
    } else if ( sourceName.startsWith(sourceTypeKeyword(JourneysSource)) ) {
        return JourneysSource;
    } else {
        return InvalidSourceName;
    }
}

PublicTransportEngine::SourceRequestData::SourceRequestData( const QString &name )
        : name(name), type(sourceTypeFromName(name)), parseMode(parseModeFromSourceType(type)),
          request(0)
{
    if ( isDataRequestingSourceType(type) ) {
        // Extract parameters, which follow after the source type keyword in name
        // and are delimited with '|'
        QStringList parameters = name.mid( QString(sourceTypeKeyword(type)).length() )
                .trimmed().split( '|', QString::SkipEmptyParts );

        switch ( parseMode ) {
        case ParseForDepartures:
            request = new DepartureRequest( name, parseMode );
            break;
        case ParseForArrivals:
            request = new ArrivalRequest( name, parseMode );
            break;
        case ParseForStopSuggestions: {
            bool hasLongitude = false, hasLatitude = false;
            for ( int i = 0; i < parameters.length(); ++i ) {
                const QString parameter = parameters.at( i ).trimmed();
                const QString parameterName = parameter.left( parameter.indexOf('=') );
                if ( parameterName == QLatin1String("longitude") ) {
                    hasLongitude = true;
                } else if ( parameterName == QLatin1String("latitude") ) {
                    hasLatitude = true;
                }
            }
            if ( hasLongitude && hasLatitude ) {
                request = new StopsByGeoPositionRequest( name, parseMode );
            } else {
                request = new StopSuggestionRequest( name, parseMode );
            }
            break;
        }
        case ParseForJourneysByDepartureTime:
        case ParseForJourneysByArrivalTime:
            request = new JourneyRequest( name, parseMode );
            break;
        default:
            kWarning() << "Cannot create a request for parse mode" << parseMode;
            return;
        }

        // Read parameters
        for ( int i = 0; i < parameters.length(); ++i ) {
            const QString parameter = parameters.at( i ).trimmed();
            const int pos = parameter.indexOf( '=' );
            if ( pos == -1 ) {
                if ( !defaultParameter.isEmpty() ) {
                    kWarning() << "More than one parameters without name given:"
                               << defaultParameter << parameter;
                }

                // No parameter name given, assume the service provider ID
                defaultParameter = parameter;
            } else {
                const QString parameterName = parameter.left( pos );
                const QString parameterValue = parameter.mid( pos + 1 ).trimmed();
                if ( parameterValue.isEmpty() ) {
                    kWarning() << "Empty parameter value for parameter" << parameterName;
                } else if ( parameterName == QLatin1String("city") ) {
                    request->setCity( parameterValue );
                } else if ( parameterName == QLatin1String("stop") ) {
                    request->setStop( parameterValue );
                } else if ( parameterName == QLatin1String("targetstop") ) {
                    JourneyRequest *journeyRequest = dynamic_cast< JourneyRequest* >( request );
                    if ( !journeyRequest ) {
                        kWarning() << "The 'targetstop' parameter is only used for journey requests";
                    }
                    journeyRequest->setTargetStop( parameterValue );
                } else if ( parameterName == QLatin1String("originstop") ) {
                    JourneyRequest *journeyRequest = dynamic_cast< JourneyRequest* >( request );
                    if ( !journeyRequest ) {
                        kWarning() << "The 'originstop' parameter is only used for journey requests";
                    }
                    journeyRequest->setStop( parameterValue );
                } else if ( parameterName == QLatin1String("timeoffset") ) {
                    request->setDateTime(
                            QDateTime::currentDateTime().addSecs(parameterValue.toInt() * 60) );
                } else if ( parameterName == QLatin1String("time") ) {
                    request->setDateTime( QDateTime(QDate::currentDate(),
                                                    QTime::fromString(parameterValue, "hh:mm")) );
                } else if ( parameterName == QLatin1String("datetime") ) {
                    request->setDateTime( QDateTime::fromString(parameterValue) );
                    if ( !request->dateTime().isValid() ) {
                        request->setDateTime( QDateTime::fromString(parameterValue, Qt::ISODate) );
                    }
                } else if ( parameterName == QLatin1String("count") ) {
                    bool ok;
                    request->setCount( parameterValue.toInt(&ok) );
                    if ( !ok ) {
                        kWarning() << "Bad value for 'count' in source name:" << parameterValue;
                        request->setCount( 20 );
                    }
                } else if ( dynamic_cast<StopsByGeoPositionRequest*>(request) ) {
                    StopsByGeoPositionRequest *stopRequest =
                            dynamic_cast< StopsByGeoPositionRequest* >( request );
                    bool ok;
                    if ( parameterName == QLatin1String("longitude") ) {
                        stopRequest->setLongitude( parameterValue.toFloat(&ok) );
                        if ( !ok ) {
                            kWarning() << "Bad value for 'longitude' in source name:" << parameterValue;
                        }
                    } else if ( parameterName == QLatin1String("latitude") ) {
                        stopRequest->setLatitude( parameterValue.toFloat(&ok) );
                        if ( !ok ) {
                            kWarning() << "Bad value for 'latitude' in source name:" << parameterValue;
                        }
                    } else if ( parameterName == QLatin1String("distance") ) {
                        stopRequest->setDistance( parameterValue.toInt(&ok) );
                        if ( !ok ) {
                            kWarning() << "Bad value for 'distance' in source name:" << parameterValue;
                        }
                    } else {
                        kWarning() << "Unknown argument" << parameterName;
                    }
                } else {
                    kWarning() << "Unknown argument" << parameterName;
                }
            }
        }

        if ( !request->dateTime().isValid() ) {
            // No date/time value given, use default offset from now
            request->setDateTime( QDateTime::currentDateTime().addSecs(DEFAULT_TIME_OFFSET * 60) );
        }

        if ( parseMode == ParseForJourneysByArrivalTime ||
             parseMode == ParseForJourneysByDepartureTime )
        {
            // Use "stop" parameter as "originStop"/"targetStop" if it is not set
            JourneyRequest *journeyRequest = dynamic_cast< JourneyRequest* >( request );
            if ( !journeyRequest->stop().isEmpty() ) {
                journeyRequest->setStop( journeyRequest->stop() );
            } else if ( !journeyRequest->targetStop().isEmpty() ) {
                journeyRequest->setTargetStop( journeyRequest->stop() );
            }
        }

        // The default parameter is the provider ID for data requesting sources,
        // use the default provider for the users country if no ID is given
        defaultParameter = PublicTransportEngine::fixProviderId( defaultParameter );
    } else {
        // Extract provider ID or country code, which follow after the source type keyword in name
        defaultParameter = name.mid( QString(sourceTypeKeyword(type)).length() ).trimmed();
    }
}

PublicTransportEngine::SourceRequestData::~SourceRequestData()
{
    delete request;
}

bool PublicTransportEngine::SourceRequestData::isValid() const
{
    if ( type == InvalidSourceName ) {
        kWarning() << "Invalid source name" << name;
        return false;
    }

    if ( isDataRequestingSourceType(type) ) {
        if ( parseMode == ParseForDepartures || parseMode == ParseForArrivals ) {
            // Check if the stop name is missing
            if ( !request || request->stop().isEmpty() ) {
                kWarning() << "Stop name is missing in data source name" << name;
                return false;
            }
        } else if ( parseMode == ParseForStopSuggestions ) {
            // Check if the stop name or geo coordinates are missing
            if ( !request || (!dynamic_cast<StopsByGeoPositionRequest*>(request) &&
                              request->stop().isEmpty()) )
            {
                kWarning() << "Stop name is missing in data source name" << name;
                return false;
            }
        } else if ( parseMode == ParseForJourneysByArrivalTime ||
                    parseMode == ParseForJourneysByDepartureTime )
        {
            // Make sure non empty originStop and targetStop parameters are filled
            JourneyRequest *journeyRequest = dynamic_cast< JourneyRequest* >( request );
            if ( !journeyRequest || journeyRequest->stop().isEmpty() ||
                 journeyRequest->targetStop().isEmpty() )
            {
                kWarning() << "Origin and/or target stop names are missing in data source name"
                           << name;
                return false;
            }
        }
    }

    return true;
}

bool PublicTransportEngine::sourceRequestEvent( const QString &name )
{
    // If name is associated with a data source that runs asynchronously,
    // create the source first with empty data, gets updated once the request has finished
    SourceRequestData data( name );
    if ( data.isValid() && isDataRequestingSourceType(data.type) ) {
        setData( name, DataEngine::Data() );
    }

    return requestOrUpdateSourceEvent( data );
}

bool PublicTransportEngine::updateSourceEvent( const QString &name )
{
    return requestOrUpdateSourceEvent( SourceRequestData(name), true );
}

bool PublicTransportEngine::requestOrUpdateSourceEvent( const SourceRequestData &sourceData,
                                                        bool update )
{
    if ( !sourceData.isValid() ) {
        return false;
    }

    switch ( sourceData.type ) {
    case ServiceProviderSource:
        return updateServiceProviderForCountrySource( sourceData );
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
    case JourneysDepSource: {
        return updateTimetableDataSource( sourceData );
    }

    // This data source never changes, ie. needs no updates
    case VehicleTypesSource:
        if ( !update ) {
            initVehicleTypesSource();
        }
        return true;

    case InvalidSourceName:
    default:
        kDebug() << "Source name incorrect" << sourceData.name;
        return false;
    }
}

void PublicTransportEngine::initVehicleTypesSource()
{
    QVariantHash vehicleTypes;
    const int index = Enums::staticMetaObject.indexOfEnumerator("VehicleType");
    const QMetaEnum enumerator = Enums::staticMetaObject.enumerator( index );
    // Start at i = 1 to skip Enums::InvalidVehicleType
    for ( int i = 1; i < enumerator.keyCount(); ++i ) {
        Enums::VehicleType vehicleType = static_cast< Enums::VehicleType >( enumerator.value(i) );
        QVariantHash vehicleTypeData;
        vehicleTypeData.insert( "id", enumerator.key(i) );
        vehicleTypeData.insert( "name", Global::vehicleTypeToString(vehicleType) );
        vehicleTypeData.insert( "namePlural", Global::vehicleTypeToString(vehicleType, true) );
        vehicleTypeData.insert( "iconName", Global::vehicleTypeToIcon(vehicleType) );
        vehicleTypes.insert( QString::number(enumerator.value(i)), vehicleTypeData );
    }
    setData( sourceTypeKeyword(VehicleTypesSource), vehicleTypes );
}

void PublicTransportEngine::timetableDataReceived( ServiceProvider *provider,
        const QUrl &requestUrl, const DepartureInfoList &items,
        const GlobalTimetableInfo &globalInfo, const DepartureRequest &request,
        bool deleteDepartureInfos, bool isDepartureData )
{
    Q_UNUSED( requestUrl );
    Q_UNUSED( provider );
    Q_UNUSED( deleteDepartureInfos );
    const QString sourceName = request.sourceName();
    DEBUG_ENGINE_JOBS( items.count() << (isDepartureData ? "departures" : "arrivals")
                       << "received" << sourceName );

    const QString nonAmbiguousName = disambiguateSourceName( sourceName );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        kWarning() << "Data source already removed";
        return;
    }
    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    Q_ASSERT( dataSource );
    m_runningSources.removeOne( nonAmbiguousName );
    QVariantList departuresData;
    const QString itemKey = isDepartureData ? "departures" : "arrivals";

    QHash< uint, TimetableData > stillUsedAdditionalData;
    foreach( const DepartureInfoPtr &departureInfo, items ) {
        QVariantHash departureData;
        TimetableData departure = departureInfo->data();
        for ( TimetableData::ConstIterator it = departure.constBegin();
              it != departure.constEnd(); ++it )
        {
            if ( it.value().isValid() ) {
                departureData.insert( Global::timetableInformationToString(it.key()), it.value() );
            }
        }

        departureData.insert( "Nightline", departureInfo->isNightLine() );
        departureData.insert( "Expressline", departureInfo->isExpressLine() );

        // Add existing additional data
        const uint hash = hashForDeparture( departure );
        if ( dataSource->additionalData().contains(hash) ) {
            // Found already downloaded additional data, add it to the updated departure data
            const TimetableData additionalData = dataSource->additionalData( hash );
            for ( TimetableData::ConstIterator it = additionalData.constBegin();
                  it != additionalData.constEnd(); ++it )
            {
                departureData.insert( Global::timetableInformationToString(it.key()), it.value() );
            }
            departureData[ "IncludesAdditionalData" ] = true;

            stillUsedAdditionalData.insert( hash, additionalData );
        }

        departuresData << departureData;
    }

    // Store still used additional data, ie. remove no longer used additional data
    dataSource->setAdditionalData( stillUsedAdditionalData );
    dataSource->setValue( itemKey, departuresData );

//     if ( deleteDepartureInfos ) {
//         kDebug() << "Delete" << items.count() << "departures/arrivals";
//         qDeleteAll( departures );
//     }

    // Fill the data source with general information
    const QDateTime dateTime = QDateTime::currentDateTime();
    dataSource->setValue( "serviceProvider", provider->id() );
    dataSource->setValue( "delayInfoAvailable", globalInfo.delayInfoAvailable );
    dataSource->setValue( "requestUrl", requestUrl );
    dataSource->setValue( "parseMode", request.parseModeName() );
    dataSource->setValue( "error", false );
    dataSource->setValue( "updated", dateTime );

    // Store a proposal for the next download time
    QDateTime last = items.isEmpty() ? dateTime
            : items.last()->value(Enums::DepartureDateTime).toDateTime();
    dataSource->setNextDownloadTimeProposal( dateTime.addSecs(dateTime.secsTo(last) / 3) );
    const QDateTime nextUpdateTime = provider->nextUpdateTime( dataSource->updateFlags(),
            dateTime, dataSource->nextDownloadTimeProposal(), dataSource->data() );
    const QDateTime minManualUpdateTime = provider->nextUpdateTime(
            dataSource->updateFlags() | UpdateWasRequestedManually,
            dateTime, dataSource->nextDownloadTimeProposal(), dataSource->data() );

    // Store update times in the data source
    dataSource->setValue( "nextAutomaticUpdate", nextUpdateTime );
    dataSource->setValue( "minManualUpdateTime", minManualUpdateTime );

    // Publish the data source and cache it
    setData( sourceName, dataSource->data() );
    m_dataSources[ nonAmbiguousName ] = dataSource;

    int msecsUntilUpdate = dateTime.msecsTo( nextUpdateTime );
    Q_ASSERT( msecsUntilUpdate >= 10000 ); // Make sure to not produce too many updates by mistake
    DEBUG_ENGINE_JOBS( "Update data source in"
                       << KGlobal::locale()->prettyFormatDuration(msecsUntilUpdate) );
    if ( !dataSource->updateTimer() ) {
        QTimer *updateTimer = new QTimer( this );
        connect( updateTimer, SIGNAL(timeout()), this, SLOT(updateTimeout()) );
        dataSource->setUpdateTimer( updateTimer );
    }
    dataSource->updateTimer()->start( msecsUntilUpdate );
}

void PublicTransportEngine::updateTimeout()
{
    // Find the timetable data source to which the timer belongs which timeout() signal was emitted
    QTimer *timer = qobject_cast< QTimer* >( sender() );
    TimetableDataSource *dataSource = dataSourceFromTimer( timer );
    if ( !dataSource ) {
        // No data source found that started the timer,
        // should not happen the data source should have deleted the timer on destruction
        kWarning() << "Timeout received from an unknown update timer";
        return;
    }

    // Found the timetable data source of the timer,
    // requests updates for all connected sources (possibly multiple combined stops)
    foreach ( const QString &sourceName, dataSource->usingDataSources() ) {
        updateTimetableDataSource( SourceRequestData(sourceName) );
    }
}

void PublicTransportEngine::additionalDataReceived( ServiceProvider *provider,
        const QUrl &requestUrl, const TimetableData &data, const AdditionalDataRequest &request )
{
    Q_UNUSED( provider );
    Q_UNUSED( requestUrl );

    // Check if the destination data source exists
    const QString nonAmbiguousName = disambiguateSourceName( request.sourceName() );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        kWarning() << "Additional data received for a source that was already removed:"
                   << nonAmbiguousName;
        emit additionalDataRequestFinished( QVariantHash(), false,
                                            "Data source to update was already removed" );
        return;
    }

    // Get the list of timetable items from the existing data source
    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    Q_ASSERT( dataSource );
    QVariantList items = dataSource->timetableItems();
    if ( request.itemNumber() >= items.count() ) {
        emit additionalDataRequestFinished( QVariantHash(), false,
                                            "Item to update not found in the data source" );
        return;
    }

    // Get the timetable item for which additional data was received
    // and insert the new data into it
    QVariantHash item = items[ request.itemNumber() ].toHash();
    for ( TimetableData::ConstIterator it = data.constBegin(); it != data.constEnd(); ++it ) {
        item.insert( Global::timetableInformationToString(it.key()), it.value() );
    }
    item[ "IncludesAdditionalData" ] = true;
    item.remove( "WaitingForAdditionalData" );

    // Store the changed item back into the item list of the data source
    items[ request.itemNumber() ] = item;
    dataSource->setTimetableItems( items );

    // Also store received additional data separately
    // to not loose additional data after updating the data source
    const uint hash = hashForDeparture( item );
    dataSource->setAdditionalData( hash, data );

    QTimer *updateDelayTimer;
    if ( dataSource->updateAdditionalDataDelayTimer() ) {
        // Use existing timer, restart it with a longer interval and
        // publish the new data of the data source after the timeout
        updateDelayTimer = dataSource->updateAdditionalDataDelayTimer();
        updateDelayTimer->setInterval( 250 );
    } else {
        // Create timer with a shorter interval, but directly publish the new data of the
        // data source. The timer is used here to delay further publishing of new data,
        // ie. combine multiple updates and publish them at once.
        setData( request.sourceName(), dataSource->data() );
        updateDelayTimer = new QTimer( this );
        updateDelayTimer->setInterval( 150 );
        connect( updateDelayTimer, SIGNAL(timeout()),
                 this, SLOT(updateDataSourcesWithNewAdditionData()) );
        dataSource->setUpdateAdditionalDataDelayTimer( updateDelayTimer );
    }

    // (Re-)start the additional data update timer
    updateDelayTimer->start();

    // Emit result
    emit additionalDataRequestFinished( item, true );
}

TimetableDataSource *PublicTransportEngine::dataSourceFromTimer( QTimer *timer ) const
{
    for ( QHash< QString, DataSource* >::ConstIterator it = m_dataSources.constBegin();
          it != m_dataSources.constEnd(); ++it )
    {
        TimetableDataSource *dataSource = dynamic_cast< TimetableDataSource* >( *it );
        if ( dataSource && (dataSource->updateAdditionalDataDelayTimer() == timer ||
                            dataSource->updateTimer() == timer) ) 
        {
            return dataSource;
        }
    }

    // The timer is not used any longer
    return 0;
}

void PublicTransportEngine::updateDataSourcesWithNewAdditionData()
{
    QTimer *updateDelayTimer = qobject_cast< QTimer* >( sender() );
    Q_ASSERT( updateDelayTimer );
    TimetableDataSource *dataSource = dataSourceFromTimer( updateDelayTimer );
    if ( !dataSource ) {
        kWarning() << "Data source was already removed";
        return;
    }

    const int interval = updateDelayTimer->interval();
    dataSource->setUpdateAdditionalDataDelayTimer( 0 ); // Deletes the timer

    // Do the upate, but only after long delays,
    // because the update is already done before short delays
    if ( interval > 150 ) {
        // Was delayed for a longer time
        publishData( dataSource );
    }
}

void PublicTransportEngine::departuresReceived( ServiceProvider *provider,
        const QUrl &requestUrl, const DepartureInfoList &departures,
        const GlobalTimetableInfo &globalInfo, const DepartureRequest &request,
        bool deleteDepartureInfos )
{
    timetableDataReceived( provider, requestUrl, departures, globalInfo, request,
                           deleteDepartureInfos, true );
}

void PublicTransportEngine::arrivalsReceived( ServiceProvider *provider, const QUrl &requestUrl,
        const ArrivalInfoList &arrivals, const GlobalTimetableInfo &globalInfo,
        const ArrivalRequest &request, bool deleteDepartureInfos )
{
    timetableDataReceived( provider, requestUrl, arrivals, globalInfo, request,
                           deleteDepartureInfos, false );
}

void PublicTransportEngine::journeysReceived( ServiceProvider* provider,
        const QUrl &requestUrl, const JourneyInfoList &journeys,
        const GlobalTimetableInfo &globalInfo,
        const JourneyRequest &request,
        bool deleteJourneyInfos )
{
    Q_UNUSED( provider );
    Q_UNUSED( deleteJourneyInfos );
    const QString sourceName = request.sourceName();
    DEBUG_ENGINE_JOBS( journeys.count() << "journeys received" << sourceName );

    const QString nonAmbiguousName = disambiguateSourceName( sourceName );
    m_runningSources.removeOne( nonAmbiguousName );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        kWarning() << "Data source already removed" << nonAmbiguousName;
        return;
    }
    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    if ( !dataSource ) {
        kWarning() << "Data source already deleted" << nonAmbiguousName;
        return;
    }
    dataSource->clear();
    QVariantList journeysData;
    foreach( const JourneyInfoPtr &journeyInfo, journeys ) {
        if ( !journeyInfo->isValid() ) {
            continue;
        }

        QVariantHash journeyData;
        TimetableData journey = journeyInfo->data();
        for ( TimetableData::ConstIterator it = journey.constBegin();
              it != journey.constEnd(); ++it )
        {
            if ( it.value().isValid() ) {
                journeyData.insert( Global::timetableInformationToString(it.key()), it.value() );
            }
        }

        journeysData << journeyData;
    }

    dataSource->setValue( "journeys", journeysData );

    int journeyCount = journeys.count();
    QDateTime first, last;
    if ( journeyCount > 0 ) {
        first = journeys.first()->value(Enums::DepartureDateTime).toDateTime();
        last = journeys.last()->value(Enums::DepartureDateTime).toDateTime();
    } else {
        first = last = QDateTime::currentDateTime();
    }
//     if ( deleteJourneyInfos ) {
//         qDeleteAll( journeys ); TODO
//     }

    // Store a proposal for the next download time
    int secs = ( journeyCount / 3 ) * first.secsTo( last );
    QDateTime downloadTime = QDateTime::currentDateTime().addSecs( secs );
    dataSource->setNextDownloadTimeProposal( downloadTime );

    // Store received data in the data source map
    dataSource->setValue( "serviceProvider", provider->id() );
    dataSource->setValue( "delayInfoAvailable", globalInfo.delayInfoAvailable );
    dataSource->setValue( "requestUrl", requestUrl );
    dataSource->setValue( "parseMode", request.parseModeName() );
    dataSource->setValue( "error", false );
    dataSource->setValue( "updated", QDateTime::currentDateTime() );
    dataSource->setValue( "nextAutomaticUpdate", downloadTime );
    dataSource->setValue( "minManualUpdateTime", downloadTime ); // TODO
    setData( sourceName, dataSource->data() );
    m_dataSources[ nonAmbiguousName ] = dataSource;
}

void PublicTransportEngine::stopsReceived( ServiceProvider *provider,
        const QUrl &requestUrl, const StopInfoList &stops,
        const StopSuggestionRequest &request, bool deleteStopInfos )
{
    Q_UNUSED( provider );
    Q_UNUSED( deleteStopInfos );

    const QString sourceName = request.sourceName();
    m_runningSources.removeOne( disambiguateSourceName(sourceName) );
    DEBUG_ENGINE_JOBS( stops.count() << "stop suggestions received" << sourceName );

    QVariantList stopsData;
    foreach( const StopInfoPtr &stopInfo, stops ) {
        QVariantHash stopData;
        TimetableData stop = stopInfo->data();
        for ( TimetableData::ConstIterator it = stop.constBegin();
              it != stop.constEnd(); ++it )
        {
            if ( it.value().isValid() ) {
                stopData.insert( Global::timetableInformationToString(it.key()), it.value() );
            }
        }
        stopsData << stopData;
    }
    setData( sourceName, "stops", stopsData );
    setData( sourceName, "serviceProvider", provider->id() );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", request.parseModeName() );
    setData( sourceName, "error", false );
    setData( sourceName, "updated", QDateTime::currentDateTime() );

//     if ( deleteStopInfos ) {
//         qDeleteAll( stops ); TODO
//     }
}

void PublicTransportEngine::requestFailed( ServiceProvider *provider,
        ErrorCode errorCode, const QString &errorMessage,
        const QUrl &requestUrl, const AbstractRequest *request )
{
    Q_UNUSED( provider );
    kDebug() << "Error while parsing" << requestUrl //<< request->serviceProvider
             << "\n  sourceName =" << request->sourceName() << request->parseMode();
    kDebug() << errorCode << errorMessage;

    // Remove erroneous source from running sources list
    m_runningSources.removeOne( disambiguateSourceName(request->sourceName()) );

    const QString sourceName = request->sourceName();
    setData( sourceName, "serviceProvider", provider->id() );
    setData( sourceName, "requestUrl", requestUrl );
    setData( sourceName, "parseMode", request->parseModeName() );
    setData( sourceName, "receivedData", "nothing" );
    setData( sourceName, "error", true );
    setData( sourceName, "errorCode", errorCode );
    setData( sourceName, "errorMessage", errorMessage );
    setData( sourceName, "updated", QDateTime::currentDateTime() );
}

bool PublicTransportEngine::requestUpdate( const QString &sourceName, QString *errorMessage )
{
    // Find the TimetableDataSource object for sourceName
    const QString nonAmbiguousName = disambiguateSourceName( sourceName );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        kWarning() << "Not an existing timetable data source:" << sourceName;
        if ( errorMessage ) {
            *errorMessage = i18nc("@info", "Data source is not an existing timetable "
                                  "data source: %1", sourceName);
        }
        return false;
    }
    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    if ( !dataSource ) {
        kWarning() << "Internal error: Invalid pointer to the data source stored";
        return false;
    }

    // Check if updates are blocked to prevent to many useless updates
    const QDateTime nextUpdateTime = sourceUpdateTime( dataSource, UpdateWasRequestedManually );
    if ( QDateTime::currentDateTime() < nextUpdateTime ) {
        kDebug() << "Too early to update again, update request rejected" << nextUpdateTime;
        if ( errorMessage ) {
            *errorMessage = i18nc("@info", "Update request rejected, earliest update time: %1",
                                  nextUpdateTime.toString());
        }
        return false;
    }

    // Stop automatic updates
    dataSource->stopUpdateTimer();

    // Start the request
    return request( sourceName );
}

bool PublicTransportEngine::requestMoreItems( const QString &sourceName,
                                              Enums::MoreItemsDirection direction,
                                              QString *errorMessage )
{
    // Find the TimetableDataSource object for sourceName
    const QString nonAmbiguousName = disambiguateSourceName( sourceName );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        kWarning() << "Not an existing timetable data source:" << sourceName;
        if ( errorMessage ) {
            *errorMessage = i18nc("@info", "Data source is not an existing timetable "
                                  "data source: %1", sourceName);
        }
        return false;
    } else if ( m_runningSources.contains(nonAmbiguousName) ) {
        // The data source currently gets processed or updated, including requests for more items
        kDebug() << "Source currently gets processed, please wait" << sourceName;
        if ( errorMessage ) {
            *errorMessage = i18nc("@info", "Source currently gets processed, "
                                  "please try again later");
        }
        return false;
    }

    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    if ( !dataSource ) {
        kWarning() << "Internal error: Invalid pointer to the data source stored";
        return false;
    }

    const SourceRequestData data( sourceName );
    kDebug() << data.defaultParameter;
    const ProviderPointer provider = providerFromId( data.defaultParameter );
    if ( provider.isNull() ) {
        // Service provider couldn't be created, should only happen after provider updates,
        // where the provider worked to get timetable items, but the new provider version
        // does not work any longer and no more items can be requested
        return false;
    }

    // Start the request
    QSharedPointer< AbstractRequest > request = dataSource->request( sourceName );
    const QVariantList items = dataSource->timetableItems();
    if ( items.isEmpty() ) {
        // TODO
        kWarning() << "No timetable items in data source" << sourceName;
//         emit moreItemsRequestFinished( QVariantHash(), false,
//                                             "No items" );
        return false;
    }
    const QVariantHash item =
            (direction == Enums::EarlierItems ? items.first() : items.last()).toHash();
    const QVariantMap _requestData = item[Enums::toString(Enums::RequestData)].toMap();

    // TODO Convert to hash from map..
    QVariantHash requestData;
    for ( QVariantMap::ConstIterator it = _requestData.constBegin(); it != _requestData.constEnd(); ++it ) {
        requestData.insert( it.key(), it.value() );
    }

    // Start the request
    m_runningSources << nonAmbiguousName;
    provider->requestMoreItems( MoreItemsRequest(sourceName, request, requestData, direction) );
    return true;
}

ErrorCode PublicTransportEngine::errorCodeFromState( const QString &stateId )
{
    if ( stateId == QLatin1String("ready") ) {
        return NoError;
    } else if ( stateId == QLatin1String("gtfs_feed_import_pending") ||
                stateId == QLatin1String("importing_gtfs_feed") )
    {
        return ErrorNeedsImport;
    } else {
        return ErrorParsingFailed;
    }
}

bool PublicTransportEngine::request( const SourceRequestData &data )
{
    // Try to get the specific provider from m_providers (if it's not in there it is created)
    const ProviderPointer provider = providerFromId( data.defaultParameter );
    if ( provider.isNull() || provider->type() == Enums::InvalidProvider ) {
        // Service provider couldn't be created
        // Remove erroneous source from running sources list
        const QString sourceName = data.request->sourceName();
        m_runningSources.removeOne( disambiguateSourceName(sourceName) );

        ProvidersDataSource *dataSource = providersDataSource();
        const QString state = dataSource->providerState( data.defaultParameter );
        if ( state != QLatin1String("ready") ) {
            const QVariantHash stateData = dataSource->providerStateData( data.defaultParameter );
            setData( sourceName, "serviceProvider", data.defaultParameter );
            setData( sourceName, "parseMode", data.request->parseModeName() );
            setData( sourceName, "receivedData", "nothing" );
            setData( sourceName, "error", true );
            setData( sourceName, "errorCode", errorCodeFromState(state) );
            setData( sourceName, "errorMessage", stateData["statusMessage"].toString() );
            setData( sourceName, "updated", QDateTime::currentDateTime() );
        }
        return false;
    } else if ( provider->useSeparateCityValue() && data.request->city().isEmpty() &&
                !dynamic_cast<StopsByGeoPositionRequest*>(data.request) )
    {
        kDebug() << QString("Service provider %1 needs a separate city value. Add to source name "
                            "'|city=X', where X stands for the city name.")
                    .arg( data.defaultParameter );
        return false; // Service provider needs a separate city value
    } else if ( !provider->features().contains(Enums::ProvidesJourneys) &&
                (data.parseMode == ParseForJourneysByDepartureTime ||
                 data.parseMode == ParseForJourneysByArrivalTime) )
    {
        kDebug() << QString("Service provider %1 doesn't support journey searches.")
                    .arg( data.defaultParameter );
        return false; // Service provider doesn't support journey searches
    }

    // Store source name as currently being processed, to not start another
    // request if there is already a running one
    m_runningSources << disambiguateSourceName( data.name );

    // Start the request
    provider->request( data.request );
    return true;
}

int PublicTransportEngine::secsUntilUpdate( const QString &sourceName, QString *errorMessage )
{
    return getSecsUntilUpdate( sourceName, errorMessage );
}

int PublicTransportEngine::minSecsUntilUpdate( const QString &sourceName, QString *errorMessage )
{
    return getSecsUntilUpdate( sourceName, errorMessage, UpdateWasRequestedManually );
}

int PublicTransportEngine::getSecsUntilUpdate( const QString &sourceName, QString *errorMessage,
                                               UpdateFlags updateFlags )
{
    const QString nonAmbiguousName = disambiguateSourceName( sourceName );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        // The data source is not a timetable data source or does not exist
        if ( errorMessage ) {
            *errorMessage = i18nc("@info", "Data source is not an existing timetable "
                                  "data source: %1", sourceName);
        }
        return -1;
    }
    TimetableDataSource *dataSource = dynamic_cast< TimetableDataSource* >(
            m_dataSources[nonAmbiguousName] );
    if ( !dataSource ) {
        kWarning() << "Internal error: Invalid pointer to the data source stored";
        return -1;
    }

    const QDateTime time = sourceUpdateTime( dataSource, updateFlags );
    return qMax( -1, QDateTime::currentDateTime().secsTo(time) );
}

QDateTime PublicTransportEngine::sourceUpdateTime( TimetableDataSource *dataSource,
                                                   UpdateFlags updateFlags )
{
    const QString providerId = dataSource->providerId();
    if ( providerId.isEmpty() ) {
        kWarning() << "Internal error: Service provider unknown"; // Could get provider id from <name>
        return QDateTime();
    }
    const ProviderPointer provider = providerFromId( providerId );
    if ( provider.isNull() ) {
        return QDateTime();
    }

    return provider->nextUpdateTime( dataSource->updateFlags() | updateFlags,
                                     dataSource->lastUpdate(),
                                     dataSource->nextDownloadTimeProposal(), dataSource->data() );
}

bool PublicTransportEngine::isSourceUpToDate( const QString &nonAmbiguousName,
                                              UpdateFlags updateFlags )
{
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        return false;
    }

    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    if ( !dataSource ) {
        // The data source is not a timetable data source, no periodic updates needed
        return true;
    }

    const QDateTime nextUpdateTime = sourceUpdateTime( dataSource, updateFlags );
    DEBUG_ENGINE_JOBS( "Wait time until next download:" << KGlobal::locale()->prettyFormatDuration(
                       (QDateTime::currentDateTime().msecsTo(nextUpdateTime))) );
    return QDateTime::currentDateTime() < nextUpdateTime;
}

ServiceProvider *PublicTransportEngine::createProvider( const QString &serviceProviderId,
        QObject *parent, const QSharedPointer<KConfig> &cache )
{
    ServiceProviderData *data = ServiceProviderDataReader::read( serviceProviderId );
    return data ? createProviderForData(data, parent, cache) : 0;
}

ServiceProvider *PublicTransportEngine::createProviderForData( const ServiceProviderData *data,
        QObject *parent, const QSharedPointer<KConfig> &cache )
{
    if ( !ServiceProviderGlobal::isProviderTypeAvailable(data->type()) ) {
        kWarning() << "Cannot create provider of type" << data->typeName() << "because the engine "
                      "has been built without support for that provider type";
        return 0;
    }

    // Warn if the format of the provider plugin is unsupported
    if ( data->fileFormatVersion() != QLatin1String("1.1") ) {
        kWarning() << "The Provider" << data->id() << "was designed for an unsupported "
                "provider plugin format version, update to version 1.1";
        return 0;
    }

    switch ( data->type() ) {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case Enums::ScriptedProvider:
        return new ServiceProviderScript( data, parent, cache );
#endif
#ifdef BUILD_PROVIDER_TYPE_GTFS
    case Enums::GtfsProvider:
        return new ServiceProviderGtfs( data, parent, cache );
#endif
    case Enums::InvalidProvider:
    default:
        kWarning() << "Invalid/unknown provider type" << data->type();
        return 0;
    }
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE( publictransport, PublicTransportEngine )

// this is needed since PublicTransportEngine is a QObject
#include "build/publictransportdataengine.moc"
