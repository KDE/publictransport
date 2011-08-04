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

const QString LexicalAnalyzer::ALLOWED_OTHER_CHARACTERS = ":,.´`'!?&()_";

JourneySearchAnalyzer::JourneySearchAnalyzer( JourneySearchKeywords *keywords,
                                              AnalyzerCorrectionLevel correctionLevel,
                                              int cursorPositionInInputString ) :
        m_keywords(keywords ? keywords : new JourneySearchKeywords()),
        m_ownKeywordsObject(!keywords),
        m_lexical(new LexicalAnalyzer(correctionLevel, cursorPositionInInputString)),
        m_syntactical(new SyntacticalAnalyzer(m_keywords, correctionLevel,
                                              cursorPositionInInputString)),
        m_contextual(new ContextualAnalyzer(correctionLevel, cursorPositionInInputString))
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
            const QLinkedList< SyntaxItem > &itemList, JourneySearchKeywords *keywords )
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
    for ( QLinkedList<SyntaxItem>::ConstIterator it = results.m_allItems.constBegin();
          it != results.m_allItems.constEnd(); ++it )
    {
        switch ( it->type() ) {
        case SyntaxItem::Error:
            results.m_hasErrors = true;
            results.m_errorItems << &*it;
            itemText = it->text();
            break;
        case SyntaxItem::StopName:
            results.m_stopName = it->text();
            results.m_syntaxItems.insert( SyntaxItem::StopName, &*it );
            itemText = QString("\"%1\"").arg(results.m_stopName);
            break;
        case SyntaxItem::KeywordTo:
            results.m_stopIsTarget = true;
            results.m_syntaxItems.insert( SyntaxItem::KeywordTo, &*it );
            itemText = it->text();
            break;
        case SyntaxItem::KeywordFrom:
            results.m_stopIsTarget = false;
            results.m_syntaxItems.insert( SyntaxItem::KeywordFrom, &*it );
            itemText = it->text();
            break;
        case SyntaxItem::KeywordTimeIn:
            results.m_time = QDateTime::currentDateTime().addSecs( 60 * it->value().toInt() );
            results.m_syntaxItems.insert( SyntaxItem::KeywordTimeIn, &*it );
            itemText = QString("%1 %2").arg(it->text())
                    .arg(keywords->relativeTimeString(it->value().toInt()));
            break;
        case SyntaxItem::KeywordTimeAt:
            results.m_time = QDateTime( QDate::currentDate(), it->value().toTime() );
            results.m_syntaxItems.insert( SyntaxItem::KeywordTimeAt, &*it );
            itemText = QString("%1 %2").arg(it->text()).arg(results.m_time.toString("hh:mm"));
            break;
        case SyntaxItem::KeywordTomorrow:
            tomorrow = true;
            results.m_syntaxItems.insert( SyntaxItem::KeywordTomorrow, &*it );
            itemText = it->text();
            break;
        case SyntaxItem::KeywordDeparture:
            results.m_timeIsDeparture = true;
            results.m_syntaxItems.insert( SyntaxItem::KeywordDeparture, &*it );
            itemText = it->text();
            break;
        case SyntaxItem::KeywordArrival:
            results.m_timeIsDeparture = false;
            results.m_syntaxItems.insert( SyntaxItem::KeywordArrival, &*it );
            itemText =  it->text();
            break;
        }

        if ( it->type() != SyntaxItem::Error ) {
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
        const QHash< SyntaxItem::Type, QVariant > &updateItemValues,
        const QList< SyntaxItem::Type > &removeItems,
        OutputStringFlags flags, JourneySearchKeywords *keywords ) const
{
    bool ownKeywords = !keywords;
    if ( ownKeywords ) {
        keywords = new JourneySearchKeywords();
    }

    // Insert dummy syntax items for updated item values without an associated syntax item
    // in m_allItems
    QLinkedList< SyntaxItem > itemList = m_allItems;
    for ( QHash<SyntaxItem::Type, QVariant>::ConstIterator itUpdate = updateItemValues.constBegin();
          itUpdate != updateItemValues.constEnd(); ++itUpdate )
    {
        if ( !m_syntaxItems.contains(itUpdate.key()) ) {
            // Current updated item not in m_allItems, insert a dummy syntax item
            switch ( itUpdate.key() ) {
            case SyntaxItem::StopName:
                // Insert after KeywordTo/KeywordFrom or prepend
                if ( itemList.isEmpty() ) {
                    // No items in the list, just prepend a dummy stop name item
                    itemList.prepend( SyntaxItem(SyntaxItem::StopName, QString(), -1) );
                    break;
                }

                for ( QLinkedList<SyntaxItem>::Iterator it = itemList.begin();
                      it != itemList.end(); ++it )
                {
                    if ( it->type() != SyntaxItem::KeywordTo ||
                         it->type() != SyntaxItem::KeywordFrom )
                    {
                        // First non-prefix item found, insert dummy stop name item before
                        it = itemList.insert( it, SyntaxItem(SyntaxItem::StopName, QString(), -1) );
                        break;
                    }
                }
                break;
            case SyntaxItem::KeywordTo:
            case SyntaxItem::KeywordFrom:
                itemList.prepend( SyntaxItem(itUpdate.key(), QString(), -1) );
                break;
            case SyntaxItem::KeywordTomorrow:
            case SyntaxItem::KeywordDeparture:
            case SyntaxItem::KeywordArrival:
            case SyntaxItem::KeywordTimeIn:
            case SyntaxItem::KeywordTimeAt:
                itemList.append( SyntaxItem(itUpdate.key(), QString(), -1) );
                break;
            case SyntaxItem::Error:
                kDebug() << "Won't insert/update error items";
                break;
            }
        }
    }

    QStringList output;
    QString itemText;
    for ( QLinkedList<SyntaxItem>::ConstIterator it = itemList.constBegin();
          it != itemList.constEnd(); ++it )
    {
        switch ( it->type() ) {
        case SyntaxItem::Error:
            if ( !flags.testFlag(ErrornousOutputString) ) {
                continue;
            }
            itemText = it->text();
            break;
        case SyntaxItem::StopName:
            if ( !removeItems.contains(it->type()) ) {
                if ( updateItemValues.contains(it->type()) ) {
                    itemText = QString("\"%1\"").arg( updateItemValues[it->type()].toString() );
                } else {
                    itemText = QString("\"%1\"").arg( it->text() );
                }
            }
            break;
        case SyntaxItem::KeywordTo:
        case SyntaxItem::KeywordFrom:
        case SyntaxItem::KeywordTomorrow:
        case SyntaxItem::KeywordDeparture:
        case SyntaxItem::KeywordArrival:
            if ( !removeItems.contains(it->type()) ) {
                if ( updateItemValues.contains(it->type()) ) {
                    // This replaces the keywords with other keyword translations or other strings
                    itemText = updateItemValues[it->type()].toString();
                } else {
                    itemText = it->text();
                }
            }
            break;
        case SyntaxItem::KeywordTimeIn:
            if ( !removeItems.contains(it->type()) ) {
                if ( updateItemValues.contains(it->type()) ) {
                    // This replaces the keyword values with new ones (the keyword "in" itself remains unchanged)
                    itemText = QString("%1 %2").arg(it->text())
                            .arg( keywords->relativeTimeString(updateItemValues[it->type()].toInt()) );
                } else {
                    itemText = QString("%1 %2").arg(it->text())
                            .arg( keywords->relativeTimeString(it->value().toInt()) );
                }
            }
            break;
        case SyntaxItem::KeywordTimeAt:
            if ( !removeItems.contains(it->type()) ) {
                if ( updateItemValues.contains(it->type()) ) {
                    // This replaces the keyword values with new ones (the keyword "at" itself remains unchanged)
                    itemText = QString("%1 %2").arg(it->text())
                            .arg( updateItemValues[it->type()].toTime().toString("hh:mm") );
                } else {
                    itemText = QString("%1 %2").arg(it->text())
                            .arg( it->value().toTime().toString("hh:mm") );
                }
            }
            break;
        }

        output << itemText;
    }
    if ( ownKeywords ) {
        delete keywords;
    }

    return output.join( " " );
}

const Results JourneySearchAnalyzer::analyze(
        const QString& input, AnalyzerCorrectionLevel correctionLevel )
{
    m_lexical->setCorrectionLevel( correctionLevel );
    m_syntactical->setCorrectionLevel( correctionLevel );
    m_contextual->setCorrectionLevel( correctionLevel );

//     TODO calculate cursor offset at end by summing up lengths of corrected items before the old cursor position
    QLinkedList<Lexem> lexems = m_lexical->analyze( input );
    m_syntactical->setCursorValues( m_lexical->cursorOffset(), m_lexical->selectionLength() );
    QLinkedList<SyntaxItem> syntaxItems = m_syntactical->analyze( lexems );
    m_contextual->setCursorValues( m_syntactical->cursorOffset(), m_syntactical->selectionLength() );
    m_results = resultsFromSyntaxItemList( m_contextual->analyze(syntaxItems) );
    m_results.m_cursorOffset = m_contextual->cursorOffset();
    m_results.m_selectionLength = m_contextual->selectionLength();
    m_results.m_inputString = input;
    m_results.m_result = m_contextual->result();
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
}

QString Results::outputString( OutputStringFlags flags ) const
{
    if ( flags.testFlag(ErrornousOutputString) ) {
        return m_outputStringWithErrors;
    } else {
        return m_outputString;
    }
}

template <typename Container>
Analyzer<Container>::Analyzer( AnalyzerCorrectionLevel correctionLevel,
                               int cursorPositionInInputString, int cursorOffset )
{
    m_state = NotStarted;
    m_result = Accepted;
    m_correctionLevel = correctionLevel;
    m_minRejectSeverity = ErrorFatal;
    m_minAcceptWithErrorsSeverity = ErrorSevere;
    m_cursorPositionInInputString = cursorPositionInInputString;
    m_cursorOffset = cursorOffset;
}

template <typename Container>
void Analyzer<Container>::setErrorHandling( ErrorSeverity minRejectSeverity,
                                            ErrorSeverity minAcceptWithErrorsSeverity )
{
    m_minRejectSeverity = minRejectSeverity;
    m_minAcceptWithErrorsSeverity = minAcceptWithErrorsSeverity;
}

template <typename Container>
void Analyzer<Container>::setError( ErrorSeverity severity, const QString& errornousText,
                                   const QString& errorMessage, int position, int index )
{
    Q_UNUSED( errornousText );
    if ( severity >= m_minRejectSeverity ) {
        m_result = Rejected;
        qDebug() << "Reject:" << errorMessage << position << index;
    } else if ( m_result < AcceptedWithErrors && severity >= m_minAcceptWithErrorsSeverity ) {
        m_result = AcceptedWithErrors;
        qDebug() << "Accept with errors:" << errorMessage << position << index;
    }
}

void SyntacticalAnalyzer::setError( ErrorSeverity severity, const QString& errornousText,
                                    const QString& errorMessage, int position, int index )
{
    Analyzer::setError( severity, errornousText, errorMessage, position, index );
    addOutputItem( SyntaxItem(SyntaxItem::Error, errornousText, position, errorMessage) );
}

void ContextualAnalyzer::setError( ErrorSeverity severity, const QString& errornousText,
                                   const QString& errorMessage, int position, int index )
{
    Analyzer::setError( severity, errornousText, errorMessage, position, index );
    m_input << SyntaxItem( SyntaxItem::Error, errornousText, position );
}

Lexem::Lexem()
{
    m_type = Error;
    m_pos = -1;
    m_followedBySpace = true;
}

Lexem::Lexem( Lexem::Type type, const QString& text, int pos, bool followedBySpace )
{
    m_type = type;
    m_text = text;
    m_pos = pos;
    m_followedBySpace = followedBySpace;
}

LexicalAnalyzer::LexicalAnalyzer( AnalyzerCorrectionLevel correction,
                                  int cursorPositionInInputString, int cursorOffset )
        : Analyzer(correction, cursorPositionInInputString, cursorOffset)
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
    if ( c == '"' ) {
        return QuotationMark;
    } else if ( c == ':' ) {
        return Colon;
    } else if ( c.isDigit() ) {
        return Digit;
    } else if ( c.isLetter() ) {
        return Letter;
    } else if ( c == ' ' ) {//.isSpace() ) {
        return Space;
    } else if ( ALLOWED_OTHER_CHARACTERS.contains(c) ) {
        return OtherSymbol;
    } else {
        return Invalid;
    }
}

