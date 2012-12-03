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

#include "ScriptApiTest.h"
#include "script/scriptapi.h"

#include <QtTest/QTest>
#include <QSignalSpy>
#include <QTimer>

void ScriptApiTest::initTestCase()
{
}

void ScriptApiTest::init()
{}

void ScriptApiTest::cleanup()
{}

void ScriptApiTest::cleanupTestCase()
{
}

void ScriptApiTest::helperAddDaysToDateTest_data()
{
    QTest::addColumn<QDateTime>("dateTime");
    QTest::addColumn<int>("daysToAdd");
    QTest::addColumn<QDateTime>("result");
    QTest::newRow("A") << QDateTime( QDate(2010, 3, 5), QTime(11, 10, 3) ) << 5
                       << QDateTime( QDate(2010, 3, 10), QTime(11, 10, 3) );
    QTest::newRow("B") << QDateTime( QDate(2012, 3, 7), QTime(7, 7, 7) ) << 53
                       << QDateTime( QDate(2012, 4, 29), QTime(7, 7, 7) );
}

void ScriptApiTest::helperAddDaysToDateTest()
{
    QFETCH(QDateTime, dateTime);
    QFETCH(int, daysToAdd);
    QFETCH(QDateTime, result);
    QCOMPARE( ScriptApi::Helper::addDaysToDate(dateTime, daysToAdd), result );
}

void ScriptApiTest::helperAddDaysToDate2Test_data()
{
    QTest::addColumn<QString>("dateTime");
    QTest::addColumn<int>("daysToAdd");
    QTest::addColumn<QString>("format");
    QTest::addColumn<QString>("result");
    QTest::newRow("A") << "22.05.03" << 5 << "dd.MM.yy" << "27.05.03";
    QTest::newRow("B") << "21.08.2011" << 14 << "dd.MM.yyyy" << "04.09.2011";
    QTest::newRow("C") << "2002-11-02" << 21 << "yyyy-MM-dd" << "2002-11-23";
}

void ScriptApiTest::helperAddDaysToDate2Test()
{
    QFETCH(QString, dateTime);
    QFETCH(int, daysToAdd);
    QFETCH(QString, format);
    QFETCH(QString, result);
    QCOMPARE( ScriptApi::Helper::addDaysToDate(dateTime, daysToAdd, format), result );
}

void ScriptApiTest::helperAddMinsToTimeTest_data()
{
    QTest::addColumn<QString>("time");
    QTest::addColumn<int>("minsToAdd");
    QTest::addColumn<QString>("format");
    QTest::addColumn<QString>("result");
    QTest::newRow("A") << "04:10" << 65 << "hh:mm" << "05:15";
    QTest::newRow("B") << "21:28" << 14 << "hh:mm" << "21:42";
    QTest::newRow("C") << "5:55" << 21 << "h:mm" << "6:16";
}

void ScriptApiTest::helperAddMinsToTimeTest()
{
    QFETCH(QString, time);
    QFETCH(int, minsToAdd);
    QFETCH(QString, format);
    QFETCH(QString, result);
    QCOMPARE( ScriptApi::Helper::addMinsToTime(time, minsToAdd, format), result );
}

void ScriptApiTest::helperMatchDateTest_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("format");
    QTest::addColumn<QDate>("result");
    QTest::newRow("A") << "22.02.2011" << "dd.MM.yyyy" << QDate(2011, 2, 22);
    QTest::newRow("B") << "1.12.2011" << "d.MM.yyyy" << QDate(2011, 12, 1);
    QTest::newRow("C") << "2002-01-06" << "yyyy-MM-dd" << QDate(2002, 1, 6);
}

void ScriptApiTest::helperMatchDateTest()
{
    QFETCH(QString, string);
    QFETCH(QString, format);
    QFETCH(QDate, result);
    QCOMPARE( ScriptApi::Helper::matchDate(string, format), result );
}

QVariantMap timeResult( int hour, int minute ) {
    QVariantMap map;
    map.insert( "hour", hour );
    map.insert( "minute", minute );
    return map;
}

void ScriptApiTest::helperMatchTimeTest_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("format");
    QTest::addColumn<QVariantMap>("result");
    QTest::newRow("A") << "6:45" << "h:mm" << timeResult(6, 45);
    QTest::newRow("B") << "07:00" << "hh:mm" << timeResult(7, 0);
    QTest::newRow("C") << "9:23" << "h:mm" << timeResult(9, 23);
}

void ScriptApiTest::helperMatchTimeTest()
{
    QFETCH(QString, string);
    QFETCH(QString, format);
    QFETCH(QVariantMap, result);
    QCOMPARE( ScriptApi::Helper::matchTime(string, format), result );
}

void ScriptApiTest::helperFormatDateTest_data()
{
    QTest::addColumn<int>("year");
    QTest::addColumn<int>("month");
    QTest::addColumn<int>("day");
    QTest::addColumn<QString>("format");
    QTest::addColumn<QString>("result");
    QTest::newRow("A") << 2011 << 10 << 2 << "yyyy-MM-dd" << "2011-10-02";
    QTest::newRow("B") << 2011 << 10 << 2 << "yy-MM-d" << "11-10-2";
    QTest::newRow("C") << 2011 << 10 << 12 << "dd.MM.yyyy" << "12.10.2011";
}

void ScriptApiTest::helperFormatDateTest()
{
    QFETCH(int, year);
    QFETCH(int, month);
    QFETCH(int, day);
    QFETCH(QString, format);
    QFETCH(QString, result);
    QCOMPARE( ScriptApi::Helper::formatDate(year, month, day, format), result );
}

