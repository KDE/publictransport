/**
 * @projectDescription Base for PublicTransport engine script provider plugins using the HAFAS API.
 *
 * @note This script needs the qt.core and qt.xml extensions.
 *
 * @author   Friedrich Pülz fpuelz@gmx.de
 * @version  0.1
 **/

include("base_hafas_private.js");
include("base_hafas_stopsuggestions.js");
include("base_hafas_timetable.js");
include("base_hafas_routedata.js");
include("base_hafas_journeys.js");

/**
 * The "Hafas" class contains functions for HAFAS providers.
 *
 * To use this class first create an instance of it:
 * {@code var hafas = Hafas({baseUrl: "http://base.url"});}
 * The constructor accepts options as argument, the options can also be changed/read
 * later using the "options" property. For a list of available options see the options
 * property of the public interface of the Hafas class.
 *
 * The data processors also use an options property, which override the global options
 * (eg. hafas.timetable.options). For example the data format of stopSuggestions data
 * can be changed like this: {@code hafas.stopSuggestions.options.format = Hafas.TextFormat;}
 * This changes the used parser function, that gets called to parse a stop suggestions
 * document. In this case the hafas.stopSuggestions.parser.parseText() function needs
 * to be implemented (it is not implemented in this class).
 *
 * The functions provided by this class can be used to implement the main script functions,
 * eg. getStopSuggestions(). If no options are needed other than those specified in
 * hafas.options (eg. baseUrl) or hafas.stopSuggestions.options (in this case) the
 * associated function can be used directly like here:
 * {@code var getStopSuggestions = hafas.stopSuggestions.get;}
 *
 * If the options need to be modified on the fly, code like this can be used:
 * {@code
 * function getStopSuggestions( values ) {
 *     var options = {}; // Fill with options
 *     return hafas.stopSuggestions.get( values, options );
 * }
 * }
 *
 * There are four main types of data supported by this class, for each a property exists:
 * "timetable" (for departures/arrivals), "stopSuggestions", "journeys" and "routeData".
 * Each of these properties has a "get" method and a list of "features".
 *
 * If there are unknown vehicle types in the results, the hafas.otherVehicleFromString()
 * and/or hafas.otherVehicleFromClass() functions should be implemented.
 *
 * @note This class needs the qt.core extension.
 *       For the default XmlFormat and for the XmlResCFormat, the qt.xml extension is required.
 **/
