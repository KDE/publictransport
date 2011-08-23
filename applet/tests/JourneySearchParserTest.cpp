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

#include "JourneySearchParserTest.h"

#include <QtTest/QTest>
#include <KDebug>
#include <KLineEdit>

#define MATCH_SEQUENCE_CONCATENATION_OPERATOR +

#include "journeysearchparser.h"
#include "matchitem.h"

using namespace Parser;
Q_DECLARE_METATYPE( AnalyzerResult );

void JourneySearchParserTest::init()
{
}

void JourneySearchParserTest::initTestCase()
{
    m_keywords = new JourneySearchKeywords();
}

void JourneySearchParserTest::cleanupTestCase()
{
    delete m_keywords;
}

void JourneySearchParserTest::cleanup()
{
}

void JourneySearchParserTest::journeySearchParserTest_data()
{
    QTest::addColumn<QString>("search");
    QTest::addColumn<AnalyzerResult>("expectedLexicalState");
    QTest::addColumn<AnalyzerResult>("expectedSyntacticalState");
    QTest::addColumn<AnalyzerResult>("expectedContextualState");

    // Input strings that should be accepted
    QTest::newRow("Stop name only")
            << "Bremen Hbf"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name only (single word)")
            << "Bremen"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name only in quotation marks")
            << "\"Bremen Hbf\""
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name, keyword 'at'")
            << "To Bremen Hbf at 15:20"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name, keyword 'in'")
            << "To Bremen Hbf in 37 minutes"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name, keywords 'to, 'departing', 'tomorrow', 'at'")
            << "To \"Bremen Hbf\" departing tomorrow at 18:00"
            << Accepted << Accepted << Accepted;

    // Input strings with errors
    QTest::newRow("Stop name, keyword 'at' and 'in'")
            << "To Bremen Hbf at 17:45 in 37 minutes"
            << Accepted << Accepted << AcceptedWithErrors;
    QTest::newRow("Keyword 'at' used two times")
            << "To Bremen Hbf at 17:45 at 19:45"
            << Accepted << Accepted << AcceptedWithErrors;
    QTest::newRow("Illegal characters")
            << "To Bremen}§"
            << Rejected << Rejected << Rejected;
    QTest::newRow("Missing closing quotation mark")
            << "To \"Bremen Hbf"
            << AcceptedWithErrors << AcceptedWithErrors << AcceptedWithErrors;
    QTest::newRow("Illegal keyword order")
            << "To \"Bremen Hbf\" tomorrow at 18:00 arriving"
            << Accepted << Accepted << AcceptedWithErrors;
    QTest::newRow("Illegal text after stop name")
            << "To \"Bremen Hbf\" unknown_keyword"
            << Accepted << AcceptedWithErrors << AcceptedWithErrors;
    QTest::newRow("Incomplete keyword")
            << "Bremen Hbf at 15:"
            << Accepted << Accepted << Accepted;

    // Input strings with errors that should be corrected
    QTest::newRow("Stop name, correctable keyword 'at'")
            << "To Bremen Hbf at"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name, correctable keyword 'at' 2")
            << "To Bremen Hbf at 18"
            << Accepted << Accepted << Accepted;
}

void JourneySearchParserTest::journeySearchParserTest()
{
    QFETCH(QString, search);
    QFETCH(AnalyzerResult, expectedLexicalState);
    QFETCH(AnalyzerResult, expectedSyntacticalState);
    QFETCH(AnalyzerResult, expectedContextualState);

    LexicalAnalyzer lex;
    QLinkedList<Lexem> lexems = lex.analyze( search );
//     QStringList lexemString; foreach ( const Lexem &lexem, lexems ) { lexemString << lexem.text(); }
//     qDebug() << "Lexem List:" << lexemString.join(", ") << lex.result();
    QCOMPARE( static_cast<int>(lex.result()), static_cast<int>(expectedLexicalState) );
    if ( lex.result() == Rejected ) {
        return;
    }

    SyntacticalAnalyzer syntax( m_keywords );
    QLinkedList<MatchItem> matchItems = syntax.analyze( lexems );
    QStringList matchString; foreach ( const MatchItem &matchItem, matchItems ) {
            matchString << matchItem.input(); }
    qDebug() << "Match List:" << matchString.join(", ") << syntax.result();
    QCOMPARE( static_cast<int>(syntax.result()), static_cast<int>(expectedSyntacticalState) );
    if ( syntax.result() == Rejected ) {
        return;
    }

    ContextualAnalyzer context;
    QLinkedList<MatchItem> correctedMatchItems = context.analyze( matchItems );
//     QStringList syntaxString2; foreach ( const SyntaxItem &syntaxItem, correctedSyntaxItems ) {
//             syntaxString2 << syntaxItem.text(); }
//     qDebug() << "Context List:" << syntaxString2.join(", ") << context.result();
    Results results =
            JourneySearchAnalyzer::resultsFromSyntaxItemList( correctedMatchItems, m_keywords );
    qDebug() << "Output string" <<  results.outputString();
    QCOMPARE( static_cast<int>(context.result()), static_cast<int>(expectedContextualState) );
}

