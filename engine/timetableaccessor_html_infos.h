/*
 *   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/** @file
* The information mainly contains raw urls and regular expressions used by
* TimetableAccessor. The raw urls are used to construct a real url that points
* to a document that can then be downloaded and parsed using a regular expression.
*
* If you want to add a new accessor for a new service provider, you need to add
* a new class to timetableaccessor_html_infos.h. Here is an example of how this
* new class can look like:
* @code
// Replace X in the class name with an abbreviation of the service Provider
// (also do this for the constructor):
class TimetableAccessorInfo_X : public TimetableAccessorInfo
{

    // TODO
    TimetableAccessorInfo_X() : TimetableAccessorInfo("name of the accessor, to be displayed in the plasmoid",
	"a short version of the url", "the authors name", "a description of the service provider",
	// Add a new value to the ServiceProvider-enumeration (timetableaccessor.h) that
	// is uniquely assocciated with the service Provider and insert it here:
	,
	// insert HTML or XML here )
    {
	setDescription(i18n([description of the accessor]));

	rawUrl = "[insert the url here, replace the stop name with %1 (or the city with %1 and the stop with %2)]";
	setRegExpJourneys( "[regexp for parsing the html document for a list of journeys]",
	    QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Direction
	    << [any TimetableInformation-value matched. The first regexp-match is assocciated with the first TimetableInformation-value in this list and so on.] );

	country = "country"; // Insert the country for which the service provider provides results
	cities << "city1" << "city2"; // Insert the cities for which the service provider provides results
	useSeperateCityValue = false; // default is false. If the url needs to have a seperate city value set it to true (and use %1 and %2 in rawUrl)

	// Optionally, if you want to get autocompletion for stop input
	setRegExpPossibleStops( "[regexp for matching a substring of the document containing the list of possible stops,
	    e.g. between <select> and </select>]",
	    "[regexp for getting stop information from the matched substring]",
	    QList< TimetableInformation >() << StopName [maybe you also want to use StopID] );

	// Optionally, if you have set JourneyNews as a meaning of a matched string in setRegExpJourneys()
	// and want the matched string to get parsed (you can use JourneyNewsOther if you never want that
	// matched string being extra-parsed)
	addRegExpJouneyNews( "[regexp for getting information out of the 'JourneyNews-matched string']",
	    QList< TimetableInformation >() << [whatever information is matched in the matched string,
	    e.g. Delay, DelayReason, JourneyNewsOther] );
    }
};
* @endcode
* @brief This file contains informations for parsing HTML documents from the service providers. */

#ifndef TIMETABLEACCESSOR_HTML_INFOS_HEADER
#define TIMETABLEACCESSOR_HTML_INFOS_HEADER

#include "timetableaccessor_htmlinfo.h"
#include <KLocalizedString>

class TimetableAccessorInfo_Bvg : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Bvg() : TimetableAccessorInfo(i18n("Berlin (bvg.de)"), "bvg.de", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", BVG, HTML)
	{
	    setDescription(i18n("Provides departure information for Berlin in Germany."));
	    setUrl("http://www.bvg.de/");

	    setDepartureRawUrl( "http://www.fahrinfo-berlin.de/IstAbfahrtzeiten/index;ref=3?input=%1&submit=Anzeigen" );
	    setRegExpDepartures( "(?:<tr class=\"\">\\s*<td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*</td>\\s*<td>\\s*<img src=\".*\" class=\"ivuTDProductPicture\" alt=\".*\"\\s*class=\"ivuTDProductPicture\" />)(\\w{1,10})(?:\\s*)((\\w*\\s*)?[0-9]+)(?:\\s*</td>\\s*<td>\\s*<a class=\"ivuLink\" href=\".*\" title=\".*\">)(.*)(?:</a>\\s*</td>\\s*<td>\\s*<!-- .* -->\\s*<a class=\"ivuLink\" href=\".*\" title=\".*\">[0-9]+</a>\\s*</td>\\s*</tr>)", QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Target );

	    addRegExpPossibleStops( "(?:<select name=\"[^\"]*\" size=\"[^\"]*\" class=\"ivuSelectBox\">\\s*)(.*)(?:\\s*</select>)", "(?:<option value=\"[^\"]*\">\\s*)([^\\s][^<]*[^\\s])(?:\\s*</option>)", QList< TimetableInformation >() << StopName );

	    setCountry( "Germany" );
	    setCities( QStringList() << "Berlin" );
	    setUseSeperateCityValue( false );
	}
};

class TimetableAccessorInfo_Dvb : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Dvb() : TimetableAccessorInfo(i18n("Dresden (dvb.de)"), "dvb.de", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", DVB, HTML)
	{
	    setDescription(i18n("Provides departure information for Dresden in Germany."));
	    setUrl("http://www.dvb.de/");

	    setDepartureRawUrl( "http://www.dvb.de:80/de/Fahrplan/Abfahrtsmonitor/abfahrten.do/%1#result" );
	    setRegExpDepartures( "(?:<tr class=\".*\">\\s*<td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*.?\\s*</td>\\s*<td><img src=\"/images/design/pikto_([^\\.]*).\\w{3,4}\" title=\"[^\"]*\" alt=\".*\" class=\".*\" /></td>\\s*<td>)(\\w*\\s*[0-9]+)(?:</td>\\s*<td>\\s*)([^<]*)(?:.*</td>\\s*</tr>)", QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Target );

	    setCountry( "Germany" );
	    setCities( QStringList() << "Dresden" );
	    setUseSeperateCityValue( false );
	}
};

