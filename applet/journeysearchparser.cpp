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

JourneySearchAnalyzer::Results JourneySearchAnalyzer::resultsFromSyntaxItemList(
            const QLinkedList< SyntaxItem >& itemList, JourneySearchKeywords *keywords )
{
    bool ownKeywords = !keywords;
    if ( ownKeywords ) {
        keywords = new JourneySearchKeywords();
    }

    Results results;
    results.allItems = itemList;

    bool tomorrow = false;
    QStringList output, outputWithErrors;
    QString itemText;
    for ( QLinkedList<SyntaxItem>::ConstIterator it = itemList.begin();
          it != itemList.end(); ++it )
    {
        switch ( it->type() ) {
        case SyntaxItem::Error:
            results.hasErrors = true;
            results.errorItems << *it;
            itemText = it->text();
            break;
        case SyntaxItem::StopName:
            results.stopName = it->text();
            results.syntaxItems.insert( SyntaxItem::StopName, *it );
            itemText = QString("\"%1\"").arg(results.stopName);
            break;
        case SyntaxItem::KeywordTo:
            results.stopIsTarget = true;
            results.syntaxItems.insert( SyntaxItem::KeywordTo, *it );
            itemText = it->text();
            break;
        case SyntaxItem::KeywordFrom:
            results.stopIsTarget = false;
            results.syntaxItems.insert( SyntaxItem::KeywordFrom, *it );
            itemText = it->text();
            break;
        case SyntaxItem::KeywordTimeIn:
            results.time = QDateTime::currentDateTime().addSecs( 60 * it->value().toInt() );
            results.syntaxItems.insert( SyntaxItem::KeywordTimeIn, *it );
            itemText = QString("%1 %2").arg(it->text())
                    .arg(keywords->relativeTimeString(it->value().toInt()));
            break;
        case SyntaxItem::KeywordTimeAt:
            results.time = QDateTime( QDate::currentDate(), it->value().toTime() );
            results.syntaxItems.insert( SyntaxItem::KeywordTimeAt, *it );
            itemText = QString("%1 %2").arg(it->text()).arg(results.time.toString("hh:mm"));
            break;
        case SyntaxItem::KeywordTomorrow:
            tomorrow = true;
            results.syntaxItems.insert( SyntaxItem::KeywordTomorrow, *it );
            itemText = it->text();
            break;
        case SyntaxItem::KeywordDeparture:
            results.timeIsDeparture = true;
            results.syntaxItems.insert( SyntaxItem::KeywordDeparture, *it );
            itemText = it->text();
            break;
        case SyntaxItem::KeywordArrival:
            results.timeIsDeparture = false;
            results.syntaxItems.insert( SyntaxItem::KeywordArrival, *it );
            itemText =  it->text();
            break;
        }

        if ( it->type() != SyntaxItem::Error ) {
            output << itemText;
        }
        outputWithErrors << itemText;
    }
    if ( !results.time.isValid() ) {
        // No time given in input string
        results.time = QDateTime::currentDateTime();
    }
    if ( tomorrow ) {
        results.time = results.time.addDays( 1 );
    }
    results.outputString = output.join( " " );
    results.outputStringWithErrors = outputWithErrors.join( " " );
    if ( ownKeywords ) {
        delete keywords;
    }

    return results;
}

JourneySearchAnalyzer::Results JourneySearchAnalyzer::analyze(
        const QString& input, AnalyzerCorrectionLevel correctionLevel )
{
    m_lexical->setCorrectionLevel( correctionLevel );
    m_syntactical->setCorrectionLevel( correctionLevel );
    m_contextual->setCorrectionLevel( correctionLevel );

    QLinkedList<Lexem> lexems = m_lexical->analyze( input );
    m_syntactical->setCursorOffset( m_lexical->cursorOffset() );
    QLinkedList<SyntaxItem> syntaxItems = m_syntactical->analyze( lexems );
    m_contextual->setCursorOffset( m_syntactical->cursorOffset() );
    Results results = resultsFromSyntaxItemList( m_contextual->analyze(syntaxItems), m_keywords );
    results.cursorOffset = m_contextual->cursorOffset();
    return results;
//     return resultsFromSyntaxItemList(
//             m_contextual->analyze(m_syntactical->analyze(m_lexical->analyze(input))),
//             m_keywords );
}

void JourneySearchAnalyzer::Results::init()
{
    stopName.clear();
    time = QDateTime();
    stopIsTarget = true;
    timeIsDeparture = true;
    hasErrors = false;
    syntaxItems.clear();
    cursorOffset = 0;
}

template <typename Container>
Analyzer<Container>::Analyzer( AnalyzerCorrectionLevel correctionLevel,
                               int cursorPositionInInputString, int cursorOffset )
{
    m_state = NotStarted;
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
}

