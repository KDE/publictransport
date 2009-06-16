#ifndef TIMETABLEACCESSOR_HTML_INFOS_HEADER
#define TIMETABLEACCESSOR_HTML_INFOS_HEADER

#include "timetableaccessor.h"

struct TimetableAccessorInfo
{
    ServiceProvider serviceProvider;
    QString rawUrl, regExpSearch, country;
    QList< TimetableInformation > regExpInfos;
    QStringList cities;
    bool putCityIntoUrl;

    TimetableAccessorInfo()
    {
	serviceProvider = NoServiceProvider;
    }
};

struct TimetableAccessorInfo_Bvg : TimetableAccessorInfo
{
    TimetableAccessorInfo_Bvg()
    {
	serviceProvider = BVG;
	rawUrl = "http://www.fahrinfo-berlin.de/IstAbfahrtzeiten/index;ref=3?input=%2+(%1)&submit=Anzeigen";
	regExpSearch = "(?:<tr class=\"\">\\s*<td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*</td>\\s*<td>\\s*<img src=\".*\" class=\"ivuTDProductPicture\" alt=\".*\"\\s*class=\"ivuTDProductPicture\" />)(\\w{1,10})(?:\\s*)((\\w*\\s*)?[0-9]+)(?:\\s*</td>\\s*<td>\\s*<a class=\"ivuLink\" href=\".*\" title=\".*\">)(.*)(?:</a>\\s*</td>\\s*<td>\\s*<!-- .* -->\\s*<a class=\"ivuLink\" href=\".*\" title=\".*\">[0-9]+</a>\\s*</td>\\s*</tr>)";
	regExpInfos = QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Direction;
	country = "Germany";
	cities = QStringList() << "Berlin";
	putCityIntoUrl = true;
    }
};

struct TimetableAccessorInfo_Dvb : TimetableAccessorInfo
{
    TimetableAccessorInfo_Dvb()
    {
	serviceProvider = DVB;
	rawUrl = "http://www.dvb.de:80/de/Fahrplan/Abfahrtsmonitor/abfahrten.do/%1#result";
	regExpSearch = "(?:<tr class=\".*\">\\s*<td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*.?\\s*</td>\\s*<td><img src=\".*\" title=\")(.*)(?:\" alt=\".*\" class=\".*\" /></td>\\s*<td>)(\\w*\\s*[0-9]+)(?:</td>\\s*<td>\\s*)(.*)(?:.*</td>\\s*</tr>)";
	regExpInfos = QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Direction;
	country = "Germany";
	cities = QStringList() << "Dresden";
	putCityIntoUrl = false;
    }
};

struct TimetableAccessorInfo_Vrn : TimetableAccessorInfo
{
    TimetableAccessorInfo_Vrn()
    {
	serviceProvider = VRN;
	rawUrl = "http://efa9.vrn.de/vrn/XSLT_DM_REQUEST?language=de&type_dm=stop&mode=direct&place_dm=%1&name_dm=%2&deleteAssignedStops=1";
	regExpSearch = "(<td class=\".*\" style=\".*\">([0-9]{2})\\:([0-9]{2})</td>.*<td class=\".*\" style=\".*\" align=\".*\"><img src=\"images/response/(.*)\\..*\" alt=\".*\"></td>.*<td class=\".*\" nowrap>(.*)</td>.*<td class=\".*\">(.*)</td>)";
	regExpInfos = QList< TimetableInformation >() << Nothing << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Direction;
	country = "Germany";
	cities = QStringList() << "Karlsruhe"; // TODO: fill with cities
	putCityIntoUrl = true;
    }
};

