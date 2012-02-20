/*
 *   Copyright 2012 Friedrich Pülz <fieti1983@gmx.de>
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

#include "javascriptcompletionmodel.h"

#include <KTextEditor/View>
#include <KTextEditor/Document>
#include <KTextEditor/TemplateInterface>
#include <KTextBrowser>
#include <KActionCollection>
#include <KIcon>
#include <KLocale>

JavaScriptCompletionModel::JavaScriptCompletionModel( const QString& completionShortcut,
						      QObject* parent )
					: KTextEditor::CodeCompletionModel( parent ) {
    m_completionShortcut = completionShortcut;
    initGlobalFunctionCompletion();
    initTimetableInfoCompletion();
    initObjectMethodCompletion();
}

QVariant JavaScriptCompletionModel::data( const QModelIndex& index, int role ) const {
    if ( index.column() == Icon && role == Qt::DecorationRole ) {
        CompletionItem completion = m_completions.at( index.row() );
        if ( completion.properties.testFlag(Function) )
            return KIcon("code-function");
        else if ( completion.properties.testFlag(Class) )
            return KIcon("code-class");
        else if ( completion.properties.testFlag(Const) )
            return KIcon("code-variable");
    }

    if ( role == ItemSelected ) {
        return m_completions.at( index.row() ).description;
    } else if ( role == IsExpandable ) {
        return true;
    } else if ( role == ExpandingWidget ) {
        QVariant v;
        KTextBrowser *textBrowser = new KTextBrowser;
        textBrowser->setText( m_completions.at(index.row()).description );
        textBrowser->setGeometry( 0, 0, 100, 85 ); // Make the widget a bit bigger
        textBrowser->setReadOnly( true );
        textBrowser->setTextInteractionFlags( Qt::LinksAccessibleByKeyboard |
                                              Qt::LinksAccessibleByMouse );
        v.setValue< QWidget* >( textBrowser );
        return v;
    } else if ( role == CompletionRole ) {
        return (int)m_completions.at( index.row() ).properties;
    }

    if ( role == Qt::DisplayRole ) {
        if ( index.column() == Name ) {
            return m_completions.at( index.row() ).name;
        } else if ( index.column() == Prefix ) {
            const QString prefix = m_completions.at( index.row() ).prefix;
            return prefix.isEmpty() ? QVariant() : prefix;
        } else if ( index.column() == Postfix ) {
            const QString postfix = m_completions.at( index.row() ).postfix;
            return postfix.isEmpty() ? QVariant() : postfix;
        }
    }

    return QVariant();
}

void JavaScriptCompletionModel::executeCompletionItem( KTextEditor::Document* document,
					    const KTextEditor::Range& word, int row ) const {
    kDebug() << "Completion" << word << row;
    CompletionItem completion = m_completions.at( row );
    if ( completion.isTemplate ) {
        KTextEditor::TemplateInterface *templateInterface =
            qobject_cast<KTextEditor::TemplateInterface*>( document->activeView() );
        if ( templateInterface ) {
	    KTextEditor::Cursor cursor = word.start();
            document->removeText( word );
            templateInterface->insertTemplateText( cursor, completion.completion,
                                                   QMap<QString, QString>() );
        } else
            kDebug() << "No template interface";
    } else {
        document->replaceText( word, completion.completion );
    }
}

void JavaScriptCompletionModel::completionInvoked( KTextEditor::View* view,
			    const KTextEditor::Range& range,
			    KTextEditor::CodeCompletionModel::InvocationType /*invocationType*/ ) {
    m_completions.clear();
    setRowCount( 0 );

    KTextEditor::Range leftRange( KTextEditor::Cursor(0, 0), range.start() );
    QString leftText = stripComments( view->document()->text(leftRange) );
    int blockLevel = leftText.count( '{' ) - leftText.count( '}' );
    if ( blockLevel < 0 ) {
        kDebug() << "More closing '}' found than opening '{' at line"
		 << range.start().line();
        return;
    } else { // at root level or inside function
        QString word = view->document()->text( range );

        kDebug() << "Completion word" << word;
        QString text, textUntilWhiteSpace, textUntilLineBegin;
        int col = range.start().column();
        QString line = view->document()->line( range.end().line() );

        textUntilLineBegin = line.left( col ).trimmed();
        kDebug() << "Complete Start:" << textUntilLineBegin << col;

        QRegExp rxWordBegin( "\\s" );
        int pos = rxWordBegin.lastIndexIn( line, col - 1 );
        if ( pos == -1 ) {
            textUntilWhiteSpace = textUntilLineBegin;
            kDebug() << "Using all left (pos is -1):" << textUntilWhiteSpace << col;
        } else {
            textUntilWhiteSpace = line.mid( pos + 1, col - pos - 1 ).trimmed();
            kDebug() << "Word Start:" << pos << textUntilWhiteSpace << col;
        }
            m_completionObjects[""].insert("test", CompletionItem(0, "", "", ""));

        if ( word.isEmpty() )
            text = textUntilWhiteSpace;
        else
            text = textUntilWhiteSpace + word;

        if ( blockLevel == 0 ) { // at root level
            m_completions << m_completionsGlobalFunctions.values();
        } else { // inside function
            QRegExp objectRegExp( "^(\\w+)\\." );
            if ( objectRegExp.indexIn(text) != -1 ) {
                m_completions << m_completionObjects[ objectRegExp.cap(1).toLower() ].values();
//             if ( text.startsWith(QLatin1String("helper.")) ) {
//                 m_completions << m_completionsHelper.values();
//             } else if ( text.startsWith(QLatin1String("timetableData.set( '"))
//                      || textUntilLineBegin.startsWith(QLatin1String("timetableData.set( '")) )
//             { // TODO no set() method, but result.addData( {TimetableInfo: 'Value'} );
//                 // Add timetable info completions
//                 m_completions << m_completionsTimetableInfo.values();
//             } else if ( text.startsWith(QLatin1String("result.")) ) {
//                 m_completions << m_completionsCalls["call:result.addData"];
            } else {
                m_completions << CompletionItem( Class | GlobalScope, "helper",
                        i18nc("@info The description for the 'helper' object",
                              "The <emphasis>helper</emphasis> object contains some useful "
                              "functions."),
                        "helper.", false, "object" );
                m_completions << CompletionItem( Class | GlobalScope, "network",
                        i18nc("@info The description for the 'network' object",
                              "The <emphasis>network</emphasis> object is used request documents "
                              "from the internet.<nl/>"),
                        "network.", false, "object" );
                m_completions << CompletionItem( Class | GlobalScope, "storage",
                        i18nc("@info The description for the 'storage' object",
                              "The <emphasis>storage</emphasis> object can be used to store some "
                              "script specific values in memory or on disk.<nl/>"),
                        "storage.", false, "object" );
                m_completions << CompletionItem(  Class | GlobalScope, "result",
                        i18nc("@info The description for the 'result' object",
                              "The result object is used to store all parsed "
                              "departure/arrival/journey items. Call <emphasis>"
                              "result.addData({Target: 'Sample', DepartureDateTime: new Date()})</emphasis> "
                              "to add new data."),
                        "result.", false, "object" );
            }
        }
    }

    setRowCount( m_completions.count() );
    reset();
}

QString JavaScriptCompletionModel::stripComments( const QString& text ) const {
    QString ret = text;
    QRegExp rx( "//.*\n|/\\*.*\\*/" );
    rx.setMinimal( true );
    return ret.remove( rx );
}

CompletionItem JavaScriptCompletionModel::completionItemFromId( const QString id ) {
    CompletionItem item = m_completionsGlobalFunctions.value( id );
    if ( item.isValid() ) {
        return item;
    }

    item = m_completionsTimetableInfo.value( id );
    if ( item.isValid() ) {
        return item;
    }

//     item = m_completionsCalls.value( id );
//     if ( item.isValid() ) {
//         return item;
//     }

    QString simpleID = id;
    QRegExp rxBraces( "\\([^\\)]*\\)" );
    rxBraces.setMinimal( true );
    simpleID.replace( rxBraces, "()" );
    for ( int i = 0; i < m_completionObjects.count(); ++i ) {
        const QHash< QString, CompletionItem > comp =
                m_completionObjects[ m_completionObjects.keys()[i] ];
        item = comp.value( simpleID );
        if ( item.isValid() ) {
            return item;
        }
    }

    QRegExp rxCall( "(\\w+)\\.(\\w+)" );
    if ( rxCall.indexIn(simpleID) != -1 ) {
        const QString obj = rxCall.cap(1);
        const QString func = QString("call:%1()").arg(rxCall.cap(2));
//         kDebug() << "FOUND" << obj << m_completionObjects.contains(obj) << func << m_completionObjects[obj].contains(func);
//         kDebug() << m_completionObjects[obj].keys();
        if ( m_completionObjects.contains(obj) && m_completionObjects[obj].contains(func) ) {
            return m_completionObjects[obj][func];
        }
    }

    return item;
}

