/*
*   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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
#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <KDE/KLocale>

static const char description[] =
    I18N_NOOP("A helper application to add support for new service providers to "
          "the plasma data engine 'PublicTransport'");

static const char version[] = "0.2.3";

int main(int argc, char **argv)
{
    KAboutData about("timetablemate", 0, ki18n("TimetableMate"), version, ki18n(description),
                     KAboutData::License_GPL_V2, ki18n("(C) 2010 Friedrich Pülz"),
             KLocalizedString(), 0, "fpuelz@gmx.de");
    about.addAuthor( ki18n("Friedrich Pülz"), KLocalizedString(), "fpuelz@gmx.de" );
    KCmdLineArgs::init(argc, argv, &about);

    KCmdLineOptions options;
    options.add("+[URL]", ki18n( "Document to open" ));
    KCmdLineArgs::addCmdLineOptions(options);
    KApplication app;

//     TimetableMate *widget = new TimetableMate;

    // see if we are starting with session management
    if (app.isSessionRestored()) {
        RESTORE(TimetableMate);
    } else {
        // no session.. just start up normally
        KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
        if (args->count() == 0) {
            TimetableMate *widget = new TimetableMate;
            widget->show();
        } else {
            for ( int i = 0; i < args->count(); i++ ) {
                TimetableMate *widget = new TimetableMate;
        widget->open( args->url(i) );
                widget->show();
            }
        }
        args->clear();
    }

    return app.exec();
}
