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

#include "JavaScriptParserTest.h"
#include "../javascriptparser.h"

#include <QtTest/QTest>
#include <QTimer>

void JavaScriptParserTest::initTestCase()
{}

void JavaScriptParserTest::init()
{}

void JavaScriptParserTest::cleanup()
{}

void JavaScriptParserTest::cleanupTestCase()
{
}

void JavaScriptParserTest::simpleTest()
{
    JavaScriptParser parser(
            "/* Comment */\n"
            "function test( i ) {\n"
            "    return i;\n"
            "}\n" );
    QVERIFY( !parser.hasError() );
    QCOMPARE( parser.nodes().count(), 2 );

    QCOMPARE( parser.nodes().first()->type(), Comment );
    QVERIFY( !parser.nodes().first()->isMultiline() );
    QCOMPARE( parser.nodes().first()->text(), QLatin1String("Comment") );
    QCOMPARE( parser.nodes().first()->type(), Comment );
    QCOMPARE( parser.nodes().first()->line(), 1 );
    QCOMPARE( parser.nodes().first()->column(), 0 );

    QCOMPARE( parser.nodes().at(1)->type(), Function );
    QCOMPARE( parser.nodes().at(1)->line(), 2 );
    QCOMPARE( parser.nodes().at(1)->column(), 0 );
    const QSharedPointer< FunctionNode > function = parser.nodes().at(1).dynamicCast<FunctionNode>();
    QVERIFY( !function.isNull() );
    QCOMPARE( function->name(), QLatin1String("test") );
    QCOMPARE( function->arguments().count(), 1 );
    QCOMPARE( function->arguments().first()->toString(), QLatin1String("i") );
    QCOMPARE( function->arguments().first()->line(), 2 );
    QCOMPARE( function->arguments().first()->column(), 15 );
    QCOMPARE( function->definition()->content(), QLatin1String("return i;") );
    QCOMPARE( function->definition()->children().first()->line(), 3 );
    QCOMPARE( function->definition()->children().first()->column(), 4 );
}

void JavaScriptParserTest::incorrectScript1Test()
{
    JavaScriptParser parser(
            "/* Comment/* function(// /*/\n"
            "function test( i } {\n"
            "    return i;\n"
            "};{\n" );
    QVERIFY( parser.hasError() );
}

void JavaScriptParserTest::incorrectScript2Test()
{
    JavaScriptParser parser(
            "\n"
            "x^}( var = 4 \n"
            "* x function()}" );
    QVERIFY( parser.hasError() );
}

void JavaScriptParserTest::incorrectScript3Test()
{
    JavaScriptParser parser(
            "x.te st,({ i ):\n"
            "    return i;\n"
            "}\n" );
    QVERIFY( parser.hasError() );
}

QTEST_MAIN(JavaScriptParserTest)
#include "JavaScriptParserTest.moc"
