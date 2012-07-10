/** Service provider www.db.de (Deutsche Bahn, germany).
  * © 2012, Friedrich Pülz */

Array.prototype.contains = function( element ) {
    for ( var i = 0; i < this.length; i++ ) {
        if ( this[i] == element )
            return true;
    }
    return false;
}

function usedTimetableInformations() {
    return [ 'Arrivals', 'Delay', 'DelayReason', 'Platform', 'JourneyNews',
            'StopID', 'Pricing', 'Changes', 'Operator', 'RouteStops', 'RouteTimes',
            'RoutePlatformsDeparture', 'RoutePlatformsArrival',
            'RouteTimesDeparture', 'RouteTimesArrival',
            'RouteTypesOfVehicles', 'RouteTransportLines' ];
}

function getTimetable( values ) {
    var url = "http://reiseauskunft.bahn.de/bin/bhftafel.exe/dn?rt=1" +
            "&input=" + values.stop + "!" +
            "&boardType=" + (values.dataType == "arrivals" ? "arr" : "dep") +
            "&date=" + helper.formatDateTime(values.dateTime, "dd.MM.yy") +
            "&time=" + helper.formatDateTime(values.dateTime, "hh:mm") +
            "&maxJourneys=" + values.maxCount +
            "&disableEquivs=no&start=yes&productsFilter=111111111";

    var request = network.createRequest( url );
//     request.readyRead.connect( parseTimetableIterative );
    request.finished.connect( parseTimetable );
    network.get( request );
}

function getJourneys( values ) {
    var url = "http://reiseauskunft.bahn.de/bin/query.exe/dn" +
            "?S=" + values.originStop + "!" +
            "&Z=" + values.targetStop + "!" +
            "&date=" + helper.formatDateTime(values.dateTime, "dd.MM.yy") +
            "&time=" + helper.formatDateTime(values.dateTime, "hh:mm") +
            "&maxJourneys=" + values.maxCount +
            "&REQ0HafasSearchForw=" + (values.dataType == "arrivals" ? "0" : "1") +
            "&start=yes&sortConnections=minDeparture&HWAI=JS!ajax=yes!&HWAI=CONNECTION$C0-0!id=C0-0!HwaiDetailStatus=details!;CONNECTION$C0-1!id=C0-1!HwaiDetailStatus=details!;CONNECTION$C0-2!id=C0-2!HwaiDetailStatus=details!;CONNECTION$C0-3!id=C0-3!HwaiDetailStatus=details!;CONNECTION$C0-4!id=C0-4!HwaiDetailStatus=details!;CONNECTION$C1-0!id=C1-0!HwaiDetailStatus=details!;CONNECTION$C1-1!id=C1-1!HwaiDetailStatus=details!;CONNECTION$C1-2!id=C1-2!HwaiDetailStatus=details!;CONNECTION$C1-3!id=C1-3!HwaiDetailStatus=details!;CONNECTION$C1-4!id=C1-4!HwaiDetailStatus=details!;CONNECTION$C2-0!id=C2-0!HwaiDetailStatus=details!;CONNECTION$C2-1!id=C2-1!HwaiDetailStatus=details!;CONNECTION$C2-2!id=C2-2!HwaiDetailStatus=details!;CONNECTION$C2-3!id=C2-3!HwaiDetailStatus=details!;CONNECTION$C2-4!id=C2-4!HwaiDetailStatus=details!;";

    // Get three journey documents
    for ( i = 0; i < 3; ++i ) {
        // Download and parse journey document
        var html = network.getSynchronous( url );
        if ( network.lastDownloadAborted ) {
            break;
        }
        parseJourneys( html );

        // Directly publish journeys parsed from the current document
        result.publish();

        // Search for another URL to a document with later journeys
        var urlRegExp = /<a href="([^"]*?)" class="arrowlink arrowlinkbottom"[^>]*>Sp&#228;ter<\/a>/i;
        var urlLater = urlRegExp.exec( html );
        if ( urlLater ) {
            // Set next download URL, need to decode "&amp;" to "&"
            url = 'http://reiseauskunft.bahn.de' + urlLater[1].replace( /&amp;/gi, "&" );
        } else {
            // No URL to later journeys found
            break;
        }
    }
}