class TimetableAccessorInfo_Vrn : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Vrn() : TimetableAccessorInfo(i18n("Rhine-Neckar (vrn.de)"), "vrn.de", QString::fromUtf8("Martin Gräßlin"), "" /*kde@martin-graesslin.com*/, "1.0", VRN, HTML)
	{
	    setDescription(i18n("Provides departure information for 'Rhine-Neckar' in Germany."));
	    setUrl("http://www.vrn.de/");

	    setDepartureRawUrl( "http://efa9.vrn.de/vrn/XSLT_DM_REQUEST?language=de&type_dm=stop&mode=direct&place_dm=%1&name_dm=%2&deleteAssignedStops=1" );
	    setRegExpDepartures( "(<td class=\".*\" style=\".*\">([0-9]{2})\\:([0-9]{2})</td>.*<td class=\".*\" style=\".*\" align=\".*\"><img src=\"images/response/(.*)\\..*\" alt=\".*\"></td>.*<td class=\".*\" nowrap>(.*)</td>.*<td class=\".*\">(.*)</td>)", QList< TimetableInformation >() << Nothing << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Target );

	    setCountry( "Germany" );
	    setCities( QStringList() << "Karlsruhe" ); // TODO: fill with cities
	    setUseSeperateCityValue( true );
	}
};

class TimetableAccessorInfo_Vvs : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Vvs() : TimetableAccessorInfo(i18n("Stuttgart (vvs.de)"), "vvs.de", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", VVS, HTML)
	{
	    setDescription(i18n("Provides departure information for Stuttgart in Germany."));
	    setUrl("http://www.vvs.de/");

    // 	Url to get a list of possible stops (%1 => part of the stop name):
    // 	http://www2.vvs.de/vvs/XSLT_DM_REQUEST?language=de&name_dm=%1&type_dm=any&anyObjFilter_dm=2

	    setDepartureRawUrl( "http://www2.vvs.de/vvs/XSLT_DM_REQUEST?language=de&type_dm=stop&mode=direct&place_dm=%1&name_dm=%2&deleteAssignedStops=1" );
	    setRegExpDepartures( "(?:<tr><td class=\"[^\"]*\" /><td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*</td>(?:<td />)?<td class=\"[^\"]*\" style=\"[^\"]*\"><div style=\"[^\"]*\"><img src=\"[^\"]*\" alt=\"[^\"]*\" title=\")([^\"]*)(?:\" border=[^>]*></div><div style=\"[^\"]*\">\\s*)([^<]*)(?:\\s*</div></td><td>\\s*)([^<]*)(?:\\s*</td><td>)([^<]*)(?:</td><td>)(?:<span class=\"hinweis\"><p[^>]*><a[^>]*>)?([^<]*)?(?:</a></p></span>)?(?:</td></tr>)", QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Target << Platform << JourneyNewsOther );
// <tr><td class="center" /><td>12:45</td><td /><td class="icon" style="vertical-align:top;"><div style="float:left;margin-top:-2px"><img src="images/means/icon_sbahn_a.gif" alt="S-Bahn" title="S-Bahn" border="0" /></div><div style="padding-top:0px;padding-left:5px;float:left"> S3</div></td><td>Backnang</td><td>�102</td><td><span class="hinweis"><p class="imglink"><a href="http://efainfo.vvs.de:8080/cm/XSLT_CM_SHOWADDINFO_REQUEST?infoID=20003657&amp;seqID=3&amp;language=de&amp;itdLPxx_showHtml=1">Stuttgart - Backnang - Crailsheim: Fahrplan�nderungen</a></p></span></td></tr>
	    setCountry( "Germany" );
	    setCities( QStringList() << "Stuttgart" ); // TODO: fill with cities
	    setUseSeperateCityValue( true );
	}
};

class TimetableAccessorInfo_Fahrplaner : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Fahrplaner() : TimetableAccessorInfo(i18n("Lower Saxony, Bremen (fahrplaner.de)"), "fahrplaner.de", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", Fahrplaner, HTML)
	{
	    setDescription(i18n("This service provider works for cities in 'Lower Saxony, Bremen' in Germany and also for many other cities in Germany. These other cities only include trains."));

	    setUrl("http://www.fahrplaner.de/");
// TODO: "&showResultPopup=popup" aus rawUrl entfernen und neue regexps schreiben
	    setDepartureRawUrl( "http://www.fahrplaner.de/hafas/stboard.exe/dn?ld=web&L=vbn&input=%1&boardType=%5&time=%2&showResultPopup=popup&disableEquivs=no&maxJourneys=%3&start=yes" );
	    setRegExpDepartures( "(?:<td class=\"nowrap\">\\s*<span style=\"[^\"]*\">\\s*)(Str|Bus|RE|IC|ICE|RB|)(?:\\s*)([^<]*)(?:\\s*</span>\\s*</td>\\s*<td class=\"nowrap\">\\s*<span style=\"[^\"]*\">\\s*)([^<]*)(?:\\s*(?:<br />\\s*<img .+ />&nbsp;\\s*<span class=\"him\">\\s*<span class=\"bold\">.*</span>.*</span>\\s*)?</span>\\s*</td>\\s*<td>\\s*<span style=\"[^\"]*\">&nbsp;)([0-9]{2})(?::)([0-9]{2})(?:&nbsp;</span></td>\\s*</tr>)", QList< TimetableInformation >() << TypeOfVehicle << TransportLine << Target << DepartureHour << DepartureMinute );

	    addRegExpPossibleStops( "(?:<th>Haltestelle:</th>\\s*<td>\\s*<select name=\"[^\"]*\">\\s*)(.*)(?:\\s*</select>)", "(?:<option value=\"[^\"]+#)([0-9]+)(?:\">)([^<]*)(?:</option>)", QList< TimetableInformation >() << StopID << StopName );

	    setCountry( "Germany" );
	    // TODO: add all fahrplaner.de-cities (Niedersachsen/Bremen)
	    setCities( QStringList() << "Bremen" << "Bremerhaven" << "Hannover" << "Braunschweig" << "Emden" );
	    setUseSeperateCityValue( false );
	}
};

