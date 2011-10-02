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

// Own includes
#include "publictransportdataengine.h"
#include "publictransportservice.h"
#include "timetableaccessor.h"
#include "generaltransitfeed_importer.h"

// KDE includes
#include <KStandardDirs>

// Qt includes
#include <QFileSystemWatcher>
#include <QFileInfo>
#include "timetableaccessor_generaltransitfeed.h"

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
	// 60 seconds should be enough, departure / arrival times have minute precision.
	setMinimumPollingInterval( 60000 );

	// Add service provider source, so when using
	// dataEngine("publictransport").sources() in an applet it at least returns this
// 	setData( sourceTypeKeyword(ServiceProviders), DataEngine::Data() );
// 	setData( sourceTypeKeyword(ErrornousServiceProviders), DataEngine::Data() );
// 	setData( sourceTypeKeyword(Locations), DataEngine::Data() );
}

PublicTransportEngine::~PublicTransportEngine()
{
	qDeleteAll( m_accessors.values() );
	delete m_fileSystemWatcher;
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
        TimetableAccessor *_accessor = getSpecificAccessor( accessorInfo.serviceProvider() );
//         TimetableAccessor *_accessor;
        if ( m_accessors.contains(accessorInfo.serviceProvider()) ) {
            // Use cached accessor
            kDebug() << "Use cached accessor to get feature info" << accessorInfo.serviceProvider();
//             _accessor = m_accessors[ accessorInfo.serviceProvider() ];
        } else {
            // Create accessor
            kDebug() << "Create accessor to get feature info" << accessorInfo.serviceProvider();
//             _accessor = TimetableAccessor::createAccessor( accessorInfo.serviceProvider() );
//             m_accessors.insert( accessorInfo.serviceProvider(), _accessor );
            // TODO Delete accessor after X seconds?
        }
        dataServiceProvider.insert( "features", _accessor->features() );
        dataServiceProvider.insert( "featuresLocalized", _accessor->featuresLocalized() );
    }
	
	QStringList changelog;
	foreach ( const ChangelogEntry &entry, accessorInfo.changelog() ) {
		changelog << QString( "%2 (%1): %3" ).arg( entry.since_version ).arg( entry.author ).arg( entry.description );
	}
	dataServiceProvider.insert( "changelog", changelog );

	return dataServiceProvider;
}

