/** Accessor for RMV (Rhein-Main), germany.
  * © 2011, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'StopID', 'Delay', 'Platform', 'JourneyNews', 'Operator', 'RouteStops', 'RouteTimes' ];
}

function getTimetable( values ) {
    // Store request date and time to know it in parseTimetable
    requestDateTime = values.dateTime;
    print( "write requestDateTime: " + requestDateTime );

    var url = "http://www.rmv.de/auskunft/bin/jp/stboard.exe/dn?L=vs_rmv.vs_sq" +
            "&selectDate=today" +
            "&time=" + helper.formatDateTime(values.dateTime, "hh:mm") +
            "&input=" + values.stop + "!" +
            "&maxJourneys=" + values.maxCount +
            "&boardType=" + (values.dataType == "arrivals" ? "arr" : "dep") +
            "&productsFilter=1111111111100000&maxStops=1&output=xml&start=yes";

    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );
    network.get( request );
}

function getStopSuggestions( values  ) {
    var url = "http://www.rmv.de/auskunft/bin/jp/stboard.exe/dn?L=vs_rmv.vs_sq" +
            "&input=" + values.stop + "&output=html&start=yes";
    var html = network.getSynchronous( url );

    if ( !network.lastDownloadAborted ) {
        // Find all stop suggestions
        var range = /<select [^>]*id="HFS_input"[^>]*>\s*([\s\S]*?)\s*<\/select>/i.exec( html );
        if ( range == null ) {
            helper.error( "Stop range not found!", html );
            return;
        }
        range = range[1];

        // Initialize regular expression, execute it
        var stopRegExp = /<option value="[^"]+#([0-9]+)">([^<]*?)<\/option>/ig;
        while ( (stop = stopRegExp.exec(range)) ) {
            result.addData( {StopName: stop[2], StopId: stop[1]} );
        }
        return result.hasData();
    } else {
        return false;
    }
}

function parseTimetable( xml ) {
//     var returnValue = new Array; // TODO

    if ( xml.length == 0 ) {
        kDebug() << "XML document is empty";
        return false;
    }

    var domDocument = new QDomDocument();
    domDocument.setContent( xml );
    var docElement = domDocument.documentElement();

    // Errors are inside the <Err> tag
    var errNodes = docElement.elementsByTagName( "Err" );
    if ( !errNodes.isEmpty() ) {
        var errElement = errNodes.item( 0 ).toElement();
        var errCode = errElement.attributeNode("code").nodeValue();
        var errMessage = errElement.attributeNode("text").nodeValue();
        var errLevel = errElement.attributeNode("level").nodeValue();

        // Error codes:
        // - H890: No trains in result (fatal)
        print( "Received an error: " + errCode + " " + errMessage + " level: " + errLevel +
               (errLevel.toLower() == "e" ? "Error is fatal" : "Error isn't fatal") );
        if ( errLevel.toLower() == "e" ) {
            // Error is fatal, don't proceed
            helper.error( "Fatal error: " + errCode + " " + errMessage + " level: " + errLevel );
            return false;
        }
    }

    // Use date of the first departure (inside <StartT>) as date for newly parsed departures.
    // If a departure is more than 3 hours before the last one, it is assumed that the new
    // departure is one day later, eg. read departure one at 23:30, next departure is at 0:45,
    // assume it's at the next day.
    var dateTime;
    var list = docElement.elementsByTagName( "StartT" );
    if ( list.isEmpty() ) {
        helper.error( "No <StartT> tag found in the received XML document", xml );
        return false;
    } else {
        var startTimeElement = docElement.elementsByTagName( "StartT" ).at( 0 ).toElement();
        if ( startTimeElement.hasAttribute("date") ) {
            dateTime = helper.matchDate( startTimeElement.attribute("date"), "yyyyMMdd" );
            if ( dateTime.getFullYear() < 1970 ) {
                dateTime = new Date();
            }
        } else {
            dateTime = new Date();
        }
    }

    // Find all <Journey> tags, which contain information about a departure/arrival
    var journeyNodes = docElement.elementsByTagName("Journey");
    var count = journeyNodes.count();
//     QTime lastTime( 0, 0 );
    for ( var i = 0; i < count; ++i ) {
        var node = journeyNodes.at( i );
        var departure = { JourneyNews: "",
                          RouteStops: new Array(), RouteTimes: new Array(), RouteExactStops: 0 };

        // <Product> tag contains the line string
        departure.TransportLine = helper.trim(
                node.firstChildElement("Product").attributeNode("name").nodeValue() );
//         line = line.remove( "tram", Qt::CaseInsensitive ).trimmed(); TODO

        // <InfoTextList> tag contains <InfoText> tags, which contain journey news
        var journeyNewsNodes = node.firstChildElement("InfoTextList").elementsByTagName("InfoText");
        var journeyNewsCount = journeyNewsNodes.count();
        for ( var j = 0; j < journeyNewsCount; ++j ) {
            var journeyNewsNode = journeyNewsNodes.at( i );
            var newJourneyNews = journeyNewsNode.toElement().attributeNode("text").nodeValue();

            if ( departure.JourneyNews.indexOf(newJourneyNews) == -1 ) {
                if ( !departure.JourneyNews.length == 0 ) {
                    departure.JourneyNews += "<br />"; // Insert line breaks between journey news
                }
                departure.JourneyNews += newJourneyNews;
            }
        }

        // <MainStop><BasicStop><Dep> tag contains some tags with more information
        // about the departing stop
        var stop = node.firstChildElement("MainStop")
                       .firstChildElement("BasicStop").firstChildElement("Dep");

        // <Time> tag contains the departure time
        var time = helper.matchTime( stop.firstChildElement("Time").text(), "hh:mm" );
        if ( time.error ) {
            helper.error( "Unexpected string in Time element", stop.firstChildElement("Time").text() );
            return false;
        }
        dateTime.setHours( time.hour, time.minute, 0 );
        departure.DepartureDateTime = dateTime;

        // <Delay> tag contains delay
        var sDelay = stop.firstChildElement("Delay").text();
        departure.Delay = sDelay.length == 0 ? -1 : parseInt(sDelay);

        // <Platform> tag contains the platform
        departure.Platform = stop.firstChildElement("Platform").text();

        // <PassList> tag contains stops on the route of the line, inside <BasicStop> tags
        var routeStopList = node.firstChildElement("PassList").elementsByTagName("BasicStop");
        var routeStopCount = routeStopList.count();
        for ( var r = 0; r < routeStopCount; ++r ) {
            var routeStop = routeStopList.at( r );

            departure.RouteStops.push(
                    helper.trim(routeStop.firstChildElement("Location").firstChildElement("Station")
                    .firstChildElement("HafasName").firstChildElement("Text").text()) );
            departure.RouteTimes.push( /*QTime::fromString(*/
                    routeStop.firstChildElement("Arr").firstChildElement("Time").text()/*, "hh:mm" )*/
                       );
        }

        // Other information is found in the <JourneyAttributeList> tag which contains
        // a list of <JourneyAttribute> tags
        var journeyAttribute = node.firstChildElement("JourneyAttributeList")
                                   .firstChildElement("JourneyAttribute");
        while ( !journeyAttribute.isNull() ) {
            // Get the child tag <Attribute> and handle it based on the value of the "type" attribute
            var attribute = journeyAttribute.firstChildElement("Attribute");
            if ( attribute.attributeNode("type").nodeValue() == "DIRECTION" ) {
                // Read direction / target
                departure.Target = attribute.firstChildElement("AttributeVariant")
                        .firstChildElement("Text").text();
            } else if ( attribute.attributeNode("type").nodeValue() == "CATEGORY" ) {
                // Read vehicle type from "category"
                var category = attribute.firstChildElement( "AttributeVariant" );
                while ( !category.isNull() ) {
                    if ( category.attributeNode("type").nodeValue() == "NORMAL" ) {
                        departure.TypeOfVehicle =
                                helper.trim( category.firstChildElement("Text").text() );
                        break;
                    }
                    category = category.nextSiblingElement("AttributeVariant");
                }
            } else if ( attribute.attributeNode("type").nodeValue() == "OPERATOR" ) {
                // Read operator
                departure.Operator = attribute.firstChildElement("AttributeVariant")
                        .firstChildElement("Text").text();
            } else if ( typeof(departure.TransportLine) == 'undefined'
                    && attribute.attributeNode("type").nodeValue() == "NAME" )
            {
                // Read line string if it wasn't read already
                departure.TransportLine = helper.trim(
                        attribute.firstChildElement("AttributeVariant").firstChildElement("Text").text() );
            } else if ( attribute.attributeNode("type").nodeValue() == "NORMAL" ) {
                // Read less important journey news
                var info = helper.trim( attribute.firstChildElement("AttributeVariant")
                                                 .firstChildElement("Text").text() );
                if ( departure.JourneyNews.indexOf(info) == -1 ) {
                    if ( typeof(departure.JourneyNews) != 'undefined' &&
                         departure.JourneyNews.length > 0 )
                    {
                        departure.JourneyNews += "<br />";
                    }
                    departure.JourneyNews += info;
                }
            } else {
                print( "Unhandled attribute type " + attribute.attributeNode("type").nodeValue() +
                       " with text (tags stripped) " + attribute.text() );
            }

            // Go to the next journey attribute
            journeyAttribute = journeyAttribute.nextSiblingElement("JourneyAttribute");
        }

//         // Parse time string
//         if ( lastTime.secsTo(time) < -60 * 60 * 3 ) {
//             // Add one day to the departure date
//             // if the last departure time is more than 3 hours before the current departure time
//             currentDate = currentDate.addDays( 1 );
//         }
//         departure.DepartureDate = currentDate;

        // Add departure to the journey list
        result.addData( departure );

//         lastTime = time;
    }

//     TODO result.sort() ?!?

    return count > 0;
}
