/**
 * @projectDescription Contains the DataTypeProcessor used for journey data.
 *
 * @author   Friedrich Pülz fpuelz@gmx.de
 * @version  0.1
 **/

/** Object for journey data. */
var __hafas_journeys = function(hafas) {
    var processor = new HafasPrivate.DataTypeProcessor({
        features: [],

        options: { format: HafasPrivate.BinaryFormat },

        get: function( values, options ) {
            var options = HafasPrivate.prepareOptions( options, processor.options, hafas.options );
            var url = processor.url( values, options );
            var userUrl = processor.userUrl( values, options );
            var request = network.createRequest( url, userUrl );

            request.finished.connect( processor.parser.parserByFormat(options.format) );
            HafasPrivate.startRequest( request, processor.addPostData, values, options );
        },

        addPostData: function( request, values, options ) {
            var postData = '<?xml version="1.0" encoding="' +
                (provider.fallbackCharset != null ? provider.fallbackCharset.toString()
                                                    : 'iso-8859-1') + '"?>';
            postData += '<ReqC ver="1.1" prod="hafas" lang="DE">';
            postData += '<ConReq deliverPolyline="1">';
            postData += '<Start><Station externalId="' + values.originStop + '" />' + //externalId="' + id + ';
                '<Prod prod="' + HafasPrivate.createProductBitString(options.productBits) + '" ' +
                'bike="0" couchette="0" direct="0" sleeper="0" /></Start>';
            postData += '<Dest><Station externalId="' + values.targetStop + '" /></Dest>';
            postData += '<ReqT a="' + (values.dataType == "arrivals" ? '1' : '0') + '" ' +
                'date="' + helper.formatDateTime(values.dateTime, "yyyy.MM.dd") + '" ' +
                'time="' + helper.formatDateTime(values.dateTime, "hh:mm") + '" />';
            postData += '<RFlags b="0" f="' + Math.max(6, values.maxCount) +
                '" chExtension="0" sMode="N" />';
            postData += '</ConReq>';
            postData += '</ReqC>';

            request.setPostData( postData, "utf-8" );
            request.setHeader( "User-Agent", "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:13.0) Gecko/20100101 Firefox/13.0", "utf-8" );
            request.setHeader( "Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8", "utf-8" );
            request.setHeader( "AcceptEncoding", "gzip,deflate", "utf-8" );
            request.setHeader( "Content-Type", "application/json; charset=utf-8", "utf-8" );
        },

        /**
        * Get an URL to journeys.
        *
        * @param {Object} values The "values" object given to the getJourneys() function from the
        *   data engine.
        * @param {Object} [options] An object containing options as properties:
        *   {String} baseUrl The provider specific base URL, until "/bin/..." (excluding).
        *   {String} program The name of the Hafas program to use, the default is "query.exe".
        *   {String} additionalUrlQueryItems Can be used to add additional URL parameters.
        *   {String} dateFormat Can be used to provide an alternative date format,
        *       the default value is "dd.MM.yy". TODO  urlDateFormat
        *   {String} timeForma Can be used to provide an alternative time format,
        *       the default value is "hh:mm".
        *   {String} layout The layout of the requested document, the default is "vs_java3", a short
        *       XML layout.
        * @return The generated URL.
        **/
        url: function( values, options ) {
            var options = HafasPrivate.prepareOptions( HafasPrivate.extend(options, processor.options),
                {program: "query", language: "d", type: "n"}, hafas.options );
            if ( typeof options.baseUrl != "string" || options.baseUrl.length == 0 )
                throw TypeError("getJourneysUrl(): Second argument must be a non-empty string");
            var query =
                "S=" + values.originStop + "!" +
                "&Z=" + values.targetStop + "!" +
                "&date=" + helper.formatDateTime(values.dateTime, options.urlDateFormat) +
                "&time=" + helper.formatDateTime(values.dateTime, options.urlTimeFormat) +
    //     "&REQ0JourneyStopsS0ID=" + values.originStop + "!" +
    //     "&REQ0JourneyStopsZ0ID=" + values.targetStop + "!" +
    //     "&REQ0JourneyDate=" + helper.formatDateTime(values.dateTime, "dd.MM.yy") +
    //     "&REQ0JourneyTime=" + helper.formatDateTime(values.dateTime, "hh:mm") +
                "&REQ0HafasSearchForw=" + (values.dataType == "arrivals" ? "0" : "1") +
                "&REQ0JourneyProduct_prod_list_1=" + HafasPrivate.createProductBitString(options.productBits) +
                "&start=yes" +
                "&sortConnections=minDeparture";
            if ( options.format == HafasPrivate.BinaryFormat ) {
                query += "&h2g-direct=11";
            }
            return HafasPrivate.getUrl( options, query );
        },

        parser: new HafasPrivate.Parser({
            parseBinary: function( data ) {
                if ( data.isEmpty() ) {
                    throw Error("hafas.journeys.parser.parseBinary(): Empty data received");
                }

                var options = HafasPrivate.prepareOptions( undefined, hafas.journeys.options );
                var buffer = new QBuffer( data );
                buffer.open( QIODevice.ReadOnly );
                var stream = new DataStream( buffer );

                // Description of the binary file format:
                //
                // File header:
                // 0x00-0x01: Version (2B),
                // 0x02-0x0f: Origin location block (14B),
                // 0x10-0x1d: Target location block (14B),
                // 0x1e-0x1f: Journey count (2B),
                // 0x20-0x23: Service days position (4B),
                // 0x24-0x27: String table position (4B),
                // 0x28-0x29: Date (2B),
                // 0x2a-0x2b: Unknown date, 30 days after Date (2B),
                // 0x2c-0x2d: Unknown date, 1 day before Date (2B),
                // 0x2e-0x35: Unknown,
                // 0x36-0x39: Stations position (4B),
                // 0x40-0x43: Comments position (4B),
                // 0x44-0x45: Unknown (2B),
                // 0x46-0x49: Extensions header position (4B)
                //
                // Journey header (fixed position, directly after file header, 12B per journey):
                // 0x4a-0x4b: Service days offset, together with the position in file header
                //            it points to the service days block (2B),
                // 0x4c-0x4f: Parts offset (4B),
                // 0x50-0x51: Part count (2B),
                // 0x52-0x53: Number of vehicle changes in this journey (2B),
                // 0x54-0x55: Unknown (2B)
                // 0x56-....: Next journey header
                //
                // Location block (only used in file header):
                // +0x01-0x02: Offset in string table to the name of the location (2B),
                // +0x03-0x04: Unknown (2B),
                // +0x05-0x06: Type (1: station, 2: address, 3: POI, 2B),
                // +0x07-0x0a: Longitude of the stop multiplied with 1 million (integer, 4B),
                // +0x0b-0x0e: Latitude of the stop multiplied with 1 million (integer, 4B)
                //
                // Service days block (position specified in journey and file header):
                // +0x00-0x01: Offset in string table of the service days string (2B),
                // +0x02-0x03: Service bit base, used to calculate a date offset in days (2B),
                // +0x04-0x05: Service bit length, used to calculate a date offset in days (2B),
                // +0x05-....: Service bits, each 1B, number defined by service bit length
                //
                // Stations block (position specified in file header, 14B per stop):
                // +0x00-0x01: Offset in string table, where the name of the stop is stored (2B),
                // +0x02-0x05: ID of the stop (4B),
                // +0x06-0x09: Longitude of the stop multiplied with 1 million (integer, 4B),
                // +0x0a-0x0d: Latitude of the stop multiplied with 1 million (integer, 4B),
                // +0x0e-....: Next station
                //
                // Comments block (position specified in file header and journey part block):
                // +0x01-0x02: Number of comments (2B),
                // +0x03-0x04, ...: Offsets in strings table to the comments (each 2B)
                //
                // Extensions header (position specified in file header, 42B):
                // +0x00-0x03: Length of this header, normally 50B (4B),
                // +0x04-0x07: Unknown (4B),
                // +0x08-0x09: Sequence number, used when requesting more journeys (2B),
                // +0x0a-0x0b: Offset in string table of the Request ID string (2B),
                // +0x0c-0x0f: Journey details position (4B),
                // +0x10-0x11: Error code (2B),
                // +0x12-0x1f: Unknown (14B),
                // +0x20-0x21: Offset in string table of the encoding string (2B),
                // +0x22-0x23: Offset in string table (2B),
                // +0x24-0x25: Attributes offset (2B),
                // +0x2c-0x2f: Attributes position, if extension header length >= 0x30
                //             (otherwise it is 0), at this position attributes indexes
                //             are stored for  all journeys, each 2B. At position attributes
                //             offset +  four times attributes index for a journey,
                //             the offset in the string table to the journey ID can be found (4B)
                //
                // Journey details header (position specified in extension header, 14B):
                // +0x00-0x01: Journey details version, 1 expected (2B),
                // +0x02-0x03: Unknown (2B),
                // +0x04-0x05: Journey details index offset, together with the position
                //             in the extension header, this is the offset of the position
                //             where journey details are stored for all journeys, each 2B (2B),
                // +0x06-0x07: Journey details part offset (2B),
                // +0x08-0x09: Journey details part size, 16 expected (2B),
                // +0x0a-0x0b: Stops size (2B),
                // +0x0c-0x0d: Stops offset (2B)
                //
                // Journey details block (position specified by journey details
                //             and extensions header, 4B per journey):
                // +0x00-0x01: Realtime status, a 2 here means "train is canceled" (2B),
                // +0x02-0x03: Delay (2B)
                //
                // Journey details for parts blocks (position specified by journey details and
                //             extensions header):
                // +0x00-0x01: Predicted departure time (2B),
                // +0x02-0x03: Predicted arrival time (2B),
                // +0x04-0x05: Offset in string table to the predicted departure platform (2B),
                // +0x06-0x07: Offset in string table to the predicted arrival platform (2B),
                // +0x08-0x0b: Unkinown (4B),
                // +0x0c-0x0d: First stop index (2B),
                // +0x0e-0x0f: Number of stops (2B)
                //
                // Stops block (position speficied in journey details header and
                //             journey details for parts blocks):
                // +0x00-0x01: Planned departure time at this stop (2B),
                // +0x02-0x03: Planned arrival time at this stop (2B),
                // +0x04-0x05: Offset in string table to the planned departure platform at this stop (2B),
                // +0x06-0x07: Offset in string table to the planned arrival platform at this stop (2B),
                // +0x08-0x0b: Unknown (4B),
                // +0x0c-0x0d: Predicted departure time at this stop (2B),
                // +0x0e-0x0f: Predicted arrival time at this stop (2B),
                // +0x10-0x11: Offset in string table to the predicted departure platform at this stop (2B),
                // +0x12-0x13: Offset in string table to the predicted arrival platform at this stop (2B),
                // +0x14-0x17: Unknown (4B),
                // +0x18-0x19: Offset in stations table, where data about the stop is stored (2B)
                //
                // Journey part block (position specified in journey header):
                // +0x00-0x01: Planned departure time (2B),
                // +0x02-0x03: Offset in stations table,
                //             where data about the departure stop is stored (2B),
                // +0x04-0x05: Planned arrival time (2B),
                // +0x06-0x07: Offset in stations table,
                //             where data about the arrival stop is stored (2B),
                // +0x08-0x09: Type (1: Footway, 2: Train, Bus, Tram, ...),
                // +0x0a-0x0b: Offset in string table of the transport line string (2B),
                // +0x0c-0x0d: Offset in string table of the planned departure platform (2B),
                // +0x0e-0x0f: Offset in string table of the planned arrival platform (2B),
                // +0x10-0x11: Journey part attribute index (2B),
                // +0x12-0x13: Offset to comments for this journey part (2B)
                //
                // Attributes block (position specified in extensions heaer and journey part block):
                // +0x00-0x01: Offset in string table to a key (eg. "Direction", "Class"),
                //             which specifies the meaning of the following string (2B),
                // +0x02-0x03: Offset in string table to a value, meaning specified in last string (2B),
                // +0x04-....: Next attribute blocks, until the key string is empty

                // Define some helper functions
                var readString = function( stringOffset, encoding ) {
                    if ( stringOffset == 0 ) {
                        return "";
                    } else {
                        var oldPos = stream.pos;
                        stream.seek( stringsPos + stringOffset );

                        var string = encoding == undefined || encoding == ""
                            ? stream.readString()
                            : helper.decode(stream.readBytesUntilZero(), encoding);
                        stream.seek( oldPos );
                        return helper.trim( string );
                    }
                };
                var readNextString = function( encoding ) {
                    var stringOffset = readUInt16();
                    return readString( stringOffset, encoding );
                };
                var readNextByteArray = function() {
                    var stringOffset = readUInt16();
                    if ( stringOffset == 0 ) {
                        return new QByteArray();
                    } else {
                        var oldPos = stream.pos;
                        stream.seek( stringsPos + stringOffset );
                        var bytes = stream.readBytesUntilZero();
                        stream.seek( oldPos );
                        return bytes;
                    }
                };
                var readLocation = function( encoding ) {
                    var placeAndName = readNextString( encoding );
                    stream.skip( 2 );
                    var type = readUInt16();
                    switch ( type ) {
                    case 1: // Station
                    case 2: // Address
                    case 3: // POI
                        break;
                    default:
                        helper.warning("Unknown type " + type);
                        break;
                    }

                    return { StopName: placeAndName,
                        StopLongitude: readUInt32() / 1000000.0,
                        StopLatitude: readUInt32() / 1000000.0 };
                };
                var readStop = function( encoding ) {
                    var index = readUInt16();
                    var position = stationsPos + 14 * index;
                    var oldPos = stream.pos;
                    stream.seek( position );
                    var placeAndName = readNextString( encoding );
                    var stop = { StopName: placeAndName,
                            StopID: readUInt32(),
                            StopLongitude: readUInt32() / 1000000.0,
                            StopLatitude: readUInt32() / 1000000.0 };
                    stream.seek( oldPos );
                    return stop;
                };
                var readDate = function() {
                    var days = readUInt16();
                    var date = new Date( 1980, 0, 1, 0, 0, 0 ).getTime();
                    return date + (days - 1) * 24 * 60 * 60 * 1000;
                };
                var readTime = function( baseDate, dayOffset ) {
                    var value = readUInt16();
                    if ( value == -1 || value > 24 * 100 ) {
                        return 0; // Invalid time
                    }

                    var hour = Math.floor( value / 100 );
                    var minute = value % 100;
                    return baseDate + ((dayOffset * 24 + hour) * 60 + minute) * 60 * 1000;
                };
                var readInt16 = function() {
                    return HafasPrivate.swap16( stream.readInt16() );
                };
                var readUInt16 = function() {
                    return HafasPrivate.swap16( stream.readUInt16() );
                };
                var readInt32 = function() {
                    return HafasPrivate.swap32( stream.readInt32() );
                };
                var readUInt32 = function() {
                    return HafasPrivate.swap32( stream.readUInt32() );
                };
                var seek = function( pos, hint ) {
                    if ( pos >= buffer.size() ) {
                        if ( hint == undefined ) {
                            throw Error("Invalid position " + pos +
                                    ", buffer size is " + buffer.size());
                        } else {
                            throw Error("Invalid position " + pos +
                                    " for '" + hint + "'" +
                                    ", buffer size is " + buffer.size());
                        }
                    }
                    buffer.seek( pos );
                };

                // Start reading, first check the file format version
                var version = readUInt16();
                if ( version != 6 && version != 5 ) {
                    throw Error("Unknown HAFAS binary format version: " + version);
                }

                // Read positions of specific data blocks
                buffer.seek( 0x20 );
                var serviceDaysPos = readUInt32();
                var stringsPos = readUInt32();

                buffer.seek( 0x36 );
                var stationsPos = readUInt32();
                var commentsPos = readUInt32();

                buffer.seek( 0x46 );
                var extensionHeaderPos = readUInt32();

                // Read extension header
                seek( extensionHeaderPos, "Extension Header" );
                var extensionHeaderLength = readUInt32();
                if ( extensionHeaderLength < 0x2c ) {
                    throw Error("hafas.journeys.parser.parseBinary(): " +
                        "Extension header is too short (" + extensionHeaderLength + ")");
                }

                stream.skip( 4 );
                var seqNr = readUInt16();
                var requestId = readNextString();

                var journeyDetailsPos = readUInt32();
                var errorCode = readUInt16();
                if ( errorCode != 0 ) {
                    switch ( errorCode ) {
                    case 1:
                        throw Error("Session expired"); // TEST
                    case 890:
                        // No journeys, proceeding would throw an exception
                        // because journeyDetailsPos is 0
                        helper.warning("No journeys in result set.");
                        buffer.close();
                        return;
                    case 9220:
                        throw Error("Unresolvable address");
                    case 9240:
                        throw Error("Service down");
                    case 9360:
                        throw Error("Invalid date");
                    case 895:
                    case 9380:
                        throw Error("Origin and target stops are too close");
                    default:
                        throw Error("Unknown error code: " + errorCode);
                    }
                }
                if ( journeyDetailsPos == 0 ) {
                    throw Error("No journey details");
                }

                stream.skip( 14 );
                var encoding = readNextByteArray();
                var id = readNextString( encoding );
                var attributesOffset = readUInt16();
                var attributesPos = 0;
                if ( extensionHeaderLength >= 0x30 ) {
                    if ( extensionHeaderLength < 0x32 ) {
                        throw Error("hafas.journeys.parser.parseBinary(): " +
                                "Extension header is too short (" + extensionHeaderLength + ")");
                    }
                    seek( extensionHeaderPos + 0x2c,
                          "Extension Header (Attribute Position)" );
                    attributesPos = readUInt32();
                }

                // Read journey details
                seek( journeyDetailsPos, "Journey Details" );
                var journeyDetailsVersion = readUInt16();
                if ( journeyDetailsVersion != 1 ) {
                    throw Error("Unknown journey details version: " + journeyDetailsVersion);
                }
                stream.skip( 2 );

                var journeyDetailsIndexOffset = readUInt16();
                var journeyDetailsPartOffset = readUInt16();
                var journeyDetailsPartSize = readUInt16();
                if ( journeyDetailsPartSize != 16 ) {
                    throw Error("Unexpected journey details part size: " + journeyDetailsPartSize);
                }
                var stopsSize = readUInt16();
                var stopsOffset = readUInt16();

                // Read header
                buffer.seek( 0x02 );
                var originStop = readLocation();
                var targetStop = readLocation();
                var journeyCount = readUInt16();

                stream.skip( 8 );
                var date = readDate();
                /* var date30 = */ readDate();

                for ( j = 0; j < journeyCount; ++j ) {
                    var journey = {
                        StartStopName: originStop.StopName,
                        TargetStopName: targetStop.StopName,
                        TypesOfVehicleInJourney: [],
                        RouteStops: [],
                        RouteNews: [],
                        RoutePlatformsDeparture: [],
                        RoutePlatformsArrival: [],
                        RouteTypesOfVehicles: [],
                        RouteTransportLines: [],
                        RouteTimesDeparture: [],
                        RouteTimesArrival: [],
                        RouteTimesDepartureDelay: [],
                        RouteTimesArrivalDelay: [],
                        RouteSubJourneys: []
                    };

                    // Read journey header
                    seek( 0x4a + j * 12, "Journey Header " + j );
                    var serviceDaysOffset = readUInt16();
                    var partsOffset = readUInt32();
                    var partCount = readUInt16();

                    // Read changes in this journey
                    journey.Changes = readUInt16();

                    // Read service days text
                    seek( serviceDaysPos + serviceDaysOffset,
                          "Service Days for Journey " + j );
                    journey.JourneyNews = readNextString();

                    // Get journey offset in days
                    var serviceBitBase = readUInt16();
                    var serviceBitLength = readUInt16();
                    var journeyDayOffset = serviceBitBase * 8;
                    for ( n = 0; n < serviceBitLength; ++n ) {
                        var bits = stream.readUInt8(); //serviceBits.at( n );
                        if ( bits == 0 ) {
                            journeyDayOffset += 8;
                            continue;
                        }
                        while ( (bits & 128/* 0x80*/) == 0 ) {
                            bits = bits << 1;
                            ++journeyDayOffset;
                        }
                        break;
                    }

                    // Get offset of details for this journey
                    seek( journeyDetailsPos + journeyDetailsIndexOffset + j * 2,
                          "Journey Details Offset for Journey " + j );
                    var journeyDetailsOffset = readUInt16();

                    // Go to the details and read status
                    seek( journeyDetailsPos + journeyDetailsOffset,
                          "Journey Details for Journey " + j );
                    var realtimeStatus = readUInt16();
                    if ( realtimeStatus == 2 ) {
                        HafasPrivate.appendJourneyNews( journey, "Train is canceled" ); // TODO i18n
                    }

                    // Read delay
                    var delay = readUInt16();
                    journey.Delay = delay;

                    // Read journey ID
                    // TODO journey.JourneyID, add TimetableInformation enumerable
                    var journeyId;
                    if ( attributesPos != 0 && attributesPos + j * 2 < data.length() ) {
                        seek( attributesPos + j * 2,
                              "Attributes Index for Journey " + j );
                        var attributesIndex = readUInt16();
                        seek( attributesOffset + attributesIndex * 4,
                              "Attributes for Journey " + j );
                        while ( true ) {
                            var key = readNextString();
                            if ( key.length == 0 ) {
                                break;
                            } else if ( key == "ConnectionId" ) {
                                journeyId = readNextString();
                            } else {
                                stream.skip( 2 );
                            }
                        }
                    }

                    // Read journey parts and get global departure/arrival date and time
                    for ( p = 0; p < partCount; ++p ) {
                        // Move to the beginning of the data block
                        // for the current part (p) of the current journey (j)
                        seek( 0x4a + partsOffset + p * 20,
                              "Part " + p + " of Journey " + j );

                        // Read departure information
                        var plannedDepartureTime = readTime( date, journeyDayOffset );
                        var departureStop = readStop();
                        if ( p == 0 ) {
                            // First stop, use it's departure time as global departure time
                            // for this journey
                            journey.DepartureDateTime = new Date( plannedDepartureTime );
                        }
                        journey.RouteStops.push( departureStop.StopName );

                        // Read arrival information
                        var plannedArrivalTime = readTime( date, journeyDayOffset );
                        var arrivalStop = readStop();
                        if ( p == partCount - 1 ) {
                            // Last stop, use it's arrival time as global arrival time
                            // for this journey
                            journey.ArrivalDateTime = new Date( plannedArrivalTime );
                        }

                        // Read type, 1: Footway, 2: Train, Bus, Tram, ...
                        var type = readUInt16();
                        var vehicleType = type == 1 ? PublicTransport.Footway
                                                    : PublicTransport.UnknownVehicleType;
                        var vehicleTypeAndTransportLine = readNextString();
                        var pos = vehicleTypeAndTransportLine.indexOf( "#" );
                        var transportLine, vehicleTypeString;
                        if ( pos < 0 ) {
                            transportLine = vehicleTypeString = vehicleTypeAndTransportLine;
                        } else {
                            transportLine = vehicleTypeAndTransportLine.substr( 0, pos );
                            vehicleTypeString = vehicleTypeAndTransportLine.substr( pos + 1 );
                        }
                        transportLine = helper.simplify(
                                transportLine.replace(/^((?:Bus|STR)\s+)/i, "") );

                        journey.RouteTransportLines.push( transportLine );

                        var plannedDeparturePlatform = readNextString();
                        var plannedArrivalPlatform = readNextString();
                        var partAttributeIndex = readUInt16();

                        // Read comments
                        var commentsOffset = readUInt16();
                        seek( commentsPos + commentsOffset,
                              "Comments for Part " + p + " of Journey " + j );
                        var commentCount = readUInt16();
                        var routeNews = "";
                        for ( c = 0; c < commentCount; ++c ) {
                            // Add comment to journey news
                            var comment = readNextString();
                            routeNews += routeNews.length > 0 ? ", \n" + comment : comment;
                        }

                        var attributesForPartPos = attributesOffset + partAttributeIndex * 4;
                        if ( attributesForPartPos < buffer.size() ) {
                            // Attributes are available for the current part
                            seek( attributesForPartPos, "Attributes for Part " + p + " of Journey " + j );
                            var direction = "";
                            var lineClass = 0, category = "";
                            while ( true ) {
                                var key = readNextString(); // Operator, AdminCode, Class, HafasName,
                                    // Category, InternalCat, ParallelTrain.0, Direction, Class, approxDelay
                                if ( key.length == 0 ) {
                                    break;
                                } else if ( key == "Operator" ) {
                                    if ( journey.Operator == undefined ) {
                                        journey.Operator = "";
                                    } else {
                                        journey.Operator += ", ";
                                    }
                                    journey.Operator += readNextString();
                                } else if ( key == "Category" ) {
                                    category = readNextString();
                                    if ( vehicleType == PublicTransport.UnknownVehicleType ) {
                                        vehicleType = hafas.vehicleFromString( category,
                                                {unknownVehicleWarning: false} );
                                    }
                                //     } else if ( key == "Direction" ) { // TODO Rename to Direction or use both Direction and Target?
                                    //         journey.Target = readNextString();
                                } else if ( key == "Class" ) {
                                    lineClass = parseInt( readNextString() );
                                    if ( vehicleType == PublicTransport.UnknownVehicleType ) {
                                        vehicleType = hafas.vehicleFromClass( lineClass,
                                                {unknownVehicleWarning: false} );
                                    }
                                } else {
                                    stream.skip( 2 );
                                }
                            }
                        }

                        if ( vehicleType == PublicTransport.UnknownVehicleType ) {
                            var vehicleTypeRegExp = /^\s*([a-z]+)/i;
                            var vehicleResult = vehicleTypeRegExp.exec( vehicleTypeString );
                            if ( vehicleResult != null ) {
                                vehicleType = hafas.vehicleFromString( vehicleResult[1] );
                            } else {
                                vehicleType = provider.defaultVehicleType;
                            }
                            if ( vehicleType == PublicTransport.UnknownVehicleType ) {
                                helper.warning( "Unknown vehicle type (category: " + category +
                                                ", class: " + lineClass +
                                                ", string: " + vehicleTypeString + ")" );
                            }
                        }
                        journey.RouteTypesOfVehicles.push( vehicleType );
                        journey.TypesOfVehicleInJourney.push( vehicleType );

                        var getPredictedPlatform = function( plannedPlatform, predictedPlatform ) {
                            return predictedPlatform.length == 0
                                    ? plannedPlatform : predictedPlatform;
                        };
                        var getPredictedTime = function( plannedTime, predictedTime ) {
                            return predictedTime == 0 ? plannedTime : predictedTime;
                        };
                        var getDelay = function( plannedTime, predictedTime ) {
                            return predictedTime == 0 ? -1
                                    : (predictedTime - plannedTime) / (60 * 1000);
                        };

                        // Read predicted times/platforms,
                        // seek behind the part data
                        seek( journeyDetailsPos + journeyDetailsOffset +
                              journeyDetailsPartOffset + p * journeyDetailsPartSize,
                              "Data for Part " + p + " of Journey " + j );
                        var predictedDepartureTime = readTime( date, journeyDayOffset );
                        var predictedArrivalTime = readTime( date, journeyDayOffset );
                        var predictedDeparturePlatform = getPredictedPlatform(
                                plannedDeparturePlatform, readNextString() );
                        var predictedArrivalPlatform = getPredictedPlatform(
                                plannedArrivalPlatform, readNextString() );

                        var delayDeparture = getDelay( plannedDepartureTime, predictedDepartureTime );
                        var delayArrival = getDelay( plannedArrivalTime, predictedArrivalTime );
                        predictedDepartureTime =
                                getPredictedTime( plannedDepartureTime, predictedDepartureTime )
                        predictedArrivalTime =
                                getPredictedTime( plannedArrivalTime, predictedArrivalTime )
                        journey.RouteTimesDeparture.push( new Date(plannedDepartureTime) );
                        journey.RouteTimesDepartureDelay.push( delayDeparture );
                        journey.RoutePlatformsDeparture.push( predictedDeparturePlatform );

                        // TODO Add News for changed platform
                        if ( p == 0 ) {
                            var news;
                            if ( plannedDeparturePlatform.length > 0 &&
                                    predictedDeparturePlatform.length > 0 &&
                                    plannedDeparturePlatform != predictedDeparturePlatform )
                            {
                                // First departure platform has changed
                                var news = "Departure platform changed from " +
                                        plannedDeparturePlatform;
                                routeNews += routeNews.length > 0 ? ", " + news : news;
                            }
                        } else if ( p == partCount - 1 ) {
                            if ( plannedArrivalPlatform.length > 0 &&
                                    predictedArrivalPlatform.length > 0 &&
                                    plannedArrivalPlatform != predictedArrivalPlatform)
                            {
                                // Last arrival platform has changed
                                var news = "Arrival platform changed from " +
                                        plannedArrivalPlatform;
                                routeNews += routeNews.length > 0 ? ", " + news : news;
                            }
                        }
                        journey.RouteNews.push( routeNews );

                        // Read intermediate stops of the current journey part
                        stream.skip( 4 );
                        var firstStopIndex = readUInt16();
                        var stopCount = readUInt16();
                        var subJourney = {
                            RouteStops: new Array(stopCount),
                            RouteNews: new Array(stopCount),
                            RoutePlatformsDeparture: new Array(stopCount),
                            RoutePlatformsArrival: new Array(stopCount),
                            RouteTimesDeparture: new Array(stopCount),
                            RouteTimesArrival: new Array(stopCount),
                            RouteTimesDepartureDelay: new Array(stopCount),
                            RouteTimesArrivalDelay: new Array(stopCount),
                        };
                        if ( stopCount > 0 ) {
                            seek( journeyDetailsPos + stopsOffset +
                                  firstStopIndex * stopsSize,
                                  "Intermediate Stops for Part " + p +
                                  " of Journey " + j );
                            if ( stopsSize != 26 ) {
                                throw Error("Unexpected stops size: " + stopsSize);
                            }

                            for ( m = 0; m < stopCount; ++m ) {
                                var plannedStopDepartureTime = readTime(date, journeyDayOffset);
                                var plannedStopArrivalTime = readTime(date, journeyDayOffset);
                                var plannedStopDeparturePlatform = readNextString();
                                var plannedStopArrivalPlatform = readNextString();
                                stream.skip( 4 );

                                var predictedStopDepartureTime = readTime(date, journeyDayOffset);
                                var predictedStopArrivalTime = readTime(date, journeyDayOffset);
                                var predictedStopDeparturePlatform = getPredictedPlatform(
                                        plannedStopDeparturePlatform, readNextString() );
                                var predictedStopArrivalPlatform = getPredictedPlatform(
                                        plannedStopArrivalPlatform, readNextString() );
                                stream.skip( 4 );

                                var delayStopDeparture = getDelay(
                                        plannedStopDepartureTime, predictedStopDepartureTime );
                                var delayStopArrival = getDelay(
                                        plannedStopArrivalTime, predictedStopArrivalTime );
                                predictedStopDepartureTime = getPredictedTime(
                                        plannedStopDepartureTime, predictedStopDepartureTime );
                                predictedStopArrivalTime = getPredictedTime(
                                        plannedStopArrivalTime, predictedStopArrivalTime );

                                var stop = readStop();
                                subJourney.RouteStops[m] = stop.StopName;
                                subJourney.RouteNews[m] = ""; // TODO Check for changed departure/arrival time/platform, ...
                                subJourney.RoutePlatformsDeparture[m] = predictedStopDeparturePlatform;
                                subJourney.RoutePlatformsArrival[m] =  predictedStopArrivalPlatform;
                                subJourney.RouteTimesDeparture[m] = new Date(plannedStopDepartureTime);
                                subJourney.RouteTimesArrival[m] = new Date(plannedStopArrivalTime);
                                subJourney.RouteTimesDepartureDelay[m] = delayStopDeparture;
                                subJourney.RouteTimesArrivalDelay[m] = delayStopArrival;
                            }
                        } // stopCount > 0

                        journey.RouteSubJourneys.push( subJourney );
                        journey.RouteTimesArrival.push( new Date(plannedArrivalTime) );
                        journey.RouteTimesArrivalDelay.push( delayArrival );
                        journey.RoutePlatformsArrival.push( predictedArrivalPlatform );
                    } // for 0 <= p < partCount

                    journey.RouteStops.push( targetStop.StopName );
                    result.addData( journey );
                } // for 0 <= j < journeyCount

                buffer.close();
            },

            parseXml: function( xml ) {
                var options = prepareOptions( undefined, public.journeys.options );
                if ( !expectFormat(_formats.XmlFormat, xml) ) {
                    if ( isHtml(xml) )
                        return public.journeys.parseHtml( xml );
                    return false;
                }
                xml = helper.decode( xml, options.charset(_formats.XmlFormat) );
                xml = fixXml( xml );
                var domDocument = new QDomDocument();
                if ( !domDocument.setContent(xml) ) {
                    helper.error( "Malformed: " + xml );
                    throw Error("Hafas.journeys.parseXml(): Malformed XML");
                }
                var docElement = domDocument.documentElement();
                if ( !checkXmlForErrors(docElement) ) {
                    return false;
                }

                // Find all <Journey> tags, which contain information about a departure/arrival
            //                     var journeyNodes = docElement.elementsByTagName("Journey");
            //                     var count = journeyNodes.count();
            //                     print( "COUNT: " + count );
            //                     for ( var i = 0; i < count; ++i ) {
            //                         var node = journeyNodes.at( i ).toElement();
            //                         var departure = {};
            //
            //                         var date = helper.matchDate( node.attributeNode("fpDate").nodeValue(),
            //                                                      options.xmlDateFormat );
            //                         var time = helper.matchTime( node.attributeNode("fpTime").nodeValue(),
            //                                                      options.xmlTimeFormat  );
            //                         if ( time.error ) {
            //                             helper.error( "Hafas.timetable.parse(): Cannot parse time",
            //                                         node.attributeNode("fpTime").nodeValue() );
            //                             continue;
            //                         }
            //                         date.setHours( time.hour, time.minute, 0, 0 );
            //                         departure.DepartureDateTime = date;
            //                         var target = node.attributeNode("targetLoc").nodeValue();
            //                         departure.Target = node.hasAttribute("dir") ? node.attributeNode("dir").nodeValue() : target;
            //     }
                return true;
            },

            parse: function( js ) { // TODO rename to parseJson() and add format JSON?
                // First check format and cut away the JavaScript code (do not eval it)
                var begin = /^\s*BAHN_MNB\.fm\s*=\s*/.exec( js );
                var end = /;\s*BAHN_MNB\.Callback\(\);\s*$/.exec( js );
                if ( begin == null || end == null) {
                    throw TypeError("Hafas.parseJourneys(): Incorrect journey document received");
                }
                var json = js.substr(begin[0].length, js.length - begin[0].length - end[0].length);
                var options = prepareOptions( undefined, public.journeys.options );
                json = helper.decode( json, options.charset(_formats.JavaScriptFormat) );

                // Parse JSON, expected is an object with these properties
                // describing the found journeys:
                //   "S": The requested start station
                //   "Z": The requested target station
                //   "E": An error string
                //   "fl": A list of found journeys as objects with these properties:
                //        "ab" Departure time,
                //        "an" Arrival time,
                //        "abps" Departure delay,
                //        "abpm" Reason for a departure delay,
                //        "anps" Arrival delay,
                //        "anpm" Reason for an arrival delay,
                //        "d" Duration in format "hh:mm",
                //        "g" Platform,
                //        "u" Number of changes,
                //        "pl" A comma separated string with all products used in the journey, long version
                //        "ps" (useless) A comma separated string with all products used in the journey, short version
                //        "durl", "surl", "zurl" (useless) Some URLs
                var data = JSON.parse( json );
                if ( data.E != undefined ) {
                    throw Error("Hafas.parseJourneys(): " + data.E);
                }

                var journeys = data.fl;
                for ( i = 0; i < journeys.length; ++i ) {
                    var journeyData = journeys[i];
                    var journey = {};

                    var departureTimeValues = helper.matchTime( journeyData.ab, "hh:mm" );
                    var arrivalTimeValues = helper.matchTime( journeyData.an, "hh:mm" );
                    if ( departureTimeValues.error || arrivalTimeValues.error ) {
                        continue;
                    }
                    journey.DepartureTime = new Date;
                    journey.ArrivalTime = new Date;
                    journey.DepartureTime.setHours( departureTimeValues.hour, departureTimeValues.minute, 0 );
                    journey.ArrivalTime.setHours( arrivalTimeValues.hour, arrivalTimeValues.minute, 0 );

                    var productStrings = journeyData.pl.split(",");
                    journey.TypesOfVehicleInJourney = [];
                    for ( p = 0; p = productStrings.length; ++p ) {
                        journey.TypesOfVehicleInJourney.push(
                                hafas.vehicleFromString(productStrings[p]) );
                    }

                    var durationString = journeyData.d;
                    var pos = durationString.indexOf( ':', 0 );
                    journey.Duration = parseInt(durationString.substr(1, pos-1)) * 60 +
                            parseInt(durationString.substr(pos+1, 2));

                    journey.StartStopName = data.S;
                    journey.TargetStopName = data.Z;

                    if ( journeyData.abps == "-" ) {
                        journey.Delay = -1;
                    } else {
                        journey.Delay = parseInt( journeyData.abps ); // anps for arrival delay
                        journey.DelayReason = journeyData.abpm != "-" ? journey.abpm : undefined, // TODO anpm for arrival delay reason
                    }
                    journey.Platform = journeyData.g,
                    journey.Changes = parseInt( journeyData.u );

                    result.addData( journey );
                }
            }
        }) // new Parser
    }); // new DataTypeProcessor

    return processor;
};