function getStopSuggestions( values  ) {
    var url = "http://reiseauskunft.bahn.de/bin/ajax-getstop.exe/dn?REQ0JourneyStopsS0A=1" +
            "&REQ0JourneyStopsS0G=" + values.stop;
    var json = network.getSynchronous( url );

    if ( !network.lastDownloadAborted ) {
        // Find all stop suggestions
        if ( json.substr(0, 24) != "SLs.sls={\"suggestions\":[" ) {
	    helper.error( "Incorrect stop suggestions document received", json );
	    return false;
	}
	json = json.substr(24);
	var stopValueRegExp = /"(\w+)":"([^"]+)"/ig;
	var stopIdRegExp = /@L=(\d+)/i;
	var jsonStops = helper.splitSkipEmptyParts( json, "},{" );
	for ( i = 0; i < jsonStops.length; ++i ) {
	    var jsonStop = jsonStops[i];
	    var stop = {};
	    while ( (rx = stopValueRegExp.exec(jsonStop)) ) {
		var name = rx[1];
		var value = rx[2];
		if ( name == "value" ) {
		    stop.StopName = value;
		} else if ( name == "id" ) {
		    var rx = stopIdRegExp.exec( value );
		    if ( rx ) {
		        stop.StopID = rx[1];
		    }
		} else if ( name == "weight" ) {
		    stop.StopWeight = parseInt(value);
		} else if ( name == "xcoord" ) {
		    stop.StopLongitude = parseInt(value) / 1000000;
		} else if ( name == "ycoord" ) {
		    stop.StopLatitude = parseInt(value) / 1000000;
		}
	    }
	    
            result.addData( stop );
	}
        return result.hasData();
    } else {
        return false;
    }
}

function operatorFromTransportLine( string ) {
    var abbreviationRegExp = /^(?:([^\s]+)\s+|([^\d]+)\d+)/i;
    var abbr = abbreviationRegExp.exec( string );
    if ( !abbr ) {
	return undefined;
    }
    abbr = (abbr[1] ? abbr[1] : abbr[2]).toLowerCase();

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
}

function typeOfVehicleFromString( string ) {
    string = string.toLowerCase();
    if ( string == "bus" ) {
        return PublicTransport.Bus;
    } else if ( string == "tro" ) {
        return PublicTransport.TrolleyBus;
    } else if ( string == "tram" ) {
        return PublicTransport.Tram;
    } else if ( string == "sbahn" ) {
        return PublicTransport.InterurbanTrain;
    } else if ( string == "ubahn" ) {
        return PublicTransport.Subway;
    } else if ( string == "met" || string == "metro" ) {
        return PublicTransport.Metro;
    } else if ( string == "ice" || // InterCityExpress, Germany
                string == "eic" ) // Ekspres InterCity, Poland
    {
        return PublicTransport.HighSpeedTrain;
    } else if ( string == "ic" || string == "ict" || // InterCity
                string == "icn" || // Intercity-Neigezug, Switzerland
                string == "ec" || // EuroCity
                string == "en" || // EuroNight
                string == "d" || //  EuroNight, Sitzwagenabteil
                string == "cnl" || //  CityNightLine
                string == "d" )//  EuroNight, Sitzwagenabteil
    {
        return PublicTransport.IntercityTrain;
    } else if ( string == "re"/* || string == "me"*/ /* Metronom, germany */ ) {
        return PublicTransport.RegionalExpressTrain;
    } else if ( string == "rb" || string == "r" || string == "zug" ||
            string == "rt" || // RegioTram
            /*string == "mer" ||*/ // Metronom Regional, Germany
            string == "wfb" || // Westfalenbahn, Germany
            string == "dz" ) // Steam train (Dampfzug), Germany
    {
        return PublicTransport.RegionalTrain;
    } else if ( string == "ir" || string == "ire" ) {
        return PublicTransport.InterregionalTrain;
    } else if ( string == "ferry" || string == "faehre" ) {
        return PublicTransport.Ferry;
    } else if ( string == "feet" || string == "zu fu&szlig;" || string == "zu fu&#223;" ) {
        return PublicTransport.Feet;
    } else {
        helper.error( "Unknown vehicle type", string );
        return PublicTransport.Unknown;
    }
}

