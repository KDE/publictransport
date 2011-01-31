/** Accessor for resrobot.se
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ /*'Delay',*/ /*'DelayReason',*/ 'JourneyNews', 'TypeOfVehicle',
	     /*'StopID' "Stop ID"'s parsed in parsePossibleStops are only valid for a session, 
		  * until a new stop suggestion request was made. But these "ID's" are required to get departures. */ ];
}

function parseTimetable( html ) {	
    // Find block of departures
    println( html.indexOf("segmentTable") );
	var str = /<table [^>]*class="segmentTable"[^>]*>([\s\S]*?)<\/table>/i.exec(html);
	if ( str == null ) {
		helper.error( "The table containing departures wasn't found!", html );
		return;
	}
	str = str[1];

    var returnValue = new Array;
	var withDelayCount = 0;
    var timeCol = 3, typeOfVehicleCol = 0, transportLineCol = 1, targetCol = 5, journeyNewsCol = 4;

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr[^>]*>([\s\S]*?)<\/tr>/ig;
    var rowsRegExp = /<tr>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /<img [^>]*src="rp\/img\/mot\/([^\.]*)\.[^"]*"/i;
//     var timeCompleteRegExp = /(\d{2}:\d{2})/i;
    var delayRegExp = /Original\s*time\s*(\d{2}:\d{2})/i;

    // Go through all departure blocks
    while ( (departureRowArr = departuresRegExp.exec(str)) ) {
		departureRow = departureRowArr[1];

		// Get column contents
		var columns = new Array;
		while ( (col = columnsRegExp.exec(departureRow)) )
			columns.push( col[1] );
		columnsRegExp.lastIndex = 0;
		
		if ( columns.length < 5 ) {
			helper.error("Too less columns in a departure row found (" + columns.length + ")", departureRow);
			continue;
		}

		// Initialize result variables with defaults
		var time, typeOfVehicle = "", transportLine, targetString, platformString = "",
			delay = -1, delayReason = "", journeyNews = "", isNightLine = false,
			routeStops = new Array, routeTimes = new Array, exactRouteStops = 0;

		// Parse time column
		time = helper.matchTime( helper.trim(helper.stripTags(columns[timeCol])), "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column", columns[timeCol]);
			continue;
		}
		
		// Parse transport line column
		transportLine = helper.trim( helper.stripTags(columns[transportLineCol]) );

		// Parse type of vehicle column
		if ( (typeOfVehicle = typeOfVehicleRegExp.exec(columns[typeOfVehicleCol])) == null ) {
			println("Unexpected string in type of vehicle column: \"" + columns[typeOfVehicleCol] + "\"! Website layout may have changed.");
			continue; // Unexcepted string in type of vehicle column
		}
		typeOfVehicle = typeOfVehicle[1];
		if ( typeOfVehicle == "J" ) {
			if ( transportLine.indexOf("Sittvagn i Nattåg") != -1 ||
					transportLine.indexOf("Nattåg") != -1 ) 
			{
				isNightLine = true;
			}
			
			if ( transportLine.indexOf("X 2000") != -1 ) {
				typeOfVehicle = "high-speed train";
// 				transportLine = transportLine.substr(7); // Remove the "X 2000" from the beginning
			} else if ( transportLine.indexOf("InterCity") != -1 ) {
				typeOfVehicle = "intercity";
			} else if ( transportLine.indexOf("Arlanda Express") != -1 ) {
				typeOfVehicle = "regional express"; // Arlanda Express = up to 200km/h, link to stockholm airport
			} else {
				typeOfVehicle = "regional"; // regional train
			}
		} else if ( typeOfVehicle == "B" ) {
			typeOfVehicle = "bus";
		} else if ( typeOfVehicle == "S" ) {
			typeOfVehicle = "tram";
		} else if ( typeOfVehicle == "A" ) {
			typeOfVehicle = "plane";
		} else if ( typeOfVehicle == "F" || typeOfVehicle == "K" || typeOfVehicle == "L" ) {
			typeOfVehicle = "ship";
		} else if ( typeOfVehicle == "G" ) {
			typeOfVehicle = "feet";
		} else if ( typeOfVehicle == "C" ) {
			typeOfVehicle = "auto";
		} else if ( typeOfVehicle == "T" ) {
			typeOfVehicle = "taxi"; // ?
		}

		// Parse target column
		targetString = helper.trim( helper.stripTags(columns[targetCol]) );
// 		if ( (targetString = targetRegExp.exec(columns[targetCol])) == null ) {
// 			println("Unexpected string in target column: \"" + columns[targetCol] + "\"! Website layout may have changed.");
// 			continue; // Unexcepted string in target column
// 		}
// 		targetString = helper.trim( helper.stripTags(targetString[1]) );

// 		// .. route
// 		// Go through all route rows
// 		while ( (routeRow = rowsRegExp.exec(routeBlock)) ) {
// 			routeRow = routeRow[1];
// 			
// 			var routeCols = new Array;
// 			while ( (col = columnsRegExp.exec(routeRow)) )
// 				routeCols.push( col[1] );
// 			columnsRegExp.lastIndex = 0;
// 
// 			var routeTime = timeCompleteRegExp.exec( routeCols[0] );
// 			if ( routeTime == null )
// 				continue;
// 			else
// 				routeTime = routeTime[1];
// 			
// 			routeTimes.push( routeTime );
// 			routeStops.push( helper.stripTags(routeCols[1]) );
// 		}

		// Parse journey news column
		var journeyNews = helper.trim( helper.stripTags(columns[journeyNewsCol]) );
		var delayArr = delayRegExp.exec(journeyNews);
		if ( delayArr != null ) {
			var originalTime = delayArr[1];
			delay = helper.duration( originalTime, time[0] + ":" + time[1] );
			
			// Use original time for departure time
			time = helper.matchTime( helper.trim(helper.stripTags(originalTime)), "hh:mm" );
			if ( time.length != 2 ) {
				helper.error("Unexpected error while parsing original departure time.", originalTime);
				continue;
			}
			
			++withDelayCount;
		} else {
			delay = -1; // Unknown delay
		}

		// Add departure
		timetableData.clear();
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		timetableData.set( 'Target', targetString );
		timetableData.set( 'JourneyNews', journeyNews );
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		timetableData.set( 'Delay', delay );
		timetableData.set( 'IsNightLine', isNightLine );
		result.addData( timetableData );
    }

	if ( withDelayCount > 0 ) {
		// Not even sure if the delay computation works (untested, no delayed departures found)
		// so only let the data engine update the data source more often for delays if there was at
		// least one delay found.
		returnValue.push( 'no delays' ); // No delay information available
	}
    return returnValue;
}


function parsePossibleStops( html ) {
    if ( parsePossibleStops_1(html) )
		return true;
//     else if ( parsePossibleStops_2(html) )
// 	return true;
    else
		return false;
}

function parsePossibleStops_1( html ) {
    // Find block of stops
    var str = helper.extractBlock( html, '<select name="searchData/fromIndex" id="from" class="selectfield">', '</select>' );

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<option value="([^"]+)"[^>]*>\s*([^<]*?)\s*(?:\[Stop\]\s*)?<\/option>/ig;

    // Go through all stop options
    while ( (stop = stopRegExp.exec(str)) ) {
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
