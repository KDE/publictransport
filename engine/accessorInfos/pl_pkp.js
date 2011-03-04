
// This function returns a list of all features supported by this script.
function usedTimetableInformations() {
	return [ 'Platform', 'StopID', 'RouteStops', 'RouteTimes' ];
}

// This function parses a given HTML document for departure/arrival data.
function parseTimetable( html ) {
	// Find block of departures
	if ( html.search(/<td class=\"errormessage\">Your input is ambiguous. Please choose from the selection list.<\/td>/i) != -1 ) {
		helper.error("No data, only stop suggestions in the HTML document");
		return false;
	} else if ( html.search(/<td [^>]*class="errormessage"[^>]*>[^<]*No trains in this space of time[^<]*<\/td>/) != -1 ) {
		helper.error("No data for the given space of time in the HTML document", html.substr(test, 100));
		return true;
	}

	// Initialize regula<span class="bold">r expressions
	var departureRegExp = /<tr class="depboard-(?:dark|light)">([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
	var transportLineRegExp = /<a[^>]*>([\s\S]*?)<\/a>/i;
	var typeOfVehicleRegExp = /<img src="\/hafas-res\/img\/([^_]*)_pic.gif"[^>]*>/i;
	var targetRegExp = /<span[^>]*>([\s\S]*)<\/span>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|&#8226;)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /&#8226;/i;

    var timeCol = 0, typeOfVehicleCol = 1, transportLineCol = 1, routeCol = 2, targetCol = 2, platformCol = 3;
	
	// Go through all departure blocks
	var departureNumber = 0;
	while ( (departureRow = departureRegExp.exec(html)) ) {
		// This gets the current departure row
		departureRow = departureRow[1];
		
		// Get column contents
		var columns = new Array;
		while ( (col = columnsRegExp.exec(departureRow)) ) {
			columns.push( col[1] );
		}
		columnsRegExp.lastIndex = 0;
		
		if ( columns.length < 3 ) {
			if ( departureRow.indexOf("<th") == -1 ) {
				helper.error("Too lesinterregionals columns in a departure row found (" + columns.length + ") ", departureRow);
			}
			continue;
		}
		
		// Initialize result variables with defaults
		var time = 0, typeOfVehicle = "", transportLine = "", targetString = "", platformString = "",
			routeStops = new Array, routeTimes = new Array, exactRouteStops = 0;
		
		// Parse time column
		time = helper.matchTime( helper.trim(helper.stripTags(columns[timeCol])), "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column", columns[timeCol]);
			continue;
		}
		
		if ( (typeOfVehicle = typeOfVehicleRegExp.exec(columns[typeOfVehicleCol])) ) {
			typeOfVehicle = typeOfVehicle[1].toLowerCase();
		} else {
			helper.error("Unexpected string in type of vehicle column", columns[typeOfVehicleCol]);
			continue;
		}
		
		if ( typeOfVehicle == "skw" || typeOfVehicle == "wkd" || typeOfVehicle == "km" ) {
			typeOfVehicle = "regional";
		} else if ( typeOfVehicle == "tlk" ) {
			typeOfVehicle = "interregional";
		} else if ( typeOfVehicle == "eic" || typeOfVehicle == "ex" ) {
			typeOfVehicle = "intercity";
		} else if ( typeOfVehicle == "trm" ) {
			typeOfVehicle = "tram";
		}
		
		// Parse transport line column
		if ( (transportLine = transportLineRegExp.exec(columns[transportLineCol])) ) {
			transportLine = helper.trim(helper.stripTags(transportLine[1]));
		} else {
			helper.error("Unexpected string in transport line column", columns[transportLineCol]);
			continue;
		}
		
		// Parse target column
		if ( (targetString = targetRegExp.exec(columns[targetCol])) ) {
			targetString = helper.trim( helper.stripTags(targetString[1]) );
		} else {
			helper.error("Unexpected string in target column", columns[targetCol]);
			continue;
		}
		
		// Parse route column
		var route = columns[routeCol];
		var posRoute = route.indexOf( "<br>" );
		if ( posRoute != -1 ) {
			route = route.substr( posRoute + 6 );
			var routeBlocks = route.split( routeBlocksRegExp );

			if ( !routeBlockEndOfExactRouteMarkerRegExp.test(route) ) {
				exactRouteStops = routeBlocks.length;
			} else {
				while ( (splitter = routeBlocksRegExp.exec(route)) ) {
					++exactRouteStops;
					if ( routeBlockEndOfExactRouteMarkerRegExp.test(splitter) )
						break;
				}
			}
			
			for ( var n = 0; n < routeBlocks.length; ++n ) {
				var lines = helper.splitSkipEmptyParts( routeBlocks[n], "\n" );
				if ( lines.count < 4 )
					continue;

				routeStops.push( helper.trim(helper.stripTags(lines[1])) );
				routeTimes.push( lines[3] );
			}
		}
		
		// Parse platform column
		var platformString = columns.length > platformCol && typeof(columns[platformCol]) != 'undefined'
				? helper.trim( helper.stripTags(columns[platformCol]) ) : "";
		
		// Add departure to the result set
		timetableData.clear();
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		timetableData.set( 'Platform', platformString );
		timetableData.set( 'Target', targetString );
		timetableData.set( 'RouteStops', routeStops );
		timetableData.set( 'RouteTimes', routeTimes );
		timetableData.set( 'RouteExactStops', exactRouteStops );
		result.addData( timetableData );
		
		++departureNumber;
	}
	
	if ( departureNumber == 0 ) {
		helper.error( "Regexp didn't match anything", html );
	}
}

// This function parses a given HTML document for stop suggestions.
function parsePossibleStops( html ) {
    // Find block of stops
    var stopRangeRegExp = /<select class="error" name="input"[^>]*>([\s\S]*?)<\/select>/i;
	var range = stopRangeRegExp.exec( html );
	if ( range == null ) {
		helper.error( "Stop range not found", html );
		return false;
	}
	range = range[1];

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<option value="[^"]+#([0-9]+)">([^<]*)<\/option>/ig;

    // Go through all stop options
    while ( (stop = stopRegExp.exec(range)) ) {
		var stopID = stop[1];
		var stopName = stop[2];

		// Add stop
		timetableData.clear();
		timetableData.set( 'StopName', stopName );
		timetableData.set( 'StopID', stopID );
		result.addData( timetableData );
    }

    return result.hasData();
}