struct TimetableAccessorInfo_Vvs : TimetableAccessorInfo
{
    TimetableAccessorInfo_Vvs()
    {
	serviceProvider = VVS;
	rawUrl = "http://www2.vvs.de/vvs/XSLT_DM_REQUEST?language=de&type_dm=stop&mode=direct&place_dm=%1&name_dm=%2&deleteAssignedStops=1";
	regExpSearch = "(?:<tr><td class=\"center\" /><td>\\s*)([0-9]{2})(?::)([0-9]{2})(?:\\s*</td><td class=\".*\" style=\".*\"><div style=\".*\"><img src=\".*\" .* title=\")(.*)(?:\" border=.*/></div><div style=\".*\">\\s*)(\\w*\\s*[0-9]+)(?:\\s*</div></td><td>\\s*)(.*)(?:\\s*</td><td>.*</td><td>.*</td></tr>)";
	regExpInfos = QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Direction;
	country = "Germany";
	cities = QStringList() << "Stuttgart"; // TODO: fill with cities
	putCityIntoUrl = true;
    }
};

struct TimetableAccessorInfo_Fahrplaner : TimetableAccessorInfo
{
    TimetableAccessorInfo_Fahrplaner()
    {
	serviceProvider = Fahrplaner;
	rawUrl = "http://www.fahrplaner.de/hafas/stboard.exe/dn?ld=web&L=vbn&input=%1 %2&boardType=dep&time=actual&showResultPopup=popup&disableEquivs=no&maxJourneys=20&start=yes";
	regExpSearch = "(?:<td class=\"nowrap\">\\s*<span style=\".+\">\\s*)(Str|Bus).*(N?[0-9]+)(?:\\s*</span>\\s*</td>\\s*<td class=\"nowrap\">\\s*<span style=\".+\">\\s*)(\\w+.*\\w+)(?:\\s*(?:<br />\\s*<img .+ />&nbsp;\\s*<span class=\"him\">\\s*<span class=\"bold\">.*</span>.*</span>\\s*)?</span>\\s*</td>\\s*<td>\\s*<span style=\".+\">&nbsp;)([0-9]{2})(?::)([0-9]{2})(?:&nbsp;</span></td>\\s*</tr>)";
	regExpInfos = QList< TimetableInformation >() << TypeOfVehicle << TransportLine << Direction << DepartureHour << DepartureMinute;
	country = "Germany";
	// TODO: add all fahrplaner.de-cities (Niedersachsen/Bremen)
	cities = QStringList() << "Bremen" << "Bremerhaven" << "Hannover" << "Braunschweig" << "Emden";
	putCityIntoUrl = true;
    }
};

struct TimetableAccessorInfo_Nasa : TimetableAccessorInfo
{
    TimetableAccessorInfo_Nasa()
    {
	serviceProvider = NASA;
	rawUrl = "http://www.nasa.de/delfi52/stboard.exe/dn?ld=web&L=vbn&input=%1 %2&boardType=dep&time=actual&showResultPopup=popup&disableEquivs=no&maxJourneys=20&start=yes";
	regExpSearch = "(?:<tr class=\".*\">\\s*<td class=\".*\">)([0-9]{2})(?::)([0-9]{2})(?:</td>\\s*<td class=\".*\">\\s*<a href=\"/delfi52/.*\"><img src=\".*\" width=\".*\" height=\".*\" alt=\")(.*)(?:\\s*.?\" style=\".*\">\\s*)(\\w*\\s*[0-9]+)(?:\\s*</a>\\s*</td>\\s*<td class=\".*\">\\s*<span class=\".*\">\\s*<a href=\"/delfi52/stboard.exe/dn.*>\\s*)(.*)(?:\\s*</a>\\s*</span>\\s*<br />\\s*<a href=\".*\">.*</a>.*</td>\\s*<td class=\".*\">.*<br />.*</td>\\s*</tr>)";
	regExpInfos = QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Direction;
	country = "Germany";
	cities = QStringList() << "Leipzig" << "Halle" << "Magdeburg" << "Dessau" << "Wernigerode" << "Halberstadt" << "Sangerhausen" << "Merseburg" << "Weissenfels" << "Zeitz" << "Altenburg" << "Delitzsch" << "Wolfen" << "Aschersleben" << "Köthen (Anhalt)" << "Wittenberg" << "Schönebeck (Elbe)" << "Stendal";
	putCityIntoUrl = true;
    }
};

