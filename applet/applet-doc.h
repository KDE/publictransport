/*
*   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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

/** @mainpage Public Transport Applet
@section intro_applet_sec Introduction
This applet shows a departure / arrival board for public transport, trains, ferries and planes.
Journeys can also be searched for. It uses the public transport data engine and has some advanced
configuration possibilities like filters, alarms and a flexible appearance.

@see models for more information about how the applet interacts with the PublicTransport data
  engine and how the data is stored in models.
@see filterSystem for more information about how the filters work.

@section install_applet_sec Installation
To install this applet type the following commands:<br>
\> cd /path-to-extracted-applet-sources/build<br>
\> cmake -DCMAKE_INSTALL_PREFIX=`kde4-config --prefix` ..<br>
\> make<br>
\> make install<br>
<br>
After installation do the following to use the applet in your plasma desktop:
Restart plasma to load the applet:<br>
\> kquitapp plasma-desktop<br>
\> plasma-desktop<br>
<br>
or test it with:<br>
\> plasmoidviewer publictransport<br>
<br>
You might need to run kbuildsycoca4 in order to get the .desktop file recognized.
*/

/**
* @defgroup models Models
@brief Data gets retrieved from data engines, processed in a thread and stored in models.

The models used for storing public transport data are: @ref DepartureModel for departures/arrivals
and @ref JourneyModel for journeys. They are both based on @ref PublicTransportModel.

The applet uses five data engines: <em>publictransport</em>, <em>geolocation</em>,
<em>openstreetmap</em> (to get stops near the user) and <em>favicons</em> (to get favicons from the
service providers).
The publictransport data engine expects data source names in a specific format, which is explained
in detail in it's documentation. Here are some examples of what source names the applet generates
(based on the settings):
@par
<em>"Departures de_db|stop=Leipzig|timeOffset=5"</em> for departures from the service provider with
the ID "de_db", a stop named "Leipzig" and an offset (from now) in minutes for the first departure
of 5.
@par
<em>"Journeys de_db|originStop=Leipzig|targetStop=Bremen"</em> for journeys from the service
provider with the ID "de_db", an origin stop named "Leipzig" and a target stop named "Bremen".

@note The service provider ID <em>"de_db"</em> can be left away to use a default service provider
  for the users country (from KDE's global settings).

The format of the data structure returned from the data engine is again explained in detail in the
data engine's documentation (@ref pageUsage). It arrives in the slot
@ref PublicTransport::dataUpdated. From there one of the following functions is called, based on
the data returned by the data engine:
@par
@ref PublicTransport::handleDataError, if the "error" key of the data structure is true, ie. there
was an error while running the query in the data engine (eg. server not reachable or an error in
the service provider while trying to parse the document from the server),
@par
@ref PublicTransport::processStopSuggestions, if the "stops" key of the data structure contains
data, which can also happen if eg. "Departures" were queried for, but the stop name is ambigous,
@par
@ref DepartureProcessor::processJourneys, @ref DepartureProcessor::processDepartures if there's
a "journeys", "departures" or "arrivals" key respectively. A new
job is added to the background thread. The thread then reads the data and creates data structures
of type @ref DepartureInfo for departures/arrivals or @ref JourneyInfo for journeys. It also checks
for alarms and applies filters. That way complex filters and or many alarms applied to many
departures/arrivals won't freeze the applet.

Before beginning a new departure/arrival/journey job the thread emits a signal that is connected
to @ref PublicTransport::beginDepartureProcessing / @ref PublicTransport::beginJourneyProcessing.
Once a chunk of departures/arrivals is ready @ref PublicTransport::departuresProcessed gets called
through a signal/slot connection. In that function the processed departures are cached based on
the source name (but with date and time values stripped) and then the departure/arrival model gets
filled with them in @ref PublicTransport::fillModel. If journeys are ready
@ref PublicTransport::journeysProcessed gets called by the thread, which calls
@ref PublicTransport::fillModelJourney. If filter settings are changed the thread is used again to
run filters on the current data. Once the filter job is ready it calls
@ref PublicTransport::departuresFiltered.

The @ref PublicTransport::fillModel and @ref PublicTransport::fillModelJourney functions add,
update and/or remove items in the models. Both the departure/arrival and the journey model have
functions called @ref DepartureModel::indexFromInfo / @ref JourneyModel::indexFromInfo, which use
a hash generated from the data items (@ref DepartureInfo / @ref JourneyInfo) to quickly check, if
there already is an item in the model for a given data item. Hashes are generated automatically in
the constructors and can be retrieved using @ref PublicTransportInfo::hash. Two data items don't
have to be exactly equal to generade an equal hash. That is important to also find
departures/arrivals/journeys which data has changed since the last update, eg. departures with a
changed delay.

@see filterSystem
*/

