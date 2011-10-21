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

#include "timetableaccessor_script.h"
#include "timetableaccessor_info.h"
#include "departureinfo.h"
#include "scripting.h"

#include <KStandardDirs>
#include <KLocalizedString>
#include <KDebug>
#include <kross/core/action.h>
#include <kross/core/manager.h>
#include <kconfig.h>
#include <kconfiggroup.h>

#include <QFile>
#include <QScriptValueIterator>

TimetableAccessorScript::TimetableAccessorScript( TimetableAccessorInfo *info, QObject *parent )
        : TimetableAccessorOnline(info, parent), m_script(0), m_resultObject(0)
{
    m_scriptState = WaitingForScriptUsage;
    m_scriptFeatures = readScriptFeatures();
}

TimetableAccessorScript::~TimetableAccessorScript()
{
    delete m_script;
}

bool TimetableAccessorScript::lazyLoadScript()
{
    if ( m_scriptState == ScriptLoaded ) {
        return true;
    }

    // Create the Kross::Action instance
    m_script = new Kross::Action( this, "TimetableParser" );

    m_script->addQObject( new Helper(m_info->serviceProvider(), m_script), "helper" );
    m_script->addQObject( new TimetableData(m_script), "timetableData" );
    m_resultObject = new ResultObject( m_script );
    m_script->addQObject( m_resultObject, "result"/*, Kross::ChildrenInterface::AutoConnectSignals*/ );

    bool ok = m_script->setFile( m_info->scriptFileName() );
    if ( !ok ) {
        m_scriptState = ScriptHasErrors;
    } else {
        m_script->trigger();
        m_scriptState = m_script->hadError() ? ScriptHasErrors : ScriptLoaded;
    }

    if ( m_scriptState == ScriptHasErrors ) {
        kDebug() << "Error in the script" << m_script->errorLineNo() << m_script->errorMessage();
    }
//     kDebug() << "Script initialized for" << m_info->serviceProvider() << m_scriptState;

    return m_scriptState == ScriptLoaded;
}

QStringList TimetableAccessorScript::readScriptFeatures()
{
    // Try to load script features from a cache file
    const QString fileName = accessorCacheFileName();
    const bool cacheExists = QFile::exists( fileName );
    KConfig cfg( fileName, KConfig::SimpleConfig );
    KConfigGroup grp = cfg.group( m_info->serviceProvider() );

    if ( cacheExists ) {
        // Check if the script file was modified since the cache was last updated
        QDateTime scriptModifiedTime = grp.readEntry("scriptModifiedTime", QDateTime());
        if ( QFileInfo(m_info->scriptFileName()).lastModified() == scriptModifiedTime ) {
            // Return feature list stored in the cache
            return grp.readEntry("features", QStringList());
        }
    }

    // No actual cached information about the service provider
    kDebug() << "No up-to-date cache information for service provider" << m_info->serviceProvider();
    QStringList features;
    bool ok = lazyLoadScript();
    if ( ok ) {
        QStringList functions = m_script->functionNames();

        if ( functions.contains("parsePossibleStops") ) {
            features << "Autocompletion";
        }
        if ( functions.contains("parseJourneys") ) {
            features << "JourneySearch";
        }

        if ( !m_script->functionNames().contains("usedTimetableInformations") ) {
            kDebug() << "The script has no 'usedTimetableInformations' function";
            kDebug() << "Functions in the script:" << m_script->functionNames();
            ok = false;
        }

        if ( ok ) {
            QStringList usedTimetableInformations = m_script->callFunction(
                    "usedTimetableInformations" ).toStringList();

            if ( usedTimetableInformations.contains("Delay", Qt::CaseInsensitive) ) {
                features << "Delay";
            }
            if ( usedTimetableInformations.contains("DelayReason", Qt::CaseInsensitive) ) {
                features << "DelayReason";
            }
            if ( usedTimetableInformations.contains("Platform", Qt::CaseInsensitive) ) {
                features << "Platform";
            }
            if ( usedTimetableInformations.contains("JourneyNews", Qt::CaseInsensitive)
                || usedTimetableInformations.contains("JourneyNewsOther", Qt::CaseInsensitive)
                || usedTimetableInformations.contains("JourneyNewsLink", Qt::CaseInsensitive) )
            {
                features << "JourneyNews";
            }
            if ( usedTimetableInformations.contains("TypeOfVehicle", Qt::CaseInsensitive) ) {
                features << "TypeOfVehicle";
            }
            if ( usedTimetableInformations.contains("Status", Qt::CaseInsensitive) ) {
                features << "Status";
            }
            if ( usedTimetableInformations.contains("Operator", Qt::CaseInsensitive) ) {
                features << "Operator";
            }
            if ( usedTimetableInformations.contains("StopID", Qt::CaseInsensitive) ) {
                features << "StopID";
            }
        }
    }

    // Store script features in a cache file
    grp.writeEntry( "scriptModifiedTime", QFileInfo(m_info->scriptFileName()).lastModified() );
    grp.writeEntry( "hasErrors", !ok );
    grp.writeEntry( "features", features );

    return features;
}

