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
    QTest::addColumn<QString>("corrected");
    QTest::addColumn<QString>("stopName");
    QTest::addColumn<AnalyzerResult>("expectedLexicalState");
    QTest::addColumn<AnalyzerResult>("expectedSyntacticalState");
    QTest::addColumn<AnalyzerResult>("expectedContextualState");

    // Input strings that should be accepted
    QTest::newRow("Stop name only")
            << "Bremen Hbf" << "Bremen Hbf" << "Bremen Hbf"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name only (single word)")
            << "Bremen" << "Bremen" << "Bremen"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name only in quotation marks")
            << "\"Bremen Hbf\"" << "\"Bremen Hbf\"" << "Bremen Hbf"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name, keyword 'at'")
            << "To Bremen Hbf at 15:20" << "To Bremen Hbf at 15:20" << "Bremen Hbf"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name, keyword 'in'")
            << "To Bremen Hbf in 37 minutes" << "To Bremen Hbf in 37 minutes" << "Bremen Hbf"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name, keywords 'to', 'departing', 'tomorrow', 'at'")
            << "To \"Bremen Hbf\" departing tomorrow at 18:00"
            << "To \"Bremen Hbf\" departing tomorrow at 18:00" << "Bremen Hbf"
            << Accepted << Accepted << Accepted;

    // Input strings with errors
    QTest::newRow("Stop name, keyword 'at' and 'in'")
            << "To Bremen Hbf at 17:45 in 37 minutes" << "To Bremen Hbf at 17:45" << "Bremen Hbf"
            << Accepted << AcceptedWithErrors << Accepted;
    QTest::newRow("Keyword 'at' used two times")
            << "To Bremen Hbf at 17:45 at 19:45" << "To Bremen Hbf at 17:45" << "Bremen Hbf"
            << Accepted << AcceptedWithErrors << Accepted;
    QTest::newRow("Illegal characters")
            << QString::fromUtf8("To Bremen}§") << "To Bremen" << "Bremen"
            << AcceptedWithErrors << AcceptedWithErrors << Accepted;
    QTest::newRow("Missing closing quotation mark")
            << "To \"Bremen Hbf" << "To Bremen Hbf" << "Bremen Hbf"
            << AcceptedWithErrors << AcceptedWithErrors << Accepted;
    QTest::newRow("Illegal keyword order")
            << "To \"Bremen Hbf\" tomorrow at 18:00 arriving" << "To \"Bremen Hbf\" arriving"
             << "Bremen Hbf"
            << Accepted << AcceptedWithErrors << Accepted;
    QTest::newRow("Illegal text after stop name")
            << "To \"Bremen Hbf\" unknown_keyword" << "To \"Bremen Hbf\"" << "Bremen Hbf"
            << Accepted << AcceptedWithErrors << Accepted;
    QTest::newRow("Incomplete keyword")
            << "Bremen Hbf at 15:"<< "Bremen Hbf at 15" << "Bremen Hbf"
            << Accepted << AcceptedWithErrors << Accepted;

    // Input strings with errors that should be corrected
    QTest::newRow("Stop name, correctable keyword 'at'")
            << "To Bremen Hbf at" << "To Bremen Hbf" << "Bremen Hbf"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Stop name, correctable keyword 'at' 2")
            << "To Bremen Hbf at 18" << "To Bremen Hbf at 18" << "Bremen Hbf"
            << Accepted << Accepted << Accepted;

    // Incomplete input strings
    QTest::newRow("Empty input") << "" << "" << ""
            << Accepted << Rejected << Rejected;
    QTest::newRow("Incomplete 1") << "Fr" << "Fr" << "Fr"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Incomplete 2") << "Fr Teststop" << "From Teststop" << "Teststop"
            << Accepted << Accepted << Accepted;
    QTest::newRow("Incomplete 3") << "Fr \"Teststop" << "From Teststop" << "Teststop"
            << AcceptedWithErrors << AcceptedWithErrors << Accepted;
    QTest::newRow("Incomplete 4") << "Fr \"Teststop depa" << "From Teststop departing" << "Teststop"
            << AcceptedWithErrors << AcceptedWithErrors << Accepted;
}

void JourneySearchParserTest::journeySearchParserTest()
{
    QFETCH(QString, search);
    QFETCH(QString, corrected);
    QFETCH(QString, stopName);
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

    SyntacticalAnalyzer syntax( Syntax::journeySearchSyntaxItem(), m_keywords );
    MatchItem matchItem = syntax.analyze( lexems );
    qDebug() << "INPUT:" << matchItem.input();
    qDebug() << "CORRECTED:" << matchItem.text();

    QCOMPARE( static_cast<int>(syntax.result()), static_cast<int>(expectedSyntacticalState) );
    if ( syntax.result() == Rejected ) {
        return;
    }
    QCOMPARE( matchItem.text(), corrected );

    ContextualAnalyzer context;
    QLinkedList<MatchItem> correctedMatchItems = context.analyze( QLinkedList<MatchItem>() << matchItem );
//     QStringList syntaxString2; foreach ( const SyntaxItem &syntaxItem, correctedSyntaxItems ) {
//             syntaxString2 << syntaxItem.text(); }
//     qDebug() << "Context List:" << syntaxString2.join(", ") << context.result();
    Results results =
            JourneySearchAnalyzer::resultsFromMatchItem( correctedMatchItems.first(), m_keywords );
    qDebug() << "Output string" << results.outputString();
    QCOMPARE( static_cast<int>(context.result()), static_cast<int>(expectedContextualState) );
    QCOMPARE( results.outputString(), corrected );
    QCOMPARE( results.stopName(), stopName );
}

