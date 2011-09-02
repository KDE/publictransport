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

#ifndef LEXEM_HEADER
#define LEXEM_HEADER

#include <QString>
#include <QExplicitlySharedDataPointer>

namespace Parser {

class LexicalAnalyzer;
class LexemPrivate;

/**
 * @brief Represents a lexem, eg. a string or a number.
 *
 * There are different types of lexems (@ref Lexem::Type). To get the position in the input string
 * use @ref Lexem::position.
 *
 * Lexem objects are elements of the output list of LexicalAnalyzer and elements of the input list
 * to SyntacticalAnalyzer.
 **/
class Lexem {
public:
    // Friend classes are allowed to call the protected constructor
    friend class LexicalAnalyzer;
    friend class SyntacticalAnalyzer; // Creates Error Lexem objects

    friend class LexemPrivate;

    /**
     * @brief Types of lexems.
     **/
    enum Type {
        Error = 0, /**< An illegal string/character in the input string. */
        Number, /**< A string of digits. */
        String, /**< A string, maybe a keyword. */
        Character, /**< A single character, eg. a quotation mark, colon or point. */
//         QuotationMark, /**< A single quotation mark ("). */
//         Colon, /**< A colon (:). */

        Space /**< A space character (" ") at the end of the input or at the specified
                * cursor position */
    };

    /**
     * @brief Constructs an invalid Lexem.
     *
     * This is used for eg. QHash as default value.
     **/
    Lexem();

    Lexem( const Lexem &other );
    virtual ~Lexem();
    Lexem &operator=( const Lexem &other );

    /**
     * @brief The type of this lexem.
     *
     * @see Type
     **/
    Type type() const;

    /**
     * @brief The original text of this lexem in the input string.
     *
     * For error items (@ref Error), this contains the illegal string read from the input string.
     **/
    QString input() const;

    /** @brief Wether or not the text of this lexem consists only of @p character. */
    bool textIsCharacter( char character ) const;

    /** @brief The position of this lexem in the input string. */
    int position() const;

    /** @brief Wether or not there are any errors in the input string for this lexem. */
    bool isErroneous() const;

    /** @brief Wether or not there is a space character after this lexem in the input string. */
    bool isFollowedBySpace() const;

    /**
     * @brief Wether or not this is a valid lexem, that has been read from the input string.
     *
     * Invalid lexems are returned by QHash if trying to get an item, which isn't in the hash.
     * That means that
     * @code
     * QHash< Lexem::Type, Lexem > hash;
     * if ( hash.contains(Lexem::Number) ) {
     *   ...
     * }
     * @endcode
     * is equivalent to
     * @code
     * QHash< Lexem::Type, Lexem > hash;
     * if ( hash[Lexem::Number].isValid() ) {
     *   ...
     * }
     * @endcode
     * but the latter needs to create a default constructed Lexem.
     **/
    bool isValid() const;

protected:
    /**
     * @brief Constructs a new Lexem object.
     *
     * @param type The type of the Lexem.
     *
     * @param text The text in the input string for which this Lexem is created.
     *
     * @param pos The position in the input string of the Lexem.
     *
     * @param followedBySpace Whether or not there is a space character after this lexem in the
     *   input string.
     **/
    Lexem( Type type, const QString &text, int pos, bool followedBySpace = true );

private:
    QExplicitlySharedDataPointer<LexemPrivate> d;
};

typedef QList<Lexem> LexemList;
typedef QList<Lexem::Type> LexemTypes;
inline bool operator <( const Lexem &lexem1, const Lexem &lexem2 ) {
    return lexem1.position() < lexem2.position();
};
QDebug &operator <<( QDebug debug, Lexem::Type type );

} // namespace

#endif // LEXEM_HEADER