void ScriptApiTest::helperFormatTimeTest_data()
{
    QTest::addColumn<int>("hour");
    QTest::addColumn<int>("minute");
    QTest::addColumn<QString>("format");
    QTest::addColumn<QString>("result");
    QTest::newRow("A") << 4 << 15 << "hh:mm" << "04:15";
    QTest::newRow("B") << 6 << 35 << "h:mm" << "6:35";
    QTest::newRow("C") << 16 << 5 << "hh-m" << "16-5";
}

void ScriptApiTest::helperFormatTimeTest()
{
    QFETCH(int, hour);
    QFETCH(int, minute);
    QFETCH(QString, format);
    QFETCH(QString, result);
    QCOMPARE( ScriptApi::Helper::formatTime(hour, minute, format), result );
}

void ScriptApiTest::helperFormatDateTimeTest_data()
{
    QTest::addColumn<int>("year");
    QTest::addColumn<int>("month");
    QTest::addColumn<int>("day");
    QTest::addColumn<int>("hour");
    QTest::addColumn<int>("minute");
    QTest::addColumn<QString>("format");
    QTest::addColumn<QString>("result");
    QTest::newRow("A") << 2011 << 10 << 2   << 4 << 15 << "yyyy-MM-dd hh:mm" << "2011-10-02 04:15";
    QTest::newRow("B") << 2011 << 10 << 2   << 6 << 35 << "yy-MM-d h:mm" << "11-10-2 6:35";
    QTest::newRow("C") << 2011 << 10 << 12  << 16 << 5 << "dd.MM.yyyy hh-m" << "12.10.2011 16-5";
}

void ScriptApiTest::helperFormatDateTimeTest()
{
    QFETCH(int, year);
    QFETCH(int, month);
    QFETCH(int, day);
    QFETCH(int, hour);
    QFETCH(int, minute);
    QFETCH(QString, format);
    QFETCH(QString, result);
    QCOMPARE( ScriptApi::Helper::formatDateTime(QDateTime(QDate(year, month, day),
                                                          QTime(hour, minute)), format), result );
}

void ScriptApiTest::helperCamelCaseTest_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("result");
    QTest::newRow("A") << "testTEstteST" << "Testtesttest";
    QTest::newRow("B") << "oNeTWo tHree Four FivE" << "Onetwo Three Four Five";
    QTest::newRow("C") << "test teST TesT" << "Test Test Test";
}

void ScriptApiTest::helperCamelCaseTest()
{
    QFETCH(QString, string);
    QFETCH(QString, result);
    QCOMPARE( ScriptApi::Helper::camelCase(string), result );
}

void ScriptApiTest::helperTrimTest_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("result");
    QTest::newRow("A") << "  word     " << "word";
    QTest::newRow("B") << "&nbsp;  word  &nbsp;  " << "word";
}

void ScriptApiTest::helperTrimTest()
{
    QFETCH(QString, string);
    QFETCH(QString, result);
    QCOMPARE( ScriptApi::Helper::trim(string), result );
}

void ScriptApiTest::helpeStripTagsTest_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("result");
    QTest::newRow("A") << "<div class=\"test\">word</div> <p>another<p style=\"></p>\"> word</p>"
                       << "word another word";
}

void ScriptApiTest::helpeStripTagsTest()
{
    QFETCH(QString, string);
    QFETCH(QString, result);
    QCOMPARE( ScriptApi::Helper::stripTags(string), result );
}

void ScriptApiTest::helperSplitSkipEmptyPartsTest_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("separator");
    QTest::addColumn<QStringList>("result");
    QTest::newRow("A") << "one,two,,four,,, ,five" << ","
            << (QStringList() << "one" << "two" << "four" << " " << "five");
}

void ScriptApiTest::helperSplitSkipEmptyPartsTest()
{
    QFETCH(QString, string);
    QFETCH(QString, separator);
    QFETCH(QStringList, result);
    QCOMPARE( ScriptApi::Helper::splitSkipEmptyParts(string, separator), result );
}

void ScriptApiTest::helperDecodeHtmlEntitiesTest_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("result");
    QTest::newRow("A") << "one&nbsp;two&amp;three" << "one two&three";
    QTest::newRow("B") << "&#188;&#182;&amp;&#62;" << QString::fromUtf8("¼¶&>");
    QTest::newRow("C") << "&lt;div&gt;Test-Element&lt;/div&gt;" << "<div>Test-Element</div>";
}

void ScriptApiTest::helperDecodeHtmlEntitiesTest()
{
    QFETCH(QString, string);
    QFETCH(QString, result);
    QCOMPARE( ScriptApi::Helper::decodeHtmlEntities(string), result );
}

void ScriptApiTest::helperDurationTest_data()
{
    QTest::addColumn<QString>("time1");
    QTest::addColumn<QString>("time2");
    QTest::addColumn<QString>("format");
    QTest::addColumn<int>("result");
    QTest::newRow("A") << "12:05" << "13:13" << "hh:mm" << 68;
    QTest::newRow("B") << "5:05" << "7:11" << "h:mm" << 126;
    QTest::newRow("C") << "23:35" << "00:25" << "hh:mm" << -1390;
}

void ScriptApiTest::helperDurationTest()
{
    QFETCH(QString, time1);
    QFETCH(QString, time2);
    QFETCH(QString, format);
    QFETCH(int, result);
    QCOMPARE( ScriptApi::Helper::duration(time1, time2, format), result );
}