void JavaScriptCompletionModel::initGlobalFunctionCompletion() {
    m_completionsGlobalFunctions.insert( "func:usedTimetableInformations()",
	    CompletionItem( Function | GlobalScope,
            "usedTimetableInformations()",
            i18nc("@info The description for the 'usedTimetableInformations' function",
                  "Should be implemented to tell which features the script supports.<nl/>"
                  "This function is called by the data engine."),
            "\n// This function returns a list of all features supported by this script.\n"
	    "function usedTimetableInformations() {\n"
	    "\t// These strings are currently recognized as features:\n"
	    "\t//   'Delay', 'DelayReason', 'Platform', 'JourneyNews', 'TypeOfVehicle',\n"
	    "\t//   'StopID', 'Pricing', 'Changes', 'RouteStops', 'RoutePlatformsDeparture',\n"
	    "\t//   'RoutePlatformsArrival', 'RouteTimesDeparture', 'RoutePlatformsArrival',\n"
	    "\t//   'RouteTransportLines'.\n"
	    "\treturn [ '${cursor}' ];\n"
	    "}\n",
            true, "Implement string array", "                   ") ); // The spaces make the completion
		    // box wider, so that the code snipped can be read

    m_completionsGlobalFunctions.insert( "func:getTimetable()",
	    CompletionItem( Function | GlobalScope,
            "getTimetable( stop, dateTime, maxCount, dataType, city )",
            i18nc("@info The description for the 'getTimetable' function",
                  "Requests and parses departure/arrival documents.<nl/>"
                  "This function is called by the data engine. Found departures/arrivals can be "
                  "handed over to the data engine like this:<nl/>" //<bcode>"
                  "<icode>  // Add timetable data to the result set</icode><nl/>"
                  "<icode>  result.addData( {TransportLine: '603',</icode><nl/>"
                  "<icode>                   TypeOfVehicle: 'bus',</icode><nl/>"
                  "<icode>                   Target: 'Samplestreet',</icode><nl/>"
                  "<icode>                   DepartureDateTime: new Date(),</icode><nl/>"
                  "<icode>                   Delay: 4} );</icode><nl/><nl/>"
                  "<note>You <emphasis>can</emphasis> return a string array with keywords "
                  "that affect all departures/arrivals. Currently only one such keyword is "
                  "supported: <emphasis>'no delays'</emphasis>, used to indicate that "
                  "there is no delay information for the given stop. The data engine can "
                  "then use a higher timeout for the next data update. When delay "
                  "information is available updates are done more often, because delays "
                  "may change.</note>"),
// 				      "</bcode>"),
            "\n// This function normally requests a document (eg. HTML or XML) and then parses "
            "it for departure/arrival data.\n"
            "function getTimetable( stop, dateTime, maxCount, dataType, city ) {\n"
            "\t// Find block of departures\n"
            "\t// TODO: Adjust so that you get the block that contains\n"
            "\t// the departures in the document\n"
            "\tvar str = helper.extractBlock( html, "
            "'<table ${departure_table}>', '</table>' );\n\n"
            "\t// Initialize regular expressions\n"
            "\t// TODO: Adjust the reg exp\n"
            "\tvar departuresRegExp = /<tr>([\\s\\S]*?)<\\/tr>/ig;\n\n"
            "\t// Go through all departure blocks\n"
            "\twhile ( (departureRow = departuresRegExp.exec(str)) ) {\n"
            "\t\t// This gets the current departure row\n"
            "\t\tdepartureRow = departureRow[1];\n\n"
            "\t\t// TODO: Parse the departure row for departure data\n"
            "\t\t${cursor}\n\n"
            "\t\t// Add departure to the result set\n"
            "\t\t// TODO: Fill in parsed values instead of the sample strings.\n"
            "\t\t// You can also add other information, use the code completion\n"
            "\t\t// (" + m_completionShortcut + ") for more information.\n"
            "\t\tresult.addData( {TransportLine: 'Sample line 4',\n"
            "\t\t                 TypeOfVehicle: 'bus',\n"
            "\t\t                 Target: 'Sample target',\n"
            "\t\t                 DepartureDateTime: new Date(),\n"
            "\t\t                 Delay: 4} );\n"
            "\t}\n"
            "}\n",
            true, "Implement string array", "                   " )); // The spaces make the completion
    // box wider, so that the code snipped can be read

    m_completionsGlobalFunctions.insert( "func:getJourneys()",
	    CompletionItem( Function | GlobalScope,
            "getJourneys( originStop, targetStop, dateTime, maxCount, dataType, city )",
            i18nc("@info The description for the 'getJourneys' function",
                  "Requests and parses journey documents.<nl/>"
                  "This function is called by the data engine. Found journeys can "
                  "be handed over to the data engine like this:<nl/>" //<bcode>"
                  "<icode>  // Add timetable data to the result set</icode><nl/>"
                  "<icode>  result.addData( {StartStopName: 'A',</icode><nl/>"
                  "<icode>                   TargetStopName: 'B',</icode><nl/>"
                  "<icode>                   Target: 'Samplestreet',</icode><nl/>"
                  "<icode>                   DepartureDateTime: new Date(),</icode><nl/>"
                  "<icode>                   ArrivalDateTime: new Date(),</icode><nl/>"
                  "<icode>                   Changes: 3,</icode><nl/>"
                  "<icode>                   Pricing: '2,30 €'} );</icode><nl/><nl/>"),
// 				      "</bcode>"),
            "\n// This function normally requests a document (eg. HTML or XML) and then parses it "
            "for journey data.\n"
	    "function getJourneys( originStop, targetStop, dateTime, maxCount, dataType, city ) {\n"
	    "\t${cursor}\n"
	    "}\n",
            true, "Implement void", "                   " )); // The spaces make the completion
    // box wider, so that the code snipped can be read

    m_completionsGlobalFunctions.insert( "func:getStopSuggestions()",
	    CompletionItem( Function | GlobalScope,
            "getStopSuggestions( stop, maxCount, city )",
            i18nc("@info The description for the 'getStopSuggestions' function",
                  "Requests and parses stop suggestion documents.<nl/>"
                  "This function is called by the data engine. The parameter "
                  "contains the contents of the document body. Found stop data "
                  "can be handed over to the data engine like this:<nl/>" //<bcode>"
                  "<icode>  // Add timetable data to the result set</icode><nl/>"
                  "<icode>  result.addData( {StopName: 'TestName', StopID: 100} );</icode>"),
// 				      "</bcode>"),
            "\n// This function normally requests a document (eg. HTML or XML) and then parses it "
            "for stop suggestions.\n"
	    "function getStopSuggestions( stop, maxCount, city ) {\n"
	    "\t${cursor}\n"
	    "}\n",
            true, "Implement void", "                   " )); // The spaces make the completion
    // box wider, so that the code snipped can be read

    // Deprecated functions: parseTimetable(html), parseJourneys(html), parsePossibleStops(html)
    // getUrlForLaterJourneyResults(html), getUrlForDetailedJourneyResults(html)
//     m_completionsGlobalFunctions.insert( "func:getUrlForLaterJourneyResults()",
// 	    CompletionItem( Function | GlobalScope,
//             "getUrlForLaterJourneyResults( html )",
//             i18nc("@info The description for the 'getUrlForLaterJourneyResults' function",
//                   "Parses a journey document for a link to a journey "
//                   "document containing later journeys.<nl/>"
//                   "This function is called by the data engine. The parameter "
//                   "contains the contents of the document body. The found link "
//                   "can be simply returned. If no link could be found, return null."),
//             "\n// This function parses a given HTML document for a link to later journeys.\n"
// 	    "function getUrlForLaterJourneyResults( html ) {\n"
// 	    "\treturn ${cursor};\n"
// 	    "}\n",
//             true, "Implement string", "                   " )); // The spaces make the completion
//     // box wider, so that the code snipped can be read
//
//     m_completionsGlobalFunctions.insert( "func:getUrlForDetailedJourneyResults()",
// 	    CompletionItem( Function | GlobalScope,
//             "getUrlForDetailedJourneyResults( html )",
//             i18nc("@info The description for the 'getUrlForDetailedJourneyResults' function",
//                   "Parses a journey document for a link to another journey "
//                   "document containing more details about journeys.<nl/>"
//                   "This function is called by the data engine. The parameter "
//                   "contains the contents of the document body. "
//                   "The found link can be simply returned. If no link could be found, return null."),
//             "\n// This function parses a given HTML document\n"
//             "// for a link to a more detailed journey document.\n"
// 	    "function getUrlForLaterJourneyResults( html ) {\n"
// 	    "\treturn ${cursor};\n"
// 	    "}\n",
//             true, "Implement string", "                   " )); // The spaces make the completion
//     // box wider, so that the code snipped can be read
}

