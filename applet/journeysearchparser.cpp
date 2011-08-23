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

#include "journeysearchparser.h"

#include <QStringList>
#include <QTime>

#include <KGlobal>
#include <KLocale>
#include <KLocalizedString>
#include <KLineEdit>
#include <KDebug>
#include <QLinkedList>

namespace Parser {

const QString LexicalAnalyzer::ALLOWED_OTHER_CHARACTERS = ":,.´`'!?&()_\"'";

JourneySearchAnalyzer::JourneySearchAnalyzer( JourneySearchKeywords *keywords,
                                              AnalyzerCorrections corrections,
                                              int cursorPositionInInputString ) :
        m_keywords(keywords ? keywords : new JourneySearchKeywords()),
        m_ownKeywordsObject(!keywords),
        m_lexical(new LexicalAnalyzer(corrections, cursorPositionInInputString)),
        m_syntactical(new SyntacticalAnalyzer(m_keywords, corrections, cursorPositionInInputString)),
        m_contextual(new ContextualAnalyzer(corrections, cursorPositionInInputString))
{
}

JourneySearchAnalyzer::~JourneySearchAnalyzer()
{
    if ( m_ownKeywordsObject ) {
        delete m_keywords;
    }
    delete m_lexical;
    delete m_syntactical;
    delete m_contextual;
}

Results JourneySearchAnalyzer::resultsFromSyntaxItemList(
            const QLinkedList< MatchItem > &itemList, JourneySearchKeywords *keywords )
{
    bool ownKeywords = !keywords;
    if ( ownKeywords ) {
        keywords = new JourneySearchKeywords();
    }

    Results results;
    results.m_allItems = itemList;

    bool tomorrow = false;
    QStringList output, outputWithErrors;
    QString itemText;
    for ( QLinkedList<MatchItem>::ConstIterator it = results.m_allItems.constBegin();
          it != results.m_allItems.constEnd(); ++it )
    {
        switch ( it->type() ) {
        case MatchItem::Invalid:
            kDebug() << "Got an invalid SyntaxItem! The SyntaxItem::Invalid flag should only be "
                        "used as argument to functions.";
            continue;
        case MatchItem::Error:
            results.m_hasErrors = true;
            results.m_errorItems << &*it;
            itemText = it->input();
            break;
        case MatchItem::Sequence:
            kDebug() << "TODO"; // TODO
            break;
        case MatchItem::Option:
            kDebug() << "TODO"; // TODO
            break;
        case MatchItem::Keyword:
        case MatchItem::Words:
        case MatchItem::Number:
        case MatchItem::Character:
        case MatchItem::String:
//         case StopName:
//             results.m_stopName = it->text();
//             results.m_syntaxItems.insert( StopName, &*it );
//             itemText = QString("\"%1\"").arg(results.m_stopName);
//             break;
//         case KeywordTo:
//             results.m_stopIsTarget = true;
//             results.m_syntaxItems.insert( KeywordTo, &*it );
//             itemText = it->text();
//             break;
//         case KeywordFrom:
//             results.m_stopIsTarget = false;
//             results.m_syntaxItems.insert( KeywordFrom, &*it );
//             itemText = it->text();
//             break;
//         case KeywordTimeIn:
//             results.m_time = QDateTime::currentDateTime().addSecs( 60 * it->value().toInt() );
//             results.m_syntaxItems.insert( KeywordTimeIn, &*it );
//             itemText = QString("%1 %2").arg(it->text())
//                     .arg(keywords->relativeTimeString(it->value().toInt()));
//             break;
//         case KeywordTimeAt:
//             results.m_time = QDateTime( QDate::currentDate(), it->value().toTime() );
//             results.m_syntaxItems.insert( KeywordTimeAt, &*it );
//             itemText = QString("%1 %2").arg(it->text()).arg(results.m_time.toString("hh:mm"));
//             break;
//         case KeywordTomorrow:
//             tomorrow = true;
//             results.m_syntaxItems.insert( KeywordTomorrow, &*it );
//             itemText = it->text();
//             break;
//         case KeywordDeparture:
//             results.m_timeIsDeparture = true;
//             results.m_syntaxItems.insert( KeywordDeparture, &*it );
//             itemText = it->text();
//             break;
//         case KeywordArrival:
//             results.m_timeIsDeparture = false;
//             results.m_syntaxItems.insert( KeywordArrival, &*it );
//             itemText =  it->text();
//             break;
            break;
        }

        if ( it->type() != MatchItem::Error ) {
            output << itemText;
        }
        outputWithErrors << itemText;
    }
    if ( !results.m_time.isValid() ) {
        // No time given in input string
        results.m_time = QDateTime::currentDateTime();
    }
    if ( tomorrow ) {
        results.m_time = results.m_time.addDays( 1 );
    }
    results.m_outputString = output.join( " " );
    results.m_outputStringWithErrors = outputWithErrors.join( " " );
    if ( ownKeywords ) {
        delete keywords;
    }

    return results;
}

QString Results::updatedOutputString(
        const QHash< JourneySearchValueType, QVariant > &updateItemValues,
        const QList< MatchItem::Type > &removeItems,
        OutputStringFlags flags, JourneySearchKeywords *keywords ) const
{
    bool ownKeywords = !keywords;
    if ( ownKeywords ) {
        keywords = new JourneySearchKeywords();
    }

    // Insert dummy syntax items for updated item values without an associated syntax item
    // in m_allItems
        // TODO FIXME Get the position of the stop name out of the new output.. syntaxItemRoot or something
//     QLinkedList< SyntaxItem > itemList = m_allItems;
//     for ( QHash<JourneySearchValueType, QVariant>::ConstIterator itUpdate = updateItemValues.constBegin();
//           itUpdate != updateItemValues.constEnd(); ++itUpdate )
//     {
//         if ( !m_syntaxItems.contains(itUpdate.key()) ) {
//             // Current updated item not in m_allItems, insert a dummy syntax item
//             switch ( itUpdate.key() ) {
//             case StopName:
//                 // Insert after KeywordTo/KeywordFrom or prepend
//                 if ( itemList.isEmpty() ) {
//                     // No items in the list, just prepend a dummy stop name item
//                     itemList.prepend( SyntaxItem(StopName, LexemList()) );
//                     break;
//                 }
// 
//                 for ( QLinkedList<SyntaxItem>::Iterator it = itemList.begin();
//                       it != itemList.end(); ++it )
//                 {
//                     if ( it->type() != KeywordTo ||
//                          it->type() != KeywordFrom )
//                     {
//                         // First non-prefix item found, insert dummy stop name item before
//                         it = itemList.insert( it, SyntaxItem(StopName, LexemList()) );
//                         break;
//                     }
//                 }
//                 break;
//             case Sequence:
//                 break; // TODO
//             case Option:
//                 break; // TODO
//             case Other:
//                 break; // TODO
//             case TimeValue:
//             case TimeHourValue:
//             case TimeMinuteValue:
//             case DateValue:
//             case DateDayValue:
//             case DateMonthValue:
//             case DateYearValue:
//             case DateAndTimeValue:
//             case TimeRelativeMinutes:
//                 kDebug() << "TODO"; // TODO
//                 break;
//             case KeywordTo:
//             case KeywordFrom:
//                 itemList.prepend( SyntaxItem(itUpdate.key(), LexemList()) );
//                 break;
//             case KeywordTomorrow:
//             case KeywordDeparture:
//             case KeywordArrival:
//             case KeywordTimeIn:
//             case KeywordTimeAt:
//                 itemList.append( SyntaxItem(itUpdate.key(), LexemList()) );
//                 break;
//             case Error:
//                 kDebug() << "Won't insert/update error items";
//                 break;
//             case Invalid:
//                 kDebug() << "Got an invalid SyntaxItem! The SyntaxItem::Invalid flag should only be "
//                             "used as argument to functions.";
//                 break;
//             }
//         }
//     }

        // TODO FIXME Get the position of the stop name out of the new output.. syntaxItemRoot or something
//     QStringList output;
//     QString itemText;
//     for ( QLinkedList<SyntaxItem>::ConstIterator it = itemList.constBegin();
//           it != itemList.constEnd(); ++it )
//     {
//         switch ( it->type() ) {
//         case Invalid:
//             kDebug() << "Got an invalid SyntaxItem! The SyntaxItem::Invalid flag should only be "
//                         "used as argument to functions.";
//             break;
//         case Error:
//             if ( !flags.testFlag(ErrornousOutputString) ) {
//                 continue;
//             }
//             itemText = it->text();
//             break;
//         case Sequence:
//             break; // TODO
//         case Option:
//             break; // TODO
//         case Other:
//             break; // TODO
//         case TimeValue:
//         case TimeHourValue:
//         case TimeMinuteValue:
//         case DateValue:
//         case DateDayValue:
//         case DateMonthValue:
//         case DateYearValue:
//         case DateAndTimeValue:
//         case TimeRelativeMinutes:
//             kDebug() << "TODO"; // TODO
//             break;
//         case StopName:
//             if ( !removeItems.contains(it->type()) ) {
//                 if ( updateItemValues.contains(it->type()) ) {
//                     itemText = QString("\"%1\"").arg( updateItemValues[it->type()].toString() );
//                 } else {
//                     itemText = QString("\"%1\"").arg( it->text() );
//                 }
//             }
//             break;
//         case KeywordTo:
//         case KeywordFrom:
//         case KeywordTomorrow:
//         case KeywordDeparture:
//         case KeywordArrival:
//             if ( !removeItems.contains(it->type()) ) {
//                 if ( updateItemValues.contains(it->type()) ) {
//                     // This replaces the keywords with other keyword translations or other strings
//                     itemText = updateItemValues[it->type()].toString();
//                 } else {
//                     itemText = it->text();
//                 }
//             }
//             break;
//         case KeywordTimeIn:
//             if ( !removeItems.contains(it->type()) ) {
//                 if ( updateItemValues.contains(it->type()) ) {
//                     // This replaces the keyword values with new ones (the keyword "in" itself remains unchanged)
//                     itemText = QString("%1 %2").arg(it->text())
//                             .arg( keywords->relativeTimeString(updateItemValues[it->type()].toInt()) );
//                 } else {
//                     itemText = QString("%1 %2").arg(it->text())
//                             .arg( keywords->relativeTimeString(it->value().toInt()) );
//                 }
//             }
//             break;
//         case KeywordTimeAt:
//             if ( !removeItems.contains(it->type()) ) {
//                 if ( updateItemValues.contains(it->type()) ) {
//                     // This replaces the keyword values with new ones (the keyword "at" itself remains unchanged)
//                     itemText = QString("%1 %2").arg(it->text())
//                             .arg( updateItemValues[it->type()].toTime().toString("hh:mm") );
//                 } else {
//                     itemText = QString("%1 %2").arg(it->text())
//                             .arg( it->value().toTime().toString("hh:mm") );
//                 }
//             }
//             break;
//         }
// 
//         output << itemText;
//     }
        // TODO FIXME Get the position of the stop name out of the new output.. syntaxItemRoot or something
    if ( ownKeywords ) {
        delete keywords;
    }

    return QString(); // output.join( " " );
}

const Results JourneySearchAnalyzer::analyze(
        const QString& input, AnalyzerCorrections corrections,
        ErrorSeverity minRejectSeverity, ErrorSeverity minAcceptWithErrorsSeverity )
{
    m_lexical->setCorrections( corrections );
    m_syntactical->setCorrections( corrections );
    m_contextual->setCorrections( corrections );
    m_lexical->setErrorHandling( minRejectSeverity, minAcceptWithErrorsSeverity );
    m_syntactical->setErrorHandling( minRejectSeverity, minAcceptWithErrorsSeverity );
    m_contextual->setErrorHandling( minRejectSeverity, minAcceptWithErrorsSeverity );

//     TODO calculate cursor offset at end by summing up lengths of corrected items before the old cursor position
    QLinkedList<Lexem> lexems = m_lexical->analyze( input );
    m_syntactical->setCursorValues( m_lexical->cursorOffset(), m_lexical->selectionLength() );
    QLinkedList<MatchItem> syntaxItems = m_syntactical->analyze( lexems );
    m_contextual->setCursorValues( m_syntactical->cursorOffset(), m_syntactical->selectionLength() );
    m_results = resultsFromSyntaxItemList( m_contextual->analyze(syntaxItems) );
    m_results.m_cursorOffset = m_contextual->cursorOffset();
    m_results.m_selectionLength = m_contextual->selectionLength();
    m_results.m_inputString = input;
    m_results.m_result = qMin( m_contextual->result(),
                               qMin(m_syntactical->result(), m_lexical->result()) );
    return m_results;
//     return resultsFromSyntaxItemList(
//             m_contextual->analyze(m_syntactical->analyze(m_lexical->analyze(input))),
//             m_keywords );
}

void Results::init()
{
    m_stopName.clear();
    m_time = QDateTime();
    m_stopIsTarget = true;
    m_timeIsDeparture = true;
    m_hasErrors = false;
    m_syntaxItems.clear();
    m_cursorOffset = 0;
    m_result = Rejected;
}

QString Results::outputString( OutputStringFlags flags ) const
{
    if ( flags.testFlag(ErrornousOutputString) ) {
        return m_outputStringWithErrors;
    } else {
        return m_outputString;
    }
}

template <typename Container, typename Item>
Analyzer<Container, Item>::Analyzer( AnalyzerCorrections corrections,
                                     int cursorPositionInInputString, int cursorOffset )
{
    m_state = NotStarted;
    m_result = Accepted;
    m_corrections = corrections;
    m_minRejectSeverity = ErrorFatal;
    m_minAcceptWithErrorsSeverity = ErrorSevere;
    m_cursorPositionInInputString = cursorPositionInInputString;
    m_cursorOffset = cursorOffset;
    m_selectionLength = 0;
}

template <typename Container, typename Item>
void Analyzer<Container, Item>::setErrorHandling( ErrorSeverity minRejectSeverity,
                                                  ErrorSeverity minAcceptWithErrorsSeverity )
{
    m_minRejectSeverity = minRejectSeverity;
    m_minAcceptWithErrorsSeverity = minAcceptWithErrorsSeverity;
}

template <typename Container, typename Item>
void Analyzer<Container, Item>::setError( ErrorSeverity severity, const QString& errornousText,
                                          const QString& errorMessage, int position,
                                          MatchItem *parent, int index )
{
    Q_UNUSED( errornousText );
    Q_UNUSED( parent );
    if ( severity >= m_minRejectSeverity ) {
        m_result = Rejected;
        qDebug() << "Inacceptable error:" << errorMessage << position << index;
    } else if ( severity >= m_minAcceptWithErrorsSeverity ) {
        qDebug() << "Acceptable error:" << errorMessage << position << index;
        if ( m_result > AcceptedWithErrors ) {
            m_result = AcceptedWithErrors;
        }
    } else {
        qDebug() << "Error without consequences:" << errorMessage << position << index;
    }
}

template <typename Container, typename Item>
void Analyzer<Container, Item>::moveToBeginning()
{
    m_inputIterator = m_input.constBegin();
    m_inputIteratorLookahead = m_inputIterator;
    updateLookahead();
}

template <typename Container, typename Item>
void Analyzer<Container, Item>::moveToEnd()
{
    m_inputIterator = m_input.constEnd() - 1;
    m_inputIteratorLookahead = m_inputIterator;
    updateLookahead();
}

template <typename Container, typename Item>
AnalyzerRL<Container, Item>::AnalyzerRL( AnalyzerCorrections corrections,
                                         int cursorPositionInInputString, int cursorOffset )
            : Analyzer<Container, Item>(corrections, cursorPositionInInputString, cursorOffset),
            m_direction(LeftToRight)
{

}

template <typename Container, typename Item>
void AnalyzerRL<Container, Item>::moveToBeginning()
{
    if ( m_direction == LeftToRight ) {
        Analyzer<Container, Item>::moveToBeginning();
    } else {
        Analyzer<Container, Item>::moveToEnd();
    }
}

template <typename Container, typename Item>
void AnalyzerRL<Container, Item>::moveToEnd()
{
    if ( m_direction == LeftToRight ) {
        Analyzer<Container, Item>::moveToEnd();
    } else {
        Analyzer<Container, Item>::moveToBeginning();
    }
}

void LexicalAnalyzer::moveToBeginning()
{
    Analyzer< QString, QChar >::moveToBeginning();
    m_pos = 0;
}

void SyntacticalAnalyzer::setError2( ErrorSeverity severity, const QString& errornousText,
                                     const QString& errorMessage, int position,
                                     const LexemList &lexems, MatchItem *parent,
                                     int index )
{
    Analyzer::setError( severity, errornousText, errorMessage, position, parent, index );
    addOutputItem( MatchItem(MatchItem::Error, lexems.isEmpty()
                              ? LexemList() << Lexem(Parser::Lexem::Error, QString(), position) : lexems,
                              ErrorMessageValue, errorMessage), parent );
}

void SyntacticalAnalyzer::setError2( ErrorSeverity severity, const QString& errornousText,
                                     AnalyzerCorrection errorCorrection, int position,
                                     const LexemList &lexems, MatchItem *parent,
                                     int index )
{
    Analyzer::setError( severity, errornousText, QString("Error correction: %1").arg(errorCorrection),
                        position, parent, index );
    addOutputItem( MatchItem(MatchItem::Error, lexems.isEmpty()
                              ? LexemList() << Lexem(Parser::Lexem::Error, QString(), position) : lexems,
                              ErrorCorrectionValue, errorCorrection), parent );
}

void ContextualAnalyzer::setError( ErrorSeverity severity, const QString& errornousText,
                                   const QString& errorMessage, int position,
                                   MatchItem *parent, int index )
{
    Analyzer::setError( severity, errornousText, errorMessage, position, parent, index );
    MatchItem errorItem( MatchItem::Error, LexemList(), ErrorMessageValue, errorMessage );
    if ( !parent ) {
        m_input << errorItem;
    } else {
        parent->addChild( errorItem );
    }
}

LexicalAnalyzer::LexicalAnalyzer( AnalyzerCorrections corrections,
                                  int cursorPositionInInputString, int cursorOffset )
        : Analyzer(corrections, cursorPositionInInputString, cursorOffset)
{
    m_pos = -1;
    m_wordStartPos = -1;
    m_firstWordSymbol = Invalid;
}

bool LexicalAnalyzer::isSpaceFollowing()
{
    if ( isAtEnd() ) {
        return false; // Nothing is following
    }
    return *m_inputIteratorLookahead == ' ';
}

void LexicalAnalyzer::endCurrentWord( bool followedBySpace )
{
    if ( !m_currentWord.isEmpty() ) {
        if ( m_firstWordSymbol == Digit ) {
            m_output << Lexem( Lexem::Number, m_currentWord, m_wordStartPos, followedBySpace );
        } else if ( m_firstWordSymbol == Letter ||  m_firstWordSymbol == OtherSymbol ) {
            m_output << Lexem( Lexem::String, m_currentWord, m_wordStartPos, followedBySpace );
        } else {
            m_result = AcceptedWithErrors;
            kDebug() << "Internal error with word" << m_currentWord << m_firstWordSymbol;
        }
        m_currentWord.clear();
        m_firstWordSymbol = Invalid;
    }
}

LexicalAnalyzer::Symbol LexicalAnalyzer::symbolOf( const QChar& c ) const
{
    if ( c.isDigit() ) {
        return Digit;
    } else if ( c.isLetter() ) {
        return Letter;
    } else if ( c == ' ' ) {//.isSpace() ) {
        return Space;
//     } else if ( ALLOWED_SPECIAL_CHARACTERS.contains(c) ) {
//         return Character;
    } else if ( ALLOWED_OTHER_CHARACTERS.contains(c) ) {
        return OtherSymbol;
    } else {
        return Invalid;
    }
}

QLinkedList<Lexem> LexicalAnalyzer::analyze( const QString& input )
{
    m_input = input;
    moveToBeginning();
    m_state = Running;
    m_result = Accepted;
    m_output.clear();
    m_currentWord.clear();
    m_firstWordSymbol = Invalid;
    bool inQuotationMarks = false;
    while ( !isAtEnd() ) {
        Symbol symbol = symbolOf( *m_inputIterator );
        switch ( symbol ) {
//         case QuotationMark:
//         case Colon:
//         case OtherSymbol:
//             endCurrentWord();
//             m_output << Lexem( Lexem::Character, "\"", m_pos, isSpaceFollowing() );
//             inQuotationMarks = !inQuotationMarks;
//             break;
//         case Colon:
//             endCurrentWord();
//             m_output << Lexem( Lexem::Character, ":", m_pos, isSpaceFollowing() );
//             break;
        case Space:
            if ( !isNextAtImaginaryIteratorEnd() && symbolOf(*(m_inputIteratorLookahead)) == Space ) {
                // Don't allow two consecutive space characters
                // Skip this one, ie. read the next one
                readItem();
            }

            endCurrentWord( true );
            if ( m_pos == m_input.length() - 1 || m_pos == m_cursorPositionInInputString - 1 ) {
                m_output << Lexem( Lexem::Space, " ", m_pos, isSpaceFollowing() );
            }
            break;
        case OtherSymbol:
            if ( *m_inputIterator == '"' ) {
                inQuotationMarks = !inQuotationMarks;
            }
            endCurrentWord();
            m_output << Lexem( Lexem::Character, *m_inputIterator, m_pos, isSpaceFollowing() );
            break;
        case Letter:
        case Digit:
            if ( m_firstWordSymbol == Invalid ) {
                // At the beginning of a new word
                m_firstWordSymbol = symbol;
                m_wordStartPos = m_pos;
                m_currentWord.append( *m_inputIterator );
            } else if ( m_firstWordSymbol == symbol ) {
                // In the middle of a word
                m_currentWord.append( *m_inputIterator );
            } else { //if ( symbol == OtherSymbol || symbol == Letter || symbol == Digit ) {
                m_firstWordSymbol = OtherSymbol; // Change to word of (other|alpha|digit)*
                m_currentWord.append( *m_inputIterator );
            }
            break;
        case Invalid:
            endCurrentWord();
            m_output << Lexem( Lexem::Error, *m_inputIterator, m_pos, isSpaceFollowing() ); // Not allowed character
            m_result = Rejected;
            m_state = Finished;
            return m_output;
        }
        readItem();
    }

    // End of last word
    endCurrentWord();

    if ( inQuotationMarks ) {
        setError( ErrorSevere, QString(), "Quotation marks not closed", m_input.length() - 1 );
//         TODO Error handling: Add a closing Quotation mark?
    }
    m_state = Finished;
    return m_output;
}

SyntacticalAnalyzer::SyntacticalAnalyzer( JourneySearchKeywords *keywords,
                                          AnalyzerCorrections corrections,
                                          int cursorPositionInInputString, int cursorOffset )
        : Analyzer(corrections, cursorPositionInInputString, cursorOffset), m_keywords(keywords)
{
    if ( !m_keywords ) {
        m_ownKeywordsObject = true;
        m_keywords = new JourneySearchKeywords();
    } else {
        m_ownKeywordsObject = false;
    }
}

SyntacticalAnalyzer::~SyntacticalAnalyzer()
{
    if ( m_ownKeywordsObject ) {
        delete m_keywords;
    }
}

QLinkedList< MatchItem > SyntacticalAnalyzer::analyze( const QLinkedList<Lexem> &input )
{
    QStringList lexemString; foreach ( const Lexem &lexem, input ) {
            lexemString << QString("%1 (pos: %2, type: %3, space?: %4)").arg(lexem.input())
                .arg(lexem.position()).arg(lexem.type()).arg(lexem.isFollowedBySpace() ? "YES" : "no"); }
    qDebug() << "Lexem List:" << lexemString.join(", ");

    m_output.clear();
    m_input = input;
    m_state = Running;
    m_result = Accepted;
    moveToBeginning();

    readSpaceItems();
    parseJourneySearch();

    // Only for debugging:
    QStringList syntaxString; foreach ( const MatchItem &syntaxItem, m_output ) {
            syntaxString << QString("%1 (pos: %2, type: %3, value: %4)").arg(syntaxItem.input())
                .arg(syntaxItem.position()).arg(syntaxItem.type()).arg(syntaxItem.value().toString()); }
    qDebug() << "Syntax List:" << syntaxString.join(", ") << m_result;

    m_state = Finished;
    return m_output;
}

void SyntacticalAnalyzer::addOutputItem( const MatchItem& syntaxItem, MatchItem *parentItem )
{
    if ( parentItem ) {
        #ifdef DEBUG_PARSING
            qDebug() << "Output for parentItem" << syntaxItem.input();
        #endif
        // Sort the new syntaxItem into the child list of the parent item, sorted by position
        parentItem->addChild( syntaxItem );
    } else {
        #ifdef DEBUG_PARSING
            qDebug() << "Output for m_output" << syntaxItem.input();
        #endif
        // Sort the new syntaxItem into the output list, sorted by position
        if ( m_output.isEmpty() || m_output.last().position() < syntaxItem.position() ) {
            // Append the new item
            m_output.insert( m_output.end(), syntaxItem );
        } else {
            QLinkedList<MatchItem>::iterator it = m_output.begin();
            while ( it->position() < syntaxItem.position() ) {
                ++it;
            }
            m_output.insert( it, syntaxItem );
        }
    }
}

bool SyntacticalAnalyzer::parseJourneySearch()
{
    if ( isAtImaginaryIteratorEnd() ) {
        setError2( ErrorSevere, QString(), "No input", m_inputIterator->position() );
        return false;
    }

    matchPrefix();
    matchSuffix();
    bool matched = matchStopName();

    return matched;
}

bool SyntacticalAnalyzer::matchPrefix()
{
    // Match keywords at the beginning of the input
    // Also match, if only the beginning of a keyword is found (for interactive typing)
    // Otherwise the first typed character would get matched as stop name and get quotation
    // marks around it, making it hard to type the journey search string.
    moveToBeginning();
    readSpaceItems();
    if ( !isAtImaginaryIteratorEnd() && m_inputIterator->type() == Lexem::String ) {
//         const QString text = m_inputIterator->text();

        bool matched = false;
        // NOTE This doesn't work any longer, because the keywordItem doesn't get added to
        // the output using addOutputItem (for keywords with values matched with MatchItems).
        MatchItem keywordItem;
        matched = matched || matchKeywordInList( KeywordTo, &keywordItem );
        matched = matched || matchKeywordInList( KeywordFrom, &keywordItem );
        m_stopNameBegin = m_inputIterator;
        return matched;
    } else {
        m_stopNameBegin = m_inputIterator;
        return false;
    }
}

bool SyntacticalAnalyzer::matchNumber( int *number, int min, int max, MatchItem *parent,
                                       bool *corrected, int *removedDigits )
{
    // Match number
    QString numberString( m_inputIterator->input() );
    if ( removedDigits ) {
        *removedDigits = 0; // Initialize
    }
    bool ok;
    if ( m_corrections.testFlag(CorrectNumberRanges) ) {
        if ( numberString.length() > 2 ) {
            if ( removedDigits ) {
                *removedDigits = numberString.length() - 2;
            }
            // TODO This only works with strings not longer than three characters
            if ( m_cursorPositionInInputString == m_inputIterator->position() + 1 ) {
                // Cursor is here (|): "X|XX:XX", overwrite second digit
                numberString.remove( 1, 1 );
            }
            numberString = numberString.left( 2 );
        }

        int readNumber = numberString.toInt( &ok );
        if ( !ok ) {
            return false; // Number invalid (shouldn't happen, since hoursString only contains digits)
        }

        // Put the number into the given range [min, max]
        *number = qBound( min, readNumber, max );
        if ( corrected ) {
            *corrected = readNumber != *number;
        }
    } else {
        *number = numberString.toInt( &ok );
        if ( !ok || *number < min || *number > max ) {
            return false; // Number out of range or invalid
        }
        if ( corrected ) {
            *corrected = false; // Correction disabled
        }
    }

    readItemAndSkipSpaces( parent );
    return true;
}

bool SyntacticalAnalyzer::matchTimeAt()
{
    // Match "at 00:00" from back (RightToLeft) => starting with a number
    if ( m_inputIterator->type() != Lexem::Number ) {
        // Wrong ending.. But if the "at" keyword gets read here (with wrong following items)
        // it may be corrected by adding the time values (use current time)
        if ( m_corrections.testFlag(CorrectEverything) /* TODO */ && m_inputIterator->type() == Lexem::String &&
             m_keywords->timeKeywordsAt().contains(m_inputIterator->input(), Qt::CaseInsensitive) )
        {
            // Add output item for the read "at" keyword with corrected time value
            addOutputItem( MatchItem(MatchItem::Keyword, //TimeAt,
                    LexemList() << *m_inputIterator, NoValue, static_cast<int>(KeywordTimeAt),
                    // QDateTime::currentDateTime() Value gets now stored in the value sequence item for the keyword item,
                    MatchItem::CorrectedMatchItem) );

            // Move one character to the beginning of the inserted time
            // and select the first digit => "at [X]X:XX"
            ++m_cursorOffset;
            m_selectionLength = 1;

            readItemAndSkipSpaces();
            return true;
        } else {
            return false;
        }
    }
    if ( isAtEnd() ) {
        return false; // Only a number was read
    }

    QLinkedList<Lexem>::ConstIterator oldIterator = m_inputIterator;

    // Match number (minutes)
    int minutes;
    if ( !matchNumber(&minutes, 0, 59) ) {
        return false; // returned false => m_inputIterator unchanged/restored
    }

    if ( m_inputIterator->type() != Lexem::Character || !m_inputIterator->textIsCharacter(':') ) {
        // Wrong ending.. But if the "at" keyword gets read here (with only a number following)
        // it may be corrected by using the number as hours value and adding the minutes
        if ( m_corrections.testFlag(CorrectEverything) /* TODO */ && m_inputIterator->type() == Lexem::String &&
             m_keywords->timeKeywordsAt().contains(m_inputIterator->input(), Qt::CaseInsensitive) )
        {
            // Add output item for the "at" keyword with corrected time value (minutes => hours)
            addOutputItem( MatchItem(MatchItem::Keyword, //TimeAt,
                    LexemList() << *m_inputIterator, NoValue, static_cast<int>(KeywordTimeAt),
//                     QDateTime(QDate::currentDate(), QTime(minutes, 0)), Value gets now stored in the value sequence item for the keyword item,
                    MatchItem::CorrectedMatchItem) );
            readItemAndSkipSpaces();
            return true;
        } else {
            m_inputIterator = oldIterator;
            m_inputIteratorLookahead = m_inputIterator;
            updateLookahead();
            return false; // TimeAt-rule not matched
        }
    }
    if ( isAtEnd() ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator;
        updateLookahead();
        return false; // Only a number and a colon were read
    }
    int colonPosition = m_inputIterator->position();
    readItemAndSkipSpaces();

    if ( isAtEnd() || m_inputIterator->type() != Lexem::Number ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator;
        updateLookahead();
        return false; // The TimeAt-rule can't be reduced here
    }

    // Match number (hours)
//     QString hoursString = m_inputIterator->text();
    int hours;
    int deletedDigits;
    if ( !matchNumber(&hours, 0, 23, NULL, NULL, &deletedDigits) ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator;
        updateLookahead();
        return false;
    }

    if ( m_inputIterator->type() != Lexem::String ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator;
        updateLookahead();
        return false; // Keyword not found
    }
    MatchItem match;
    if ( !matchKeywordInList(KeywordTimeAt, &match) ) {
//     if ( !m_keywords->timeKeywordsAt().contains(m_inputIterator->text(), Qt::CaseInsensitive) ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator;
        updateLookahead();
        return false; // Keyword not found
    }

    // Set value of the matched at keyword
    match.setValue( QDateTime(QDate::currentDate(), QTime(hours, minutes)) );
    colonPosition -= deletedDigits; // Add offset from corrections (more than two digits for the hour)
//     addOutputItem( SyntaxItem(SyntaxItem::KeywordTimeAt,
//                               m_inputIterator->text(), m_inputIterator->position(),
//                               QDateTime(QDate::currentDate(), QTime(hours, minutes))) );
    if ( m_cursorPositionInInputString == colonPosition ) {
        // Cursor before the colon, move cursor over the colon while typing
        m_cursorOffset = colonPosition + 1 - m_cursorPositionInInputString;
    }
    if ( m_cursorPositionInInputString <= colonPosition + 2 ) {
        // Cursor not after the second minute digit, select the digit in front of the cursor
        m_selectionLength = 1;
    }

    return true;
}

bool SyntacticalAnalyzer::matchTimeIn()
{
    QLinkedList<Lexem>::ConstIterator oldIterator = m_inputIterator;

    // "in X minutes" from back => starting with "minutes"
    if ( !matchMinutesString() ) {
        return false; // The TimeIn-rule can't be reduced here, wrong ending lexem
    }

    readItemAndSkipSpaces();
    if ( m_inputIterator->type() != Lexem::Number ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator;
        updateLookahead();
        return false; // TimeIn-rule not matched
    }
    // Parse number, maximally one minute less than one day (1339). Should use "tomorrow" for
    // "in one day".
    int minutes;
    if ( !matchNumber(&minutes, 1, 1339) ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator;
        updateLookahead();
        return false; // Number out of range
    }

    // Read keyword "in"
    readItemAndSkipSpaces();
    if ( m_inputIterator->type() != Lexem::String ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator;
        updateLookahead();
        return false; // Keyword not found
    }
    if ( !m_keywords->timeKeywordsIn().contains(m_inputIterator->input(), Qt::CaseInsensitive) ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator;
        updateLookahead();
        return false; // Keyword not found
    }

    addOutputItem( MatchItem(MatchItem::Keyword, LexemList() << *m_inputIterator, RelativeTimeValue, minutes) );
    readItemAndSkipSpaces();

// int minutes;
// matchValue( QList<const MatchItem>() , &minutes );

    return true;
}

int SyntacticalAnalyzer::matchKleeneStar( SyntaxItems *items, SyntaxItems::ConstIterator* item,
        MatchItem* matchedItem, MatchItem *parent )
{
    // The current item should have a Kleene flag set (KleenePlus or KleeneStar)
    Q_ASSERT( (**item)->flags().testFlag(Parser::SyntaxItem::KleenePlus) );

    int matches = 0;
    MatchItem subMatchedItem;
    if ( (**item)->flags().testFlag(SyntaxItem::MatchGreedy) ) {
        // Greedy matching, ie. stop when this item doesn't match any longer
        while ( !isAtImaginaryIteratorEnd()
                && matchItemKleeneUnaware(items, item, matchedItem, parent) )
        {
            ++matches; // This item matched (again), proceed with greedy Kleene star
        }
    } else {
        // Non-greedy matching, ie. stop when the next item matches
        while ( !isAtImaginaryIteratorEnd() ) {
            // First check if the next item matches (non-greedy), then check this item (again)
            ++*item;
            if ( *item != items->constEnd() && matchItem(items, item, &subMatchedItem, parent) ) {
                break; // Next item matches, don't proceed with Kleene star
            }
            --*item; // Go back to the Kleene star item

            if ( !matchItemKleeneUnaware(items, item, matchedItem, parent) ) {
                break; // Didn't match in Kleene star, proceed matching the next item
            } else {
                ++matches; // This item matched (again), proceed with non-greedy Kleene star
            }
        }
    }
    return matches; // Kleene star always matches, return match count here
}

bool SyntacticalAnalyzer::matchItem( SyntaxItems *items, SyntaxItems::ConstIterator* item,
        MatchItem* matchedItem, MatchItem *parent )
{
    if ( *item == items->constEnd() ) {
        return true; // Not an item, return true ("matched")
    }
    #ifdef DEBUG_PARSING
        kDebug() << "matchItem" << (**item)->type();
    #endif

    int matched = 0;
    MatchItem subMatchedItem;
    SyntaxItem::Flags flags = (**item)->flags();

    // A KleeneStar is an optional KleenePlus. 
    // That means, that the first test for KleenePlus also matches KleeneStar.
    if ( flags.testFlag(SyntaxItem::KleenePlus) ) {
        if ( flags.testFlag(SyntaxItem::KleeneStar) ) {
            // Match any number of times
            matched += matchKleeneStar( items, item, matchedItem, parent );
        } else {
            // Match at least once
            if ( matchItemKleeneUnaware(items, item, matchedItem, parent) ) {
                // Count the first matched item
                ++matched;

                // Then match any additional number of times
                matched += matchKleeneStar( items, item, matchedItem, parent );
            }
        }
    } else if ( matchItemKleeneUnaware(items, item, matchedItem, parent) ) {
        // No Kleene star or plus, only one match (but it may be optional and have matched nothing)
        ++matched;
    }

    // Matches with at least one match. The KleeneStar always matches
    return matched > 0 || flags.testFlag(SyntaxItem::KleeneStar);
}

bool SyntacticalAnalyzer::matchSequence( const SyntaxSequencePointer &sequence,
                                         MatchItem *matchedItem, MatchItem *parent )
{
    #ifdef DEBUG_PARSING
        qDebug() << "matchSequence" << sequence->items()->count();
    #endif

    MatchItem sequenceItem( MatchItem::Sequence );

    // Don't add the sequence item to the output if there is only one child item
    const bool addSequenceItem = sequence->items()->count() != 1;
    MatchItem *childItemParent = addSequenceItem ? &sequenceItem : parent;

    MatchItem matchedSequenceItem;
    for ( SyntaxItems::ConstIterator it = sequence->items()->constBegin();
          it != sequence->items()->constEnd(); ++it )
    {
        bool matched = false;
//         TODO Remove here, skipping is done in matchItemKleeneUnaware
//         if ( m_corrections.testFlag(SkipUnexpectedTokens) ) {
//             // If a sequence item doesn't match, skip it and try to match the next one
//             QList<ErrorInformation> errors;
//             while ( !(matched = matchItem(sequence->items(), &it, &matchedSequenceItem, childItemParent)) ) {
//                 #ifdef DEBUG_PARSING
//                     qDebug() << "Skipping" << m_inputIterator->position() << m_inputIterator->input();
//                 #endif
//                 setError2( ErrorSevere, m_inputIterator->input(), SkipUnexpectedTokens,
//                            m_inputIterator->position(), LexemList() << *m_inputIterator, parent );
//                 readItemAndSkipSpaces( parent );
//             }
//         } else {
            matched = matchItem( sequence->items(), &it, &matchedSequenceItem, childItemParent );
//         }

        if ( matched ) {
            if ( isAtImaginaryIteratorEnd() ) {
                ++it; // Move after the last matched sequence item
                // No more input after the last matched item of the sequence
                for ( ; it != sequence->items()->constEnd(); ++it ) {
                    if ( !(*it)->isOptional() ) {
                        // There is a non-optional MatchItem in the list (but no input)
                        #ifdef DEBUG_PARSING
                            qDebug() << " X - More MatchItems in sequence, no more input, non-optional item"
                                     << (*it)->toString();
                        #endif
                        return false;
                    }
                }
                // All other MatchItems are optional
                #ifdef DEBUG_PARSING
                    qDebug() << "OK - More MatchItems in sequence, no more input, but all are optional";
                #endif
                break;
            }
        } else {
            return false;
        }
    }

    if ( addSequenceItem ) {
        if ( sequence->valueType() != NoValue ) {
            // Construct the value for the sequence item from it's children
            sequenceItem.setValue( readValueFromChildren(sequence->valueType(), &sequenceItem) );
        }

        addOutputItem( sequenceItem, parent );
        #ifdef FILL_MATCHES_IN_MATCHITEM
            sequence->addMatch( sequenceItem );
        #endif
        if ( matchedItem ) {
            *matchedItem = sequenceItem;
        }
    } else {
        // Don't add a sequence item to the output, there is only one item in the sequence,
        // which has already been added to parent (instead of the sequence item)
        #ifdef FILL_MATCHES_IN_MATCHITEM
            sequence->addMatch( matchedSequenceItem.position(), matchedSequenceItem.input(), QVariant() );
        #endif
        if ( matchedItem ) {
            *matchedItem = matchedSequenceItem;
        }
    }
    return true;
}

bool SyntacticalAnalyzer::matchOption( SyntaxOptionItem* option, MatchItem* matchedItem,
                                       MatchItem* parent )
{
    #ifdef DEBUG_PARSING
        qDebug() << "matchOption" << option->options()->count();
    #endif

    MatchItem optionItem( MatchItem::Option );
    MatchItem matchedOptionItem;
    for ( SyntaxItems::ConstIterator it = option->options()->constBegin();
          it != option->options()->constEnd(); ++it )
    {
        if ( matchItem(option->options(), &it, &matchedOptionItem, &optionItem) ) {
            if ( (*it)->isOptional() ) {
                kDebug() << "An option item shouldn't have optional child items.\n"
                        "Because an optional item always matches, items after an optional child\n"
                        "are never used. Make the option item optional instead.";
            }
            addOutputItem( optionItem, parent );
            #ifdef FILL_MATCHES_IN_MATCHITEM
                option->addMatch( optionItem.position(), optionItem.input(), optionItem.value() );
            #endif
            if ( matchedItem ) {
                *matchedItem = optionItem;
            }
            return true;
        }
    }

    if ( option->isOptional() ) {
        return true;
    } else {
        #ifdef DEBUG_PARSING
            qDebug() << "   X - no item of a non-optional option list matched";
        #endif
        return false;
    }
}

bool SyntacticalAnalyzer::matchKeyword( SyntaxKeywordItem* keyword, MatchItem* matchedItem,
                                        MatchItem* parent )
{
    // Match a single keyword without any associated value   **TODO > UPDATE DOCU < TODO**
    MatchItem match;
    bool matched = matchKeywordInList( keyword->keyword(), &match, parent );

    if ( matched ) {
        #ifdef DEBUG_PARSING
            qDebug() << "  OK - keyword matched:" << keyword->keyword();
        #endif
        *matchedItem = match;
    } else {
        // NOTE Match is uninitialized, if matched is false
        #ifdef DEBUG_PARSING
            if ( keyword->isOptional() ) {
                qDebug() << "  OK - keyword not matched, but optional:" << keyword->keyword();
            } else {
                qDebug() << "   X - non-optional keyword not matched:" << keyword->keyword();
                // TODO Error handling: Add SyntaxItem::Error item with default value text (eg. minimum for MatchItemNumber)
            }
        #endif
        return keyword->isOptional();
    }

    if ( keyword->valueSequence() ) {
        // Match value sequence for the keyword
        MatchItem valueSequence;
        matched = matchSequence( keyword->valueSequence(), &valueSequence, &match );
    }

    if ( matched ) {
        addOutputItem( match, parent );
        #ifdef FILL_MATCHES_IN_MATCHITEM
            keyword->addMatch( match.position(), match.input(), match.value() );
        #endif
    }

    return true;
}

// if this returns false, the contents of @p matchedItem are undefined
bool SyntacticalAnalyzer::matchItemKleeneUnaware( SyntaxItems *items,
        SyntaxItems::ConstIterator *item, MatchItem *matchedItem, MatchItem *parent,
        AnalyzerCorrections enabledCorrections )
{
    Q_UNUSED( items );
    #ifdef DEBUG_PARSING
        qDebug() << "matchItemKleeneUnaware" << (**item)->type() << (**item);
    #endif

    QLinkedList<Lexem>::ConstIterator oldIterator = m_inputIterator;
    SyntaxItem::Type type = (**item)->type();
    switch ( type ) {
    case SyntaxItem::MatchSequence: {
        SyntaxSequenceItem *sequenceItem = qobject_cast<SyntaxSequenceItem*>( (*item)->data() );
        if ( matchSequence(sequenceItem, matchedItem, parent) || sequenceItem->isOptional() ) {
            return true;
        }
        break;
    } case SyntaxItem::MatchOption: {
        SyntaxOptionItem *optionItem = qobject_cast<SyntaxOptionItem*>( (*item)->data() );
        if ( matchOption(optionItem, matchedItem, parent) || optionItem->isOptional() ) {
            return true;
        }
        break;
    } case SyntaxItem::MatchKeyword: {
        SyntaxKeywordItem *keywordItem = qobject_cast<SyntaxKeywordItem*>( (*item)->data() );
        if ( matchKeyword(keywordItem, matchedItem, parent) || keywordItem->isOptional() ) {
            return true;
        }
        break;
    } case SyntaxItem::MatchNumber: {
        Lexem numberLexem = *m_inputIterator;
        int matchedNumber;
        SyntaxNumberItem *numberItem = qobject_cast<SyntaxNumberItem*>( (*item)->data() );
        if ( matchNumber(&matchedNumber, numberItem->min(), numberItem->max(), parent) // TODO removedDigits
             || (**item)->isOptional() )
        {
            #ifdef FILL_MATCHES_IN_MATCHITEM
                numberItem->addMatch( numberLexem.position(), numberLexem.input(), matchedNumber );
            #endif
            *matchedItem = MatchItem( MatchItem::Number, LexemList() << numberLexem,
                                      (**item)->valueType(), matchedNumber );
//             if ( (**item)->hasValue() ) { TODO
                addOutputItem( *matchedItem, parent );
//             }
            #ifdef DEBUG_PARSING
                qDebug() << "  OK - Matched number" << numberLexem.input();
            #endif
            return true;
        }
        break;
    } case SyntaxItem::MatchString: {
        Lexem stringLexem = *m_inputIterator;
        QString matchedString;
        SyntaxStringItem *stringItem = qobject_cast<SyntaxStringItem*>( (*item)->data() );
        if ( matchString(stringItem->strings(), &matchedString, parent) || (**item)->isOptional() ) {
            #ifdef FILL_MATCHES_IN_MATCHITEM
                stringItem->addMatch( stringLexem.position(), matchedString, matchedString );
            #endif
            *matchedItem = MatchItem( MatchItem::String, LexemList() << stringLexem,
                                       (**item)->valueType(), matchedString );
//             if ( (**item)->isUsingOutput() ) {
                addOutputItem( *matchedItem, parent );
//             }
            #ifdef DEBUG_PARSING
                qDebug() << "  OK - Matched string" << matchedString;
            #endif
            return true;
        }
        break;
    } case SyntaxItem::MatchWords: {
        LexemList wordLexems;
        QStringList matchedWords;
        QString matchedText;
        SyntaxWordsItem *wordsItem = qobject_cast<SyntaxWordsItem*>( (*item)->data() );
        if ( matchWords(wordsItem->wordCount(), &matchedWords, &matchedText, &wordLexems,
                        wordsItem->wordTypes(), parent) || (**item)->isOptional() )
        {
            #ifdef FILL_MATCHES_IN_MATCHITEM
                wordsItem->addMatch( matchedItem->position(), matchedText, matchedWords );
            #endif
            *matchedItem = MatchItem( MatchItem::Words, wordLexems, (**item)->valueType(), matchedWords );
//             if ( (**item)->isUsingOutput() ) {
                addOutputItem( *matchedItem, parent );
//             }
            #ifdef DEBUG_PARSING
                qDebug() << "  OK - Matched words" << matchedWords;
            #endif
            return true;
        }
        break;
    }
//     case MatchItem::MatchQuotationMark:
//     case MatchItem::MatchColon:
    case SyntaxItem::MatchCharacter: {
        Lexem symbolLexem = *m_inputIterator;
        QString text;
        SyntaxCharacterItem *characterItem = qobject_cast<SyntaxCharacterItem*>( (*item)->data() );
        if ( matchCharacter(characterItem->character(), &text, parent) || (**item)->isOptional() ) {
            #ifdef FILL_MATCHES_IN_MATCHITEM
                characterItem->addMatch( symbolLexem.position(), text, text);
            #endif
            *matchedItem = MatchItem( MatchItem::Character, LexemList() << symbolLexem,
                                       (**item)->valueType(), text );
//             if ( (**item)->isUsingOutput() ) {
                addOutputItem( *matchedItem, parent );
//             }
            #ifdef DEBUG_PARSING
                qDebug() << "  OK - Matched character" << text;
            #endif
            return true;
        }
        break;
    } case SyntaxItem::MatchNothing:
        #ifdef DEBUG_PARSING
            qDebug() << "  OK - Matched nothing";
        #endif
        return true; // Nothing to do
    } // End of switch

    // No match, try enabled corrections
    // First try to insert a missing required token, if it is similar to the input
    if ( enabledCorrections.testFlag(InsertMissingRequiredTokens)
         && m_corrections.testFlag(InsertMissingRequiredTokens) && !(**item)->isOptional() )
    {
        // TODO This should maybe be done in a new function matchSimilarItemKleeneUnaware
//         switch ( type ) {
//         case SyntaxItem::MatchString:
//             if ( m_inputIterator->type() == Lexem::String &&
//                  stringsSimilar(m_inputIterator->input(),
//                                 qobject_cast<SyntaxStringItem*>(**item)->strings().first()) )
//             {
//                 addOutputItem( MatchItem(MatchItem::String, LexemList() << *m_inputIterator,
//                                          (**item)->valueType()), parent );
//                 return true;
//             }
//             break;
// //         case SyntaxItem::MatchKeyword:
// //             break;
//         default:
//             break;
//         }
    }

    // Skip the not matching item and try to match the next one
    if ( enabledCorrections.testFlag(SkipUnexpectedTokens)
         && m_corrections.testFlag(SkipUnexpectedTokens) )
    {
        const int MAX_SKIP_UNEXPECTED = 3;
        QList<ErrorInformation> skipErrorItems;
        do {
            #ifdef DEBUG_PARSING
                qDebug() << "Skipping" << m_inputIterator->position() << m_inputIterator->input();
            #endif
//             setError2( ErrorSevere, m_inputIterator->input(), SkipUnexpectedTokens,
//                        m_inputIterator->position(), LexemList() << *m_inputIterator, parent );
            skipErrorItems << ErrorInformation( SkipUnexpectedTokens, m_inputIterator->position(),
                                                *m_inputIterator );
            readItemAndSkipSpaces( parent );
            if ( matchItemKleeneUnaware(items, item, matchedItem, parent, CorrectNothing) ) {
                #ifdef DEBUG_PARSING
                    qDebug() << "Match after" << skipErrorItems.count() << "skipped lexems";
                #endif
                // Add the error items
                foreach ( const ErrorInformation &error, skipErrorItems ) {
                    setError2( ErrorSevere, error.lexem.input(), error.correction,
                               error.position, LexemList() << error.lexem, parent );
                }
                return true;
            }
        } while ( skipErrorItems.count() < MAX_SKIP_UNEXPECTED && !isAtImaginaryIteratorEnd() );

        #ifdef DEBUG_PARSING
            qDebug() << "Skipping didn't help matching the input";
        #endif
        // m_inputIterator is not where it was before this if, it gets restored below
    }

    #ifdef DEBUG_PARSING
        qDebug() << "   X - Failed" << type << '\n' << (**item)->toString() << "\nat input:"
                 << (isAtImaginaryIteratorEnd() ? "END" : m_inputIterator->input());
    #endif
    m_inputIterator = oldIterator;
    m_inputIteratorLookahead = m_inputIterator;
    updateLookahead();
    return false;
};

bool SyntacticalAnalyzer::matchItem( const QLinkedList<Lexem> &input, SyntaxItemPointer item )
{
    // Initialize input
    m_input = input;
    moveToBeginning();

    // Create a list with only item in it and use it for matchNextItem
    SyntaxItems items = SyntaxItems() << item;
    SyntaxItems::ConstIterator iterator = items.constBegin();

    MatchItem firstMatchedItem;
    return matchItem( &items, &iterator, &firstMatchedItem );
//     return matchNextItem( &items, &iterator, &firstMatchedItem );
}

QTime SyntacticalAnalyzer::timeValue( const QHash< JourneySearchValueType, QVariant >& values ) const
{
    if ( !values.contains(TimeHourValue) ) {
        // No hour value found, discard minute value if present
        return QTime::currentTime();
    } else if ( !values.contains(TimeMinuteValue) ) {
        // Only an hour value was found
        return QTime( values[TimeHourValue].toInt(), 0 );
    } else {
        return QTime( values[TimeHourValue].toInt(), values[TimeMinuteValue].toInt() );
    }
}

QDate SyntacticalAnalyzer::dateValue( const QHash< JourneySearchValueType, QVariant >& values ) const
{
    if ( !values.contains(DateDayValue) || !values.contains(DateMonthValue) ) {
        // No day or month values found, discard year value if present
        return QDate::currentDate();
    } else if ( !values.contains(DateYearValue) ) {
        // Only day and month values were found
        return QDate( QDate::currentDate().year(),
                      values[DateMonthValue].toInt(), values[DateDayValue].toInt() );
    } else {
        return QDate( values[DateYearValue].toInt(),
                      values[DateMonthValue].toInt(), values[DateDayValue].toInt() );
    }
}

QVariant SyntacticalAnalyzer::readValueFromChildren( JourneySearchValueType valueType,
                                                     MatchItem *syntaxItem ) const
{
    MatchItems allChildren = syntaxItem->allChildren();
    QHash< JourneySearchValueType, QVariant > values;
    foreach ( const MatchItem &childItem, allChildren ) {
        values.insert( childItem.valueType(), childItem.value() );
    }
    switch ( valueType ) {
        case TimeValue:
            return timeValue( values );
        case DateValue:
            return dateValue( values );
        case DateAndTimeValue:
            return QDateTime( dateValue(values), timeValue(values) );

        case RelativeTimeValue:
        default:
            if ( !values.contains(valueType) ) {
                kDebug() << "Didn't find a value for a syntax item in it's child items:"
                         << valueType << syntaxItem->toString();
            }
            return values[valueType];
    }

    return QVariant(); // Couldn't find matching child items for the given value type
}

void SyntacticalAnalyzer::readSpaceItems( MatchItem *parent )
{
    #ifdef DEBUG_PARSING
        qDebug() << "readSpaceItems" << (isAtImaginaryIteratorEnd());
        if ( !isAtImaginaryIteratorEnd() ) {
            qDebug() << "Not at input end";
        }
    #endif
    while ( !isAtImaginaryIteratorEnd() && m_inputIterator->type() == Lexem::Space ) {
        addOutputItem( MatchItem(MatchItem::Error, LexemList() << *m_inputIterator, ErrorMessageValue,
                                  "Double space character"), parent );
        readItem();
    }
}

bool SyntacticalAnalyzer::matchCharacter( char character, QString *text, MatchItem *parentItem )
{
    #ifdef DEBUG_PARSING
        qDebug() << "matchCharacter" << character;
    #endif

    if ( m_inputIterator->type() == Lexem::Character && m_inputIterator->textIsCharacter(character) ) {
        if ( text ) {
            *text = m_inputIterator->input();
        }
        readItemAndSkipSpaces( parentItem );
        return true;
    } else {
        return false;
    }
}

bool SyntacticalAnalyzer::matchWords( int wordCount, QStringList *matchedWords, QString *matchedText,
                                      LexemList *wordsLexems, const QList<Lexem::Type> &wordTypes,
                                      MatchItem *parentItem )
{
//     TODO do the addOutputItem work here?
    #ifdef DEBUG_PARSING
        qDebug() << "matchWordsNextItem" << wordCount << wordTypes;
    #endif

    QLinkedList<Lexem>::ConstIterator oldIterator = m_inputIterator;
    QStringList words;
    QString text;
    bool result = true;
    for ( ; wordCount != 0 && !isAtImaginaryIteratorEnd(); --wordCount ) {
        if ( wordTypes.contains(m_inputIterator->type()) ) {
            *wordsLexems << *m_inputIterator;
            words << m_inputIterator->input();
            text += m_inputIterator->input();
            if ( m_inputIterator->isFollowedBySpace() ) {
                text += ' ';
            }
            readItemAndSkipSpaces( parentItem );
        } else if ( wordCount < 0 ) {
            result = true;
            break;
        } else {
            m_inputIterator = oldIterator;
            m_inputIteratorLookahead = m_inputIterator;
            updateLookahead();
            result = false;
            break;
        }
    }

    if ( matchedWords ) {
        *matchedWords << words;
    }
    if ( matchedText ) {
        *matchedText = text;
    }
    return result;
}

bool SyntacticalAnalyzer::matchString( const QStringList& stringList, QString *matchedString,
                                       MatchItem *parentItem )
{
    if ( m_inputIterator->type() == Lexem::String ) {
        foreach ( const QString &string, stringList ) {
            if ( m_inputIterator->input().compare(string, Qt::CaseInsensitive) == 0 ) {
                if ( matchedString ) {
                    *matchedString = m_inputIterator->input();
                }
                readItemAndSkipSpaces( parentItem );
                return true;
            }
        }
        return false; // No string in stringList matched
    } else {
        return false;
    }
}

bool SyntacticalAnalyzer::matchMinutesString()
{
//     TODO: i18n
    return matchString( QStringList() << "mins" << "minutes" );
}

bool SyntacticalAnalyzer::matchKeywordInList( KeywordType type, MatchItem *match,
                                              MatchItem *parent )
{
    const QStringList& keywordList = m_keywords->keywords( type );
    Q_ASSERT( !keywordList.isEmpty() );

//     QStringList matchedTexts;
#ifdef ALLOW_MULTIWORD_KEYWORDS
    LexemList lexems;
    foreach ( const QString &keyword, keywordList ) {
        // Keywords themselves may consist of multiple words (ie. contain whitespaces)
        const QStringList words = keyword.split( QLatin1Char(' '), QString::SkipEmptyParts );
        const int wordCount = words.count();
        int word = 0;
        int index;
        do {
            index = m_direction == RightToLeft ? wordCount - 1 - word : word;
            if ( m_inputIterator->type() != Lexem::String
                || words[index].compare(m_inputIterator->input(), Qt::CaseInsensitive) != 0 )
            {
                break; // Didn't match keyword
            }

//             matchedTexts << m_inputIterator->input();
            lexems << *m_inputIterator;
            ++word;
            if ( word >= wordCount ||
                 (m_direction == RightToLeft ? index <= 0 : index >= m_input.count() - 1) )
            {
                break; // End of word list reached
            }
            readItemAndSkipSpaces( parent );
        } while ( !isAtImaginaryIteratorEnd() );

        if ( word == wordCount ) {
            // All words of the keyword matched
//             SyntaxItem item( type, lexems, keyword );
//             addOutputItem( item, parent );
            *match = SyntaxItem( SyntaxItem::Keyword, lexems, NoValue, static_cast<int>(type) );
            readItemAndSkipSpaces( parent );
            return true;
        } else {
            // Test if the beginning of a keyword matches.
            // If a cursor position is given, only complete the keyword, if the cursor is at the 
            // end of the so far typed keyword.
            // This needs to also match with one single character for matchPrefix, because
            // otherwise eg. a single typed "t" would get matched as stop name and get quotation
            // marks around it, making it hard to type "to"..
            if ( !isAtImaginaryIteratorEnd() && m_corrections.testFlag(CompleteKeywords) &&
                 (m_cursorPositionInInputString == -1 ||
                  m_cursorPositionInInputString == m_inputIterator->position() + m_inputIterator->input().length()) &&
                 (word > 0 || words[index].startsWith(m_inputIterator->input(), Qt::CaseInsensitive)) )
            {
                // Add output item for the partially read keyword
//                 matchedTexts << m_inputIterator->input();
                lexems << *m_inputIterator;
//                 SyntaxItem item = SyntaxItem( type, lexems, keyword );
//                 addOutputItem( item, parent );
                *match = SyntaxItem( SyntaxItem::Keyword, lexems, NoValue, static_cast<int>(type) );
                m_selectionLength = keyword.length() - m_inputIterator->input().length() -
                        QStringList(words.mid(0, index)).join(" ").length();
                readItemAndSkipSpaces( parent );
                return true;
            }

            if ( word > 0 ) {
                // Not all words of the keyword matched (but at least one)
                // Restore position
                m_inputIterator -= m_direction == RightToLeft ? -word : word;
                m_inputIteratorLookahead = m_inputIterator;
                updateLookahead();
            }
        }
    }
#else
// TODO Two runs, first check for complete keywords and then try to find beginnings
    foreach ( const QString &keyword, keywordList ) {
        if ( m_inputIterator->type() != Lexem::String
             || keyword.compare(m_inputIterator->input(), Qt::CaseInsensitive) != 0 )
        {
            // Didn't match keyword
            // Test if the beginning of a keyword matches.
            // If a cursor position is given, only complete the keyword, if the cursor is at the
            // end of the so far typed keyword.
            // This needs to also match with one single character for matchPrefix, because
            // otherwise eg. a single typed "t" would get matched as stop name and get quotation
            // marks around it, making it hard to type "to"..
            if ( !isAtImaginaryIteratorEnd() && m_corrections.testFlag(CompleteKeywords) &&
                 (m_cursorPositionInInputString == -1 ||
                  m_cursorPositionInInputString == m_inputIterator->position() + m_inputIterator->input().length()) &&
                 keyword.startsWith(m_inputIterator->input(), Qt::CaseInsensitive) )
            {
                // Add output item for the partially read keyword
//                 SyntaxItem item = SyntaxItem( type, lexems, keyword );
//                 addOutputItem( item, parent );
//                 *match = MatchItem( MatchItem::Keyword, LexemList() << *m_inputIterator,
//                                     NoValue, QVariantList() << static_cast<int>(type) << keyword,
//                                     MatchItem::CorrectedMatchItem );
                *match = MatchItem( LexemList() << *m_inputIterator, type, keyword,
                                    MatchItem::CorrectedMatchItem );
                m_selectionLength = keyword.length() - m_inputIterator->input().length();
                readItemAndSkipSpaces( parent );
                return true;
            }
            continue;
//             return false;
        }

        // The keyword was matched
//         matchedTexts << m_inputIterator->input();
//         *match = MatchItem( MatchItem::Keyword, LexemList() << *m_inputIterator,
//                              NoValue, static_cast<int>(type) );
        *match = MatchItem( LexemList() << *m_inputIterator, type, keyword );
        readItemAndSkipSpaces( parent );
        return true;
    }
#endif

    return false; // No keyword matched
}

bool SyntacticalAnalyzer::matchSuffix()
{
//     TODO Parse dates, other time formats?
    if ( m_stopNameBegin == m_input.constEnd() ) {
        // Complete input read as prefix
        m_stopNameEnd = m_stopNameBegin;
        return false;
    }

//     m_direction = RightToLeft; TODO
    m_inputIterator = m_input.constEnd();
    m_inputIteratorLookahead = m_inputIterator;
    updateLookahead();
    readSpaceItems();
    m_stopNameEnd = m_inputIterator;
    int matches = 0;
    bool matched = false;
    while ( !isAtEnd() && m_inputIteratorLookahead != m_stopNameBegin ) {
        if ( !matched ) {
            readItemAndSkipSpaces();
        }
        matched = false;

        if ( m_inputIterator->type() == Lexem::Character && m_inputIterator->textIsCharacter('"') ) {
            // Stop if a quotation mark is read (end of the stop name)
            break;
        } else if ( m_inputIterator->type() == Lexem::Space ) {
            // Add spaces to the output as error items
            setError2( ErrorMinor, m_inputIterator->input(),
                      "Space character at the end of the input", m_inputIterator->position() );
            continue;
        }
        matched = matchTimeAt();
        matched = matched || matchTimeIn();

        // NOTE This doesn't work any longer, because the keywordItem doesn't get added to
        // the output using addOutputItem (for keywords with values matched with MatchItems).
        MatchItem keywordItem;
        matched = matched || matchKeywordInList( KeywordTomorrow, &keywordItem );
        matched = matched || matchKeywordInList( KeywordDeparture, &keywordItem );
        matched = matched || matchKeywordInList( KeywordArrival, &keywordItem );
        if ( matched ) {
            ++matches;
        }
    }

//     m_direction = LeftToRight; TODO
    m_inputIteratorLookahead = m_inputIterator;
    updateLookahead();
//     m_inputIteratorLookahead = m_inputIterator + 1; // Update lookahead iterator to left-to-right
    if ( matches > 0 ) {
        // Move to previous item (right-to-left mode)
        // eg. "[StopName] tomorrow" => "StopName [tomorrow]"
        m_stopNameEnd = m_inputIteratorLookahead;
        #ifdef DEBUG_PARSING
            qDebug() << (m_stopNameEnd == m_input.constEnd() ? "stop name end: END"
                            : ("stop name end: " + m_stopNameEnd->input()));
        #endif
        return true;
    } else {
        kDebug() << "Suffix didn't match";
        return false;
    }
}

bool SyntacticalAnalyzer::matchStopName()
{
    m_inputIterator = m_stopNameBegin;
    m_inputIteratorLookahead = m_inputIterator;
    updateLookahead();
    readSpaceItems();
    if ( m_inputIterator == m_stopNameEnd || isAtImaginaryIteratorEnd() ) {
        setError2( ErrorFatal, QString(), "No stop name", m_inputIterator->position() );
        return false;
    }

    QStringList stopNameWords;
    LexemList lexems;
    int firstWordPos = -1;

    if ( m_inputIterator->type() == Lexem::Character && m_inputIterator->textIsCharacter('"') ) {
        // The while loop ends when a quotation mark is found, or if there is no more input to read
        firstWordPos = m_inputIterator->position() + 1;
        readItem(); // m_inputIterator isn't at m_stopNameEnd, tested above
        while ( m_inputIterator != m_stopNameEnd ) {
            if ( m_inputIterator->type() == Lexem::Character && m_inputIterator->textIsCharacter('"') ) {
                break; // Closing quotation mark
            } else {
                lexems << *m_inputIterator;
                stopNameWords << m_inputIterator->input();
            }
            readItem();
        }

        if ( m_inputIterator == m_stopNameEnd || m_inputIterator->type() != Lexem::Character ||
             m_inputIterator->textIsCharacter('"') )
        {
            setError2( ErrorSevere, QString(), "No closing quotation mark",
                       m_inputIterator->position() );
//             TODO Error handling: Add a closing quotation mark
        } else {
//             ++m_inputIterator;
            readItem();
        }
    } else {
        firstWordPos = m_inputIterator->position();  // m_inputIterator isn't at m_stopNameEnd, tested above
        do {
            lexems << *m_inputIterator;
            stopNameWords << m_inputIterator->input();
            readItem();
        } while ( m_inputIterator != m_stopNameEnd && !isAtImaginaryIteratorEnd() );

        if ( m_cursorPositionInInputString >= firstWordPos /*&& m_correctionLevel > CorrectNothing TODO*/ ) {
            ++m_cursorOffset; // Add an offset for the quotation marks that get inserted
        }
    }

    if ( stopNameWords.isEmpty() ) {
        setError2( ErrorFatal, QString(), "No stop name", firstWordPos + 1 );
        return false;
    } else {
        addOutputItem( MatchItem(MatchItem::Words, lexems, StopNameValue, stopNameWords) );

        if ( m_inputIterator != m_stopNameEnd && !isAtImaginaryIteratorEnd() ) {
            // Remaining lexems in m_input
            // Put their texts into an error item (eg. a Lexem::Space)
            int position = m_inputIterator->position();
            QString errornousText;
            for ( ; m_inputIterator != m_stopNameEnd; ++m_inputIterator ) {
                if ( m_inputIterator->isFollowedBySpace() ) {
                    errornousText.append( m_inputIterator->input() + ' ' );
                } else {
                    errornousText.append( m_inputIterator->input() );
                }
            }
            m_inputIteratorLookahead = m_inputIterator;
            updateLookahead();

            // If the unparsed text only contains spaces, the error is a minor one.
            // Otherwise it's severe, eg. unrecognized keywords/values.
            setError2( errornousText.trimmed().isEmpty() ? ErrorMinor : ErrorSevere,
                       errornousText, "Unknown elements remain unparsed", position );
        }
    }

    return true;
}

ContextualAnalyzer::ContextualAnalyzer( AnalyzerCorrections corrections,
                                        int cursorPositionInInputString, int cursorOffset )
        : Analyzer(corrections, cursorPositionInInputString, cursorOffset)
{
}

QLinkedList< MatchItem > ContextualAnalyzer::analyze( const QLinkedList< MatchItem >& input )
{
    m_input = input;
    moveToBeginning();
    m_state = Running;
    m_result = Accepted;
    QList< QSet<KeywordType> > disjointKeywords;
    QHash< KeywordType, QSet<KeywordType> > notAllowedAfter;

    // Type isn't allowed after one of the set of other types
    const QSet<KeywordType> timeTypes = QSet<KeywordType>()
            << KeywordTimeAt << KeywordTimeIn << KeywordTomorrow;
    notAllowedAfter.insert( KeywordDeparture, timeTypes );
    notAllowedAfter.insert( KeywordArrival, timeTypes );

    // Sets of keywords that should be used at the same time
    disjointKeywords << (QSet<KeywordType>() << KeywordArrival << KeywordDeparture);
    disjointKeywords << (QSet<KeywordType>() << KeywordTimeAt << KeywordTimeIn);
    disjointKeywords << (QSet<KeywordType>() << KeywordTo << KeywordFrom);

    // TODO apply "tomorrow" to time somewhere?

//     TODO UPDATE TO NEW SYNTAXITEM STRUCTURE
//     QHash< SyntaxItem::Type, QLinkedList<SyntaxItem>::ConstIterator > usedKeywords;
//     while ( !isAtEnd() ) {
//         readItem();
// 
//         if ( usedKeywords.contains(m_inputIterator->type()) ) {
//             // Keyword found twice, replace with an error item
//             setError( ErrorSevere, QString(),
//                       QString("Keyword '%1' used twice").arg(m_inputIterator->text()),
//                       m_inputIterator->position() );
// //             TODO remove the double keyword (=> replace, not just add an error item), set text of the second keyword to the error item
//         } else if ( notAllowedAfter.contains(m_inputIterator->type()) ) {
//             QSet<SyntaxItem::Type> intersection =
//                     notAllowedAfter[ m_inputIterator->type() ] & usedKeywords.keys().toSet();
//             if ( !intersection.isEmpty() ) {
//                 // Keyword found after another keyword, but is illegal there, replace with an error item
//                 QStringList intersectingKeywords;
//                 foreach ( SyntaxItem::Type type, intersection ) {
//                     intersectingKeywords << SyntaxItem::typeName(type);
//                 }
//                 setError( ErrorSevere, QString(),
//                           QString("Keyword '%1' not allowed after keyword(s) '%2'")
//                           .arg(m_inputIterator->text()).arg(intersectingKeywords.join("', '")),
//                           m_inputIterator->position() );
// //             TODO move the keyword to the correct location
//             }
//         }
// 
//         // Insert used keyword into usedKeywords, but not if it's an error item
//         // (that would result in an infinite loop here, because error items get added here)
//         if ( m_inputIterator->type() != Error ) {
//             usedKeywords.insert( m_inputIterator->type(), m_inputIterator );
//         }
//     }
// 
//     // Check for keywords that shouldn't be used together
//     // Error handling: Remove second keyword
//     foreach ( const QSet<SyntaxItem::Type> &disjoint, disjointKeywords ) {
//         QList<SyntaxItem::Type> intersection = (disjoint & usedKeywords.keys().toSet()).values();
//         while ( intersection.count() > 1 ) {
//             // More than one keyword of the current set of disjoint keywords is used
//             QLinkedList<SyntaxItem>::const_iterator keywordItemToRemove =
//                     usedKeywords[ intersection.takeFirst() ];
//             QStringList intersectingKeywords;
//             foreach ( SyntaxItem::Type type, intersection ) {
//                 if ( type != keywordItemToRemove->type() ) {
//                     intersectingKeywords << SyntaxItem::typeName(type);
//                 }
//             }
//             setError( ErrorSevere, QString(),
//                       QString("Keyword '%1' can't be used together with '%2'")
//                       .arg(keywordItemToRemove->text()).arg(intersectingKeywords.join("', '")),
//                       keywordItemToRemove->position() );
// 
// //             TODO remove the second keyword (=> replace, not just add an error item), set text of the second keyword to the error item
//         }
//     }

    m_state = Finished;
    return m_input;
}

JourneySearchKeywords::JourneySearchKeywords() :
        m_toKeywords(i18nc("@info/plain A comma separated list of keywords for the "
            "journey search, indicating that a journey TO the given stop should be searched. This "
            "keyword needs to be placed at the beginning of the field.", "to")
            .split(',', QString::SkipEmptyParts)),
        m_fromKeywords(i18nc("@info/plain A comma separated list of keywords for the "
            "journey search, indicating that a journey FROM the given stop should be searched. "
            "This keyword needs to be placed at the beginning of the field.", "from")
            .split(',', QString::SkipEmptyParts)),
        m_departureKeywords(i18nc("@info/plain A comma separated list of keywords for the "
            "journey search to indicate that given times are meant as departures (default). The "
            "order is used for autocompletion.\nNote: Keywords should be unique for each meaning.",
            "departing,depart,departure,dep").split(',', QString::SkipEmptyParts)),
        m_arrivalKeywords(i18nc("@info/plain A comma separated list of keywords for the "
            "journey search to indicate that given times are meant as arrivals. The order is used "
            "for autocompletion.\nNote: Keywords should be unique for each meaning.",
            "arriving,arrive,arrival,arr").split(',', QString::SkipEmptyParts)),
        m_timeKeywordsAt(i18nc("@info/plain A comma separated list of keywords for the "
            "journey search field, indicating that a date/time string follows.\nNote: Keywords "
            "should be unique for each meaning.", "at").split(',', QString::SkipEmptyParts)),
        m_timeKeywordsIn(i18nc("@info/plain A comma separated list of keywords for the "
            "journey search field, indicating that a relative time string follows.\nNote: Keywords "
            "should be unique for each meaning.", "in").split(',', QString::SkipEmptyParts)),
        m_timeKeywordsTomorrow(i18nc("@info/plain A comma separated list of keywords for the "
            "journey search field, as replacement for tomorrows date.\nNote: Keywords should be "
            "unique for each meaning.", "tomorrow").split(',', QString::SkipEmptyParts)),
        m_relativeTimeStringPattern(i18nc("@info/plain This is a regular expression used to match "
                "a string after the 'in' keyword in the journey search line. The english version "
                "matches 'strings like '5 mins.', '1 minute', ... '\\d+' stands for at least "
                "'one digit, '\\.' is just a point, a '?' after a character means that it's "
                "optional (eg. the 's' in 'mins?' is optional to match singular and plural forms). "
                "Normally you will only have to translate 'mins?' and 'minutes?'. The regexp must "
                "include one pair of matching 'parantheses, that match an int (the number of "
                "minutes from now). Note: '(?:...)' are non-matching parantheses.",
                "(\\d+)\\s+(?:mins?\\.?|minutes?)"))
{
}

const QStringList JourneySearchKeywords::keywords( KeywordType type ) const
{
    switch ( type ) {
    case KeywordTimeIn:
        return timeKeywordsIn();
    case KeywordTimeAt:
        return timeKeywordsAt();
    case KeywordTo:
        return toKeywords();
    case KeywordFrom:
        return fromKeywords();
    case KeywordTomorrow:
        return timeKeywordsTomorrow();
    case KeywordDeparture:
        return departureKeywords();
    case KeywordArrival:
        return arrivalKeywords();
    default:
        kDebug() << "Unknown keyword" << type;
        return QStringList();
    }
}

const QString JourneySearchKeywords::relativeTimeString( const QVariant &value ) const
{
    return i18nc( "@info/plain The automatically added relative time string, when the journey "
                "search line ends with the keyword 'in'. This should be match by the "
                "regular expression for a relative time, like '(in) 5 minutes'. That "
                "regexp and the keyword ('in') are also localizable. Don't include "
                "the 'in' here.", "%1 minutes", value.toString() );
}

/*
void JourneySearchParser::doCorrections( KLineEdit *lineEdit, QString *searchLine, int cursorPos,
                                         const QStringList &words, int removedWordsFromLeft )
{
    int selStart = -1;
    int selLength = 0;

    int pos = searchLine->lastIndexOf( ' ', cursorPos - 1 );
    int posEnd = searchLine->indexOf( ' ', cursorPos );
    if ( posEnd == -1 ) {
        posEnd = searchLine->length();
    }
    QString lastWordBeforeCursor;
    JourneySearchKeywords keywords;
    if ( posEnd == cursorPos && pos != -1 && !( lastWordBeforeCursor = searchLine->mid(
                    pos, posEnd - pos ).trimmed() ).isEmpty() ) {
        if ( keywords.timeKeywordsAt().contains( lastWordBeforeCursor, Qt::CaseInsensitive ) ) {
            // Automatically add the current time after 'at'
            QString formattedTime = KGlobal::locale()->formatTime( QTime::currentTime() );
            searchLine->insert( posEnd, ' ' + formattedTime );
            selStart = posEnd + 1; // +1 for the added space
            selLength = formattedTime.length();
        } else if ( keywords.timeKeywordsIn().contains( lastWordBeforeCursor, Qt::CaseInsensitive ) ) {
            // Automatically add '5 minutes' after 'in'
            searchLine->insert( posEnd, ' ' + keywords.relativeTimeString() );
            selStart = posEnd + 1; // +1 for the added space
            selLength = 1; // only select the number (5)
        } else {
            QStringList completionItems;
            completionItems << keywords.timeKeywordsAt() << keywords.timeKeywordsIn()
                    << keywords.timeKeywordsTomorrow() << keywords.departureKeywords()
                    << keywords.arrivalKeywords();

            KCompletion *comp = lineEdit->completionObject( false );
            comp->setItems( completionItems );
            comp->setIgnoreCase( true );
            QString completion = comp->makeCompletion( lastWordBeforeCursor );
            setJourneySearchWordCompletion( lineEdit, completion );
        }
    }

    // Select an appropriate substring after inserting something
    if ( selStart != -1 ) {
        QStringList removedWords = ( QStringList )words.mid( 0, removedWordsFromLeft );
        QString removedPart = removedWords.join( " " ).trimmed();
        QString correctedSearch;
        if ( removedPart.isEmpty() ) {
            correctedSearch = *searchLine;
        } else {
            correctedSearch = removedPart + ' ' + *searchLine;
            selStart += removedPart.length() + 1;
        }
        lineEdit->setText( correctedSearch );
        lineEdit->setSelection( selStart, selLength );
    }
}*/

bool JourneySearchAnalyzer::isInsideQuotedString( const QString& testString, int cursorPos )
{
    int posQuotes1 = testString.indexOf( '\"' );
    int posQuotes2 = testString.indexOf( '\"', posQuotes1 + 1 );
    if ( posQuotes2 == -1 ) {
        posQuotes2 = testString.length();
    }
    return posQuotes1 == -1 ? false : cursorPos > posQuotes1 && cursorPos <= posQuotes2;
}
/*
bool JourneySearchParser::parseJourneySearch( KLineEdit* lineEdit,
        const QString& search, QString* stop, QDateTime* departure,
        bool* stopIsTarget, bool* timeIsDeparture, int* posStart, int* len, bool correctString )
{
//     kDebug() << search;
    // Initialize output parameters
    stop->clear();
    *departure = QDateTime();
    *stopIsTarget = true;
    *timeIsDeparture = true;
    if ( posStart ) {
        *posStart = -1;
    }
    if ( len ) {
        *len = 0;
    }

    QString searchLine = search;
    int removedWordsFromLeft = 0;

    // Remove double spaces without changing the cursor position or selection
    int selStart = lineEdit->selectionStart();
    int selLength = lineEdit->selectedText().length();
    int cursorPos = lineEdit->cursorPosition();
    cursorPos -= searchLine.left( cursorPos ).count( "  " );
    if ( selStart != -1 ) {
        selStart -= searchLine.left( selStart ).count( "  " );
        selLength -= searchLine.mid( selStart, selLength ).count( "  " );
    }

    searchLine = searchLine.replace( "  ", " " );
    lineEdit->setText( searchLine );
    lineEdit->setCursorPosition( cursorPos );
    if ( selStart != -1 ) {
        lineEdit->setSelection( selStart, selLength );
    }

    // Get word list
    QStringList words = searchLine.split( ' ', QString::SkipEmptyParts );
    if ( words.isEmpty() ) {
        return false;
    }

    // Combine words between double quotes to one word
    // to allow stop names containing keywords.
    JourneySearchParser::combineDoubleQuotedWords( &words );

    // First search for keywords at the beginning of the string ('to' or 'from')
    QString firstWord = words.first();
    if ( toKeywords().contains( firstWord, Qt::CaseInsensitive ) ) {
        searchLine = searchLine.mid( firstWord.length() + 1 );
        cursorPos -= firstWord.length() + 1;
        ++removedWordsFromLeft;
    } else if ( fromKeywords().contains( firstWord, Qt::CaseInsensitive ) ) {
        searchLine = searchLine.mid( firstWord.length() + 1 );
        *stopIsTarget = false; // the given stop is the origin
        cursorPos -= firstWord.length() + 1;
        ++removedWordsFromLeft;
    }
    if ( posStart ) {
        QStringList removedWords = ( QStringList )words.mid( 0, removedWordsFromLeft );
        *posStart = removedWords.isEmpty() ? 0 : removedWords.join( " " ).length() + 1;
    }

    // Do corrections
    if ( correctString && !isInsideQuotedString( searchLine, cursorPos )
                && lineEdit->completionMode() != KGlobalSettings::CompletionNone ) {
        doCorrections( lineEdit, &searchLine, cursorPos, words, removedWordsFromLeft );
    }

    // Now search for keywords inside the string from back
    for ( int i = words.count() - 1; i >= removedWordsFromLeft; --i ) {
        QString word = words[ i ];
        if ( timeKeywordsAt().contains( word, Qt::CaseInsensitive ) ) {
            // An 'at' keyword was found at position i
            QString sDeparture;
            JourneySearchParser::splitWordList( words, i, stop, &sDeparture, removedWordsFromLeft );

            // Search for keywords before 'at'
            QDate date;
            JourneySearchParser::searchForJourneySearchKeywords( *stop,
                    timeKeywordsTomorrow(), departureKeywords(), arrivalKeywords(),
                    &date, stop, timeIsDeparture, len );

            // Parse date and/or time from the string after 'at'
            JourneySearchParser::parseDateAndTime( sDeparture, departure, &date );
            return true;
        } else if ( timeKeywordsIn().contains( word, Qt::CaseInsensitive ) ) {
            // An 'in' keyword was found at position i
            QString sDeparture;
            JourneySearchParser::splitWordList( words, i, stop, &sDeparture, removedWordsFromLeft );

            // Match the regexp and extract the relative datetime value
            QRegExp rx( relativeTimeStringPattern(), Qt::CaseInsensitive );
            int pos = rx.indexIn( sDeparture );
            if ( pos != -1 ) {
                int minutes = rx.cap( 1 ).toInt();

                // Search for keywords before 'in'
                QDate date = QDate::currentDate();
                JourneySearchParser::searchForJourneySearchKeywords( *stop,
                        timeKeywordsTomorrow(), departureKeywords(), arrivalKeywords(),
                        &date, stop, timeIsDeparture, len );
                *departure = QDateTime( date, QTime::currentTime().addSecs( minutes * 60 ) );
                return true;
            }
        }
    }

    *stop = searchLine;

    // Search for keywords at the end of the string
    QDate date = QDate::currentDate();
    JourneySearchParser::searchForJourneySearchKeywords( *stop, timeKeywordsTomorrow(),
            departureKeywords(), arrivalKeywords(),
            &date, stop, timeIsDeparture, len );
    *departure = QDateTime( date, QTime::currentTime() );
    return false;
}*/

void JourneySearchAnalyzer::completeStopName( KLineEdit *lineEdit, const QString &completion )
{
    if ( completion.isEmpty() ) {
        return;
    }
    kDebug() << "MATCH" << completion;

    Results results = analyze( lineEdit->text() );
    int stopNamePosStart = -1; // TODO FIXME GET STOPNAME POSITION results.syntaxItem( StopNameValue )->position();
    int stopNameLen = results.stopName().length(); // TODO may be wrong if the input contains double spaces "  "
    kDebug() << "STOPNAME =" << lineEdit->text().mid( stopNamePosStart, stopNameLen );

    int selStart = lineEdit->selectionStart();
    if ( selStart == -1 ) {
        selStart = lineEdit->cursorPosition();
    }
    bool stopNameChanged = selStart > stopNamePosStart && selStart + lineEdit->selectedText().length()
                        <= stopNamePosStart + stopNameLen;
    if ( stopNameChanged ) {
        lineEdit->setText( lineEdit->text().replace(stopNamePosStart, stopNameLen, completion) );
        lineEdit->setSelection( stopNamePosStart + stopNameLen, completion.length() - stopNameLen );
    }
}

QStringList JourneySearchAnalyzer::notDoubleQuotedWords( const QString& input )
{
    QStringList words = input.split( ' ', QString::SkipEmptyParts );
    combineDoubleQuotedWords( &words, false );
    return words;
}

void JourneySearchAnalyzer::combineDoubleQuotedWords( QStringList* words, bool reinsertQuotedWords )
{
    int quotedStart = -1, quotedEnd = -1;
    for ( int i = 0; i < words->count(); ++i ) {
        if ( words->at( i ).startsWith( '\"' ) ) {
            quotedStart = i;
        }
        if ( words->at( i ).endsWith( '\"' ) ) {
            quotedEnd = i;
            break;
        }
    }
    if ( quotedStart != -1 ) {
        if ( quotedEnd == -1 ) {
            quotedEnd = words->count() - 1;
        }

        // Combine words
        QString combinedWord;
        for ( int i = quotedEnd; i >= quotedStart; --i ) {
            combinedWord = words->takeAt( i ) + ' ' + combinedWord;
        }

        if ( reinsertQuotedWords ) {
            words->insert( quotedStart, combinedWord.trimmed() );
        }
    }
}

QDebug &operator <<( QDebug debug, ErrorSeverity error )
{
    switch ( error ) {
    case ErrorFatal:
        return debug << "ErrorFatal";
    case ErrorSevere:
        return debug << "ErrorSevere";
    case ErrorMinor:
        return debug << "ErrorMinor";
    case ErrorInformational:
        return debug << "ErrorInformational";
    default:
        return debug << static_cast<int>(error);
    }
};

QDebug &operator <<( QDebug debug, AnalyzerState state )
{
    switch ( state ) {
    case NotStarted:
        return debug << "NotStarted";
    case Running:
        return debug << "Running";
    case Finished:
        return debug << "Finished";
    default:
        return debug << static_cast<int>(state);
    }
};

QDebug &operator <<( QDebug debug, AnalyzerResult state )
{
    switch ( state ) {
    case Accepted:
        return debug << "Accepted";
    case AcceptedWithErrors:
        return debug << "AcceptedWithErrors";
    case Rejected:
        return debug << "Rejected";
    default:
        return debug << static_cast<int>(state);
    }
};

}; // namespace Parser