function parseTimetable( html ) {
    var returnValue = new Array;
    if ( html.indexOf('<p class="red">F&#252;r diese Haltestelle sind keine aktuellen Informationen verf&#252;gbar.</p>') != -1
        || html.indexOf('<p>Aktuelle Informationen zu Ihrer Reise sind nur 120 Minuten im Voraus m&#246;glich.</p>') != -1 )
    {
        returnValue.push( 'no delays' ); // No delay information available
    }

    // Find block of departures
    var departureBlock = helper.findFirstHtmlTag( html, "table",
            {attributes: {"class": "result\\s+stboard\\s+(dep|arr)"}} );
    if ( !departureBlock.found ) {
        helper.error("Result table not found", html);
        return;
    }

    // Initialize regular expressions (compile them only once)
    var dateTimeRegExp = /<a href="[^\"]*&amp;date=(\d{2}\.\d{2}\.\d{2})[^\"]*&amp;time=(\d{2}:\d{2})[^\"]*"\s*>/i;
    var typeOfVehicleRegExp = /<img src="[^"]*?\/img\/([^_]*?)_/i;
    var targetRegExp = /<span class="bold">[^<]*?<a[^>]*?>([\s\S]*?)<\/a>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|<img[^>]*?>)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /<img[^>]*?>/i;
    var platformRegExp = /<strong>([^<]*?)<\/strong>/i;
    var delayRegExp = /<span style="[^"]*?">ca.&nbsp;(\d*)&nbsp;Minuten&nbsp;sp&#228;ter<\/span>/i;
    var delayWithReasonRegExp = /<span><span style="[^"]*?">ca.&nbsp;(\d*)&nbsp;Minuten&nbsp;sp&#228;ter<\/span><\/span>,<br\/><span class="[^"]*?">Grund:\s*?([^<]*)<\/span>/i;
    var delayOnTimeRegExp = /<span style="[^"]*?">p&#252;nktlich<\/span>/i;
    var trainCanceledRegExp = /<span class="[^"]*?">Zug f&#228;llt aus<\/span>/i;

    var lastDeparture = false;

    // Go through all departure blocks
    var departureRow = { position: -1 };
    while ( (departureRow = helper.findFirstHtmlTag(departureBlock.contents, "tr",
                            {position: departureRow.position + 1})).found )
    {
        // If there is at least one attribute, this row is not a departure/arrival row,
        // it is a row showing other controls/information, eg. a link to go to earlier/later results.
        var hasAttributes = false;
        for ( a in departureRow.attributes ) {
            hasAttributes = true;
            break;
        }
        if ( hasAttributes ) { continue; }

        // Find columns, ie. <td> tags in the current departure row
        var columns = helper.findNamedHtmlTags( departureRow.contents, "td",
                {attributes: {"class": ""}, ambiguousNameResolution: "addNumber",
                 namePosition: {type: "attribute", name: "class", regexp: "([^\\s]*)(?:\\s+\\w+)?"}} );

        // Check that all needed columns are found
        if ( !columns.names.contains("time") || !columns.names.contains("route") ||
             !columns.names.contains("train") || !columns.names.contains("train2") )
        {
            if ( departureRow.contents.indexOf("<th") == -1 ) {
                // There's only an error, if this isn't the header row of the table
                helper.error("Didn't find all columns in a row of the result table! " +
                             "Found: " + columns.names, departureRow.contents);
            }
            continue; // Not all columns where not found
        }

        // Initialize result variables with defaults
        var departure = { RouteStops: new Array, RouteTimes: new Array, RouteExactStops: 0 };

        // Parse date and time from the train URL in the "train" column
        var train = columns["train"].contents;
        dateTimeValues = dateTimeRegExp.exec( train );
        if ( !dateTimeValues ) {
	      helper.error("Unexpected string in train column", columns["train"].contents);
            continue;
        }
        departure.DepartureDateTime = helper.matchDate( dateTimeValues[1], "dd.MM.yy" );
        var time = helper.matchTime( dateTimeValues[2], "hh:mm" );
        departure.DepartureDateTime.setHours( time.hour, time.minute, 0 );

        // Parse type of vehicle column
        if ( !(typeOfVehicle = typeOfVehicleRegExp.exec(columns["train"].contents)) ) {
            helper.error("Unexpected string in type of vehicle column", columns["train"].contents);
            continue; // Unexcepted string in type of vehicle column
        }
        departure.TypeOfVehicle = typeOfVehicleFromString( typeOfVehicle[1] );

        // Parse transport line column
	var transportLine = helper.trim( helper.stripTags(columns["train2"].contents) );
	var operator = operatorFromTransportLine( transportLine );
	if ( operator != undefined ) {
	    departure.Operator = operator;
	}
        departure.TransportLine = transportLine.replace( /^((?:Bus|STR)\s+)/g, "" );

        // Parse route column:
        // > Target information
        if ( !(targetString = targetRegExp.exec(columns["route"].contents)) ) {
            helper.error("Unexpected string in target column", columns["route"].contents);
            continue; // Unexcepted string in target column
        }
        departure.Target = helper.trim( targetString[1] );

        // > Route information
        var route = columns["route"].contents;
        var posRoute = route.indexOf( "<br />" );
        if ( posRoute != -1 ) {
            route = route.substr( posRoute + 6 );
            var routeBlocks = route.split( routeBlocksRegExp );

            if ( !routeBlockEndOfExactRouteMarkerRegExp.test(route) ) {
                departure.RouteExactStops = routeBlocks.length;
            } else {
                while ( (splitter = routeBlocksRegExp.exec(route)) ) {
                    ++departure.RouteExactStops;
                    if ( routeBlockEndOfExactRouteMarkerRegExp.test(splitter) )
                        break;
                }
            }

            for ( var n = 0; n < routeBlocks.length; ++n ) {
                if ( helper.trim(routeBlocks[n]).length <= 1 )
                    continue; // Current routeBlock is the matched split part

                var lines = helper.splitSkipEmptyParts( routeBlocks[n], "\n" );
                if ( lines.count < 2 )
                    continue;

                departure.RouteStops.push( lines[0] );
                departure.RouteTimes.push( lines[1] );
            }
        }

        // Parse platform column if any
        if ( columns.names.contains("platform") ) {
            if ( (platformArr = platformRegExp.exec(columns["platform"].contents)) )
                departure.Platform = helper.trim( helper.stripTags(platformArr[0]) );
        }

        // Parse delay column if any
        if ( columns.names.contains("ris") ) {
            if ( delayOnTimeRegExp.test(columns["ris"].contents) ) {
                departure.Delay = 0;
            } else if ( (delayArr = delayWithReasonRegExp.exec(columns["ris"].contents)) ) {
                departure.Delay = delayArr[1];
                departure.DelayReason = delayArr[2];
            } else if ( (delayArr = delayRegExp.exec(columns["ris"].contents)) ) {
                departure.Delay = delayArr[1];
            } else if ( trainCanceledRegExp.test(columns["ris"].contents) ) {
                var infoText = "Zug f&auml;llt aus.";
                if ( departure.JourneyNews == null ) {
                    departure.JourneyNews = infoText;
                } else {
                    departure.JourneyNews += " " + infoText;
                }
            }
        } else {
            departure.Delay = -1; // Unknown delay
        }

        // Add departure
        print( "Add Data: " + departure.TransportLine + ": " + departure.Operator + " " + departure.Target + " " + departure.DepartureDateTime );
//         result.addData( departure );
	if ( lastDeparture != false &&
	     !compareDepartures(lastDeparture, departure) )
	{
	    result.addData( lastDeparture );
	}
	lastDeparture = departure;

//         if ( result.count % 10 == 0 ) {
//             result.publish( result );
//         }
    }

    if ( lastDeparture != false ) {
      result.addData( lastDeparture );
    }

    print( "Finished script execution" );
    return returnValue;
}

