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

/**
 * @projectDescription Contains the DataTypeProcessor used for route data.
 *
 * @author   Friedrich Pülz fpuelz@gmx.de
 * @version  0.1
 **/

/** Object for route data. */
var __hafas_routedata = function(hafas) {
    var processor = new HafasPrivate.DataTypeProcessor({
        /** Options for functions in Hafas.routeData, overwrite global Hafas.options. */
        options: {
            format: Hafas.XmlFormat
        },

        parser: new HafasPrivate.Parser({
            /**
            * Parse route document in Hafas.formats.XmlFormat.
            *
            * This function does not access the "result" object. It should only be used to get
            * train info URLs for departures.
            * This function uses a very small data source, ie. it is faster and downloading takes
            * less time. The request URL must include "&L=vs_java3", but is not supported by every
            * provider. If no XML document can be retrieved, you can implement the function
            * Hafas.routeData.parser.parseText().
            * @param {String} html Contents of a train info document in XML format received from
            *   the HAFAS provider.
            * @return {Object} An object with these properties: RouteStops, RouteTimes.
            **/
            parseXml: function( xml, values ) {
                print( "parseXml" );
                if ( !HafasPrivate.expectFormat(Hafas.XmlFormat, xml) ) {
                    if ( HafasPrivate.isHtml(xml) ) {
                        return processor.parser.parseHtml( xml, values );
                    } else {
                        throw Error( "File is not in XML format" );
                    }
                    return {};
                }
                var options = HafasPrivate.prepareOptions( processor.options, hafas.options );
                xml = helper.decode( xml, options.charset(Hafas.XmlFormat) );
                xml = HafasPrivate.fixXml( xml );

                var domDocument = new QDomDocument();
                if ( !domDocument.setContent(xml) ) {
                    throw Error("Hafas.routeData.parseXml(): Malformed XML");
                }
                var docElement = domDocument.documentElement();
                if ( !HafasPrivate.checkXmlForErrors(docElement) ) {
                    return {};
                }

                // Find all <St> tags, which contain information about intermediate stops
                var stopNodes = docElement.elementsByTagName("St");
                var routeData = { RouteStops: [], RouteTimes: [] };
                var inRange = values.dateTime == undefined || values.dataType == "arrivals";
                var lastTime, firstDate = new Date;
                for ( var i = 0; i < stopNodes.count(); ++i ) {
                    // Get the <St> tag for the current intermediate stop
                    var node = stopNodes.at( i ).toElement();

                    // Read information about the current stop from attributes
                    var routeStop = helper.decodeHtmlEntities( node.attributeNode("name").nodeValue() );
                    var arrival = node.attributeNode("arrTime").nodeValue();
                    var departure = node.attributeNode("depTime").nodeValue();
                    var arrivalDelay = parseInt( node.attributeNode("adelay").nodeValue() );
                    var departureDelay = parseInt( node.attributeNode("ddelay").nodeValue() );
                    if ( isNaN(arrivalDelay) ) {
                        arrivalDelay = -1;
                    }
                    if ( isNaN(departureDelay) ) {
                        departureDelay = -1;
                    }
                    var timeString = departure.length < 5 ? arrival : departure;
                    var delay = departure.length < 5 ? arrivalDelay : departureDelay;
                    var timeValue; //, timeValueWithDelay;
                    var validTimeFound = false;
                    if ( timeString.length < 5 ) {
                        // No time given for the current intermediate stop
                        if ( i == 0 && values.dateTime != undefined ) {
                            // Use "first time" for the first route stop if available
                            timeValue = values.dateTime;
                        } else {
                            // The HTML versions simply do not show such stops,
                            // create an invalid Date object for the stop here
                            timeValue = new Date( "unknown" );
                        }
//                         timeValueWithDelay = timeValue;
                    } else {
                        time = helper.matchTime( timeString, "hh:mm" );
                        if ( time.error ) {
                            helper.warning( "Hafas.routeData.parseXml(): Could not match route time: '" + timeString + "'", timeString );
                            continue;
                        }
                        timeValue = typeof(lastTime) != 'undefined'
                            ? new Date(lastTime.getFullYear(), lastTime.getMonth(), lastTime.getDate())
                            : new Date(firstDate.getFullYear(), firstDate.getMonth(), firstDate.getDate());
                        timeValue.setHours( time.hour, time.minute, 0, 0 );

                        if ( typeof(lastTime) != 'undefined' && timeValue < lastTime ) {
                            // Assume departures to be sorted by date and time
                            // Add one day
                            print( "  (will add one day)" );
                            timeValue.setTime( timeValue.getTime() + 24 * 60 * 60 * 1000 );
                        }

                        if ( values.dataType == "arrivals" &&
                            routeData.RouteStops.length == 0 &&
                            timeValue.getTime() >= values.dateTime.getTime() )
                        {
                            // No arrival route stops collected, but the current route time
                            // is after the departure time from the home stop, which follows
                            // later, subtract one day to correct the date
                            print( "  (will subtract one day)" );
                            timeValue.setTime( timeValue.getTime() - 24 * 60 * 60 * 1000 );
                        }

                        // Simply add delays to the time value,
                        // route stop delays are currently only supported for journeys
//                         timeValueWithDelay = timeValue;
//                         if ( delay > 0 ) {
//                             timeValueWithDelay.setTime( timeValue.getTime() + delay * 60 * 1000 );
//                         }

                        validTimeFound = true;
                        lastTime = timeValue;
                    }

                    if ( inRange || routeStop == values.stop ||
                         (values.dataType == "departures" && validTimeFound &&
                          timeValue.getTime() >= values.dateTime.getTime()) )
                    {
                        inRange = true;
                        routeData.RouteStops.push( routeStop );
                        routeData.RouteTimes.push( timeValue );
//                         routeData.RouteTimesDelay.push( delay ); // TODO
                    }
                    if ( inRange && values.dataType == "arrivals" &&
                         timeValue.getTime() >= values.dateTime.getTime() )
                    {
                        // Last route stop for arrivals, ie. the home stop
                        break;
                    }
                }

                return routeData;
            },

            /**
            * Parse train info document in Hafas.formats.HtmlFormatformat.
            * This function needs to be implemented outside this script to support the text format.
            * It should not access the "result" object. It should only be used to get train info URLs
            * for departures/arrivals.
            * @param {String} document Contents of a train info document in HTML text format received
            *   from the HAFAS provider.
            * @param {String} stop The name of the stop of the departure, should one of the
            *    found route stops TODO
            * @param {Date} [firstTime] Date and time of the departure at stop
            * @return {Object} An object with these properties: RouteStops, RouteTimes.
            **/
            parseHtml: function( html, values ) {
                print( "parseHtml" );
                var options = HafasPrivate.prepareOptions( processor.options, hafas.options );
                html = helper.decode( html, options.charset(Hafas.HtmlFormat) );

                // Find table
                var trainBlock = helper.findFirstHtmlTag( html, "table",
                    {attributes: {"class": "result\\s+stboard\\s+train"}} );
                if ( !trainBlock.found ) {
                    throw Error("Hafas.routeData.parseHtml(): Result table not found");
                }

                var routeData = { RouteStops: [], RouteTimes: [] };
                var trainRow = { position: -1 };
                var inRange = values.dateTime == undefined;
                var lastTime;
                while ( (trainRow = helper.findFirstHtmlTag(trainBlock.contents, "tr",
                    {position: trainRow.position + 1})).found )
                {
                    // If there is at least one attribute, this row is not a departure/arrival row,
                    // it is a row showing other controls/information, eg. a link to go to earlier/later results.
                    var hasAttributes = false;
                    for ( a in trainRow.attributes ) {
                        hasAttributes = true;
                        break;
                    }
                    if ( hasAttributes ) { continue; }

                    // Find columns, ie. <td> tags in the current departure row
                    var columns = helper.findNamedHtmlTags( trainRow.contents, "td",
                        {attributes: {"class": ""}, ambiguousNameResolution: "addNumber",
                        namePosition: {type: "attribute", name: "class", regexp: "([^\\s]*)(?:\\s+\\w+)?"}} );

                    // Check that all needed columns are found
                    if ( !columns.names.contains("station") || !columns.names.contains("arrival") ||
                         !columns.names.contains("departure") )
                    {
                        if ( trainRow.contents.indexOf("<th") == -1 ) {
                            // There's only an error, if this isn't the header row of the table
                            helper.error("Hafas.routeData.parseHtml(): Did not find all columns in a row of " +
                                         "the result table! Found: " + columns.names, trainRow.contents);
                        }
                        continue; // Not all columns where not found
                    }

                    var timeString = helper.decodeHtmlEntities(columns["departure"].contents).length < 5
                            ? columns["arrival"].contents : columns["departure"].contents;

                    time = helper.matchTime( timeString, "hh:mm" );
                    if ( time.error ) {
                        helper.error( "Hafas.routeData.parseHtml(): Could not match route time", timeString );
                        continue;
                    }
                    var timeValue;
                    if ( values.dateTime == undefined ) {
                        timeValue = new Date();
                        timeValue.setHours( time.hour, time.minute, 0, 0 );
                    } else {
                        timeValue = new Date( values.dateTime.getFullYear(),
                                              values.dateTime.getMonth(),
                                              values.dateTime.getDate(),
                                              time.hour, time.minute, 0, 0 );
                    }
                    if ( typeof(lastTime) != 'undefined' && timeValue < lastTime ) {
                        // Add one day
                        timeValue.setTime( timeValue.getTime() + 24 * 60 * 60 * 1000 );
                    }
                    lastTime = timeValue;

                    var routeStop = helper.stripTags( columns["station"].contents );
                    if ( inRange || routeStop == values.stop ||
                         timeValue.getTime() >= values.dateTime.getTime() )
                    {
                        inRange = true;
                        routeData.RouteStops.push( routeStop );
                        routeData.RouteTimes.push( timeValue );
                    }
                }

                return routeData;
            }
        })
    }); // End DataTypeProcessor

    return processor;
};
