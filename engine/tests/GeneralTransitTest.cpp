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

#include "GeneralTransitTest.h"

#include <QtTest/QTest>
#include "gtfs/generaltransitfeed_importer.h"

void GeneralTransitTest::init()
{

}

void GeneralTransitTest::cleanup()
{

}

void GeneralTransitTest::initTestCase()
{

}

void GeneralTransitTest::cleanupTestCase()
{

}

void GeneralTransitTest::readGtfsDataTest()
{
    // Expects that the test is started from path engine/build/tests/
    const QString fileName( "../../tests/sample-feed.zip" );

    GeneralTransitFeedImporter importer( "sample_gtfs" );
    importer.startImport( fileName );
    importer.wait();

    QCOMPARE( importer.hasError(), false );
}

QTEST_MAIN(GeneralTransitTest)
#include "GeneralTransitTest.moc"