void JavaScriptCompletionModel::initObjectMethodCompletion() {
    // This code was generated by TimetableMateCompletionGenerator
    // Completion for helper.errorReceived()
    m_completionObjects["helper"].insert( "call:errorReceived()", CompletionItem(Function,
            "errorReceived(string message, string failedParseText)",
            "<b>Brief:</b> An error was received from the script."
            "<br><b>Parameter <i>message</i>:</b> The error message. "
            "<br><b>Parameter <i>failedParseText</i>:</b> The text in the source document where parsing failed.",
            "errorReceived(${message}, ${failedParseText});", true, "void") );
    // Completion for helper.error()
    m_completionObjects["helper"].insert( "call:error()", CompletionItem(Function,
            "error(string message, string failedParseText)",
            "<b>Brief:</b> Prints <i>message</i> on stdout and logs it in a file."
            "Logs the error message with the given data string, eg. the HTML code where parsing failed. "
            "The message gets also send to stdout with a short version of the data "
            "The log file is normally located at \"~/.kde4/share/apps/plasma_engine_publictransport/accessors.log\". "
            "<br><b>Parameter <i>message</i>:</b> The error message. "
            "<br><b>Parameter <i>failedParseText</i>:</b> The text in the source document where parsing failed.",
            "error(${message}, ${failedParseText});", true, "void") );
    // Completion for helper.decodeHtmlEntities()
    m_completionObjects["helper"].insert( "call:decodeHtmlEntities()", CompletionItem(Function,
            "decodeHtmlEntities(string html)",
            "<b>Brief:</b> Decodes HTML entities in <i>html</i>."
            "For example \"&nbsp;\" gets replaced by \" \". "
            "HTML entities which include a charcode, eg. \"&#100;\" are also replaced, in the example "
            "by the character for the charcode 100, ie. QChar(100). "
            "<br><b>Parameter <i>html</i>:</b> The string to be decoded. "
            "<br><b>Returns:</b> <i>html</i> with decoded HTML entities.",
            "decodeHtmlEntities(${html});", true, "string") );
    // Completion for helper.trim()
    m_completionObjects["helper"].insert( "call:trim()", CompletionItem(Function,
            "trim(string str)",
            "<b>Brief:</b> Trims spaces from the beginning and the end of the given string <i>str</i>."
            "<br><b>Note:</b>  The HTML entitiy <em>&nbsp;</em> is also trimmed. "
            "<br><b>Parameter <i>str</i>:</b> The string to be trimmed. "
            "<br><b>Returns:</b> <i>str</i> without spaces at the beginning or end.",
            "trim(${str});", true, "string") );
    // Completion for helper.stripTags()
    m_completionObjects["helper"].insert( "call:stripTags()", CompletionItem(Function,
            "stripTags(string str)",
            "<b>Brief:</b> Removes all HTML tags from str."
            "<br><b>Parameter <i>str</i>:</b> The string from which the HTML tags should be removed. "
            "<br><b>Returns:</b> <i>str</i> without HTML tags.",
            "stripTags(${str});", true, "string") );
    // Completion for helper.camelCase()
    m_completionObjects["helper"].insert( "call:camelCase()", CompletionItem(Function,
            "camelCase(string str)",
            "<b>Brief:</b> Makes the first letter of each word upper case, all others lower case."
            "<br><b>Parameter <i>str</i>:</b> The input string. "
            "<br><b>Returns:</b> <i>str</i> in camel case.",
            "camelCase(${str});", true, "string") );
    // Completion for helper.extractBlock()
    m_completionObjects["helper"].insert( "call:extractBlock()", CompletionItem(Function,
            "extractBlock(string str, string beginString, string endString)",
            "<b>Brief:</b> Extracts a block from <i>str</i>, which begins at the first occurance of <i>beginString</i>"
            "in <i>str</i> and end at the first occurance of <i>endString</i> in <i>str</i>. "
            "<br><b>Parameter <i>str</i>:</b> The input string. "
            "<br><b>Parameter <i>beginString</i>:</b> A string to search for in <i>str</i> and to use as start position. "
            "<br><b>Parameter <i>endString</i>:</b> A string to search for in <i>str</i> and to use as end position. "
            "<br><b>Returns:</b> The text block in <i>str</i> between <i>beginString</i> and <i>endString</i>.",
            "extractBlock(${str}, ${beginString}, ${endString});", true, "string") );
    // Completion for helper.matchTime()
    m_completionObjects["helper"].insert( "call:matchTime()", CompletionItem(Function,
            "matchTime(string str, string format)",
            "<b>Brief:</b> Gets a map with the hour and minute values parsed from <i>str</i> using <i>format</i>."
            "QVariantMap gets converted to an object in scripts. The result can be used in the script "
            "like this: "
            "<br><b>Code example:</b><br> "
            "var time = matchTime( \"15:23\" );<br> "
            "if ( !time.error ) {<br> "
            "var hour = time.hour;<br> "
            "var minute = time.minute;<br> "
            "}<br> "
            "<br> "
            "<br><b>Parameter <i>str</i>:</b> The string containing the time to be parsed, eg. \"08:15\". "
            "<br><b>Parameter <i>format</i>:</b> The format of the time string in <i>str</i>. Default is \"hh:mm\". "
            "<br><b>Returns:</b> A map with two values: 'hour' and 'minute' parsed from <i>str</i>. On error it contains "
            "an 'error' value of true. "
            "<br><b>See also:</b>  formatTime",
            "matchTime(${str}, ${format});", true, "object") );
    // Completion for helper.matchDate()
    m_completionObjects["helper"].insert( "call:matchDate()", CompletionItem(Function,
            "matchDate(string str, string format)",
            "<b>Brief:</b> Gets a date object parsed from <i>str</i> using <i>format</i>."
            "<br><b>Parameter <i>str</i>:</b> The string containing the date to be parsed, eg. \"2010-12-01\". "
            "<br><b>Parameter <i>format</i>:</b> The format of the time string in <i>str</i>. Default is \"YY-MM-dd\". "
            "<br><b>See also:</b>  formatDate TODO",
            "matchDate(${str}, ${format});", true, "date") );
    // Completion for helper.formatTime()
    m_completionObjects["helper"].insert( "call:formatTime()", CompletionItem(Function,
            "formatTime(int hour, int minute, string format)",
            "<b>Brief:</b> Formats the time given by the values <i>hour</i> and <i>minute</i>"
            "as string in the given <i>format</i>. "
            "<br><b>Parameter <i>hour</i>:</b> The hour value of the time. "
            "<br><b>Parameter <i>minute</i>:</b> The minute value of the time. "
            "<br><b>Parameter <i>format</i>:</b> The format of the time string to return. Default is \"hh:mm\". "
            "<br><b>Returns:</b> The formatted time string. "
            "<br><b>See also:</b>  matchTime",
            "formatTime(${hour}, ${minute}, ${format});", true, "string") );
    // Completion for helper.formatDate()
    m_completionObjects["helper"].insert( "call:formatDate()", CompletionItem(Function,
            "formatDate(int year, int month, int day, string format)",
            "<b>Brief:</b> Formats the time given by the values <i>hour</i> and <i>minute</i>"
            "as string in the given <i>format</i>. "
            "<br><b>Parameter <i>year</i>:</b> The year value of the date. "
            "<br><b>Parameter <i>month</i>:</b> The month value of the date. "
            "<br><b>Parameter <i>day</i>:</b> The day value of the date. "
            "<br><b>Parameter <i>format</i>:</b> The format of the date string to return. Default is \"yyyy-MM-dd\". "
            "<br><b>Returns:</b> The formatted date string. "
            "<br><b>See also:</b>  matchTime",
            "formatDate(${year}, ${month}, ${day}, ${format});", true, "string") );
    // Completion for helper.formatDateTime()
    m_completionObjects["helper"].insert( "call:formatDateTime()", CompletionItem(Function,
            "formatDateTime(date dateTime, string format)",
            "<b>Brief:</b> Formats <i>dateTime</i> using <i>format</i>.",
            "formatDateTime(${dateTime}, ${format});", true, "string") );
    // Completion for helper.duration()
    m_completionObjects["helper"].insert( "call:duration()", CompletionItem(Function,
            "duration(string sTime1, string sTime2, string format)",
            "<b>Brief:</b> Calculates the duration in minutes from the time in <i>sTime1</i> until <i>sTime2</i>."
            "<br><b>Parameter <i>sTime1</i>:</b> A string with the start time, in the given <i>format</i>. "
            "<br><b>Parameter <i>sTime2</i>:</b> A string with the end time, in the given <i>format</i>. "
            "<br><b>Parameter <i>format</i>:</b> The format of <i>sTime1</i> and <i>sTime2</i>. Default is \"hh:mm\". "
            "<br><b>Returns:</b> The number of minutes from <i>sTime1</i> until <i>sTime2</i>.",
            "duration(${sTime1}, ${sTime2}, ${format});", true, "int") );
    // Completion for helper.addMinsToTime()
    m_completionObjects["helper"].insert( "call:addMinsToTime()", CompletionItem(Function,
            "addMinsToTime(string sTime, int minsToAdd, string format)",
            "<b>Brief:</b> Adds <i>minsToAdd</i> minutes to the time in <i>sTime</i>."
            "<br><b>Parameter <i>sTime</i>:</b> A string with the base time. "
            "<br><b>Parameter <i>minsToAdd</i>:</b> The number of minutes to add to <i>sTime</i>. "
            "<br><b>Parameter <i>format</i>:</b> The format of <i>sTime</i>. Default is \"hh:mm\". "
            "<br><b>Returns:</b> A time string formatted in <i>format</i> with the calculated time.",
            "addMinsToTime(${sTime}, ${minsToAdd}, ${format});", true, "string") );
    // Completion for helper.addDaysToDate()
    m_completionObjects["helper"].insert( "call:addDaysToDate()", CompletionItem(Function,
            "addDaysToDate(date dateTime, int daysToAdd)",
            "<b>Brief:</b> ",
            "addDaysToDate(${dateTime}, ${daysToAdd});", true, "date") );
    // Completion for helper.addDaysToDateArray()
    m_completionObjects["helper"].insert( "call:addDaysToDateArray()", CompletionItem(Function,
            "addDaysToDateArray(list values, int daysToAdd)",
            "<b>Brief:</b> ",
            "addDaysToDateArray(${values}, ${daysToAdd});", true, "list") );
    // Completion for helper.splitSkipEmptyParts()
    m_completionObjects["helper"].insert( "call:splitSkipEmptyParts()", CompletionItem(Function,
            "splitSkipEmptyParts(string str, string sep)",
            "<b>Brief:</b> Splits <i>str</i> at <i>sep</i>, but skips empty parts."
            "<br><b>Parameter <i>str</i>:</b> The string to split. "
            "<br><b>Parameter <i>sep</i>:</b> The separator. "
            "<br><b>Returns:</b> A list of string parts.",
            "splitSkipEmptyParts(${str}, ${sep});", true, "list") );
    // Completion for helper.findTableHeaderPositions()
    m_completionObjects["helper"].insert( "call:findTableHeaderPositions()", CompletionItem(Function,
            "findTableHeaderPositions(string str, object options)",
            "<b>Brief:</b> Finds positions of columns in an HTML table."
            "Table header names are currently only found as \"class\" attributes of \"th\" tags. "
            "<br><b>Parameter <i>str</i>:</b> The string is in which to search for positions of table headers. "
            "<br><b>Parameter <i>options</i>:</b> A map (javascript object) with these optional properties: "
            "<br> <b>&bull;</b>  <b>required</b>: A list of strings, ie. the names of the required table headers. "
            "<br> <b>&bull;</b>  <b>optional</b>: A list of strings, ie. the names of the optional table headers. "
            "<br> <b>&bull;</b>  <b>debug</b>: A boolean, false by default. If true, more debug output gets generated. "
            "<br> <b>&bull;</b>  <b>headerContainerOptions</b>: A map of options that gets passed to findFirstHtmlTag() "
            "to find the HTML tag (eg. \"tr\") containing the header HTML tags (eg. \"th\"). For example "
            "this can be used to specify required attributes for the header container tag. "
            "Additionally this map can contain a value \"tagName\", by default this is \"tr\". "
            "<br> <b>&bull;</b>  <b>headerOptions</b>: A map of options that gets passed to findFirstHtmlTag() "
            "to find the header HTML tags (eg. \"th\"). For example this can be used to specify "
            "required attributes for the header tags. "
            "Additionally this map can contain a value \"tagName\", by default this is \"th\". "
            "Another additional value is @em \"namePosition\", which indicates the position of the name "
            "of headers. This value is again a map, with these properties: @em \"type\": Can be "
            "@em \"contents\" (ie. use tag contents as name, the default) or @em \"attribute\" (ie. use "
            "a tag attribute value as name). If @em \"attribute\" is used for @em \"type\", the name of "
            "the attribute can be set as @em \"name\" property. Additionally a @em \"regexp\" property "
            "can be used to extract a string from the string that would otherwise be used as name "
            "as is.",
            "findTableHeaderPositions(${str}, ${options});", true, "object") );
    // Completion for helper.findFirstHtmlTag()
    m_completionObjects["helper"].insert( "call:findFirstHtmlTag()", CompletionItem(Function,
            "findFirstHtmlTag(string str, string tagName, object options)",
            "<b>Brief:</b> Finds the first occurrence of an HTML tag with <i>tagName</i> in <i>str</i>."
            "<br><b>Parameter <i>str</i>:</b> The string containing the HTML tag to be found. "
            "<br><b>Parameter <i>tagName</i>:</b> The name of the HTML tag to be found. "
            "<br><b>Parameter <i>options</i>:</b> The same as in findHtmlTags(), \"maxCount\" will be set to 1. "
            "<br><b>Returns:</b> A map with properties like in findHtmlTags(). Additionally these properties are "
            "returned: "
            "<br> <b>&bull;</b>  <b>found</b>: A boolean, true if the tag was found, false otherwise. "
            "<br><b>See also:</b>  findHtmlTags",
            "findFirstHtmlTag(${str}, ${tagName}, ${options});", true, "object") );
    // Completion for helper.findHtmlTags()
    m_completionObjects["helper"].insert( "call:findHtmlTags()", CompletionItem(Function,
            "findHtmlTags(string str, string tagName, object options)",
            "<b>Brief:</b> Finds all occurrences of HTML tags with <i>tagName</i> in <i>str</i>."
            "Using this function avoids having to deal with various problems when matching HTML elements: "
            "<br> <b>&bull;</b>  Nested HTML elements with the same <i>tagName</i>. When simply searching for the first "
            "closing tag after the found opening tag, a nested closing tag gets matched. If you are "
            "sure that there are no nested tags or if you want to only match until the first nested "
            "closing tag set the option \"noNesting\" in <i>options</i> to true. "
            "<br> <b>&bull;</b>  Matching tags with specific attributes. This function extracts all attributes of a "
            "matched tag. They can have values, which can be put in single/double/no quotation marks. "
            "To only match tags with specific attributes, add them to the \"attributes\" option in "
            "<i>options</i>. Regular expressions can be used to match the attribute name and value "
            "independently. Attribute order does not matter. "
            "<br> <b>&bull;</b>  Matching HTML tags correctly. For example a \">\" inside an attributes value could cause "
            "problems and have the tag cut off there. "
            "<br><b>Parameter <i>str</i>:</b> The string containing the HTML tags to be found. "
            "<br><b>Parameter <i>tagName</i>:</b> The name of the HTML tags to be found. "
            "<br><b>Parameter <i>options</i>:</b> A map with these properties: "
            "<br> <b>&bull;</b>  <b>attributes</b>: A map containing all required attributes and it's values. The keys of that "
            "map are the names of required attributes and can be regular expressions. The values "
            "are the values of the required attributes and are also handled as regular expressions. "
            "<br> <b>&bull;</b>  <b>contentsRegExp</b>: A regular expression pattern which the contents of found HTML tags "
            "must match. If it does not match, that tag does not get returned as found. "
            "If no parenthesized subexpressions are present in this regular expression, the whole "
            "matching string gets used as contents. If more than one parenthesized subexpressions "
            "are found, only the first one gets used. By default all content of the HTML tag "
            "gets matched. "
            "<br> <b>&bull;</b>  <b>position</b>: An integer, where to start the search for tags. This is 0 by default. "
            "<br> <b>&bull;</b>  <b>noContent</b>: A boolean, false by default. If true, HTML tags without any content are "
            "matched, eg. \"br\" or \"img\" tags. Otherwise tags need to be closed to get matched. "
            "<br> <b>&bull;</b>  <b>noNesting</b>: A boolean, false by default. If true, no checks will be made to ensure "
            "that the first found closing tag belongs to the opening tag. In this case the found "
            "contents always end after the first closing tag after the opening tag, no matter "
            "if the closing tag belongs to a nested tag or not. By setting this to true you can "
            "enhance performance. "
            "<br> <b>&bull;</b>  <b>maxCount</b>: The maximum number of HTML tags to match or 0 to match any number of HTML tags. "
            "<br> <b>&bull;</b>  <b>debug</b>: A boolean, false by default. If true, more debug output gets generated. "
            "<br><b>Returns:</b> A list of maps, each map represents one found tag and has these properties: "
            "<br> <b>&bull;</b>  <b>contents</b>: A string, the contents of the found tag (if found is true). "
            "<br> <b>&bull;</b>  <b>position</b>: An integer, the position of the found tag in <i>str</i> (if found is true). "
            "<br> <b>&bull;</b>  <b>endPosition</b>: An integer, the ending position of the found tag in <i>str</i> "
            "(if found is true). "
            "<br> <b>&bull;</b>  <b>attributes</b>: A map containing all found attributes of the tag and it's values (if "
            "found is true). The attribute names are the keys of the map, while the attribute "
            "values are the values of the map.",
            "findHtmlTags(${str}, ${tagName}, ${options});", true, "list") );
    // Completion for helper.findNamedHtmlTags()
    m_completionObjects["helper"].insert( "call:findNamedHtmlTags()", CompletionItem(Function,
            "findNamedHtmlTags(string str, string tagName, object options)",
            "<b>Brief:</b> Finds all occurrences of HTML tags with <i>tagName</i> in <i>str</i>."
            "This function uses findHtmlTags() to find the HTML tags and then extracts a name for each "
            "found tag from <i>str</i>. "
            "Instead of returning a list of all matched tags, a map is returned, with the found names as "
            "keys and the tag objects (as returned in a list by findHtmlTags()) as values. "
            "<br><b>Parameter <i>str</i>:</b> The string containing the HTML tag to be found. "
            "<br><b>Parameter <i>tagName</i>:</b> The name of the HTML tag to be found. "
            "<br><b>Parameter <i>options</i>:</b> The same as in findHtmlTags(), but <i>additionally</i> these options can be used: "
            "<br> <b>&bull;</b>  <b>namePosition</b>: A map with more options, indicating the position of the name of tags: "
            "<br> <b>&bull;</b>  <i>type</i>: Can be @em \"contents\" (ie. use tag contents as name, the default) or "
            "@em \"attribute\" (ie. use a tag attribute value as name). If @em \"attribute\" is used "
            "for @em \"type\", the name of the attribute can be set as @em \"name\" property. "
            "Additionally a @em \"regexp\" property can be used to extract a string from the string "
            "that would otherwise be used as name as is. "
            "<br> <b>&bull;</b>  <i>ambiguousNameResolution</i>: Can be used to tell what should be done if the same name "
            "was found multiple times. This can currently be one of: @em \"addNumber\" (adds a "
            "number to the name, ie. \"..1\", \"..2\")., @em \"replace\" (a later match with an already "
            "matched name overwrites the old match, the default). "
            "<br><b>Returns:</b> A map with the found names as keys and the tag objects as values. <i>Additionally</i> "
            "these properties are returned: "
            "<br> <b>&bull;</b>  <b>names</b>: A list of all found tag names. "
            "<br><b>See also:</b>  findHtmlTags",
            "findNamedHtmlTags(${str}, ${tagName}, ${options});", true, "object") );

    // Completion for network.requestStarted()
    m_completionObjects["network"].insert( "call:requestStarted()", CompletionItem(Function,
            "requestStarted(NetworkRequest request)",
            "<b>Brief:</b> Emitted when an asynchronous request has been started."
            "<br><b>Parameter <i>request</i>:</b> The request that has been started.",
            "requestStarted(${request});", true, "void") );
    // Completion for network.requestFinished()
    m_completionObjects["network"].insert( "call:requestFinished()", CompletionItem(Function,
            "requestFinished(NetworkRequest request)",
            "<b>Brief:</b> Emitted when an asynchronous request has finished."
            "<br><b>Parameter <i>request</i>:</b> The request that has finished.",
            "requestFinished(${request});", true, "void") );
    // Completion for network.allRequestsFinished()
    m_completionObjects["network"].insert( "call:allRequestsFinished()", CompletionItem(Function,
            "allRequestsFinished()",
            "<b>Brief:</b> Emitted when all requests are finished."
            "This signal gets emitted just after emitting requestFinished(), if there are no more running "
            "requests.",
            "allRequestsFinished();", true, "void") );
    // Completion for network.requestAborted()
    m_completionObjects["network"].insert( "call:requestAborted()", CompletionItem(Function,
            "requestAborted(NetworkRequest request)",
            "<b>Brief:</b> Emitted when an asynchronous request got aborted."
            "<br><b>Parameter <i>request</i>:</b> The request that was aborted.",
            "requestAborted(${request});", true, "void") );
    // Completion for network.abortAllRequests()
    m_completionObjects["network"].insert( "call:abortAllRequests()", CompletionItem(Function,
            "abortAllRequests()",
            "<b>Brief:</b> Aborts all running (asynchronous) downloads.",
            "abortAllRequests();", true, "void") );
    // Completion for network.slotRequestStarted()
    m_completionObjects["network"].insert( "call:slotRequestStarted()", CompletionItem(Function,
            "slotRequestStarted()",
            "<b>Brief:</b> ",
            "slotRequestStarted();", true, "void") );
    // Completion for network.slotRequestFinished()
    m_completionObjects["network"].insert( "call:slotRequestFinished()", CompletionItem(Function,
            "slotRequestFinished()",
            "<b>Brief:</b> ",
            "slotRequestFinished();", true, "void") );
    // Completion for network.slotRequestAborted()
    m_completionObjects["network"].insert( "call:slotRequestAborted()", CompletionItem(Function,
            "slotRequestAborted()",
            "<b>Brief:</b> ",
            "slotRequestAborted();", true, "void") );
    // Completion for network.lastDownloadAborted()
    m_completionObjects["network"].insert( "call:lastDownloadAborted()", CompletionItem(Function,
            "lastDownloadAborted()",
            "<b>Brief:</b> Returns true, if the last download was aborted before it was ready."
            "Use lastUrl() to get the URL of the aborted download. Downloads may be aborted eg. by "
            "closing plasma.",
            "lastDownloadAborted();", true, "bool") );
    // Completion for network.getSynchronous()
    m_completionObjects["network"].insert( "call:getSynchronous()", CompletionItem(Function,
            "getSynchronous(string url, int timeout)",
            "<b>Brief:</b> Download the document at <i>url</i> synchronously."
            "After the request is sent an QEventLoop gets started to wait for the reply to finish. "
            "If the <i>timeout</i> expires or the abort() slot gets called, the download gets stopped. "
            "<br><b>Parameter <i>url</i>:</b> The URL to download. "
            "<br><b>Parameter <i>timeout</i>:</b> Maximum time in milliseconds to wait for the reply to finish. If smaller than 0, "
            "no timeout gets used.",
            "getSynchronous(${url}, ${timeout});", true, "string") );
    // Completion for network.downloadSynchronous()
    m_completionObjects["network"].insert( "call:downloadSynchronous()", CompletionItem(Function,
            "downloadSynchronous(string url, int timeout)",
            "<b>Brief:</b> This is an alias for get().",
            "downloadSynchronous(${url}, ${timeout});", true, "void") );
    // Completion for network.createRequest()
    m_completionObjects["network"].insert( "call:createRequest()", CompletionItem(Function,
            "createRequest(string url)",
            "<b>Brief:</b> Creates a new NetworkRequest for asynchronous network access.",
            "createRequest(${url});", true, "NetworkRequest") );
    // Completion for network.get()
    m_completionObjects["network"].insert( "call:get()", CompletionItem(Function,
            "get(NetworkRequest request)",
            "<b>Brief:</b> Perform the network <i>request</i> asynchronously."
            "<br><b>Parameter <i>url</i>:</b> The URL to download.",
            "get(${request});", true, "void") );
    // Completion for network.post()
    m_completionObjects["network"].insert( "call:post()", CompletionItem(Function,
            "post(NetworkRequest request)",
            "<b>Brief:</b> Perform the network <i>request</i> asynchronously using POST method."
            "<br><b>Parameter <i>url</i>:</b> The URL to download.",
            "post(${request});", true, "void") );
    // Completion for network.head()
    m_completionObjects["network"].insert( "call:head()", CompletionItem(Function,
            "head(NetworkRequest request)",
            "<b>Brief:</b> Perform the network <i>request</i> asynchronously, but only get headers."
            "<br><b>Parameter <i>url</i>:</b> The URL to download.",
            "head(${request});", true, "void") );
    // Completion for network.download()
    m_completionObjects["network"].insert( "call:download()", CompletionItem(Function,
            "download(NetworkRequest request)",
            "<b>Brief:</b> This is an alias for get().",
            "download(${request});", true, "void") );

    // Completion for result.publish()
    m_completionObjects["result"].insert( "call:publish()", CompletionItem(Function,
            "publish()",
            "<b>Brief:</b> Can be called by scripts to trigger the data engine to publish collected data."
            "This does not need to be called by scripts, the data engine will publish all collected data, "
            "when the script returns and all network requests are finished. After the first ten items "
            "have been added, this signal is emitted automatically, if the AutoPublish feature is "
            "enabled (the default). Use <pre>enableFeature(AutoPublish, false)</pre> to "
            "disable this feature. "
            "If collecting data takes too long, calling this signal causes the data collected so far "
            "to be published immediately. Good reasons to call this signal are eg. because additional "
            "documents need to be downloaded or because a very big document gets parsed. Visualizations "
            "connected to the data engine will then receive data not completely at once, but step by "
            "step. "
            "It also means that the first data items are published to visualizations faster. A good idea "
            "could be to only call publish() after the first few data items (similar to the AutoPublish "
            "feature). That way visualizations get the first dataset very quickly, eg. the data that "
            "fits into the current view. Remaining data will then be added after the script is finished. "
            "<br><b>Note:</b>  Do not call publish() too often, because it causes some overhead. Visualizations "
            "will get notified about the updated data source and process it at whole, ie. not only "
            "newly published items but also the already published items again. Publishing data in "
            "groups of less than ten items will be too much in most situations. But if eg. another "
            "document needs to be downloaded to make more data available, it is a good idea to call "
            "publish() before starting the download (even with less than ten items). "
            "Use count() to see how many items are collected so far. "
            "<br><b>See also:</b>  Feature "
            "<br><b>See also:</b>  setFeatureEnabled "
            "<br><b>Since:</b>  0.10",
            "publish();", true, "void") );
    // Completion for result.clear()
    m_completionObjects["result"].insert( "call:clear()", CompletionItem(Function,
            "clear()",
            "<b>Brief:</b> Clears the list of stored TimetableData objects.",
            "clear();", true, "void") );
    // Completion for result.addData()
    m_completionObjects["result"].insert( "call:addData()", CompletionItem(Function,
            "addData(object map)",
            "<b>Brief:</b> Adds the data from <i>map</i>."
            "This can be used by scripts to add a timetable data object. "
            "<br><b>Code example:</b><br> "
            "result.addData({ DepartureDateTime: new Date(), Target: 'Test' });<br> "
            "<br> "
            "A predefined object can also be added like this: "
            "<br><b>Code example:</b><br> "
            "var departure = { DepartureDateTime: new Date() };<br> "
            "departure.Target = 'Test';<br> "
            "result.addData( departure);<br> "
            "<br> "
            "Keys of <i>map</i>, ie. properties of the script object are matched case insensitive. "
            "<br><b>Parameter <i>map</i>:</b> A map with all timetable informations as pairs of the information names and "
            "their values.",
            "addData(${map});", true, "void") );
    // Completion for result.hasData()
    m_completionObjects["result"].insert( "call:hasData()", CompletionItem(Function,
            "hasData()",
            "<b>Brief:</b> Checks whether or not the list of TimetableData objects is empty."
            "<br><b>Returns:</b> True, if the list of TimetableData objects isn't empty. False, otherwise.",
            "hasData();", true, "bool") );
    // Completion for result.count()
    m_completionObjects["result"].insert( "call:count()", CompletionItem(Function,
            "count()",
            "<b>Brief:</b> Returns the number of timetable elements currently in the resultset.",
            "count();", true, "int") );
    // Completion for result.isFeatureEnabled()
    m_completionObjects["result"].insert( "call:isFeatureEnabled()", CompletionItem(Function,
            "isFeatureEnabled(enum.feature feature)",
            "<b>Brief:</b> Whether or not <i>feature</i> is enabled."
            "Script examples: "
            "<br><b>Code example:</b><br> "
            "if ( result.isFeatureEnabled(features.AutoPublish) ) {<br> "
            "// Do something when the AutoPublish feature is enabled<br> "
            "}<br> "
            "<br> "
            "<br><b>Parameter <i>feature</i>:</b> The feature to check. Scripts can access the Feature enumeration "
            "as <b>accessor</b>. "
            "<br><b>See also:</b>  Feature "
            "<br><b>Since:</b>  0.10",
            "isFeatureEnabled(${feature});", true, "bool") );
    // Completion for result.enableFeature()
    m_completionObjects["result"].insert( "call:enableFeature()", CompletionItem(Function,
            "enableFeature(enum.feature feature, bool enable)",
            "<b>Brief:</b> Sets whether or not <i>feature</i> is <i>enabled</i>."
            "Script examples: "
            "<br><b>Code example:</b><br> "
            "// Disable the AutoPublish feature<br> "
            "result.enableFeature( accessor.AutoPublish, false );<br> "
            "<br> "
            "<br><b>Parameter <i>feature</i>:</b> The feature to enable/disable. Scripts can access the Feature enumeration "
            "as <b>accessor</b>. "
            "<br><b>Parameter <i>enable</i>:</b> True to enable <i>feature</i>, false to disable it. "
            "<br><b>See also:</b>  Feature "
            "<br><b>Since:</b>  0.10",
            "enableFeature(${feature}, ${enable});", true, "void") );
    // Completion for result.isHintGiven()
    m_completionObjects["result"].insert( "call:isHintGiven()", CompletionItem(Function,
            "isHintGiven(enum.hint hint)",
            "<b>Brief:</b> ",
            "isHintGiven(${hint});", true, "bool") );
    // Completion for result.giveHint()
    m_completionObjects["result"].insert( "call:giveHint()", CompletionItem(Function,
            "giveHint(enum.hint hint, bool enable)",
            "<b>Brief:</b> ",
            "giveHint(${hint}, ${enable});", true, "void") );

    // Completion for storage.write()
    m_completionObjects["storage"].insert( "call:write()", CompletionItem(Function,
            "write(string name, any data)",
            "<b>Brief:</b> Stores <i>data</i> in memory with <i>name</i>.",
            "write(${name}, ${data});", true, "void") );
    // Completion for storage.remove()
    m_completionObjects["storage"].insert( "call:remove()", CompletionItem(Function,
            "remove(string name)",
            "<b>Brief:</b> Removes data stored in memory with <i>name</i>.",
            "remove(${name});", true, "void") );
    // Completion for storage.clear()
    m_completionObjects["storage"].insert( "call:clear()", CompletionItem(Function,
            "clear()",
            "<b>Brief:</b> Clears all data stored in memory.",
            "clear();", true, "void") );
    // Completion for storage.writePersistent()
    m_completionObjects["storage"].insert( "call:writePersistent()", CompletionItem(Function,
            "writePersistent(string name, any data, uint lifetime)",
            "<b>Brief:</b> Stores <i>data</i> on disk with <i>name</i>."
            "<br><b>Parameter <i>name</i>:</b> A name to access the written data with. "
            "<br><b>Parameter <i>data</i>:</b> The data to write to disk. "
            "<br><b>Parameter <i>lifetime</i>:</b> The lifetime in days of the data. "
            "<br><b>See also:</b>  lifetime()",
            "writePersistent(${name}, ${data}, ${lifetime});", true, "void") );
    // Completion for storage.removePersistent()
    m_completionObjects["storage"].insert( "call:removePersistent()", CompletionItem(Function,
            "removePersistent(string name)",
            "<b>Brief:</b> Removes data stored on disk with <i>name</i>."
            "<br><b>Note:</b>  Scripts do not need to remove data written persistently, ie. to disk, because each "
            "data entry has a lifetime, which is currently limited to 30 days and defaults to 7 days.",
            "removePersistent(${name});", true, "void") );
    // Completion for storage.clearPersistent()
    m_completionObjects["storage"].insert( "call:clearPersistent()", CompletionItem(Function,
            "clearPersistent()",
            "<b>Brief:</b> Clears all data stored persistently, ie. on disk."
            "<br><b>Note:</b>  Scripts do not need to remove data written persistently, ie. to disk, because each "
            "data entry has a lifetime, which is currently limited to 30 days and defaults to 7 days.",
            "clearPersistent();", true, "void") );
    // Completion for storage.read()
    m_completionObjects["storage"].insert( "call:read()", CompletionItem(Function,
            "read()",
            "<b>Brief:</b> Reads all data stored in memory.",
            "read();", true, "object") );
    // Completion for storage.lifetime()
    m_completionObjects["storage"].insert( "call:lifetime()", CompletionItem(Function,
            "lifetime(string name)",
            "<b>Brief:</b> Reads the lifetime remaining for data written using writePersistent() with <i>name</i>.",
            "lifetime(${name});", true, "int") );
    // Completion for storage.readPersistent()
    m_completionObjects["storage"].insert( "call:readPersistent()", CompletionItem(Function,
            "readPersistent(string name, any defaultData)",
            "<b>Brief:</b> Reads data stored on disk with <i>name</i>."
            "<br><b>See also:</b>  lifetime()",
            "readPersistent(${name}, ${defaultData});", true, "any") );
}