var Hafas = function( options ) {
    // Public interface:
    var public = {
        /**
        * Global options used as default values.
        *
        * These options are used in functions that accept an options argument.
        * If an option is undefined, but defined in the global options,
        * the global option value gets used.
        *
        * This object can be modified by providers using this base script,
        * instead of providing the values in each function call.
        **/
        options: HafasPrivate.extend(options, { // Extend options from the constructor with default options
            baseUrl: "", // The base URL to the HAFAS API (followed by binDir, program, programExtension, ...)
            productBits: 14, // The number of bits used for product filter bit strings
            encodeUrlQuery: false, // Can be used to automatically percent encode URL queries
            binDir: "bin", // The name of the binary directory
            programExtension: "exe", // Program extension, sometimes this is "bin"
            urlDateFormat: "dd.MM.yy", // Format of dates in URLs
            urlTimeFormat: "hh:mm", // Format of times in URLs
            xmlDateFormat: "dd.MM.yy", // Format of dates in received XML documents
            xmlTimeFormat: "hh:mm", // Format of times in received XML documents
            format: Hafas.XmlFormat, // Default format
            language: "d", // Default language
            requestType: "GET", // Default request type (GET or POST)
            timeout: undefined // Default timeout in milliseconds for network requests,
                    // if undefined the default timeout of the "network" object gets used (30000)
        }),

        /** Enum for available data formats */
        formats: HafasPrivate.formats,

        /**
        * Get the full operator name from it's abbreviation used by HAFAS providers.
        * @param {String} abbr The abbreviation to get an operator name for.
        * @return {String} The full operator name.
        **/
        operatorFromAbbreviation: HafasPrivate.operatorFromAbbreviation,

        /**
        * Try to get a vehicle type from an operator abbreviation.
        * @param {String} abbr The abbreviation to get a vehicle type for.
        * @return {String} The full operator name.
        **/
        vehicleFromOperatorAbbreviation: HafasPrivate.vehicleFromOperatorAbbreviation,

        /**
        * Get the type of vehicle enumerable from a HAFAS vehicle type string.
        * To return vehicle types for non standard strings, implement the
        * Hafas.otherVehicleFromString() function.
        * @param {String} string The string to get the associated vehicle type enumerable for.
        * @return {PublicTransport.VehicleType} The vehicle type enumerable associated with the
        *   given string or PublicTransport.UnknownVehicleType if the string cannot be associated
        *   with a vehicle type.
        **/
        vehicleFromString: function( string, options ) {
            var vehicleType = HafasPrivate.vehicleFromString( string );
            if ( vehicleType == PublicTransport.UnknownVehicleType ) {
                vehicleType = public.otherVehicleFromString( string );
                if ( vehicleType == undefined ||
                     vehicleType == PublicTransport.UnknownVehicleType )
                {
                    vehicleType = public.vehicleFromOperatorAbbreviation( string );
                    if ( vehicleType == PublicTransport.UnknownVehicleType ) {
                        // Use default vehicle type, warn if it's still unknown
                        vehicleType = provider.defaultVehicleType;
                        options = HafasPrivate.extend( options, {unknownVehicleWarning: true} );
                        if ( options.unknownVehicleWarning &&
                             vehicleType == PublicTransport.UnknownVehicleType )
                        {
                            helper.warning( "Unknown vehicle type string: '" + string + "'" );
                        }
                    }
                }
            }
            return vehicleType;
        },

        /**
        * Get the type of vehicle enumerable from a HAFAS class ID.
        * To return vehicle types for non standard IDs, implement the
        * Hafas.otherVehicleFromClass() function.
        * @param {int} classId The ID to get the associated vehicle type enumerable for.
        * @return {PublicTransport.VehicleType} The vehicle type enumerable associated with the
        *   given class ID or PublicTransport.UnknownVehicleType if the ID cannot be associated
        *   with a vehicle type.
        **/
        vehicleFromClass: function( classId, options ) {
            var vehicleType = HafasPrivate.vehicleFromClass( classId );
            if ( vehicleType == PublicTransport.UnknownVehicleType ) {
                vehicleType = public.otherVehicleFromClass( classId );
                if ( vehicleType == undefined ||
                     vehicleType == PublicTransport.UnknownVehicleType )
                {
                    // Use default vehicle type, warn if it's still unknown
                    vehicleType = provider.defaultVehicleType;
                    options = HafasPrivate.extend( options, {unknownVehicleWarning: true} );
                    if ( options.unknownVehicleWarning &&
                         vehicleType == PublicTransport.UnknownVehicleType )
                    {
                        helper.warning( "Unknown vehicle type class: '" + classId + "'" );
                    }
                }
            }
            return vehicleType;
        },

        /** Can be replaced with a function that matches strings not handled
        * in the default vehicleFromString() function.
        * If nothing gets returned, an unknown vehicle type gets assumed. */
        otherVehicleFromString: function( string ) {
            return PublicTransport.UnknownVehicleType;
        },

        /** Can be replaced with a function that matches IDs not handled
        * in the default vehicleFromClass() function.
        * If nothing gets returned, an unknown vehicle type gets assumed. */
        otherVehicleFromClass: function( classId ) {
            return PublicTransport.UnknownVehicleType;
        }
    }; // End public

    // Add DataTypeProcessor's loaded from included script files
    public.stopSuggestions = __hafas_stopsuggestions( public );
    public.timetable = __hafas_timetable( public );
    public.routeData = __hafas_routedata( public );
    public.journeys = __hafas_journeys( public );

    // Return the public interface
    return public;
}; // Hafas

// HAFAS Formats, static variables
Hafas.BinaryFormat = HafasPrivate.BinaryFormat;
Hafas.XmlFormat = HafasPrivate.XmlFormat;
Hafas.XmlResCFormat = HafasPrivate.XmlResCFormat;
Hafas.JsonFormat = HafasPrivate.JsonFormat;
Hafas.JavaScriptFormat = HafasPrivate.JavaScriptFormat;
Hafas.TextFormat = HafasPrivate.TextFormat;
Hafas.HtmlMobileFormat = HafasPrivate.HtmlMobileFormat;
Hafas.HtmlFormat = HafasPrivate.HtmlFormat;
