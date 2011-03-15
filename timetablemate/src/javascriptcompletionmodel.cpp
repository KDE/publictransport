/*
*   Copyright 2010 Friedrich Pülz <fieti1983@gmx.de>
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
    initHelperCompletion();
    initFunctionCallCompletion();
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
    kDebug() << "COMPLETION" << word << row;
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

        kDebug() << "COMPLETION WORD" << word;
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

        if ( word.isEmpty() )
            text = textUntilWhiteSpace;
        else
            text = textUntilWhiteSpace + word;

        if ( blockLevel == 0 ) { // at root level
            m_completions << m_completionsGlobalFunctions.values();
        } else { // inside function
            if ( text.startsWith("helper.") ) {
                m_completions << m_completionsHelper.values();
            } else if ( text.startsWith("timetableData.set( '")
                        || textUntilLineBegin.startsWith("timetableData.set( '") ) {
                // Add timetable info completions
                m_completions << m_completionsTimetableInfo.values();
            } else if ( text.startsWith("timetableData.") ) {
                m_completions << m_completionsCalls["call:timetableData.set"];
                m_completions << m_completionsCalls["call:timetableData.clear"];
            } else if ( text.startsWith("result.") ) {
                m_completions << m_completionsCalls["call:result.addData"];
            } else {
                m_completions << CompletionItem( Class | GlobalScope, "helper",
			i18nc("@info The description for the 'helper' object",
			    "The <emphasis>helper</emphasis> object contains some "
			    "useful functions."),
			"helper.", false, "object" );
                m_completions << CompletionItem( Class | GlobalScope, "timetableData",
			i18nc("@info The description for the 'timetableData' object",
			    "The <emphasis>timetableData</emphasis> object is used to "
			    "put parsed timetable data into it.<nl/>"
			    "<note>Once all data is stored inside <emphasis>"
			    "timetableData</emphasis> you can call <emphasis>result" ".addData()</emphasis>.</note>"),
			"timetableData.", false, "object" );
                m_completions << CompletionItem(  Class | GlobalScope, "result",
			i18nc("@info The description for the 'result' object",
			    "The result object is used to store all parsed "
			    "departure/arrival/journey items. Call <emphasis>"
			    "result.addData( timetableData )</emphasis> to add the "
			    "current item to the result set."),
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
    if ( item.isValid() )
	return item;

    item = m_completionsTimetableInfo.value( id );
    if ( item.isValid() )
	return item;

    item = m_completionsCalls.value( id );
    if ( item.isValid() )
	return item;

    QString simpleID = id;
    QRegExp rxBraces( "\\([^\\)]*\\)" );
    rxBraces.setMinimal( true );
    simpleID.replace( rxBraces, "()" );
    item = m_completionsHelper.value( simpleID );

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
    
    m_completionsGlobalFunctions.insert( "func:parseTimetable(html)",
	    CompletionItem( Function | GlobalScope,
            "parseTimetable( html )",
            i18nc("@info The description for the 'parseTimetable' function",
                  "Parses departure/arrival documents.<nl/>"
                  "This function is called by the data engine. The parameter contains the "
                  "contents of the document body. Found departures/arrivals can be handed "
                  "over to the data engine like this:<nl/>" //<bcode>"
                  "<icode>  // First clear the old data</icode><nl/>"
                  "<icode>  timetableData.clear();</icode><nl/>"
                  "<icode>  // Then set all read values</icode><nl/>"
                  "<icode>  timetableData.set( 'TransportLine', '603' );</icode><nl/>"
                  "<icode>  timetableData.set( 'TypeOfVehicle', 'bus' );</icode><nl/>"
                  "<icode>  timetableData.set( 'Target', 'Samplestreet' );</icode><nl/>"
                  "<icode>  timetableData.set( 'DepartureHour', 10 );</icode><nl/>"
                  "<icode>  timetableData.set( 'DepartureMinute', 23 );</icode><nl/>"
                  "<icode>  timetableData.set( 'Delay', 4 );</icode><nl/>"
                  "<icode>  // Add timetable data to the result set</icode><nl/>"
                  "<icode>  result.addData( timetableData );</icode><nl/><nl/>"
                  "<note>You <emphasis>can</emphasis> return a string array with keywords "
                  "that affect all departures/arrivals. Currently only one such keyword is "
                  "supported: <emphasis>'no delays'</emphasis>, used to indicate that "
                  "there is no delay information for the given stop. The data engine can "
                  "then use a higher timeout for the next data update. When delay "
                  "information is available updates are done more often, because delays "
                  "may change.</note>"),
// 				      "</bcode>"),
            "\n// This function parses a given HTML document for departure/arrival data.\n"
            "function parseTimetable( html ) {\n"
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
            "\t\ttimetableData.clear();\n"
            "\t\ttimetableData.set( 'TransportLine', 'Sample line 4' );\n"
            "\t\ttimetableData.set( 'TypeOfVehicle', 'bus' );\n"
            "\t\ttimetableData.set( 'Target', 'Sample target' );\n"
            "\t\ttimetableData.set( 'DepartureHour', 10 );\n"
            "\t\ttimetableData.set( 'DepartureMinute', 15 );\n"
            "\t\tresult.addData( timetableData );\n"
            "\t}\n"
            "}\n",
            true, "Implement string array", "                   " )); // The spaces make the completion
    // box wider, so that the code snipped can be read

    m_completionsGlobalFunctions.insert( "func:parseJourneys(html)",
	    CompletionItem( Function | GlobalScope,
            "parseJourneys( html )",
            i18nc("@info The description for the 'parseJourneys' function",
                  "Parses journey documents.<nl/>"
                  "This function is called by the data engine. The parameter "
                  "contains the contents of the document body. Found journeys can "
                  "be handed over to the data engine like this:<nl/>" //<bcode>"
                  "<icode>  // First clear the old data</icode><nl/>"
                  "<icode>  timetableData.clear();</icode><nl/>"
                  "<icode>  // Then set all read values</icode><nl/>"
                  "<icode>  timetableData.set( 'StartStopName', 'A' );</icode><nl/>"
                  "<icode>  timetableData.set( 'TargetStopName', 'B' );</icode><nl/>"
                  "<icode>  timetableData.set( 'DepartureHour', 10 );</icode><nl/>"
                  "<icode>  timetableData.set( 'DepartureMinute', 23 );</icode><nl/>"
                  "<icode>  timetableData.set( 'ArrivalHour', 11 );</icode><nl/>"
                  "<icode>  timetableData.set( 'ArrivalMinute', 05 );</icode><nl/>"
                  "<icode>  timetableData.set( 'Changes', 3 );</icode><nl/>"
                  "<icode>  timetableData.set( 'Pricing', '2,30 €' );</icode><nl/>"
                  "<icode>  // Add timetable data to the result set</icode><nl/>"
                  "<icode>  result.addData( timetableData );</icode>"),
// 				      "</bcode>"),
            "\n// This function parses a given HTML document for journey data.\n"
	    "function parseJourneys( html ) {\n"
	    "\t${cursor}\n"
	    "}\n",
            true, "Implement void", "                   " )); // The spaces make the completion
    // box wider, so that the code snipped can be read

    m_completionsGlobalFunctions.insert( "func:parsePossibleStops(html)",
	    CompletionItem( Function | GlobalScope,
            "parsePossibleStops( html )",
            i18nc("@info The description for the 'parsePossibleStops' function",
                  "Parses stop suggestion documents.<nl/>"
                  "This function is called by the data engine. The parameter "
                  "contains the contents of the document body. Found stop data "
                  "can be handed over to the data engine like this:<nl/>" //<bcode>"
                  "<icode>  // First clear the old data</icode><nl/>"
                  "<icode>  timetableData.clear();</icode><nl/>"
                  "<icode>  // Then set all read values</icode><nl/>"
                  "<icode>  timetableData.set( 'StopName', 'Bremen Hbf' );</icode><nl/>"
                  "<icode>  // Add timetable data to the result set</icode><nl/>"
                  "<icode>  result.addData( timetableData );</icode>"),
// 				      "</bcode>"),
            "\n// This function parses a given HTML document for stop suggestions.\n"
	    "function parsePossibleStops( html ) {\n"
	    "\t${cursor}\n"
	    "}\n",
            true, "Implement void", "                   " )); // The spaces make the completion
    // box wider, so that the code snipped can be read

    m_completionsGlobalFunctions.insert( "func:getUrlForLaterJourneyResults(html)",
	    CompletionItem( Function | GlobalScope,
            "getUrlForLaterJourneyResults( html )",
            i18nc("@info The description for the 'getUrlForLaterJourneyResults' function",
                  "Parses a journey document for a link to a journey "
                  "document containing later journeys.<nl/>"
                  "This function is called by the data engine. The parameter "
                  "contains the contents of the document body. The found link "
                  "can be simply returned. If no link could be found, return null."),
            "\n// This function parses a given HTML document for a link to later journeys.\n"
	    "function getUrlForLaterJourneyResults( html ) {\n"
	    "\treturn ${cursor};\n"
	    "}\n",
            true, "Implement string", "                   " )); // The spaces make the completion
    // box wider, so that the code snipped can be read

    m_completionsGlobalFunctions.insert( "func:getUrlForDetailedJourneyResults(html)",
	    CompletionItem( Function | GlobalScope,
            "getUrlForDetailedJourneyResults( html )",
            i18nc("@info The description for the 'getUrlForDetailedJourneyResults' function",
                  "Parses a journey document for a link to another journey "
                  "document containing more details about journeys.<nl/>"
                  "This function is called by the data engine. The parameter "
                  "contains the contents of the document body. "
                  "The found link can be simply returned. If no link could be found, return null."),
            "\n// This function parses a given HTML document\n"
            "// for a link to a more detailed journey document.\n"
	    "function getUrlForLaterJourneyResults( html ) {\n"
	    "\treturn ${cursor};\n"
	    "}\n",
            true, "Implement string", "                   " )); // The spaces make the completion
    // box wider, so that the code snipped can be read
}

void JavaScriptCompletionModel::initHelperCompletion() {
    m_completionsHelper.insert( "call:extractBlock()", CompletionItem( Function,
	    "extractBlock( string, string begin, string end )",
	    i18nc("@info The description for the 'extractBlock' function",
		    "Extracts the first block in the given string, that begins with "
		    "<placeholder>begin</placeholder> and ends with <placeholder>end"
		    "</placeholder>. Returns the found block or an empty string if the "
		    "block could not be found."),
	    "extractBlock( ${string}, ${begin}, ${end} );", true, "string" ));
    m_completionsHelper.insert( "call:stripTags()", CompletionItem( Function, "stripTags( string )",
	    i18nc("@info The description for the 'stripTags' function",
		    "Strips all tags from a given string and returns the result."),
	    "stripTags( ${string} );", true, "string" ));
    m_completionsHelper.insert( "call:trim()", CompletionItem( Function, "trim( string )",
	    i18nc("@info The description for the 'trim' function",
		    "Trims a string and returns the result."),
	    "trim( ${string} );", true, "string" ));
    m_completionsHelper.insert( "call:matchTime()", CompletionItem( Function,
	    "matchTime( string time, string format = 'hh:mm' )",
	    i18nc("@info The description for the 'matchTime' function",
		    "Searches for a time with the given <emphasis>format</emphasis> in the "
		    "given <emphasis>time</emphasis> string. Returns an integer array with "
		    "two integers: The first one is the hour part, the second one the "
		    "minute part."),
	    "matchTime( ${timeString} );", true, "int array" ));
    m_completionsHelper.insert( "call:matchDate()", CompletionItem( Function,
	    "matchDate( string date, string format = 'yyyy-MM-dd' )",
	    i18nc("@info The description for the 'matchDate' function",
		    "Searches for a date with the given <emphasis>format</emphasis> in the "
		    "given <emphasis>date</emphasis> string. Returns an integer array with "
		    "three integers: The first one is the year part, the second one the "
		    "month part and the third one the day part."),
	    "matchDate( ${dateString} );", true, "int array" ));
    m_completionsHelper.insert( "call:formatTime()", CompletionItem( Function,
	    "formatTime( int hour, int minute, string format = 'hh:mm' )",
	    i18nc("@info The description for the 'formatTime' function",
		    "Formats a time given by it's <emphasis>hour</emphasis> and <emphasis>"
		    "minute</emphasis> using the given <emphasis>format</emphasis>."),
	    "formatTime( ${hour}, ${minute} );", true, "string" ));
    m_completionsHelper.insert( "call:duration()", CompletionItem( Function,
	    "duration( string time1, string time2, string format = 'hh:mm' )",
	    i18nc("@info The description for the 'duration' function",
		    "Computes the duration in minutes between the two times, given as strings. "
		    "The time strings are parsed using the given <emphasis>format</emphasis>."),
	    "duration( ${timeString1}, ${timeString2} );", true, "int" ));
    m_completionsHelper.insert( "call:addMinsToTime()", CompletionItem( Function,
	    "addMinsToTime( string time, int minsToAdd, string format = 'hh:mm' )",
	    i18nc("@info The description for the 'addMinsToTime' function",
		    "Adds <emphasis>minsToAdd</emphasis> minutes to the <emphasis>time"
		    "</emphasis> given as a string. The time string is parsed using the "
		    "given <emphasis>format</emphasis>. Returns a time string formatted "
		    "using the given <emphasis>format</emphasis>"),
	    "addMinsToTime( ${timeString}, ${minsToAdd} );", true, "string" ));
    m_completionsHelper.insert( "call:addDaysToDate()", CompletionItem( Function,
	    "addDaysToDate( int[] dateArray, int daysToAdd )",
	    i18nc("@info The description for the 'addDaysToDate' function",
		    "Adds <emphasis>daysToAdd</emphasis> days to the <emphasis>date"
		    "</emphasis> given as an integer array (with three integers: year, month, day). "
		    "Returns an integer array with the new values"),
	    "addDaysToDate( ${dateArray [year, month, day]}, ${daysToAdd} );", true, "string" ));
    m_completionsHelper.insert( "call:splitSkipEmptyParts()", CompletionItem( Function,
	    "splitSkipEmptyParts( string, string separator )",
	    i18nc("@info The description for the 'splitSkipEmptyParts' function",
		    "Splits the given <emphasis>string</emphasis> using the given "
		    "<emphasis>separator</emphasis>. Returns an array of strings."),
	    "splitSkipEmptyParts( ${string}, ${separator} );", true, "string array" ));
    m_completionsHelper.insert( "call:error()", CompletionItem( Function,
	    "error( string message, string data )",
	    i18nc("@info The description for the 'error' function",
		    "Logs the error message with the given data string, eg. the HTML code where parsing "
			"failed. The message gets also send to stdout with a short version of the data."),
	    "error( ${message}, ${data} );", true, "void" ));
}

void JavaScriptCompletionModel::initTimetableInfoCompletion() {
    m_completionsTimetableInfo.insert( "str:DepartureHour", CompletionItem( Const, "DepartureHour",
            i18nc("@info The description for the 'DepartureHour' info",
                  "The hour of the departure time."),
            "DepartureHour",
            false, QString(), i18nc("@info/plain", "Needed for Departures/Journeys") ));
    m_completionsTimetableInfo.insert( "str:DepartureMinute", CompletionItem( Const, "DepartureMinute",
            i18nc("@info The description for the 'DepartureMinute' info",
                  "The minute of the departure time."),
            "DepartureMinute",
            false, QString(), i18nc("@info/plain", "Needed for Departures/Journeys") ));
    m_completionsTimetableInfo.insert( "str:DepartureDate", CompletionItem( Const, "DepartureDate",
            i18nc("@info The description for the 'DepartureDate' info",
                  "The date of the departure."),
            "DepartureDate" ));
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
    m_completionsTimetableInfo.insert( "str:DepartureHourPrognosis",
	    CompletionItem( Const, "DepartureHourPrognosis",
            i18nc("@info The description for the 'DepartureHourPrognosis' info",
                  "The prognosis for the departure hour, which is the departure hour "
                  "plus the delay."),
            "DepartureHourPrognosis" ));
    m_completionsTimetableInfo.insert( "str:DepartureMinutePrognosis",
	    CompletionItem( Const, "DepartureMinutePrognosis",
            i18nc("@info The description for the 'DepartureMinutePrognosis' info",
                  "The prognosis for the departure minute, which is the departure minute "
                  "plus the delay."),
            "DepartureMinutePrognosis" ));
    m_completionsTimetableInfo.insert( "str:Operator", CompletionItem( Const, "Operator",
            i18nc("@info The description for the 'Operator' info",
                  "The company that is responsible for the journey."),
            "Operator" ));
    m_completionsTimetableInfo.insert( "str:DepartureAMorPM", CompletionItem( Const, "DepartureAMorPM",
            i18nc("@info The description for the 'DepartureAMorPM' info", "'am' or 'pm' "
                  "for the departure time.<nl/>"
                  "<note>If not set, 24 hour format is assumed.</note>"),
            "DepartureAMorPM" ));
    m_completionsTimetableInfo.insert( "str:DepartureAMorPMPrognosis",
	    CompletionItem( Const, "DepartureAMorPMPrognosis",
            i18nc("@info The description for the 'DepartureAMorPMPrognosis' info",
                  "'am' or 'pm' for the prognosis departure time.<nl/>"
                  "<note>If not set, 24 hour format is assumed.</note>"),
            "DepartureAMorPMPrognosis" ));
    m_completionsTimetableInfo.insert( "str:ArrivalAMorPM", CompletionItem( Const, "ArrivalAMorPM",
            i18nc("@info The description for the 'ArrivalAMorPM' info",
                  "'am' or 'pm' for the arrival time.<nl/>"
                  "<note>If not set, 24 hour format is assumed.</note>"),
            "ArrivalAMorPM" ));
    m_completionsTimetableInfo.insert( "str:Status", CompletionItem( Const, "Status",
            i18nc("@info The description for the 'Status' info", "The current status of "
                  "the departure / arrival. Currently only used for planes."),
            "Status" ));
    m_completionsTimetableInfo.insert( "str:DepartureYear", CompletionItem( Const, "DepartureYear",
            i18nc("@info The description for the 'DepartureYear' info", "The year of the "
                  "departure, to be used when the year is separated from the date."),
            "DepartureYear" ));
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
                  "<emphasis>RouteTimesArrival</emphasis> should be used instead of " "<emphasis>RouteTimes</emphasis>.</note>"),
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
    m_completionsTimetableInfo.insert( "str:ArrivalDate", CompletionItem( Const, "ArrivalDate",
            i18nc("@info The description for the 'ArrivalDate' info",
                  "The date of the arrival."),
            "ArrivalDate" ));
    m_completionsTimetableInfo.insert( "str:ArrivalHour", CompletionItem( Const, "ArrivalHour",
            i18nc("@info The description for the 'ArrivalHour' info",
                  "The hour of the arrival time."),
            "ArrivalHour", false, QString(), i18nc("@info/plain", "Needed for Journeys") ));
    m_completionsTimetableInfo.insert( "str:ArrivalMinute", CompletionItem( Const, "ArrivalMinute",
            i18nc("@info The description for the 'ArrivalMinute' info",
                  "The minute of the arrival time."),
            "ArrivalMinute", false, QString(), i18nc("@info/plain", "Needed for Journeys") ));
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

void JavaScriptCompletionModel::initFunctionCallCompletion() {
    m_completionsCalls.insert( "call:timetableData.set",
	    CompletionItem( Function, "timetableData.set( string infoName, variant value )",
			    i18nc("@info The description for the 'timetableData.set' function",
				  "Saves the given <placeholder>value</placeholder> under the "
				  "given <placeholder>infoName</placeholder>."),
			    "set( '${infoName}', ${value} );", true, "void" ) );    
    m_completionsCalls.insert( "call:timetableData.clear",
	    CompletionItem( Function, "timetableData.clear()",
			    i18nc("@info The description for the 'timetableData.clear' function",
				  "Clears the current values of the "
				  "<emphasis>timetableData</emphasis> object.<nl/>"
				  "<note>You should call this method before setting values "
				  "for the next item.</note>"),
			    "clear();", false, "void" ) );
    
    m_completionsCalls.insert( "call:result.addData",
	    CompletionItem( Function, "result.addData( timetableData )",
			    i18nc("@info The description for the 'result.addData' function",
				  "Adds the current <emphasis>timetableData</emphasis> object "
				  "to the result set."),
			    "addData( timetableData );", false, "void" ));
}