class TimetableAccessorInfo_Nasa : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Nasa() : TimetableAccessorInfo(i18n("Saxony-Anhalt (nasa.de)"), "nasa.de", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", NASA, HTML)
	{
	    setUrl("http://www.nasa.de/");
	    setDescription(i18n("Provides departure information for 'Saxony-Anhalt' in Germany."));

	    //&showResultPopup=popup  // not needed?
	    setDepartureRawUrl( "http://www.nasa.de/delfi52/stboard.exe/dn?ld=web&L=vbn&input=%1&boardType=%5&time=%2&disableEquivs=no&maxJourneys=%3&start=yes" );
	    setRegExpDepartures( "(?:<tr class=\".*\">\\s*<td class=\".*\">)([0-9]{2})(?::)([0-9]{2})(?:</td>\\s*<td class=\".*\">\\s*<a href=\"/delfi52/.*\"><img src=\"/img52/)([^_]*)(?:_pic.\\w{3,4}\" width=\".*\" height=\".*\" alt=\"[^\"]*\" style=\".*\">\\s*)([^<]*)(?:\\s*</a>\\s*</td>\\s*<td class=\".*\">\\s*<span class=\".*\">\\s*<a href=\"/delfi52/stboard.exe/dn.*>\\s*)([^<]*)(?:\\s*</a>\\s*</span>\\s*<br />\\s*<a href=\".*\">.*</a>.*</td>\\s*)(?:<td class=\".*\">\\s*([^<]*)\\s*<br />[^<]*</td>\\s*)?(?:.*</tr>)", QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Target << Platform );

	    addRegExpPossibleStops( "(?:<select class=\"error\" name=\"input\">\\s*)(.*)(?:\\s*</select>)", "(?:<option value=\"[^\"]+#)([0-9]+)(?:\">)([^<]*)(?:</option>)", QList< TimetableInformation >() << StopID << StopName );

	    setCountry( "Germany" );
	    setCities( QStringList() << "Leipzig" << "Halle" << "Magdeburg" << "Dessau" << "Wernigerode" << "Halberstadt" << "Sangerhausen" << "Merseburg" << "Weissenfels" << "Zeitz" << "Altenburg" << "Delitzsch" << "Wolfen" << "Aschersleben" << "Köthen (Anhalt)" << "Wittenberg" << "Schönebeck (Elbe)" << "Stendal" );
	    setUseSeperateCityValue( false );
	}
};

class TimetableAccessorInfo_Imhd : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Imhd() : TimetableAccessorInfo(i18n("Bratislava (imhd.sk)"), "imhd.sk", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", IMHD, HTML)
	{
	    setUrl("http://www.imhd.zoznam.sk/ba/");
	    setDescription("Provides departure information for Bratislava in Slovakia.");

// 	    \"http://www.imhd.zoznam.sk/ba/index.php?w=212b36213433213aef2f302523ea〈=en&hladaj=%1\";
	    setDepartureRawUrl( "http://www.imhd.zoznam.sk/%1/index.php?w=212b36213433213aef2f302523ea&lang=en&hladaj=%2" );
	    setRegExpDepartures( "(?:<tr><td><b>)([0-9]{2})(?:\\.)([0-9]{2})(?:</b></td><td><center><b><em>)(N?[0-9]+)(?:</em></b></center></td><td>)(.*)(?:</td></tr>)", QList< TimetableInformation >() << DepartureHour << DepartureMinute << TransportLine << Target,
			    "(?:<tr><td class=\"[^\"]*\"><center><b>)([^<]*)(?:</b></center></td><td><center>)([^<]*)(?:</center></td>)", TransportLine, TypeOfVehicle );

	    // Parses all available stops:
	    addRegExpPossibleStops( "(?:<form .* name=\"zastavka\">\\s*<select [^>]*>\\s*<option value=\"[^\"]*\">[^<]*</option>)(.*)(?:</select>)", "(?:<option value=\"[^\"]*\">)([^<]*)(?:</option>)", QList< TimetableInformation >() << StopName );

	    setCountry( "Slovakia" );
	    setCities( QStringList() << "Bratislava" << QString::fromUtf8("Banská Bystrica") << QString::fromUtf8("Košice") << "Hlohovec" << QString::fromUtf8("lLiptovský Mikuláš") << "Nitra" << QString::fromUtf8("Piešťany") << QString::fromUtf8("Považská Bystrica") << QString::fromUtf8("Prešov") << "Prievidza" << "Senica" << "Skalica" << QString::fromUtf8("Trenčín") << "Trnava" << QString::fromUtf8("Vysoké Tatry") << QString::fromUtf8("Žilina") );

	    addCityNameToValueReplacement( "bratislava", "ba" );
	    addCityNameToValueReplacement( QString::fromUtf8("banská bystrica"), "bb" );
	    addCityNameToValueReplacement( QString::fromUtf8("košice"), "ke" );
	    addCityNameToValueReplacement( "hlohovec", "hc" );
	    addCityNameToValueReplacement( QString::fromUtf8("liptovský mikuláš"), "lm" );
	    addCityNameToValueReplacement( "nitra", "nr" );
	    addCityNameToValueReplacement( QString::fromUtf8("piešťany"), "pn" );
	    addCityNameToValueReplacement( QString::fromUtf8("považská bystrica"), "pb" );
	    addCityNameToValueReplacement( QString::fromUtf8("prešov"), "po" );
	    addCityNameToValueReplacement( "prievidza", "pd" );
	    addCityNameToValueReplacement( "senica", "se" );
	    addCityNameToValueReplacement( "skalica", "si" );
	    addCityNameToValueReplacement( QString::fromUtf8("trenčín"), "tn" );
	    addCityNameToValueReplacement( "trnava", "tt" );
	    addCityNameToValueReplacement( QString::fromUtf8("vysoké tatry"), "tatry" );
	    addCityNameToValueReplacement( QString::fromUtf8("žilina"), "za" );

 	    setCharsetForUrlEncoding("windows-1250");
	    setUseSeperateCityValue( true );
	    setOnlyUseCitiesInList( true ); // only values in "cities" are allowed as city names
	}
};

