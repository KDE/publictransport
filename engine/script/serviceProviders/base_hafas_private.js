/**
 * @projectDescription Contains HafasPrivate with helping functions.
 *
 * @author   Friedrich Pülz fpuelz@gmx.de
 * @version  0.1
 **/

/** Add a "contains" function to arrays. */
Array.prototype.contains = function( element ) {
    for ( var i = 0; i < this.length; i++ ) {
        if ( this[i] == element )
            return true;
    }
    return false;
};

String.prototype.startsWith = function( prefix ) {
    return this.substr(0, prefix.length) == prefix;
};

String.prototype.endsWith = function( suffix ) {
    return this.substr(this.length - suffix.length) == suffix;
};

function Enum() {
    for ( var i = 0; i < arguments.length; ++i ) {
        this[ arguments[i] ] = i;
    }
};

QByteArray.prototype.string = function() {
//     print( arguments.callee.name + ", called by " + arguments.caller.name );
    ts = new QTextStream( this, QIODevice.ReadOnly );
    return ts.readAll();
};
// function QByteArrayToString() {
// //     print( arguments.callee.name + ", called by " + arguments.caller.name );
//     ts = new QTextStream( this, QIODevice.ReadOnly );
//     return ts.readAll();
// };
//
// if ( QByteArray != undefined && QByteArray.prototype != undefined &&
//      QByteArray.prototype.toString != QByteArrayToString
//      /*typeof(QByteArray.prototype.toString) != 'function'*/ )
// {
//     QByteArray.prototype.toString = QByteArrayToString;
// } else {
//     print( QByteArray.prototype.toString );
// }