Lexem::Lexem( Lexem::Type type, const QString& text, int pos )
{
    m_type = type;
    m_text = text;
    m_pos = pos;
}

LexicalAnalyzer::LexicalAnalyzer( AnalyzerCorrectionLevel correction,
                                  int cursorPositionInInputString, int cursorOffset )
        : Analyzer(correction, cursorPositionInInputString, cursorOffset)
{
    m_pos = -1;
}

void LexicalAnalyzer::endCurrentWord()
{
    if ( !m_currentWord.isEmpty() ) {
        if ( m_firstWordSymbol == Digit ) {
            m_output << Lexem( Lexem::Number, m_currentWord, m_wordStartPos );
        } else if ( m_firstWordSymbol == Letter ||  m_firstWordSymbol == OtherSymbol ) {
            m_output << Lexem( Lexem::String, m_currentWord, m_wordStartPos );
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
            m_output << Lexem( Lexem::QuotationMark, "\"", m_pos );
            inQuotationMarks = !inQuotationMarks;
            break;
        case Colon:
            endCurrentWord();
            m_output << Lexem( Lexem::Colon, ":", m_pos );
            break;
        case Space:
            endCurrentWord();
            if ( m_pos == m_input.length() - 1 ) { // TODO Also add a space lexem at the specified cursor position (also TODO)
                m_output << Lexem( Lexem::Space, " ", m_pos );
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
            m_output << Lexem( Lexem::Error, *m_inputIterator, m_pos ); // Not allowed character
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
            lexemString << QString("%1 (pos: %2, type: %3)").arg(lexem.text())
                .arg(lexem.position()).arg(lexem.type()); }
    qDebug() << "Lexem List:" << lexemString.join(", ");

    m_output.clear();
    m_input = input;
    m_inputIterator = m_input.begin();
    m_state = Running;
    m_result = Accepted;

    parseJourneySearch();

    QStringList syntaxString; foreach ( const SyntaxItem &syntaxItem, m_output ) {
            syntaxString << QString("%1 (pos: %2, type: %3, value: %4)").arg(syntaxItem.text())
                .arg(syntaxItem.position()).arg(syntaxItem.type()).arg(syntaxItem.value().toString()); }
    qDebug() << "Syntax List:" << syntaxString.join(", ") << m_result;

    m_state = Finished;
    return m_output;
}

void SyntacticalAnalyzer::addOutputItem( const SyntaxItem& syntaxItem )
{
    // Sort the new syntaxItem into the output list, sorted by position
    if ( m_output.isEmpty() || m_output.last().position() < syntaxItem.position() ) {
        m_output << syntaxItem;
    } else {
        QLinkedList<SyntaxItem>::iterator it = m_output.begin();
        while ( it->position() < syntaxItem.position() ) {
            ++it;
        }
        m_output.insert( it, syntaxItem );
    }
}

bool SyntacticalAnalyzer::parseJourneySearch()
{
    if ( m_inputIterator == m_input.end() ) {
        setError( ErrorInformational, QString(), "No input", m_inputIterator->position() );
        return false;
    }

    if ( !parsePrefix() ) {
        return false;
    }

    if ( !parseSuffix() ) {
        return false;
    }

    if ( !parseStopName() ) {
        return false;
    }

    return true;
}

bool SyntacticalAnalyzer::parsePrefix()
{
    // Match keywords at the beginning of the input
    // Also match, if only the beginning of a keyword is found (for interactive typing)
    // Otherwise the first typed character would get matched as stop name and get quotation
    // marks around it, making it hard to type the journey search string.
    m_inputIterator = m_input.begin();
    if ( m_inputIterator->type() == Lexem::String ) {
        const QString text = m_inputIterator->text();
        if ( m_keywords->toKeywords().contains(text, Qt::CaseInsensitive) ) {
            addOutputItem( SyntaxItem(SyntaxItem::KeywordTo, text, m_inputIterator->position()) );
            m_stopNameBegin = m_inputIterator + 1;
        } else if ( m_keywords->fromKeywords().contains(text, Qt::CaseInsensitive) ) {
            addOutputItem( SyntaxItem(SyntaxItem::KeywordFrom, text, m_inputIterator->position()) );
            m_stopNameBegin = m_inputIterator + 1;
        } else if ( m_correctionLevel > CorrectNothing ) {
//             TODO only if there are no words following?
            // Test if the beginning of a keyword is matched
            foreach ( const QString &keyword, m_keywords->toKeywords() ) {
                if ( keyword.startsWith(text, Qt::CaseInsensitive) ) {
                    // Found a keyword that starts with the input string
                    addOutputItem( SyntaxItem(SyntaxItem::KeywordTo, text,
                                              m_inputIterator->position()) );
                    m_stopNameBegin = m_inputIterator + 1;
                    return true;
                }
            }
            foreach ( const QString &keyword, m_keywords->fromKeywords() ) {
                if ( keyword.startsWith(text, Qt::CaseInsensitive) ) {
                    // Found a keyword that starts with the input string
                    addOutputItem( SyntaxItem(SyntaxItem::KeywordFrom, text,
                                              m_inputIterator->position()) );
                    m_stopNameBegin = m_inputIterator + 1;
                    return true;
                }
            }

            // No keyword beginning matched
            m_stopNameBegin = m_inputIterator;
        } else {
            // No complete keyword matched
            m_stopNameBegin = m_inputIterator;
        }
    } else {
        m_stopNameBegin = m_inputIterator;
    }
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
            ++m_cursorOffset; // Move one character to the beginning of the inserted time "XX:XX"
            if ( !isAtEnd() ) {
                readItem();
            }
            return true;
        } else {
            return false;
        }
    }
    if ( isAtEnd() ) {
        return false; // Only a number was read
    }

    // Parse number (minutes)
    bool ok;
    int minutes = m_inputIterator->text().toInt( &ok );
    if ( !ok || minutes < 0 || minutes >= 60 ) {
        ++m_inputIterator; // Restore position
        return false; // Number out of range
    }

    readItem(); // isAtEnd was checked above, before parsing the number
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
            if ( !isAtEnd() ) {
                readItem();
            }
            return true;
        } else {
            ++m_inputIterator; // Restore position
            return false; // TimeAt-rule not matched
        }
    }
    if ( isAtEnd() ) {
        return false; // Only a number and a colon were read
    }

    readItem();
    if ( /*m_pos < 2 ||*/ m_inputIterator->type() != Lexem::Number ) {
        m_inputIterator += 2; // Restore position
        return false; // The TimeAt-rule can't be reduced here
    }
    if ( isAtEnd() ) {
        return false; // Only the time value was read (Number, Colon, Number)
    }

    // Parse number (hours)
    int hours = m_inputIterator->text().toInt( &ok );
    if ( !ok || hours < 0 || hours >= 24 ) {
        return false; // Number out of range
    }

    // Read keyword "at", isAtEnd was checked above, before parsing the number
    readItem();
    if ( m_inputIterator->type() != Lexem::String ) {
        m_inputIterator += 3; // Restore position
        return false; // Keyword not found
    }
    if ( !m_keywords->timeKeywordsAt().contains(m_inputIterator->text(), Qt::CaseInsensitive) ) {
        m_inputIterator += 3; // Restore position
        return false; // Keyword not found
    }

    addOutputItem( SyntaxItem(SyntaxItem::KeywordTimeAt,
                              m_inputIterator->text(), m_inputIterator->position(),
                              QDateTime(QDate::currentDate(), QTime(hours, minutes))) );
    if ( !isAtEnd() ) {
        readItem();
    }
    return true;
}