typedef Syntax M;
void JourneySearchParserTest::abstractMatchTest()
{
//     TODO Macro "parser generator"
    QString search( "fr \"Bremen Hbf\" taassdc departing at 33:45, 39.08." );
//     QString search( "to xyz \"Bremen Hbf\" departing at 13:45, 09.08." );
//     QString search( "to \"Bremen Hbf\" departing tomorrow at 13:45" );
    LexicalAnalyzer lex;
    QLinkedList<Lexem> lexems = lex.analyze( search );
    if ( lex.isRejected() ) {
        kDebug() << "The lexical analyzer rejected the input";
        return;
    }

    SyntaxItemPointer journeySearchSyntaxItem = Syntax::journeySearchSyntaxItem();
//     qDebug() << '\n' << journeySearchSyntaxItem->toString();
    SyntacticalAnalyzer syntax( m_keywords );
//     QBENCHMARK {
//         bool result = syntax.matchItem( lexems, journeySearchSyntaxItem );
//     }
//     delete journeySearchSyntaxItem;
//     return;

//     QCOMPARE( static_cast<int>(lex.result()), static_cast<int>(Accepted) );
//     SyntaxItems syntaxItems; // TODO replace QList<MatchItemPointer> with MatchSequencePointer?
//     syntaxItems << M::keyword(KeywordTo)->kleenePlus();
//     syntaxItems << (M::character('"') + M::words()->outputTo(StopNameValue) + M::character('"'));
//     syntaxItems << M::keyword( KeywordTimeAt,
//             (M::number(0, 23) + M::character(':')->optional() + M::number(0, 59)->optional())
//             )->outputTo(DateAndTimeValue)->optional();
//     qDeleteAll( syntaxItems );

//     SyntaxItemPointer journeySearchSyntaxItem = Match::journeySearchSyntaxItem();
//     SyntacticalAnalyzer syntax( m_keywords );
    bool result = syntax.matchItem( lexems, journeySearchSyntaxItem );

    qDebug() << "\nSYNTAX:" << journeySearchSyntaxItem->toString();

//     bool result = syntax.matchItems( lexems, matchItems );
    QLinkedList<MatchItem> matchItems = syntax.output();
    QStringList matchStrings;
    foreach ( const MatchItem &matchItem, matchItems ) {
        matchStrings << matchItem.toString();
//         QString("'%1' (type: %2, val: %3)")
//                 .arg(matchItem.input()).arg(matchItem.type()).arg(matchItem.value().toString());
    }
    qDebug() << "\nMATCHES:" << matchStrings.join(", ") << result << syntax.result();

    qDebug() << "INPUT:" << matchItems.first().input();
    qDebug() << "CORRECTED:" << matchItems.first().text();

    delete journeySearchSyntaxItem;
}

void JourneySearchParserTest::benchmarkLexicalTest()
{
    QString search = "To \"Bremen Hbf\" departing tomorrow at 18:00";
    LexicalAnalyzer lex;
    SyntacticalAnalyzer syntax( m_keywords );
    ContextualAnalyzer context;
    QBENCHMARK {
        QLinkedList<Lexem> lexems = lex.analyze( search );
    }
}

void JourneySearchParserTest::benchmarkSyntacticalTest()
{
    QString search = "To \"Bremen Hbf\" departing tomorrow at 18:00";
    LexicalAnalyzer lex;
    SyntacticalAnalyzer syntax( m_keywords );
    ContextualAnalyzer context;
    QLinkedList<Lexem> lexems = lex.analyze( search );
    QBENCHMARK {
        QLinkedList<MatchItem> matchItems = syntax.analyze( lexems );
    }
}

void JourneySearchParserTest::benchmarkContextualTest()
{
    QString search = "To \"Bremen Hbf\" departing tomorrow at 18:00";
    LexicalAnalyzer lex;
    SyntacticalAnalyzer syntax( m_keywords );
    ContextualAnalyzer context;
    QLinkedList<Lexem> lexems = lex.analyze( search );
    QLinkedList<MatchItem> matchItems = syntax.analyze( lexems );
    QBENCHMARK {
        QLinkedList<MatchItem> correctedMatchItems = context.analyze( matchItems );
    }
}

// 29.07.2011, 00:33: 0.24 msecs per iteration (total: 67, iterations: 256)
//                      0.015 msecs for lexical analysis
//                    > 0.200 msecs for syntatical analysis
//                      0.025 msecs for contextual analysis
//                    565,500 instruction reads
//                    600,000 CPU ticks per iteration
// 29.07.2011, 21:53: 0.12 msecs per iteration (total: 115, iterations: 512)
//                    180,000 instruction reads
//                    210,000 CPU ticks per iteration
// 30.07.2011, 18:33: 0.052 msecs per iteration (total: 54, iterations: 1024)
//                      0.012 msecs for lexical analysis
//                      0.014 msecs for syntatical analysis
//                      0.027 msecs for contextual analysis
//                    108,000 instruction reads
//                    118,000 CPU ticks per iteration
void JourneySearchParserTest::benchmarkTest()
{
    QString search = "To \"Bremen Hbf\" departing tomorrow at 18:00";

    LexicalAnalyzer lex;
    SyntacticalAnalyzer syntax( m_keywords );
    ContextualAnalyzer context;
    QBENCHMARK {
        QLinkedList<Lexem> lexems = lex.analyze( search );
        QLinkedList<MatchItem> matchItems = syntax.analyze( lexems );
        QLinkedList<MatchItem> correctedSyntaxItems = context.analyze( matchItems );
    }
}

// void JourneySearchParserTest::benchmarkTestOld()
// {
//     KLineEdit *lineEdit = new KLineEdit( "To \"Bremen Hbf\" departing tomorrow at 18:00" );
//     QString stop;
//     QDateTime departure;
//     bool stopIsTarget, timeIsDeparture;
//     int posStart, len;
//     QBENCHMARK {
//         JourneySearchParser::parseJourneySearch( lineEdit, lineEdit->text(), &stop, &departure,
//                                                  &stopIsTarget, &timeIsDeparture, &posStart,
//                                                  &len, false );
//     }
// }

QTEST_MAIN(JourneySearchParserTest)
#include "JourneySearchParserTest.moc"
