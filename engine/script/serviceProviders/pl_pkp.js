/** Service provider rozklad.sitkol.pl (poland).
 * © 2011, Friedrich Pülz */

// This function returns a list of all features supported by this script.
function usedTimetableInformations() {
	return [ 'Platform', 'StopID', 'Delay', 'RouteStops', 'RouteTimes' ];
}

function getTimetable( values ) {
    var url = "http://rozklad.sitkol.pl/bin/stboard.exe/en" +
	"?input=" + values.stop + "!" +
	"&dateBegin=" + helper.formatDateTime(values.dateTime, "dd.MM.yy") +
	"&time=" + helper.formatDateTime(values.dateTime, "hh:mm") +
	"&boardType=" + (values.dataType == "arrivals" ? "arr" : "dep") +
	"&start=Show";

    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );
    network.get( request );
}

function getStopSuggestions( values  ) {
    var url = "http://rozklad.sitkol.pl/bin/stboard.exe/en" +
	"?input=" + values.stop + "?&start=Show";
    var html = network.getSynchronous( url );

    if ( !network.lastDownloadAborted ) {
        // Find all stop suggestions
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
	    result.addData( {StopID: stop[1], StopName: stop[2]} );
	}

	return result.hasData();
    } else {
	return false;
    }
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

    // Initialize regular expressions
    var departureRegExp = /<tr class="(?:dep|arr)board-(?:dark|light)">([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var transportLineRegExp = /<a[^>]*>([\s\S]*?)<\/a>/i;
    var typeOfVehicleRegExp = /<img [^>]*src="\/hafas-res\/img\/([^_]*)_pic.gif"[^>]*>/i;
    var targetRegExp = /<span[^>]*>([\s\S]*?)<\/span>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|&#8226;)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /&#8226;/i;
    var meaningsRegExp = /<table [^>]*class="[^"]*hafasResult(?:\s+[^"]*)?"[^>]*>[\s\S]*?<tr>[\s\S]*?(<th[\s\S]*?)<\/tr>/i;
    var columnMeaningsRegExp = /<th[^>]*?>([\s\S]*?)<\/th>/ig;

    if ( (meanings = meaningsRegExp.exec(html)) == null ) {
        helper.error( "Couldn't find result table header!", html );
        return;
    }
    meanings = meanings[1];

    var timeCol = -1, delayCol = -1, typeOfVehicleCol = -1, transportLineCol = -1,
        targetCol = -1, routeCol = -1, platformCol = -1;
    var i = 0;
    while ( (colMeaning = columnMeaningsRegExp.exec(meanings)) ) {
        colMeaning = helper.trim( colMeaning[1] );

        if ( colMeaning == "Time" ) {
            timeCol = i;
        } else if ( colMeaning == "Prognosis" ) {
            delayCol = i; // TODO Use delay
        } else if ( colMeaning == "Train" ) {
            typeOfVehicleCol = transportLineCol = i;
        } else if ( colMeaning == "Timetable" ) {
            targetCol = routeCol = i;
        } else if ( colMeaning == "Platform" ) {
            platformCol = i;
        } else {
            helper.error( "Unknown column meaning: " + colMeaning, meanings );
        }

        ++i;
    }

    if ( timeCol == -1 || typeOfVehicleCol == -1 || targetCol == -1 || routeCol == -1
        || transportLineCol == -1 )
    {
        helper.error( "Didn't find all required columns in the header! (time: " + timeCol
                      + ", type of vehicle: " + typeOfVehicleCol + ", target: " + targetCol
                      + ", transport line: " + transportLineCol, meanings );
        return;
    }

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
		var departure = { RouteStops: new Array, RouteTimes: new Array, RouteExactStops: 0 };

		// Parse time column
		time = helper.matchTime( helper.trim(helper.stripTags(columns[timeCol])), "hh:mm" );
		if ( time.error ) {
			helper.error("Unexpected string in time column", columns[timeCol]);
			continue;
		}
		departure.DepartureDateTime = new Date(); // FIXME Expect today
		departure.DepartureDateTime.setHours( time.hour, time.minute );

		if ( (typeOfVehicle = typeOfVehicleRegExp.exec(columns[typeOfVehicleCol])) ) {
			departure.TypeOfVehicle = typeOfVehicleFromString( typeOfVehicle[1].toLowerCase() );
		} else {
			helper.error("Unexpected string in type of vehicle column", columns[typeOfVehicleCol]);
			continue;
		}


		// Parse transport line column
		if ( (transportLine = transportLineRegExp.exec(columns[transportLineCol])) ) {
			departure.TransportLine = helper.trim(helper.stripTags(transportLine[1]));
		} else {
			helper.error("Unexpected string in transport line column", columns[transportLineCol]);
			continue;
		}

		// Parse target column
		if ( (targetString = targetRegExp.exec(columns[targetCol])) ) {
			departure.Target = helper.trim( helper.stripTags(targetString[1]) );
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
				departure.RouteExactStops = routeBlocks.length;
			} else {
				while ( (splitter = routeBlocksRegExp.exec(route)) ) {
					++departure.RouteExactStops;
					if ( routeBlockEndOfExactRouteMarkerRegExp.test(splitter) )
						break;
				}
			}

			for ( var n = 0; n < routeBlocks.length; ++n ) {
				var lines = helper.splitSkipEmptyParts( routeBlocks[n], "\n" );
				if ( lines.count < 4 )
					continue;

				departure.RouteStops.push( helper.trim(helper.stripTags(lines[1])) );
				departure.RouteTimes.push( lines[3] );
			}
		}

		// Parse platform column
		departure.Platform = columns.length > platformCol && typeof(columns[platformCol]) != 'undefined'
				? helper.trim( helper.stripTags(columns[platformCol]) ) : "";

		// Parse delay column
		departure.Delay = -1;
		if ( delayCol != -1 ) {
		    var delayString = helper.trim( helper.stripTags(columns[delayCol]) );
		    if ( delayString.length != 0 ) {
			if ( delayString.indexOf("/hafas-res/img/rt_on_time.gif") != -1 ) {
			    departure.Delay = 0;
			} else {
			    var prognosisTime = helper.matchTime( delayString, "hh:mm" );
			    if ( prognosisTime.length == 2 ) {
				// Prognosis time found
				var delayResult = helper.duration( timeString,
					helper.formatTime(prognosisTime[0], prognosisTime[1], "hh:mm"),
					"hh:mm" );
				if ( delayResult < 0 ) {
				    helper.error("Unexpected string in prognosis column!", columns[delayCol]);
				} else {
				    println("parsed delay: " + delayResult + " minutes");
				    departure.Delay = delayResult;
				}
			    }
			}
		    }
		}

		// Add departure to the result set
		result.addData( departure );

		++departureNumber;
	}

	if ( departureNumber == 0 ) {
		helper.error( "Regexp didn't match anything", html );
	}
}

function typeOfVehicleFromString( string ) {
    string = string.toLowerCase();
    if ( string == "trm" ) {
        return "tram";
    } else if ( string == "ic" || string == "eic" || string == "ex" || string == "ec" ) { // Eurocity
        return "intercitytrain";
    } else if ( string == "re" ) {
        return "regionalexpresstrain";
    } else if ( string == "skw" || string == "wkd" || string == "km" ) { // Metronom regional
        return "regionaltrain";
    } else if ( string == "tlk" || string == "ir" ) {
        return "interregionaltrain";	
    } else {
        return string;
    }
}