QLinkedList<Lexem> LexicalAnalyzer::analyze( const QString& input )
{
    m_input = input;
    m_inputIterator = m_input.begin();
    m_inputIteratorLookahead = m_inputIterator + 1;
    m_state = Running;
    m_result = Accepted;
    m_output.clear();
    m_currentWord.clear();
    m_firstWordSymbol = Invalid;
    m_pos = 0;
    bool inQuotationMarks = false;
    while ( !isAtEnd() ) {
        Symbol symbol = symbolOf( *m_inputIterator );
        switch ( symbol ) {
        case QuotationMark:
            endCurrentWord();
            m_output << Lexem( Lexem::QuotationMark, "\"", m_pos, isSpaceFollowing() );
            inQuotationMarks = !inQuotationMarks;
            break;
        case Colon:
            endCurrentWord();
            m_output << Lexem( Lexem::Colon, ":", m_pos, isSpaceFollowing() );
            break;
        case Space:
            if ( m_inputIteratorLookahead != m_input.constEnd() &&
                 symbolOf(*(m_inputIteratorLookahead)) == Space )
            {
                // Don't allow two consecutive space characters
                // Skip this one, ie. read the next one
                readItem();
            }

            endCurrentWord( true );
            if ( m_pos == m_input.length() - 1 || m_pos == m_cursorPositionInInputString - 1 ) {
                m_output << Lexem( Lexem::Space, " ", m_pos, isSpaceFollowing() );
            }
            break;
        case Letter:
        case Digit:
        case OtherSymbol:
            if ( m_firstWordSymbol == Invalid ) {
                // At the beginning of a new word
                m_firstWordSymbol = symbol;
                m_wordStartPos = m_pos;
                m_currentWord.append( *m_inputIterator );
            } else if ( m_firstWordSymbol == symbol ) {
                // In the middle of a word
                m_currentWord.append( *m_inputIterator );
            } else if ( symbol == OtherSymbol || symbol == Letter || symbol == Digit ) {
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

SyntaxItem::SyntaxItem()
{
    m_type = Error;
    m_flags = DefaultSyntaxItem;
    m_pos = -1;
}

SyntaxItem::SyntaxItem( SyntaxItem::Type type, const QString& text, int pos, const QVariant& value,
                        SyntaxItem::Flags flags  )
{
    m_type = type;
    m_flags = flags;
    m_text = text;
    m_pos = pos;
    m_value = value;
}

QString SyntaxItem::typeName( SyntaxItem::Type type )
{
    switch ( type ) {
    case Error:
        return "error";
    case StopName:
        return "stop name";
    case KeywordTo:
        return "to";
    case KeywordFrom:
        return "from";
    case KeywordTimeIn:
        return "in";
    case KeywordTimeAt:
        return "at";
    case KeywordTomorrow:
        return "tomorrow";
    case KeywordDeparture:
        return "departure";
    case KeywordArrival:
        return "arrival";
    default:
        return "<unknown>";
    }
}

SyntacticalAnalyzer::SyntacticalAnalyzer( JourneySearchKeywords *keywords,
                                          AnalyzerCorrectionLevel correction,
                                          int cursorPositionInInputString, int cursorOffset )
        : AnalyzerRL(correction, cursorPositionInInputString, cursorOffset), m_keywords(keywords)
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

QLinkedList< SyntaxItem > SyntacticalAnalyzer::analyze( const QLinkedList<Lexem> &input )
{
    QStringList lexemString; foreach ( const Lexem &lexem, input ) {
            lexemString << QString("%1 (pos: %2, type: %3, space?: %4)").arg(lexem.text())
                .arg(lexem.position()).arg(lexem.type()).arg(lexem.isFollowedBySpace() ? "JA" : "nein"); }
    qDebug() << "Lexem List:" << lexemString.join(", ");

    m_output.clear();
    m_input = input;
    m_state = Running;
    m_result = Accepted;
    m_inputIterator = m_input.begin();
    m_inputIteratorLookahead = m_inputIterator + 1;
    readSpaceItems();

    parseJourneySearch();

    QStringList syntaxString; foreach ( const SyntaxItem &syntaxItem, m_output ) {
            syntaxString << QString("%1 (pos: %2, type: %3, value: %4)").arg(syntaxItem.text())
                .arg(syntaxItem.position()).arg(syntaxItem.type()).arg(syntaxItem.value().toString()); }
    qDebug() << "Syntax List:" << syntaxString.join(", ") << m_result;

    m_state = Finished;
    return m_output;
}

QLinkedList<SyntaxItem>::iterator SyntacticalAnalyzer::addOutputItem( const SyntaxItem& syntaxItem )
{
    // Sort the new syntaxItem into the output list, sorted by position
    if ( m_output.isEmpty() || m_output.last().position() < syntaxItem.position() ) {
        // Append the new item
        return m_output.insert( m_output.end(), syntaxItem );
    } else {
        QLinkedList<SyntaxItem>::iterator it = m_output.begin();
        while ( it->position() < syntaxItem.position() ) {
            ++it;
        }
        return m_output.insert( it, syntaxItem );
    }
}

bool SyntacticalAnalyzer::parseJourneySearch()
{
    if ( m_inputIterator == m_input.end() ) {
        setError( ErrorInformational, QString(), "No input", m_inputIterator->position() );
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
    m_inputIterator = m_input.begin();
    m_inputIteratorLookahead = m_inputIterator + 1;
    readSpaceItems();
    if ( m_inputIterator != m_input.constEnd() && m_inputIterator->type() == Lexem::String ) {
//         const QString text = m_inputIterator->text();

        bool matched = false;
        matched = matched || matchKeywordInList( SyntaxItem::KeywordTo, m_keywords->toKeywords() );
        matched = matched || matchKeywordInList( SyntaxItem::KeywordFrom, m_keywords->fromKeywords() );
        m_stopNameBegin = m_inputIterator;
        return matched;
    } else {
        m_stopNameBegin = m_inputIterator;
        return false;
    }
}

bool SyntacticalAnalyzer::matchNumber( int *number, int min, int max, int* removedDigits )
{
    // Match number
    QString numberString = m_inputIterator->text();
    if ( removedDigits ) {
        *removedDigits = 0; // Initialize
    }
    bool ok;
    if ( m_correctionLevel > CorrectNothing ) {
        if ( numberString.length() > 2 ) {
            if ( removedDigits ) {
                *removedDigits = numberString.length() - 2;
            }
            // TODO This only works with strings not longer than two characters
            if ( m_cursorPositionInInputString == m_inputIterator->position() + 1 ) {
                // Cursor is here (|): "X|XX:XX", overwrite second digit
                numberString.remove( 1, 1 );
            }
            numberString = numberString.left( 2 );
        }

        // Put the number into the given range [min, max]
        *number = qBound( min, numberString.toInt(&ok), max );
        if ( !ok ) {
            return false; // Number invalid (shouldn't happen, since hoursString only contains digits)
        }
    } else {
        *number = numberString.toInt( &ok );
        if ( !ok || *number < min || *number > max ) {
            return false; // Number out of range or invalid
        }
    }

    readItemAndSkipSpaces();
    return true;
}

bool SyntacticalAnalyzer::matchTimeAt()
{
    // Match "at 00:00" from back (RightToLeft) => starting with a number
    if ( m_inputIterator->type() != Lexem::Number ) {
        // Wrong ending.. But if the "at" keyword gets read here (with wrong following items)
        // it may be corrected by adding the time values (use current time)
        if ( m_correctionLevel > CorrectNothing && m_inputIterator->type() == Lexem::String &&
             m_keywords->timeKeywordsAt().contains(m_inputIterator->text(), Qt::CaseInsensitive) )
        {
            // Add output item for the read "at" keyword with corrected time value
            addOutputItem( SyntaxItem(SyntaxItem::KeywordTimeAt,
                    m_inputIterator->text(), m_inputIterator->position(),
                    QDateTime::currentDateTime(), SyntaxItem::CorrectedSyntaxItem) );

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

    if ( m_inputIterator->type() != Lexem::Colon ) {
        // Wrong ending.. But if the "at" keyword gets read here (with only a number following)
        // it may be corrected by using the number as hours value and adding the minutes
        if ( m_correctionLevel > CorrectNothing && m_inputIterator->type() == Lexem::String &&
             m_keywords->timeKeywordsAt().contains(m_inputIterator->text(), Qt::CaseInsensitive) )
        {
            // Add output item for the "at" keyword with corrected time value (minutes => hours)
            addOutputItem( SyntaxItem(SyntaxItem::KeywordTimeAt,
                    m_inputIterator->text(), m_inputIterator->position(),
                    QDateTime(QDate::currentDate(), QTime(minutes, 0)),
                    SyntaxItem::CorrectedSyntaxItem) );
            readItemAndSkipSpaces();
            return true;
        } else {
            m_inputIterator = oldIterator;
            m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
            return false; // TimeAt-rule not matched
        }
    }
    if ( isAtEnd() ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
        return false; // Only a number and a colon were read
    }
    int colonPosition = m_inputIterator->position();
    readItemAndSkipSpaces();

    if ( isAtEnd() || m_inputIterator->type() != Lexem::Number ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
        return false; // The TimeAt-rule can't be reduced here
    }

    // Match number (hours)
//     QString hoursString = m_inputIterator->text();
    int hours;
    int deletedDigits;
    if ( !matchNumber(&hours, 0, 23, &deletedDigits) ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
        return false;
    }

    if ( m_inputIterator->type() != Lexem::String ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
        return false; // Keyword not found
    }
    QLinkedList<SyntaxItem>::iterator match;
    if ( !matchKeywordInList(SyntaxItem::KeywordTimeAt, m_keywords->timeKeywordsAt(), &match) ) {
//     if ( !m_keywords->timeKeywordsAt().contains(m_inputIterator->text(), Qt::CaseInsensitive) ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
        return false; // Keyword not found
    }

    // Set value of the matched at keyword
    match->m_value = QDateTime( QDate::currentDate(), QTime(hours, minutes) );
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
    // AND needs at least 2 more symbols => m_pos >= 1
    if ( /*m_pos < 1 ||*/ !matchMinutesString() ) {
        return false; // The TimeIn-rule can't be reduced here, wrong ending lexem
    }

    // Don't need to check isAtEnd() here, because m_pos >= 1 was checked above
    readItemAndSkipSpaces();
    if ( m_inputIterator->type() != Lexem::Number ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
        return false; // TimeIn-rule not matched
    }
    // Parse number
    bool ok;
    int minutes = m_inputIterator->text().toInt( &ok );
    if ( !ok || minutes < 0 || minutes >= 1440 ) { // max. 1 day
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
        return false; // Number out of range
    }

    // Read keyword "in"
    readItemAndSkipSpaces();
    if ( m_inputIterator->type() != Lexem::String ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
        return false; // Keyword not found
    }
    const QString text = m_inputIterator->text();
    if ( !m_keywords->timeKeywordsIn().contains(text, Qt::CaseInsensitive) ) {
        m_inputIterator = oldIterator;
        m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
        return false; // Keyword not found
    }

    addOutputItem( SyntaxItem(SyntaxItem::KeywordTimeIn, text, m_inputIterator->position(), minutes) );
    readItemAndSkipSpaces();
    return true;
}

bool SyntacticalAnalyzer::matchMinutesString()
{
    if ( m_inputIterator->type() == Lexem::String ) {
//         TODO
        static QRegExp rx( "mins?\\.?|minutes?", Qt::CaseInsensitive );
        return rx.exactMatch( m_inputIterator->text() ); // Takes too long?
    } else {
        return false;
    }
}

bool SyntacticalAnalyzer::matchKeywordInList( SyntaxItem::Type type, const QStringList& keywordList,
                                              QLinkedList<SyntaxItem>::iterator *match )
{
    Q_ASSERT( !keywordList.isEmpty() );

    int startPosition = m_inputIterator->position();
    if ( match ) {
        *match = m_output.end(); // Initialize
    }
    foreach ( const QString &keyword, keywordList ) {
        // Keywords themselves may consist of multiple words (ie. contain whitespaces)
        const QStringList words = keyword.split( QLatin1Char(' '), QString::SkipEmptyParts );
        const int wordCount = words.count();
        int word = 0;
        int index;
        do {
            index = m_direction == RightToLeft ? wordCount - 1 - word : word;
            if ( m_inputIterator->type() != Lexem::String
                || words[index].compare(m_inputIterator->text(), Qt::CaseInsensitive) != 0 )
            {
                break; // Didn't match keyword
            }

            ++word;
            if ( word >= wordCount ||
                 (m_direction == RightToLeft ? index <= 0 : index >= m_input.count() - 1) )
            {
                break; // End of word list reached
            }
            readItemAndSkipSpaces();
        } while ( m_inputIterator != m_input.constEnd() );

        if ( word == wordCount ) {
            // All words of the keyword matched
            QLinkedList<SyntaxItem>::iterator it =
                    addOutputItem( SyntaxItem(type, keyword, startPosition) );
            if ( match ) {
                *match = it;
            }
            readItemAndSkipSpaces();
            return true;
        } else {
            // Test if the beginning of a keyword matches.
            // If a cursor position is given, only complete the keyword, if the cursor is at the 
            // end of the so far typed keyword.
            // This needs to also match with one single character for matchPrefix, because
            // otherwise eg. a single typed "t" would get matched as stop name and get quotation
            // marks around it, making it hard to type "to"..
            if ( m_inputIterator != m_input.constEnd() && m_correctionLevel > CorrectNothing &&
                 (m_cursorPositionInInputString == -1 ||
                  m_cursorPositionInInputString == m_inputIterator->position() + m_inputIterator->text().length()) &&
                 (word > 0 || words[index].startsWith(m_inputIterator->text(), Qt::CaseInsensitive)) )
            {
                // Add output item for the partially read keyword
                QLinkedList<SyntaxItem>::iterator it =
                        addOutputItem( SyntaxItem(type, keyword, startPosition) );
                if ( match ) {
                    *match = it;
                }
                m_selectionLength = keyword.length() - m_inputIterator->text().length() -
                        QStringList(words.mid(0, index)).join(" ").length(); // TEST
                readItemAndSkipSpaces();
                return true;
            }

            if ( word > 0 ) {
                // Not all words of the keyword matched (but at least one)
                // Restore position
                m_inputIterator -= m_direction == RightToLeft ? -word : word;
                m_inputIteratorLookahead = m_inputIterator + (m_direction == LeftToRight ? 1 : -1);
            }
        }
    }

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

    m_inputIterator = m_input.constEnd();
    m_inputIteratorLookahead = m_inputIterator - 1;
    readSpaceItems();
    m_direction = RightToLeft;
    m_stopNameEnd = m_inputIterator;
    int matches = 0;
    bool matched = false;
    while ( !isAtEnd() && m_inputIteratorLookahead != m_stopNameBegin ) {
        if ( !matched ) {
            readItemAndSkipSpaces();
        }
        matched = false;

        if ( m_inputIterator->type() == Lexem::QuotationMark ) {
            // Stop if a quotation mark is read (end of the stop name)
            break;
        } else if ( m_inputIterator->type() == Lexem::Space ) {
            // Add spaces to the output as error items
            setError( ErrorMinor, m_inputIterator->text(),
                      "Space character at the end of the input", m_inputIterator->position() );
            continue;
        }
        matched = matchTimeAt();
        matched = matched || matchTimeIn();
        matched = matched || matchKeywordInList( SyntaxItem::KeywordTomorrow,
                                                 m_keywords->timeKeywordsTomorrow() );
        matched = matched || matchKeywordInList( SyntaxItem::KeywordDeparture,
                                                 m_keywords->departureKeywords() );
        matched = matched || matchKeywordInList( SyntaxItem::KeywordArrival,
                                                 m_keywords->arrivalKeywords() );
        if ( matched ) {
            ++matches;
        }
    }

    m_direction = LeftToRight;
    m_inputIteratorLookahead = m_inputIterator + 1; // Update lookahead iterator to left-to-right
    if ( matches > 0 ) {
        // Move to previous item (right-to-left mode)
        // eg. "[StopName] tomorrow" => "StopName [tomorrow]"
        m_stopNameEnd = m_inputIteratorLookahead;
        kDebug() << (m_stopNameEnd == m_input.constEnd() ? "stop name end: END" : ("stop name end: " + m_stopNameEnd->text()));
        return true;
    } else {
        kDebug() << "Suffix didn't match";
        return false;
    }
}

bool SyntacticalAnalyzer::matchStopName()
{
    m_inputIterator = m_stopNameBegin;
    m_inputIteratorLookahead = m_inputIterator + 1;
    readSpaceItems();
    if ( m_inputIterator == m_stopNameEnd ) {
        setError( ErrorFatal, QString(), "No stop name", m_inputIterator->position() );
        return false;
    }

    QStringList stopNameWords;
    int firstWordPos = -1;

    if ( m_inputIterator->type() == Lexem::QuotationMark ) {
        // The while loop ends when a quotation mark is found, or if there is no more input to read
        firstWordPos = m_inputIterator->position() + 1;
        readItem(); // m_inputIterator isn't at m_stopNameEnd, tested above
        while ( m_inputIterator != m_stopNameEnd ) {
            if ( m_inputIterator->type() == Lexem::QuotationMark ) {
                break; // Closing quotation mark
            } else {
                stopNameWords << m_inputIterator->text();
            }
            readItem();
        }

        if ( m_inputIterator == m_stopNameEnd || m_inputIterator->type() != Lexem::QuotationMark ) {
            setError( ErrorSevere, QString(), "No closing quotation mark",
                      m_inputIterator->position() );
//             TODO Error handling: Add a closing quotation mark
        } else {
//             ++m_inputIterator;
            readItem();
        }
    } else {
        firstWordPos = m_inputIterator->position();  // m_inputIterator isn't at m_stopNameEnd, tested above
        do {
            stopNameWords << m_inputIterator->text();
            readItem();
        } while ( m_inputIterator != m_stopNameEnd );

        if ( m_cursorPositionInInputString >= firstWordPos && m_correctionLevel > CorrectNothing ) {
            ++m_cursorOffset; // Add an offset for the quotation marks that get inserted
        }
    }

    if ( stopNameWords.isEmpty() ) {
        setError( ErrorFatal, QString(), "No stop name", firstWordPos + 1 );
        return false;
    } else {
        addOutputItem( SyntaxItem(SyntaxItem::StopName, stopNameWords.join(" "), firstWordPos) );

        if ( m_inputIterator != m_stopNameEnd && m_inputIterator != m_input.constEnd() ) {
            // Remaining lexems in m_input
            // Put their texts into an error item (eg. a Lexem::Space)
            int position = m_inputIterator->position();
            QString errornousText;
            for ( ; m_inputIterator != m_stopNameEnd; ++m_inputIterator ) {
                if ( m_inputIterator->isFollowedBySpace() ) {
                    errornousText.append( ' ' + m_inputIterator->text() );
                } else {
                    errornousText.append( m_inputIterator->text() );
                }
            }
            m_inputIteratorLookahead = m_inputIterator + 1;
            setError( ErrorSevere, errornousText.trimmed(), "Unknown elements remain unparsed",
                      position );
        }
    }

    return true;
}

ContextualAnalyzer::ContextualAnalyzer( AnalyzerCorrectionLevel correction,
                                        int cursorPositionInInputString, int cursorOffset )
        : Analyzer(correction, cursorPositionInInputString, cursorOffset)
{
}

QLinkedList< SyntaxItem > ContextualAnalyzer::analyze( const QLinkedList< SyntaxItem >& input )
{
    m_input = input;
    m_inputIterator = m_input.begin();
    m_inputIteratorLookahead = m_inputIterator + 1;
    m_state = Running;
    m_result = Accepted;
    QList< QSet<SyntaxItem::Type> > disjointKeywords;
    QHash< SyntaxItem::Type, QSet<SyntaxItem::Type> > notAllowedAfter;

    // Type isn't allowed after one of the set of other types
    const QSet<SyntaxItem::Type> timeTypes = QSet<SyntaxItem::Type>()
            << SyntaxItem::KeywordTimeAt << SyntaxItem::KeywordTimeIn
            << SyntaxItem::KeywordTomorrow;
    notAllowedAfter.insert( SyntaxItem::KeywordDeparture, timeTypes );
    notAllowedAfter.insert( SyntaxItem::KeywordArrival, timeTypes );

    // Sets of keywords that should be used at the same time
    disjointKeywords << (QSet<SyntaxItem::Type>()
            << SyntaxItem::KeywordArrival << SyntaxItem::KeywordDeparture);
    disjointKeywords << (QSet<SyntaxItem::Type>()
            << SyntaxItem::KeywordTimeAt << SyntaxItem::KeywordTimeIn);
    disjointKeywords << (QSet<SyntaxItem::Type>()
            << SyntaxItem::KeywordTo << SyntaxItem::KeywordFrom);

    // TODO apply "tomorrow" to time somewhere?

    QHash< SyntaxItem::Type, QLinkedList<SyntaxItem>::const_iterator > usedKeywords;
    while ( !isAtEnd() ) {
        readItem();

        if ( usedKeywords.contains(m_inputIterator->type()) ) {
            // Keyword found twice, replace with an error item
            setError( ErrorSevere, QString(),
                      QString("Keyword '%1' used twice").arg(m_inputIterator->text()),
                      m_inputIterator->position() );
//             TODO remove the double keyword (=> replace, not just add an error item), set text of the second keyword to the error item
        } else if ( notAllowedAfter.contains(m_inputIterator->type()) ) {
            QSet<SyntaxItem::Type> intersection =
                    notAllowedAfter[ m_inputIterator->type() ] & usedKeywords.keys().toSet();
            if ( !intersection.isEmpty() ) {
                // Keyword found after another keyword, but is illegal there, replace with an error item
                QStringList intersectingKeywords;
                foreach ( SyntaxItem::Type type, intersection ) {
                    intersectingKeywords << SyntaxItem::typeName(type);
                }
                setError( ErrorSevere, QString(),
                          QString("Keyword '%1' not allowed after keyword(s) '%2'")
                          .arg(m_inputIterator->text()).arg(intersectingKeywords.join("', '")),
                          m_inputIterator->position() );
//             TODO move the keyword to the correct location
            }
        }

        // Insert used keyword into usedKeywords, but not if it's an error item
        // (that would result in an infinite loop here, because error items get added here)
        if ( m_inputIterator->type() != SyntaxItem::Error ) {
            usedKeywords.insert( m_inputIterator->type(), m_inputIterator );
        }
    }

    // Check for keywords that shouldn't be used together
    // Error handling: Remove second keyword
    foreach ( const QSet<SyntaxItem::Type> &disjoint, disjointKeywords ) {
        QList<SyntaxItem::Type> intersection = (disjoint & usedKeywords.keys().toSet()).values();
        while ( intersection.count() > 1 ) {
            // More than one keyword of the current set of disjoint keywords is used
            QLinkedList<SyntaxItem>::const_iterator keywordItemToRemove =
                    usedKeywords[ intersection.takeFirst() ];
            QStringList intersectingKeywords;
            foreach ( SyntaxItem::Type type, intersection ) {
                if ( type != keywordItemToRemove->type() ) {
                    intersectingKeywords << SyntaxItem::typeName(type);
                }
            }
            setError( ErrorSevere, QString(),
                      QString("Keyword '%1' can't be used together with '%2'")
                      .arg(keywordItemToRemove->text()).arg(intersectingKeywords.join("', '")),
                      keywordItemToRemove->position() );

//             TODO remove the second keyword (=> replace, not just add an error item), set text of the second keyword to the error item
        }
    }

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
    int stopNamePosStart = results.syntaxItem( SyntaxItem::StopName )->position();
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

QDebug &operator <<( QDebug debug, ErrorSeverity error ) {
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

QDebug &operator <<( QDebug debug, AnalyzerState state ) {
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

QDebug &operator <<( QDebug debug, AnalyzerResult state ) {
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

QDebug &operator <<( QDebug debug, Lexem::Type type ) {
    switch ( type ) {
    case Lexem::Error:
        return debug << "Invalid";
    case Lexem::Number:
        return debug << "Number";
    case Lexem::QuotationMark:
        return debug << "QuotationMark";
    case Lexem::Colon:
        return debug << "Colon";
    case Lexem::Space:
        return debug << "Space";
    case Lexem::String:
        return debug << "String";
    default:
        return debug << static_cast<int>(type);
    }
};

QDebug &operator <<( QDebug debug, SyntaxItem::Type type ) {
    switch ( type ) {
    case SyntaxItem::Error:
        return debug << "Error";
    case SyntaxItem::StopName:
        return debug << "StopName";
    case SyntaxItem::KeywordTo:
        return debug << "KeywordTo";
    case SyntaxItem::KeywordFrom:
        return debug << "KeywordFrom";
    case SyntaxItem::KeywordTimeIn:
        return debug << "KeywordTimeIn";
    case SyntaxItem::KeywordTimeAt:
        return debug << "KeywordTimeAt";
    case SyntaxItem::KeywordTomorrow:
        return debug << "KeywordTomorrow";
    case SyntaxItem::KeywordDeparture:
        return debug << "KeywordDeparture";
    case SyntaxItem::KeywordArrival:
        return debug << "KeywordArrival";
    default:
        return debug << static_cast<int>(type);
    }
};

bool operator==( const SyntaxItem& l, const SyntaxItem& r )
{
    // There can only be one item at a given position, so it's enough to check SyntaxItem::pos()
    return l.position() == r.position();
}