void ScriptApiTest::helperFindFirstHtmlTagTest_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("tagName");
    QTest::addColumn<QVariantMap>("options");
    QTest::addColumn<QVariantMap>("expectedResult");

    QVariantMap optionsA;
    QVariantMap optionsAttributesA;
    optionsAttributesA.insert( "class", "test?" );
    optionsA.insert( "attributes", optionsAttributesA );
//     optionsA.insert( "debug", true );
    QVariantMap resultA;
    QVariantMap resultAttributesA;
    resultAttributesA.insert( "class", "test" );
    resultA.insert( "found", true );
    resultA.insert( "contents", "Paragraph 2" );
    resultA.insert( "position", 36 );
    resultA.insert( "endPosition", 67 );
    resultA.insert( "attributes", resultAttributesA );

    QTest::newRow("A") << "<div class=\"test\"><p>Paragraph 1</p>"
                          "<p class=\"test\">Paragraph 2</p></div>"
                       << "p" << optionsA << resultA;

    resultA.insert( "position", 27 );
    resultA.insert( "endPosition", 65 );
    resultA.insert( "contents", "Paragraph <p>2</p>" );
    QTest::newRow("B") << "<div class=\"test\"><p>Parent<p class=\"test\">Paragraph <p>2</p>"
                          "</p>more parent text</p></div>"
                       << "p" << optionsA << resultA;
}

void ScriptApiTest::helperFindFirstHtmlTagTest()
{
    QFETCH(QString, string);
    QFETCH(QString, tagName);
    QFETCH(QVariantMap, options);
    QFETCH(QVariantMap, expectedResult);

    QVariantMap results = ScriptApi::Helper::findFirstHtmlTag( string, tagName, options );
    for ( QVariantMap::ConstIterator it = expectedResult.constBegin();
          it != expectedResult.constEnd(); ++it )
    {
//         qDebug() << it.key() << "EXPECTED:" << it.value() << "\nRESULT:" << results[it.key()];
        QVERIFY( results.contains(it.key()) );
        QCOMPARE( results[it.key()], it.value() );
    }
}

void ScriptApiTest::helperFindHtmlTagsTest_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("tagName");
    QTest::addColumn<QVariantMap>("options");
    QTest::addColumn<int>("expectedResultCount");
    QTest::addColumn<QVariantList>("expectedResults");

    // Test searching for <p>-tags with a "class" attribute with an arbitrary value
    QVariantMap optionsA;
