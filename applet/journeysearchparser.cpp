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

#ifdef DEBUG_PARSING
    QByteArray __DEBUG_PARSING_LEVEL( void *data ) {
        SyntacticalAnalyzer::MatchData *matchData =
                reinterpret_cast<SyntacticalAnalyzer::MatchData*>(data);
        QByteArray s;
        SyntacticalAnalyzer::MatchData *level = matchData;
        while ( level->parent ) {
            s += DEBUG_PARSING_INDENTATION;
            level = level->parent;
        }
        if ( matchData->onlyTesting ) {
            s += "[TEST] ";
        }
        return s;
    };
#endif

const QString LexicalAnalyzer::ALLOWED_OTHER_CHARACTERS = ":,.´`'!?&()_\"'";

JourneySearchAnalyzer::JourneySearchAnalyzer( SyntaxItemPointer syntaxItem,
                                              JourneySearchKeywords *keywords,
                                              AnalyzerCorrections corrections,
                                              int cursorPositionInInputString ) :
        m_keywords(keywords ? keywords : new JourneySearchKeywords()),
        m_ownKeywordsObject(!keywords),
        m_lexical(new LexicalAnalyzer(corrections, cursorPositionInInputString)),
        m_syntactical(new SyntacticalAnalyzer(syntaxItem, m_keywords, corrections, cursorPositionInInputString)),
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

Results JourneySearchAnalyzer::resultsFromMatchItem( const MatchItem &matchItem,
                                                     JourneySearchKeywords *keywords )
{
    const bool ownKeywords = !keywords;
    if ( ownKeywords ) {
        keywords = new JourneySearchKeywords();
    }

    Results results;
    results.m_rootItem = matchItem;
    const MatchItems allItems = matchItem.allChildren();

    bool tomorrow = false;
    for ( MatchItems::ConstIterator it = allItems.constBegin(); it != allItems.constEnd(); ++it ) {
        if ( it->type() == MatchItem::Error ) {
            results.m_hasErrors = true;
            results.m_errorItems << &*it;
        } else { //if ( it->valueType() != NoValue ) {
            results.m_matchItems[ it->type() ] << &*it;
        }

//         switch ( it->type() ) {
//         case MatchItem::Invalid:
//             kDebug() << "Got an invalid MatchItem! The MatchItem::Invalid flag should only be "
//                         "used as argument to functions.";
//             continue;
//         case MatchItem::Error:
//             results.m_hasErrors = true;
//             results.m_errorItems << &*it;
//             itemText = it->input();
//             break;
//         case MatchItem::Sequence:
//             kDebug() << "TODO"; // TODO
//             break;
//         case MatchItem::Option:
//             kDebug() << "TODO"; // TODO
//             break;
//         case MatchItem::Keyword:
//         case MatchItem::Words:
//         case MatchItem::Number:
//         case MatchItem::Character:
//         case MatchItem::String:
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
//             break;
//         }

//         if ( it->type() != MatchItem::Error ) {
//             output << itemText;
//         }
//         outputWithErrors << itemText;
    }

    if ( !results.hasMatchItem(MatchItem::Word) ) {
        results.m_hasErrors = true;
    } else {
        QStringList stopNameWords;
        foreach ( const MatchItem *matchItem, results.m_matchItems[MatchItem::Word] ) {
            if ( matchItem->valueType() == StopNameValue ) {
                stopNameWords << matchItem->text();
                results.m_stopNameItems << matchItem;
            }
        }
        results.m_stopName += stopNameWords.join(" ");
    }

    if ( results.hasMatchItem(MatchItem::Keyword) ) {
        foreach ( const MatchItem *matchItem, results.m_matchItems[MatchItem::Keyword] ) {
            KeywordType keyword = static_cast<KeywordType>( matchItem->value().toInt() );
            switch ( keyword ) {
            case KeywordTo:
                results.m_stopIsTarget = true;
                break;
            case KeywordFrom:
                results.m_stopIsTarget = false;
                break;
            case KeywordTomorrow:
                tomorrow = true;
                break;
            case KeywordDeparture:
                results.m_timeIsDeparture = true;
                break;
            case KeywordArrival:
                results.m_timeIsDeparture = false;
                break;
            case KeywordTimeIn:
                results.m_time = QDateTime::currentDateTime().addSecs(
                        60 * matchItem->keywordValue().toInt() );
                break;
            case KeywordTimeAt:
                results.m_time = matchItem->keywordValue().toDateTime();
                break;
            default:
                kDebug() << "Unknown keyword" << static_cast<int>(keyword);
                break;
            }

            results.m_keywords.insert( keyword, matchItem );
        }
    }

    if ( !results.m_time.isValid() ) {
        // No time given in input string
        results.m_time = QDateTime::currentDateTime();
    }
    if ( tomorrow ) {
        results.m_time = results.m_time.addDays( 1 );
    }
//     results.m_outputString = output.join( " " );
//     results.m_outputStringWithErrors = outputWithErrors.join( " " );
    if ( ownKeywords ) {
        delete keywords;
    }

    return results;
}

QString Results::updateOutputString( SyntaxItemPointer syntaxItem,
                                     AnalyzerCorrections appliedCorrections,
                                     JourneySearchKeywords *keywords )
{
    bool ownKeywords = !keywords;
    if ( ownKeywords ) {
        keywords = new JourneySearchKeywords();
    }

    QString output = updateOutputString( syntaxItem, &m_rootItem, appliedCorrections, keywords );
    if ( ownKeywords ) {
        delete keywords;
    }
    return output.trimmed();
}