/**
* @defgroup filterSystem Filter System
@brief The applet has the possibility to filter departures/arrivals based on various constraints.

Those constraints are combined to filters using logical AND. Filters on the other hand can be
combined to filter lists using logical OR. The filter system is also used to match alarms.

The filtering is performed in classes described under @ref filter_classes_sec, while those filters
can be setup using widgets described under @ref filter_widgets_sec.

@n
@section filter_classes_sec Classes That Perform Filtering
The lowest class in the hierarchy of filter classes is @ref Constraint, which describes a single
constraint which should match the departures/arrivals to be shown/hidden. One step higher
in the hierarchy comes the class @ref Filter, which is a list of constraints (combined using
logical AND). Another step higher comes @ref FilterList, which is a list of filters (combined
using logical OR). A @ref FilterList is wrapped by an object of type @ref FilterSettings together
with the @ref FilterAction (show or hide matching departures/arrivals), name and affected stops
for that filter configuration.

Each @ref Constraint has a @ref FilterType, ie. what to filter with this constraint. For example
a constraint can filter departures/arrivals by the used vehicle type using @ref FilterByVehicleType.
Each @ref Constraint also has a @ref FilterVariant, eg. equals / doesn't equal. The used
@ref FilterVariant affects the way a constraint matches specific departures/arrivals. Last but not
least each @ref Constraint has a value.
So for example a constraint can be assembled like this: Filter by vehicle type, match
departures/arrivals that have the same value as stored in the constraint.

Filters can be serialized using toData() / fromData() methods.
Filter widgets described in the next section can be easily created from these filter classes.

@n
@section filter_widgets_sec Widgets for Editing Filters
There are accompanying QWidget based classes for the filter classes from the previous section:
@par
@ref ConstraintWidget for @ref Constraint, @ref FilterWidget for @ref Filter and
@ref FilterListWidget for @ref FilterList. Filter widgets can be constructed from the filter
classes of the previous section.

For each constraint data type there is a separate constraint widget class:
@par
@ref ConstraintListWidget to select values of a given list of values (eg. a list of vehicle types),
@par
@ref ConstraintStringWidget to enter a string for matching (eg. matching intermediate stops),
@par
@ref ConstraintIntWidget to enter an integer for matching and
@par
@ref ConstraintTimeWidget to enter a time for matching (eg. a departure time, used for alarms).

@ref FilterWidget uses @ref AbstractDynamicLabeledWidgetContainer as base class to allow dynamic
adding / removing of constraints. @ref FilterListWidget uses @ref AbstractDynamicWidgetContainer
to do the same with filters. Those base classes automatically add buttons to let the user
add / remove widgets. They are pretty flexible and will maybe end up in a library later for
reuse in other projects (like the publictransport runner).
*/