//     optionsA.insert( "debug", true );
    QVariantMap optionsAttributesA;
    optionsAttributesA.insert( "class", "" ); // Match only tags with a class attribute
    optionsA.insert( "attributes", optionsAttributesA );
    QVariantMap resultsA1;
    QVariantMap resultAttributesA1;
    resultAttributesA1.insert( "class", "test" );
    resultsA1.insert( "contents", "Paragraph 2" );
    resultsA1.insert( "position", 36 );
    resultsA1.insert( "endPosition", 67 );
    resultsA1.insert( "attributes", resultAttributesA1 );
    QTest::newRow("A") << "<div class=\"test\"><p>Paragraph 1</p>"
                          "<p class=\"test\">Paragraph 2</p></div>"
                       << "p" << optionsA << 1 << (QVariantList() << resultsA1);

    // Test searching for <p>-tags with any number of attributes
    // Test child tags with the same name
    QVariantMap optionsB;
    QVariantMap resultsB1;
    resultsB1.insert( "contents", "Parent" );
    resultsB1.insert( "position", 18 );
    resultsB1.insert( "endPosition", 31 );
    resultsB1.insert( "attributes", QVariantMap() );
    QVariantMap resultsB2;
    QVariantMap resultAttributesB2;
    resultAttributesB2.insert( "class", "test" );
    resultsB2.insert( "contents", "Paragraph <p>2</p>" );
    resultsB2.insert( "position", 31 );
    resultsB2.insert( "endPosition", 69 );
    resultsB2.insert( "attributes", resultAttributesB2 );
    QTest::newRow("B") << "<div class=\"test\"><p>Parent</p>"
                          "<p class='test'>Paragraph <p>2</p></p>more parent text</div>"
                       << "p" << optionsB << 2 << (QVariantList() << resultsB1 << resultsB2);

    // Test "maxCount" option
    optionsB.insert( "maxCount", 1 );
    QTest::newRow("option \"maxCount\"") << "<div class=\"test\"><p>Parent</p>"
                           "<p class='test'>Paragraph <p>2</p></p>more parent text</div>"
                        << "p" << optionsB << 1 << (QVariantList() << resultsB1);

    // Test searching for <div>-tags with any number of attributes
    // Test child tags with the same name, test attributes with different quotes,
    // test attribute value that could cause problems when not detected as attribute value
    QVariantMap optionsC;
    QVariantMap resultsC1;
    QVariantMap resultAttributesC1;
    resultAttributesC1.insert( "class", "test" );
    resultsC1.insert( "contents", "Parent <div>Child</div>" );
    resultsC1.insert( "position", 0 );
    resultsC1.insert( "endPosition", 47 );
    resultsC1.insert( "attributes", resultAttributesC1 );
    QVariantMap resultsC2;
    QVariantMap resultAttributesC2;
    resultAttributesC2.insert( "class", "test" );
    resultsC2.insert( "contents", "Paragraph <p>2</p>" );
    resultsC2.insert( "position", 47 );
    resultsC2.insert( "endPosition", 89 );
    resultsC2.insert( "attributes", resultAttributesC2 );
    QVariantMap resultsC3;
    QVariantMap resultAttributesC3;
    resultAttributesC3.insert( "style", "> </div>" );
    resultAttributesC3.insert( "width", "100" );
    resultsC3.insert( "contents", "more parent text" );
    resultsC3.insert( "position", 89 );
    resultsC3.insert( "endPosition", 143 );
    resultsC3.insert( "attributes", resultAttributesC3 );
    QTest::newRow("C") << "<div class=\"test\">Parent <div>Child</div></div>"
                          "<div class='test'>Paragraph <p>2</p></div>"
                          "<div style=\"> </div>\" width=100>more parent text</div>"
                       << "div" << optionsC << 3
                       << (QVariantList() << resultsC1 << resultsC2 << resultsC3);

    // Test "maxCount" option
    optionsC.insert( "maxCount", 2 );
    QTest::newRow("option \"maxCount\"") << "<div class=\"test\">Parent <div>Child</div></div>"
                           "<div class='test'>Paragraph <p>2</p></div>"
                           "<div style=\"> </div>\" width=100>more parent text</div>"
                        << "div" << optionsC << 2 << (QVariantList() << resultsC1 << resultsC2);

    // Test "maxCount" option
    optionsC.insert( "maxCount", 1 );
    QTest::newRow("option \"maxCount\"") << "<div class=\"test\">Parent <div>Child</div></div>"
                           "<div class='test'>Paragraph <p>2</p></div>"
                           "<div style=\"> </div>\" width=100>more parent text</div>"
                        << "div" << optionsC << 1 << (QVariantList() << resultsC1);

    // Test "contentsRegExp" option
    optionsC.remove( "maxCount" );
    optionsC.insert( "contentsRegExp", "^Par.*" );
    QTest::newRow("option \"contentsRegExp\"") << "<div class=\"test\">Parent <div>Child</div></div>"
                           "<div class='test'>Paragraph <p>2</p></div>"
                           "<div style=\"> </div>\" width=100>more parent text</div>"
                        << "div" << optionsC << 2 << (QVariantList() << resultsC1 << resultsC2);

    // Test "noNesting" option
    optionsC.remove( "contentsRegExp" );
    optionsC.insert( "noNesting", true );
    QVariantMap resultsC4;
    resultsC4.insert( "contents", "Parent <div>Child" );
    resultsC4.insert( "position", 0 );
    resultsC4.insert( "endPosition", 41 );
    resultsC4.insert( "attributes", resultAttributesC1 );
    QVariantMap resultsC5;
    resultsC5.insert( "contents", "Paragraph <div>2" );
    resultsC5.insert( "position", 47 );
    resultsC5.insert( "endPosition", 87 );
    resultsC5.insert( "attributes", resultAttributesC1 );
    QTest::newRow("option \"noNesting\"") << "<div class=\"test\">Parent <div>Child</div></div>"
                           "<div class=\"test\">Paragraph <div>2</div></div>"
                        << "div" << optionsC << 2 << (QVariantList() << resultsC4 << resultsC5);

    // Test "position" option
    optionsC.remove( "noNesting" );
    optionsC.insert( "position", 25 );
    QVariantMap resultsC6;
    resultsC6.insert( "contents", "Child" );
    resultsC6.insert( "position", 25 );
    resultsC6.insert( "endPosition", 41 );
    QVariantMap resultsC7;
    resultsC7.insert( "contents", "Paragraph <div>2</div>" );
    resultsC7.insert( "position", 47 );
    resultsC7.insert( "endPosition", 93 );
    resultsC7.insert( "attributes", resultAttributesC1 );
    QTest::newRow("option \"position\"") << "<div class=\"test\">Parent <div>Child</div></div>"
                           "<div class=\"test\">Paragraph <div>2</div></div>"
                        << "div" << optionsC << 2 << (QVariantList() << resultsC6 << resultsC7);

    // Test searching for <img />-tags without content ("noContent" option),
    // but with a "src" attribute
    QVariantMap optionsD;
//     optionsA.insert( "debug", true );
    QVariantMap optionsAttributesD;
    optionsAttributesD.insert( "src", "" ); // Match only tags with a src attribute
    optionsD.insert( "attributes", optionsAttributesD );
    optionsD.insert( "noContent", true );
    QVariantMap resultsD1;
    QVariantMap resultAttributesD1;
    resultAttributesD1.insert( "src", "test.png" );
    resultsD1.insert( "contents", QString() );
    resultsD1.insert( "position", 0 );
    resultsD1.insert( "endPosition", 20 );
    resultsD1.insert( "attributes", resultAttributesD1 );
    QVariantMap resultsD2;
    QVariantMap resultAttributesD2;
    resultAttributesD2.insert( "src", "two.png" );
    resultsD2.insert( "contents", QString() );
    resultsD2.insert( "position", 20 );
    resultsD2.insert( "endPosition", 40 );
    resultsD2.insert( "attributes", resultAttributesD2 );
    QVariantMap resultsD3;
    QVariantMap resultAttributesD3;
    resultAttributesD3.insert( "src", "s.jpeg" );
    resultsD3.insert( "contents", QString() );
    resultsD3.insert( "position", 40 );
    resultsD3.insert( "endPosition", 60 );
    resultsD3.insert( "attributes", resultAttributesD3 );
    QTest::newRow("D") << "<img src=\"test.png\"><img src='two.png'/>"
                          "<img src=\"s.jpeg\" />"
                       << "img" << optionsD << 3
                       << (QVariantList() << resultsD1 << resultsD2 << resultsD3);
}