// Returns true, if departure1 and departure2 are equal, except for a longer route stop list
// in departure2, but the route stop list of departure2 must begin with the route stop list
// of departure1
function compareDepartures( departure1, departure2 ) {
    if ( departure1.DepartureDateTime.getTime() == departure2.DepartureDateTime.getTime() &&
         departure1.TypeOfVehicle == departure2.TypeOfVehicle &&
         departure1.TransportLine == departure2.TransportLine &&
         departure1.Platform == departure2.Platform &&
         departure1.RouteStops.length < departure2.RouteStops.length )
    {
        for ( var i = 0; i < departure1.RouteStops.length; ++i ) {
            if ( departure1.RouteStops[i] != departure2.RouteStops[i] ||
                 (departure1.RouteTimes.length > i && departure2.RouteTimes.length > i &&
                  departure1.RouteTimes[i] != departure2.RouteTimes[i]) )
            {
                return false;
            }
        }
        print( "Equal departures found" );
        return true;
    }


    return false;
}

function parseJourneys( html ) {
    // Find block of journeys
    var journeyBlock = helper.findFirstHtmlTag( html, "table",
            {attributes: {"class": "result"}} );
    if ( !journeyBlock.found ) {
        helper.error("Result table not found", html);
        return;
    }

    // Initialize regular expressions (compile them only once)
    var nextJourneyBeginRegExp = /<tr class="\s*?firstrow">/i;
    var startStopNameRegExp = /<div class="resultDep">\s*([^<]*?)\s*<\/div>/i;
    var timeRegExp = /(\d{1,2}):(\d{2})/i;
    var dateRegExp = /(\d{2}).(\d{2}).(\d{2,4})/;
    var vehicleTypesRegExp = /<span[^>]*?><a[^>]*?>(?:[^<]*?<img src="[^"]*?"[^>]*?>)?([^<]*?)<\/a><\/span>/i;
    var journeyDetailsRegExp = /<div id="[^"]*?" class="detailContainer[^"]*?">\s*?<table class="result">([\s\S]*?)<\/table>/ig;

    // Go through all journey blocks (each block consists of two table rows (<tr> elements)
    var journeyRow1, journeyRow2 = { endPosition: -1 };
    while ( (journeyRow1 = helper.findFirstHtmlTag(journeyBlock.contents, "tr",
                {attributes: {"class": "firstrow"}, position: journeyRow2.endPosition + 1})).found &&
            (journeyRow2 = helper.findFirstHtmlTag(journeyBlock.contents, "tr",
                {attributes: {"class": "last"}, position: journeyRow1.endPosition + 1})).found )
    {
        // Find columns, ie. <td> tags in the current journey rows
        var columnsRow1 = helper.findNamedHtmlTags( journeyRow1.contents, "td",
                {attributes: {"class": ""}, ambiguousNameResolution: "addNumber",
                 namePosition: {type: "attribute", name: "class", regexp: "([^\\s]*)(?:\\s+\\w+)?"}} );
        var columnsRow2 = helper.findNamedHtmlTags( journeyRow2.contents, "td",
                {attributes: {"class": ""}, ambiguousNameResolution: "addNumber",
                 namePosition: {type: "attribute", name: "class", regexp: "([^\\s]*)(?:\\s+\\w+)?"}} );

        if ( !columnsRow1.names.contains("time") || !columnsRow1.names.contains("duration") ||
             !columnsRow1.names.contains("station") || !columnsRow2.names.contains("station") )
        {
            helper.error("Did not find all required columns",
                         journeyRow1.contents + "  -  " + journeyRow2.contents);
            continue;
        }

        // Initialize result variables with defaults
        var journey = { TypesOfVehicleInJourney: new Array, JourneyNews: '', RouteStops: new Array,
                        RoutePlatformsDeparture: new Array, RoutePlatformsArrival: new Array,
                        RouteTypesOfVehicles: new Array, RouteTransportLines: new Array,
                        RouteTimesDeparture: new Array, RouteTimesArrival: new Array,
                        RouteTimesDepartureDelay: new Array, RouteTimesArrivalDelay: new Array };

        // Parse start/target stop column
        if ( !(startStopName = startStopNameRegExp.exec(columnsRow1["station"].contents)) ) {
            helper.error("Unexcepted string in start stop column", columnsRow1["station"].contents);
            continue;
        }
        journey.StartStopName = startStopName[1];
        journey.TargetStopName = helper.trim( columnsRow2["station"].contents );

        // Parse departure/arrival date column TODO
//         journey.DepartureDateTime = helper.matchDate( columns["dateRow1"], "dd.MM.yy" );
//         journey.ArrivalDateTime = helper.matchDate( columns["dateRow2"], "dd.MM.yy" );
        if ( !(departureDate = dateRegExp.exec(columnsRow1["date"].contents)) ) {
            helper.error("Unexcepted string in departure date column", columnsRow1["date"].contents);
            continue;
        }
        var departureYear = parseInt( departureDate[3] );
        journey.DepartureDate = new Date( departureYear < 100 ? departureYear + 2000 : departureYear,
                                          parseInt(departureDate[2]) - 1, // month is zero based
                                          parseInt(departureDate[1]) );

        if ( !(arrivalDate = dateRegExp.exec(columnsRow2["date"].contents)) ) {
            helper.error("Unexcepted string in arrival date column", columnsRow2["date"].contents);
            continue;
        }
        var arrivalYear = parseInt( arrivalDate[3] );
        journey.ArrivalDate = new Date( arrivalYear < 100 ? arrivalYear + 2000 : arrivalYear,
                                        parseInt(arrivalDate[2]) - 1, // month is zero based
                                        parseInt(arrivalDate[1]) );

        // Parse departure time column // TODO parse delay
//         var departureTime = columns["timeRow1"];
//         var pos = departureTime.indexOf("\n");
//         if ( pos > 0 ) {
//             // Cut until line break
//             departureTime = departureTime.substr( 0, pos );
//         }
//         journey.DepartureTime = QTime.fromString( helper.trim(departureTime), "hh:mm" );
        if ( !(departureTime = timeRegExp.exec(columnsRow1["time"].contents)) ) {
//         if ( !journey.DepartureTime.isValid() ) {
            helper.error("Unexcepted string in departure time column", columnsRow1["time"].contents);
            continue;
        }
        journey.DepartureTime = new QTime( departureTime[1], departureTime[2] );

        if ( !(arrivalTime = timeRegExp.exec(columnsRow2["time"].contents)) ) {
//         if ( !journey.DepartureTime.isValid() ) {
            helper.error("Unexcepted string in arrival time column", columnsRow2["time"].contents);
            continue;
        }
        journey.ArrivalTime = new QTime( arrivalTime[1], arrivalTime[2] );

//         // Parse arrival time column // TODO parse delay
//         var arrivalTime = columns["timeRow2"];
//         var pos = arrivalTime.indexOf("\n");
//         if ( pos > 0 ) {
//             // Cut until line break
//             arrivalTime = arrivalTime.substr( 0, pos );
//         }
//         journey.ArrivalTime = QTime.fromString( helper.trim(arrivalTime), "hh:mm" );
//         if ( !journey.ArrivalTime.isValid() ) {
//             helper.error("Unexcepted string in arrival time column", columns["timeRow2"]);
//             continue;
//         }

        // Parse duration column
        if ( columnsRow1.names.contains("duration") ) {
            if ( (duration = timeRegExp.exec(columnsRow1["duration"].contents)) ) {
                // Duration is a string in format "hh:mm"
                journey.Duration = duration[1] * 60 + duration[2];
            }
        }

        // Parse types of vehicles (products) column
        if ( columnsRow1.names.contains("products") ) {
            var types = vehicleTypesRegExp.exec( columnsRow1["products"].contents );
            if ( types ) {
                types = types[1];
                types = types.split( /,/i );
                for ( var n = 0; n < types.length; ++n ) {
                    journey.TypesOfVehicleInJourney.push( typeOfVehicleFromString(helper.trim(types[n])) );
                }
            } else {
                // This is changed, test it TODO
                types = helper.stripTags( columnsRow1["products"].contents ).split( /,/i );
                for ( var n = 0; n < types.length; ++n ) {
                    journey.TypesOfVehicleInJourney.push( typeOfVehicleFromString(helper.trim(types[n])) );
                }
            }
        }

        // Parse changes column
        journey.Changes = parseInt( helper.trim(columnsRow1["changes"].contents) );

        // Parse pricing column
        if ( columnsRow1.names.contains("fareStd") ) {
            journey.Pricing = helper.trim( helper.stripTags(columnsRow1["fareStd"].contents) );
            journey.Pricing = journey.Pricing.replace( /(Zur&nbsp;Buchung|Zur&nbsp;Reservierung|\(Res. Info\)|Preisauskunft nicht m&#246;glich)/ig, '' );
        }

        // Parse details if available
        var str = journeyBlock.contents.substr( journeyRow2.endPosition );
        var details = journeyDetailsRegExp.exec( str );
        // Make sure that the details block belongs to the just parsed journey
        var beginOfNextJourney = str.search( nextJourneyBeginRegExp );
        if ( details &&
             (beginOfNextJourney < 0 || journeyDetailsRegExp.lastIndex <= beginOfNextJourney) )
        {
            parseJourneyDetails( details[1], journey );
        }
        journeyDetailsRegExp.lastIndex = 0; // start from beginning next time

        // Add journey
        result.addData( journey );
    }
}

function parseJourneyDetails( details, resultObject ) {
    var journeyNews = null;
    var journeyNewsRegExp = /<tr>\s*?<td colspan="8" class="red bold"[^>]*?>([\s\S]*?)<\/td>\s*?<\/tr>/i;
    if ( (journeyNews = journeyNewsRegExp.exec(details)) != null ) {
        journeyNews = helper.trim( journeyNews[1] );
        if ( journeyNews != '' ) {
            resultObject.JourneyNews = journeyNews;
        }
    }

    var headerEndRegExp = /<tr>\s*?<th>[\s\S]*?<\/tr>/ig;
    if ( !headerEndRegExp.exec(details) ) {
        return;
    }
    var str = details.substr( headerEndRegExp.lastIndex );

    var lastStop = null, lastArrivalTime = '';
    var detailsBlockRegExp = /<tr>([\s\S]*?)<\/tr>\s*?<tr class="last">([\s\S]*?)<\/tr>/ig;
    var columnsContentsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var timeCompleteRegExp = /(\d{2}:\d{2})/i;
    var delayRegExp = /<span[^>]*?>ca.&nbsp;\+(\d+?)&nbsp;Min.&nbsp;<\/span>/i;
    var delayOnScheduleRegExp = /<span[^>]*?>p&#252;nktlich<\/span>/i;
    var trainCanceledRegExp = /<span[^>]*?>Zug f&#228;llt aus<\/span>/i;

    while ( (detailsBlock = detailsBlockRegExp.exec(str)) ) {
        detailsBlockRow1 = detailsBlock[1];
        detailsBlockRow2 = detailsBlock[2];

        var columnsRow1 = new Array;
        var columnsRow2 = new Array;
        while ( (col = columnsContentsRegExp.exec(detailsBlockRow1)) )
            columnsRow1.push( col[1] );
        while ( (col = columnsContentsRegExp.exec(detailsBlockRow2)) )
            columnsRow2.push( col[1] );

        if ( columnsRow1.count < 4 )
            break;

        var routeStop = helper.trim( columnsRow1[0] );

        // Search for the vehicle type in the transport line column
        // (including html entities, ie. '&#0245;' or '&uuml;')
        var routeVehicleType = /(([abcdefghijklmnopqrstuvwxyz]|&#\d{2,5};|&\w{2,5};)+)/i.exec(
            helper.stripTags(columnsRow1[5]) );
        if ( routeVehicleType != null )
            routeVehicleType = typeOfVehicleFromString( helper.trim(routeVehicleType[1]) );

        var routePlatformDeparture = helper.trim( helper.stripTags(columnsRow1[4]) );
        var routePlatformArrival = helper.trim( helper.stripTags(columnsRow2[4]) );

        var routeTransportLine;
        var separatorPos = columnsRow1[5].search( /<br \/>/i );
        if ( separatorPos <= 0 ) {
            routeTransportLine = helper.trim( helper.stripTags(columnsRow1[5]) );
        } else {
            // When the line of the vehicle changes during the journey,
            // both lines are written into the column, separated by <br />
            var line1 = columnsRow1[5].substr( 0, separatorPos );
            var line2 = columnsRow1[5].substr( separatorPos );
            routeTransportLine = helper.trim( helper.stripTags(line1) ) + ", "
                + helper.trim( helper.stripTags(line2) );
        }

        // Infos about canceled trains are given in the
        // departure times column for some reason...
        if ( trainCanceledRegExp.test(columnsRow1[3]) ) {
            var infoText = "Zug '" + routeTransportLine + "' f&auml;llt aus.";
            if ( resultObject.JourneyNews == null )
                resultObject.JourneyNews = infoText;
            else
                resultObject.JourneyNews += " " + infoText;

            routeTransportLine += " <span style='color:red;'>Zug f&auml;llt aus</span>";
        }

        // If there's no departure time given, use the last arrival
        // as departure (eg. when vehicle type is Feet).
        var timeDeparture = timeCompleteRegExp.exec( columnsRow1[3] );
        if ( timeDeparture != null )
            timeDeparture = timeDeparture[1];
        else
            timeDeparture = lastArrivalTime;

        var delayDeparture = delayRegExp.exec( columnsRow1[3] );
        if ( delayDeparture != null )
            delayDeparture = delayDeparture[1];
        else if ( delayOnScheduleRegExp.test(columnsRow1[3]) )
            delayDeparture = 0;
        else
            delayDeparture = -1;

        var delayArrival = delayRegExp.exec( columnsRow2[3] );
        if ( delayArrival != null )
            delayArrival = delayArrival[1];
        else if ( delayOnScheduleRegExp.test(columnsRow2[3]) )
            delayArrival = 0;
        else
            delayArrival = -1;

        // If there's no arrival time given try to find the duration
        // in the notes column and add it to the departure time (used for footways)
        var timeArrival = timeCompleteRegExp.exec( columnsRow2[3] );
        if ( timeArrival )
            timeArrival = timeArrival[1];
        else {
            var timeArrivalParts = helper.matchTime( columnsRow2[3], "hh:mm" );
            var timeDepartureParts = helper.matchTime( timeDeparture, "hh:mm" );

            if ( !timeArrivalParts.error && !timeDepartureParts.error ) {
                var durationPart = /(\d*?) Min./i.exec( columnsRow1[6] );
                if ( durationPart && parseInt(durationPart[1]) > 0 ) {
                    timeArrival = helper.addMinsToTime( timeDeparture,
                            parseInt(durationPart[1]), "hh:mm" );
                }
            }
        }

        resultObject.RouteStops.push( routeStop );
        resultObject.RoutePlatformsDeparture.push( routePlatformDeparture );
        resultObject.RoutePlatformsArrival.push( routePlatformArrival );
        resultObject.RouteTypesOfVehicles.push( routeVehicleType );
        resultObject.RouteTransportLines.push( routeTransportLine );
        resultObject.RouteTimesDeparture.push( timeDeparture );
        resultObject.RouteTimesDepartureDelay.push( delayDeparture );
        resultObject.RouteTimesArrival.push( timeArrival );
        resultObject.RouteTimesArrivalDelay.push( delayArrival );

        lastStop = columnsRow2[0];
        lastArrivalTime = timeArrival;
    }

    if ( lastStop != null ) {
        resultObject.RouteStops.push( helper.trim(lastStop) );
    }
    return true;
}