QHash< QString, QVariant > PublicTransportEngine::locations()
{
	QVariantHash ret, locationHash;
	QString name;

	locationHash.insert( "name", name = "international" );
	locationHash.insert( "description", i18n( "Contains international providers. There is one for getting flight departures / arrivals." ) );
	locationHash.insert( "defaultAccessor", "international_flightstats" );
	ret.insert( name, locationHash );

	locationHash.clear();
	locationHash.insert( "name", name = "de" );
	locationHash.insert( "description", i18n( "Support for all cities in Germany (and limited support for cities in europe). There is also support for providers specific to regions / cities." ) );
	locationHash.insert( "defaultAccessor", "de_db" );
	ret.insert( name, locationHash );

	locationHash.clear();
	locationHash.insert( "name", name = "fr" );
	locationHash.insert( "description", i18n( "Support for some cities in France. No local public transportation information." ) );
	locationHash.insert( "defaultAccessor", "fr_gares" );
	ret.insert( name, locationHash );

	locationHash.clear();
	locationHash.insert( "name", name = "it" );
	locationHash.insert( "description", i18n( "Support for some cities in Italia." ) );
	locationHash.insert( "defaultAccessor", "it_cup2000" );
	ret.insert( name, locationHash );

	locationHash.clear();
	locationHash.insert( "name", name = "be" );
	locationHash.insert( "description", i18n( "Support for some cities in Belgium." ) );
	locationHash.insert( "defaultAccessor", "be_brail" );
	ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "dk" );
    locationHash.insert( "description", i18n( "Support for some cities in Denmark." ) );
    locationHash.insert( "defaultAccessor", "dk_rejseplanen" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "es" );
    locationHash.insert( "description", i18n( "Support for some cities in Spain (GTFS)." ) );
    locationHash.insert( "defaultAccessor", "es_vitoria" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "hu" );
    locationHash.insert( "description", i18n( "Support for some cities in Hungary (GTFS)." ) );
    locationHash.insert( "defaultAccessor", "hu_szeget" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "nz" );
    locationHash.insert( "description", i18n( "Support for some cities in New Zealand (GTFS)." ) );
    locationHash.insert( "defaultAccessor", "nz_metlink" );
    ret.insert( name, locationHash );

	locationHash.clear();
	locationHash.insert( "name", name = "se" );
	locationHash.insert( "description", i18n( "Support for all cities in Sweden." ) );
	locationHash.insert( "defaultAccessor", "se_resrobot" );
	ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "us" );
    locationHash.insert( "description", i18n( "Support for many cities in the USA." ) );
    locationHash.insert( "defaultAccessor", "us_amtrak" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "ua" );
    locationHash.insert( "description", i18n( "Support for Ukraine." ) );
    locationHash.insert( "defaultAccessor", "ua_lviv" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "gb" );
    locationHash.insert( "description", i18n( "Support for Great Britain." ) );
    locationHash.insert( "defaultAccessor", "gb_datagm" );
    ret.insert( name, locationHash );

	locationHash.clear();
	locationHash.insert( "name", name = "ch" );
	locationHash.insert( "description", i18n( "Support for all cities in Switzerland." ) );
	locationHash.insert( "defaultAccessor", "ch_sbb" );
	ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "au" );
    locationHash.insert( "description", i18n( "Support for Australia." ) );
    locationHash.insert( "defaultAccessor", "au_winnipeg" );
    ret.insert( name, locationHash );

    locationHash.clear();
    locationHash.insert( "name", name = "at" );
    locationHash.insert( "description", i18n( "Support for all cities in Austria." ) );
    locationHash.insert( "defaultAccessor", "at_oebb" );
    ret.insert( name, locationHash );

	locationHash.clear();
	locationHash.insert( "name", name = "pl" );
	locationHash.insert( "description", i18n( "Support for all cities in Poland." ) );
	locationHash.insert( "defaultAccessor", "pl_pkp" );
	ret.insert( name, locationHash );

	locationHash.clear();
	locationHash.insert( "name", name = "cz" ); //= i18n("Czechia") );
	locationHash.insert( "description", i18n( "Support for many cities in Czechia, but with static data." ) );
	locationHash.insert( "defaultAccessor", "cz_idnes" );
	ret.insert( name, locationHash );

	locationHash.clear();
	locationHash.insert( "name", name = "sk" );
	locationHash.insert( "description", i18n( "Support for many cities in Slovakia, but with static data. There is also support for bratislava with dynamic data." ) );
	locationHash.insert( "defaultAccessor", "sk_atlas" );
	ret.insert( name, locationHash );

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
	
    // Try to get the specific accessor
//     kDebug() << "Check accessor" << accessorId;
// TODO Cache result for this data source
    const TimetableAccessorInfo *accessorInfo = getSpecificAccessorInfo( accessorId );
	if ( accessorInfo ) {
		setData( name, serviceProviderInfo(*accessorInfo) );
		delete accessorInfo;
	} else {
		if ( !m_errornousAccessors.contains(accessorId) ) {
			m_errornousAccessors << accessorId;
		}
		return false;
	}

	return true;
}

bool PublicTransportEngine::updateServiceProviderSource()
{
	const QString name = sourceTypeKeyword( ServiceProviders );
	QVariantHash dataSource;
	if ( m_dataSources.contains( name ) ) {
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
		m_errornousAccessors.clear();
		foreach( const QString &fileName, fileNames ) {
			if ( QFileInfo(fileName).isSymLink() && fileName.endsWith(QLatin1String("_default.xml")) ) {
				// Don't use symlinks to default service providers
				continue;
			}

			// Remove file extension
			QString serviceProvider = KUrl( fileName ).fileName().remove( QRegExp( "\\..*$" ) );

            // Try to get the specific accessor
            const TimetableAccessor *accessor = getSpecificAccessor( serviceProvider );
			if ( accessor ) {
				dataSource.insert( accessor->timetableAccessorInfo().name(),
				                   serviceProviderInfo(accessor) );
				loadedAccessors << serviceProvider;
// 				delete accessor; TODO Delete after not used for X seconds
			} else {
				m_errornousAccessors << serviceProvider;
			}
		}

		kDebug() << "Loaded" << loadedAccessors.count() << "accessors";
		if ( !m_errornousAccessors.isEmpty() ) {
			kDebug() << "Errornous accessor info XMLs, that couldn't be loaded:" << m_errornousAccessors;
		}

		m_dataSources.insert( name, dataSource );
	}

	for ( QVariantHash::const_iterator it = dataSource.constBegin();
	        it != dataSource.constEnd(); ++it ) {
		setData( name, it.key(), it.value() );
	}
	return true;
}