QStringList TimetableAccessorScript::scriptFeatures() const
{
    return m_scriptFeatures;
}

QString TimetableAccessorScript::decodeHtmlEntities(const QString& html)
{
    if ( html.isEmpty() ) {
        return html;
    }

    QString ret = html;
    QRegExp rx( "(?:&#)([0-9]+)(?:;)" );
    rx.setMinimal( true );
    int pos = 0;
    while ( (pos = rx.indexIn(ret, pos)) != -1 ) {
        int charCode = rx.cap( 1 ).toInt();
        QChar ch( charCode );
        ret = ret.replace( QString( "&#%1;" ).arg( charCode ), ch );
    }

    ret = ret.replace( "&nbsp;", " " );
    ret = ret.replace( "&amp;", "&" );
    ret = ret.replace( "&lt;", "<" );
    ret = ret.replace( "&gt;", ">" );
    ret = ret.replace( "&szlig;", "ß" );
    ret = ret.replace( "&auml;", "ä" );
    ret = ret.replace( "&Auml;", "Ä" );
    ret = ret.replace( "&ouml;", "ö" );
    ret = ret.replace( "&Ouml;", "Ö" );
    ret = ret.replace( "&uuml;", "ü" );
    ret = ret.replace( "&Uuml;", "Ü" );

    return ret;
}

QString TimetableAccessorScript::decodeHtml(const QByteArray& document, const QByteArray& fallbackCharset)
{
    // Get charset of the received document and convert it to a unicode QString
    // First parse the charset with a regexp to get a fallback charset
    // if QTextCodec::codecForHtml doesn't find the charset
    QString sDocument = QString( document );
    QTextCodec *textCodec;
    QRegExp rxCharset( "(?:<head>.*<meta http-equiv=\"Content-Type\" content=\"text/html; charset=)([^\"]*)(?:\"[^>]*>)", Qt::CaseInsensitive );
    rxCharset.setMinimal( true );
    if ( rxCharset.indexIn(sDocument) != -1 && rxCharset.isValid() ) {
        textCodec = QTextCodec::codecForName( rxCharset.cap(1).trimmed().toUtf8() );
    } else if ( !fallbackCharset.isEmpty() ) {
        textCodec = QTextCodec::codecForName( fallbackCharset );
    } else {
        textCodec = QTextCodec::codecForName( "UTF-8" );
    }
    sDocument = QTextCodec::codecForHtml( document, textCodec )->toUnicode( document );

    return sDocument;
}