class TimetableAccessorInfo_Idnes : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Idnes() : TimetableAccessorInfo(i18n("Czech (jizdnirady.idnes.cz)"), "jizdnirady.idnes.cz", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", IDNES, HTML)
	{
	    setDescription(i18n("This service provider has static data. That means, that displayed departures may actually not be valid for today. Please view the journey news, to see if a departure is valid for today (e.g. 'Mo-Fr')."));
	    setUrl("http://jizdnirady.idnes.cz/vlakyautobusy/spojeni/");

	    setDepartureRawUrl( "http://jizdnirady.idnes.cz/%1/odjezdy/?f=%2&submit=true&lng=E" );
	    setRegExpDepartures( "(?:<tr class=\"[^\"]*\">\\s*<td class=\"datedt\">)([0-9]{1,2})(?::)([0-9]{2})(?:</td><td>[^<]*</td><td class=\"[^\"]*\"></td><td class=\"[^\"]*\">[^<]*</td><td><img src=\"[^\"]*\" alt=\")([^\"]*)(?:\" title=\"[^\"]*\" />\\s*<a href=\"[^\"]*\" title=\"[^\\(]*\\([^>]*>>\\s)([^\\)]*)(?:\\)\" style=\"[^\"]*\" onclick=\"[^\"]*\">)([^<]*)(?:</a>\\s*)(?:<img [^>]*>\\s*)?(?:</td>\\s*</tr>)", QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << Target << TransportLine );

    // 			<tr class="septr">
    // 			<td class="datedt">20:01</td><td>Bachova </td><td class="right"></td><td class="right">&nbsp;</td><td><img src="img/bus_p.gif" alt="Bus" title="Bus" /> <a href="/draha/?p=MzMlNDMyNjglMTM0NTAwMDIlLTElMCU0NSUlMjA6MDEl&amp;irk=MTkzMDAxMTgwNw--" title="Bus: bus (Háje >> Na Knížecí)" style="color:#0000FF;" onclick="return PopupOpen(event,'route');">197</a> <img src="img/FC/072.gif" alt="" title="" width="14" height="14" /> </td>
    // 		</tr><tr>
    // 			<td>&nbsp;</td><td colspan="4"><ul><li class="ico-info-blue">Háje (19:57) &gt; Na Knížecí (20:49)</li><li class="ico-info-beige">Dopravní podnik hl.m. Prahy, a.s.; Praha 9; 296192150</li><li class="ico-info-orange">runs on <img src="img/FC/088.gif" alt="Working day" title="Working day" width="14" height="14" /></li></ul></td>
    //
    // 		</tr><tr class="septr">

    // 	setRegExpPossibleStops( "(?:<form .* name=\"zastavka\">\\s*<select [^>]*>\\s*<option value=\"[^\"]*\">[^<]*</option>)(.*)(?:</select>)", "(?:<option value=\"[^\"]*\">)([^<]*)(?:</option>)", QList< TimetableInformation >() << StopName );

	    setCountry( "Czech" );
	    setCities( QStringList() << "Praha" );
	    setUseSeperateCityValue( true );
	}
};

class TimetableAccessorInfo_Rmv : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Rmv() : TimetableAccessorInfo(i18n("Rhine-Main (rmv.de)"), "rmv.de", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", RMV, XML)
	{
	    setDescription(i18n("Provides departure information for 'Rhine-Main' in Germany. It uses an XML source for departure information and HTML for stop autocompletion."));
	    setUrl("http://www.rmv.de/");

	    setDepartureRawUrl( "http://www.rmv.de/auskunft/bin/jp/stboard.exe/dn?L=vs_rmv.vs_sq&selectDate=today&time=%2&input=%1&maxJourneys=%3&boardType=%5&productsFilter=1111111111100000&maxStops=1&output=html&start=yes" );
	    setRawUrlXml("http://www.rmv.de/auskunft/bin/jp/stboard.exe/dn?L=vs_rmv.vs_sq&selectDate=today&time=%2&input=%1&maxJourneys=%3&boardType=%5&productsFilter=1111111111100000&maxStops=1&output=xml&start=yes");

	    addRegExpPossibleStops( "(?:<td class=\"result\" nowrap>\\s*<img[^>]*>\\s*<select name=\"input\">\\s*)(.*)(?:\\s*</select>\\s*</td>)", "(?:<option value=\"[^\"]+#)([0-9]+)(?:\">)([^<]*)(?:</option>)", QList< TimetableInformation >() << StopID << StopName );

	    setCountry( "Germany" );
	    setCities( QStringList() << "Frankfurt (Main)" << "Langen (Hessen)" << "Köln" << "Mainz" ); // TODO: fill with cities
	    setUseSeperateCityValue( false );
	}
};

