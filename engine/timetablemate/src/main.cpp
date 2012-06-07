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

#include "timetablemate.h"

#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>
#include <KDE/KLocale>
#include <KMainWindow>

static const char description[] =
    I18N_NOOP("A little IDE for adding support for new service providers to "
              "the plasma data engine 'PublicTransport'.");

static const char version[] = "0.3";

int main(int argc, char **argv)
{
    KAboutData about( "timetablemate", 0, ki18n("TimetableMate"), version, ki18n(description),
                      KAboutData::License_GPL_V2, ki18n("© 2010-2012 Friedrich Pülz"),
                      KLocalizedString(), 0, "fpuelz@gmx.de" );
    about.addAuthor( ki18n("Friedrich Pülz"), ki18n("Main Developer"), "fpuelz@gmx.de",
                     QByteArray(), "fpuelz" );
    about.setTranslator( ki18nc("Names of translators, separated by ','", ""),
                         ki18nc("Emails of translators, separated by ','", "") );
    KCmdLineArgs::init( argc, argv, &about );

    KCmdLineOptions options;
    options.add( "+[URL]", ki18n("Project to open") );
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app;

    // See if we are starting with session management
    if ( app.isSessionRestored() ) {
        kRestoreMainWindows< TimetableMate >();
    } else {
        // No session, just start up normally
        KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
        TimetableMate *window = new TimetableMate;
        window->setObjectName("TimetableMate#");
        for ( int i = 0; i < args->count(); i++ ) {
            window->open( args->url(i) );
        }
        window->show();
        args->clear();
    }

    return app.exec();
}
