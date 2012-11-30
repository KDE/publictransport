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

/**
 * @defgroup gtfsProviders GTFS Provider Plugins
 *
 * GTFS providers first need to download and import a GTFS feed into a local database. Depending
 * on the feed that database can get quite big (eg. ~300MB for a bigger area). After having the
 * feed imported it will be checked for updates from time to time. GTFS providers can then be used
 * without an internet connection and are very fast. While internet is available realtime data
 * like delays or news can be updated using GTFS-realtime without changing the database.
 *
 * Departures/arrivals with route information, stop suggestions and stops by geolocation are
 * supported by all GTFS provider plugins. Delays and news are supported, if GTFS-realtime is
 * supported. Journeys are currently not supported, routing would need to be done locally.
 *
 * Support for GTFS providers can be added with only a .pts file, see @ref provider_plugin_pts.
 * The type of the provider needs to be @em "gtfs". TimetableMate can be used to easily create new
 * valid GTFS provider plugins. Most important is the \<feedUrl\> tag in the .pts file, which
 * points to the GTFS feed zip file. To use GTFS-realtime simply add the used GTFS-realtime URLs
 * to the .pts file, ie. \<realtimeTripUpdateUrl\> and/or \<realtimeAlertsUrl\>.
 *
 * There are provider states specific to GTFS provider plugins: @em "gtfs_feed_import_pending"
 * (first start the GTFS feed import before the provider plugin can be used) and
 * @em "importing_gtfs_feed" (the GTFS feed currently gets imported). After the feed was
 * successfully imported the state gets set to @em "ready". The current state can be determined
 * using the @em "ServiceProviders" data source.
 *
 * The first GTFS feed import is @em not done automatically, only updates are done when available.
 * To start the first import, the GTFS service of the data engine can be used. That service also
 * allows to delete the database again or to manually request to check for feed updates.
 *
 * @see GtfsService, @ref provider_plugin_types
 **/