bool TimetableAccessorScript::parseDocument( const QByteArray &document,
        QList<PublicTransportInfo*> *journeys, GlobalTimetableInfo *globalInfo,
        ParseDocumentMode parseDocumentMode )
{
//     kDebug() << "Called for" << m_info->serviceProvider();

    if ( !lazyLoadScript() ) {
        kDebug() << "Script couldn't be loaded" << m_info->scriptFileName();
        return false;
    }
    QString functionName = parseDocumentMode == ParseForJourneys
                           ? "parseJourneys" : "parseTimetable";
    if ( !m_script->functionNames().contains( functionName ) ) {
        kDebug() << "The script has no '" << functionName << "' function";
        kDebug() << "Functions in the script:" << m_script->functionNames();
        return false;
    }

    QString doc = TimetableAccessorScript::decodeHtml( document, m_info->fallbackCharset() );
    // Performance(?): Cut everything before "<body>" from the document
    doc = doc.mid( doc.indexOf("<body>", 0, Qt::CaseInsensitive) );

    kDebug() << "Parsing..." << parseDocumentMode;

    // Call script using Kross
    m_resultObject->clear();
    QVariant result = m_script->callFunction( functionName, QVariantList() << doc );

    if ( result.isValid() && result.canConvert(QVariant::StringList) ) {
        QStringList globalInfos = result.toStringList();
        if ( globalInfos.contains(QLatin1String("no delays"), Qt::CaseInsensitive) ) {
            // No delay information available for the given stop
            globalInfo->delayInfoAvailable = false;
        }
        if ( globalInfos.contains(QLatin1String("dates need adjustment"), Qt::CaseInsensitive) ) {
            globalInfo->datesNeedAdjustment = true;
        }
    }

    QList<TimetableData> data = m_resultObject->data();
    int count = 0;
    QDate curDate;
    QTime lastTime;
    int dayAdjustment = globalInfo->datesNeedAdjustment
            ? QDate::currentDate().daysTo(globalInfo->requestDate) : 0;
    if ( dayAdjustment != 0 ) {
        kDebug() << "Dates get adjusted by" << dayAdjustment << "days";
    }

    for ( int i = 0; i < data.count(); ++ i ) {
        TimetableData timetableData = data.at( i );

        // Set default vehicle type if none is set
        if ( !timetableData.values().contains(TypeOfVehicle) ||
            timetableData.value(TypeOfVehicle).toString().isEmpty() )
        {
            timetableData.set(TypeOfVehicle, static_cast<int>(m_info->defaultVehicleType()));
        }

        QDate date = timetableData.value( DepartureDate ).toDate();
        QTime departureTime = timetableData.value( DepartureTime ).toTime();
        if ( !date.isValid() ) {
            if ( curDate.isNull() ) {
                // First departure
                if ( QTime::currentTime().hour() < 3 && departureTime.hour() > 21 ) {
                    date = QDate::currentDate().addDays( -1 );
                } else if ( QTime::currentTime().hour() > 21 && departureTime.hour() < 3 ) {
                    date = QDate::currentDate().addDays( 1 );
                } else {
                    date = QDate::currentDate();
                }
            } else if ( lastTime.secsTo(departureTime) < -5 * 60 ) {
                // Time too much ealier than last time, estimate it's tomorrow
                date = curDate.addDays( 1 );
            } else {
                date = curDate;
            }
        }

        if ( dayAdjustment != 0 ) {
            date = date.addDays( dayAdjustment );
        }

        timetableData.set( DepartureDate, date );

        curDate = date;
        lastTime = departureTime;

        PublicTransportInfo *info;
        if ( parseDocumentMode == ParseForJourneys ) {
            info = new JourneyInfo( timetableData.values() );
        } else {
            info = new DepartureInfo( timetableData.values() );
        }

        if ( !info->isValid() ) {
            delete info;
            continue;
        }

        journeys->append( info );
        ++count;
    }

    if ( count == 0 ) {
        kDebug() << "The script didn't find anything";
    }
    return count > 0;
}

QString TimetableAccessorScript::parseDocumentForLaterJourneysUrl( const QByteArray &document )
{
//     kDebug() << "Called for" << m_info->serviceProvider();

    if ( !lazyLoadScript() ) {
        kDebug() << "Script couldn't be loaded" << m_info->scriptFileName();
        return QString();
    }
    if ( !m_script->functionNames().contains("getUrlForLaterJourneyResults") ) {
        kDebug() << "The script has no 'getUrlForLaterJourneyResults' function";
        kDebug() << "Functions in the script:" << m_script->functionNames();
        return QString();
    }

    QString doc = TimetableAccessorScript::decodeHtml( document );
    // Performance(?): Cut everything before "<body>" from the document
    doc = doc.mid( doc.indexOf( "<body>", 0, Qt::CaseInsensitive ) );

    // Call script
    QString result = m_script->callFunction( "getUrlForLaterJourneyResults",
                     QVariantList() << doc ).toString();
    if ( result.isEmpty() || result == "null" ) {
        return QString();
    } else {
        return TimetableAccessorScript::decodeHtmlEntities( result );
    }
}

