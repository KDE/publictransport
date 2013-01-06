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

// Header
#include "parser_structures.h"

// Own includes
#include "documentationparser.h"

DoxygenComment::DoxygenComment( const QString &comment, DoxygenCommandType type )
        : m_type(type), m_flags(flagsFromCommand(type))
{
    appendCommentLine( comment );
}

DoxygenCommentWithArguments::DoxygenCommentWithArguments(
        const QString &argument, const QString &comment, DoxygenCommandType type )
        : DoxygenComment(comment, type), arguments(QStringList() << argument)
{

}

DoxygenCommentWithArguments::DoxygenCommentWithArguments(
        const QString &argument1, const QString &argument2, const QString &comment,
        DoxygenCommandType type )
        : DoxygenComment(comment, type), arguments(QStringList() << argument1 << argument2)
{

}

DoxygenCommentWithArguments::DoxygenCommentWithArguments(
        const QStringList &arguments, const QString &comment, DoxygenCommandType type )
        : DoxygenComment(comment, type), arguments(arguments)
{

}

DoxygenSectionCommand::DoxygenSectionCommand(
        const QString &id, const QString &title, DoxygenCommandType type )
        : DoxygenComment(title, type), id(id)
{

}

void DoxygenComment::appendCommentLine( const QString &line )
{
    if ( !isMultiline() && !m_comment.isEmpty() ) {
        qWarning() << "Cannot append lines to single line command" << m_type << line;
        return;
    }

    if ( !m_comment.isEmpty() ) {
        m_comment += isVerbatim() ? '\n' : ' ';
    }

    m_comment += isVerbatim() ? line : line.trimmed();
}

DoxygenComment* DoxygenComment::createDoxygenComment( const QString &commentLine,
                                                        DoxygenCommandType type )
{
    if ( type == UnknownDoxygenCommand || type == InvalidDoxygenCommand ) {
        return 0;
    }
    DoxygenCommandFlags flags = flagsFromCommand( type );

    if ( flags.testFlag(DoxygenCommandInline) ) {
        qWarning() << "Cannot create a new DoxygenComment for an inline command" << type;
        return 0;
    } else if ( flags.testFlag(DoxygenCommandExpectsArgument) ) {
        const int firstSpacePos = commentLine.indexOf(' ');
        if ( firstSpacePos == -1 ) {
            return 0; // No comment text found
        }
        const QString argument = commentLine.left( firstSpacePos );
        const QString comment = commentLine.mid( firstSpacePos + 1 );
        return new DoxygenCommentWithArguments( argument, comment, type );
//     } else if ( flags.testFlag(DoxygenCommandExpectsTwoArguments) ) {
//         QRegExp twoArgumentsRegExp("\\b(\\w+)\\s+(\\w+)");
//         if ( twoArgumentsRegExp.indexIn(commentLine) == -1 ) {
//             return 0; //
//         }
//         const QString firstArgument = twoArgumentsRegExp.cap(1);
//         const QString secondArgument = twoArgumentsRegExp.cap(2);
//         const QString comment = commentLine.mid(
//                 twoArgumentsRegExp.pos() + twoArgumentsRegExp.matchedLength() + 1 );
//         return new DoxygenCommentWithArguments( firstArgument, secondArgument, comment, type );
    } else if ( flags.testFlag(DoxygenCommandIsSection) ) {
        const int firstSpacePos = commentLine.indexOf(' ');
        if ( firstSpacePos == -1 ) {
            return 0; // No reference id found
        }
        const QString id = commentLine.left( firstSpacePos );
        const QString title = commentLine.mid( firstSpacePos + 1 );
        return new DoxygenSectionCommand( id, title, type );
    } else if ( flags.testFlag(DoxygenCommandVerbatim) ) {
        return new DoxygenComment( commentLine, type );
    } else {
        return new DoxygenComment( commentLine, type );
    }
}

Comment::Comment( const QString &brief, const QString &returns,
                  const DoxygenCommentsWithArguments &parameters,
                  const DoxygenComments &otherComments )
        : brief(brief), returns(returns), parameters(parameters),
          otherComments(otherComments)
{
}

Comment::~Comment()
{
}

EnumerableComment::EnumerableComment( const QString &name, int value, const QString &brief,
                                      const DoxygenComments &otherComments )
        : name(name), value(value), brief(brief), otherComments(otherComments)
{
}

EnumComment::EnumComment( const QString &name,  const QString &brief,
                          const DoxygenComments &otherComments )
        : name(name), lastEnumerableValue(-1), brief(brief), otherComments(otherComments)
{
}

void EnumComment::addEnumerable( const EnumerableComment &enumerable )
{
    enumerables.insert( enumerable.name, enumerable );
    lastEnumerableValue = enumerable.value;
}

Method::Method( const QMetaMethod &metaMethod, DocumentationParser *parser )
        : metaMethod( metaMethod )
{
    const QString signature = metaMethod.signature();
    name = signature.left( signature.indexOf( '(' ) );

    returnType = parser->cToQtScriptTypeName( metaMethod.typeName() );
    for( int p = 0; p < metaMethod.parameterNames().count(); ++p ) {
        const QString type = parser->cToQtScriptTypeName( metaMethod.parameterTypes()[p] );
        const QString parameterName = metaMethod.parameterNames()[p];
        parameters << parameterName;
        typedParameters << QString( "%1 %2" ).arg( type ).arg( parameterName );
        templatedParameters << QString( "${%1}" ).arg( parameterName );
    }
}

bool methodLessThan( const Method &method1, const Method &method2 ) {
    if ( method1.metaMethod.methodType() == method2.metaMethod.methodType() ) {
        // Sort by name if method type is the same
        return QString::compare( method1.name, method2.name ) < 0;
    } else {
        // Sort normal methods to the top, then signals, slots
        return method1.metaMethod.methodType() == QMetaMethod::Method ||
                (method1.metaMethod.methodType() < method2.metaMethod.methodType());
    }
}

bool enumerationLessThan2( const EnumComment &enum1, const EnumComment &enum2 )
{
    // Sort by name
    return QString::compare( enum1.name, enum2.name ) < 0;
}

bool enumerableLessThan( const EnumerableComment &enumerable1,
                         const EnumerableComment &enumerable2 )
{
    // Sort by value
    return enumerable1.value < enumerable2.value;
}

ClassInformation::ClassInformation( const QMetaObject &object, const QString &className,
                                    const Comment &classComment, const Methods &methods,
                                    const EnumComments &enums, const QString &scriptObjectName )
        : metaObject(object), className(className), scriptObjectName(scriptObjectName),
          comment(classComment), methods(methods), enums(enums)
{
    foreach ( const Method &method, methods ) {
        methodNames << method.name;
    }

    sortedMethods << methods.values();
    qSort( sortedMethods.begin(), sortedMethods.end(), methodLessThan );

    sortedEnums << enums.values();
    qSort( sortedEnums.begin(), sortedEnums.end(), enumerationLessThan2 );
}

void EnumComment::sort()
{
    sortedEnumerables = enumerables.values();
    qSort( sortedEnumerables.begin(), sortedEnumerables.end(), enumerableLessThan );
}
