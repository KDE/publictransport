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
            parseXml: function( xml, stop, firstTime ) {
                if ( !HafasPrivate.expectFormat(Hafas.XmlFormat, xml) ) {
                    if ( HafasPrivate.isHtml(xml) ) {
                        return processor.parser.parseHtml( xml, stop, firstTime );
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

                // Find all <St> tags, which contain information about an intermediate stop
                var stopNodes = docElement.elementsByTagName("St");
                var count = stopNodes.count();
                var routeData = { RouteStops: [], RouteTimes: [] };
                var inRange = firstTime == undefined;
                for ( var i = 0; i < count; ++i ) {
                    var node = stopNodes.at( i ).toElement();

		    var routeStop = helper.decodeHtmlEntities( node.attributeNode("name").nodeValue() );
		    var arrival = node.attributeNode("arrTime").nodeValue();
		    var departure = node.attributeNode("depTime").nodeValue();
		    var timeString = arrival.length < 5 ? departure : arrival;
		    time = helper.matchTime( timeString, "hh:mm" );
		    if ( time.error ) {
			helper.warning( "Hafas.routeData.parseXml(): Could not match route time: '" + timeString + "'", timeString );
			continue;
		    }
		    timeValue = new Date( firstTime.getFullYear(), firstTime.getMonth(),
					   firstTime.getDate(), time.hour, time.minute, 0, 0 );

                    if ( inRange || routeStop == stop || firstTime.getTime() <= timeValue.getTime() ) {
                        inRange = true;
                        routeData.RouteStops.push( routeStop );
                        routeData.RouteTimes.push( timeValue );
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
	    *    found route stops
	    * @param {Date} [firstTime] Date and time of the departure at stop
	    * @return {Object} An object with these properties: RouteStops, RouteTimes.
	    **/
            parseHtml: function( html, stop, firstTime ) {
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

                    var timeString = helper.decodeHtmlEntities(columns["departure"].contents).length < 5 ?
                        columns["arrival"].contents : columns["departure"].contents;

                    time = helper.matchTime( timeString, "hh:mm" );
                    if ( time.error ) {
                        helper.error( "Hafas.routeData.parseHtml(): Could not match route time", timeString );
                        continue;
                    }
                    timeValue = new Date();
                    timeValue.setHours( time.hour, time.minute, 0, 0 );

                    if ( firstTime == undefined || timeValue >= firstTime ) {
                        routeData.RouteStops.push( helper.stripTags(columns["station"].contents) );
                        routeData.RouteTimes.push( timeValue );
                    }
                }

                return routeData;
            }
        })
    }); // End DataTypeProcessor

    return processor;
};