void ScriptApiTest::helperFindHtmlTagsTest()
{
    QFETCH(QString, string);
    QFETCH(QString, tagName);
    QFETCH(QVariantMap, options);
    QFETCH(int, expectedResultCount);
    QFETCH(QVariantList, expectedResults);

    QVariantList results = ScriptApi::Helper::findHtmlTags( string, tagName, options );
    QCOMPARE( results.count(), expectedResultCount );
    for ( int i = 0; i < expectedResultCount; ++i ) {
        const QVariantMap expectedResult = expectedResults[i].toMap();
        const QVariantMap result = results[i].toMap();
        for ( QVariantMap::ConstIterator it = expectedResult.constBegin();
            it != expectedResult.constEnd(); ++it )
        {
            QVERIFY( result.contains(it.key()) );
            QCOMPARE( result[it.key()], it.value() );
        }
    }
}

void ScriptApiTest::helperFindNamedHtmlTagsTest_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("tagName");
    QTest::addColumn<QVariantMap>("options");
    QTest::addColumn<int>("expectedResultCount");
    QTest::addColumn<QVariantMap>("expectedResults");
    QTest::addColumn<QStringList>("expectedNames");

    // Test searching for <td>-tags with a "class" attribute with an arbitrary value
    // The first two columns have ambiguous names, the second one should get a "2" appended (addNumber)
    QVariantMap optionsA;
//     optionsA.insert( "debug", true );
    optionsA.insert( "ambiguousNameResolution", "addNumber" );
    QVariantMap optionsNamePositionA;
    optionsNamePositionA.insert( "type", "attribute" );
    optionsNamePositionA.insert( "name", "class" );
    optionsNamePositionA.insert( "regexp", "\\W+" );
    optionsA.insert( "namePosition", optionsNamePositionA );
    QVariantMap optionsAttributesA;
    optionsAttributesA.insert( "class", "" ); // Match only tags with a class attribute
    optionsA.insert( "attributes", optionsAttributesA );
    QVariantMap resultsA1, resultsA2, resultsA3;
    QVariantMap resultAttributesA1, resultAttributesA2, resultAttributesA3;
    resultAttributesA1.insert( "class", "A" );
    resultAttributesA2.insert( "class", "A" );
    resultAttributesA3.insert( "class", "C" );
    resultsA1.insert( "contents", "Column 1" );
    resultsA1.insert( "position", 0 );
    resultsA1.insert( "endPosition", 27 );
    resultsA1.insert( "attributes", resultAttributesA1 );
    resultsA2.insert( "contents", "Column <td>2</td>" );
    resultsA2.insert( "position", 27 );
    resultsA2.insert( "endPosition", 63 );
    resultsA2.insert( "attributes", resultAttributesA2 );
    resultsA3.insert( "contents", "Column 3" );
    resultsA3.insert( "position", 63 );
    resultsA3.insert( "endPosition", 90 );
    resultsA3.insert( "attributes", resultAttributesA3 );
    QVariantMap resultsMapA;
    resultsMapA.insert( "A", resultsA1 ); // The key is the found name for the result
    resultsMapA.insert( "A2", resultsA2 );
    resultsMapA.insert( "C", resultsA3 );
    QTest::newRow("A") << "<td class=\"A\">Column 1</td>"
                          "<td class=\"A\">Column <td>2</td></td>"
                          "<td class=\"C\">Column 3</td>"
                       << "td" << optionsA << 3 << resultsMapA
                       << (QStringList() << "A" << "A2" << "C");

    // Test other "ambiguousNameResolution" value, the default
    optionsA.insert( "ambiguousNameResolution", "replace" );
    // Replace old "A" with "A2" and remove "A2",
    // because the second <td>-tag with class "A" (old name "A2") now replaces the first one
    // (old name "A").
    resultsMapA.insert( "A", resultsMapA.take("A2") );
    QTest::newRow("B") << "<td class=\"A\">Column 1</td>"
                          "<td class=\"A\">Column <td>2</td></td>"
                          "<td class=\"C\">Column 3</td>"
                       << "td" << optionsA << 2 << resultsMapA
                       << (QStringList() << "A" << "C");

    // Test without ambiguous names
    // Restore first "A" result and change expected "class" attribute of the second column to "B"
    resultsMapA.insert( "A", resultsA1 );
    resultAttributesA2.insert( "class", "B" );
    resultsA2.insert( "attributes", resultAttributesA2 );
    resultsMapA.insert( "B", resultsA2 );
    QTest::newRow("C") << "<td class=\"A\">Column 1</td>"
                          "<td class=\"B\">Column <td>2</td></td>"
                          "<td class=\"C\">Column 3</td>"
                       << "td" << optionsA << 3 << resultsMapA
                       << (QStringList() << "A" << "B" << "C");
}