QString Results::updateOutputString( SyntaxItemPointer syntaxItem, const MatchItem *matchItem,
                                     AnalyzerCorrections corrections,
                                     JourneySearchKeywords *keywords )
{
    if ( syntaxItem->flags().testFlag(SyntaxItem::RemoveFromInput) ) {
        return QString(); // Remove syntax item by returning an empty string
    }

    QString output;
    switch ( syntaxItem->type() ) {
    case SyntaxItem::MatchSequence:
        Q_ASSERT( matchItem );
        if ( matchItem->type() == MatchItem::Sequence ) {
            output += updateOutputStringChildren( syntaxItem, matchItem, corrections, keywords );
        } else {
            // May be a one-item sequence (with the sequence item left out)
            SyntaxItems syntaxChildren = syntaxItem->children();
            SyntaxItems::ConstIterator syntaxChild = syntaxChildren.constBegin();
            if ( matchItem->matchedSyntaxItemIndex() == 0 ) {
                // There was a match for the current syntax item
                output += updateOutputString( *syntaxChild, matchItem, corrections, keywords );
            } else if ( (*syntaxChild)->flags().testFlag(SyntaxItem::AddToInput) ) {
                // There was no match for the current syntax item, add it
                output += updateOutputString( *syntaxChild, 0, corrections, keywords );
            }
        }
        break;
    case SyntaxItem::MatchOption:
        Q_ASSERT( matchItem );
        if ( matchItem->type() == MatchItem::Option ) {
            output += updateOutputStringChildren( syntaxItem, matchItem, corrections, keywords );
        }
        break;
    case SyntaxItem::MatchKeyword:
        if ( matchItem && matchItem->type() == MatchItem::Keyword ) {
            SyntaxKeywordItem *keywordItem = qobject_cast<SyntaxKeywordItem*>( syntaxItem );
            if ( keywordItem->keyword() == static_cast<KeywordType>(matchItem->value().toInt()) ) {
                // There is a matching MatchItem for the current keyword syntax item
                if ( keywordItem->flags().testFlag(Parser::SyntaxItem::ChangeInput)
                     && keywordItem->addToInputValue().isValid() )
                {
                    // Change the value of the keyword item
                    output += matchItem->lexems().first().input();
                    output += ' ' + keywordItem->addToInputValue().toString(); // TODO
                } else {
                    // Add output of the keyword item
                    output += matchItem->text( corrections, false );
                }
            }
        } else if ( !matchItem && syntaxItem->flags().testFlag(SyntaxItem::AddToInput) ) {
            // No match for the current keyword syntax item, but it should be added
            SyntaxKeywordItem *keywordItem = qobject_cast<SyntaxKeywordItem*>( syntaxItem );

            if ( !keywords->hasValue(keywordItem->keyword()) ) {
                // Keyword without a value
                output += keywords->keywords( keywordItem->keyword() ).first() + ' ';
            } else if ( keywordItem->addToInputValue().isValid() ) {
                // The keyword needs a value and it was found in the keyword syntax item
                output += keywords->keywords( keywordItem->keyword() ).first() + ' ';
                output += keywordItem->addToInputValue().toString() + ' ';
            } else {
                // The keyword needs a value, but none was given
                kDebug() << "Can not add keyword syntax item, because no value has been set using "
                            "SyntaxItem::setAddToInputValue()";
            }
        }
        break;
    default:
        if ( matchItem ) {
            // There is a matching MatchItem for the current syntax item
            output += matchItem->text( corrections, false );
        } else if ( syntaxItem->flags().testFlag(SyntaxItem::AddToInput) ) {
            // There was no match for the current syntax item, but it should be added
            kDebug() << "Can not add syntax items of type" << syntaxItem->type()
                     << "(not implemented)";
//             if ( syntaxItem->addToInputValue().isValid() ) {
//                 output += syntaxItem->addToInputValue().toString(); // TODO
//             } else {
//                 kDebug() << "Can not add syntax item, because no value has been set using "
//                             "SyntaxItem::setAddToInputValue()";
//             }
        }
        break;
    }

    return output;
}

QString Results::updateOutputStringChildren( SyntaxItemPointer syntaxItem,
        const Parser::MatchItem* matchItem, AnalyzerCorrections corrections,
        JourneySearchKeywords* keywords )
{
    Q_ASSERT_X( !syntaxItem->flags().testFlag(SyntaxItem::AddToInput),
                "Results::updateOutputStringChildren()",
                "Can not add non-terminal items to the input (SyntaxItem::AddToInput), "
                "set this flag only to terminal items, ie. leafs of syntax item trees "
                "(except for keyword items)." );
    QString output;

    // Initialize syntax and match item iterators
    SyntaxItems syntaxChildren = syntaxItem->children();
    SyntaxItems::ConstIterator syntaxChild = syntaxChildren.constBegin();
    MatchItems matchChildren = matchItem->children();
    MatchItems::ConstIterator matchChild = matchChildren.constBegin();

    // Iterate through the syntax item children while having the current index present
    for ( int i = 0; i < syntaxChildren.count(); ++i, ++syntaxChild ) {
        // Add erroneous output (based on corrections)
        output += updatedOutputStringSkipErrorItems( matchChildren, &matchChild, corrections );

        // Test if the next MatchItem is a match of the current SyntaxItem
        if ( matchChild != matchChildren.constEnd() && matchChild->matchedSyntaxItemIndex() == i ) {
            // There was a match for the current syntax item
            do {
                // Add output of the matched child item
                output += updateOutputString( *syntaxChild, &*matchChild, corrections, keywords );
                if ( matchChild == matchChildren.constEnd() ) {
                    break; // No more match item children
                } else {
                    // Go to next match item child and add possible new erroneous output
                    ++matchChild;
                    output += updatedOutputStringSkipErrorItems( matchChildren, &matchChild,
                                                                 corrections );
                }
            } while( matchChild != matchChildren.constEnd()
                     && matchChild->matchedSyntaxItemIndex() == i );
        } else if ( (*syntaxChild)->flags().testFlag(SyntaxItem::AddToInput) ) {
            // There was no match for the current syntax item, but output should be added
            output += updateOutputString( *syntaxChild, 0, corrections, keywords );
        }
    }

    return output;
}

