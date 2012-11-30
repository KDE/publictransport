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
 * @defgroup scriptProviders Script Provider Plugins
 *
 * Script provider plugins implement execution of timetable data requests in a script. The script
 * file to use gets referenced in the .pts file in the \<script\> tag, see @ref provider_plugin_pts.
 * Script extensions that are needed inside the script (eg. @em qt.xml) can be specified in an
 * @em extensions attribute for that tag. The script can use the @ref scriptApi, which provides
 * ways to start network requests synchronously and asynchronously, helper functions to parse
 * HTML, a Storage to cache/save information etc.
 *
 * Currently all included script providers need a network connection to work and directly get the
 * state @em ready if there are no errors in the script. If a provider instead requires to download
 * all timetable data at once and could then execute requests offline, this provider plugin type
 * is not the right one. If you want to add support for a GTFS provider, use @ref gtfsProviders.
 * Otherwise a new plugin type could be created with different request strategies, state handling,
 * error checking and maybe an own data engine service.
 *
 * @see scriptApi, @ref provider_plugin_types
 **/
