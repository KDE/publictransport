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

#include "matchitem.h"
#include <QDebug>
#include <QMetaEnum>

#define ENUM_NAME_STRING(namespace, enum) \
    namespace::staticMetaObject.enumerator(namespace::staticMetaObject.indexOfEnumerator(#enum)).name()
#define ENUM_STRING(namespace, enum, value) \
    namespace::staticMetaObject.enumerator(namespace::staticMetaObject.indexOfEnumerator(#enum)).valueToKey(value)
#define FLAGS_STRING(namespace, enum, value) \
    QString(namespace::staticMetaObject.enumerator(namespace::staticMetaObject.indexOfEnumerator(#enum)).valueToKeys(value))

namespace Parser {

class MatchItemPrivate : public QSharedData {
public:
    friend class MatchItem;

    MatchItemPrivate() : QSharedData(), position(-1) {;
    };
    MatchItemPrivate( MatchItem::Type type, const LexemList &lexems,
                      JourneySearchValueType valueType, const QVariant& value,
                      MatchItem::Flags flags, int matchedSyntaxItemIndex = 0 )
            : QSharedData(), type(type), flags(flags),
              matchedSyntaxItemIndex(matchedSyntaxItemIndex), lexems(lexems),
              valueType(valueType), value(value)
    {
        updatePosition();
    };
    virtual ~MatchItemPrivate() {};

    // Updates the position of this syntax item in the input string.
    // Gets called in the constructor (for terminals) 
    // and when new children are prepended (for non-terminals)
    inline void updatePosition() { position = firstLexem().position(); };

    inline const Lexem firstLexem() const {
        return lexems.isEmpty() ? (children.isEmpty() ? Lexem() : children.first().firstLexem())
                                : lexems.first();
    };

    inline const MatchItems allChildren() const {
        MatchItems ret = children;
        foreach ( const MatchItem &item, children ) {
            ret << item.allChildren();
        }
        return ret;
    };

    inline void addChild( const MatchItem& matchItem ) {
        bool prepended = false;

        // Sort the new matchItem into the output list, sorted by position
        if ( children.isEmpty() || children.last().position() < matchItem.position() ) {
            // Append the new item
            prepended = children.isEmpty();
            children.insert( children.end(), matchItem );
        } else {
            MatchItems::iterator it = children.begin();
            while ( it->position() < matchItem.position() ) {
                ++it;
            }
            prepended = it == children.begin();
            children.insert( it, matchItem );
        }

        if ( position == -1 || prepended ) {
            // Update position if childs have been prepended
            updatePosition();
        }
    };

    virtual QString correctedText() const {
        switch ( type ) {
        case MatchItem::String: {
            QString output = value.toString();
            if ( !lexems.isEmpty() ) { // String items should always have (exactly) one associated lexem
                if ( lexems.first().isFollowedBySpace() ) {
                    output.append(' ');
                }
            }
            return output;
        } case MatchItem::Number: {
            QString output = value.toString();
            if ( !lexems.isEmpty() ) { // Number items should always have (exactly) one associated lexem
                // Add leading zeros, if the input string has leading zeros
                const QString input = lexems.first().input();
                const int maxZeros = input.length() - output.length();
                int i = 0;
                while ( i < maxZeros && input[i] == '0' ) {
                    output.prepend('0');
                    ++i;
                }

                if ( lexems.first().isFollowedBySpace() ) {
                    output.append(' ');
                }
            }
            return output;
        } default:
            return input();
        }
    };

    inline QString text( AnalyzerCorrections appliedCorrections ) const
    {
        if ( appliedCorrections.testFlag(SkipUnexpectedTokens) && valueType == ErrorCorrectionValue
             && static_cast<AnalyzerCorrection>(value.toInt()) == SkipUnexpectedTokens )
        {
            // Apply correction SkipUnexceptedTokens by simply returning an empty string
            // instead of the unexpected lexems (in d->lexems and in d->children)
            return QString();
        }

//         if ( appliedCorrections.testFlag(CompleteKeywords) && type == MatchItem::Keyword
//              && flags.testFlag(Parser::MatchItem::CorrectedMatchItem)
//              /*&& valueType == NoValue*/ ) // TODO Add a value type for the keyword type?
//         {
//             // Apply correction CompleteKeywords by returning the completed keyword string
//             return correctedText() + ' '; // TODO space afterwards?
//         }
/*
        if ( updateValues.contains(valueType) ) {
            
        }*/

        QString string;
        switch ( type ) {
        case MatchItem::String:
            string += correctedText();
            break;
        case MatchItem::Number:
            string += appliedCorrections.testFlag(CorrectNumberRanges)
                    ? correctedText() : input();
            break;
        case MatchItem::Keyword:
            string += appliedCorrections.testFlag(CompleteKeywords)
                    ? correctedText() : input(); // uses MatchItemPrivateKeyword::correctedText()
            break;
        default:
            foreach ( const Lexem &lexem, lexems ) {
                string += lexem.input();
                if ( lexem.isFollowedBySpace() ) {
                    string += ' ';
                }
            }
            break;
        }

        // Add text of child items.
        foreach ( const MatchItem &item, children ) {
            // Use d-pointer directly, because MatchItem::text() trims the resulting string.
            string += item.d->text( appliedCorrections );
        }
        return string;
    };

    inline QString input() const
    {
        QString string;
        foreach ( const Lexem &lexem, lexems ) {
            string += lexem.input();
            if ( lexem.isFollowedBySpace() ) {
                string += ' ';
            }
        }
        foreach ( const MatchItem &item, children ) {
            string += item.input();
        }
        return string;
    };

    inline bool combineStopNameItems() {
        for ( int i = 0; i < children.count(); ++ i ) {
            if ( children[i].valueType() == StopNameValue ) {
                while ( i < children.count() - 1 && children[i + 1].valueType() == StopNameValue ) {
                    // Combine children
                    children[i].d->lexems << children[i + 1].d->lexems;
                    children[i].d->children << children[i + 1].d->children;
                    children[i].d->value = children[i].value().toString() + ' ' +
                                           children[i + 1].value().toString();

                    // Remove second child
                    children.removeAt( i + 1 );
                }
                return true;
            } else if ( children[i].combineStopNameItems() ) {
                return true;
            }
        }

        return false;
    };

private:
    MatchItem::Type type;
    MatchItem::Flags flags;
    int matchedSyntaxItemIndex;

    LexemList lexems; // Matched Lexems
    int position; // Stores d->lexems.position()
    MatchItems children; // Child items, eg. items of a sequence/option, value items of a keyword, ...

    JourneySearchValueType valueType;
    QVariant value;
//     QVariant correctedValue;
};

class MatchItemPrivateKeyword : public MatchItemPrivate {
public:
    friend class MatchItem;

    MatchItemPrivateKeyword( const LexemList &lexems, KeywordType keyword,
                             const QString &completedKeyword, MatchItem::Flags flags,
                             int matchedSyntaxItemIndex )
            : MatchItemPrivate(MatchItem::Keyword, lexems, NoValue, static_cast<int>(keyword),
                               flags, matchedSyntaxItemIndex),
              completedKeyword(completedKeyword)
    {
    };
    virtual ~MatchItemPrivateKeyword() {};

    virtual QString correctedText() const { return completedKeyword + ' '; };

private:
    QString completedKeyword;
};

MatchItem::MatchItem() : d(new MatchItemPrivate)
{
}

MatchItem::MatchItem( const Parser::MatchItem& other ) : d(other.d)
{
}

MatchItem::MatchItem( MatchItem::Type type, const LexemList &lexems,
                      JourneySearchValueType valueType, const QVariant& value,
                      MatchItem::Flags flags, int matchedSyntaxItemIndex )
        : d(new MatchItemPrivate(type, lexems, valueType, value, flags, matchedSyntaxItemIndex))
{
}

MatchItem::MatchItem( const LexemList &lexems, KeywordType keyword,
                      const QString &completedKeyword, MatchItem::Flags flags,
                      int matchedSyntaxItemIndex )
        : d(new MatchItemPrivateKeyword(lexems, keyword, completedKeyword, flags,
                                        matchedSyntaxItemIndex))
{

}

MatchItem::MatchItem( const QExplicitlySharedDataPointer<MatchItemPrivate> &dd ) : d(dd)
{
    d.detach();
}

MatchItem::~MatchItem()
{
}

MatchItem& MatchItem::operator=( const Parser::MatchItem &other )
{
    d = other.d;
    return *this;
}

MatchItem MatchItem::copy() const
{
    return MatchItem( d );
}

const Parser::Lexem MatchItem::firstLexem() const
{
    return d->firstLexem();
}

MatchItem::Type MatchItem::type() const
{
    return d->type;
}

MatchItem::Flags MatchItem::flags() const
{
    return d->flags;
}

int MatchItem::matchedSyntaxItemIndex() const
{
    return d->matchedSyntaxItemIndex;
}

int MatchItem::position() const
{
    return d->position;
}

JourneySearchValueType MatchItem::valueType() const
{
    return d->valueType;
}

QVariant MatchItem::value() const
{
    return d->value;
}

void MatchItem::setValue( const QVariant& value )
{
    d->value = value;
}

void MatchItem::setMatchedSyntaxItemIndex( int matchedSyntaxItemIndex )
{
    d->matchedSyntaxItemIndex = matchedSyntaxItemIndex;
}

QVariant MatchItem::keywordValue() const
{
    return d->type == Keyword ? d->children.first().value() : QVariant();
}

bool MatchItem::combineStopNameItems()
{
    return d->combineStopNameItems();
}

bool MatchItem::isTerminal() const
{
    return d->children.isEmpty();
}

bool MatchItem::isErroneous() const
{
    return d->type == Error;
}

bool MatchItem::isValid() const
{
    return position() >= 0 || d->type == Error;
}

void MatchItem::addChild( const MatchItem& matchItem )
{
    d->addChild( matchItem );
}

void MatchItem::addChildren( const MatchItems& matchItems )
{
    foreach ( const MatchItem &matchItem, matchItems ) {
        d->addChild( matchItem );
    }
//     d->children << matchItems;
}

const MatchItems MatchItem::children() const
{
    return d->children;
}

MatchItems& MatchItem::children()
{
    return d->children;
}

QString MatchItem::input() const
{
    return d->input();
}

QString MatchItem::text( AnalyzerCorrections appliedCorrections, bool trim ) const
{
    return trim ? d->text( appliedCorrections ).trimmed() : d->text( appliedCorrections );
}

const LexemList MatchItem::lexems() const
{
    LexemList allLexems = d->lexems;
    foreach ( const MatchItem &item, d->children ) {
        allLexems << item.lexems();
    }
    return allLexems;
}

const MatchItems MatchItem::allChildren() const
{
    return d->allChildren();
}

bool operator==( const MatchItem& l, const MatchItem& r )
{
    // There can only be one item at a given position, so it's enough to check MatchItem::position() TODO
    return l.position() == r.position();
}

QString MatchItem::toString( int level ) const
{
    QString s; int l = level; while ( l > 0 ) { s += "  "; --l; }
    QString string;
    MatchItemPrivateKeyword *privateKeyword = dynamic_cast<MatchItemPrivateKeyword*>( d.data() );
    string += QString("\n%1%2 (matchPos: %8, flags: %3, pos: %4, value: %5, text: %6%7) {").arg(s)
            .arg(ENUM_STRING(MatchItem, Type, d->type))
            .arg(FLAGS_STRING(MatchItem, Flag, d->flags))
            .arg(position()).arg(d->value.toString()).arg(input())
            .arg(privateKeyword ? QString(", corrected: %1").arg(privateKeyword->correctedText()) : QString())
            .arg(d->matchedSyntaxItemIndex);
    if ( d->children.isEmpty() ) {
        return string + "}";
    }

    foreach ( const MatchItem &item, d->children ) {
        string += item.toString( level + 1 );
    }
    string += QString("\n%1}").arg(s);
    return string;
}

QString MatchItem::keywordId( KeywordType keywordType )
{
    switch ( keywordType ) {
    case KeywordTo:
        return "to";
    case KeywordFrom:
        return "from";
    case KeywordTimeIn:
        return "in";
    case KeywordTimeInMinutes:
        return "minutes";
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

QDebug &operator <<( QDebug debug, MatchItem::Type type )
{
    switch ( type ) {
    case MatchItem::Invalid:
        return debug << "MatchItem::Invalid";
    case MatchItem::Error:
        return debug << "MatchItem::Error";
    case MatchItem::Sequence:
        return debug << "MatchItem::Sequence";
    case MatchItem::Option:
        return debug << "MatchItem::Option";
    case MatchItem::Keyword:
        return debug << "KMatchItem::eyword";
    case MatchItem::Number:
        return debug << "MatchItem::Number";
    case MatchItem::Character:
        return debug << "MatchItem::Character";
    case MatchItem::String:
        return debug << "MatchItem::String";
    case MatchItem::Word:
        return debug << "MatchItem::Words";
    default:
        return debug << static_cast<int>(type);
    }
};

#include "matchitem.moc"

}; // namespace Parser
