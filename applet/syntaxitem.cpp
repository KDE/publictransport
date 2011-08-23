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

#include "syntaxitem.h"

#include "matchitem.h"
#include <QVariant>
#include <QMetaEnum>

#include "journeysearchenums.moc"

#define ENUM_NAME_STRING(namespace, enum) \
    namespace::staticMetaObject.enumerator(namespace::staticMetaObject.indexOfEnumerator(#enum)).name()
#define ENUM_STRING(namespace, enum, value) \
    namespace::staticMetaObject.enumerator(namespace::staticMetaObject.indexOfEnumerator(#enum)).valueToKey(value)
#define FLAGS_STRING(namespace, enum, value) \
    QString(namespace::staticMetaObject.enumerator(namespace::staticMetaObject.indexOfEnumerator(#enum)).valueToKeys(value))

namespace Parser {

typedef Syntax M; // A bit shorter syntax definition
SyntaxItemPointer Syntax::journeySearchSyntaxItem()
{
    // Define longer match parts here for better readability
    SyntaxSequencePointer matchDate = (M::number(1, 31)->outputTo(DateDayValue) + M::character('.') +
             M::number(1, 12)->outputTo(DateMonthValue) + M::character('.') +
             M::number(1970, 2999)->outputTo(DateYearValue)->optional())->outputTo(DateValue); // FIXME MemLeak: new SyntaxNumberItem
    SyntaxSequencePointer matchTimeAt = M::keyword( KeywordTimeAt,
           ( M::number(0, 23)->outputTo(TimeHourValue) +
            (M::character(':') + M::number(0, 59)->outputTo(TimeMinuteValue))->optional() +
            (M::character(',')->optional() + matchDate)->optional() )->outputTo(DateAndTimeValue) );
    SyntaxSequencePointer matchTimeIn =
             M::keyword( KeywordTimeIn, M::number(1, 1339)->outputTo(RelativeTimeValue) ); // FIXED MemLeak: new SyntaxNumberItem

    // Define the journey search syntax
    return (M::keywordTo() | M::keywordFrom())->optional() +
           (M::character('"') + M::words()->outputTo(StopNameValue) + M::character('"')) +
           (M::keywordDeparture() | M::keywordArrival())->optional() +
            M::keywordTomorrow()->optional() +
           (matchTimeAt | matchTimeIn)->optional(); // FIXED MemLeak: SyntaxSequenceItem | SyntaxItem (new SyntaxOptionItem)

// FIXME This causes a crash:
//     SyntaxSequencePointer matchTimeIn =
//              M::keyword( KeywordTimeIn, M::number(1, 1339)->outputTo(RelativeTimeValue) ); // FIXED MemLeak: new SyntaxNumberItem
//     return matchTimeIn;
}

SyntaxItem::SyntaxItem( SyntaxItem::Type type, SyntaxItem::Flags flags,
                        JourneySearchValueType valueType, QObject* parent )
        : QObject(parent)
{
    m_type = type;
    m_flags = flags;
    m_valueType = valueType;
}

SyntaxItem::~SyntaxItem()
{
    #ifdef DEBUG_SYNTAXITEM_PARENTS
        qDebug() << QString("*** Deleting %1 ***:  %2")
                .arg(reinterpret_cast<long>(this), 0, 16).arg(toString());
    #endif
}

#ifdef USE_MATCHES_IN_SYNTAXITEM
void SyntaxItem::addMatch( const Parser::MatchItem& matchedItem )
{
    addMatch( matchedItem.position(), matchedItem.input(), matchedItem.value() );
}
#endif

QString SyntaxItem::toString( int ) const
{
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    return QString("%1 (%6, %2%3%4%5)")
            .arg( metaObject()->className() )
            .arg( FLAGS_STRING(SyntaxItem, Flag, m_flags) )
            .arg( m_valueType == Parser::NoValue ? QString()
                  : QString(", output -> %1").arg(ENUM_STRING(Parser, JourneySearchValueType, m_valueType)) )
            #ifdef USE_MATCHES_IN_SYNTAXITEM
                .arg( QString(", matches (%1): %2").arg(matchCount()).arg(matchStrings.join(", ")) )
            #else
                .arg( QString() )
            #endif
            #ifdef DEBUG_SYNTAXITEM_PARENTS
                .arg( QString(", this: %1, parent: %2").arg(reinterpret_cast<long>(this), 0, 16)
                                                       .arg(reinterpret_cast<long>(parent()), 0, 16) )
            #else
                .arg( QString() )
            #endif
            .arg(ENUM_STRING(SyntaxItem, Type, m_type));
}

QString SyntaxSequenceItem::toString( int level ) const
{
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    QString s; int l = level; while ( l > 0 ) { s += "  "; --l; }
    QString string;// = SyntaxItem::toString(level);
    string += QString("\n%1Sequence (%2%3%4%5) {")
            .arg(s)
            .arg(FLAGS_STRING(SyntaxItem, Flag, flags()))
            .arg( valueType() == Parser::NoValue ? QString()
                  : QString(", output -> %1").arg(ENUM_STRING(Parser, JourneySearchValueType, valueType())) )
            #ifdef USE_MATCHES_IN_SYNTAXITEM
                .arg( QString(", matches (%1): %2").arg(matchCount()).arg(matchStrings.join(", ")) )
            #else
                .arg( QString() )
            #endif
            #ifdef DEBUG_SYNTAXITEM_PARENTS
                .arg( QString(", this: %1, parent: %2").arg(reinterpret_cast<long>(this), 0, 16)
                                                       .arg(reinterpret_cast<long>(parent()), 0, 16) )
            #else
                .arg( QString() )
            #endif
            ;
    int i = 1;
    foreach ( SyntaxItemPointer item, m_items ) {
        string += QString("\n%1  Sequence Item %2: %3").arg(s).arg( i++ ).arg( item->toString(level + 1) );
    }
    string += QString("\n%1}").arg(s);
    return string;
}

QString SyntaxOptionItem::toString( int level ) const
{
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    QString s; int l = level; while ( l > 0 ) { s += "  "; --l; }
    QString string;// = SyntaxItem::toString(level);
    string += QString("\n%1Option (%2%3%4%5) {")
            .arg( s )
            .arg( FLAGS_STRING(SyntaxItem, Flag, flags()) )
            .arg( valueType() == Parser::NoValue ? QString()
                  : QString(", output -> %1").arg(ENUM_STRING(Parser, JourneySearchValueType, valueType())) )
            #ifdef USE_MATCHES_IN_SYNTAXITEM
                .arg( QString(", matches (%1): %2").arg(matchCount()).arg(matchStrings.join(", ")) )
            #else
                .arg( QString() )
            #endif
            #ifdef DEBUG_SYNTAXITEM_PARENTS
                .arg( QString(", this: %1, parent: %2").arg(reinterpret_cast<long>(this), 0, 16)
                                                       .arg(reinterpret_cast<long>(parent()), 0, 16) )
            #else
                .arg( QString() )
            #endif
            ;
    int i = 1;
    foreach ( SyntaxItemPointer item, m_options ) {
        string += QString("\n%1  Option %2: %3").arg(s).arg( i++ ).arg( item->toString(level + 1) );
    }
    string += QString("\n%1}").arg(s);
    return string;
}

QString SyntaxKeywordItem::toString( int level ) const
{
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    QString s; int l = level; while ( l > 0 ) { s += "  "; --l; }
    QString string;// = SyntaxItem::toString(level);
    if ( !m_valueSequence ) {
        string += QString("Keyword (%1, %2%3%4)")
                .arg(ENUM_STRING(Parser, KeywordType, m_keyword))
                .arg(FLAGS_STRING(SyntaxItem, Flag, flags()))
                #ifdef USE_MATCHES_IN_SYNTAXITEM
                    .arg( QString(", matches (%1): %2").arg(matchCount()).arg(matchStrings.join(", ")) )
                #else
                    .arg( QString() )
                #endif
                #ifdef DEBUG_SYNTAXITEM_PARENTS
                    .arg( QString(", this: %1, parent: %2").arg(reinterpret_cast<long>(this), 0, 16)
                                                           .arg(reinterpret_cast<long>(parent()), 0, 16) )
                #else
                    .arg( QString() )
                #endif
                ;
    } else {
        string += QString("\n%1Keyword (%2, %4, value type: %3%5%6) {").arg(s)
                .arg(ENUM_STRING(Parser, KeywordType, m_keyword)).arg(valueType())
                .arg(FLAGS_STRING(SyntaxItem, Flag, flags()))
                #ifdef USE_MATCHES_IN_SYNTAXITEM
                    .arg( QString(", matches (%1): %2").arg(matchCount()).arg(matchStrings.join(", ")) )
                #else
                    .arg( QString() )
                #endif
                #ifdef DEBUG_SYNTAXITEM_PARENTS
                    .arg( QString(", this: %1, parent: %2").arg(reinterpret_cast<long>(this), 0, 16)
                                                           .arg(reinterpret_cast<long>(parent()), 0, 16) )
                #else
                    .arg( QString() )
                #endif
                ;
//         int i = 1;
        string += m_valueSequence->toString( level + 1 );
//         foreach ( SyntaxItemPointer item, *m_valueSequence->items() ) {
//             if ( item->hasValue() ) {
//                 string += QString("\n%1  Value %2%3: %4").arg(s)
//                         .arg(i++).arg( valueType() == Parser::NoValue ? ""
//                                 : QString(" (output -> %1)").arg(ENUM_STRING(Parser, JourneySearchValueType, valueType())) )
//                         .arg(item->toString(level + 1));
//             } else {
//                 string += QString("\n%1  Value %2 (no output):     %3").arg(s).arg(i++)
//                         .arg(item->toString(level + 1));
//             }
//         }
        string += QString("\n%1}").arg(s);
    }
    return string;
}

QString SyntaxNumberItem::toString( int ) const
{
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    return QString("Number (range: %1-%2, %3%4%5%6)")
            .arg(m_min).arg(m_max)
            .arg(FLAGS_STRING(SyntaxItem, Flag, flags()))
            .arg(valueType() == Parser::NoValue ? QString()
                 : QString(", output -> %1").arg(ENUM_STRING(Parser, JourneySearchValueType, valueType())))
            #ifdef USE_MATCHES_IN_SYNTAXITEM
                .arg( QString(", matches (%1): %2").arg(matchCount()).arg(matchStrings.join(", ")) )
            #else
                .arg( QString() )
            #endif
            #ifdef DEBUG_SYNTAXITEM_PARENTS
                .arg( QString(", this: %1, parent: %2").arg(reinterpret_cast<long>(this), 0, 16)
                                                       .arg(reinterpret_cast<long>(parent()), 0, 16) )
            #else
                .arg( QString() )
            #endif
            ;
}

QString SyntaxWordsItem::toString( int ) const
{
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    return /*SyntaxItem::toString(level) + */QString("Words (words: %1, types: %2, %3%4%5%6)")
            .arg(m_wordCount).arg(m_wordTypes.count())
            .arg(FLAGS_STRING(SyntaxItem, Flag, flags()))
            .arg(valueType() == Parser::NoValue ? ""
                 : QString(", output -> %1,").arg(ENUM_STRING(Parser, JourneySearchValueType, valueType())))
            #ifdef USE_MATCHES_IN_SYNTAXITEM
                .arg( QString(", matches (%1): %2").arg(matchCount()).arg(matchStrings.join(", ")) )
            #else
                .arg( QString() )
            #endif
            #ifdef DEBUG_SYNTAXITEM_PARENTS
                .arg( QString(", this: %1, parent: %2").arg(reinterpret_cast<long>(this), 0, 16)
                                                       .arg(reinterpret_cast<long>(parent()), 0, 16) )
            #else
                .arg( QString() )
            #endif
            ;
}

/*
QDebug& operator<<( QDebug debug, SyntaxItem::Type type )
{
    switch ( type ) {
    case SyntaxItem::MatchNothing:
        return debug << "SyntaxItem::MatchNothing";
    case SyntaxItem::MatchSequence:
        return debug << "SyntaxItem::MatchSequence";
    case SyntaxItem::MatchOption:
        return debug << "SyntaxItem::MatchOption";
    case SyntaxItem::MatchQuotationMark:
        return debug << "SyntaxItem::MatchQuotationMark";
    case SyntaxItem::MatchColon:
        return debug << "SyntaxItem::MatchColon";
    case SyntaxItem::MatchNumber:
        return debug << "SyntaxItem::MatchNumber";
    case SyntaxItem::MatchString:
        return debug << "SyntaxItem::MatchString";
    case SyntaxItem::MatchKeyword:
        return debug << "SyntaxItem::MatchKeyword";
    case SyntaxItem::MatchWords:
        return debug << "SyntaxItem::MatchWords";
    default:
        return debug << static_cast<int>(type);
    }
}

QDebug& operator<<( QDebug debug, SyntaxItem::Flag flag )
{
    switch ( flag ) {
    case SyntaxItem::DefaultMatch:
        return debug << "SyntaxItem::DefaultMatch";
    case SyntaxItem::MatchIsOptional:
        return debug << "SyntaxItem::MatchIsOptional";
    case SyntaxItem::MatchGreedy:
        return debug << "SyntaxItem::MatchGreedy";
    case SyntaxItem::KleenePlus:
        return debug << "SyntaxItem::KleenePlus";
    case SyntaxItem::KleeneStar:
        return debug << "SyntaxItem::KleeneStar";
    default:
        return debug << static_cast<int>(flag);
    }
}*/

#include "syntaxitem.moc"

}; // namespace Parser