void ScriptApiTest::helperFindNamedHtmlTagsTest()
{
    QFETCH(QString, string);
    QFETCH(QString, tagName);
    QFETCH(QVariantMap, options);
    QFETCH(int, expectedResultCount);
    QFETCH(QVariantMap, expectedResults);
    QFETCH(QStringList, expectedNames);

    QVariantMap results = ScriptApi::Helper::findNamedHtmlTags( string, tagName, options );
    QCOMPARE( results.count() - 1, expectedResultCount ); // -1 for the "names" entry which contains all found names
    QVERIFY( results.contains("names") );
    QCOMPARE( results["names"].toStringList(), expectedNames );
    for ( QVariantMap::ConstIterator it = results.constBegin(); it != results.constEnd(); ++it ) {
        const QVariantMap expectedResult = expectedResults[it.key()].toMap();
        const QVariantMap result = it->toMap();
        for ( QVariantMap::ConstIterator itExpected = expectedResult.constBegin();
              itExpected != expectedResult.constEnd(); ++itExpected )
        {
            QVERIFY( result.contains(itExpected.key()) );
            QCOMPARE( result[itExpected.key()], itExpected.value() );
        }
    }
}

void ScriptApiTest::storageReadWriteTest_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QVariant>("data");
    QTest::newRow("integer") << "Test" << QVariant(5);
    QTest::newRow("string") << "Test2" << QVariant("abc&ABC_¼¶");

    QVariantMap map;
    map.insert( "test1", "abc" );
    map.insert( "test2", "123" );
    QTest::newRow("map") << "Test3" << QVariant(map);
}

void ScriptApiTest::storageReadWriteTest()
{
    QFETCH(QString, name);
    QFETCH(QVariant, data);

    ScriptApi::Storage storage( "Test" );
    storage.write( name, data );
    QVERIFY( storage.hasData(name) );

    QCOMPARE( storage.read(name), data );

    storage.remove( name );
    QVERIFY( !storage.hasData(name) );
}

void ScriptApiTest::storageReadWritePersistentTest_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QVariant>("data");
    QTest::addColumn<int>("lifetime");
    QTest::addColumn<int>("expectedLifetime");

    QTest::newRow("integer") << "Test" << QVariant(5) << 45 << 30;
    QTest::newRow("string") << "Test2" << QVariant("abc&ABC_¼¶") << 10 << 10;
    QTest::newRow("date") << "Test3" << QVariant(QDate(2012, 01, 04)) << 1 << 1;
    QTest::newRow("time") << "Test4" << QVariant(QTime(12, 15, 23)) << 6 << 6;
    QTest::newRow("datetime") << "Test5" << QVariant(QDateTime(QDate(2012, 01, 04), QTime(12, 15, 23))) << 6 << 6;
    QTest::newRow("list") << "Test6" << QVariant(QStringList() << "test1" << "abc") << 2 << 2;
    QTest::newRow("variantlist") << "Test7" << QVariant(QVariantList() << 5 << "abc") << 2 << 2;

    QVariantMap map;
    map.insert( "test1", "abc" );
    map.insert( "test2", "123" );
    QTest::newRow("map") << "Test8" << QVariant(map) << 3 << 3;
}

void ScriptApiTest::storageReadWritePersistentTest()
{
    QFETCH(QString, name);
    QFETCH(QVariant, data);
    QFETCH(int, lifetime);
    QFETCH(int, expectedLifetime);

    ScriptApi::Storage storage( "Test" );
    storage.writePersistent( name, data, lifetime );
    QVERIFY( storage.hasPersistentData(name) );

    QCOMPARE( storage.readPersistent(name), data );
    QCOMPARE( storage.lifetime(name), expectedLifetime );

    storage.removePersistent( name );
    QVERIFY( !storage.hasPersistentData(name) );
}

void ScriptApiTest::resultFeaturesHintsTest()
{
    ScriptApi::ResultObject result( this );

    // Test defaults
    QCOMPARE( result.features(), ScriptApi::ResultObject::DefaultFeatures );
    const bool defaultAutoDecode = ScriptApi::ResultObject::Features(ScriptApi::ResultObject::DefaultFeatures)
              .testFlag(ScriptApi::ResultObject::AutoDecodeHtmlEntities);
    const bool defaultAutoPublish = ScriptApi::ResultObject::Features(ScriptApi::ResultObject::DefaultFeatures)
              .testFlag(ScriptApi::ResultObject::AutoPublish);
    const bool defaultAutoRemoveCityFromStopNames = ScriptApi::ResultObject::Features(ScriptApi::ResultObject::DefaultFeatures)
              .testFlag(ScriptApi::ResultObject::AutoRemoveCityFromStopNames);
    QCOMPARE( result.isFeatureEnabled(ScriptApi::ResultObject::AutoDecodeHtmlEntities),
              defaultAutoDecode );
    QCOMPARE( result.isFeatureEnabled(ScriptApi::ResultObject::AutoPublish), defaultAutoPublish );
    QCOMPARE( result.isFeatureEnabled(ScriptApi::ResultObject::AutoRemoveCityFromStopNames),
              defaultAutoRemoveCityFromStopNames );
    QCOMPARE( result.hints(), ScriptApi::ResultObject::NoHint );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::CityNamesAreLeft), false );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::CityNamesAreRight), false );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::DatesNeedAdjustment), false );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::NoDelaysForStop), false );

    result.enableFeature( ScriptApi::ResultObject::AutoDecodeHtmlEntities, false );
    QCOMPARE( result.isFeatureEnabled(ScriptApi::ResultObject::AutoDecodeHtmlEntities), false );
    QCOMPARE( result.isFeatureEnabled(ScriptApi::ResultObject::AutoPublish), defaultAutoPublish );
    QCOMPARE( result.isFeatureEnabled(ScriptApi::ResultObject::AutoRemoveCityFromStopNames),
              defaultAutoRemoveCityFromStopNames );
