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

#ifndef PARSERSTRUCTURES_HEADER
#define PARSERSTRUCTURES_HEADER

// Own includes
#include "global_generator.h"

// Qt includes
#include <QStringList>
#include <QMetaMethod>

class DocumentationParser;

/**
 * @brief Represents a doxygen comment paragraph.
 *
 * Plain comment paragraphs have type StandardCommentParagraph, other types are doxygen commands
 * with an argument until the end of the paragraph.
 **/
class DoxygenComment {
    friend class DoxygenCommentWithArgument;
public:
    // Creates an invalid DoxygenComment object (for QHash)
    DoxygenComment() : m_type(InvalidDoxygenCommand), m_flags(flagsFromCommand(m_type)) {};
    virtual ~DoxygenComment() {};

    DoxygenCommandType type() const { return m_type; };
    DoxygenCommandFlags flags() const { return m_flags; };
    QString comment() const { return m_comment; };

//     inline bool isInline() const { return m_flags.testFlag(DoxygenCommandInline); }; // No DoxygenComments for inline commands
    inline bool isMultiline() const { return m_flags.testFlag(DoxygenCommandMultiline); };
    inline bool isInword() const { return m_flags.testFlag(DoxygenCommandInWord); };
    inline bool isBegin() const { return m_flags.testFlag(DoxygenCommandBegin); };
    inline bool isEnd() const { return m_flags.testFlag(DoxygenCommandEnd); };
    inline bool isExpectingArgument() const { return m_flags.testFlag(DoxygenCommandExpectsArgument); };
    inline bool isExpectingTwoArguments() const { return m_flags.testFlag(DoxygenCommandExpectsTwoArguments); };
    inline bool isVerbatim() const { return m_flags.testFlag(DoxygenCommandVerbatim); };
    inline bool isSection() const { return m_flags.testFlag(DoxygenCommandIsSection); };
    inline bool isVirtual() const { return m_flags.testFlag(DoxygenCommandVirtual); };

    void appendCommentLine( const QString &line );

    static DoxygenComment* createDoxygenComment(
            const QString &commentLine = QString(), DoxygenCommandType type = StandardCommentParagraph );

protected:
    DoxygenComment( const QString &comment, DoxygenCommandType type = StandardCommentParagraph );

    const DoxygenCommandType m_type;
    DoxygenCommandFlags m_flags;
    QString m_comment;
};

/** @brief Represents a doxygen comment paragraph which expects an argument. */
class DoxygenCommentWithArguments : public DoxygenComment {
    friend class DoxygenComment;
public:
    const QStringList arguments;

protected:
    DoxygenCommentWithArguments( const QString &argument, const QString &comment,
                                 DoxygenCommandType type = StandardCommentParagraph );
    DoxygenCommentWithArguments( const QString &argument1, const QString &argument2,
                                 const QString &comment,
                                 DoxygenCommandType type = StandardCommentParagraph );
    DoxygenCommentWithArguments( const QStringList &arguments, const QString &comment,
                                 DoxygenCommandType type = StandardCommentParagraph );
};

/**
 * @brief Represents a doxygen @section or @subsection command.
 *
 * Expects a one-word argument (reference id), the rest until new line is the title of the section.
 **/
class DoxygenSectionCommand : public DoxygenComment {
    friend class DoxygenComment;
public:
    const QString id;
    QString title() const { return m_comment; };

protected:
    DoxygenSectionCommand( const QString &id, const QString &title,
                           DoxygenCommandType type = DoxygenSection );
};

/** @brief Contains information about the comment block for a method. */
class Comment {
    friend class DocumentationParser;
public:
    Comment( const QString &brief = QString(), const QString &returns = QString(),
             const DoxygenCommentsWithArguments &parameters = DoxygenCommentsWithArguments(),
             const DoxygenComments &otherComments = DoxygenComments() );

    virtual ~Comment();

    QString brief;
    QString returns;
    DoxygenParameters parameters;
    DoxygenComments otherComments;
};

struct EnumerableComment {
    EnumerableComment( const QString &name = QString(), int value = 0,
                       const QString &brief = QString(),
                       const DoxygenComments &otherComments = DoxygenComments() );

    QString name; // Name of the enumerable
    int value;
    QString brief;
    DoxygenComments otherComments;
};

struct EnumComment {
    EnumComment( const QString &name = QString(), const QString &brief = QString(),
                 const DoxygenComments &otherComments = DoxygenComments() );

    void sort();
    void addEnumerable( const EnumerableComment &enumerable );

    QString name; // Name of the enumeration
    int lastEnumerableValue;
    QString brief;
    DoxygenComments otherComments;
    EnumerableComments enumerables; // For each enumerable of the enumeration
    QList<EnumerableComment> sortedEnumerables;
};

/** @brief Contains information about the comment block for a class and its methods. */
struct ClassComment {
    ClassComment( const Comment &classComment = Comment() ) : comment(classComment) {};

    Comment comment;
    MethodComments methodComments;
    EnumComments enumComments;
};

/** @brief Represents a method. */
class Method {
    friend class DocumentationParser;

public:
    Method() {};

    bool isValid() const { return !name.isEmpty(); };

    QMetaMethod metaMethod;
    QString name;
    QString returnType;
    QStringList parameters;
    QStringList typedParameters;
    QStringList templatedParameters;
    Comment comment;

protected:
    Method( const QMetaMethod &metaMethod, DocumentationParser *parser = 0 );
};

/**
 * @brief A QMap of Method objects, keyed with the methods signatures.
 *
 * Use findByMethodName() to find a method by its name, rather than its complete signature.
 * If there are multiple method overloads the first one gets returned.
 **/
class Methods : public QMap< QString, Method > {
public:
    Methods() : QMap< QString, Method >() {};

    Method findByMethodName( const QString &name ) const {
        for ( QMap< QString, Method >::ConstIterator it = constBegin(); it != constEnd(); ++it ) {
            const QString methodName = methodNameFromSignature( it.key() );
            if ( methodName == name ) {
                return it.value();
            } else if ( name == "get" ) {
//                 qDebug() << methodName << it.key();
            }
        }

        // Return an invalid Method
        return Method();
    };

private:
    inline QString methodNameFromSignature( const QString &signature ) const {
        const int pos = signature.indexOf( '(' );
        if ( pos == -1 ) {
            qDebug() << "Method name not found in signature" << signature;
            return QString();
        } else {
            return signature.left( pos );
        }
    };
};

/**
 * @brief Contains information about a class and its methods
 *
 * Automatically parses the source file and extracts information from the given QMetaObject.
 **/
class ClassInformation {
    friend class DocumentationParser;

public:
    QMetaObject metaObject;
    QString className;
    QString scriptObjectName;
    Comment comment;

    Methods methods;
    QList<Method> sortedMethods;
    QStringList methodNames; // TODO ADD THIS

    EnumComments enums;
    QList<EnumComment> sortedEnums;

protected:
    ClassInformation( const QMetaObject &object, const QString &className,
                      const Comment &classComment, const Methods &methods,
                      const EnumComments &enums, const QString &scriptObjectName = QString() );
};

#endif // Multiple inclusion guard
