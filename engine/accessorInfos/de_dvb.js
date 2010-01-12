function stripTags( html ) {
    return html.replace( /<\/?[^>]+>/g, '' );
}

function trim( str ) {
    return str.replace( /^\s+|\s+$/g, '' );
}

var Parser = {
    result: null,
    
    parseTimetable: function( html ) {
	this.result = new Array();
	
	// Find block of departures
	var pos = html.indexOf( '<table class="full">' );
	if ( pos == -1 )
	    return;
	var end = html.indexOf( '</table>', pos + 1 );
	var str = html.substr( pos, end - pos );

	// Initialize regular expressions (compile them only once)
	var departuresRegExp = /<tr class="[^"]*?">([\s\S]*?)<\/tr>/ig;
	var columnsRegExp = /<td>([\s\S]*?)<\/td>/ig;
	var timeRegExp = /(\d{2}):(\d{2})/i;
	var typeOfVehicleRegExp = /<img src="\/images\/design\/pikto_([^\.]*?)\./i;
	var transportLineRegExp = /(\w*\s*\d+)/i;

	// Go through all departure blocks
	while ( (departure = departuresRegExp.exec(str)) ) {
	    departure = departure[1];

	    // Get column contents
	    var columns = new Array;
	    while ( (column = columnsRegExp.exec(departure)) ) {
		column = column[1];
		columns.push( column );
	    }
	    if ( columns.length < 4 )
		continue; // Too less columns

	    // Parse time column
	    var timeValues = timeRegExp.exec( columns[0] );
	    if ( timeValues == null || timeValues.length != 3 )
		continue; // Unexpected string in time column
	    var hour = timeValues[1];
	    var minute = timeValues[2];

	    // Parse type of vehicle column
	    var typeOfVehicle = typeOfVehicleRegExp.exec( columns[1] );
	    if ( typeOfVehicle == null )
		continue; // Unexcepted string in type of vehicle column
	    typeOfVehicle = typeOfVehicle[1];

	    // Parse transport line column
	    var transportLine = transportLineRegExp.exec( columns[2] );
	    if ( transportLine == null )
		continue; // Unexcepted string in transport line column
	    transportLine = transportLine[1];

	    // Parse target column
	    var targetString = trim( stripTags(columns[3]) );

	    // Add departure
	    this.result.push({ 'TransportLine'		: transportLine,
			       'TypeOfVehicle'		: typeOfVehicle,
			       'Target'			: targetString,
			       'DepartureHour'		: hour,
			       'DepartureMinute'	: minute
	    });
	}
    }
}