struct TimetableAccessorInfo_Imhd : TimetableAccessorInfo
{
    TimetableAccessorInfo_Imhd()
    {
	serviceProvider = IMHD;
	rawUrl = "http://www.imhd.zoznam.sk/ba/index.php?w=212b36213433213aef2f302523ea&lang=en&hladaj=%1";
	regExpSearch = "(?:<tr><td><b>)([0-9]{2})(?:\\.)([0-9]{2})(?:</b></td><td><center><b><em>)(N?[0-9]+)(?:</em></b></center></td><td>)(.*)(?:</td></tr>)";
	regExpInfos = QList< TimetableInformation >() << DepartureHour << DepartureMinute << TransportLine << Direction;
	country = "Germany";
	cities = QStringList() << "Bratislava";
	putCityIntoUrl = false;
    }
};

struct TimetableAccessorInfo_Db : TimetableAccessorInfo
{
    TimetableAccessorInfo_Db()
    {
	serviceProvider = DB;
	rawUrl = "http://reiseauskunft.bahn.de/bin/bhftafel.exe/dn?ld=212.203&rt=1&input=%2, %1&boardType=dep&time=actual&showResultPopup=popup&disableEquivs=no&maxJourneys=20&start=yes&GUIREQProduct_5&GUIREQProduct_7&GUIREQProduct_8";
	regExpSearch = "(?:<tr>\\s*<td class=\".*\">)([0-9]{2})(?::)([0-9]{2})(?:</td>\\s*<td class=\".*\"><a href=\".*\"><img src=\".*\" class=\".*\" alt=\".*\" /></a></td><td class=\".*\">\\s*<a href=\".*\">\\s*)(\\w*)(?:\\s*)(\\S*[0-9]+)(?:\\s*</a>\\s*</td>\\s*<td class=\".*\">\\s*<span class=\".*\">\\s*<a onclick=\".*\" href=\".*\">\\s*)(.*)(?:\\s*</a>\\s*</span>\\s*<br />.*</td>\\s*</tr>)";
	regExpInfos = QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Direction;
	country = "Germany";
	cities = QStringList() << "";
	putCityIntoUrl = true;
    }
};

struct TimetableAccessorInfo_Sbb : TimetableAccessorInfo
{
    TimetableAccessorInfo_Sbb()
    {
	serviceProvider = SBB;
	rawUrl = "http://fahrplan.sbb.ch/bin/bhftafel.exe/dn?&input=%1&boardType=dep&time=actual&showResultPopup=popup&disableEquivs=no&maxJourneys=20&start=yes";
	regExpSearch = "(?:<tr class=\".*\">\\s*<td class=\".*\" valign=\".*\" align=\".*\">\\s*<span class=\".*\">)([0-9]{2})(?::)([0-9]{2})(?:</span>\\s*</td>\\s*<td class=\".*\" align=\".*\" valign=\".*\">\\s*<a href=\".*\">\\s*<img src=\".*\" .* alt=\".*\">\\s*</a>\\s*</td>\\s*<td class=\".*\" align=\".*\" valign=\".*\" nowrap>\\s*<span class=\".*\">\\s*<a href=\".*\">\\s*)(.*)(?:\\s*</a>\\s*</span>\\s*</td>\\s*<td class=\".*\">\\s*<span class=\".*\">\\s*<a href=\".*\">)(.*)(?:</a></span>\\s*<br>.*<span class=\".*\">\\s*</span>\\s*</td>\\s*</tr>)";
	regExpInfos = QList< TimetableInformation >() << DepartureHour << DepartureMinute << TransportLine << Direction;
	country = "Swiss";
	cities = QStringList() << "";
	putCityIntoUrl = false;
    }
};


/*** To add a new Accessor create a struct like this: ***
struct TimetableAccessorInfo_ : TimetableAccessorInfo
{
    TimetableAccessorInfo_()
    {
	serviceProvider = ;
	rawUrl = "";
	regExpSearch = "";
	regExpInfos = QList< TimetableInformation >() << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Direction;
	country = "Germany";
	cities = QStringList() << "";
	putCityIntoUrl = true;
    }
};
*/
#endif // TIMETABLEACCESSOR_HTML_INFOS_HEADER