QString TimetableAccessorScript::parseDocumentForDetailedJourneysUrl(
        const QByteArray &document )
{
//     kDebug() << "Called for" << m_info->serviceProvider();

    if ( !lazyLoadScript() ) {
        kDebug() << "Script couldn't be loaded" << m_info->scriptFileName();
        return QString();
    }
    if ( !m_script->functionNames().contains("getUrlForDetailedJourneyResults") ) {
        kDebug() << "The script has no 'getUrlForDetailedJourneyResults' function";
        kDebug() << "Functions in the script:" << m_script->functionNames();
        return QString();
    }

    QString doc = TimetableAccessorScript::decodeHtml( document );
    // Performance(?): Cut everything before "<body>" from the document
    doc = doc.mid( doc.indexOf( "<body>", 0, Qt::CaseInsensitive ) );

    QString result = m_script->callFunction( "getUrlForDetailedJourneyResults",
                        QVariantList() << doc ).toString();
    if ( result.isEmpty() || result == "null" ) {
        return QString();
    } else {
        return TimetableAccessorScript::decodeHtmlEntities( result );
    }
}

QString TimetableAccessorScript::parseDocumentForSessionKey(const QByteArray& document)
{
//     kDebug() << "Called for" << m_info->serviceProvider();

    if ( !lazyLoadScript() ) {
        kDebug() << "Script couldn't be loaded" << m_info->scriptFileName();
        return QString();
    }
    if ( !m_script->functionNames().contains("parseSessionKey") ) {
        kDebug() << "The script has no 'parseSessionKey' function";
        kDebug() << "Functions in the script:" << m_script->functionNames();
        return QString();
    }

    QString doc = TimetableAccessorScript::decodeHtml( document );
    // Performance(?): Cut everything before "<body>" from the document
//     doc = doc.mid( doc.indexOf( "<body>", 0, Qt::CaseInsensitive ) );

    QString result = m_script->callFunction( "parseSessionKey", QVariantList() << doc ).toString();
    if ( result.isEmpty() || result == "null" ) {
        return QString();
    } else {
        return result;
    }
}

bool TimetableAccessorScript::parseDocumentForStopSuggestions( const QByteArray &document,
        QList<StopInfo*> *stops )
{
//     kDebug() << "Called for" << m_info->serviceProvider();

    if ( !lazyLoadScript() ) {
        kDebug() << "Script couldn't be loaded" << m_info->scriptFileName();
        return false;
    }
    if ( !m_script->functionNames().contains("parsePossibleStops") ) {
        kDebug() << "The script has no 'parsePossibleStops' function" << m_script->file();
        kDebug() << "Functions in the script:" << m_script->functionNames();
        kDebug() << m_script->errorMessage();
        return false;
    }

    QString doc = TimetableAccessorScript::decodeHtml( document, m_info->fallbackCharset() );

    // Call script
    m_resultObject->clear();
    QVariant result = m_script->callFunction( "parsePossibleStops", QVariantList() << doc );
    if ( m_script->hadError() ) {
        kDebug() << "Error while running the 'parsePossibleStops' script function"
                 << m_script->errorMessage() << "at" << m_script->errorLineNo() << m_script->errorTrace();
    }

    QList<TimetableData> data = m_resultObject->data();
    int count = 0;
    foreach( const TimetableData &timetableData, data ) {
        QString stopName = timetableData.value( StopName ).toString();
        QString stopID, stopCity, stopCountryCode;
        int stopWeight = -1;

        if ( stopName.isEmpty() ) {
            continue;
        }

        if ( timetableData.values().contains(StopID) ) {
            stopID = timetableData.value( StopID ).toString();
        }
        if ( timetableData.values().contains(StopWeight) ) {
            stopWeight = timetableData.value( StopWeight ).toInt();
        }
        if ( timetableData.values().contains(StopCity) ) {
            stopCity = timetableData.value( StopCity ).toString();
        }
        if ( timetableData.values().contains(StopCountryCode) ) {
            stopCountryCode = timetableData.value( StopCountryCode ).toString();
        }

        stops->append( new StopInfo(stopName, stopID, stopWeight, stopCity, stopCountryCode) );
        ++count;
    }

    if ( count == 0 ) {
        kDebug() << "No stops found";
    }
    return count > 0;
}
