/**
 * @projectDescription Contains the DataTypeProcessor used for stop suggestion data.
 *
 * @author   Friedrich Pülz fpuelz@gmx.de
 * @version  0.1
 **/

/** Object for stop suggestion data. */
var __hafas_stopsuggestions = function(hafas) {
    var processor = new HafasPrivate.DataTypeProcessor({
        /**
        * Features supported when using Hafas.stopSuggestions.get(), Hafas.stopSuggestions.parse().
        * PublicTransport.ProvidesStopsByGeoPosition can be removed by the provider
        * using the Hafas object if not supported.
        **/
        features: [PublicTransport.ProvidesStopID,
                PublicTransport.ProvidesStopPosition,
                PublicTransport.ProvidesStopsByGeoPosition],

        /** Options for functions in Hafas.timetable, overwrite global Hafas.options. */
        options: {
            format: Hafas.JavaScriptFormat
        },

        /**
        * A default implementation of the getStopSuggestions() function for HAFAS-providers.
        *
        * If no options are needed other than those specified in Hafas.options (eg. baseUrl)
        * this function can be directly used like here:
        * {@code var getStopSuggestions = Hafas.stopSuggestions.get;}
        *
        * If the options need to be modified on the fly code like this can be used:
        * {@code
        * function getStopSuggestions(values) {
        *     var options = {}; // Fill with options
        *     return Hafas.stopSuggestions.get( values, options );
        * }
        * }
        *
        * This functions automatically requests stop suggestions by name or by geo position,
        * depending on the values in the "values" argument. If the provider does not support
        * requesting stops by position, remove the PublicTransport.ProvidesStopsByGeoPosition
        * feature from Hafas.stopSuggestions.features.
        *
        * @param {Object} values The "values" object given to the getStopSuggestions()
        *   function from the data engine.
        * @param {Object} [options] Options to be passed to Hafas.stopSuggestions.url()
        *   or Hafas.getStopSuggestionsByGeoPositionUrl().
        **/
        get: function( values, options ) {
            HafasPrivate.checkValues( values,
                    {optional: {stop: 'string', count: 'number',
                                longitude: 'number', latitude: 'number',
                                distance: 'number'}} );
            var byGeoPosition = values.stop == undefined &&
                    values.longitude != undefined && values.latitude != undefined;
            var options = HafasPrivate.prepareOptions( HafasPrivate.extend(options, processor.options),
                                        {type: byGeoPosition ? "ny" : "n"}, hafas.options );
            if ( byGeoPosition &&
                 !processor.features.contains(PublicTransport.ProvidesStopsByGeoPosition) )
            {
                // Stop suggestions were requested by position, but the features were modified
                // to not allow this, ie. it's not supported by the provider using the Hafas object
                return false;
            }

            var url = byGeoPosition
                ? processor.urlGeoPosition(values, options)
                : processor.url(values, options);
            var document = network.getSynchronous( url, options.timeout );
            if ( !network.lastDownloadAborted ) {
                var parserObject = byGeoPosition ? processor.parserGeoPosition
                                                    : processor.parser;
                var parser = parserObject.parserByFormat( options.format );
                return parser( document );
            } else {
                return false;
            }
        },

        /**
        * Get an URL to stop suggestions in HAFAS JavaScript format.
        *
        * @param {Object} values The "values" object given to the getStopSuggestions() function from
        *   the data engine. Should contain a "stop" property.
        * @param {Object} [options] An object containing options as properties:
        *   {String} baseUrl The provider specific base URL, until "/bin/..." (excluding).
        *   {String} program The name of the Hafas program to use, the default is "ajax-getstop.exe".
        *   {String} language One character for the language.
        *   {String} type Can be "n" for a normal layout or "l" for a Lynx text layout.
        *       A special value is "ox" for a mobile HTML language.
        *   {String} additionalUrlQueryItems Can be used to add additional URL parameters.
        **/
        url: function( values, options ) {
            // Apply default options
            var options = HafasPrivate.prepareOptions(
                    HafasPrivate.extend(options, processor.options),
                    {program: "ajax-getstop", type: "ny"}, hafas.options );
            if ( typeof options.baseUrl != "string" || options.baseUrl.length == 0 )
                throw TypeError("Hafas.stopSuggestions.url(): No base URL given");

            var query = "REQ0JourneyStopsS0A=1" + // Stop type(s) to request, 1 for normal stops,
                          // 2 for addresses, 3 for normal stops AND addresses, 4 for POIs, ...
                    "&REQ0JourneyStopsS0G=" + values.stop + options.additionalUrlQueryItems;
            return HafasPrivate.getUrl( options, query );
        },

        /** Contains functions to parse normal stop suggestions documents. */
        parser: new HafasPrivate.Parser({
            /**
            * Parse stop suggestions in HAFAS JavaScript format, from the ajax-getstop.exe program.
            *
            * The JavaScript part simply gets cut away and the list of stop suggestions gets parsed
            * using JSON.parse() (good performance).
            * Found suggestions will be added to the "result" object.
            * @param {String} js Contents of a stop suggestions document in JavaScript format received from
            *   the HAFAS provider.
            * @return {Boolean} True, if stop suggestions were found, false otherwise.
            **/
            parseJavaScript: function( js ) {
                // First check format and cut away the JavaScript code (do not eval it)
                if ( js.left(23).string() != "SLs.sls={\"suggestions\":" ||
                        js.right(23).string() != "};SLs.showSuggestion();" )
                {
                    throw TypeError("Hafas.stopSuggestions.parse(): Incorrect stop suggestions document received");
                }

                var options = HafasPrivate.prepareOptions( processor.options,
                        {program: "query", language: "d", type: "ny", additionalUrlQueryItems: ""},
                         hafas.options );
                var json = helper.decode( js.mid(23, js.length() - 46),
                                          options.charset(Hafas.JavaScriptFormat) );

                // QtScript's JSON.parse() does not like "\'" inside JSON strings,
                // in a browser it works..
                json = json.replace( /\\'/g, "'" );

                // Parse JSON, expected is a list of objects with these properties
                // describing the found stops:
                //   "value": The name of the stop
                //   "id": A string containing the name, longitude/latitude, ID, ...
                //   "xcoord": Longitude * 1,000,000
                //   "ycoord": Latitude * 1,000,000
                //   "weight": The weight of the stop suggestion
                //   "type": The type of the stop, 1 for a normal station, 2 for an address, 4 for a POI,
                //      other values are used by some providers
                //   "typeStr": "type" as human readable string (1: "[Bhf/Hst]", 2: "[Adr]", 4: "[POI]" for db.de)
                //   "state": Unknown, a string, eg. "id"
                //   "prodClass": Unknown, a number, eg. "0"
                var stops = JSON.parse( json );
                var stopIdRegExp = /@L=(\d+)/i;
                for ( i = 0; i < stops.length; ++i ) {
                    var stop = stops[i];
                    var stopId = stopIdRegExp.exec( stop.id );
                    result.addData( {StopName: stop.value,
                            StopID: stopId ? stopId[1] : undefined,
                            StopWeight: parseInt(stop.weight),
                            StopLongitude: parseInt(stop.xcoord) / 1000000,
                            StopLatitude: parseInt(stop.ycoord) / 1000000} );
                }
                return result.hasData();
            }
        }),

        /**
        * Get an URL to a document containing stops around a given geological position.
        *
        * @param {Object} values The "values" object given to the getStopSuggestions() function from
        *   the data engine. Should contain "longitude" and "latitude" properties.
        * @param {Object} [options] An object containing options as properties:
        *   {String} baseUrl The provider specific base URL, until "/bin/..." (excluding).
        *   {String} program The name of the Hafas program to use, the default is "query".
        *   {String} language Two characters, the first for the language, the second can be "n" for
        *       the normal language or "l" for a Lynx text language. A special value is "dox" for a
        *       mobile HTML language. Another special value is "dny", the default value used
        *       for a JSON format.
        *   {String} additionalUrlQueryItems Can be used to add additional URL parameters.
        **/
        urlGeoPosition: function( values, options ) {
            var options = HafasPrivate.prepareOptions( HafasPrivate.extend(options, processor.options),
                    {program: "query", language: "d", type: "ny"}, hafas.options );
            if ( typeof options.baseUrl != "string" || options.baseUrl.length == 0 )
                throw TypeError("Hafas.stopSuggestions.urlGeoPosition(): No base URL given");

            var query = "performLocating=2" +
                "&tpl=stop2json" +
                "&look_maxno=" + (values.count != undefined ? Math.min(values.count, 999) : 200) +
                "&look_maxdist=" + (values.distance != undefined ? values.distance : 500) + // maximal distance in meters
                "&look_stopclass=11111111111111" + // All products
                "&look_nv=deleteDoubles|yes" + //get_stopweight|yes" +
                "&look_x=" + Math.round(values.longitude * 1000000) +
                "&look_y=" + Math.round(values.latitude * 1000000);
            return HafasPrivate.getUrl( options, query );
        },

        /** Contains functions to parse stop suggestion documents for specific geo positions. */
        parserGeoPosition: new HafasPrivate.Parser({
            options: { fixJson: false },

            /**
            * Parse stop suggestions in HAFAS JSON format, from the query.exe program.
            *
            * Found suggestions will be added to the "result" object. Uses JSON.parse() (good performance).
            * @param {String} json Contents of a stop suggestions document in JSON format received from
            *   the HAFAS provider.
            * @return {Boolean} True, if stop suggestions were found, false otherwise.
            **/
            parseJavaScript: function( js ) {
                if ( js.length < 2 ) {
                    throw Error("Hafas.stopSuggestions.parseGeoPosition(): " +
                            "Incorrect stop suggestions document received");
                }

                var options = HafasPrivate.prepareOptions( processor.parserGeoPosition.options,
                        processor.options, hafas.options );
                var json = helper.decode( js, options.charset(Hafas.JavaScriptFormat) );

                if ( options.fixJson ) {
                    // Add quotation marks around keys
                    json = json.replace( /\b(\w+)\b\s*:/g, "\"$1\":" ); // TODO
                }

                // Parse JSON, expected is an object with these properties:
                // "prods": A bitfield as string with "1" for enabled products, eg. "11111111111111",
                //    copied from the request
                // "stops": A list of objects with these properties describing the found stops:
                //   "x": Longitude * 1,000,000
                //   "y": Latitude * 1,000,000
                //   "name": The name of the stop
                //   "urlname" (useless): Same as "name", but URL encoded..
                //   "extId": The external ID of the stop
                //   "puic": Unknown, a number, eg. "80"
                //   "dist": The distance from the requested position in meters
                //   "planId": Unknown, a number, eg. "1343155529"
                // "error": "0" if no error
                // "numberofstops" (useless): Number of stop objects in "stops"
                var stopData;
                try {
                    stopData = JSON.parse( json ); // TODO only works with {"property": "value"}, not working: {property: "value"}
                } catch ( err ) {
                    throw Error("Error while parsing stop suggestions in JSON format: " +
                        err.message + ", JSON content: " + json );
                }

                if ( parseInt(stopData.error) != 0 ) {
                    throw Error("Hafas.stopSuggestions.parseGeoPosition(): " +
                            "Error reported in stop suggestions document (" + stopData.error + ")");
                }

                for ( i = 0; i < stopData.stops.length; ++i ) {
                    var stop = stopData.stops[ i ];
                    result.addData( {StopName: stop.name, StopID: stop.extId,
                            StopLongitude: stop.x / 1000000, StopLatitude: stop.y / 1000000} );
                }
                return result.hasData();
            }
        })
    }); // End DataTypeProcessor

    return processor;
};
