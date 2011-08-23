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

#include "lexem.h"
#include <QDebug>

namespace Parser {

class LexemPrivate : public QSharedData {
public:
    friend class Lexem;

    LexemPrivate() : QSharedData(), type(Lexem::Error), position(-1), followedBySpace(true) {
    }
    LexemPrivate( Lexem::Type type, const QString& text, int position, bool followedBySpace )
            : QSharedData(), type(type), text(text), position(position),
              followedBySpace(followedBySpace) {
    };

private:
    Lexem::Type type;
    QString text;
    int position;
    bool followedBySpace;
};

Lexem::Lexem() : d(new LexemPrivate)
{
}

Lexem::Lexem( Lexem::Type type, const QString& text, int position, bool followedBySpace )
        : d(new LexemPrivate(type, text, position, followedBySpace))
{
}

Lexem::Lexem( const Lexem& other ) : d(other.d)
{
}

Lexem::~Lexem()
{
}

Lexem& Lexem::operator=( const Lexem& other )
{
    d = other.d;
    return *this;
}

Lexem::Type Lexem::type() const
{
    return d->type;
}

bool Lexem::isValid() const
{
    return d->position >= 0;
}

QString Lexem::input() const
{
    return d->text;
}

bool Lexem::textIsCharacter(char character) const
{
    return d->text.length() == 1 && d->text[0] == character;
}

int Lexem::position() const
{
    return d->position;
}

bool Lexem::isErrornous() const
{
    return d->type == Error;
}

bool Lexem::isFollowedBySpace() const
{
    return d->followedBySpace;
}

QDebug &operator <<( QDebug debug, Lexem::Type type )
{
    switch ( type ) {
    case Lexem::Error:
        return debug << "Lexem::Invalid";
    case Lexem::Number:
        return debug << "Lexem::Number";
//     case Lexem::QuotationMark:
//         return debug << "Lexem::QuotationMark";
//     case Lexem::Colon:
//         return debug << "Lexem::Colon";
    case Lexem::Character:
        return debug << "Lexem::Character";
    case Lexem::Space:
        return debug << "Lexem::Space";
    case Lexem::String:
        return debug << "Lexem::String";
    default:
        return debug << static_cast<int>(type);
    }
};

}; // namespace Parser