class TimetableAccessorInfo_Pkp : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Pkp() : TimetableAccessorInfo(i18n("Poland (rozklad-pkp.pl)"), "rozklad-pkp.pl", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", PKP, HTML)
	{
	    setDescription(i18n("This service provider has static data. That means, that displayed departures may actually not be valid for today. Please view the journey news, to see if a departure is valid for today (e.g. 'Mo-Fr')."));
	    setUrl("http://rozklad-pkp.pl/?q=en/node/143");

	    setDepartureRawUrl( "http://rozklad-pkp.pl/stboard.php/en?q=en/node/149&input=%1&boardType=%5&time=%2&start=yes" );
	    setRegExpDepartures( "(?:<tr valign=\"[^\"]*\" bgcolor=\"[^\"]*\">\\s*<td class=\"result\"[^>]*>\\s*<span class=\"[^\"]*\">&nbsp;\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*&nbsp;\\s*</span>\\s*</td>\\s*<td class=\"result\"></td>\\s*<td class=\"result\"[^>]*>\\s*<a href=\"[^\"]*\">\\s*<img src=\"/img/)([^_]*)(?:_pic.[^\"]{3,4}\"[^>]*>\\s*</a>\\s*</td>\\s*<td class=\"result\"></td>\\s*<td class=\"result\"[^>]*>\\s*<span class=\"[^\"]*\">\\s*<a href=\"[^\"]*\">\\s*)([^<]*)(?:\\s*</a>\\s*</span>\\s*</td>\\s*<td class=\"result\"></td>\\s*<td class=\"result\">\\s*<span class=\"[^\"]*\">\\s*<a href=\"[^\"]*\">\\s*)([^<]*)(?:</a>\\s*</span>.*<span class=\"rsx\">\\s*<br>\\s*)([^<]*)(?:\\s*</span>\\s*</td>\\s*</tr>)", QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Target << JourneyNewsOther );

	    addRegExpPossibleStops( "(?:<td class=\"result\" nowrap>\\s*<img[^>]*>\\s*<select name=\"input\">)(.*)(?:</select>)", "(?:<option value=\"[^\"]+#)([0-9]+)(?:\">)([^<]*)(?:</option>)", QList< TimetableInformation >() << StopID << StopName );

	    setCountry( "Poland" );
	    setCities( QStringList() );
	    setUseSeperateCityValue( false );
	}
};

