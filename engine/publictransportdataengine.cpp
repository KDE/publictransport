/*
 *   Copyright 2013 Friedrich Pülz <fpuelz@gmx.de>
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
#include <KLocalizedString>
#include <KStandardDirs>
#include <KGlobal>
#include <KConfig>
#include <KLocale>

// Qt includes
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QTimer>
#include <QDebug>
#include <QtDBus/QDBusConnection>
#include <QStandardPaths>

const int PublicTransportEngine::DEFAULT_TIME_OFFSET = 0;
const int PublicTransportEngine::PROVIDER_CLEANUP_TIMEOUT = 10000; // 10 seconds

Plasma::Service* PublicTransportEngine::serviceForSource( const QString &name )
{
#ifdef BUILD_PROVIDER_TYPE_GTFS
    // Return the GTFS service for "GTFS" or "gtfs" names
    if ( name.toLower() == QLatin1String("gtfs") ) {
        GtfsService *service = new GtfsService( name, this );
        service->setDestination( name );

        QStringList providersList = ServiceProviderGlobal::installedProviders();
        QString defaultProviderFile = providersList.first();
        QString defaultProviderId   = ServiceProviderGlobal::idFromFileName(defaultProviderFile);

        QVariantMap importFeedParams = service->operationDescription("importGtfsFeed"),
                    updateFeedParams = service->operationDescription("updateGtfsFeedInfo"),
                    updateDbParams = service->operationDescription("updateGtfsDatabase"),
                    deleteDbParams = service->operationDescription("deleteGtfsDatabase");

        importFeedParams.insert("serviceProviderId", defaultProviderId);
        updateFeedParams.insert("serviceProviderId", defaultProviderId);
        updateDbParams.insert("serviceProviderId", defaultProviderId);
        deleteDbParams.insert("serviceProviderId", defaultProviderId);

        Plasma::ServiceJob  *importFeedJob = service->startOperationCall(importFeedParams),
                            *updateFeedJob = service->startOperationCall(updateFeedParams),
                            *updateDbJob = service->startOperationCall(updateDbParams),
                            *deleteDbJob = service->startOperationCall(deleteDbParams);

        connect( importFeedJob, SIGNAL(finished(KJob*)), this, SLOT(gtfsServiceJobFinished(KJob*)) );
        connect( updateFeedJob, SIGNAL(finished(KJob*)), this, SLOT(gtfsServiceJobFinished(KJob*)) );
        connect( updateDbJob, SIGNAL(finished(KJob*)), this, SLOT(gtfsServiceJobFinished(KJob*)) );
        connect( deleteDbJob, SIGNAL(finished(KJob*)), this, SLOT(gtfsServiceJobFinished(KJob*)) );

        return service;
    }
#endif

    // If the name of a data requesting source is given, return the timetable service
    const SourceType type = sourceTypeFromName( name );
    if ( isDataRequestingSourceType(type) ) {
        const QString nonAmbiguousName = disambiguateSourceName( name );
        if ( m_dataSources.contains(nonAmbiguousName) ) {
            // Data source exists
            TimetableService *service = new TimetableService( this, name, this );
            service->setDestination( name );
            return service;
        }
    }

    // No service for the given name found
    return 0;
}

void PublicTransportEngine::publishData( DataSource *dataSource,
                                         const QString &newlyRequestedProviderId )
{
    Q_ASSERT( dataSource );
    TimetableDataSource *timetableDataSource = dynamic_cast< TimetableDataSource* >( dataSource );
    if ( timetableDataSource ) {
        foreach ( const QString &usingDataSource, timetableDataSource->usingDataSources() ) {
            setData( usingDataSource, timetableDataSource->data() );
        }
        return;
    }

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

        if ( !newlyRequestedProviderId.isEmpty() ) {
            const QString providerSource =
                    sourceTypeKeyword(ServiceProviderSource) + ' ' + newlyRequestedProviderId;
            removeAllData( providerSource );
            setData( providerSource, providersSource->providerData(newlyRequestedProviderId) );
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
    QVariantMap stateData = dataSource->providerStateData( providerId );
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
    for ( QVariantMap::ConstIterator it = stateData.constBegin();
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

void PublicTransportEngine::gtfsServiceJobFinished( KJob *job )
{
    // Disconnect messages of the job
    disconnect( job, SIGNAL(infoMessage(KJob*,QString,QString)),
                this, SLOT(gtfsImportJobInfoMessage(KJob*,QString,QString)) );
    disconnect( job, SIGNAL(percent(KJob*,ulong)),
                this, SLOT(gtfsImportJobPercent(KJob*,ulong)) );

    // Check that the job was not canceled because another database job was already running
    const bool canAccessGtfsDatabase = job->property("canAccessGtfsDatabase").toBool();
    const bool isAccessingGtfsDatabase = job->property("isAccessingGtfsDatabase").toBool();
    const QString providerId = job->property( "serviceProviderId" ).toString();
    if ( (!canAccessGtfsDatabase && isAccessingGtfsDatabase) || providerId.isEmpty() ) {
        // Invalid job or cancelled, because another import job is already running
        // for the provider
        return;
    }

    // Reset state in "ServiceProviders", "ServiceProvider <id>" data sources
    // Do not read and give the current feed URL for the provider to updateProviderState(),
    // because the feed URL should not have changed since the beginning of the feed import
    QVariantMap stateData;
    const QString state = updateProviderState( providerId, &stateData, "GTFS", QString(), false );
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
    QVariantMap stateData = dataSource->providerStateData( providerId );
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
    QVariantMap stateData = dataSource->providerStateData( providerId );
    stateData[ "progress" ] = int( percent );
    dataSource->setProviderStateData( providerId, stateData );
    publishData( dataSource );
}
#endif // BUILD_PROVIDER_TYPE_GTFS

PublicTransportEngine::PublicTransportEngine( QObject* parent, const QVariantList& args )
        : Plasma::DataEngine( parent, args ),
        m_fileSystemWatcher(0), m_providerUpdateDelayTimer(0), m_cleanupTimer(0)
{
    // We ignore any arguments - data engines do not have much use for them
    Q_UNUSED( args )

    // This prevents applets from setting an unnecessarily high update interval
    // and using too much CPU.
    // 60 seconds should be enough, departure / arrival times have minute precision (except for GTFS).
    setMinimumPollingInterval( 60000 );

    // Cleanup the cache from obsolete data for providers that were uninstalled
    // while the engine was not running
    ServiceProviderGlobal::cleanupCache();

    // Get notified when data sources are no longer used
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
    const QString saveDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/') + installationSubDirectory;
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
    if ( !m_runningSources.isEmpty() || !m_providers.isEmpty() ) {
        qDebug() << m_runningSources.count() << "data sources are still being updated,"
                 << m_providers.count() << "providers used, abort and delete all providers";
        QStringList providerIds = m_providers.keys();
        foreach ( const QString &providerId, providerIds ) {
            deleteProvider( providerId, false );
        }
    }

    delete m_fileSystemWatcher;
    delete m_providerUpdateDelayTimer;
    qDeleteAll( m_dataSources );
    m_dataSources.clear();

    // Providers cached in m_cachedProviders get deleted automatically (QSharedPointer)
    // and need no special handling, since they are not used currently
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
                // Move provider from the list of used providers to the list of cached providers
                m_cachedProviders.insert( providerId, m_providers.take(providerId) );
            }
        }

        // Start the cleanup timer, will delete cached providers after a timeout
        startCleanupLater();

        // The data source is no longer used, delete it
        delete dataSource;
    } else if ( nonAmbiguousName.startsWith(sourceTypeKeyword(ServiceProviderSource)) ) {
        // A "ServiceProvider xx_xx" data source was removed, data for these sources
        // is stored in the "ServiceProviders" data source object in m_dataSources.
        // Remove not installed providers also from the "ServiceProviders" data source.
        const QString providerId = nonAmbiguousName.mid(
                QString(sourceTypeKeyword(ServiceProviderSource)).length() + 1 );
        if ( !providerId.isEmpty() && !isProviderUsed(providerId) ) {
            // Test if the provider is installed
            const QStringList providerPaths = ServiceProviderGlobal::installedProviders();
            bool isInstalled = false;
            foreach ( const QString &providerPath, providerPaths ) {
                const QString &installedProviderId =
                        ServiceProviderGlobal::idFromFileName( providerPath );
                if ( providerId == installedProviderId ) {
                    isInstalled = true;
                    break;
                }
            }
            if ( !isInstalled ) {
                qDebug() << "Remove provider" << providerId;
                // Provider is not installed, remove it from the "ServiceProviders" data source
                providersDataSource()->removeProvider( providerId );
                removeData( sourceTypeKeyword(ServiceProvidersSource), providerId );
            }
        }
    }
}

void PublicTransportEngine::startCleanupLater()
{
    // Create the timer if it is not currently running
    if ( !m_cleanupTimer ) {
        m_cleanupTimer = new QTimer( this );
        m_cleanupTimer->setInterval( PROVIDER_CLEANUP_TIMEOUT );
        connect( m_cleanupTimer, SIGNAL(timeout()), this, SLOT(cleanup()) );
    }

    // (Re)start the timer
    m_cleanupTimer->start();
}

void PublicTransportEngine::cleanup()
{
    // Delete the timer
    delete m_cleanupTimer;
    m_cleanupTimer = 0;

    // Remove all shared pointers of unused cached providers,
    // ie. delete the provider objects
    m_cachedProviders.clear();
}

QVariantMap PublicTransportEngine::serviceProviderData( const ServiceProvider *provider )
{
    Q_ASSERT( provider );
    return serviceProviderData( *(provider->data()), provider );
}

QVariantMap PublicTransportEngine::serviceProviderData( const ServiceProviderData &data,
                                                         const ServiceProvider *provider )
{
    QVariantMap dataServiceProvider;
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
            qDebug() << "No cached feature data was found for provider" << data.id();
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

QVariantMap PublicTransportEngine::locations()
{
    QVariantMap ret;
    const QStringList providers = ServiceProviderGlobal::installedProviders();

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
                        ServiceProviderGlobal::defaultProviderForLocation( location );

                // Extract service provider ID from the filename
                const QString defaultProviderId =
                        ServiceProviderGlobal::idFromFileName( defaultProviderFileName );

                // Store location values in a hash and insert it into [ret]
                QVariantMap locationMap;
                locationMap.insert( "name", location );
                if ( location == "international" ) {
                    locationMap.insert( "description", i18n("International providers. "
                                         "There is one for getting flight departures/arrivals.") );
                } else {
                    locationMap.insert( "description", i18n("Service providers for %1.",
                            KGlobal::locale()->countryCodeToName(location)) );
                }
                locationMap.insert( "defaultProvider", defaultProviderId );
                ret.insert( location, locationMap );
            }
        }
    }

    return ret;
}

PublicTransportEngine::ProviderPointer PublicTransportEngine::providerFromId(
        const QString &id, bool *newlyCreated )
{
    if ( m_providers.contains(id) ) {
        // The provider was already created and is currently used by the engine
        if ( newlyCreated ) {
            *newlyCreated = false;
        }
        return m_providers[ id ];
    } else if ( m_cachedProviders.contains(id) ) {
        // The provider was already created, is now unused by the engine,
        // but is still cached (cleanup timeout not reached yet)
        if ( newlyCreated ) {
            *newlyCreated = false;
        }

        // Move provider back from the list of cached providers to the list of used providers
        const ProviderPointer provider = m_cachedProviders.take( id );
        m_providers.insert( id, provider );
        return provider;
    } else {
        // Provider not currently used or cached
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
                qWarning() << "Provider" << id << "is not ready, state is" << state;
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
        QVariantMap locations = m_dataSources[ sourceTypeKeyword(LocationsSource) ]->data();
        QVariantMap locationCountry = locations[ data.defaultParameter.toLower() ].toMap();
        QString defaultProvider = locationCountry[ "defaultProvider" ].toString();
        if ( defaultProvider.isEmpty() ) {
            // No provider for the location found
            return false;
        }

        providerId = defaultProvider;
    }

    updateProviderData( providerId );
    publishData( providersDataSource(), providerId );
    return true;
}

bool PublicTransportEngine::updateProviderData( const QString &providerId,
                                                const QSharedPointer<KConfig> &cache )
{
    QVariantMap providerData;
    QString errorMessage;
    ProvidersDataSource *providersSource = providersDataSource();

    // Test if the provider is valid
    if ( testServiceProvider(providerId, &providerData, &errorMessage, cache) ) {
        QVariantMap stateData;
        const QString state = updateProviderState( providerId, &stateData,
                providerData["type"].toString(), providerData.value("feedUrl").toString() );
        providerData["error"] = false;
        providersSource->addProvider( providerId,
                ProvidersDataSource::ProviderData(providerData, state, stateData) );
        return true;
    } else {
        // Invalid provider
        providerData["id"] = providerId;
        providerData["error"] = true;
        providerData["errorMessage"] = errorMessage;

        // Prepare state data with the error message and a boolean whether or not the provider
        // is installed or could not be found (only possible if the provider data source was
        // manually requested)
        QVariantMap stateData;
        stateData["statusMessage"] = errorMessage;
        stateData["isInstalled"] = ServiceProviderGlobal::isProviderInstalled( providerId );

        providersSource->addProvider( providerId,
                ProvidersDataSource::ProviderData(providerData, "error", stateData) );
        return false;
    }
}

bool PublicTransportEngine::updateServiceProviderSource()
{
    const QString name = sourceTypeKeyword( ServiceProvidersSource );
    ProvidersDataSource *providersSource = providersDataSource();
    if ( providersSource->isDirty() ) {
        const QStringList providers = ServiceProviderGlobal::installedProviders();
        if ( providers.isEmpty() ) {
            qWarning() << "Could not find any service provider plugins";
        } else {
            QStringList loadedProviders;
            m_erroneousProviders.clear();
            QSharedPointer<KConfig> cache = ServiceProviderGlobal::cache();
            foreach( const QString &provider, providers ) {
                const QString providerId =
                        ServiceProviderGlobal::idFromFileName( QUrl(provider).fileName() );
                if ( updateProviderData(providerId, cache) ) {
                    loadedProviders << providerId;
                }
            }

            // Print information about loaded/erroneous providers
            qDebug() << "Loaded" << loadedProviders.count() << "service providers";
            if ( !m_erroneousProviders.isEmpty() ) {
                qWarning() << "Erroneous service provider plugins, that could not be loaded:"
                           << m_erroneousProviders;
            }
        }

        // Mark and update all providers that are no longer installed
        QSharedPointer< KConfig > cache = ServiceProviderGlobal::cache();
        const QStringList uninstalledProviderIDs = providersSource->markUninstalledProviders();
        foreach ( const QString &uninstalledProviderID, uninstalledProviderIDs ) {
            // Clear all values stored in the cache for the provider
            ServiceProviderGlobal::clearCache( uninstalledProviderID, cache );

            // Delete the provider and update it's state and other data
            deleteProvider( uninstalledProviderID );
            updateProviderData( uninstalledProviderID, cache );
        }

        // Insert the data source
        m_dataSources.insert( name, providersSource );
    }

    // Remove all old data, some service providers may have been updated and are now erroneous
    removeAllData( name );
    publishData( providersSource );

    return true;
}

QString PublicTransportEngine::updateProviderState( const QString &providerId,
                                                    QVariantMap *stateData,
                                                    const QString &providerType,
                                                    const QString &feedUrl,
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
                    providerId, feedUrl, cache, stateData );
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
        state = ServiceProviderGtfs::updateGtfsDatabaseState( providerId, feedUrl,
                                                              cache, stateData );
    }
#endif // BUILD_PROVIDER_TYPE_GTFS

    // Ensure a status message is given (or at least warn if not)
    if ( stateData->value("statusMessage").toString().isEmpty() ) {
        if ( state == QLatin1String("ready") ) {
            stateData->insert( "statusMessage",
                                i18nc("@info/plain", "The provider is ready to use") );
        } else {
            qWarning() << "Missing status message explaining why the provider" << providerId
                        << "is not ready";
        }
    }

    // Store the state in the cache
    group.writeEntry( "state", state );

    // Write state data to the cache
    KConfigGroup stateGroup = group.group( "stateData" );
    for ( QVariantMap::ConstIterator it = stateData->constBegin();
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
           stateDataKey != QLatin1String("gtfsDatabaseSize") &&
           stateDataKey != QLatin1String("gtfsDatabaseModifiedTime") &&
           stateDataKey != QLatin1String("gtfsFeedImported") &&
           stateDataKey != QLatin1String("gtfsFeedSize") &&
           stateDataKey != QLatin1String("gtfsFeedModifiedTime");
}

bool PublicTransportEngine::testServiceProvider( const QString &providerId,
                                                 QVariantMap *providerData, QString *errorMessage,
                                                 const QSharedPointer<KConfig> &_cache )
{
    const bool providerUsed = m_providers.contains( providerId );
    const bool providerCached = m_cachedProviders.contains( providerId );
    if ( providerUsed || providerCached ) {
        // The provider is cached in the engine, ie. it is valid,
        // use it's ServiceProviderData object
        const ProviderPointer provider = providerUsed
                ? m_providers[providerId] : m_cachedProviders[providerId];
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
                qDebug() << "Script changed" << providerId;
                testData.setSubTypeTestStatus( ServiceProviderTestData::Pending );
                testData.write( providerId, cache );
            }
            break;
#endif
        case Enums::GtfsProvider:
            break;
        case Enums::InvalidProvider:
        default:
            qWarning() << "Provider type unknown" << type;
            break;
        }
    }

    // Check the cache if the provider plugin .pts file can be read
    if ( testData.xmlStructureTestStatus() == ServiceProviderTestData::Failed ) {
        // The XML structure of the provider plugin .pts file is marked as failed in the cache
        // Cannot add provider data to data sources, the file needs to be fixed first
        qWarning() << "Provider plugin" << providerId << "is invalid.";
        qDebug() << "Fix the provider file at" << ServiceProviderGlobal::fileNameFromId(providerId);
        qDebug() << testData.errorMessage();
        qDebug() << "************************************";

        providerData->clear();
        *errorMessage = testData.errorMessage();
        m_erroneousProviders.insert( providerId, testData.errorMessage() );
        updateErroneousServiceProviderSource();
        return false;
    }
    // The sub type test may already be marked as failed in the cache,
    // but when provider data can be read it should be added to provider data source
    // also when the provider plugin is invalid

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
    }

    // Check if support for the used provider type has been build into the engine
    if ( !ServiceProviderGlobal::isProviderTypeAvailable(data->type()) ) {
        providerData->clear();
        _errorMessage = i18nc("@info/plain", "Support for provider type %1 is not available",
                              ServiceProviderGlobal::typeName(data->type(),
                                    ServiceProviderGlobal::ProviderTypeNameWithoutUnsupportedHint));
        *errorMessage = _errorMessage;
        m_erroneousProviders.insert( providerId, _errorMessage );
        updateErroneousServiceProviderSource();
        return false;
    }

    // Mark the XML test as passed if not done already
    if ( testData.isXmlStructureTestPending() ) {
        testData.setXmlStructureTestStatus( ServiceProviderTestData::Passed );
        testData.write( providerId, cache );
    }

    // XML file structure test is passed, run provider type test if not done already
    switch ( testData.subTypeTestStatus() ) {
    case ServiceProviderTestData::Pending: {
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
            qWarning() << "Test failed for" << providerId << testData.errorMessage();
            providerData->clear();
            *errorMessage = testData.errorMessage();
            m_erroneousProviders.insert( providerId, testData.errorMessage() );
            updateErroneousServiceProviderSource();
            return false;
        }

        // The provider is already created, use it in serviceProviderData(), if needed
        *providerData = serviceProviderData( provider.data() );
        break;
    }

    case ServiceProviderTestData::Failed:
        // Test is marked as failed in the cache
        *errorMessage = testData.errorMessage();
        m_erroneousProviders.insert( providerId, testData.errorMessage() );
        updateErroneousServiceProviderSource();
        *providerData = serviceProviderData( *data );
        return false;

    case ServiceProviderTestData::Passed:
        // Test is marked as passed in the cache
        *providerData = serviceProviderData( *data );
        break;
    }

    m_erroneousProviders.remove( providerId );
    const QLatin1String name = sourceTypeKeyword( ErroneousServiceProvidersSource );
    removeData( name, providerId );

    errorMessage->clear();
    return true;
}

bool PublicTransportEngine::updateErroneousServiceProviderSource()
{
    QVariantMap erroneousProvidersMap;
    const QLatin1String name = sourceTypeKeyword( ErroneousServiceProvidersSource );
    foreach(QString key, m_erroneousProviders.keys())
        erroneousProvidersMap[key] = m_erroneousProviders[key];
    setData( name, erroneousProvidersMap );
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
        qDebug() << "Source already gets processed, please wait" << data.name;
    } else if ( data.parseMode == ParseInvalid || !data.request ) {
        qWarning() << "Invalid source" << data.name;
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

void PublicTransportEngine::requestAdditionalData( const QString &sourceName,
                                                   int updateItem, int count )
{
    TimetableDataSource *dataSource = testDataSourceForAdditionalDataRequests( sourceName );
    if ( dataSource ) {
        // Start additional data requests
        bool dataChanged = false;
        for ( int itemNumber = updateItem; itemNumber < updateItem + count; ++itemNumber ) {
            dataChanged = requestAdditionalData(sourceName, itemNumber, dataSource) || dataChanged;
        }

        if ( dataChanged ) {
            // Publish changes to "additionalDataState" fields
            publishData( dataSource );
        }
    }
}

TimetableDataSource *PublicTransportEngine::testDataSourceForAdditionalDataRequests(
        const QString &sourceName )
{
    // Try to get a pointer to the provider with the provider ID from the source name
    const QString providerId = providerIdFromSourceName( sourceName );
    const ProviderPointer provider = providerFromId( providerId );
    if ( provider.isNull() || provider->type() == Enums::InvalidProvider ) {
        emit additionalDataRequestFinished( sourceName, -1, false,
                QString("Service provider %1 could not be created").arg(providerId) );
        return 0; // Service provider couldn't be created
    }

    // Test if the provider supports additional data
    if ( !provider->features().contains(Enums::ProvidesAdditionalData) ) {
        emit additionalDataRequestFinished( sourceName, -1, false,
                i18nc("@info/plain", "Additional data not supported") );
        qWarning() << "Additional data not supported by" << provider->id();
        return 0; // Service provider does not support additional data
    }

    // Test if the source with the given name is cached
    const QString nonAmbiguousName = disambiguateSourceName( sourceName );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        emit additionalDataRequestFinished( sourceName, -1, false,
                "Data source to update not found: " + sourceName );
        return 0;
    }

    // Get the data list, currently only for departures/arrivals TODO: journeys
    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    if ( !dataSource ) {
        emit additionalDataRequestFinished( sourceName, -1, false,
                "Data source is not a timetable data source: " + sourceName );
        return 0;
    }

    return dataSource;
}

bool PublicTransportEngine::requestAdditionalData( const QString &sourceName,
                                                   int itemNumber, TimetableDataSource *dataSource )
{
    QVariantList items = dataSource->timetableItems();
    if ( itemNumber >= items.count() || itemNumber < 0 ) {
        emit additionalDataRequestFinished( sourceName, itemNumber, false,
                QString("Item to update (%1) not found in data source").arg(itemNumber) );
        return false;
    }

    // Get the timetable item stored in the data source at the given index
    QVariantMap item = items[ itemNumber ].toMap();

    // Check if additional data is already included or was already requested
    const QString additionalDataState = item["additionalDataState"].toString();
    if ( additionalDataState == QLatin1String("included") ) {
        emit additionalDataRequestFinished( sourceName, itemNumber, false,
                QString("Additional data is already included for item %1").arg(itemNumber) );
        return false;
    } else if ( additionalDataState == QLatin1String("busy") ) {
        qDebug() << "Additional data for item" << itemNumber << "already requested, please wait";
        emit additionalDataRequestFinished( sourceName, itemNumber, false,
                QString("Additional data was already requested for item %1, please wait")
                .arg(itemNumber) );
        return false;
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
        emit additionalDataRequestFinished( sourceName, itemNumber, false,
                                            QString("Item to update is invalid: %1, %2, %3")
                                            .arg(dateTime.toString()).arg(transportLine, target) );
        return false;
    }

    // Store state of additional data in the timetable item
    item["additionalDataState"] = "busy";
    items[ itemNumber ] = item;
    dataSource->setTimetableItems( items );

    // Found data of the timetable item to update
    const SourceRequestData sourceData( dataSource->name() );
    const ProviderPointer provider = providerFromId( dataSource->providerId() );
    Q_ASSERT( provider );
    provider->requestAdditionalData( AdditionalDataRequest(dataSource->name(), itemNumber,
            sourceData.request->stop(), sourceData.request->stopId(), dateTime, transportLine,
            target, sourceData.request->city(), routeDataUrl) );
    return true;
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
    qDebug() << "No service provider ID given, using the default one for country"
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
            time = QDateTime::fromString( timeParameter.mid(9), Qt::ISODate );
            if ( !time.isValid() ) {
                time = QDateTime::fromString( timeParameter.mid(9) );
            }
        }

        // Round 15 minutes
        qint64 msecs = time.toMSecsSinceEpoch();
        time = QDateTime::fromMSecsSinceEpoch( msecs - msecs % (1000 * 60 * 15) );

        // Replace old time parameter with a new one
        ret.replace( rx.pos(), rx.matchedLength(),
                     QLatin1String("datetime=") + time.toString(Qt::ISODate) );
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
                parameters[ parameterName ] = parameterValue;
            }
        }
    }

    // Build non-ambiguous source name with standardized parameter order
    ret = typeKeyword + ' ' + defaultParameter;
    if ( parameters.contains(QLatin1String("city")) ) {
        ret += "|city=" + parameters["city"].toLower();
    }
    if ( parameters.contains(QLatin1String("stop")) ) {
        ret += "|stop=" + parameters["stop"].toLower();
    }
    if ( parameters.contains(QLatin1String("stopid")) ) {
        ret += "|stopid=" + parameters["stopid"];
    }
    if ( parameters.contains(QLatin1String("originstop")) ) {
        ret += "|originstop=" + parameters["originstop"].toLower();
    }
    if ( parameters.contains(QLatin1String("originstopid")) ) {
        ret += "|originstopid=" + parameters["originstopid"];
    }
    if ( parameters.contains(QLatin1String("targetstop")) ) {
        ret += "|targetstop=" + parameters["targetstop"].toLower();
    }
    if ( parameters.contains(QLatin1String("targetstopid")) ) {
        ret += "|targetstopid=" + parameters["targetstopid"];
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

void PublicTransportEngine::deleteProvider( const QString &providerId,
                                            bool keepProviderDataSources )
{
    // Clear all cached data
    const QStringList cachedSources = m_dataSources.keys();
    foreach( const QString &cachedSource, cachedSources ) {
        const QString currentProviderId = providerIdFromSourceName( cachedSource );
        if ( currentProviderId == providerId ) {
            // Disconnect provider and abort all running requests,
            // take provider from the provider list without caching it
            ProviderPointer provider = m_providers.take( providerId );
            if ( provider ) {
                disconnect( provider.data(), 0, this, 0 );
                provider->abortAllRequests();
            }
            m_runningSources.removeOne( cachedSource );

            if ( keepProviderDataSources ) {
                // Update data source for the provider
                TimetableDataSource *timetableSource =
                        dynamic_cast< TimetableDataSource* >( m_dataSources[cachedSource] );
                if ( timetableSource ) {
                    // Stop automatic updates
                    timetableSource->stopUpdateTimer();

                    // Update manually
                    foreach ( const QString &sourceName, timetableSource->usingDataSources() ) {
                        updateTimetableDataSource( SourceRequestData(sourceName) );
                    }
                }
            } else {
                // Delete data source for the provider
                delete m_dataSources.take( cachedSource );
            }
        }
    }
}

void PublicTransportEngine::reloadChangedProviders()
{
    qDebug() << "Reload service providers (the service provider dir changed)";

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
            m_providers.remove( providerId );
            m_cachedProviders.remove( providerId );
            m_erroneousProviders.remove( providerId );

            updateProviderData( providerId, cache );

            TimetableDataSource *timetableSource =
                    dynamic_cast< TimetableDataSource* >( m_dataSources[cachedSource] );
            if ( timetableSource ) {
                // Stop automatic updates
                timetableSource->stopUpdateTimer();

                // Update manually
                foreach ( const QString &sourceName, timetableSource->usingDataSources() ) {
                    updateTimetableDataSource( SourceRequestData(sourceName) );
                }
            }
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
            qWarning() << "Cannot create a request for parse mode" << parseMode;
            return;
        }

        // Read parameters
        for ( int i = 0; i < parameters.length(); ++i ) {
            const QString parameter = parameters.at( i ).trimmed();
            const int pos = parameter.indexOf( '=' );
            if ( pos == -1 ) {
                if ( !defaultParameter.isEmpty() ) {
                    qWarning() << "More than one parameters without name given:"
                               << defaultParameter << parameter;
                }

                // No parameter name given, assume the service provider ID
                defaultParameter = parameter;
            } else {
                const QString parameterName = parameter.left( pos );
                const QString parameterValue = parameter.mid( pos + 1 ).trimmed();
                if ( parameterValue.isEmpty() ) {
                    qWarning() << "Empty parameter value for parameter" << parameterName;
                } else if ( parameterName == QLatin1String("city") ) {
                    request->setCity( parameterValue );
                } else if ( parameterName == QLatin1String("stop") ) {
                    request->setStop( parameterValue );
                } else if ( parameterName == QLatin1String("stopid") ) {
                    request->setStopId( parameterValue );
                } else if ( parameterName == QLatin1String("targetstop") ) {
                    JourneyRequest *journeyRequest = dynamic_cast< JourneyRequest* >( request );
                    if ( !journeyRequest ) {
                        qWarning() << "The 'targetstop' parameter is only used for journey requests";
                    } else {
                        journeyRequest->setTargetStop( parameterValue );
                    }
                } else if ( parameterName == QLatin1String("targetstopid") ) {
                    JourneyRequest *journeyRequest = dynamic_cast< JourneyRequest* >( request );
                    if ( !journeyRequest ) {
                        qWarning() << "The 'targetstopId' parameter is only used for journey requests";
                    } else {
                        journeyRequest->setTargetStopId( parameterValue );
                    }
                } else if ( parameterName == QLatin1String("originstop") ) {
                    JourneyRequest *journeyRequest = dynamic_cast< JourneyRequest* >( request );
                    if ( !journeyRequest ) {
                        qWarning() << "The 'originstop' parameter is only used for journey requests";
                    } else {
                        journeyRequest->setStop( parameterValue );
                    }
                } else if ( parameterName == QLatin1String("originstopid") ) {
                    JourneyRequest *journeyRequest = dynamic_cast< JourneyRequest* >( request );
                    if ( !journeyRequest ) {
                        qWarning() << "The 'originstopId' parameter is only used for journey requests";
                    } else {
                        journeyRequest->setStopId( parameterValue );
                    }
                } else if ( parameterName == QLatin1String("timeoffset") ) {
                    request->setDateTime(
                            QDateTime::currentDateTime().addSecs(parameterValue.toInt() * 60) );
                } else if ( parameterName == QLatin1String("time") ) {
                    request->setDateTime( QDateTime(QDate::currentDate(),
                                                    QTime::fromString(parameterValue, "hh:mm")) );
                } else if ( parameterName == QLatin1String("datetime") ) {
                    request->setDateTime( QDateTime::fromString(parameterValue, Qt::ISODate) );
                    if ( !request->dateTime().isValid() ) {
                        request->setDateTime( QDateTime::fromString(parameterValue) );
                    }
                } else if ( parameterName == QLatin1String("count") ) {
                    bool ok;
                    request->setCount( parameterValue.toInt(&ok) );
                    if ( !ok ) {
                        qWarning() << "Bad value for 'count' in source name:" << parameterValue;
                        request->setCount( 20 );
                    }
                } else if ( dynamic_cast<StopsByGeoPositionRequest*>(request) ) {
                    StopsByGeoPositionRequest *stopRequest =
                            dynamic_cast< StopsByGeoPositionRequest* >( request );
                    bool ok;
                    if ( parameterName == QLatin1String("longitude") ) {
                        stopRequest->setLongitude( parameterValue.toFloat(&ok) );
                        if ( !ok ) {
                            qWarning() << "Bad value for 'longitude' in source name:" << parameterValue;
                        }
                    } else if ( parameterName == QLatin1String("latitude") ) {
                        stopRequest->setLatitude( parameterValue.toFloat(&ok) );
                        if ( !ok ) {
                            qWarning() << "Bad value for 'latitude' in source name:" << parameterValue;
                        }
                    } else if ( parameterName == QLatin1String("distance") ) {
                        stopRequest->setDistance( parameterValue.toInt(&ok) );
                        if ( !ok ) {
                            qWarning() << "Bad value for 'distance' in source name:" << parameterValue;
                        }
                    } else {
                        qWarning() << "Unknown argument" << parameterName;
                    }
                } else {
                    qWarning() << "Unknown argument" << parameterName;
                }
            }
        }

        if ( !request->dateTime().isValid() ) {
            // No date/time value given, use default offset from now
            request->setDateTime( QDateTime::currentDateTime().addSecs(DEFAULT_TIME_OFFSET * 60) );
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
        qWarning() << "Invalid source name" << name;
        return false;
    }

    if ( isDataRequestingSourceType(type) ) {
        if ( parseMode == ParseForDepartures || parseMode == ParseForArrivals ) {
            // Check if the stop name/ID is missing
            if ( !request || (request->stop().isEmpty() && request->stopId().isEmpty()) ) {
                qWarning() << "No stop ID or name in data source name" << name;
                return false;
            }
        } else if ( parseMode == ParseForStopSuggestions ) {
            // Check if the stop name or geo coordinates are missing
            if ( !request || (!dynamic_cast<StopsByGeoPositionRequest*>(request) &&
                              request->stop().isEmpty()) )
            {
                qWarning() << "Stop name (part) is missing in data source name" << name;
                return false;
            }
        } else if ( parseMode == ParseForJourneysByArrivalTime ||
                    parseMode == ParseForJourneysByDepartureTime )
        {
            // Make sure non empty originstop(id) and targetstop(id) parameters are available
            JourneyRequest *journeyRequest = dynamic_cast< JourneyRequest* >( request );
            if ( !journeyRequest ) {
                qWarning() << "Internal error: Not a JourneyRequest object but parseMode is"
                           << parseMode;
                return false;
            }
            if ( journeyRequest->stop().isEmpty() && journeyRequest->stopId().isEmpty() ) {
                qWarning() << "No stop ID or name for the origin stop in data source name" << name;
                return false;
            }
            if ( journeyRequest->targetStop().isEmpty() &&
                 journeyRequest->targetStopId().isEmpty() )
            {
                qWarning() << "No stop ID or name for the target stop in data source name" << name;
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
        qDebug() << "Source name incorrect" << sourceData.name;
        return false;
    }
}

void PublicTransportEngine::initVehicleTypesSource()
{
    QVariantMap vehicleTypes;
    const int index = Enums::staticMetaObject.indexOfEnumerator("VehicleType");
    const QMetaEnum enumerator = Enums::staticMetaObject.enumerator( index );
    // Start at i = 1 to skip Enums::InvalidVehicleType
    for ( int i = 1; i < enumerator.keyCount(); ++i ) {
        Enums::VehicleType vehicleType = static_cast< Enums::VehicleType >( enumerator.value(i) );
        QVariantMap vehicleTypeData;
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
        qWarning() << "Data source already removed";
        return;
    }
    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    Q_ASSERT( dataSource );
    m_runningSources.removeOne( nonAmbiguousName );
    QVariantList departuresData;
    const QString itemKey = isDepartureData ? "departures" : "arrivals";

    foreach( const DepartureInfoPtr &departureInfo, items ) {
        QVariantMap departureData;
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
        const uint hash = TimetableDataSource::hashForDeparture( departure, isDepartureData );
        if ( dataSource->additionalData().contains(hash) ) {
            // Found already downloaded additional data, add it to the updated departure data
            const TimetableData additionalData = dataSource->additionalData( hash );
            if ( !additionalData.isEmpty() ) {
                bool hasAdditionalData = false;
                for ( TimetableData::ConstIterator it = additionalData.constBegin();
                      it != additionalData.constEnd(); ++it )
                {
                    if ( it.key() != Enums::RouteDataUrl ) {
                        hasAdditionalData = true;
                    }
                    departureData.insert( Global::timetableInformationToString(it.key()), it.value() );
                }
                departureData[ "additionalDataState" ] = hasAdditionalData ? "included" : "error";
            }
        }

        if ( !departureData.contains("additionalDataState") ) {
            departureData[ "additionalDataState" ] =
                    provider->features().contains(Enums::ProvidesAdditionalData)
                    ? "notrequested" : "notsupported";
        }

        departuresData << departureData;
    }

    // Cleanup the data source later
    startDataSourceCleanupLater( dataSource );

    dataSource->setValue( itemKey, departuresData );

//     if ( deleteDepartureInfos ) {
//         qDebug() << "Delete" << items.count() << "departures/arrivals";
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
    publishData( dataSource );
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

void PublicTransportEngine::startDataSourceCleanupLater( TimetableDataSource *dataSource )
{
    if ( !dataSource->cleanupTimer() ) {
        QTimer *cleanupTimer = new QTimer( this );
        cleanupTimer->setInterval( 2500 );
        connect( cleanupTimer, SIGNAL(timeout()), this, SLOT(cleanupTimeout()) );
        dataSource->setCleanupTimer( cleanupTimer );
    }
    dataSource->cleanupTimer()->start();
}

void PublicTransportEngine::updateTimeout()
{
    // Find the timetable data source to which the timer belongs which timeout() signal was emitted
    QTimer *timer = qobject_cast< QTimer* >( sender() );
    TimetableDataSource *dataSource = dataSourceFromTimer( timer );
    if ( !dataSource ) {
        // No data source found that started the timer,
        // should not happen the data source should have deleted the timer on destruction
        qWarning() << "Timeout received from an unknown update timer";
        return;
    }

    // Found the timetable data source of the timer,
    // requests updates for all connected sources (possibly multiple combined stops)
    foreach ( const QString &sourceName, dataSource->usingDataSources() ) {
        updateTimetableDataSource( SourceRequestData(sourceName) );
    }  // TODO FIXME Do not update while running additional data requests?
}

void PublicTransportEngine::cleanupTimeout()
{
    // Find the timetable data source to which the timer belongs which timeout() signal was emitted
    QTimer *timer = qobject_cast< QTimer* >( sender() );
    TimetableDataSource *dataSource = dataSourceFromTimer( timer );
    if ( !dataSource ) {
        // No data source found that started the timer,
        // should not happen the data source should have deleted the timer on destruction
        qWarning() << "Timeout received from an unknown cleanup timer";
        return;
    }

    // Found the timetable data source of the timer
    dataSource->cleanup();
    dataSource->setCleanupTimer( 0 ); // Deletes the timer
}

void PublicTransportEngine::additionalDataReceived( ServiceProvider *provider,
        const QUrl &requestUrl, const TimetableData &data, const AdditionalDataRequest &request )
{
    Q_UNUSED( provider );
    Q_UNUSED( requestUrl );

    // Check if the destination data source exists
    const QString nonAmbiguousName = disambiguateSourceName( request.sourceName() );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        qWarning() << "Additional data received for a source that was already removed:"
                   << nonAmbiguousName;
        emit additionalDataRequestFinished( request.sourceName(), request.itemNumber(), false,
                                            "Data source to update was already removed" );
        return;
    }

    // Get the list of timetable items from the existing data source
    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    Q_ASSERT( dataSource );
    QVariantList items = dataSource->timetableItems();
    if ( request.itemNumber() >= items.count() ) {
        emit additionalDataRequestFinished( request.sourceName(), request.itemNumber(), false,
                QString("Item %1 not found in the data source (with %2 items)")
                .arg(request.itemNumber()).arg(items.count()) );
        return;
    }

    // Get the timetable item for which additional data was received
    // and insert the new data into it
    bool newDataInserted = false;
    TimetableData _data = data;
    QVariantMap item = items[ request.itemNumber() ].toMap();
    for ( TimetableData::ConstIterator it = data.constBegin(); it != data.constEnd(); ++it ) {
        // Check if there already is data in the current additional data field
        const QString key = Global::timetableInformationToString( it.key() );
        const bool isRouteDataUrl = key == Enums::toString(Enums::RouteDataUrl);
        if ( item.contains(key) && item[key].isValid() && !isRouteDataUrl ) {
            // The timetable item already contains data for the current field,
            // do not allow updates here, only additional data
            qWarning() << "Cannot update timetable data in additional data requests";
            qWarning() << "Tried to update field" << key << "to value" << it.value()
                       << "from value" << item[key] << "in data source" << request.sourceName();
        } else {
            // Allow to add the additional data field
            item.insert( key, it.value() );
            if ( !isRouteDataUrl ) {
                newDataInserted = true;

                if ( key == Enums::toString(Enums::RouteStops) ) {
                    // Added a RouteStops field, automatically generate RouteStopsShortened
                    const QString routeStopsShortenedKey =
                            Global::timetableInformationToString( Enums::RouteStopsShortened );
                    if ( !item.contains(routeStopsShortenedKey) ) {
                        const QStringList routeStopsShortened =
                                removeCityNameFromStops( it.value().toStringList() );
                        item.insert( routeStopsShortenedKey, routeStopsShortened );
                        _data.insert( Enums::RouteStopsShortened, routeStopsShortened );
                    }
                }
            }
        }
    }

    if ( !newDataInserted ) {
        const QString errorMessage = i18nc("@info/plain", "No additional data found");
        emit additionalDataRequestFinished( request.sourceName(), request.itemNumber(),
                                            false, errorMessage );
        item[ "additionalDataState" ] = "error";
        item[ "additionalDataError" ] = errorMessage;
        items[ request.itemNumber() ] = item;
        dataSource->setTimetableItems( items );
        return;
    }

    item[ "additionalDataState" ] = "included";

    // Store the changed item back into the item list of the data source
    items[ request.itemNumber() ] = item;
    dataSource->setTimetableItems( items );

    // Also store received additional data separately
    // to not loose additional data after updating the data source
    const uint hash = TimetableDataSource::hashForDeparture( item,
            dataSource->timetableItemKey() != QLatin1String("arrivals") );
    dataSource->setAdditionalData( hash, _data );
    startDataSourceCleanupLater( dataSource );

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
        publishData( dataSource );
        updateDelayTimer = new QTimer( this );
        updateDelayTimer->setInterval( 150 );
        connect( updateDelayTimer, SIGNAL(timeout()),
                 this, SLOT(updateDataSourcesWithNewAdditionData()) );
        dataSource->setUpdateAdditionalDataDelayTimer( updateDelayTimer );
    }

    // (Re-)start the additional data update timer
    updateDelayTimer->start();

    // Emit result
    emit additionalDataRequestFinished( request.sourceName(), request.itemNumber(), true );
}

TimetableDataSource *PublicTransportEngine::dataSourceFromTimer( QTimer *timer ) const
{
    for ( QHash< QString, DataSource* >::ConstIterator it = m_dataSources.constBegin();
          it != m_dataSources.constEnd(); ++it )
    {
        TimetableDataSource *dataSource = dynamic_cast< TimetableDataSource* >( *it );
        if ( dataSource && (dataSource->updateAdditionalDataDelayTimer() == timer ||
                            dataSource->updateTimer() == timer ||
                            dataSource->cleanupTimer() == timer) )
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
        qWarning() << "Data source was already removed";
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
        qWarning() << "Data source already removed" << nonAmbiguousName;
        return;
    }
    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    if ( !dataSource ) {
        qWarning() << "Data source already deleted" << nonAmbiguousName;
        return;
    }
    dataSource->clear();
    QVariantList journeysData;
    foreach( const JourneyInfoPtr &journeyInfo, journeys ) {
        if ( !journeyInfo->isValid() ) {
            continue;
        }

        QVariantMap journeyData;
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
        QVariantMap stopData;
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
    qDebug() << errorCode << errorMessage << "- Requested URL:" << requestUrl;

    const AdditionalDataRequest *additionalDataRequest =
            dynamic_cast< const AdditionalDataRequest* >( request );
    if ( additionalDataRequest ) {
        // A request for additional data failed
        emit additionalDataRequestFinished( request->sourceName(),
                additionalDataRequest->itemNumber(), false, errorMessage );

        // Check if the destination data source exists
        const QString nonAmbiguousName = disambiguateSourceName( request->sourceName() );
        if ( m_dataSources.contains(nonAmbiguousName) ) {
            // Get the list of timetable items from the existing data source
            TimetableDataSource *dataSource =
                    dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
            Q_ASSERT( dataSource );
            QVariantList items = dataSource->timetableItems();
            if ( additionalDataRequest->itemNumber() < items.count() ) {
                // Found the item for which the request failed,
                // reset additional data included/waiting fields
                QVariantMap item = items[ additionalDataRequest->itemNumber() ].toMap();
                item[ "additionalDataState" ] = "error";
                item[ "additionalDataError" ] = errorMessage;

                items[ additionalDataRequest->itemNumber() ] = item;
                dataSource->setTimetableItems( items );

                // Publish updated fields
                publishData( dataSource );
            } else {
                qWarning() << "Timetable item" << additionalDataRequest->itemNumber()
                           << "not found in data source" << request->sourceName()
                           << "additional data error discarded";
            }
        } else {
            qWarning() << "Data source" << request->sourceName()
                       << "not found, additional data error discarded";
        }

        // Do not mark the whole timetable data source as erroneous
        // when a request for additional data has failed
        return;
    }

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

bool PublicTransportEngine::requestUpdate( const QString &sourceName )
{
    // Find the TimetableDataSource object for sourceName
    const QString nonAmbiguousName = disambiguateSourceName( sourceName );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        qWarning() << "Not an existing timetable data source:" << sourceName;
        emit updateRequestFinished( sourceName, false,
                i18nc("@info", "Data source is not an existing timetable data source: %1",
                      sourceName) );
        return false;
    }
    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    if ( !dataSource ) {
        qWarning() << "Internal error: Invalid pointer to the data source stored";
        emit updateRequestFinished( sourceName, false,
                                    "Internal error: Invalid pointer to the data source stored" );
        return false;
    }

    // Check if updates are blocked to prevent to many useless updates
    const QDateTime nextUpdateTime = sourceUpdateTime( dataSource, UpdateWasRequestedManually );
    if ( QDateTime::currentDateTime() < nextUpdateTime ) {
        qDebug() << "Too early to update again, update request rejected" << nextUpdateTime;
        emit updateRequestFinished( sourceName, false,
                i18nc("@info", "Update request rejected, earliest update time: %1",
                      nextUpdateTime.toString()) );
        return false;
    }

    // Stop automatic updates
    dataSource->stopUpdateTimer();

    // Start the request
    const bool result = request( sourceName );
    if ( result ) {
        emit updateRequestFinished( sourceName );
    } else {
        emit updateRequestFinished( sourceName, false, i18nc("@info", "Request failed") );
    }
    return result;
}

bool PublicTransportEngine::requestMoreItems( const QString &sourceName,
                                              Enums::MoreItemsDirection direction )
{
    // Find the TimetableDataSource object for sourceName
    const QString nonAmbiguousName = disambiguateSourceName( sourceName );
    if ( !m_dataSources.contains(nonAmbiguousName) ) {
        qWarning() << "Not an existing timetable data source:" << sourceName;
        emit moreItemsRequestFinished( sourceName, direction, false,
                i18nc("@info", "Data source is not an existing timetable data source: %1",
                      sourceName) );
        return false;
    } else if ( m_runningSources.contains(nonAmbiguousName) ) {
        // The data source currently gets processed or updated, including requests for more items
        qDebug() << "Source currently gets processed, please wait" << sourceName;
        emit moreItemsRequestFinished( sourceName, direction, false,
                i18nc("@info", "Source currently gets processed, please try again later") );
        return false;
    }

    TimetableDataSource *dataSource =
            dynamic_cast< TimetableDataSource* >( m_dataSources[nonAmbiguousName] );
    if ( !dataSource ) {
        qWarning() << "Internal error: Invalid pointer to the data source stored";
        emit moreItemsRequestFinished( sourceName, direction, false, "Internal error" );
        return false;
    }

    const SourceRequestData data( sourceName );
    qDebug() << data.defaultParameter;
    const ProviderPointer provider = providerFromId( data.defaultParameter );
    if ( provider.isNull() ) {
        // Service provider couldn't be created, should only happen after provider updates,
        // where the provider worked to get timetable items, but the new provider version
        // does not work any longer and no more items can be requested
        emit moreItemsRequestFinished( sourceName, direction, false,
                                       "Provider could not be created" );
        return false;
    }

    // Start the request
    QSharedPointer< AbstractRequest > request = dataSource->request( sourceName );
    const QVariantList items = dataSource->timetableItems();
    if ( items.isEmpty() ) {
        // TODO
        qWarning() << "No timetable items in data source" << sourceName;
        emit moreItemsRequestFinished( sourceName, direction, false,
                i18nc("@info", "No timetable items in data source") );
        return false;
    }
    const QVariantMap item =
            (direction == Enums::EarlierItems ? items.first() : items.last()).toMap();
    const QVariantMap _requestData = item[Enums::toString(Enums::RequestData)].toMap();

    // TODO Convert to hash from map..
    QVariantMap requestData;
    for ( QVariantMap::ConstIterator it = _requestData.constBegin(); it != _requestData.constEnd(); ++it ) {
        requestData.insert( it.key(), it.value() );
    }

    // Start the request
    m_runningSources << nonAmbiguousName;
    provider->requestMoreItems( MoreItemsRequest(sourceName, request, requestData, direction) );
    emit moreItemsRequestFinished( sourceName, direction );
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
            const QVariantMap stateData = dataSource->providerStateData( data.defaultParameter );
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
        qDebug() << QString("Service provider %1 needs a separate city value. Add to source name "
                            "'|city=X', where X stands for the city name.")
                    .arg( data.defaultParameter );
        return false; // Service provider needs a separate city value
    } else if ( !provider->features().contains(Enums::ProvidesJourneys) &&
                (data.parseMode == ParseForJourneysByDepartureTime ||
                 data.parseMode == ParseForJourneysByArrivalTime) )
    {
        qDebug() << QString("Service provider %1 doesn't support journey searches.")
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
        qWarning() << "Internal error: Invalid pointer to the data source stored";
        return -1;
    }

    const QDateTime time = sourceUpdateTime( dataSource, updateFlags );
    return qMax( -1, int( QDateTime::currentDateTime().secsTo(time) ) );
}

QDateTime PublicTransportEngine::sourceUpdateTime( TimetableDataSource *dataSource,
                                                   UpdateFlags updateFlags )
{
    const QString providerId = fixProviderId( dataSource->providerId() );
    const ProviderPointer provider = providerFromId( providerId );
    if ( provider.isNull() || provider->type() == Enums::InvalidProvider ) {
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
        qWarning() << "Cannot create provider of type" << data->typeName() << "because the engine "
                      "has been built without support for that provider type";
        return 0;
    }

    // Warn if the format of the provider plugin is unsupported
    if ( data->fileFormatVersion() != QLatin1String("1.1") ) {
        qWarning() << "The Provider" << data->id() << "was designed for an unsupported "
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
        qWarning() << "Invalid/unknown provider type" << data->type();
        return 0;
    }
}

QStringList PublicTransportEngine::removeCityNameFromStops( const QStringList &stopNames )
{
    // Find words at the beginning/end of target and route stop names that have many
    // occurrences. These words are most likely the city names where the stops are in.
    // But the timetable becomes easier to read and looks nicer, if not each stop name
    // includes the same city name.
    QHash< QString, int > firstWordCounts; // Counts occurrences of words at the beginning
    QHash< QString, int > lastWordCounts; // Counts occurrences of words at the end

    // minWordOccurrence is the minimum occurence count of a word in stop names to have the word
    // removed. maxWordOccurrence is the maximum number of occurences to count, if this number
    // of occurences has been found, just remove that word.
    const int minWordOccurrence = qMax( 2, stopNames.count() );
    const int maxWordOccurrence = qMax( 3, stopNames.count() / 2 );

    // This regular expression gets used to search for word at the end, possibly including
    // a colon before the last word
    QRegExp rxLastWord( ",?\\s+\\S+$" );

    // These strings store the words with the most occurrences in stop names at the beginning/end
    QString removeFirstWord;
    QString removeLastWord;

    // Analyze stop names
    foreach ( const QString &stopName, stopNames ) {
        // Find word to remove from beginning/end of stop names, if not already found
        if ( !removeFirstWord.isEmpty() || !removeLastWord.isEmpty() ) {
            break;
        }

        int pos = stopName.indexOf( ' ' );
        const QString newFirstWord = stopName.left( pos );
        if ( pos > 0 && ++firstWordCounts[newFirstWord] >= maxWordOccurrence ) {
            removeFirstWord = newFirstWord;
        }
        if ( rxLastWord.indexIn(stopName) != -1 &&
             ++lastWordCounts[rxLastWord.cap()] >= maxWordOccurrence )
        {
            removeLastWord = rxLastWord.cap();
        }
    }

    // Remove word with most occurrences from beginning/end of stop names
    if ( removeFirstWord.isEmpty() && removeLastWord.isEmpty() ) {
        // If no first/last word with at least maxWordOccurrence occurrences was found,
        // find the word with the most occurrences
        int max = 0;

        // Find word at the beginning with most occurrences
        for ( QHash< QString, int >::ConstIterator it = firstWordCounts.constBegin();
              it != firstWordCounts.constEnd(); ++it )
        {
            if ( it.value() > max ) {
                max = it.value();
                removeFirstWord = it.key();
            }
        }

        // Find word at the end with more occurrences
        for ( QHash< QString, int >::ConstIterator it = lastWordCounts.constBegin();
              it != lastWordCounts.constEnd(); ++it )
        {
            if ( it.value() > max ) {
                max = it.value();
                removeLastWord = it.key();
            }
        }

        if ( max < minWordOccurrence ) {
            // The first/last word with the most occurrences has too few occurrences
            // Do not remove any word
            removeFirstWord.clear();
            removeLastWord.clear();
        } else if ( !removeLastWord.isEmpty() ) {
            // removeLastWord has more occurrences than removeFirstWord
            removeFirstWord.clear();
        }
    }

    if ( !removeFirstWord.isEmpty() ) {
        // Remove removeFirstWord from all stop names
        QStringList returnStopNames;
        foreach ( const QString &stopName, stopNames ) {
            if ( stopName.startsWith(removeFirstWord) ) {
                returnStopNames << stopName.mid( removeFirstWord.length() + 1 );
            } else {
                returnStopNames << stopName;
            }
        }
        return returnStopNames;
    } else if ( !removeLastWord.isEmpty() ) {
        // Remove removeLastWord from all stop names
        QStringList returnStopNames;
        foreach ( const QString &stopName, stopNames ) {
            if ( stopName.endsWith(removeLastWord) ) {
                returnStopNames << stopName.left( stopName.length() - removeLastWord.length() );
            } else {
                returnStopNames << stopName;
            }
        }
        return returnStopNames;
    } else {
        // Nothing to remove found
        return stopNames;
    }
}

QVariantMap PublicTransportEngine::hashToMap(QVariantHash data)
{
    QVariantMap mapData;
    QHashIterator<QString, QVariant> it(data);
    while (it.hasNext()) {
        it.next();
        mapData.insert(it.key(), it.value());
    }
    return mapData;
}

// This does the magic that allows Plasma to load
// this plugin.  The first argument must match
// the X-Plasma-EngineName in the .desktop file.
K_EXPORT_PLASMA_DATAENGINE_WITH_JSON( publictransport, PublicTransportEngine, "plasma-engine-publictransport.json")

// this is needed since PublicTransportEngine is a QObject
#include "build/publictransportdataengine.moc"