bool SyntacticalAnalyzer::matchTimeIn()
{
    // "in X minutes" from back => starting with "minutes"
    // AND needs at least 2 more symbols => m_pos >= 1
    if ( /*m_pos < 1 ||*/ !matchMinutesString() ) {
        return false; // The TimeIn-rule can't be reduced here, wrong ending lexem
    }

    // Don't need to check isAtEnd() here, because m_pos >= 1 was checked above
    readItem();
    if ( m_inputIterator->type() != Lexem::Number ) {
        ++m_inputIterator; // Restore position
        return false; // TimeIn-rule not matched
    }
    // Parse number
    bool ok;
    int minutes = m_inputIterator->text().toInt( &ok );
    if ( !ok || minutes < 0 || minutes >= 1440 ) { // max. 1 day
        ++m_inputIterator; // Restore position
        return false; // Number out of range
    }

    // Read keyword "in"
    readItem();
    if ( m_inputIterator->type() != Lexem::String ) {
        m_inputIterator += 2; // Restore position
        return false; // Keyword not found
    }
    const QString text = m_inputIterator->text();
    if ( !m_keywords->timeKeywordsIn().contains(text, Qt::CaseInsensitive) ) {
        m_inputIterator += 2; // Restore position
        return false; // Keyword not found
    }

    addOutputItem( SyntaxItem(SyntaxItem::KeywordTimeIn, text, m_inputIterator->position(), minutes) );
    readItem();
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

bool SyntacticalAnalyzer::matchKeywordInList( SyntaxItem::Type type, const QStringList& keywordList )
{
    Q_ASSERT( !keywordList.isEmpty() );

    foreach ( const QString &keyword, keywordList ) {
        // Keywords themselves may consist of multiple words (ie. contain whitespaces)
        const QStringList words = keyword.split( QLatin1Char(' '), QString::SkipEmptyParts );
        const int wordCount = words.count();
        int word = 0;
        forever {
            const int index = m_direction == RightToLeft ? wordCount - 1 - word : word;
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
            readItem();
        }

        if ( word == wordCount ) {
            // All words of the keyword matched
            addOutputItem( SyntaxItem(type, m_inputIterator->text(), m_inputIterator->position()) );
            readItem();
            return true;
        } else if ( word > 0 ) {
            // Not all words of the keyword matched (but at least one)
            // Restore position
            m_inputIterator -= m_direction == RightToLeft ? -word : word;
        }
    }

    return false; // No keyword matched
}

bool SyntacticalAnalyzer::parseSuffix()
{
//     TODO Parse dates, other time formats?
    m_inputIterator = m_input.end();
    m_stopNameEnd = m_inputIterator;
    m_direction = RightToLeft;
    bool matched = false;
    while ( !isAtEnd() ) {
        if ( !matched ) {
            readItem();
        } else {
            m_stopNameEnd = m_inputIterator + 1;
        }
        matched = false;

        if ( m_inputIterator->type() == Lexem::QuotationMark ) {
            // Stop if a quotation mark is read (end of the stop name)
            break;
        }
        matched = matchTimeAt();
        matched = matched || matchTimeIn();
        matched = matched || matchKeywordInList( SyntaxItem::KeywordTomorrow,
                                                 m_keywords->timeKeywordsTomorrow() );
        matched = matched || matchKeywordInList( SyntaxItem::KeywordDeparture,
                                                 m_keywords->departureKeywords() );
        matched = matched || matchKeywordInList( SyntaxItem::KeywordArrival,
                                                 m_keywords->arrivalKeywords() );
    }
    m_direction = LeftToRight;
    return true;
}

bool SyntacticalAnalyzer::parseStopName()
{
    m_inputIterator = m_stopNameBegin;
    if ( m_inputIterator == m_stopNameEnd ) {
        setError( ErrorFatal, QString(), "No stop name", m_inputIterator->position() );
        return false;
    }

    QStringList stopNameWords;
    int firstWordPos = -1;
    int quotationMarks = 0;

    if ( m_inputIterator->type() == Lexem::QuotationMark ) {
        // This loop ends when a quotation mark is found, or if there is no more input to read
        firstWordPos = m_inputIterator->position() + 1;
        ++quotationMarks;
        if ( m_inputIterator != m_stopNameEnd ) {
            readItem();
            while ( m_inputIterator != m_stopNameEnd ) {
                if ( m_inputIterator->type() == Lexem::QuotationMark ) {
                    ++quotationMarks;
                    break; // Closing quotation mark
                } else {
                    stopNameWords << m_inputIterator->text();
                }
                readItem();
            }
        }

        if ( m_inputIterator->type() != Lexem::QuotationMark ) {
            setError( ErrorSevere, QString(), "No closing quotation mark",
                      m_inputIterator->position() );
//             TODO Error handling: Add a closing quotation mark?
        } else {
            ++m_inputIterator;
        }
    } else {
        if ( m_inputIterator->type() == Lexem::Space ) {
            readItem();
        }

        if ( m_inputIterator != m_stopNameEnd ) {
            firstWordPos = m_inputIterator->position();
            do {
                stopNameWords << m_inputIterator->text();
                readItem();
            } while ( m_inputIterator != m_stopNameEnd );

            if ( m_cursorPositionInInputString >= firstWordPos &&
                 m_correctionLevel > CorrectNothing )
            {
                ++m_cursorOffset; // Add an offset for the quotation marks that get inserted
            }
        } else {
            firstWordPos = m_inputIterator->position() + 1;
        }
    }

    if ( stopNameWords.isEmpty() ) {
        setError( ErrorFatal, QString(), "No stop name (but empty quotation marks)",
                  firstWordPos + 1 );
        return false;
    } else {
        addOutputItem( SyntaxItem(SyntaxItem::StopName, stopNameWords.join(" "), firstWordPos) );

        if ( m_inputIterator != m_stopNameEnd ) {
            // Remaining lexems in m_input
            int position = m_inputIterator->position();
            QStringList errornousText;
            for ( ; m_inputIterator != m_stopNameEnd; ++m_inputIterator ) {
                errornousText << m_inputIterator->text();
            }
            setError( ErrorSevere, errornousText.join(" "), "Unknown elements remain unparsed",
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

    JourneySearchAnalyzer::Results results = analyze( lineEdit->text() );
    int stopNamePosStart = results.syntaxItems[ SyntaxItem::StopName ].position();
    int stopNameLen = results.stopName.length(); // TODO may be wrong if the input contains double spaces "  "
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