//     QCOMPARE( result.features(), ScriptApi::ResultObject::AutoPublish |
//                                  ScriptApi::ResultObject::AutoRemoveCityFromStopNames );

    result.enableFeature( ScriptApi::ResultObject::AutoPublish, false );
    QCOMPARE( result.isFeatureEnabled(ScriptApi::ResultObject::AutoDecodeHtmlEntities), false );
    QCOMPARE( result.isFeatureEnabled(ScriptApi::ResultObject::AutoPublish), false );
    QCOMPARE( result.isFeatureEnabled(ScriptApi::ResultObject::AutoRemoveCityFromStopNames),
              defaultAutoRemoveCityFromStopNames );
//     QCOMPARE( result.features(), ScriptApi::ResultObject::AutoRemoveCityFromStopNames );

    result.giveHint( ScriptApi::ResultObject::CityNamesAreLeft, true );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::CityNamesAreLeft), true );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::CityNamesAreRight), false );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::DatesNeedAdjustment), false );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::NoDelaysForStop), false );
    QCOMPARE( result.hints(), ScriptApi::ResultObject::CityNamesAreLeft );

    // Test automatic disabling of CityNamesAreLeft if CityNamesAreRight gets enabled
    result.giveHint( ScriptApi::ResultObject::CityNamesAreRight, true );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::CityNamesAreLeft), false );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::CityNamesAreRight), true );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::DatesNeedAdjustment), false );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::NoDelaysForStop), false );
    QCOMPARE( result.hints(), ScriptApi::ResultObject::CityNamesAreRight );

    result.giveHint( ScriptApi::ResultObject::DatesNeedAdjustment, true );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::CityNamesAreLeft), false );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::CityNamesAreRight), true );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::DatesNeedAdjustment), true );
    QCOMPARE( result.isHintGiven(ScriptApi::ResultObject::NoDelaysForStop), false );
    QCOMPARE( result.hints(), ScriptApi::ResultObject::CityNamesAreRight |
                              ScriptApi::ResultObject::DatesNeedAdjustment );
}

void ScriptApiTest::resultDataTest()
{
    ScriptApi::ResultObject result( this );

    // Test defaults
    QVERIFY( !result.hasData() );

    // Add data
    QVariantMap map;
    map.insert( "DepartureTime", QTime(11, 10) );
    map.insert( "TypeOfVehicle", "HighSpeedTrain" );
    map.insert( "TransportLine", "N1" );
    map.insert( "Target", "Test-Target" );
    result.addData( map );
    QVERIFY( result.hasData() );
    QCOMPARE( result.count(), 1 );

    // Test added data
    QList< TimetableData > data = result.data();
    QCOMPARE( data.count(), 1 );
    const TimetableData first = data.first();
    QCOMPARE( first[Enums::DepartureTime].toTime(), QTime(11, 10) );
    QCOMPARE( first[Enums::TypeOfVehicle].toString(), QString("HighSpeedTrain") );
    QCOMPARE( first[Enums::TransportLine].toString(), QString("N1") );
    QCOMPARE( first[Enums::Target].toString(), QString("Test-Target") );

    // Test clear()
    result.clear();
    QVERIFY( !result.hasData() );
    QCOMPARE( result.count(), 0 ); // Just cleared, should not contain anything

    // Test AutoPublish feature, should emit publish() after every 10 datasets
    QSignalSpy spy( &result, SIGNAL(publish()) );
    for ( int i = 0; i < 10; ++i ) {
        result.addData( map );
    }
    QCOMPARE( spy.count(), 1 );
    QCOMPARE( result.count(), 10 );

    // Test other TimetableInformation data
    QVariantMap mapRoute;
    const QStringList routeStops = QStringList() << "StopA" << "StopB" << "StopC";
    const QStringList routeTimes = QStringList() << "10:15" << "11:33" << "04:14";
    mapRoute.insert( "RouteStops", routeStops );
    mapRoute.insert( "RouteTimes", routeTimes );
    result.addData( mapRoute );
    QVERIFY( result.hasData() );
    QCOMPARE( result.count(), 11 ); // Contains previous 10 + 1 new one

    // Test added data
    data = result.data();
    const TimetableData dataRoute = data.at( 10 );
    QCOMPARE( dataRoute.count(), 2 ); // Contains RouteStops and RouteTimes
    QCOMPARE( dataRoute[Enums::RouteStops].toStringList(), routeStops );
    QCOMPARE( dataRoute[Enums::RouteTimes].toStringList(), routeTimes );
}

void ScriptApiTest::networkSynchronousTest()
{
    ScriptApi::Network network;

    // Test synchronous download (max 10 seconds)
    const QString url = "http://www.google.de";
    const QString downloaded = network.downloadSynchronous( url, url, 10000 );
    QVERIFY( !downloaded.isEmpty() );
    QVERIFY( downloaded.indexOf("<html") != -1 );
    QCOMPARE( network.lastUrl(), url );

    // Test Network::clear()
    network.clear();
    QCOMPARE( network.lastUrl(), QString() );
}