var HafasPrivate = {
    // HAFAS Formats
    formats: new Enum(
        "BinaryFormat", // Short binary format
        "XmlFormat", // Short XML format, "layout" should be "vs_java3"
        "XmlResCFormat", // See http://www.rmv.de/xml/hafasXMLStationBoard.dtd
        "JsonFormat", // TODO
        "JavaScriptFormat", // TODO
        "TextFormat", // Text format, "type" should be "l" for Lynx
        "HtmlMobileFormat", // Mobile HTML format, "type" should be "ox"
        "HtmlFormat" // HTML format, "type" should be "n" for normal
    ),

    // Private methods:
    defaultArgument: function( value, defaultValue ) {
        return value == undefined ? defaultValue : value;
    },
    fixAdditionalUrlParameters: function( additionalUrlQueryItems ) {
        if ( typeof(additionalUrlQueryItems) != 'string' ) {
            return "";
        }

        // Prepend a "&" before additional URL parameters if given without a beginning "&"
        return additionalUrlQueryItems.length == 0 || additionalUrlQueryItems[0] != "&"
            ? "&" + additionalUrlQueryItems : additionalUrlQueryItems;
    },
    extend: function( values, defaultValues ) {
        if ( values == undefined ) {
            return defaultValues;
        }
        var _values = HafasPrivate.cloneObject( values );
        for ( var index in defaultValues ) {
            _values[index] = HafasPrivate.defaultArgument( values[index], defaultValues[index] );
        }
        return _values;
    },
    prepareOptions: function( options, defaultOptions, publicOptions ) {
        // Extend with default options, then global options, fix it and return
        var options = HafasPrivate.extend( HafasPrivate.extend(options, defaultOptions),
                                           publicOptions );
        options.additionalUrlQueryItems = HafasPrivate.fixAdditionalUrlParameters(
                options.additionalUrlQueryItems );

        // If a string is given as charset, create a function that returns that string
        // for all formats, except BinaryFormat. Otherwise use latin1 as default charset.
        var charset = typeof(options.charset) == 'string'
                ? options.charset : "iso-8859-1";
        options.charset = function( format ) {
            switch ( format ) {
            case HafasPrivate.formats.XmlFormat:
            case HafasPrivate.formats.XmlResCFormat:
            case HafasPrivate.formats.JsonFormat:
            case HafasPrivate.formats.JavaScriptFormat:
            case HafasPrivate.formats.TextFormat:
            case HafasPrivate.formats.HtmlMobileFormat:
            case HafasPrivate.formats.HtmlFormat:
                return charset;

            case HafasPrivate.formats.BinaryFormat: // Do not decode binary data
            default:
                return undefined;
            }
        }

        switch ( options.format ) {
        case HafasPrivate.formats.XmlFormat:
        case HafasPrivate.formats.JsonFormat:
        case HafasPrivate.formats.JavaScriptFormat:
            if ( options.layout == undefined ) {
                options.layout = "vs_java3";
            }
            if ( options.type == undefined ) {
                options.type = typeof(options.type) == 'string'
                        ? options.type.replace(/(ox|l)/, "n") : "n";
            }
            break;
        case HafasPrivate.formats.TextFormat:
            if ( options.type == undefined ) {
                options.type = typeof(options.type) == 'string'
                        ? options.type.replace(/(ox|n)/, "l") : "l";
            }
            options.layout = undefined;
            break;
        case HafasPrivate.formats.HtmlFormat:
            if ( options.type == undefined ) {
                options.type = typeof(options.type) == 'string'
                        ? options.type.replace(/(ox|l)/, "n") : "n";
            }
            options.layout = undefined;
            break;
        case HafasPrivate.formats.HtmlMobileFormat:
            if ( options.type == undefined ) {
                options.type = typeof(options.type) == 'string'
                        ? options.type.replace(/(n|l)/, "ox") : "ox";
            }
            options.layout = undefined;
            break;
        default:
            break;
        }
        return options;
    },
    getUrl: function( options, query ) {
        query = options.encodeUrlQuery ? QUrl.toPercentEncoding(query).toString() + "" : query;
        return options.baseUrl + "/" + options.binDir + "/" +
            options.program + "." + options.programExtension + "/" +
            options.language + options.type +
            (query.length == 0 ? "" : "?" + query);
    },
    checkXmlForErrors: function( docElement ) {
        // Errors are inside an <Err> tag
        var handleErrElement = function( errElement ) {
            var errCode = errElement.attributeNode("code").nodeValue();
            var errMessage = errElement.attributeNode("text").nodeValue();
            var errLevel = errElement.attributeNode("level").nodeValue();

            // Error codes:
            // - H730: Invalid input
            // - H890: No trains in result (fatal)
            if ( errLevel.toLowerCase() == 'e' ) {
                helper.error( "Hafas fatal error: " + errCode + " " + errMessage +
                              " level: " + errLevel, docElement.text() );
                return false;
            } else {
                helper.warning( "Hafas error: " + errCode + " " + errMessage +
                                " level: " + errLevel, docElement.text() );
                return true;
            }
        };

        if ( docElement.tagName() == "Err" ) {
            return handleErrElement( docElement );
        } else {
            var errNodes = docElement.elementsByTagName( "Err" );
            if ( errNodes.isEmpty() ) {
                return true;
            } else {
                var hasFatalError = false;
                for ( i = 0; i < errNodes.count(); ++i ) {
                    if ( !handleErrElement(errNodes.item(i).toElement()) ) {
                        hasFatalError = true;
                    }
                }
                return !hasFatalError;
            }
        }

        return true;
    },
    createProductBitString: function( productBits ) {
        var count = parseInt( productBits );
        var bitString = "";
        for ( i = 0; i < count; ++i ) {
            bitString += "1";
        }
        return bitString;
    },
    isHtml: function( document ) {
        return document instanceof QByteArray
                ? document.startsWith(new QByteArray("<!DOCTYPE html"))
                : document.startsWith("<!DOCTYPE html");
    },
    isXml: function( document ) {
        return document instanceof QByteArray
                ? document.startsWith(new QByteArray("<?xml"))
                : document.startsWith("<?xml");
    },
    expectFormat: function( format, document ) {
        if ( document == undefined || document.length == 0 ) {
            throw Error("Hafas.timetable.parseXml(): Empty document received");
        } else if ( (format == HafasPrivate.formats.XmlFormat || format == HafasPrivate.formats.XmlResCFormat) &&
                HafasPrivate.isHtml(document) )
        {
            helper.error( "HTML document received, but XML expected.", document );
            return false;
        } else if ( (format == HafasPrivate.formats.HtmlFormat || format == HafasPrivate.formats.HtmlMobileFormat) &&
                HafasPrivate.isXml(document) )
        {
            helper.error( "XML document received, but HTML expected.", document );
            return false;
        } else if ( (format == HafasPrivate.formats.JsonFormat || format == HafasPrivate.formats.JavaScriptFormat) &&
                (HafasPrivate.isHtml(document) || HafasPrivate.isXml(document)) )
        {
            helper.error( "HTML/XML document received, but JSON/JavaScript expected.", document );
            return false;
        } else {
            return true;
        }
    },
    fixXml: function( xml ) {
        if ( HafasPrivate.isXml(xml) ) {
            return xml;
        } else {
            // QDomDocument expects a root element, but there may be only a list of eg. <Journey> elements
            return "<root>" + xml + "</root>";
        }
    },
    notImplemented: function( name ) {
        var func = function() { throw Error(name + "() needs to be implemented by the using script"); }
        func.isNotImplemented = true;
        return func;
    },
    swap16: function( value ) {
        return ((value & 0xff) << 8) | ((value >> 8) & 0xff);
    },
    swap32: function( value ) {
        return ((value & 0xff) << 24) | ((value & 0xff00) << 8)
             | ((value >> 8) & 0xff00) | ((value >> 24) & 0xff);
    },
    appendJourneyNews: function( timetableItem, text, splitText ) {
        splitText = HafasPrivate.defaultArgument( splitText, ", " );
        if ( text.length == 0 ) {
            return;
        } else if ( timetableItem.JourneyNews != undefined ) {
            timetableItem.JourneyNews += splitText + text;
        } else {
            timetableItem.JourneyNews = text;
        }
    },
    /** Remove unneeded information from the transport line string,
      * such as a (redundant) note about the vehicle type. */
    trimTransportLine: function( transportLine ) {
        return helper.simplify( transportLine.replace(/^((?:bus|str)\s+)/ig, "") );
    },
    startRequest: function( request, addPostDataFunction, values, options ) {
        if ( options.requestType == "GET" ) {
            network.get( request, options.timeout );
        } else if ( options.requestType == "POST" ) {
            addPostDataFunction( request, values, options );
            network.post( request, options.timeout );
        }
    },
    /** Clone the given object, because QtScript only uses references to objects. */
    cloneObject: function( object ) {
        var cloned = {};
        for ( property in object ) {
            cloned[ property ] = object[ property ];
        }
        return cloned;
    },

    /** A subclass of Hafas which provides parser functions. */
    Parser: function( publicInterface ) {
        publicInterface = HafasPrivate.extend(publicInterface, {
            /**
            * These functions parse documents in different HAFAS formats.
            *
            * @note parseText(), parseHtmlMobile() and parseHtml() contain much useless text
            *   and cannot be implemented in the Hafas class for all providers, since the
            *   HTML layout differs between providers. Therefore these functions need to
            *   be implemented by the using script if it needs to parse documents in the
            *   associated format.
            *
            * @param {String} document Contents of a timetable document received from
            *   the HAFAS provider.
            * @return {Array} An array of found departures, each with RouteDataUrl, TransportLine,
            *   Target and DepartureDateTime property.
            **/
            parseBinary: HafasPrivate.notImplemented( "parseBinary" ),
            parseXml: HafasPrivate.notImplemented( "parseXml" ),
            parseXmlResC: HafasPrivate.notImplemented( "parseXmlResC" ),
            parseText: HafasPrivate.notImplemented( "parseText" ),
            parseHtmlMobile: HafasPrivate.notImplemented( "parseHtmlMobile" ),
            parseHtml: HafasPrivate.notImplemented( "parseHtml" ),
            parseJson: HafasPrivate.notImplemented( "parseJson" ),
            parseJavaScript: HafasPrivate.notImplemented( "parseJavaScript" ),

            /** Get a parse function that is able to parse documents in the given format. */
            parserByFormat: function( format ) {
                switch ( format ) {
                case Hafas.BinaryFormat:
                    return this.parseBinary;
                case Hafas.XmlFormat:
                    return this.parseXml;
                case Hafas.XmlResCFormat:
                    return this.parseXmlResC;
                case Hafas.JsonFormat:
                    return this.parseJson;
                case Hafas.JavaScriptFormat:
                    return this.parseJavaScript;
                case Hafas.TextFormat:
                    return this.parseText;
                case Hafas.HtmlMobileFormat:
                    return this.parseHtmlMobile;
                case Hafas.HtmlFormat:
                    return this.parseHtml;
                default:
                    return function(){};
                }
            }
        });
        return publicInterface;
    },

    /** A subclass of Hafas for data types, eg. timetable, stopSuggestions, journeys, additionalData. */
    DataTypeProcessor: function( publicInterface ) {
        // Extend interface from the constructor with default interface
        publicInterface = HafasPrivate.extend(publicInterface, {
            features: [],
            options: {},

            get: HafasPrivate.notImplemented( "get" ),
            url: HafasPrivate.notImplemented( "url" ),
            userUrl: function( values, options, clonedOptionsCallback ) {
                var userUrlOptions = HafasPrivate.cloneObject( options );
                userUrlOptions.layout = undefined;
                userUrlOptions.format = Hafas.HtmlFormat;
                if ( typeof(clonedOptionsCallback) == 'function' ) {
                    clonedOptionsCallback( userUrlOptions );
                }
                return publicInterface.url( values, userUrlOptions );
            },
            addPostData: HafasPrivate.notImplemented( "postData" ),
            parser: undefined,

            hasFeature: function( feature ) {
                return this.features.contains( feature );
            },

            addFeature: function( feature ) {
                this.features.push( feature );
            },

            removeFeature: function( feature ) {
                var index = this.features.indexOf( feature );
                if ( index != -1 ) {
                    this.features.splice( index, 1 );
                }
            }
        });
        return publicInterface;
    },

    /**
    * Get the full operator name from it's abbreviation used by HAFAS providers.
    * @param {String} abbr The abbreviation to get an operator name for.
    * @return {String} The full operator name.
    **/
    operatorFromAbbreviation: function( abbr ) {
        abbr = abbr.toLowerCase();
        if ( abbr == "ice" || abbr == "ic" || abbr == "ec" || abbr == "ire" ||
             abbr == "re" || abbr == "rb" || abbr == "cnl" || abbr == "en" ) return "Deutsche Bahn AG";
        else if ( abbr == "tha" ) return "Thalys International";
        else if ( abbr == "est" ) return "Eurostar Group Ltd.";
        else if ( abbr == "rj" ) return "Österreichische Bundesbahnen, railjet";
        else if ( abbr == "me" ) return "metronom Eisenbahngesellschaft mbH";
        else if ( abbr == "mer" ) return "metronom Eisenbahngesellschaft mbH, metronomRegional";
        else if ( abbr == "wfb" ) return "WestfalenBahn GmbH";
        else if ( abbr == "nwb" ) return "NordWestBahn GmbH";
        else if ( abbr == "osb" ) return "Ortenau-S-Bahn GmbH";
        else if ( abbr == "swe" ) return "Südwestdeutsche Verkehrs-AG";
        else if ( abbr == "ktb" ) return "Südwestdeutsche Verkehrs-AG, Kandertalbahn";
        else if ( abbr == "rer" ) return "Réseau Express Régional";
        else if ( abbr == "wkd" ) return "Warszawska Kolej Dojazdowa";
        else if ( abbr == "skm" ) return "Szybka Kolej Miejska Tricity";
        else if ( abbr == "skw" ) return "Szybka Kolej Miejska Warschau";
        else if ( abbr == "erx" ) return "Erixx GmbH";
        else if ( abbr == "hex" ) return "Veolia Verkehr Sachsen-Anhalt GmbH, HarzElbeExpress";
        else if ( abbr == "pe" || abbr == "peg" ) return "Prignitzer Eisenbahn GmbH";
        else if ( abbr == "ne" ) return "NEB Betriebsgesellschaft mbH";
        else if ( abbr == "mrb" ) return "Mitteldeutsche Regiobahn";
        else if ( abbr == "erb" ) return "Keolis Deutschland, eurobahn";
        else if ( abbr == "hlb" ) return "Hessische Landesbahn GmbH";
        else if ( abbr == "hsb" ) return "Harzer Schmalspurbahnen GmbH";
        else if ( abbr == "vbg" ) return "Vogtlandbahn GmbH";
        else if ( abbr == "vx" ) return "Vogtlandbahn GmbH, Vogtland-Express";
        else if ( abbr == "tlx" ) return "Vogtlandbahn GmbH, Trilex";
        else if ( abbr == "alx" ) return "Vogtlandbahn GmbH, alex";
        else if ( abbr == "akn" ) return "AKN Eisenbahn AG";
        else if ( abbr == "ola" ) return "Ostseeland Verkehr GmbH";
        else if ( abbr == "ubb" ) return "Usedomer Bäderbahn GmbH";
        else if ( abbr == "can" ) return "cantus Verkehrsgesellschaft mbH";
        else if ( abbr == "brb" ) return "Abellio Rail NRW GmbH";
        else if ( abbr == "sbb" ) return "Schweizerische Bundesbahnen";
        else if ( abbr == "vec" ) return "vectus Verkehrsgesellschaft mbH";
        else if ( abbr == "hzl" ) return "Hohenzollerische Landesbahn AG";
        else if ( abbr == "abr" ) return "Bayerische Regiobahn GmbH";
        else if ( abbr == "cb" ) return "City Bahn Chemnitz GmbH";
        else if ( abbr == "weg" ) return "Württembergische Eisenbahn-Gesellschaft mbH";
        else if ( abbr == "neb" ) return "Niederbarnimer Eisenbahn AG";
        else if ( abbr == "eb" ) return "Erfurter Bahn GmbH";
        else if ( abbr == "ebx" ) return "Erfurter Bahn GmbH, express";
        else if ( abbr == "ven" ) return "Rhenus Veniro GmbH & Co. KG";
        else if ( abbr == "bob" ) return "Bayerische Oberlandbahn GmbH";
        else if ( abbr == "sbs" ) return "Städtebahn Sachsen GmbH";
        else if ( abbr == "ses" ) return "Städtebahn Sachsen GmbH, express";
        else if ( abbr == "evb" ) return "Eisenbahnen und Verkehrsbetriebe Elbe-Weser GmbH";
        else if ( abbr == "stb" ) return "Süd-Thüringen-Bahn GmbH";
        else if ( abbr == "pre" ) return "Eisenbahn-Bau- und Betriebsgesellschaft Pressnitztalbahn";
        else if ( abbr == "dbg" ) return "Döllnitzbahn GmbH";
        else if ( abbr == "nob" ) return "Nord-Ostsee-Bahn GmbH";
        else if ( abbr == "rtb" ) return "Rurtalbahn GmbH";
        else if ( abbr == "blb" ) return "Berchtesgadener Land Bahn GmbH";
        else if ( abbr == "nbe" ) return "nordbahn Eisenbahngesellschaft mbh & Co. KG";
        else if ( abbr == "soe" ) return "Sächsisch-Oberlausitzer Eisenbahngesellschaft";
        else if ( abbr == "sdg" ) return "Sächsische Dampfeisenbahngesellschaft mbH";
        else if ( abbr == "dab" ) return "Westerwaldbahn GmbH, Daadetalbahn";
        else if ( abbr == "htb" ) return "Hörseltalbahn GmbH";
        else if ( abbr == "feg" ) return "Freiberger Eisenbahngesellschaft mbH";
        else if ( abbr == "neg" ) return "Norddeutsche Eisenbahngesellschaft Niebüll GmbH";
        else if ( abbr == "rbg" ) return "Regentalbahn AG";
        else if ( abbr == "mbb" ) return "Mecklenburgische Bäderbahn Molli";
        else if ( abbr == "veb" ) return "Vulkan-Eifel-Bahn Betriebsgesellschaft mbH";
        else if ( abbr == "msb" ) return "Betriebsgesellschaft Mainschleifenbahn";
        else if ( abbr == "öba" ) return "Öchsle Bahn Betriebs-GmbH";
        else if ( abbr == "wb" ) return "WESTbahn Management GmbH";
        else if ( abbr == "rnv" ) return "Rhein-Neckar-Verkehr GmbH";
        else if ( abbr == "dwe" ) return "Dessauer Verkehrs- und Eisenbahngesellschaft";
        else return undefined;
    },

    vehicleFromOperatorAbbreviation: function ( abbr ) {
        abbr = abbr.toLowerCase();
        switch ( abbr ) {
        case "ice": case "tha": case "rj":
            return PublicTransport.HighSpeedTrain;

        case "ic": case "ec": case "ire": case "cnl":
        case "en": case "est":
            return PublicTransport.IntercityTrain;

        case "re":  case "me":  case "rer": case "vx":
        case "tlx": case "alx": case "ebx": case "ses":
            return PublicTransport.RegionalExpressTrain;

        case "mer": case "rb":  case "wfb": case "nwb":
        case "osb": case "swe": case "ktb": case "wkd":
        case "skm": case "skw": case "erx": case "hex":
        case "pe":  case "peg": case "ne": case "mrb":
        case "erb": case "hlb": case "hsb": case "vbg":
        case "akn": case "ola": case "ubb": case "can":
        case "brb": case "vec": case "hzl": case "abr":
        case "cb":  case "weg": case "neb": case "eb":
        case "ven": case "bob": case "sbs": case "evb":
        case "stb": case "pre": case "dbg": case "nob":
        case "rtb": case "blb": case "nbe": case "soe":
        case "sdg": case "dab": case "htb": case "feg":
        case "neg": case "rbg": case "mbb": case "veb":
        case "msb": case "öba": case "wb": case "rnv":
        case "dwe":
            return PublicTransport.RegionalTrain;

        default:
            return PublicTransport.UnknownVehicleType;
        }
    },

    /**
    * Get the type of vehicle enumerable from a HAFAS vehicle type string.
    * @param {String} string The string to get the associated vehicle type enumerable for.
    * @return {PublicTransport.VehicleType} The vehicle type enumerable associated with the
    *   given string or PublicTransport.UnknownVehicleType if the string cannot be associated
    *   with a vehicle type.
    **/
    vehicleFromString: function( string ) {
        string = string.toLowerCase();

        // Cut away everything after the first word, there may be additional information
        // encoded as a single character and maybe a "+", eg. be_brail has values like
        // "IC J", "IR m" or "IR m".
        var pos = string.indexOf( ' ' );
        if ( pos != -1 ) {
            string = string.substr( 0, pos );
        }

        switch ( string ) {
        case "bus":
        case "buss": // Buss for no_dri TODO Move there
        case "b":
            return PublicTransport.Bus;
        case "tro":
            return PublicTransport.TrolleyBus;
        case "tram":
        case "str":
        case "t":
            return PublicTransport.Tram;
        case "s":
        case "sbahn":
        case "s-bahn":
            return PublicTransport.InterurbanTrain;
        case "u":
        case "ubahn":
        case "u-bahn":
            return PublicTransport.Subway;
        case "met":
        case "metro":
        case "m":
            return PublicTransport.Metro;
        case "ice": // InterCityExpress, Germany
        case "tgv": // Train à grande vitesse, France
        case "eic": // Ekspres InterCity, Poland
        case "rj": // RailJet
            return PublicTransport.HighSpeedTrain;
        case "ic":
        case "ict": // InterCity
        case "icn": // Intercity-Neigezug, Switzerland
        case "ec": // EuroCity
        case "en": // EuroNight
        case "d": // EuroNight, Sitzwagenabteil
        case "cnl": // CityNightLine
        case "int": // International train
            return PublicTransport.IntercityTrain;
        case "re":
        case "me":
        case "alx":
            return PublicTransport.RegionalExpressTrain;
        case "rb":
        case "r":
        case "mer":
        case "zug":
        case "l": // L: local train (be_brail)
        case "cr": // CR: suburban train (be_brail)
        case "ict": // Tourist train
        case "p": // Piekuurtrein/peak-hour trains
            return PublicTransport.RegionalTrain;
        case "ir":
        case "ire":
        case "x": // InterConnex
            return PublicTransport.InterregionalTrain;
        case "ferry":
        case "faehre":
            return PublicTransport.Ferry;
        case "feet":
        case "byfeet":
	case "uebergang":
            return PublicTransport.Feet;

        default:
            return PublicTransport.UnknownVehicleType;
        }
    },

    vehicleFromClass: function( classId ) {
        switch ( classId ) {
        default:
            return PublicTransport.UnknownVehicleType;
        }
    }
}; // HafasPrivate

// HAFAS Formats, static variables
HafasPrivate.BinaryFormat = 0;
HafasPrivate.XmlFormat = 1;
HafasPrivate.XmlResCFormat = 2;
HafasPrivate.JsonFormat = 3;
HafasPrivate.JavaScriptFormat = 4;
HafasPrivate.TextFormat = 5;
HafasPrivate.HtmlMobileFormat = 6;
HafasPrivate.HtmlFormat = 7;