void PublicTransportEngine::updateErrornousServiceProviderSource( const QString &name )
{
	setData( name, "names", m_errornousAccessors );
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

        // Try to get the specific accessor
        TimetableAccessor *accessor = getSpecificAccessor( serviceProvider );
        if ( !accessor ) {
            return false;
        }

        if ( accessor->useSeparateCityValue() && city.isEmpty() ) {
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

		if ( parseDocumentMode == ParseForDeparturesArrivals ) {
			accessor->requestDepartures( name, city, stop, maxCount, dateTime, dataType );
		} else if ( parseDocumentMode == ParseForStopSuggestions ) {
			accessor->requestStopSuggestions( name, city, stop );
		} else { // if ( parseDocumentMode == ParseForJourneys )
			accessor->requestJourneys( name, city, originStop, targetStop,
									   maxCount, dateTime, dataType );
		}
	}

	return true;
}

TimetableAccessorInfo* PublicTransportEngine::getSpecificAccessorInfo(
        const QString &serviceProvider )
{
    return TimetableAccessor::readAccessorInfo( serviceProvider );
}

TimetableAccessor* PublicTransportEngine::getSpecificAccessor( const QString &serviceProvider )
{
    // Try to get the specific accessor from m_accessors
    bool newlyCreated = false;
    TimetableAccessor *accessor;
    if ( !m_accessors.contains(serviceProvider) ) {
        // Accessor not already created, do it now
        accessor = TimetableAccessor::createAccessor( serviceProvider );
        m_accessors.insert( serviceProvider, accessor );
        newlyCreated = true;
    } else {
        // Use already created accessor
        accessor = m_accessors.value( serviceProvider );
    }

    if ( !accessor ) {
        kDebug() << QString( "Accessor %1 couldn't be created" ).arg( serviceProvider );
        return 0; // accessor could not be created
    }

    if ( newlyCreated ) {
        connect( accessor,
                 SIGNAL(departureListReceived(TimetableAccessor*,QUrl,QList<DepartureInfo*>,GlobalTimetableInfo,QString,QString,QString,QString,QString,ParseDocumentMode)),
                 this, SLOT(departureListReceived(TimetableAccessor*,QUrl,QList<DepartureInfo*>,GlobalTimetableInfo,QString,QString,QString,QString,QString,ParseDocumentMode)) );
        connect( accessor,
                 SIGNAL(journeyListReceived(TimetableAccessor*,QUrl,QList<JourneyInfo*>,GlobalTimetableInfo,QString,QString,QString,QString,QString,ParseDocumentMode)),
                 this, SLOT(journeyListReceived(TimetableAccessor*,QUrl,QList<JourneyInfo*>,GlobalTimetableInfo,QString,QString,QString,QString,QString,ParseDocumentMode)) );
        connect( accessor,
                 SIGNAL(stopListReceived(TimetableAccessor*,QUrl,QList<StopInfo*>,QString,QString,QString,QString,QString,ParseDocumentMode)),
                 this, SLOT(stopListReceived(TimetableAccessor*,QUrl,QList<StopInfo*>,QString,QString,QString,QString,QString,ParseDocumentMode)) );
        connect( accessor,
                 SIGNAL(errorParsing(TimetableAccessor*,ErrorType,QString,QUrl,QString,QString,QString,QString,QString,ParseDocumentMode)),
                 this, SLOT(errorParsing(TimetableAccessor*,ErrorType,QString,QUrl,QString,QString,QString,QString,QString,ParseDocumentMode)) );
        connect( accessor,
                 SIGNAL(progress(TimetableAccessor*,qreal,QString,QUrl,QString,QString,QString,QString,QString,ParseDocumentMode)),
                 this, SLOT(progress(TimetableAccessor*,qreal,QString,QUrl,QString,QString,QString,QString,QString,ParseDocumentMode)) );
    }

    return accessor;
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
	m_timer = NULL;

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
	case ErrornousServiceProviders:
		return "ErrornousServiceProviders";
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
	} else if ( sourceName.compare(sourceTypeKeyword(ErrornousServiceProviders), 
								   Qt::CaseInsensitive) == 0 ) {
		return ErrornousServiceProviders;
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
	case ErrornousServiceProviders:
		updateErrornousServiceProviderSource( name );
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
		const QUrl &requestUrl, const QList<DepartureInfo*> &departures,
		const GlobalTimetableInfo &globalInfo,
		const QString &serviceProvider, const QString &sourceName,
		const QString &city, const QString &stop,
		const QString &dataType, ParseDocumentMode parseDocumentMode )
{
	Q_UNUSED( accessor );
	Q_UNUSED( serviceProvider );
	Q_UNUSED( city );
	Q_UNUSED( stop );
	Q_UNUSED( dataType );
	Q_UNUSED( parseDocumentMode );
	kDebug() << departures.count() << "departures / arrivals received" << sourceName;

	int i = 0;
	m_dataSources.remove( sourceName );
	QVariantHash dataSource;
	foreach( DepartureInfo *departureInfo, departures ) {
// 	if ( !departureInfo->isValid() ) {
// 	    kDebug() << "Departure isn't valid" << departureInfo->line();
// 	    continue;
// 	}
		QVariantHash data;
		data.insert( "line", departureInfo->line() );
		data.insert( "target", departureInfo->target() );
		data.insert( "departure", departureInfo->departure() );
		data.insert( "vehicleType", static_cast<int>( departureInfo->vehicleType() ) );
		data.insert( "vehicleIconName", Global::vehicleTypeToIcon( departureInfo->vehicleType() ) );
		data.insert( "vehicleName", Global::vehicleTypeToString( departureInfo->vehicleType() ) );
		data.insert( "vehicleNamePlural", Global::vehicleTypeToString( departureInfo->vehicleType(), true ) );
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
		data.insert( "routeTimes", departureInfo->routeTimesVariant() );
		data.insert( "routeExactStops", departureInfo->routeExactStops() );
        data.insert( "pricing", departureInfo->pricing() );

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

	setData( sourceName, "serviceProvider", serviceProvider );
	setData( sourceName, "count", departureCount );
	setData( sourceName, "delayInfoAvailable", globalInfo.delayInfoAvailable );
	setData( sourceName, "requestUrl", requestUrl );
	setData( sourceName, "parseMode", "departures" );
	setData( sourceName, "receivedData", "departures" );
	setData( sourceName, "receivedPossibleStopList", false );
	setData( sourceName, "error", false );
	setData( sourceName, "updated", QDateTime::currentDateTime() );

	// Store received data in the data source map
	dataSource.insert( "serviceProvider", serviceProvider );
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
		const GlobalTimetableInfo &globalInfo,
		const QString &serviceProvider, const QString& sourceName,
		const QString& city, const QString& stop,
		const QString& dataType, ParseDocumentMode parseDocumentMode )
{
	Q_UNUSED( accessor );
	Q_UNUSED( serviceProvider );
	Q_UNUSED( city );
	Q_UNUSED( stop );
	Q_UNUSED( dataType );
	Q_UNUSED( parseDocumentMode );
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
		data.insert( "Operator", journeyInfo->operatorName() );
		data.insert( "routeStops", journeyInfo->routeStops() );
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
		// 	kDebug() << "setData" << sourceName << data;
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

	setData( sourceName, "serviceProvider", serviceProvider );
	setData( sourceName, "count", journeyCount );
	setData( sourceName, "delayInfoAvailable", globalInfo.delayInfoAvailable );
	setData( sourceName, "requestUrl", requestUrl );
	setData( sourceName, "parseMode", "journeys" );
	setData( sourceName, "receivedData", "journeys" );
	setData( sourceName, "receivedPossibleStopList", false );
	setData( sourceName, "error", false );
	setData( sourceName, "updated", QDateTime::currentDateTime() );

	// Store received data in the data source map
	dataSource.insert( "serviceProvider", serviceProvider );
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
		const QUrl &requestUrl, const QList<StopInfo*> &stops,
		const QString &serviceProvider, const QString &sourceName,
		const QString &city, const QString &stop,
		const QString &dataType, ParseDocumentMode parseDocumentMode )
{
	Q_UNUSED( accessor );
	Q_UNUSED( serviceProvider );
	Q_UNUSED( city );
	Q_UNUSED( stop );
	Q_UNUSED( dataType );
//     QString sStop = stopToStopId.value( stop, stop );
//     if ( sStop.isEmpty() )
// 	sStop = stop;

	int i = 0;
	foreach( const StopInfo *stopInfo, stops ) {
		QVariantHash data;
		data.insert( "stopName", stopInfo->name() );
		
// 		kDebug() << stopInfo->name() << stopInfo->id() << stopInfo->city() << stopInfo->countryCode();
		
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

// 	kDebug() << "setData" << i << data;
		setData( sourceName, QString( "stopName %1" ).arg( i++ ), data );
	}

	// Remove values from an old possible stop list
	for ( i = stops.count(); i < m_lastStopNameCount; ++i ) {
		removeData( sourceName, QString( "stopName %1" ).arg( i ) );
	}
	m_lastStopNameCount = stops.count();

	setData( sourceName, "serviceProvider", serviceProvider );
	setData( sourceName, "count", stops.count() );
	setData( sourceName, "requestUrl", requestUrl );
	if ( parseDocumentMode == ParseForDeparturesArrivals ) {
		setData( sourceName, "parseMode", "departures" );
	} else if ( parseDocumentMode == ParseForJourneys ) {
		setData( sourceName, "parseMode", "journeys" );
	} else if ( parseDocumentMode == ParseForStopSuggestions ) {
		setData( sourceName, "parseMode", "stopSuggestions" );
	}
	setData( sourceName, "receivedData", "stopList" );
	setData( sourceName, "receivedPossibleStopList", true );
	setData( sourceName, "error", false );
	setData( sourceName, "updated", QDateTime::currentDateTime() );

	// TODO: add to m_dataSources?
}

void PublicTransportEngine::errorParsing( TimetableAccessor *accessor,
		ErrorType errorType, const QString &errorString,
		const QUrl &requestUrl, const QString &serviceProvider,
		const QString &sourceName, const QString &city, const QString &stop,
		const QString &dataType, ParseDocumentMode parseDocumentMode )
{
	Q_UNUSED( accessor );
	Q_UNUSED( serviceProvider );
	Q_UNUSED( city );
	Q_UNUSED( stop );
	Q_UNUSED( dataType );
	kDebug() << "Error while parsing" << requestUrl << serviceProvider
			 << "\n  sourceName =" << sourceName << dataType << parseDocumentMode;
	kDebug() << errorType << errorString;

	setData( sourceName, "serviceProvider", serviceProvider );
	setData( sourceName, "count", 0 );
	setData( sourceName, "requestUrl", requestUrl );
	if ( parseDocumentMode == ParseForDeparturesArrivals ) {
		setData( sourceName, "parseMode", "departures" );
	} else if ( parseDocumentMode == ParseForJourneys ) {
		setData( sourceName, "parseMode", "journeys" );
	} else if ( parseDocumentMode == ParseForStopSuggestions ) {
		setData( sourceName, "parseMode", "stopSuggestions" );
	}
	setData( sourceName, "receivedData", "nothing" );
	setData( sourceName, "error", true );
	setData( sourceName, "errorCode", errorType );
	setData( sourceName, "errorString", errorString );
	setData( sourceName, "updated", QDateTime::currentDateTime() );
}

void PublicTransportEngine::progress( TimetableAccessor *accessor, qreal progress,
        const QString &jobDescription, const QUrl &requestUrl, const QString &serviceProvider,
        const QString &sourceName, const QString &city, const QString &stop, const QString &dataType,
        ParseDocumentMode parseDocumentMode )
{
    setData( sourceName, "serviceProvider", serviceProvider );
    setData( sourceName, "count", 0 );
    setData( sourceName, "progress", progress );
    setData( sourceName, "jobDescription", jobDescription );
    setData( sourceName, "requestUrl", requestUrl );
    if ( parseDocumentMode == ParseForDeparturesArrivals ) {
        setData( sourceName, "parseMode", "departures" );
    } else if ( parseDocumentMode == ParseForJourneys ) {
        setData( sourceName, "parseMode", "journeys" );
    } else if ( parseDocumentMode == ParseForStopSuggestions ) {
        setData( sourceName, "parseMode", "stopSuggestions" );
    }
    setData( sourceName, "receivedData", "nothing" );
//     setData( sourceName, "error", true );
//     setData( sourceName, "errorCode", errorType );
//     setData( sourceName, "errorString", errorString );
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
// 	if ( !m_accessors.contains(serviceProvider) ) {
// 		accessor = TimetableAccessor::createAccessor( serviceProvider );
// 		m_accessors.insert( serviceProvider, accessor );
// 	} else {
// 		accessor = m_accessors.value( serviceProvider );
// 	}

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

        minFetchWait = qMax( minFetchWait, accessor->minFetchWait() );
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