class TimetableAccessorInfo_Oebb : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Oebb() : TimetableAccessorInfo(i18n("Austria (oebb.de)"), "oebb.de", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", OEBB, HTML)
	{
	    //TODO: description
	    setDescription(i18n("This service provider works for cities in Austria and also for many cities in Europe. Cities in Austria include trams, buses, subways and interurban trains, while cities in other countries may only include trains."));

	    setUrl("http://www.oebb.at/");

	    setDepartureRawUrl( "http://fahrplan.oebb.at/bin/stboard.exe/dn?ld=oebb&input=%1&boardType=%5&time=%2&REQ0JourneyProduct_list=0:1111111111010000-000000.&disableEquivs=no&maxJourneys=%3&start=yes" );
// <tr class="depboard-light"><td class="bold center sepline top">20:07</td><td class="sepline prognosis nowrap top"></td><td class="bold top nowrap sepline"><a href="..."><img alt="U      2" src="/img/vs_oebb/u_pic.gif"/> U      2</a></td><td class="sepline top"><span class="bold"><a href="...">Wien Stadion (U2)</a></span><br/></td><td class="center sepline top">Wien Praterstern Bf (U2)</td></tr>
	    QString regexpAnyDoubleQuotedString = "\"[^\"]*\"";
	    //TODO fix regexp. This one works but gives much too little information
	    setRegExpDepartures( QString( "(?:<tr class=\"depboard[^\"]*\">\\s*<td class=%1>)([0-9]{2})(?::)([0-9]{2})(?:</td>\\s*<td class=%1>.*</td>\\s*<td class=%1>\\s*<a href=%1>.*)(?:src=\"/img/vs_oebb/)([^_]*)(?:.*)(?:</td>\\s*<td class=%1>\\s*)(?:.*</td>\\s*</tr>)" ).arg(regexpAnyDoubleQuotedString), QList< TimetableInformation >() << DepartureHour << DepartureMinute << TransportLine << TypeOfVehicle << Target );

// 	    setRegExpJourneys( QString( "(?:<tr class=\"depboard[^\"]*\">\\s*<td class=%1>)([0-9]{2})(?::)([0-9]{2})(?:</td>\\s*<td class=%1>.*</td>\\s*<td class=%1>\\s*<a href=%1><img alt=\")([^\"]*)(?:\" src=\"/img/vs_oebb/)([^_]*)(?:_pic\\.\\w{3,4}\"/?>[^<]*</a>\\s*</td>\\s*<td class=%1>\\s*<span class=%1>\\s*<a href=%1>\\s*)([^<]*)(?:\\s*</a>\\s*</span>\\s*<br/>.*)(?:</td>\\s*<td class=%1>\\s*)(?:.*</td>\\s*</tr>)" ).arg(regexpAnyDoubleQuotedString), QList< TimetableInformation >() << DepartureHour << DepartureMinute << TransportLine << TypeOfVehicle << Direction );

// 	    setRegExpJourneys( QString("(?:<tr class=\"depboard-[^\"]*\">\\s*<td class=%1>)([0-9]{2})(?::)([0-9]{2})(?:</td>\\s*<td class=%1>\\s*)(<span class=\"rtLimit\\d\">[0-9]{2}:[0-9]{2}</span>\\s*)?(?:</td><td class=%1>\\s*<a href=%1><img alt=\")([^\"]*)(?:\" src=\"/img/vs_oebb/)([^_]*)(?:_pic\\.\\w{3,4}\"/>[^<]*</a>\\s*</td>\\s*<td class=%1>\\s*<span class=%1>\\s*<a href=%1>\\s*)([^<]*)(?:\\s*</a>\\s*</span>\\s*<br/>.*)(?:<br/>\\s*<img width=%1 height=%1 alt=%1 src=\"/img/info_i.gif\"/>\\s*<span class=\"him\">([^<]*)\\s*</span>\\s*)?(?:</td>\\s*<td class=%1>\\s*)((?:<span class=%1>\\s*)[^<]*(?:\\s*<img width=%1 height=%1 border=%1 alt=%1 src=%1/>)?(?:<br/>)?(?:\\s*</span>)\\s*[^<]*|(?:<td class=%1>\\s*)[^<]*(?:\\s*</td>))?(?:\\s*</td>\\s*</tr>)").arg(regexpAnyDoubleQuotedString), QList< TimetableInformation >() << DepartureHour << DepartureMinute << JourneyNews << TransportLine << TypeOfVehicle << Direction << JourneyNewsOther << Platform );
	    addRegExpJouneyNews( "(?:<span class=\"rtLimit1\">[^<]*</span>)", QList< TimetableInformation >() << NoMatchOnSchedule );
	    addRegExpJouneyNews( "(?:<span class=\"rtLimit3\">([0-9]{2})(?::)([0-9]{2})</span>)", QList< TimetableInformation >() << DepartureHourPrognosis << DepartureMinutePrognosis );

	    addRegExpPossibleStops( "(?:<select class=\"error\" name=\"input\">\\s*)(.*)(?:</select>)", "(?:<option value=\"[^\"]+#)([0-9]+)(?:\">)([^<]*)(?:</option>)", QList< TimetableInformation >() << StopID << StopName );

	    setCountry( "Austria" );
	    setCities( QStringList() );
	    setUseSeperateCityValue( false );
	}
};