// typedef Syntax S;
void JourneySearchParserTest::abstractMatchTest()
{
//     TODO Macro "parser generator"
//     QString search( "fr \"Bremen Hbf\" taassdc at 33:45, 39.08." );
    QString search( QString::fromUtf8("To Bremen Hbf at pü 17:45 in 37 minutes p") );
//     QString search( "Bremen Hbf" );
//     QString search( "to xyz \"Bremen Hbf\" departing at 13:45, 09.08." );
//     QString search( "to \"Bremen Hbf\" departing tomorrow at 13:45" );
    LexicalAnalyzer lex;
    QLinkedList<Lexem> lexems = lex.analyze( search );
    if ( lex.isRejected() ) {
        kDebug() << "The lexical analyzer rejected the input";
        return;
    }

//     qDebug() << '\n' << journeySearchSyntaxItem->toString();
    SyntaxItemPointer syntaxItem = Syntax::journeySearchSyntaxItem();
    SyntacticalAnalyzer syntax( syntaxItem, m_keywords );
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

    MatchItem matchItem = syntax.analyze( lexems );
    qDebug() << "\nMATCHES:" << matchItem.toString() << syntax.result();
    qDebug() << "INPUT:" << matchItem.input();
    qDebug() << "CORRECTED:" << matchItem.text();

    Results results = JourneySearchAnalyzer::resultsFromMatchItem( matchItem, m_keywords );
    qDebug() << "CORRECTED2:" << results.stopName() << results.outputString();

    syntaxItem->findKeywordChild( KeywordTo )->setFlag( SyntaxItem::RemoveFromInput );
    qDebug() << "REMOVED TO KEYWORD:" << results.updateOutputString(syntaxItem, CorrectEverything, m_keywords);

    syntaxItem->removeChangeFlags();
    syntaxItem->findKeywordChild( KeywordTimeAt )->setFlag( SyntaxItem::RemoveFromInput );
    qDebug() << "REMOVED AT KEYWORD:" << results.updateOutputString(syntaxItem, CorrectEverything, m_keywords);

    syntaxItem->removeChangeFlags();
    syntaxItem->findKeywordChild( KeywordTomorrow )->setFlag( SyntaxItem::AddToInput );
    qDebug() << "ADDED TOMORROW KEYWORD:" << results.updateOutputString(syntaxItem, CorrectEverything, m_keywords);
}

void JourneySearchParserTest::benchmarkLexicalTest()
{
    QString search = "To \"Bremen Hbf\" departing tomorrow at 18:00";
    LexicalAnalyzer lex;
    QBENCHMARK {
        QLinkedList<Lexem> lexems = lex.analyze( search );
    }
}

void JourneySearchParserTest::benchmarkSyntacticalTest()
{
    QString search = "To \"Bremen Hbf\" departing tomorrow at 18:00";
    LexicalAnalyzer lex;
    SyntacticalAnalyzer syntax( Syntax::journeySearchSyntaxItem(), m_keywords );
    QLinkedList<Lexem> lexems = lex.analyze( search );
    QBENCHMARK {
        MatchItem matchItem = syntax.analyze( lexems );
    }
}

void JourneySearchParserTest::benchmarkContextualTest()
{
    QString search = "To \"Bremen Hbf\" departing tomorrow at 18:00";
    LexicalAnalyzer lex;
    SyntacticalAnalyzer syntax( Syntax::journeySearchSyntaxItem(), m_keywords );
    ContextualAnalyzer context;
    QLinkedList<Lexem> lexems = lex.analyze( search );
    MatchItem matchItem = syntax.analyze( lexems );
    QBENCHMARK {
        QLinkedList<MatchItem> correctedMatchItems = context.analyze( QLinkedList<MatchItem>() << matchItem );
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
    SyntacticalAnalyzer syntax( Syntax::journeySearchSyntaxItem(), m_keywords );
    ContextualAnalyzer context;
    QBENCHMARK {
        QLinkedList<Lexem> lexems = lex.analyze( search );
        MatchItem matchItem = syntax.analyze( lexems );
        QLinkedList<MatchItem> correctedSyntaxItems = context.analyze( QLinkedList<MatchItem>() << matchItem );
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