QString Results::updatedOutputStringSkipErrorItems( const MatchItems &matchItems,
        QList< MatchItem >::ConstIterator *iterator, AnalyzerCorrections appliedCorrections )
{
    QString output;
    while ( *iterator != matchItems.constEnd() && (*iterator)->type() == MatchItem::Error ) {
        output += (*iterator)->text( appliedCorrections );
        ++(*iterator);
    }
    return output;
}

QString Results::updatedOutputString( const QString &updatedStopName,
        const QHash<KeywordType, QString> &updateKeywordValues,
        const MatchItemTypes &removeItems, AnalyzerCorrections appliedCorrections,
        JourneySearchKeywords *keywords ) const
{
    bool ownKeywords = !keywords;
    if ( ownKeywords ) {
        keywords = new JourneySearchKeywords();
    }

    // Insert dummy match items for updated item values without an associated match item
        // TODO FIXME Get the position of the stop name out of the new output.. syntaxItemRoot or something
//     MatchItem item = m_rootItem.copy();
//     item.combineStopNameItems();
    QString output = m_rootItem.input();
//     if ( m_stopNameItems.isEmpty() ) {
//         if ( !updatedStopName.isEmpty() ) {
//             int stopNamePos = 0;
//             if ( hasKeyword(KeywordTo) | hasKeyword(KeywordFrom) ) {
//                 const MatchItem *keyword = hasKeyword(KeywordTo)
//                         ? keyword(KeywordTo) : keyword(KeywordFrom);
//                 stopNamePos = keyword->position() + keyword->input().length() + 1;
//             }
//             output.insert( stopNamePos, updatedStopName + ' ' );
//         }
//     } else {
//         output.replace( m_stopNameItems.first()->position(), m_stopName.length(), updatedStopName );
//     }
/*
    for ( QHash<KeywordType, QString>::ConstIterator itUpdate = updateKeywordValues.constBegin();
          itUpdate != updateKeywordValues.constEnd(); ++itUpdate )
    {
        if ( hasKeyword(itUpdate.key()) ) {
            const MatchItem *keyword = keyword( itUpdate.key() );
            if ( itUpdate.value().isEmpty() ) {
                output.remove( keyword->position(), keyword->input().length() );
            } else {
                output.replace( keyword->position(), keyword->input().length(), itUpdate.value() );
            }
        } else {
        }
    }*/

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
//             if ( !flags.testFlag(ErroneousOutputString) ) {
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
    MatchItem matchItem = m_syntactical->analyze( lexems );
    m_contextual->setCursorValues( m_syntactical->cursorOffset(), m_syntactical->selectionLength() );
    m_results = resultsFromMatchItem( m_contextual->analyze(QLinkedList<MatchItem>() << matchItem).first() ); // TODO No QLinkedList
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
    m_stopNameItems.clear();
    m_matchItems.clear();
    m_keywords.clear();
    m_cursorOffset = 0;
    m_result = Rejected;
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
void Analyzer<Container, Item>::setError( ErrorSeverity severity, const QString& erroneousText,
        const QString& errorMessage, int position, MatchItem *parent )
{
    Q_UNUSED( parent );
    if ( severity >= m_minRejectSeverity ) {
        m_result = Rejected;
        DO_DEBUG_PARSING_ERROR( Rejected, position, errorMessage, erroneousText );
    } else if ( severity >= m_minAcceptWithErrorsSeverity ) {
        if ( m_result > AcceptedWithErrors ) {
            m_result = AcceptedWithErrors;
        }
        DO_DEBUG_PARSING_ERROR( AcceptedWithErrors, position, errorMessage, erroneousText );
    } else {
        DO_DEBUG_PARSING_ERROR( Accepted, position, errorMessage, erroneousText );
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

void LexicalAnalyzer::moveToBeginning()
{
    Analyzer< QString, QChar >::moveToBeginning();
    m_pos = 0;
}

void SyntacticalAnalyzer::setError2( MatchData *matchData, ErrorSeverity severity,
        const QString &erroneousText, const QString &errorMessage, int position,
        const LexemList &lexems )
{
    Analyzer::setError( severity, erroneousText, errorMessage, position,
                        matchData->parentMatchItem );
    addOutputItem( matchData, MatchItem(MatchItem::Error, lexems.isEmpty()
            ? LexemList() << Lexem(Parser::Lexem::Error, QString(), position) : lexems,
            ErrorMessageValue, errorMessage, MatchItem::DefaultMatchItem, -1) );
}

void SyntacticalAnalyzer::setError2( MatchData *matchData, ErrorSeverity severity,
        const QString &erroneousText, AnalyzerCorrection errorCorrection, int position,
        const LexemList &lexems )
{
    Analyzer::setError( severity, erroneousText, QString("Error correction: %1").arg(errorCorrection),
                        position, matchData->parentMatchItem );
    addOutputItem( matchData, MatchItem(MatchItem::Error, lexems.isEmpty()
            ? LexemList() << Lexem(Parser::Lexem::Error, QString(), position) : lexems,
            ErrorCorrectionValue, errorCorrection, MatchItem::DefaultMatchItem, -1) );
}

void ContextualAnalyzer::setError( ErrorSeverity severity, const QString &erroneousText,
        const QString &errorMessage, int position, MatchItem *parent )
{
    Analyzer::setError( severity, erroneousText, errorMessage, position, parent );
    MatchItem errorItem( MatchItem::Error, LexemList(), ErrorMessageValue, errorMessage,
                         MatchItem::DefaultMatchItem, -1 );
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
            if ( m_result > AcceptedWithErrors ) {
                m_result = AcceptedWithErrors;
            }
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
        qDebug() << "Invalid:" << QString("%1").arg(c.unicode(), 0, 16);
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
            if ( m_corrections.testFlag(RemoveInvalidCharacters) ) {
                if ( m_result > AcceptedWithErrors ) {
                    m_result = AcceptedWithErrors;
                }
            } else {
                m_result = Rejected;
                m_state = Finished;
                return m_output;
            }
            break;
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

SyntacticalAnalyzer::SyntacticalAnalyzer( SyntaxItemPointer syntaxItem,
                                          JourneySearchKeywords *keywords,
                                          AnalyzerCorrections corrections,
                                          int cursorPositionInInputString, int cursorOffset )
        : Analyzer(corrections, cursorPositionInInputString, cursorOffset),
          m_syntaxItem(syntaxItem ? syntaxItem : Syntax::journeySearchSyntaxItem()),
          m_keywords(keywords)
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
    delete m_syntaxItem;
}

MatchItem SyntacticalAnalyzer::analyze( const QLinkedList<Lexem> &input )
{
    // Initialize
    m_input = input;
    m_state = Running;
    m_result = Accepted;
    moveToBeginning();

    // Match the used syntax item
    MatchData matchData( SyntaxItems() << m_syntaxItem );
    MatchItems firstMatchedItems;
    if ( !matchItem(&matchData, &firstMatchedItems) ) {
        m_result = Rejected;
    }

    // Check for additional input
    if ( !isAtImaginaryIteratorEnd() ) {
        if ( m_result > AcceptedWithErrors ) {
            m_result = AcceptedWithErrors;
        }
        matchData.parentMatchItem = &m_outputRoot; // Add remaining input items to the one match item
        while ( !isAtImaginaryIteratorEnd() ) {
            // No more SyntaxItems to match, but more input
            setError2( &matchData, ErrorSevere, m_inputIterator->input(), SkipUnexpectedTokens,
                       m_inputIterator->position(), LexemList() << *m_inputIterator );
            readItemAndSkipSpaces( &matchData );
        }
    }

    m_state = Finished;
    return m_outputRoot;
}

void SyntacticalAnalyzer::addOutputItem( MatchData *matchData, const MatchItem &matchItem )
{
    if ( matchData->onlyTesting ) {
        return;
    }

    DO_DEBUG_PARSING_OUTPUT( matchItem.input() );
    if ( matchData->parentMatchItem ) {
        // Sort the new syntaxItem into the child list of the parent item, sorted by position
        matchData->parentMatchItem->addChild( matchItem );
    } else {
        m_outputRoot = matchItem;
//         // Sort the new syntaxItem into the output list, sorted by position
//         if ( m_output.isEmpty() || m_output.last().position() < matchItem.position() ) {
//             // Append the new item
//             m_output.insert( m_output.end(), matchItem );
//         } else {
//             QLinkedList<MatchItem>::iterator it = m_output.begin();
//             while ( it->position() < matchItem.position() ) {
//                 ++it;
//             }
//             m_output.insert( it, matchItem );
//         }
    }
}

bool SyntacticalAnalyzer::matchNumber( MatchData *matchData, MatchItems *matchedItems )
{
    DO_DEBUG_PARSING_FUNCTION_CALL( matchNumber,
            (*matchData->item)->toString(SyntaxItem::ShortDescription) );
    const SyntaxNumberItem *numberItem = qobject_cast< const SyntaxNumberItem* >( matchData->item->data() );

    // Initialize
    QString numberString( m_inputIterator->input() );
//     int removedDigits = 0;
    bool ok;
    int number;
    if ( m_corrections.testFlag(CorrectNumberRanges) ) {
        if ( numberString.length() > 2 ) {
//             removedDigits = numberString.length() - 2;
            // TODO This only works with number strings not longer than three characters
            if ( m_cursorPositionInInputString == m_inputIterator->position() + 1 ) {
                // Cursor is here (|): "X|XX:XX", overwrite second digit
                numberString.remove( 1, 1 );
            }
            numberString = numberString.left( 2 );
        }

        number = numberString.toInt( &ok );
        if ( !ok ) {
            return false; // Number invalid
        }

        // Put the number into the given range [min, max]
        number = qBound( numberItem->min(), number, numberItem->max() );
        if ( number != number && m_result > AcceptedWithErrors ) {
            m_result = AcceptedWithErrors;
        }
    } else {
        number = numberString.toInt( &ok );
        if ( !ok || number < numberItem->min() || number > numberItem->max() ) {
            return false; // Number out of range or invalid
        }
    }

    #ifdef FILL_MATCHES_IN_SYNTAXITEM
        numberItem->addMatch( m_inputIterator->position(), m_inputIterator->input(), number );
    #endif
    *matchedItems << MatchItem( MatchItem::Number, LexemList() << *m_inputIterator,
                                numberItem->valueType(), number );
    addOutputItem( matchData, matchedItems->last() );
    readItemAndSkipSpaces( matchData );
    DO_DEBUG_PARSING_MATCH( Number, numberString );
    return true;
}

int SyntacticalAnalyzer::matchKleeneStar( MatchData *matchData, MatchItems *matchedItems )
{
    DO_DEBUG_PARSING_FUNCTION_CALL( matchKleeneStar,
            (*matchData->item)->toString(SyntaxItem::ShortDescription) );
    Q_ASSERT_X( (*matchData->item)->flags().testFlag(Parser::SyntaxItem::KleenePlus),
                "SyntacticalAnalyzer::matchKleeneStar",
                "The current item should have a Kleene flag set (KleenePlus or KleeneStar)" );

    int matches = 0; // Counts matches of the Kleene star item (matchData->item)
    if ( (*matchData->item)->flags().testFlag(SyntaxItem::MatchGreedy) ) {
        // Greedy matching, ie. stop when this item doesn't match any longer
        DO_DEBUG_PARSING_INFO( "Match item using Kleene star (greedy):" << (*matchData->item)->type() );
        while ( !isAtImaginaryIteratorEnd() && matchItemKleeneUnaware(matchData, matchedItems) ) {
            ++matches; // The item matched, proceed with greedy Kleene star
        }
    } else {
        // Non-greedy matching, ie. stop when the next item matches
        DO_DEBUG_PARSING_INFO( "Match item using Kleene star (non-greedy):" << (*matchData->item)->type() );
        while ( !isAtImaginaryIteratorEnd() ) {
            MatchData checkMatchData = *matchData; // Copy MatchData
            bool hasFollowingItem;
            bool followingMatches;
            do {
                // Find a MatchData object (maybe a parent) with a following item
                hasFollowingItem = true;
                while ( checkMatchData.item + 1 == checkMatchData.items.constEnd() ) {
                    // No following items in the current MatchData object
                    if ( checkMatchData.parent ) {
                        // Use parent MatchData object to test for a following item
                        checkMatchData = *checkMatchData.parent;
                    } else {
                        // No following item and no parent with possible following items
                        hasFollowingItem = false;
                        break;
                    }
                }

                // Test if the following item matches (without skipping unexpected items)
                followingMatches = false;
                if ( hasFollowingItem ) {
                    ++checkMatchData.item; // Go to the next item
                    checkMatchData.onlyTesting = true; // Do not skip unexpected items
                    QLinkedList<Lexem>::ConstIterator oldIterator = m_inputIterator;
                    MatchItems subMatchedItems;
                    DO_DEBUG_PARSING_INFO( "Test following item for non-greedy Kleene star:"
                                           << (*checkMatchData.item)->type() );
                    if ( matchItem(&checkMatchData, &subMatchedItems) ) {
                        DO_DEBUG_PARSING_INFO( "The following item matches, do not proceed "
                                "with Kleene star and go back to old input position" );
                        followingMatches = true;
                        m_inputIterator = oldIterator;
                        m_inputIteratorLookahead = m_inputIterator;
                        updateLookahead();
                        break; // The following item matches, don't proceed with Kleene star
                    }
                } else {
                    break; // No following items, do not proceed with Kleene star
                }
            // Test next following item, if the current one did not match but is optional
            } while( (*checkMatchData.item)->isOptional() );

            // Match Kleene star item if the following item did not match
            if ( followingMatches || !matchItemKleeneUnaware(matchData, matchedItems) ) {
                // Stop with Kleene star:
                // The following item matched or the Kleene star item did not match
                break;
            } else {
                ++matches; // This item matched (maybe again), proceed with non-greedy Kleene star
            }
        }
    }

    return matches; // Return match count
}

bool SyntacticalAnalyzer::matchItem( MatchData *matchData, MatchItems *matchedItems )
{
    if ( matchData->item == matchData->items.constEnd() ) {
        return false; // Not an item, return false ("not matched")
    }

    // A KleeneStar is an optional KleenePlus.
    // That means, that the first test for KleenePlus also matches KleeneStar.
    int matches = 0; // Counts matches of the item (matchData->item)
    const SyntaxItem::Flags flags = (*matchData->item)->flags();
    if ( flags.testFlag(SyntaxItem::KleenePlus) ) {
        if ( flags.testFlag(SyntaxItem::KleeneStar) ) {
            // Match any number of times
            DO_DEBUG_PARSING_FUNCTION_CALL( matchItem, "Kleene star" << (*matchData->item)->type() );
            matches += matchKleeneStar( matchData, matchedItems );
        } else {
            // Match at least once
            DO_DEBUG_PARSING_FUNCTION_CALL( matchItem, "Kleene plus" << (*matchData->item)->type() );
            if ( matchItemKleeneUnaware(matchData, matchedItems) ) {
                ++matches; // Count the first matched item

                // Then match any additional number of times
                matches += matchKleeneStar( matchData, matchedItems );
            }
        }
    } else if ( matchItemKleeneUnaware(matchData, matchedItems) ) {
        ++matches; // No Kleene star or plus, only one match
    }

    // Matched at least one time
    return matches > 0;
}

bool SyntacticalAnalyzer::matchSequence( MatchData *matchData, MatchItems *matchedItems )
{
    const SyntaxSequenceItem *sequence = qobject_cast< const SyntaxSequenceItem* >( *matchData->item );
    Q_ASSERT( sequence );
    DO_DEBUG_PARSING_FUNCTION_CALL( matchSequence, sequence->toString(SyntaxItem::ShortDescription) );

    // Don't add the sequence item to the output if there is only one child item
    const bool addSequenceItem = sequence->items()->count() != 1;

    MatchItem sequenceItem( MatchItem::Sequence );
    MatchItems matchedSequenceItems;
    bool hasMatch = false;
    MatchData sequenceMatchData( *sequence->items(),
            addSequenceItem ? &sequenceItem : matchData->parentMatchItem, matchData,
            matchData->onlyTesting );
    int i = 0;
    for ( ; sequenceMatchData.item != sequenceMatchData.items.constEnd(); ++sequenceMatchData.item ) {
        matchedSequenceItems.clear();
        const bool matched = matchItem( &sequenceMatchData, &matchedSequenceItems );
        hasMatch = hasMatch || matched;

        // Optional items in the sequence do not need to match
        if ( matched || (*sequenceMatchData.item)->isOptional() ) {
            if ( matched ) {
                for ( int n = 0; n < matchedSequenceItems.count(); ++n ) {
                    matchedSequenceItems[n].setMatchedSyntaxItemIndex( i );
                }
            }
            if ( isAtImaginaryIteratorEnd() ) {
                // No more input after the last matched item of the sequence
                ++sequenceMatchData.item; // Move to the sequence item after the last matched one
                for ( ; sequenceMatchData.item != sequence->items()->constEnd(); ++sequenceMatchData.item ) {
                    if ( !(*sequenceMatchData.item)->isOptional() ) {
                        DO_DEBUG_PARSING_INFO( "More non-optional SyntaxItems in a sequence, "
                                "but no more input" << (*sequenceMatchData.item)->toString() );
                        return false; // There is a non-optional SyntaxItems in the list (but no input)
                    }
                }
                // All following SyntaxItems are optional
                DO_DEBUG_PARSING_INFO( "No more input, but all following SyntaxItems "
                        "in the sequence are optional" );
                break;
            }
        } else {
            return false; // A non-optional item in the sequence did not match
        }
        ++i;
    }

    if ( addSequenceItem ) {
        if ( sequence->valueType() != NoValue ) {
            // Construct the value for the sequence item from it's children
            sequenceItem.setValue( readValueFromChildren(sequence->valueType(), &sequenceItem) );
        }

        addOutputItem( matchData, sequenceItem );
        #ifdef FILL_MATCHES_IN_SYNTAXITEM
            sequence->addMatch( sequenceItem );
        #endif
        *matchedItems << sequenceItem;
    } else {
        // Don't add a sequence item to the output, there is only one item in the sequence,
        // which has already been added to the parent (instead of the sequence item)
        #ifdef FILL_MATCHES_IN_SYNTAXITEM
            sequence->addMatch( matchedSequenceItem.position(), matchedSequenceItem.input(), QVariant() );
        #endif
        *matchedItems << matchedSequenceItems;
    }

    #ifdef DEBUG_PARSING_MATCH
        if ( hasMatch ) {
            DO_DEBUG_PARSING_MATCH( Sequence, sequence->toString(0, SyntaxItem::ShortDescription) );
        }
    #endif
    return hasMatch;
}

bool SyntacticalAnalyzer::matchOption( MatchData *matchData, MatchItems *matchedItems )
{
    const SyntaxOptionItem *option = qobject_cast< const SyntaxOptionItem* >( *matchData->item );
    Q_ASSERT( option );
    DO_DEBUG_PARSING_FUNCTION_CALL( matchOption, option->toString(SyntaxItem::ShortDescription) );

    MatchItem optionItem( MatchItem::Option );
    MatchItems matchedOptionItems;
    int i = 0;
    for ( SyntaxItems::ConstIterator it = option->options()->constBegin();
          it != option->options()->constEnd(); ++it )
    {
        MatchData optionMatchData( *option->options(), &optionItem, matchData,
                                    matchData->onlyTesting );
        optionMatchData.item = it;

        bool matched = matchItem( &optionMatchData, &matchedOptionItems );
        if ( matched ) {
            for ( int n = 0; n < matchedOptionItems.count(); ++n ) {
                matchedOptionItems[n].setMatchedSyntaxItemIndex( i );
            }
            if ( (*it)->isOptional() ) {
                kDebug() << "An option item shouldn't have optional child items.\n"
                        "Because an optional item always matches, items after an optional child\n"
                        "are never used. Make the option item optional instead.";
            }
            addOutputItem( matchData, optionItem );
            #ifdef FILL_MATCHES_IN_SYNTAXITEM
                option->addMatch( optionItem.position(), optionItem.input(), optionItem.value() );
            #endif
            *matchedItems << optionItem;
            DO_DEBUG_PARSING_MATCH( Option, option->toString(0, SyntaxItem::ShortDescription) );
            return true;
        }
        ++i;
    }

    DO_DEBUG_PARSING_INFO( "No item of a non-optional option list matched" );
    return false;
}

bool SyntacticalAnalyzer::matchKeyword( MatchData *matchData, MatchItems *matchedItems )
{
    const SyntaxKeywordItem *keyword = qobject_cast< const SyntaxKeywordItem* >( *matchData->item );
    Q_ASSERT( keyword );
    DO_DEBUG_PARSING_FUNCTION_CALL( matchKeyword, keyword->toString(SyntaxItem::ShortDescription) );

    MatchItem match;
    bool matched = matchKeywordInList( matchData, keyword->keyword(), &match );
    if ( matched ) {
        *matchedItems << match;

        if ( keyword->valueSequence() ) {
            // Match value sequence for the keyword
            MatchItems valueSequenceItems;
            MatchData valueMatchData( SyntaxItems() << keyword->valueSequence(), &match, matchData );
            matched = matchSequence( &valueMatchData, &valueSequenceItems );
        }

        if ( matched ) {
            addOutputItem( matchData, match );
            #ifdef FILL_MATCHES_IN_SYNTAXITEM
                keyword->addMatch( match.position(), match.input(), match.value() );
            #endif
        }

        DO_DEBUG_PARSING_MATCH( Keyword, keyword->keyword() );
        return true;
    } else {
        // NOTE [match] is uninitialized, if matched is false
//         if ( !keyword->isOptional() ) {
            // TODO Error handling: Add SyntaxItem::Error item with default value text
            // (eg. minimum for MatchItemNumber)
//         }
        return false;
    }
}

bool SyntacticalAnalyzer::matchItemKleeneUnaware( MatchData *matchData, MatchItems *matchedItems,
                                                  AnalyzerCorrections enabledCorrections )
{
    DO_DEBUG_PARSING_FUNCTION_CALL( matchItemKleeneUnaware,
            (*matchData->item)->toString(SyntaxItem::ShortDescription) );

    if ( isAtImaginaryIteratorEnd() ) {
        return false; // TODO
    }

    QLinkedList<Lexem>::ConstIterator oldIterator = m_inputIterator;
    bool matched;
    switch ( (*matchData->item)->type() ) {
    case SyntaxItem::MatchSequence:
        matched = matchSequence( matchData, matchedItems );
        break;
    case SyntaxItem::MatchOption:
        matched = matchOption( matchData, matchedItems );
        break;
    case SyntaxItem::MatchKeyword:
        matched = matchKeyword( matchData, matchedItems );
        break;
    case SyntaxItem::MatchNumber:
        matched = matchNumber( matchData, matchedItems );
        break;
    case SyntaxItem::MatchString:
        matched = matchString( matchData, matchedItems );
        break;
    case SyntaxItem::MatchWord:
        matched = matchWord( matchData, matchedItems );
        break;
    case SyntaxItem::MatchCharacter:
        matched = matchCharacter( matchData, matchedItems );
        break;
    case SyntaxItem::MatchNothing:
        DO_DEBUG_PARSING_MATCH( Nothing, "" );
        return true; // Nothing to do
    }

    if ( matched ) {
        return true;
    }

    if ( !isAtImaginaryIteratorEnd() ) {
        // No match, try enabled corrections
        // First try to insert a missing required token, if it is similar to the input
        if ( enabledCorrections.testFlag(InsertMissingRequiredTokens)
             && m_corrections.testFlag(InsertMissingRequiredTokens)
             && !(*matchData->item)->isOptional() )
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
        if ( !matchData->onlyTesting && enabledCorrections.testFlag(SkipUnexpectedTokens)
             && m_corrections.testFlag(SkipUnexpectedTokens) )
        {
            const int MAX_SKIP_UNEXPECTED = 3;
            QList<ErrorInformation> skipErrorItems;
            do {
                DO_DEBUG_PARSING_INFO( "Skipping"
                        << m_inputIterator->position() << m_inputIterator->input() );
    //             setError2( ErrorSevere, m_inputIterator->input(), SkipUnexpectedTokens,
    //                        m_inputIterator->position(), LexemList() << *m_inputIterator, parent );
                skipErrorItems << ErrorInformation( SkipUnexpectedTokens, m_inputIterator->position(),
                                                    *m_inputIterator );
                readItemAndSkipSpaces( matchData );
                if ( !isAtImaginaryIteratorEnd() &&
                     matchItemKleeneUnaware(matchData, matchedItems, CorrectNothing) )
                {
                    DO_DEBUG_PARSING_INFO( "Match after" << skipErrorItems.count()
                                           << "skipped lexem(s)" );
                    // Add the error items
                    foreach ( const ErrorInformation &error, skipErrorItems ) {
                        setError2( matchData, ErrorSevere, error.lexem.input(), error.correction,
                                   error.position, LexemList() << error.lexem );
                    }
                    if ( m_result > AcceptedWithErrors ) {
                        m_result = AcceptedWithErrors;
                    }
                    return true;
                }
            } while ( skipErrorItems.count() < MAX_SKIP_UNEXPECTED && !isAtImaginaryIteratorEnd() );

            DO_DEBUG_PARSING_INFO( "Skipping lexems did not help matching the input" );
            // m_inputIterator is not where it was before this if, it gets restored below
        }
    }

    DO_DEBUG_PARSING_INFO( "Failed" << (*matchData->item)->toString() << "at input:"
                 << (isAtImaginaryIteratorEnd() ? "END" : m_inputIterator->input()) );
    m_inputIterator = oldIterator;
    m_inputIteratorLookahead = m_inputIterator;
    updateLookahead();

    return false;
};

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

void SyntacticalAnalyzer::readSpaceItems( MatchData *matchData )
{
    DO_DEBUG_PARSING_FUNCTION_CALL2( readSpaceItems );

    while ( !isAtImaginaryIteratorEnd() && m_inputIterator->type() == Lexem::Space ) {
        if ( !matchData->onlyTesting ) {
            addOutputItem( matchData,
                           MatchItem(MatchItem::Error, LexemList() << *m_inputIterator,
                                     ErrorMessageValue, "Double space character") );
        }
        readItem();
    }
}

bool SyntacticalAnalyzer::matchCharacter( MatchData *matchData, MatchItems *matchedItems )
{
    const SyntaxCharacterItem *characterItem =
            qobject_cast< const SyntaxCharacterItem* >( matchData->item->data() );
    Q_ASSERT( characterItem );
    DO_DEBUG_PARSING_FUNCTION_CALL( matchCharacter, QChar(characterItem->character()) );

    if ( m_inputIterator->type() == Lexem::Character &&
         m_inputIterator->textIsCharacter(characterItem->character()) )
    {
        const QString text = m_inputIterator->input();
        #ifdef FILL_MATCHES_IN_SYNTAXITEM
            characterItem->addMatch( m_inputIterator->position(), text, text );
        #endif
        *matchedItems << MatchItem( MatchItem::Character, LexemList() << *m_inputIterator,
                                    characterItem->valueType(), text );
        addOutputItem( matchData, matchedItems->last() );
        readItemAndSkipSpaces( matchData );
        DO_DEBUG_PARSING_MATCH( Number, QChar(characterItem->character()) );
        return true;
    } else {
        return false;
    }
}

bool SyntacticalAnalyzer::matchWord( MatchData *matchData, MatchItems *matchedItems )
{
    const SyntaxWordItem *wordItem =
            qobject_cast< const SyntaxWordItem* >( matchData->item->data() );
    Q_ASSERT( wordItem );
    DO_DEBUG_PARSING_FUNCTION_CALL( matchWord, wordItem->wordTypes() );

    if ( !isAtImaginaryIteratorEnd() && wordItem->wordTypes().contains(m_inputIterator->type()) ) {
        const QString matchedWord = m_inputIterator->input();
        #ifdef FILL_MATCHES_IN_SYNTAXITEM
            wordsItem->addMatch( m_inputIterator->position(), matchedWord, matchedWord );
        #endif
        *matchedItems << MatchItem( MatchItem::Word, LexemList() << *m_inputIterator,
                                    wordItem->valueType(), matchedWord );
        addOutputItem( matchData, matchedItems->last() );
        readItemAndSkipSpaces( matchData );
        DO_DEBUG_PARSING_MATCH( Word, matchedWord );
        return true;
    } else {
        return false;
    }
}

bool SyntacticalAnalyzer::matchString( MatchData *matchData, MatchItems *matchedItems )
{
    const SyntaxStringItem *stringItem
            = qobject_cast< const SyntaxStringItem* >( matchData->item->data() );
    Q_ASSERT( stringItem );
    DO_DEBUG_PARSING_FUNCTION_CALL( matchString, stringItem->strings() );

    if ( m_inputIterator->type() == Lexem::String ) {
        foreach ( const QString &string, stringItem->strings() ) {
            if ( m_inputIterator->input().compare(string, Qt::CaseInsensitive) == 0 ) {
                const QString matchedString = m_inputIterator->input();
                #ifdef FILL_MATCHES_IN_SYNTAXITEM
                    stringItem->addMatch( m_inputIterator->position(), matchedString, matchedString );
                #endif
                *matchedItems << MatchItem( MatchItem::String, LexemList() << *m_inputIterator,
                                            (*matchData->item)->valueType(), matchedString );
                addOutputItem( matchData, matchedItems->last() );
                readItemAndSkipSpaces( matchData );
                DO_DEBUG_PARSING_MATCH( String, matchedString );
                return true;
            }
        }
        return false; // No string matched
    } else {
        return false; // Not a string at current input position
    }
}

bool SyntacticalAnalyzer::matchKeywordInList( MatchData *matchData, KeywordType type,
                                              MatchItem *matchedItem )
{
    DO_DEBUG_PARSING_FUNCTION_CALL( matchKeywordInList, type );

    const QStringList& keywordList = m_keywords->keywords( type );
    Q_ASSERT( !keywordList.isEmpty() );

#ifdef ALLOW_MULTIWORD_KEYWORDS
    // TODO TEST this
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
            readItemAndSkipSpaces( matchData );
        } while ( !isAtImaginaryIteratorEnd() );

        if ( word == wordCount ) {
            // All words of the keyword matched
//             SyntaxItem item( type, lexems, keyword );
//             addOutputItem( item, parent );
            *matchedItem = MatchItem( MatchItem::Keyword, lexems, NoValue, static_cast<int>(type) ); // TODO
            readItemAndSkipSpaces( matchData );
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
                *matchedItem = MatchItem( MatchItem::Keyword, lexems, NoValue, static_cast<int>(type) ); // TODO
                m_selectionLength = keyword.length() - m_inputIterator->input().length() -
                        QStringList(words.mid(0, index)).join(" ").length();
                readItemAndSkipSpaces( matchData );
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
                // Add output item for the partially read keyword (with capitalization of the input)
                *matchedItem = MatchItem( LexemList() << *m_inputIterator, type,
                        m_inputIterator->input() + keyword.mid(m_inputIterator->input().length()),
                        MatchItem::CorrectedMatchItem );
                m_selectionLength = keyword.length() - m_inputIterator->input().length();
                readItemAndSkipSpaces( matchData );
                return true;
            }
            continue;
        }

        // The keyword was matched
        *matchedItem = MatchItem( LexemList() << *m_inputIterator, type, keyword );
        readItemAndSkipSpaces( matchData );
        return true;
    }
#endif

    return false; // No keyword matched
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
        m_timeKeywordsInMinutes(i18nc("@info/plain A comma separated list of keywords which can "
            "follow after a relative time keyword.\nNote: Keywords should be unique for each "
            "meaning.", "minutes").split(',', QString::SkipEmptyParts)),
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

bool JourneySearchKeywords::hasValue( KeywordType type ) const
{
    switch ( type ) {
    case KeywordTimeIn:
    case KeywordTimeAt:
        return true; // The keywords listed above need a value

    case KeywordTo:
    case KeywordFrom:
    case KeywordTomorrow:
    case KeywordDeparture:
    case KeywordArrival:
    case KeywordTimeInMinutes: // This is special, because it can be part of the value sequence of KeywordTimeIn
        return false; // No value for the keywords listed above

    default:
        kDebug() << "Unknown keyword" << type;
        return false;
    }
}

const QStringList JourneySearchKeywords::keywords( KeywordType type ) const
{
    switch ( type ) {
    case KeywordTimeIn:
        return timeKeywordsIn();
    case KeywordTimeInMinutes:
        return timeKeywordsInMinutes();
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
