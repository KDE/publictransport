/** Accessor for RMV (Rhein-Main), germany.
  * © 2011, Friedrich Pülz */

// function usedTimetableInformations() {
//     return [ 'StopID' ];
// }

function parseStopSuggestions( html ) {
    var range = /<select [^>]*id="HFS_input"[^>]*>\s*([\s\S]*?)\s*<\/select>/i.exec( html );
    if ( range == null ) {
		helper.error( "Stop range not found!", html );
		return;
	}
	range = range[1];

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<option value="[^"]+#([0-9]+)">([^<]*?)<\/option>/ig;

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