void ScriptApiTest::networkAsynchronousTest()
{
    ScriptApi::Network network;

    // Signals are only emitted for asynchronous access
    QSignalSpy requestStartedSpy( &network, SIGNAL(requestStarted(NetworkRequest::Ptr)) );
    QSignalSpy requestFinishedSpy( &network, SIGNAL(requestFinished(NetworkRequest::Ptr)) );
    QSignalSpy requestAbortedSpy( &network, SIGNAL(requestAborted(NetworkRequest::Ptr)) );
    QSignalSpy allRequestsFinishedSpy( &network, SIGNAL(allRequestsFinished()) );

    // Test synchronous download (max 10 seconds)
    const QString url = "http://www.google.de";
    ScriptApi::NetworkRequest *request = network.createRequest( url );

    // Wait for asynchronous download to finish
    QEventLoop loop( this );
    connect( request, SIGNAL(finished()), &loop, SLOT(quit()) );
    network.head( request ); // Use head() to save network bandwidth
    loop.exec();

    QCOMPARE( requestStartedSpy.count(), 1 );
    QCOMPARE( requestFinishedSpy.count(), 1 );
    QCOMPARE( requestAbortedSpy.count(), 0 );
    QCOMPARE( allRequestsFinishedSpy.count(), 1 );

    QCOMPARE( network.hasRunningRequests(), false );
    QCOMPARE( network.lastUrl(), url );
}

void ScriptApiTest::networkAsynchronousAbortTest()
{
    ScriptApi::Network network;

    // Signals are only emitted for asynchronous access
    QSignalSpy requestStartedSpy( &network, SIGNAL(requestStarted(NetworkRequest::Ptr)) );
    QSignalSpy requestFinishedSpy( &network, SIGNAL(requestFinished(NetworkRequest::Ptr)) );
    QSignalSpy requestAbortedSpy( &network, SIGNAL(requestAborted(NetworkRequest::Ptr)) );
    QSignalSpy allRequestsFinishedSpy( &network, SIGNAL(allRequestsFinished()) );

    // Test synchronous download (max 10 seconds)
    const QString url = "http://www.google.de";
    ScriptApi::NetworkRequest *request = network.createRequest( url );

    // Start asynchronous download and wait for it to finish,
    // but directly abort the download
    QEventLoop loop( this );
    connect( request, SIGNAL(finished()), &loop, SLOT(quit()) );
    network.head( request ); // Use head() to save network bandwidth
    QTimer::singleShot( 50, request, SLOT(abort()) );
    loop.exec();

    QCOMPARE( requestStartedSpy.count(), 1 );
    QCOMPARE( requestFinishedSpy.count(), 1 );
    QCOMPARE( requestAbortedSpy.count(), 1 );
    QCOMPARE( allRequestsFinishedSpy.count(), 1 );

    QCOMPARE( network.lastDownloadAborted(), true );
    QCOMPARE( network.hasRunningRequests(), false );
    QCOMPARE( network.lastUrl(), url );
}

void ScriptApiTest::networkAsynchronousMultipleTest()
{
    ScriptApi::Network network;

    // Signals are only emitted for asynchronous access
    QSignalSpy requestStartedSpy( &network, SIGNAL(requestStarted(NetworkRequest::Ptr)) );
    QSignalSpy requestFinishedSpy( &network, SIGNAL(requestFinished(NetworkRequest::Ptr)) );
    QSignalSpy requestAbortedSpy( &network, SIGNAL(requestAborted(NetworkRequest::Ptr)) );
    QSignalSpy allRequestsFinishedSpy( &network, SIGNAL(allRequestsFinished()) );

    // Test synchronous download (max 10 seconds)
    const QString url1 = "http://www.google.de";
    const QString url2 = "http://www.wikipedia.de";
    ScriptApi::NetworkRequest *request1 = network.createRequest( url1 );
    ScriptApi::NetworkRequest *request2 = network.createRequest( url2 );

    // Start two asynchronous downloads and wait for both to finish
    QEventLoop loop(this);
    connect( request1, SIGNAL(finished()), &loop, SLOT(quit()) );
    connect( request2, SIGNAL(finished()), &loop, SLOT(quit()) );
    network.head( request1 ); // Use head() to save network bandwidth
    network.head( request2 );

    QCOMPARE( requestStartedSpy.count(), 2 );
    QCOMPARE( requestFinishedSpy.count(), 0 );
    QCOMPARE( requestAbortedSpy.count(), 0 );
    QCOMPARE( allRequestsFinishedSpy.count(), 0 );
    loop.exec();

    if ( !request1->isFinished() || !request2->isFinished() ) {
        // One of the both requests is still running,
        // check signals and wait for the second request to finish
        QCOMPARE( requestStartedSpy.count(), 2 );
        QCOMPARE( requestFinishedSpy.count(), 1 );
        QCOMPARE( requestAbortedSpy.count(), 0 );
        QCOMPARE( allRequestsFinishedSpy.count(), 0 );

        QCOMPARE( network.hasRunningRequests(), true );
        QCOMPARE( network.runningRequestCount(), 1 );
        loop.exec();
    }

    QCOMPARE( requestStartedSpy.count(), 2 );
    QCOMPARE( requestFinishedSpy.count(), 2 );
    QCOMPARE( requestAbortedSpy.count(), 0 ); // Nothing was aborted
    QCOMPARE( allRequestsFinishedSpy.count(), 1 ); // Gets only emitted once for both requests

    QCOMPARE( network.hasRunningRequests(), false );
    QCOMPARE( network.runningRequestCount(), 0 );
    QVERIFY( network.runningRequests().isEmpty() );
    QCOMPARE( network.lastDownloadAborted(), false );
    QCOMPARE( network.lastUrl(), url2 );
}

QTEST_MAIN(ScriptApiTest)
#include "ScriptApiTest.moc"