/** @page pageClassDiagram Class Diagram
@dot
digraph publicTransportDataEngine {
    ratio="compress";
    size="10,100";
    concentrate="true";
    // rankdir="LR";
    clusterrank=local;

    node [
    shape=record
    fontname=Helvetica, fontsize=10
    style=filled
    fillcolor="#eeeeee"
    ];

    applet [
    fillcolor="#ffdddd"
    label="{PublicTransportApplet|Shows a departure / arrival list or a list of journeys.\l|+ dataUpdated( QString, Data ) : void\l# createTooltip() : void\l# updatePopupIcon() : void\l# processData( Data ) : void\l}"
    URL="\ref PublicTransport"
    ];

    subgraph clusterWidgets {
        label="Widgets";
        style="rounded, filled";
        color="#cceeee";
        node [ fillcolor="#ccffff" ];

        timetableWidget [
        label="{TimetableWidget|Represents the departure/arrial board.\l}"
        URL="\ref TimetableWidget"
        ];

        departureGraphicsItem [
        label="{DepartureGraphicsItem|Represents one item in the departure/arrial board.\l}"
        URL="\ref DepartureGraphicsItem"
        ];

        journeyTimetableWidget [
        label="{JourneyTimetableWidget|Represents the journey board.\l}"
        URL="\ref JourneyTimetableWidget"
        ];

        journeyGraphicsItem [
        label="{JourneyGraphicsItem|Represents one item in the journey board.\l}"
        URL="\ref JourneyGraphicsItem"
        ];

        titleWidget [
        label="{TitleWidget|Represents the title of the applet.\l}"
        URL="\ref TitleWidget"
        ];

        routeGraphicsItem [
        label="{RouteGraphicsItem|Represents the route item in a departure/arrival item.\l}"
        URL="\ref RouteGraphicsItem"
        ];

        journeyRouteGraphicsItem [
        label="{JourneyRouteGraphicsItem|Represents the route item in a journey item.\l}"
        URL="\ref JourneyRouteGraphicsItem"
        ];
    };

    subgraph thread {
        label="Background Thread";
        style="rounded, filled";
        color="#ffcccc";
        node [ fillcolor="#ffdfdf" ];

        departureProcessor [
        label="{DepartureProcessor|Processes data from the data engine and applies filters/alarms.\l}"
        URL="\ref DepartureProcessor"
        ];
    };

    subgraph clusterSettings {
        label="Settings";
        style="rounded, filled";
        color="#ccccff";
        node [ fillcolor="#dfdfff" ];

        settings [
            label="{PublicTransportSettings|Manages the settings of the applet.\l}"
            URL="\ref PublicTransportSettings"
            ];

        dataSourceTester [
            label="{DataSourceTester|Tests a departure / arrival or journey data source \lat the public transport data engine.\l|+ setTestSource( QString ) : void\l+ testResult( TestResult, QVariant ) : void [signal] }"
            URL="\ref DataSourceTester"
            ];
    };

    subgraph clusterModels {
        label="Models";
        style="rounded, filled";
        color="#ccffcc";
        node [ fillcolor="#dfffdf" ];
        rank="sink";

        departureModel [
            label="{DepartureModel|Stores information about a departures / arrivals.\l}"
            URL="\ref DepartureModel"
            ];

        journeyModel [
            label="{JourneyModel|Stores information about a journeys.\l}"
            URL="\ref JourneyModel"
            ];

        departureItem [
            label="{DepartureItem|Wraps DepartureInfo objects for DepartureModel.\l}"
            URL="\ref DepartureItem"
            ];

        journeyItem [
            label="{JourneyItem|Wraps JourneyInfo objects for JourneyModel.\l}"
            URL="\ref JourneyItem"
            ];

        departureInfo [
            label="{DepartureInfo|Stores information about a single departure / arrival.\l}"
            URL="\ref DepartureInfo"
            ];

        journeyInfo [
            label="{JourneyInfo|Stores information about a single journey.\l}"
            URL="\ref JourneyInfo"
            ];
    };

    edge [ dir=back, arrowhead="normal", arrowtail="none", style="dashed", fontcolor="darkgray",
           taillabel="", headlabel="0..*" ];
    timetableWidget -> departureGraphicsItem [ label="uses" ];
    journeyTimetableWidget -> journeyGraphicsItem [ label="uses" ];
    departureModel -> departureItem [ label="uses" ];
    journeyModel -> journeyItem [ label="uses" ];

    edge [ dir=forward, arrowhead="none", arrowtail="normal", style="dashed", fontcolor="darkgray",
           taillabel="1", headlabel="" ];
    departureProcessor -> applet [ label="m_departureProcessor" ];
    settings -> applet [ label="m_settings" ];
    dataSourceTester -> settings [ label="m_dataSourceTester" ];

    edge [ dir=back, arrowhead="normal", arrowtail="none", style="dashed", fontcolor="darkgray",
           taillabel="", headlabel="1" ];
    applet -> timetableWidget [ label="m_timetable" ];
    applet -> journeyTimetableWidget [ label="m_journeyTimetable" ];
    applet -> titleWidget [ label="m_titleWidget" ];
    applet -> departureModel [ label="m_model" ];
    applet -> journeyModel [ label="m_modelJourneys" ];
    departureItem -> departureInfo [ label="uses" ];
    journeyItem -> journeyInfo [ label="uses" ];
    departureGraphicsItem -> routeGraphicsItem [ label="uses" ];
    journeyGraphicsItem -> journeyRouteGraphicsItem [ label="uses" ];
}
@enddot
*/
