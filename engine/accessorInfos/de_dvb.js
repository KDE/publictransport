/** Accessor for www.dvb.de (Dresden, germany).
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle', 'StopID' ];
}

function parseTimetable( json ) {
// 	if ( html.search(/<td [^>]*class="errormessage">[^<]*No trains in this space of time[^<]*<\/td>/i) != -1 ) {
// 		helper.error("Warning: No trains in this space of time");
// 		return;
// 	}
// 	
//     // Find block of departures
//     var pos = html.indexOf( '<table class="full">' );
//     if ( pos == -1 ) {
// 		helper.error("Result table not found (<table class=\"full\">)!", html);
// 		return;
// 	}
//     var end = html.indexOf( '</table>', pos + 1 );
//     var str = html.substr( pos, end - pos );
// 
//     // Initialize regular expressions (compile them only once)
//     var departuresRegExp = /<tr class="[^"]*?">([\s\S]*?)<\/tr>/ig;
//     var columnsRegExp = /<td[^>]*>([\s\S]*?)<\/td>/ig;
//     var typeOfVehicleRegExp = /<img src="\/images\/design\/pikto_([^\.]*?)\./i;
//     var transportLineRegExp = /(\w*\s*\d+)/i;
// 
//     // Go through all departure blocks
//     while ( (departure = departuresRegExp.exec(str)) ) {
// 		departure = departure[1];
// 
// 		// Get column contents
// 		var columns = new Array;
// 		while ( (column = columnsRegExp.exec(departure)) ) {
// 			column = column[1];
// 			columns.push( column );
// 		}
// 		if ( columns.length < 4 ) {
// 			if ( departure.indexOf("sp&#228;ter") == -1 ) {
// 				helper.error("Too less columns (" + columns.length + ") found in a departure row!", departure);
// 			}
// 			continue; // Too less columns
// 		}
// 
// 		// Parse time column
// 		var time = helper.matchTime( columns[0], "hh:mm" );
// 		if ( time.length != 2 ) {
// 			helper.error("Unexpected string in time column!", columns[0]);
// 			continue; // Unexpected string in time column
// 		}
// 
// 		// Parse type of vehicle column
// 		var typeOfVehicle = typeOfVehicleRegExp.exec( columns[1] );
// 		if ( typeOfVehicle == null ) {
// 			helper.error("Unexpected string in type of vehicle column!", columns[1]);
// 			continue; // Unexcepted string in type of vehicle column
// 		}
// 		typeOfVehicle = typeOfVehicle[1];
// 
// 		// Parse transport line column
// 		var transportLine = transportLineRegExp.exec( columns[2] );
// 		if ( transportLine == null ) {
// 			helper.error("Unexpected string in transport line column!", columns[2]);
// 			continue; // Unexcepted string in transport line column
// 		}
// 		transportLine = transportLine[1];
// 		// Parse target column
// 		var targetString = helper.trim( helper.stripTags(columns[3]) );

    var departureRegExp = /\["([^"]*)","([^"]*)","([^"]*)"\]/ig;
	println(json);
    // Go through all departures
	var now = new Date();
    while ( (departure = departureRegExp.exec(json)) ) {
		var minutes = parseInt( departure[3] );
		var timeString = helper.addMinsToTime( now.getHours() + ":" + now.getMinutes(), minutes );
		var time = helper.matchTime( timeString, "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column!", departure[3]);
			continue; // Unexpected string in time column
		}
		
		var vehicleType = "Unknown";
		var line = departure[1];
		if ( line[0] == "S" ) {
			vehicleType = "InterurbanTrain";
		} else {
			var lineNr = parseInt( line );
			if ( lineNr > 0 ) {
				vehicleType = lineNr <= 15 ? "Tram" : "Bus";
			}
		}

		// Add departure
		timetableData.clear();
		timetableData.set( 'TransportLine', departure[1] );
		timetableData.set( 'Target', departure[2] );
		timetableData.set( 'TypeOfVehicle', vehicleType );
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		result.addData( timetableData );
    }
}
// [["10","Striesen","3"],["10","Friedrichstadt","3"],["7","Weixdorf","3"],["S1","Meißen","3"],["S1","Schöna","3"],["66","Lockwitz","4"],["66","Mockritz","4"],["7","Pennrich","5"],["8","Hellerau","8"],["3","Wilder Mann","8"]]

function parsePossibleStops( json ) {
    // Initialize regular expressions (compile them only once)
//     var stopRegExp = /<li>([^<]+)<\/li>/ig; // for old URL / HTML
	var stopRangeRegExp = /\[\[\["[^"]*"\]\],\[(.*)\]\]$/i;
	var range = stopRangeRegExp.exec( json );
	if ( range == null ) {
		helper.error("Stop range not found", json);
		return false;
	}
	range = range[1];
	
// 	[[["Dresden"]],[["Am Trachauer Bahnhof","Dresden","33000211"],["","",""]]]
    var stopRegExp = /\["([^"]*)","[^"]*","([^"]*)"\]/ig;

    // Go through all stop options
    while ( (stop = stopRegExp.exec(range)) ) {
		var stopName = stop[1];
		var stopID = stop[2];

		// Add stop
		timetableData.clear();
		timetableData.set( 'StopName', stopName );
		timetableData.set( 'StopID', stopID );
		result.addData( timetableData );
    }

    return result.hasData();
}
