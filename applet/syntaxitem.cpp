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

typedef Syntax S; // A bit shorter syntax definition
SyntaxItemPointer Syntax::journeySearchSyntaxItem()
{
    // Define longer match parts here for better readability
    SyntaxSequencePointer matchDate = (S::number(1, 31)->outputTo(DateDayValue) + S::character('.') +
             S::number(1, 12)->outputTo(DateMonthValue) + S::character('.') +
             S::number(1970, 2999)->outputTo(DateYearValue)->optional())->outputTo(DateValue);
    SyntaxSequencePointer matchTimeAt = S::keyword( KeywordTimeAt,
           ( S::number(0, 23)->outputTo(TimeHourValue) +
            (S::character(':') + S::number(0, 59)->outputTo(TimeMinuteValue))->optional() +
            (S::character(',')->optional() + matchDate)->optional() )->outputTo(DateAndTimeValue) );
    SyntaxSequencePointer matchTimeIn =
             S::keyword( KeywordTimeIn, S::number(1, 1339)->outputTo(RelativeTimeValue) ) +
             S::keyword( KeywordTimeInMinutes )->optional();

    // Define the journey search syntax
    return (S::keywordTo() | S::keywordFrom())->optional() +
           ((S::character('"') + S::word()->plus()->outputTo(StopNameValue) + S::character('"'))
            | S::word()->plus()->outputTo(StopNameValue)) +
           (S::keywordDeparture() | S::keywordArrival())->optional() +
            S::keywordTomorrow()->optional() +
           (matchTimeAt | matchTimeIn)->optional();
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

SyntaxItems SyntaxItem::findChildren( Type type ) const
{
    SyntaxItems itemList;

    // Add all children of the given type to itemList
    SyntaxItems childItems = children();
    foreach ( const SyntaxItemPointer &childItem, childItems ) {
        if ( childItem->type() == type ) {
            itemList << childItem;
        }
        itemList << childItem->findChildren( type );
    }

    return itemList;
}

SyntaxKeywordPointer SyntaxItem::findKeywordChild( KeywordType keywordType ) const
{
    SyntaxItems childItems = children();
    foreach ( const SyntaxItemPointer &childItem, childItems ) {
        if ( childItem->type() == MatchKeyword ) {
            // Test if the found keyword child item is of the given keyword type
            SyntaxKeywordPointer keyword = qobject_cast< SyntaxKeywordItem* >( childItem.data() );
            if ( keyword->keyword() == keywordType ) {
                return keyword; // Keyword of the given type found
            }
        }

        // Search for a keyword item of the given type in the child item
        SyntaxKeywordPointer keyword = childItem->findKeywordChild( keywordType );
        if ( keyword ) {
            return keyword; // Keyword of the given type found in the child item
        }
    }

    return 0; // Not found
}

void SyntaxItem::removeChangeFlags()
{
    unsetFlag( ChangingFlags );
    foreach ( const SyntaxItemPointer &syntaxItem, children() ) {
        syntaxItem->removeChangeFlags();
    }
}

#ifdef USE_MATCHES_IN_SYNTAXITEM
void SyntaxItem::addMatch( const MatchItem& matchedItem )
{
    addMatch( matchedItem.position(), matchedItem.input(), matchedItem.value() );
}
#endif

QString SyntaxItem::toString( int level, DescriptionType descriptionType ) const
{
    Q_UNUSED( descriptionType );
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    QString s; int l = level; while ( l > 0 ) { s += "  "; --l; }
    return QString("%1%2 (%7, %3%4%5%6)").arg(s)
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
            .arg( ENUM_STRING(SyntaxItem, Type, m_type) );
}

QString SyntaxSequenceItem::toString( int level, DescriptionType descriptionType ) const
{
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    QString s; int l = level; while ( l > 0 ) { s += "  "; --l; }
    QString string;// = SyntaxItem::toString(level);
    string += QString("%1Sequence (%6 items, %2%3%4%5)").arg(s)
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
            .arg( m_items.count() );
    if ( descriptionType == FullDescription ) {
        string += " {";
        int i = 1;
        foreach ( SyntaxItemPointer item, m_items ) {
            string += QString("\n%1  Sequence Item %2: %3").arg(s).arg( i++ )
                      .arg( item->toString(level + 1, descriptionType) );
        }
        string += QString("\n%1}").arg(s);
    }
    return string;
}

QString SyntaxOptionItem::toString( int level, DescriptionType descriptionType ) const
{
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    QString s; int l = level; while ( l > 0 ) { s += "  "; --l; }
    QString string;// = SyntaxItem::toString(level);
    string += QString("%1Option (%6 items, %2%3%4%5)").arg(s)
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
            .arg( m_options.count() );
    if ( descriptionType == FullDescription ) {
        string += " {";
        int i = 1;
        foreach ( SyntaxItemPointer item, m_options ) {
            string += QString("\n%1  Option %2: %3").arg(s).arg( i++ )
                      .arg( item->toString(level + 1, descriptionType) );
        }
        string += QString("\n%1}").arg(s);
    }
    return string;
}

QString SyntaxKeywordItem::toString( int level, DescriptionType descriptionType ) const
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
        string += QString("%1Keyword (%2, %3%4%5)").arg(s)
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
        string += QString("%1Keyword (%2, %4, value type: %3%5%6) { ").arg(s)
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
        string += m_valueSequence->toString( level + 1, descriptionType );
        string += " }";
    }
    return string;
}

QString SyntaxNumberItem::toString( int level, DescriptionType descriptionType ) const
{
    Q_UNUSED( descriptionType );
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    QString s; int l = level; while ( l > 0 ) { s += "  "; --l; }
    return QString("%1Number (range: %2-%3, %4%5%6%7)").arg(s)
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

QString SyntaxWordItem::toString( int level, DescriptionType descriptionType ) const
{
    Q_UNUSED( descriptionType );
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QStringList matchStrings;
        foreach ( const MatchData &match, matches() ) {
            matchStrings << match.toString();
        }
    #endif

    QString s; int l = level; while ( l > 0 ) { s += "  "; --l; }
    return QString("%1Words (types: %2, %3%4%5%6)").arg(s)
            .arg(m_wordTypes.count())
            .arg(FLAGS_STRING(SyntaxItem, Flag, flags()))
            .arg(valueType() == Parser::NoValue ? ""
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

#include "syntaxitem.moc"

}; // namespace Parser
