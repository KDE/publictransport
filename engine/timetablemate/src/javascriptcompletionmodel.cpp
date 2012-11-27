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
#include "javascriptcompletiongeneric.h"

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
    JavaScriptCompletionGeneric::addCompletions( &m_completionObjects );
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
        } else {
            kDebug() << "No template interface";
        }
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
        kDebug() << "More closing '}' found than opening '{' at line" << range.start().line();
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
        if ( m_completionObjects.contains(obj) && m_completionObjects[obj].contains(func) ) {
            return m_completionObjects[obj][func];
        }
    }

    return item;
}

void JavaScriptCompletionModel::initGlobalFunctionCompletion() {
    m_completionsGlobalFunctions.insert( "func:features()",
            CompletionItem( Function | GlobalScope,
            "features()",
            i18nc("@info The description for the 'features' function",
                  "Should be implemented to tell which features the script supports.<nl/>"
                  "This function is called by the data engine."),
            "\n// This function returns a list of all features supported by this script.\n"
            "function features() {\n"
            "\t// These strings are currently recognized as features:\n" // TODO PublicTransport.ProvidesDelay, ...
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
            "getTimetable( values )",
            i18nc("@info The description for the 'getTimetable' function",
                  "Requests and parses departure/arrival documents. The argument has the "
                  "following properties: stop, dateTime (Date object), count, dataType "
                  "('departures' or 'arrivals'), city.<nl/>"
                  "This function is called by the data engine. Found departures/arrivals can be "
                  "handed over to the data engine like this:<nl/>"
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
            "\n// This function normally requests a document (eg. HTML or XML) and then parses "
            "it for departure/arrival data.\n"
            "function getTimetable( values ) {\n" // TODO Template code fore requesting a document
            "\t// Find block of departures\n"
            "\t// TODO: Adjust so that you get the block that contains\n"
            "\t// the departures in the document\n"
            "\tvar str = helper.extractBlock( html, '<table ${departure_table}>', '</table>' );\n\n"
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
            "getJourneys( values )",
            i18nc("@info The description for the 'getJourneys' function",
                  "Requests and parses journey documents. The argument has the "
                  "following properties: originStop, targetStop, dateTime (Date object), "
                  "count, dataType, city.<nl/>"
                  "This function is called by the data engine. Found journeys can "
                  "be handed over to the data engine like this:<nl/>"
                  "<icode>  // Add timetable data to the result set</icode><nl/>"
                  "<icode>  result.addData( {StartStopName: 'A',</icode><nl/>"
                  "<icode>                   TargetStopName: 'B',</icode><nl/>"
                  "<icode>                   Target: 'Samplestreet',</icode><nl/>"
                  "<icode>                   DepartureDateTime: new Date(),</icode><nl/>"
                  "<icode>                   ArrivalDateTime: new Date(),</icode><nl/>"
                  "<icode>                   Changes: 3,</icode><nl/>"
                  "<icode>                   Pricing: '2,30 €'} );</icode><nl/><nl/>"),
            "\n// This function normally requests a document (eg. HTML or XML) and then parses it "
            "for journey data.\n"
            "function getJourneys( values ) {\n"
            "\t${cursor}\n"
            "}\n",
            true, "Implement void", "                   " )); // The spaces make the completion
    // box wider, so that the code snipped can be read

    m_completionsGlobalFunctions.insert( "func:getStopSuggestions()",
            CompletionItem( Function | GlobalScope,
            "getStopSuggestions( values )",
            i18nc("@info The description for the 'getStopSuggestions' function",
                  "Requests and parses stop suggestion documents. The argument has the "
                  "following properties: stop, count, city.<nl/>"
                  "This function is called by the data engine. The parameter "
                  "contains the contents of the document body. Found stop data "
                  "can be handed over to the data engine like this:<nl/>"
                  "<icode>  // Add timetable data to the result set</icode><nl/>"
                  "<icode>  result.addData( {StopName: 'TestName', StopID: 100} );</icode>"),
            "\n// This function normally requests a document (eg. HTML or XML) and then parses it "
            "for stop suggestions.\n"
            "function getStopSuggestions( values ) {\n"
            "\t${cursor}\n"
            "}\n",
            true, "Implement void", "                   " )); // The spaces make the completion
    // box wider, so that the code snipped can be read
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
                  "<note>The url of the service provider is prepended, if a relative path has been "
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
