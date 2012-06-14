/*
*   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef DOCUMENTATIONPARSER_HEADER
#define DOCUMENTATIONPARSER_HEADER

// Own includes
#include "global_generator.h"
#include "parser_structures.h"

// PublicTransport engine includes
#include <engine/scripting.h>

// Qt includes
#include <QFile>
#include <QDir>

using namespace Scripting;

/**
 * @brief Parse doxygen comment blocks in C++ header files.
 *
 * Supports most basic doxygen commands:
 * @@em, @@b, @@p, @@n, @@brief, @@param, @@return(s), @@todo, @@warning, @@note, @@see, @@since,
 * @@deprecated, @@li, @@ref, @@bug, @@section, @@subsection, @@subsubsection, @@ code, @@endcode,
 * @@c, @@ verbatim, @@endverbatim and @@image (without size parameters).
 *
 * Parses documentation blocks for all public methods, signals and slots. Supports different method
 * overloads, a normalized signature gets used for indexing methods.
 **/
class DocumentationParser {
    friend class ClassInformation;
    friend class Method;
    friend class Comment;

public:
    DocumentationParser( const QString &sourceFilePath );

    static Comments parseGlobalDocumentation( QIODevice *dev );

    /**
     * @brief Add class to be parsed.
     *
     * This function needs to be called with all classes to be parsed, before calling parse().
     * The function classInformations() will only return information for classes added using
     * this function.
     *
     * @param metaObject The QMetaObject of the class to add.
     * @param scriptObjectName The name of a global instance of this class in scripts, if any.
     **/
    void addClass( const QMetaObject &metaObject, const QString &scriptObjectName = QString() );

    void addEnum( const QString &name );

    /** @brief Parse the source file and build structures for all classes added using addClass(). */
    void parse();

    /** @brief Get all global comment blocks (with an empty line following the comment block). */
    Comments globalComments() const { return m_classParseResults.globalComments; };

    /** @brief Get information about classes added using addClass(). */
    ClassInformationList classInformations() const { return m_classInformations; };

    EnumCommentList globalEnumComments() const { return m_globalEnumComments; };

protected:
    QString cToQtScriptTypeName( const QString &cTypeName );
    bool checkMethod( const QMetaMethod &method );
    bool readUntil( QIODevice *dev, const QString &str );

private:
    /** @brief Holds information about all parsed comment blocks. */
    struct ParseResults {
        /** @brief Global comment blocks (no signature in the following line). */
        Comments globalComments;

        /** @brief Class comment blocks (directly followed by a class declaration). */
        ClassComments classComments;

        /** @brief Global enumeration comment blocks (directly followed by a enum declaration). */
        EnumComments enumComments;
    };

    struct ParseContext {
        QString className;
        QString enumName;

        inline bool isInClassEnum() const { return isInClass() && isInEnum(); };
        inline bool isInClass() const { return !className.isEmpty(); };
        inline bool isInEnum() const { return !enumName.isEmpty(); };
    };

    ClassInformation parseClass( const QMetaObject &object );

    // Call after all classes are added via addClass().
    // Stores method names (lower case) as keys and method comments as values,
    // the first string in the pair is the brief comment, the second string (list) are normal
    // comment lines
    ParseResults parseDocumentation();

    // Returns the context, ie. name of the class/enum if currently in such such a declaration
    static ParseContext parseDocumentationBlock( QIODevice *dev, int *lineNumber,
                                                 const ParseContext &context,
                                                 ParseResults *parseResults );

    Methods getMethods( const QMetaObject &obj, const MethodComments &comments );

    QString scriptObjectName( const QString &className ) const;

    QString m_sourceFilePath;
    ParseResults m_classParseResults;
    ClassInformationList m_classInformations;
    EnumCommentList m_globalEnumComments;
    QStringList m_enums; // Keys are enumeration names
    QHash< QString, QMetaObject > m_metaObjects;
    QHash< QString, QString > m_scriptObjectNames;
};

/** @brief Get flags for @p type. */
DoxygenCommandFlags flagsFromCommand( DoxygenCommandType type );

/** @brief Contains information and offers functions used while parsing one comment block. */
class ParseInfo {
public:
    ParseInfo( int lineNumber = 0, const QString &line = QString() )
            : line(line), lineNumber(lineNumber), newComment(0), unclosedComment(0),
              unclosedBeginCommand(InvalidDoxygenCommand), unclosedList(false),
              lastCommentClosed(false)
    {
    };

    /** @brief Increase line number and initialize for parsing the new @p line. */
    void startNewLine( const QString &line = QString() );

    /** @brief Close open comment blocks and list commands, if any. */
    void closeOpenComment();

    /** @brief Parse inline commands and replace by markers. */
    QString parseInlineCommands();

    /** @brief Parse non-inline commands and create DoxygenComment objects. */
    void parseNoninlineCommands();

    QString line; ///< Current line
    int lineNumber; ///< Current line number
    Comment comment; ///< Parser output
    DoxygenComment *newComment; //
    DoxygenComment *unclosedComment;
    DoxygenCommandType unclosedBeginCommand;
    bool unclosedList;
    bool lastCommentClosed;
};

#endif // Multiple inclusion guard