class TimetableAccessorInfo_Db : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Db() : TimetableAccessorInfo(i18n("Germany (db.de)"), "db.de", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.1", DB, HTML)
	{
	    setDescription(i18n("This service provider works for cities in Germany and also for many cities in Europe. Cities in Germany include trams, buses, subways and interurban trains, while cities in other countries may only include trains. Delays are rounded to five minute steps (there's currently no way to get delays with minute accuracy). A delay between zero and four minutes isn't shown, five to nine minutes delay are shown as five minutes dealy and so on."));

	    setUrl("http://reiseauskunft.bahn.de/bin/query.exe/d");

	    // TODO: "?" after the stop name => force possible stop list
	    //	 "!" after stop name => force no ambiguities
	    setDepartureRawUrl( "http://reiseauskunft.bahn.de/bin/bhftafel.exe/dn?input=%1&boardType=%5&time=%2&disableEquivs=no&maxJourneys=%3&start=yes&GUIREQProduct_0&GUIREQProduct_1&GUIREQProduct_2&GUIREQProduct_3&GUIREQProduct_4&GUIREQProduct_5&GUIREQProduct_7&GUIREQProduct_8" );

	    setJourneyRawUrl( "http://reiseauskunft.bahn.de/bin/query.exe/dn?S=%1&Z=%2&time=%3&maxJourneys=%4&start=yes" );

// 	    &nbsp;<span style="color:#50aa50;">p&#252;nktlich</span>
	    QString regExpDate = "([0-9]{2}\\.[0-9]{2}\\.[0-9]{2})";
	    QString regExpTime = "([0-9]{2})(?::)([0-9]{2})(?:\\s*)(?:[^<]*<span style=\"[^\"]*\">[^<]*</span>)?";
	    QString regExpAnyString = "\"[^\"]*\"";
	    setRegExpJourneys( QString( "(?:<tr class=\"\\s*firstrow\">\\s*<td class=\"station first\\s*\">\\s*<a [^>]*></a>\\s*<div class=\"resultDep\">\\s*)([^<]*)(?:\\s*</div>\\s*</td>\\s*<td class=\"date\">[^,]*,?\\s*)%1(?:\\s*</td>\\s*<td class=\"timetx\">[^<]*</td>\\s*<td class=\"time\">\\s*)%2(?:\\s*</td>\\s*<td class=\"duration[^\"]*\" rowspan=\"2\">\\s*)([0-9]{1,2}:[0-9]{2})(?:\\s*</td>\\s*<td class=\"changes[^\"]*\"[^>]*>\\s*)([0-9]*)(?:\\s*</td>\\s*<td class=\"products[^\"]*\"[^>]*>\\s*<span[^>]*><a[^>]*>)(?:[^<]*<img src=%3[^>]*>)?([^<]*)(?:</a></span>\\s*</td>\\s*<td class=\"\\s*fareStd[^\"]*\"[^>]*>\\s*)(.*)(?:\\s*</td><td class=\"return[^\"]*\"[^>]*>\\s*<a[^>]*>[^<]*</a>\\s*</td>\\s*</tr>\\s*<tr class=\"\\s*last\">\\s*<td class=\"[^\"]*stationDest\">\\s*)([^<]*)(?:\\s*</td>\\s*<td class=\"date\">[^,]*,?\\s*)%1(?:\\s*</td>\\s*<td class=\"timetx\">[^<]*</td>\\s*<td class=\"time\">\\s*)%2(?:[^<]*</td>\\s*</tr>)" ).arg(regExpDate).arg(regExpTime).arg(regExpAnyString),
		QList<TimetableInformation>() << StartStopName << DepartureDate << DepartureHour << DepartureMinute << Duration << Changes << TypesOfVehicleInJourney << Pricing << TargetStopName << ArrivalDate << ArrivalHour << ArrivalMinute );

	    setRegExpDepartures( "(?:<tr>\\s*<td class=\"time\">)([0-9]{2})(?::)([0-9]{2})(?:</td>\\s*<td class=\"train\"><a href=\"[^\"]*\"><img src=\"[^\"]*/img/)(.*)(?:_.{3,7}\\.\\w{3,4}\" class=\"[^\"]*\" alt=\"[^\"]*\" /></a></td><td class=\"[^\"]*\">\\s*<a href=\"[^\"]*\">\\s*)(.*)(?:\\s*</a>\\s*</td>\\s*<td class=\"route\">\\s*<span class=\"[^\"]*\">\\s*<a onclick=\"[^\"]*\" href=\"[^\"]*\">\\s*)([^<]*)(?:(?:\\s*</a>\\s*</span>\\s*<br />.*</td>\\s*<td class=\"platform\">\\s*<strong>)([^<]*)(?:</strong><br />\\s*[^<]*</td>\\s*))?(?:<td class=\"ris\">\\s*(.*)\\s*</td>)?(?:.*</tr>)", QList<TimetableInformation>() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Target << Platform << JourneyNews );


// 	    <td class="ris"><span><span style="color:#50aa50;">p&#252;nktlich</span></span><span style="padding-left:-5px;">,</span><br/><span class="red">Gleiswechsel</span></td>
	    addRegExpJouneyNews( "(?:<span><span style=\"[^\"]*\">ca.&nbsp;)(\\d*)(?:&nbsp;Minuten&nbsp;sp&#228;ter</span></span>)", QList< TimetableInformation >() << Delay );
	    addRegExpJouneyNews( "(?:<span><span style=\"[^\"]*\">ca.&nbsp;)(\\d*)(?:&nbsp;Minuten&nbsp;sp&#228;ter</span></span>)(?:,<br/><span class=\"[^\"]*\">Grund:\\s*)([^<]*)(?:</span>)", QList< TimetableInformation >() << Delay << DelayReason );
	    addRegExpJouneyNews( "(?:<span><span style=\"[^\"]*\">p&#252;nktlich</span></span>)", QList< TimetableInformation >() << NoMatchOnSchedule );
    // 	addRegExpJouneyNews( "(?:<span><span title=\"[^\"]*\">k.A.</span></span>)", QList< TimetableInformation >() << Nothing );
    // TODO (to find again):    Änderung im Zuglauf!,

	    addRegExpPossibleStops( "(?:<select class=\"error\" id=\"rplc0\" name=\"input\">\\s*)(.*)(?:</select>)",
				    "(?:<option value=\"[^\"]+#)([0-9]+)(?:\">)([^<]*)(?:</option>)",
				    QList< TimetableInformation >() << StopID << StopName );

	    // Possible stops for targets of journeys
	    addRegExpPossibleStops( "(?:<select class=\"locInput\" name=\"REQ0JourneyStopsZ0K\" id=\"REQ0JourneyStopsZ0K\"\\s*>\\s*)(.*)(?:</select>)",
				    "(?:<option value=\"[^\"]*\">)([^<]*)(?:</option>)",
				    QList< TimetableInformation >() << StopName );

	    setCountry( "Germany" );
	    setCities( QStringList() );
	    setUseSeperateCityValue( false );
	}
};

