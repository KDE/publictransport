/**
 * @projectDescription Contains the DataTypeProcessor used for timetable data.
 *
 * @author   Friedrich Pülz fpuelz@gmx.de
 * @version  0.1
 **/

/** Object for timetable data. */
var __hafas_timetable = function(hafas) {
    var processor = new HafasPrivate.DataTypeProcessor({
        /** Features supported when using Hafas.timetable.get(), Hafas.timetable.parse(). */
        features: [ PublicTransport.ProvidesArrivals,
                    PublicTransport.ProvidesDelays,
                    PublicTransport.ProvidesPlatform,
                    PublicTransport.ProvidesNews ],

        /** Options for functions in Hafas.timetable, overwrite global Hafas.options. */
        options: {
            format: Hafas.XmlFormat
        },

        /**
        * A default implementation of the getTimetable() function for HAFAS-providers.
        *
        * Code example from Hafas.stopSuggestions.get() also apply to Hafas.timetable.get().
        * @note The script extension "qt.xml" is needed.
        *
        * @param {Object} values The "values" object given to the getTimetable()
        *   function from the data engine.
        * @param {Object} [options] Options to be passed to Hafas.timetable.url().
        **/
        get: function( values, options ) {
            HafasPrivate.checkValues( values,
                    {required: {stop: 'string'},
                     optional: {dateTime: 'object', maxCount: 'number'}} );
            var options = HafasPrivate.prepareOptions( options,
		    processor.options, hafas.options );
            var url = processor.url( values, options );
            var userUrl = processor.userUrl( values, options );
            var request = network.createRequest( url, userUrl );
            request.finished.connect( processor.parser.parserByFormat(options.format) );
            HafasPrivate.startRequest( request, processor.addPostData, values, options );
        },

        /**
        * Get an URL to departures/arrivals.
        *
        * @param {Object} values The "values" object given to the getTimetable() function from the
        *       data engine.
        * @param {Object} [options] An object containing options as properties:
        *   {String} baseUrl The provider specific base URL, until "/bin/..." (excluding).
        *   {String} program The name of the Hafas program to use, the default is "stboard".
        *   {String} additionalUrlQueryItems Can be used to add additional URL parameters.
        *   {int} productBits The number of bits used to enable/disable searching for specific product
        *       types, ie. vehicle types. The default value is 14.
        *   {String} urlDateFormat Can be used to provide an alternative date format,
        *       the default value is "dd.MM.yy".
        *   {String} urlTimeFormat Can be used to provide an alternative time format,
        *       the default value is "hh:mm".
        *   {String} language One character for the language
        *   {String} type Can be "n" for normal or "l" for Lynx type. A special value is "ox" for a
        *       mobile HTML type.
        *   {String} layout The layout of the requested document, the default is "vs_java3", a short
        *       XML layout.
        * @return {String} The generated URL.
        **/
        url: function( values, options ) {
            var options = HafasPrivate.prepareOptions( HafasPrivate.extend(options, processor.options),
                    {program: "stboard", additionalUrlQueryItems: "",
                     language: "d", type: "n", layout: "vs_java3",
                     stopPostfix: "!"}, // "!"/"?" to never/always show suggestions for ambiguous stop names
                    hafas.options );
            if ( typeof options.baseUrl != "string" || options.baseUrl.length == 0 ) {
                throw TypeError("Hafas.timetable.url(): The option baseUrl must be a non-empty string");
            }

            var query = "rt=1" + // Enable realtime data
                "&input=" + values.stop + options.stopPostfix +
                "&boardType=" + (values.dataType == "arrivals" ? "arr" : "dep");
            if ( values.dateTime != undefined ) {
                query += "&date=" + helper.formatDateTime(values.dateTime, options.urlDateFormat) +
                         "&time=" + helper.formatDateTime(values.dateTime, options.urlTimeFormat);
            }
            query += "&maxJourneys=" + Math.max(20, values.maxCount == undefined ? 0 : values.maxCount) + // Maximum number of results
                "&disableEquivs=yes" + // Do not use nearby stations
                (options.layout == undefined ? "" : ("&L=" + options.layout)) + // Specify the output layout
                "&start=yes" + // Start the request instead of showing a form
                "&productsFilter=" + HafasPrivate.createProductBitString(options.productBits) + // Extend filter to include all products
                options.additionalUrlQueryItems;
            if ( options.format == Hafas.XmlResCFormat ) {
                query += "&output=xml";
            }
            return HafasPrivate.getUrl( options, query );
        },

        parser: new HafasPrivate.Parser({
            /**
            * Parse departures/arrivals in HAFAS XML format.
            *
            * Found departures/arrivals will be added to the "result" object.
            * @note The script extension "qt.xml" is needed.
            *
            * @param {String} xml Contents of a departures/arrivals document in XML format received from
            *   the HAFAS provider.
            * @return {Boolean} True, if departures/arrivals were found, false otherwise.
            **/
            parseXml: function( xml ) {
                var options = HafasPrivate.prepareOptions( processor.options, hafas.options );
                if ( !HafasPrivate.expectFormat(Hafas.XmlFormat, xml) ) {
                    if ( HafasPrivate.isHtml(xml) &&
                         typeof(processor.parser.parseHtml) == 'function' &&
                         (processor.parser.parseHtml.isNotImplemented == undefined ||
                          !processor.parser.parseHtml.isNotImplemented) )
                    {
                        helper.warning( "Received document is not in XML format, but HTML" );
                        return processor.parser.parseHtml( xml );
                    }
                    throw Error( "Received document is not in XML format" );
                    return false;
                }

                xml = helper.decode( xml, options.charset(Hafas.XmlFormat) );
                xml = HafasPrivate.fixXml( xml );
                var domDocument = new QDomDocument();
                if ( !domDocument.setContent(xml) ) {
                    helper.error( "Malformed: " + xml );
                    throw Error("Hafas.timetable.parseXml(): Malformed XML");
                }
                var docElement = domDocument.documentElement();
                if ( !HafasPrivate.checkXmlForErrors(docElement) ) {
                    return false;
                }

                // Find all <Journey> tags, which contain information about a departure/arrival
                var journeyNodes = docElement.elementsByTagName("Journey");
                var count = journeyNodes.count();
                for ( var i = 0; i < count; ++i ) {
                    var node = journeyNodes.at( i ).toElement();
                    var departure = {};

                    var date = helper.matchDate( node.attributeNode("fpDate").nodeValue(),
                                                    options.xmlDateFormat );
                    var time = helper.matchTime( node.attributeNode("fpTime").nodeValue(),
                                                    options.xmlTimeFormat  );
                    if ( time.error ) {
                        helper.error( "Hafas.timetable.parse(): Cannot parse time",
                                    node.attributeNode("fpTime").nodeValue() );
                        continue;
                    }
                    date.setHours( time.hour, time.minute, 0, 0 );
                    departure.DepartureDateTime = date;
                    var target = node.attributeNode("targetLoc").nodeValue();
                    departure.Target = node.hasAttribute("dir") ? node.attributeNode("dir").nodeValue() : target;

                    var transportLine, vehicleType;
                    var product = node.attributeNode("prod").nodeValue();
                    var pos = product.indexOf( "#" );
                    if ( pos < 1 ) {
                        var classId = parseInt( node.attributeNode("class").nodeValue() );
                        vehicleType = hafas.vehicleFromClass( classId );

                        if ( vehicleType == PublicTransport.UnknownVehicleType ) {
                            if ( node.hasAttribute("hafasname") ) {
                                product = node.attributeNode("hafasname").nodeValue();
                                helper.information( "hafasname: " + product );
                            }

                            var vehicleTypeString;
                            pos = product.indexOf( " " );
                            if ( pos < 1 ) {
                                transportLine = product;
                                vehicleTypeString = product;
                            } else {
                                transportLine = product.substr( pos + 1 );
                                vehicleTypeString = product.substr( 0, pos );
                            }
                            vehicleType = hafas.vehicleFromString( vehicleTypeString );
                        } else {
                            transportLine = product;
                        }
                    } else {
                        transportLine = product.substr( 0, pos );
                        vehicleType = hafas.vehicleFromString( product.substr(pos + 1) );
                    }
                    departure.TransportLine = HafasPrivate.trimTransportLine( transportLine );
                    departure.TypeOfVehicle = vehicleType;

                    var delay = node.attributeNode("delay").nodeValue();
                    if ( delay.length > 0 ) {
                        if ( delay == "-" ) {
                            departure.Delay = -1;
                        } else if ( delay == "0" ) {
                            departure.Delay = 0;
                        } else if ( delay[0] == "-" ) {
                            departure.Delay = -parseInt( delay.substr(1) );
                        } else if ( delay[0] == "+" ) {
                            departure.Delay = parseInt( delay.substr(1) );
                        } else if ( delay[0] == "cancel" ) {
                            departure.Delay = -2; // TODO -2 => cancelled?
                        } else {
                            helper.warning( "Hafas.timetable.parse(): Unknown delay string: " + delay );
                        }
                    }
                    departure.DelayReason = helper.trim( node.attributeNode("delayReason").nodeValue() );

                    result.addData( departure );
                }
            },

            /**
            * Parse departures/arrivals in HAFAS XML ResC format.
            *
            * Found departures/arrivals will be added to the "result" object.
            * @note The script extension "qt.xml" is needed.
            *
            * @param {String} xml Contents of a departures/arrivals document in XML format received from
            *   the HAFAS provider.
            * @return {Boolean} True, if departures/arrivals were found, false otherwise.
            **/
            parseXmlResC: function( xml ) {
                if ( !HafasPrivate.expectFormat(Hafas.XmlFormat, xml) ) {
                    if ( HafasPrivate.isHtml(xml) ) {
                        return processor.parseHtml( xml );
                    }
                    return;
                }

                var options = HafasPrivate.prepareOptions( processor.options, hafas.options );
                xml = helper.decode( xml, options.charset(Hafas.XmlFormat) );
                var domDocument = new QDomDocument();
                if ( !domDocument.setContent(xml) ) {
                    helper.error( "Malformed: " + xml );
                    throw Error("Hafas.timetable.parseXmlResC(): Malformed XML");
                }
                var docElement = domDocument.documentElement();

                // Errors are inside the <Err> tag
                var errNodes = docElement.elementsByTagName( "Err" );
                if ( !errNodes.isEmpty() ) {
                    var errElement = errNodes.item( 0 ).toElement();
                    var errCode = errElement.attributeNode("code").nodeValue();
                    var errMessage = errElement.attributeNode("text").nodeValue();
                    var errLevel = errElement.attributeNode("level").nodeValue();

                    // Error codes:
                    // - H890: No trains in result (fatal)
                    if ( errLevel.toLower() == "e" ) {
                        // Error is fatal, don't proceed
                        helper.error( "Fatal error: " + errCode + " " + errMessage + " level: " + errLevel );
                        return false;
                    } else {
                        helper.warning( "Received a non fatal error: " + errCode + " " +
                                errMessage + " level: " + errLevel +
                                (errLevel.toLower() == "e" ? "Error is fatal" : "Error isn't fatal") );
                    }
                }

                // Use date of the first departure (inside <StartT>) as date for newly parsed departures.
                // If a departure is more than 3 hours before the last one, it is assumed that the new
                // departure is one day later, eg. read departure one at 23:30, next departure is at 0:45,
                // assume it's at the next day.
                var dateTime;
                var list = docElement.elementsByTagName( "StartT" );
                if ( list.isEmpty() ) {
                    helper.error( "No <StartT> tag found in the received XML document", xml );
                    return false;
                } else {
                    var startTimeElement = docElement.elementsByTagName( "StartT" ).at( 0 ).toElement();
                    if ( startTimeElement.hasAttribute("date") ) {
                        dateTime = helper.matchDate( startTimeElement.attribute("date"), "yyyyMMdd" );
                        if ( dateTime.getFullYear() < 1970 ) {
                            dateTime = new Date();
                        }
                    } else {
                        dateTime = new Date();
                    }
                }

                // Find all <Journey> tags, which contain information about a departure/arrival
                var journeyNodes = docElement.elementsByTagName("Journey");
                var count = journeyNodes.count();
                for ( var i = 0; i < count; ++i ) {
                    var node = journeyNodes.at( i );
                    var departure = { JourneyNews: "",
                            RouteStops: new Array(), RouteTimes: new Array(), RouteExactStops: 0 };

                    // <Product> tag contains the line string
                    departure.TransportLine = helper.trim(
                        node.firstChildElement("Product").attributeNode("name").nodeValue() );

                    // <InfoTextList> tag contains <InfoText> tags, which contain journey news
                    var journeyNewsNodes = node.firstChildElement("InfoTextList").elementsByTagName("InfoText");
                    var journeyNewsCount = journeyNewsNodes.count();
                    for ( var j = 0; j < journeyNewsCount; ++j ) {
                        var journeyNewsNode = journeyNewsNodes.at( i );
                        var newJourneyNews = journeyNewsNode.toElement().attributeNode("text").nodeValue();

                        if ( departure.JourneyNews.indexOf(newJourneyNews) == -1 ) {
                            HafasPrivate.appendJourneyNews( departure, newJourneyNews );
                        }
                    }

                    // <MainStop><BasicStop><Dep> tag contains some tags with more information
                    // about the departing stop
                    var basicStop = node.firstChildElement("MainStop").firstChildElement("BasicStop");
                    var stop = basicStop.firstChildElement("Dep").isNull()
                        ? basicStop.firstChildElement("Arr") : basicStop.firstChildElement("Dep");

                    // <Time> tag contains the departure time
                    var time = helper.matchTime( stop.firstChildElement("Time").text(), "hh:mm" );
                    if ( time.error ) {
                        helper.error( "Unexpected string in Time element", stop.firstChildElement("Time").text() );
                        return false;
                    }
                    dateTime.setHours( time.hour, time.minute, 0 );
                    departure.DepartureDateTime = dateTime;

                    // <Delay> tag contains delay
                    var sDelay = stop.firstChildElement("Delay").text();
                    departure.Delay = sDelay.length == 0 ? -1 : parseInt(sDelay);

                    // <Platform> tag contains the platform
                    departure.Platform = stop.firstChildElement("Platform").text();

                    // <PassList> tag contains stops on the route of the line, inside <BasicStop> tags
                    var routeStopList = node.firstChildElement("PassList").elementsByTagName("BasicStop");
                    var routeStopCount = routeStopList.count();
                    for ( var r = 0; r < routeStopCount; ++r ) {
                        var routeStop = routeStopList.at( r );

                        departure.RouteStops.push(
                            helper.trim(routeStop.firstChildElement("Location").firstChildElement("Station")
                            .firstChildElement("HafasName").firstChildElement("Text").text()) );
                        departure.RouteTimes.push( /*QTime::fromString(*/
                            routeStop.firstChildElement("Arr").firstChildElement("Time").text()/*, "hh:mm" )*/
                                );
                    }

                    // Other information is found in the <JourneyAttributeList> tag which contains
                    // a list of <JourneyAttribute> tags
                    var journeyAttribute = node.firstChildElement("JourneyAttributeList")
                                .firstChildElement("JourneyAttribute");
                    while ( !journeyAttribute.isNull() ) {
                        // Get the child tag <Attribute> and handle it based on the value of the "type" attribute
                        var attribute = journeyAttribute.firstChildElement("Attribute");
                        if ( attribute.attributeNode("type").nodeValue() == "DIRECTION" ) {
                            // Read direction / target
                            departure.Target = attribute.firstChildElement("AttributeVariant")
                                .firstChildElement("Text").text();
                        } else if ( attribute.attributeNode("type").nodeValue() == "CATEGORY" ) {
                            // Read vehicle type from "category"
                            var category = attribute.firstChildElement( "AttributeVariant" );
                            while ( !category.isNull() ) {
                                if ( category.attributeNode("type").nodeValue() == "NORMAL" ) {
                                    departure.TypeOfVehicle = hafas.vehicleFromString(
                                        helper.trim(category.firstChildElement("Text").text()) );
                                    break;
                                }
                                category = category.nextSiblingElement("AttributeVariant");
                            }
                        } else if ( attribute.attributeNode("type").nodeValue() == "OPERATOR" ) {
                            // Read operator
                            departure.Operator = attribute.firstChildElement("AttributeVariant")
                                .firstChildElement("Text").text();
                        } else if ( typeof(departure.TransportLine) == 'undefined'
                            && attribute.attributeNode("type").nodeValue() == "NAME" )
                        {
                            // Read line string if it wasn't read already
                            departure.TransportLine = helper.trim(
                                attribute.firstChildElement("AttributeVariant").firstChildElement("Text").text() );
                        } else if ( attribute.attributeNode("type").nodeValue() == "NORMAL" ) {
                            // Read less important journey news
                            var info = helper.trim( attribute.firstChildElement("AttributeVariant")
                                            .firstChildElement("Text").text() );
                            if ( departure.JourneyNews.indexOf(info) == -1 ) {
                                HafasPrivate.appendJourneyNews( departure, info );
                            }
                    //             } else {
                    //                 print( "Unhandled attribute type " + attribute.attributeNode("type").nodeValue() +
                    //                        " with text (tags stripped) " + attribute.text() );
                        }

                        // Go to the next journey attribute
                        journeyAttribute = journeyAttribute.nextSiblingElement("JourneyAttribute");
                    }

                    // Add departure to the journey list
                    result.addData( departure );
                }

                return count > 0;
            }
        }),

        /**
        * Object for additional timetable data.
        *
        * Currently it can read route data documents and provide it in the
        * form needed for the getAdditionalData() script function.
        *
        * @note Because traininfo.exe-URLs are not included in XML data
        *   sources, by default the mobile HTML format gets used by default
        *   to get these URLs. Therefore you need to implement a
        *   Hafas.timetable.parseHtmlMobile() function.
        * @note The route data documents themselves are available in
        *   XML format, but only from some providers. If it is not
        *   available, specify another format using the
        *   "Hafas.routeData.options.format" option and implement the
        *   associated function, eg. Hafas.routeData.parser.parseText().
        **/
        additionalData: new HafasPrivate.DataTypeProcessor({
            /*
            * Features supported when using Hafas.additionalData.get(),
            * Hafas.additionalData.getWithoutAdd().
            **/
            features: [PublicTransport.ProvidesRouteInformation],

            /**
            * Options for functions in Hafas.timetable used for additional data, o
            * verwrite global Hafas.options.
            **/
            options: {
                format: Hafas.HtmlMobileFormat,
                program: "stboard",
                batchSize: 15,
                cacheSize: 50
            },

            /**
            * A default implementation of the getAdditionalData() function for Hafas-providers.
            *
            * Code example from Hafas.stopSuggestions.get() also apply to
            * Hafas.additionalData.get().
            *
            * @param {Object} values The "values" object given to the getAdditionalData()
            *   function from the data engine.
            * @param {Object} [options] Options to be passed to Hafas.additionalData.getWithoutAdd().
            **/
            get: function( values, options ) {
                var additionalData = processor.additionalData.getWithoutAdd( values, options );
                result.addData( additionalData );
                return additionalData;
            },

            /** No parser used here, uses parsers from Hafas.timetable and Hafas.routeData. */
            parser: undefined,

            /**
            * Get an associative array containing additional data for a timetable item.
            *
            * This function is intended to be used with XML data sources that do not contain
            * traininfo.exe-URLs. It uses the mobile HTML timetable data source (/dox/) to get
            * these URLs, multiple URLs are requested at once. URLs get cached in the storage
            * automatically.
            * When the URL is available (cached or newly downloaded) it gets downloaded and
            * then parsed using parseTrainInfoText().
            *
            * Currently this function adds RouteStops and RouteTimes for departures/arrivals.
            *
            * @param {Object} values The "values" object given to the getAdditionalData()
            *   function from the data engine.
            * @param {Object} [options] An object containing options as properties:
            *   {String} program The name of the Hafas program to use, the default is "stboard".
            *   {String} additionalUrlQueryItems Can be used to add additional URL parameters.
            *   {int} productBits The number of bits used to enable/disable searching for
            *       specific product types, ie. vehicle types. The default value is 14.
            *   {String} dateFormat Can be used to provide an alternative date format,
            *       the default value is "dd.MM.yy". // TODO urlDateFormat
            *   {String} timeFormat Can be used to provide an alternative time format,
            *       the default value is "hh:mm".
            *   {String} layout The layout of the requested document,
            *       the default is "vs_java3", a short XML layout.
            *   {Integer} batchSize The number of traininfo.exe-URLs to request at once.
            *       The default is 15. A bigger value means that the mobile HTML document
            *       needs to be downloaded less often, but downloading it will take
            *       somewhat longer.
            *   {Integer} cacheSize The maximum number of traininfo.exe-URLs to keep in the cache.
            *   {Integer} format The format to use for timetable documents containing
            *       traininfo.exe URLs, the default is formats.HtmlMobileFormat.
            *       Unfortunately the XML format does not contain traininfo.exe-URLs and there
            *       does not seem to exist a way to generate these URLs from the train data.
            * @return {Array} An associative array containing additional data,
            *   keyed by Enums.TimetableInformation.
            **/
            getWithoutAdd: function( values, options ) {
                HafasPrivate.checkValues( values,
                        {required: {stop: 'string', dateTime: 'object',
                                    transportLine: 'string'}} );
                var options = HafasPrivate.prepareOptions( options,
                        processor.additionalData.options, hafas.options );
                var routeDataUrl = typeof(values.routeDataUrl) == 'string'
                        ? values.routeDataUrl : "";
                if ( routeDataUrl.length == 0 ) {
                    // No RouteDataUrl value given, get it from HTML data source or the cache
                    var key = "lastTrainData_" + values.stop;
                    var findRouteDataUrl = function( items, values ) {
                        for ( i = 0; i < items.length; ++i ) {
                            if ( items[i].DepartureDateTime.getTime() == values.dateTime.getTime() &&
                                 items[i].TransportLine == values.transportLine )
                            {
                                // Found the route data URL cached in the storage
                                return items[i].RouteDataUrl;
                            }
                        }

                        helper.warning( "No matching route data URL found" );
                        return "";
                    };

                    var items = storage.read( key, [] );
                    if ( items.length > 0 ) {
                        // Train info data is stored in the cache, try to find the train URL
                        routeDataUrl = findRouteDataUrl( items, values );
                    }
                    if ( routeDataUrl.length == 0 ) {
                        // Remove old items from the cache
                        if ( items.length > options.cacheSize ) {
                            items.slice( options.batchSize, options.cacheSize - options.batchSize );
                        }

                        // Request 5 minutes before the item to update
                        var requestDateTime = new Date( values.dateTime.getTime() - 5 * 60000 );

                        // TODO Journeys
                        // First get an URL to the mobile version of the departure board
                        // which contains traininfo.exe URLs
                        var urlValues = values;
                        urlValues.maxCount = options.batchSize;
                        var url = processor.url( urlValues, options );

                        // Download and parse new data and add it to the old cached items
                        var data = network.getSynchronous( url, options.timeout );
                        var parser = processor.parser.parserByFormat( options.format );
                        items = items.concat( parser(data) );
                        if ( items.length < 1 ) {
                            helper.error( "Could not find any traininfo.exe URL" );
                            return {};
                        }

                        // Cache items in the storage
                        storage.write( key, items );

                        // Try to find the train URL in the result, should be one of the first items
                        routeDataUrl = findRouteDataUrl( items, values );
                    }
                }

                if ( routeDataUrl.length == 0 ) {
                    helper.error( "Could not find any traininfo.exe URL" );
                    return {};
                } else {
                    if ( hafas.routeData.options.format == Hafas.XmlFormat ) {
                        routeDataUrl = routeDataUrl + "&L=vs_java3";
                    }
                    var routeDocument = network.downloadSynchronous( routeDataUrl, options.timeout );
                    if ( !network.lastDownloadAborted ) {
                        var parser = hafas.routeData.parser.parserByFormat(hafas.routeData.options.format);
                        return parser( routeDocument, values.stop, values.dateTime );
                    } else {
                        return {}; // Download was aborted
                    }
                }
            }
        }) // End timetable.additionalData
    }); // End DataTypeProcessor

    return processor;
};
