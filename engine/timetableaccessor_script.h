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

/** @file
 * @brief This file contains the base class for accessors using script files
 *   for parsing that are used by the public transport data engine.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_SCRIPT_HEADER
#define TIMETABLEACCESSOR_SCRIPT_HEADER

#include "timetableaccessor_online.h"

namespace Kross {
    class Action;
}
class ResultObject;

/**
 * @brief Gets timetable data for public transport from different service providers using scripts.
 *
 * Scripts can be written in languages supported by Kross, ie. JavaScript, Ruby or Python.
 * The tool TimetableMate can help implementing such scripts.
 *
 * Scripts are only loaded if needed, features read from a script function are cached.
 *
 * To add support for a new service provider using this accessor type you need to write an accessor
 * XML file and a script that parses documents from the service provider.
 * See @ref pageAccessorInfos.
 **/
class TimetableAccessorScript : public TimetableAccessorOnline {
    Q_OBJECT

    // Because the XML accessor uses TimetableAccessorScript::parseDocumentPossibleStops().
    friend class TimetableAccessorXml;

public:
    /** @brief States of the script, used for loading the script only when needed. */
    enum ScriptState {
        WaitingForScriptUsage = 0x00,
        ScriptLoaded = 0x01,
        ScriptHasErrors = 0x02
    };

    /**
     * @brief Creates a new TimetableAccessorScript object with the given information.
     *
     * @param info Information about how to download and parse the documents of a service provider.
     * @param parent The parent QObject.
     *
     * @note Can be used if you have a custom TimetableAccessorInfo object.
     *   TimetableAccessorXml uses this to create an HTML accessor for parsing of stop lists. */
    TimetableAccessorScript( TimetableAccessorInfo *info, QObject *parent = 0 );

    /** @brief Destructor. */
    virtual ~TimetableAccessorScript();

    /** @brief Gets the type of this accessor, ie ScriptedAccessor. */
    virtual AccessorType type() const { return ScriptedAccessor; };

    /** @brief Whether or not the script has been successfully loaded. */
    bool isScriptLoaded() const { return m_scriptState == ScriptLoaded; };

    /** @brief Whether or not the script has errors. */
    bool hasScriptErrors() const { return m_scriptState == ScriptHasErrors; };

    /** @brief Gets a list of features that this accessor supports through a script. */
    virtual QStringList scriptFeatures() const;

    /** @brief Decodes HTML entities in @p html, e.g. "&nbsp;" is replaced by " ". */
    static QString decodeHtmlEntities( const QString &html );

    /**
     * @brief Decodes the given HTML document.
     *
     * First it tries QTextCodec::codecForHtml().
     * If that doesn't work, it parses the document for the charset in a meta-tag.
     **/
    static QString decodeHtml( const QByteArray &document,
                               const QByteArray &fallbackCharset = QByteArray() );

protected:
    /**
     * @brief Loads the script if it was not already loaded.
     *
     * @return True, if the script could be loaded successfully or was already loaded.
     *   False, otherwise.
     **/
    bool lazyLoadScript();

    /**
     * @brief Reads features of the script using a special function in the script.
     *
     * If the script implements a function with the name @verbatim usedTimetableInformations @endverbatim
     * it is used to construct the list of features.
     *
     * Script features are cached in the accessor cache file
     * (Timetableaccessor::accessorCacheFileName()) and updated if the last modified time of the
     * script changes.
     **/
    QStringList readScriptFeatures();

    /**
     * @brief Uses the script to parse @p document for departures/arrivals/journeys.
     *
     * Calls the 'parseTimetable'/'parseJourneys' function in the script to parse the contents of
     * the received @p document for a list of departures/arrivals or journeys (depending on
     * @p parseDocumentMode) and puts the results into @p journeys.
     *
     * @param document The contents of the document that should be parsed.
     * @param journeys A pointer to a list of departure/arrival or journey
     *   information. The results of parsing the document is stored in @p journeys.
     * @param globalInfo Information for all items get stored in this object.
     * @param parseDocumentMode The mode of parsing, e.g. parse for
     *   departures/arrivals or journeys.
     * @return true, if there were no errors and the data in @p journeys is valid.
     * @return false, if there were an error parsing the document.
     **/
    virtual bool parseDocument( const QByteArray &document,
            QList<PublicTransportInfo*> *journeys, GlobalTimetableInfo *globalInfo,
            ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals );

    /**
     * @brief Uses the script to parse @p document for stop suggestions.
     *
     * Calls the 'parsePossibleStops' function in the script to parse the contents of @p document
     * for a list of stop suggestions and puts the results into @p stops.
     *
     * @param document The contents of the document that should be parsed.
     * @param stops A pointer to a list of @ref StopInfo objects.
     * @return true, if there were no errors.
     * @return false, if there were an error parsing the document.
     **/
    virtual bool parseDocumentForStopSuggestions( const QByteArray &document,
                                                  QList<StopInfo*> *stops );

    /**
     * @brief Uses the script to parse @p document for an URL to later journeys.
     *
     * Calls the 'getUrlForLaterJourneyResults' function in the script to parse @p document for
     * an URL to a document containing later journeys. This is for service providers that only
     * provide a very limited number of journeys in one document, but contain an URL to the next
     * set of journeys.
     *
     * @param document The contents of the document that should be parsed.
     * @return The parsed URL.
     **/
    virtual QString parseDocumentForLaterJourneysUrl( const QByteArray &document );

    /**
     * @brief Uses the script to parse @p document for an URL to more detailed journeys.
     *
     * Calls the 'getUrlForDetailedJourneyResults' function in the script to parse @p document for
     * an URL to a document containing detailed journey information. This is for service providers
     * that won't directly put all available information into the document, but contain an URL
     * to a document with more details.
     *
     * @param document The contents of the document that should be parsed.
     * @return The parsed URL.
     **/
    virtual QString parseDocumentForDetailedJourneysUrl( const QByteArray &document );

    /**
     * @brief Uses the script to parse @p document for a session key.
     *
     * Calls the 'parseSessionKey' function in the script to parse @p document for the session key.
     *
     * @param document The contents of the document that should be parsed.
     * @return The parsed session key or QString() if none was found.
     **/
    virtual QString parseDocumentForSessionKey( const QByteArray &document );

private:
    ScriptState m_scriptState; // The state of the script
    QStringList m_scriptFeatures; // Caches the features the script provides

    Kross::Action *m_script; // The script object
    ResultObject *m_resultObject; // An object used by the script to store results in
};

#endif // TIMETABLEACCESSOR_SCRIPT_HEADER