class TimetableAccessorInfo_Sbb : public TimetableAccessorInfo {
    public:
	TimetableAccessorInfo_Sbb() : TimetableAccessorInfo(i18n("Switzerland (sbb.ch)"), "sbb.ch", QString::fromUtf8("Friedrich Pülz"), "fpuelz@gmx.de", "1.0", SBB, HTML)
	{
	    setDescription(i18n("This service provider works for cities in Switzerland and also for many cities in Europe. Cities in Switzerland include trams, buses, subways and interurban trains, while cities in other countries may only include trains."));
	    setUrl("http://fahrplan.sbb.ch/");

	    setDepartureRawUrl( "http://fahrplan.sbb.ch/bin/bhftafel.exe/dn?input=%1&boardType=%5&time=%2&showResultPopup=popup&disableEquivs=no&maxJourneys=%3&start=yes" );

	    setJourneyRawUrl( "http://fahrplan.sbb.ch/bin/query.exe/dn?S=%1&Z=%2&time=%3&maxJourneys=%4&start=yes" );

	    QString regExpDate = "[0-9]{2}\\.[0-9]{2}\\.[0-9]{2}";
	    QString regExpTime = "[0-9]{2})(?::)([0-9]{2}"; //(?:\\s*)(?:[^<]*<span style=\"[^\"]*\">[^<]*</span>)?";
	    QString regExpAnyString = "\"[^\"]*\"";
	    setRegExpDepartures( QString("(?:<tr class=%1>\\s*<td class=%1[^>]*>\\s*<span class=%1>)([0-9]{2})(?::)([0-9]{2})(?:</span>\\s*</td>\\s*<td class=%1[^>]*>\\s*<a href=%1[^>]*>\\s*<img src=\"[^\"]*/products/)([^_]*)(?:_pic\\.\\w{3,4}\"[^>]*>\\s*</a>\\s*</td>\\s*<td class=%1[^>]*>\\s*<span class=%1>\\s*<a href=%1>\\s*)([^<]*)(?:\\s*</a>\\s*</span>\\s*</td>\\s*<td class=%1>\\s*<span class=%1>\\s*<a href=%1>)([^<]*)(?:</a></span>\\s*<br>.*<span class=%1>\\s*</span>\\s*</td>)(?:\\s*<td class[^>]*>\\s*([^<]*)\\s*</td>)?(?:.*\\s*</tr>)").arg( regExpAnyString ), QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Target << Platform );

	    /* TODO: Baustelle
	    <td headers="date" class="result" nowrap>Do, 09.07.09</td><td headers="time" class="result timeLeft">ab</td><td headers="time" class="result timeRight">15:40&nbsp;</td><td headers="time" class="result" valign="bottom" rowspan="2"><span class="hint_realtime">&nbsp;</span></td>
	    <td headers="time" class="result" width="1" rowspan="2"><a href="#HIM_connection_C0-0_messages" title="Bauarbeiten: Altst&#228;tten SG - Oberriet (THURBO)">
	    <img src="/img/1/him/ico_baustelle.gif" alt="" title="Baustelle" border="0" style="width:16px;height:16px" />
	    </a></td>
	    <td headers="duration" class="result" nowrap rowspan="2" valign="top">13:49</td>
	    */
	    setRegExpJourneys( QString( "(?:<tr class=%1>\\s*<td headers=\"details\"[^>]*><div[^>]*><a[^>]*>\\d*</a>\\s*</div><input type=\"checkbox\"[^>]*></td><td headers=\"location\"[^>]*><a href=%1><img[^>]*></a>[^<]*<a href=%1>)([^<]*)(?:</a></td><td headers=\"date\"[^>]*>[^,]*,?\\s*)(%2)(?:\\s*</td>\\s*<td headers=\"time\" [^>]*>[^<]*</td>\\s*<td[^>]*>)(%3)(?:\\s*&nbsp;)?(?:\\s*</td>\\s*<td headers=\"time\"[^>]*><span class=%1>[^<]*</span></td>\\s*<td[^>]*>)(?:<a href=%1 title=\"([^\"]*)\">\\s*<img src=%1[^>]*>\\s*</a>)?(?:[^<]*</td>\\s*<td headers=\"duration\"[^>]*>\\s*)([0-9]{1,2}:[0-9]{2})(?:\\s*</td>\\s*<td headers=\"changes\"[^>]*>\\s*)([0-9]*)(?:\\s*</td>\\s*<td headers=\"products\"[^>]*>\\s*)([^<]*)(?:&nbsp;)?(?:\\s*</td>\\s*<td headers=\"capacity\"[^>]*>.*</td>\\s*</tr>\\s*<tr class=%1>\\s*<td headers=\"location\"[^>]*><a href=%1><img src=%1[^>]*></a>[^<]*<a href=%1>)([^<]*)(?:</a></td><td headers=\"date\"[^>]*>\\s*(?:[^,]*,?\\s*(%2))?(?:&nbsp;)?\\s*</td>\\s*<td headers=\"time\"[^>]*>[^<]*</td>\\s*<td headers=\"time\"[^>]*>)(%3)(?:(?:&nbsp;)?\\s*</td>\\s*</tr>)"
	    ).arg( regExpAnyString ).arg( regExpDate ).arg( regExpTime ),
		QList<TimetableInformation>() << StartStopName << DepartureDate << DepartureHour << DepartureMinute << JourneyNewsOther << Duration << Changes << TypesOfVehicleInJourney << TargetStopName
		<< ArrivalDate << ArrivalHour << ArrivalMinute );

	    addRegExpPossibleStops( "(?:<td class=\"[^\"]*\" nowrap>\\s*<select name=\"input\">\\s*)(.*)(?:</select>\\s*</td>)", "(?:<option value=\"[\"]+#)([0-9]+)(?:\">)(.*)(?:</option>)", QList< TimetableInformation >() << StopID << StopName );

	    addRegExpPossibleStops( "(?:<select name=\"REQ0JourneyStopsZ0K\"[^>]*>)(.*)(?:</select>)", "(?:<option value=\"[^\"]*\">)([^<]*)(?:</option>)", QList< TimetableInformation >() << StopName );

	    setCountry( "Switzerland" );
	    setCities( QStringList() );
	    setUseSeperateCityValue( false );
	}
};

#endif // TIMETABLEACCESSOR_HTML_INFOS_HEADER