void JavaScriptCompletionModel::initTimetableInfoCompletion() {
    m_completionsTimetableInfo.insert( "str:DepartureDateTime", CompletionItem( Const, "DepartureDateTime",
            i18nc("@info The description for the 'DepartureDateTime' info",
                  "The date and time of the departure. Can be a ECMAScript Date object. Use this "
                  "information instead of DepartureDate and DepartureTime if possible."),
            "DepartureDateTime",
            false, QString(), i18nc("@info/plain", "Needed for Departures/Journeys") ));
    m_completionsTimetableInfo.insert( "str:DepartureDate", CompletionItem( Const, "DepartureDate",
            i18nc("@info The description for the 'DepartureDate' info",
                  "The date of the departure."),
            "DepartureDate" ));
    m_completionsTimetableInfo.insert( "str:DepartureTime", CompletionItem( Const, "DepartureTime",
            i18nc("@info The description for the 'DepartureTime' info",
                  "The time of the departure."),
            "DepartureTime" ));
    m_completionsTimetableInfo.insert( "str:TypeOfVehicle", CompletionItem( Const, "TypeOfVehicle",
            i18nc("@info The description for the 'TypeOfVehicle' info", "The type of vehicle."),
            "TypeOfVehicle" ));
    m_completionsTimetableInfo.insert( "str:TransportLine", CompletionItem( Const, "TransportLine",
            i18nc("@info The description for the 'TransportLine' info", "The name of the "
                  "public transport line, e.g. '4', '6S', 'S 5', 'RB 24122.'"),
            "TransportLine",
            false, QString(), i18nc("@info/plain", "Needed for Departures") ));
    m_completionsTimetableInfo.insert( "str:FlightNumber", CompletionItem( Const, "FlightNumber",
            i18nc("@info The description for the 'FlightNumber' info",
                  "Same as TransportLine, used for flights."),
            "FlightNumber" ));
    m_completionsTimetableInfo.insert( "str:Target", CompletionItem( Const, "Target",
            i18nc("@info The description for the 'Target' info",
                  "The target of a journey / of a public transport line."),
            "Target",
            false, QString(), i18nc("@info/plain", "Needed for Departures") ));
    m_completionsTimetableInfo.insert( "str:Platform", CompletionItem( Const, "Platform",
            i18nc("@info The description for the 'Platform' info",
                  "The platform at which the vehicle departs/arrives."),
            "Platform" ));
    m_completionsTimetableInfo.insert( "str:Delay", CompletionItem( Const, "Delay",
            i18nc("@info The description for the 'Delay' info",
                  "The delay of a departure/arrival in minutes."),
            "Delay" ));
    m_completionsTimetableInfo.insert( "str:DelayReason", CompletionItem( Const, "DelayReason",
            i18nc("@info The description for the 'DelayReason' info",
                  "The reason of a delay."),
            "DelayReason" ));
    m_completionsTimetableInfo.insert( "str:JourneyNews", CompletionItem( Const, "JourneyNews",
            i18nc("@info The description for the 'JourneyNews' info",
                  "Can contain delay / delay reason / other news."),
            "JourneyNews" ));
    m_completionsTimetableInfo.insert( "str:JourneyNewsOther", CompletionItem( Const, "JourneyNewsOther",
            i18nc("@info The description for the 'JourneyNewsOther' info",
                  "Other news (not delay / delay reason)."),
            "JourneyNewsOther" ));
    m_completionsTimetableInfo.insert( "str:JourneyNewsLink", CompletionItem( Const, "JourneyNewsLink",
            i18nc("@info The description for the 'JourneyNewsLink' info",
                  "A link to an html page with journey news.<nl/>"
                  "<note>The url of the accessor is prepended, if a relative path has been "
                  "matched (starting with '/').</note>"),
            "JourneyNewsLink" ));
    m_completionsTimetableInfo.insert( "str:Operator", CompletionItem( Const, "Operator",
            i18nc("@info The description for the 'Operator' info",
                  "The company that is responsible for the journey."),
            "Operator" ));
    m_completionsTimetableInfo.insert( "str:Status", CompletionItem( Const, "Status",
            i18nc("@info The description for the 'Status' info", "The current status of "
                  "the departure / arrival. Currently only used for planes."),
            "Status" ));
    m_completionsTimetableInfo.insert( "str:IsNightLine", CompletionItem( Const, "IsNightLine",
            i18nc("@info The description for the 'IsNightLine' info", "A boolean indicating if "
				  "the transport line is a nightline or not."),
            "IsNightLine" ));
    m_completionsTimetableInfo.insert( "str:RouteStops", CompletionItem( Const, "RouteStops",
            i18nc("@info The description for the 'RouteStops' info",
                  "A list of stops of the departure/arrival to it's destination stop or "
                  "a list of stops of the journey from it's start to it's destination "
                  "stop.<nl/>If <emphasis>RouteStops</emphasis> and <emphasis>RouteTimes"
                  "</emphasis> are both set, they should contain the same number of "
                  "elements. And elements with equal indices should be associated (the "
                  "times at which the vehicle is at the stops).<nl/>"
                  "<note>For journeys <emphasis>RouteTimesDeparture</emphasis> and "
                  "<emphasis>RouteTimesArrival</emphasis> should be used instead of "
                  "<emphasis>RouteTimes</emphasis>.</note>"),
            "RouteStops" ));
    m_completionsTimetableInfo.insert( "str:RouteTimes", CompletionItem( Const, "RouteTimes",
            i18nc("@info The description for the 'RouteTimes' info",
                  "A list of times of the departure/arrival to it's destination stop.<nl/>"
                  "If <emphasis>RouteStops</emphasis> and <emphasis>RouteTimes</emphasis> "
                  "are both set, they should contain the same number of elements. And "
                  "elements with equal indices should be associated (the times at which "
                  "the vehicle is at the stops)."),
            "RouteTimes" ));

    // Journey information
    m_completionsTimetableInfo.insert( "str:RouteTimesDeparture",
	    CompletionItem( Const, "RouteTimesDeparture",
            i18nc("@info The description for the 'RouteTimesDeparture' info",
                  "A list of departure times of the journey.<nl/>If <emphasis>RouteStops"
                  "</emphasis> and <emphasis>RouteTimesDeparture</emphasis> are both set, "
                  "the latter should contain one element less (because the last stop has "
                  "no departure, only an arrival time). Elements with equal indices should "
                  "be associated (the times at which the vehicle departs from the stops)."),
            "RouteTimesDeparture" ));
    m_completionsTimetableInfo.insert( "str:RouteTimesArrival",
	    CompletionItem( Const, "RouteTimesArrival",
            i18nc("@info The description for the 'RouteTimesArrival' info",
                  "A list of arrival times of the journey.<nl/>If <emphasis>RouteStops"
                  "</emphasis> and <emphasis>RouteTimesArrival</emphasis> are both set, "
                  "the latter should contain one element less (because the first stop has "
                  "no arrival, only a departure time). Elements with equal indices should "
                  "be associated (the times at which the vehicle arrives at the stops)."),
            "RouteTimesArrival" ));
    m_completionsTimetableInfo.insert( "str:RouteExactStops", CompletionItem( Const, "RouteExactStops",
            i18nc("@info The description for the 'RouteExactStops' info",
                  "The number of exact route stops.<nl/>The route stop list in <emphasis>"
                  "RouteStops</emphasis> is not complete from the last exact route stop."),
            "RouteExactStops" ));
    m_completionsTimetableInfo.insert( "str:RouteTypesOfVehicles",
	    CompletionItem( Const, "RouteTypesOfVehicles",
            i18nc("@info The description for the 'RouteTypesOfVehicles' info",
                  "The types of vehicles used for each 'sub-journey' of a journey."),
            "RouteTypesOfVehicles" ));
    m_completionsTimetableInfo.insert( "str:RouteTransportLines",
	    CompletionItem( Const, "RouteTransportLines",
            i18nc("@info The description for the 'RouteTransportLines' info",
                  "The transport lines used for each 'sub-journey' of a journey."),
            "RouteTransportLines" ));
    m_completionsTimetableInfo.insert( "str:RoutePlatformsDeparture",
	    CompletionItem( Const, "RoutePlatformsDeparture",
            i18nc("@info The description for the 'RoutePlatformsDeparture' info",
                  "The platforms of departures used for each 'sub-journey' of a journey.<nl/>"
                  "If <emphasis>RouteStops</emphasis> and <emphasis>RoutePlatformsDeparture"
                  "</emphasis> are both set, the latter should contain one element less (because "
                  "the last stop has no departure, only an arrival platform). Elements with "
                  "equal indices should be associated (the platforms from which the vehicle "
                  "departs from the stops)."),
            "RoutePlatformsDeparture" ));
    m_completionsTimetableInfo.insert( "str:RoutePlatformsArrival",
	    CompletionItem( Const, "RoutePlatformsArrival",
            i18nc("@info The description for the 'RoutePlatformsArrival' info",
                  "The platforms of arrivals used for each 'sub-journey' of a journey.<nl/>"
                  "If <emphasis>RouteStops</emphasis> and <emphasis>RoutePlatformsArrival"
                  "</emphasis> are both set, the latter should contain one element less "
                  "(because the first stop has no arrival, only a departure platform). "
                  "Elements with equal indices should be associated (the platforms at which "
                  "the vehicle arrives at the stops)"),
            "RoutePlatformsArrival" ));
    m_completionsTimetableInfo.insert( "str:RouteTimesDepartureDelay",
	    CompletionItem( Const, "RouteTimesDepartureDelay",
            i18nc("@info The description for the 'RouteTimesDepartureDelay' info",
                  "A list of delays in minutes for each departure time of a route "
                  "(see <emphasis>RouteTimesDeparture</emphasis>).<nl/>If set it should contain "
                  "the same number of elements as 'RouteTimesDeparture'."),
            "RouteTimesDepartureDelay" ));
    m_completionsTimetableInfo.insert( "str:RouteTimesArrivalDelay",
	    CompletionItem( Const, "RouteTimesArrivalDelay",
            i18nc("@info The description for the 'RouteTimesArrivalDelay' info",
                  "A list of delays in minutes for each arrival time of a route "
                  "(see <emphasis>RouteTimesArrival</emphasis>).<nl/>If set it should contain "
                  "the same number of elements as 'RouteTimesArrival'."),
            "RouteTimesArrivalDelay" ));
    m_completionsTimetableInfo.insert( "str:Duration", CompletionItem( Const, "Duration",
            i18nc("@info The description for the 'Duration' info",
                  "The duration of a journey in minutes."),
            "Duration" ));
    m_completionsTimetableInfo.insert( "str:StartStopName", CompletionItem( Const, "StartStopName",
            i18nc("@info The description for the 'StartStopName' info",
                  "The name of the starting stop of a journey."),
            "StartStopName",
            false, QString(), i18nc("@info/plain", "Needed for Journeys") ));
    m_completionsTimetableInfo.insert( "str:StartStopID", CompletionItem( Const, "StartStopID",
            i18nc("@info The description for the 'StartStopID' info",
                  "The ID of the starting stop of a journey."),
            "StartStopID" ));
    m_completionsTimetableInfo.insert( "str:TargetStopName", CompletionItem( Const, "TargetStopName",
            i18nc("@info The description for the 'TargetStopName' info",
                  "The name of the target stop of a journey."),
            "TargetStopName", false, QString(), i18nc("@info/plain", "Needed for Journeys") ));
    m_completionsTimetableInfo.insert( "str:TargetStopID", CompletionItem( Const, "TargetStopID",
            i18nc("@info The description for the 'TargetStopID' info",
                  "The ID of the target stop of a journey."),
            "TargetStopID" ));
    m_completionsTimetableInfo.insert( "str:ArrivalDateTime", CompletionItem( Const, "ArrivalDateTime",
            i18nc("@info The description for the 'ArrivalDateTime' info",
                  "The date and time of the arrival. Can be a ECMAScript Date object. Use this "
                  "information instead of ArrivalDate and ArrivalTime if possible."),
            "ArrivalDateTime" ));
    m_completionsTimetableInfo.insert( "str:ArrivalDate", CompletionItem( Const, "ArrivalDate",
            i18nc("@info The description for the 'ArrivalDate' info",
                  "The date of the arrival."),
            "ArrivalDate" ));
    m_completionsTimetableInfo.insert( "str:ArrivalTime", CompletionItem( Const, "ArrivalTime",
            i18nc("@info The description for the 'ArrivalTime' info",
                  "The time of the arrival time."),
            "ArrivalTime", false, QString(),
            i18nc("@info/plain", "This or ArrivalDateTime is needed for journeys") ));
    m_completionsTimetableInfo.insert( "str:Changes", CompletionItem( Const, "Changes",
            i18nc("@info The description for the 'Changes' info",
                  "The number of changes between different vehicles in a journey."),
            "Changes" ));
    m_completionsTimetableInfo.insert( "str:TypesOfVehicleInJourney",
	    CompletionItem( Const, "TypesOfVehicleInJourney",
            i18nc("@info The description for the 'TypesOfVehicleInJourney' info",
                  "A list of vehicle types used in a journey."),
            "TypesOfVehicleInJourney" ));
    m_completionsTimetableInfo.insert( "str:Pricing", CompletionItem( Const, "Pricing",
            i18nc("@info The description for the 'Pricing' info",
                  "Information about the pricing of a journey."),
            "Pricing" ));

    // Stop suggestion information
    m_completionsTimetableInfo.insert( "str:StopName", CompletionItem( Const, "StopName",
            i18nc("@info The description for the 'StopName' info",
                  "The name of a stop/station."),
            "StopName", false, QString(), i18nc("@info/plain", "Needed for Stop Suggestions") ));
    m_completionsTimetableInfo.insert( "str:StopID", CompletionItem( Const, "StopID",
            i18nc("@info The description for the 'StopID' info",
                  "The ID of a stop/station."),
            "StopID" ));
    m_completionsTimetableInfo.insert( "str:StopWeight", CompletionItem( Const, "StopWeight",
            i18nc("@info The description for the 'StopWeight' info",
                  "The weight of a stop suggestion."),
            "StopWeight" ));
    // TODO: range? Explain: passed through to eg. KCompletion
}
