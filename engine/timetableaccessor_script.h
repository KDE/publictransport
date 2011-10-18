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

#ifndef TIMETABLEACCESSOR_HTML_HEADER
#define TIMETABLEACCESSOR_HTML_HEADER

#include "timetableaccessor.h"

namespace Kross {
    class Action;
}
class ResultObject;

/** @class TimetableAccessorScript
* @brief The base class for all scripted accessors.
*/
class TimetableAccessorScript : public TimetableAccessor {
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
     *
     * @note Can be used if you have a custom TimetableAccessorInfo object.
     *   TimetableAccessorXml uses this to create an HTML accessor for parsing of stop lists. */
    TimetableAccessorScript( TimetableAccessorInfo *info = new TimetableAccessorInfo() );

    /** @brief Destructor. */
    virtual ~TimetableAccessorScript();

    /** @brief Whether or not the script has been successfully loaded. */
    bool isScriptLoaded() const { return m_scriptState == ScriptLoaded; };

    bool hasScriptErrors() const { return m_scriptState == ScriptHasErrors; };

    /** @brief Gets a list of features that this accessor supports through a script. */
    virtual QStringList scriptFeatures() const;

    /** @brief Decodes HTML entities in @p html, e.g. "&nbsp;" is replaced by " ". */
    static QString decodeHtmlEntities( const QString &html );

    /**
     * @brief Decodes the given HTML document.
     *
     * First it tries QTextCodec::codecForHtml().
     * If that doesn't work, it parses the document for the charset in a meta-tag. */
    static QString decodeHtml( const QByteArray &document,
                               const QByteArray &fallbackCharset = QByteArray() );

protected:
    bool lazyLoadScript();
    QStringList readScriptFeatures();

    /**
     * @brief Calls the 'parseTimetable'/'parseJourneys' function in the script to
     *   parse the contents of a received document for a list of departures/arrivals
     *   or journeys (depending on @p parseDocumentMode) and puts the results
     *   into @p journeys.
     *
     * @param journeys A pointer to a list of departure/arrival or journey
     *   information. The results of parsing the document is stored in @p journeys.
     * @param parseDocumentMode The mode of parsing, e.g. parse for
     *   departures/arrivals or journeys.
     * @return true, if there were no errors and the data in @p journeys is valid.
     * @return false, if there were an error parsing the document. */
    virtual bool parseDocument( const QByteArray &document,
            QList<PublicTransportInfo*> *journeys, GlobalTimetableInfo *globalInfo,
            ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals );

    /**
     * @brief Calls the 'getUrlForLaterJourneyResults' function in the script to
     *   parse the contents of a received document for an url to a document
     *   containing later journeys.
     *
     * @return The parsed url. */
    virtual QString parseDocumentForLaterJourneysUrl( const QByteArray &document );

    /**
     * @brief Calls the 'getUrlForDetailedJourneyResults' function in the script to
     *   parse the contents of a received document for an url to a document
     *   containing detailed journey information.
     *
     * @return The parsed url. */
    virtual QString parseDocumentForDetailedJourneysUrl( const QByteArray &document );

    /**
     * @brief Calls the 'parseSessionKey' function in the script to
     *   parse the contents of a received document for the session key.
     *
     * @return The parsed session key or QString() if none was found. */
    virtual QString parseDocumentForSessionKey( const QByteArray &document );

    /**
     * @brief Calls the 'parsePossibleStops' function in the script to parse the contents of the
     *   given document for a list of possible stop names and puts the results into @p stops.
     *
     * @param document A document to be parsed.
     *
     * @param stops A pointer to a list of @ref StopInfo objects.
     *
     * @return true, if there were no errors.
     *
     * @return false, if there were an error parsing the document.
     *
     * @note Can be used if you have an html document containing a stop list.
     *   TimetableAccessorXml uses this to let the HTML accessor parse a downloaded
     *   document for stops.
     *
     * @see parseDocumentPossibleStops(QHash<QString,QString>*) */
    virtual bool parseDocumentPossibleStops( const QByteArray &document, QList<StopInfo*> *stops );

private:
    ScriptState m_scriptState; // The state of the script
    QStringList m_scriptFeatures; // Caches the features the script provides

    Kross::Action *m_script; // The script object
    ResultObject *m_resultObject; // An object used by the script to store results in
};

#endif // TIMETABLEACCESSOR_HTML_HEADER
