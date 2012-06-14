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

#include "documentationparser.h"

#include <iostream>

DocumentationParser::DocumentationParser( const QString &sourceFilePath )
         : m_sourceFilePath(sourceFilePath)
{
}

void DocumentationParser::addClass( const QMetaObject &metaObject, const QString &scriptObjectName )
{
    const QString className = QString( metaObject.className() ).remove( QRegExp("^\\w+::") );
    if ( !scriptObjectName.isEmpty() ) {
        m_scriptObjectNames.insert( className, scriptObjectName );
    }
    m_metaObjects.insert( className, metaObject );
}

void DocumentationParser::addEnum( const QString &name )
{
    m_enums << name;
}

bool classInformationLessThan( const ClassInformation &class1, const ClassInformation &class2 ) {
    return QString::compare( class1.className, class2.className ) < 0; // Sort by name
}

bool enumerationLessThan( const EnumComment &enum1, const EnumComment &enum2 ){
    return QString::compare( enum1.name, enum2.name ) < 0; // Sort by name
}

void DocumentationParser::parse()
{
    if ( !m_classInformations.isEmpty() ) {
        qDebug() << "Is already parsed";
        return;
    }

    // Parse the source file
    m_classParseResults = parseDocumentation();

    // Create and sort class information structures
    for ( QHash< QString, QMetaObject >::ConstIterator it = m_metaObjects.constBegin();
          it != m_metaObjects.constEnd(); ++it )
    {
        m_classInformations << parseClass( it.value() );
    }
    qSort( m_classInformations.begin(), m_classInformations.end(), classInformationLessThan );

    // Create and sort enumeration information structures
    for ( EnumComments::Iterator it = m_classParseResults.enumComments.begin();
          it != m_classParseResults.enumComments.end(); ++it )
    {
        if ( m_enums.contains(it->name) ) {
            it->sort();
            m_globalEnumComments << *it;
        }
    }
    qSort( m_globalEnumComments.begin(), m_globalEnumComments.end(), enumerationLessThan );
}

QString DocumentationParser::scriptObjectName( const QString &className ) const
{
    return m_scriptObjectNames.contains( className )
            ? m_scriptObjectNames[ className ] : className;
}

ClassInformation DocumentationParser::parseClass( const QMetaObject &object )
{
    const QString className = QString( object.className() ).remove( QRegExp("^\\w+::") );
    const QString objectName = scriptObjectName( className );

    if ( !m_classParseResults.classComments.contains(className) ) {
        qDebug() << "Class not found in parsed source:" << className;
    }
    const Methods methods = getMethods( object,
            m_classParseResults.classComments[className].methodComments );

    if ( m_classParseResults.classComments.contains(className) ) {
        m_classParseResults.classComments.insert( objectName,
                m_classParseResults.classComments[className] );
    }
    return ClassInformation( object, className,
                             m_classParseResults.classComments[className].comment, methods,
                             m_classParseResults.classComments[className].enumComments,
                             objectName );
}

QString DocumentationParser::cToQtScriptTypeName( const QString &cTypeName )
{
    if( cTypeName == QLatin1String("QString") ||
        cTypeName == QLatin1String("QByteArray") )
    {
        return "string";
    } else if( cTypeName == QLatin1String("QVariantMap") ) {
        return "object";
    } else if( cTypeName == QLatin1String("QVariantList") ||
               cTypeName == QLatin1String("QStringList") ||
               cTypeName.startsWith(QLatin1String("QList")) )
    {
        return "list";
    } else if( cTypeName == QLatin1String("QDateTime") ||
               cTypeName == QLatin1String("QDate") ||
               cTypeName == QLatin1String("QTime") )
    {
        return "date";
    } else if( cTypeName == QLatin1String("QVariant") ) {
        return "any";
    } else if( cTypeName == QLatin1String("NetworkRequest*") ) {
        return "NetworkRequest";
    } else if( cTypeName == QLatin1String("Feature") ) {
        return "enum.feature"; // Enums are available under the enum object
    } else if( cTypeName == QLatin1String("Hint") ) {
        return "enum.hint";
    } else if( cTypeName == QLatin1String("int") ||
               cTypeName == QLatin1String("uint") ||
               cTypeName == QLatin1String("bool") )
    {
        return cTypeName; // unchanged
    } else if( cTypeName == QLatin1String("void") || cTypeName.isEmpty() ) {
        return "void";
    } else {
        qWarning() << "Type unknown " << cTypeName;
        return cTypeName;
    }
}

bool DocumentationParser::checkMethod( const QMetaMethod &method )
{
    return (method.access() == QMetaMethod::Public && method.methodType() == QMetaMethod::Method) ||
           (method.access() == QMetaMethod::Public && method.methodType() == QMetaMethod::Slot) ||
           (method.methodType() == QMetaMethod::Signal);
}

bool DocumentationParser::readUntil( QIODevice *dev, const QString &str )
{
    while( !dev->atEnd() ) {
        const QString line = QString( dev->readLine() ).trimmed();
        if( line.startsWith( str ) ) {
            return true; // str found
        }
    }
    return false; // str not found
}

DocumentationParser::ParseResults DocumentationParser::parseDocumentation()
{
    ParseResults result;
    if ( m_sourceFilePath.isEmpty() ) {
        // No file path to the source file given in the constructor
        return result;
    }

    // Open the source file
    QFile sourceFile( m_sourceFilePath );
    if ( !sourceFile.open(QIODevice::ReadOnly | QIODevice::Text) ) {
        qCritical() << "Could not open source file " << m_sourceFilePath;
        return result;
    }

    // Read source file line by line
    int lineNumber = 0;
    ParseContext context;
    while ( !sourceFile.atEnd() ) {
        // Parse one block of documentation
        context = parseDocumentationBlock( &sourceFile, &lineNumber, context, &result );
    }
    sourceFile.close();
    return result;
}

Comments DocumentationParser::parseGlobalDocumentation( QIODevice *dev )
{
    const bool closeAfterReading = !dev->isOpen();
    if ( !dev->isOpen() && !dev->open(QIODevice::ReadOnly | QIODevice::Text) ) {
        qCritical() << "Could not open source file:" << dev->errorString();
        return Comments();
    }

    // Read source file line by line
    int lineNumber = 0;
    ParseContext context;
    ParseResults results;
    while ( !dev->atEnd() ) {
        // Parse one block of documentation
        context = parseDocumentationBlock( dev, &lineNumber, context, &results );
    }

    if ( closeAfterReading ) {
        dev->close();
    }
    return results.globalComments;
}

void ParseInfo::startNewLine( const QString &line )
{
    this->line = line;
    this->line.replace( "\\<", "&lt;" ).replace( "\\>", "&gt;" );
    newComment = 0;
    lastCommentClosed = false;
    ++lineNumber;
}

void ParseInfo::closeOpenComment()
{
    if ( lastCommentClosed ) {
        if ( unclosedComment ) {
            if ( unclosedList ) {
                // Close list by appending an end list marker
                QString prefix;
                if ( unclosedBeginCommand == DoxygenListItem ) {
                    // Close last list item in the list
                    prefix += MarkerPair::fromCommand( DoxygenListItem, true );
                }
                unclosedComment->appendCommentLine( prefix +
                                                    MarkerPair::fromCommand(DoxygenEndList) );
                unclosedList = false;
            }

            // There is an unclosed doxygen comment and now it got closed
            switch ( unclosedComment->type() ) {
            case DoxygenParam:
                comment.parameters << dynamic_cast<DoxygenCommentWithArguments*>( unclosedComment );
                break;
            case DoxygenReturn:
                comment.returns = unclosedComment->comment();
                break;
            default:
                comment.otherComments << unclosedComment;
                break;
            }

            unclosedComment = 0;
        }
        lastCommentClosed = false;

        // Last unclosed comment was closed but there may be a new one
        unclosedComment = newComment;
    } else if ( newComment ) {
        unclosedComment = newComment;
    }
}

QString ParseInfo::parseInlineCommands() {
    int pos = 0;
    QRegExp doxygenInlineCommandRegExp( "(?:@|\\\\)(\\w+)(?:\\s+([\"\\.'_-\\w\\d]+))?(?:(\\s+[\"\\.'_-\\w\\d]+))?" );
    while ( (pos = doxygenInlineCommandRegExp.indexIn(line, pos)) != -1 ) {
        // Get found doxygen command name and check for a known inline doxygen command
        QString commandName = doxygenInlineCommandRegExp.cap(1);
        DoxygenCommandType type = typeFromString( commandName );
        QString inWordAfterCommandName;
        if ( type == UnknownDoxygenCommand ) {
            // Some commands can stand in a word without spaces before or after it,
            // eg. @verbatim, @endverbatim (flag DoxygenCommandInWord)
            const QString word = commandName;
            type = typeFromBeginningOfString( &commandName ); // Updates commandName
            if ( type != UnknownDoxygenCommand ) {
                // Store the part of the word with the command name in it, that does not
                // belong to the command name, eg. "@verbatimint" => "int"
                inWordAfterCommandName = word.mid( commandName.length() );
            }
        }
        DoxygenCommandFlags flags = flagsFromCommand( type );

        // Test flags of found command
        if ( flags.testFlag(DoxygenCommandInline) &&
             (flags.testFlag(DoxygenCommandExpectsArgument) ||
              flags.testFlag(DoxygenCommandExpectsTwoArguments) ||
              flags.testFlag(DoxygenCommandIsSection)) )
        {
            // Found an inline doxygen command, insert markers
            // TODO Use argument or line as value
            MarkerPair markers = MarkerPair::fromInlineCommand( type );
            if ( flags.testFlag(DoxygenCommandIsSection) ) {
                // Found a section command
                // "@<section-command> <title until line end>\n"
                const QString id = doxygenInlineCommandRegExp.cap(2);
                const int titleBegin = doxygenInlineCommandRegExp.pos(3);
                const int replaceLength = line.length() - doxygenInlineCommandRegExp.pos();
                const QString title = line.mid( titleBegin );
                line.replace( pos, replaceLength - inWordAfterCommandName.length(),
                              markers.begin + "ID=" + id + '%' + title + markers.end );
            } else if ( flags.testFlag(DoxygenCommandExpectsTwoArguments) ) {
                // Expects two arguments, currently only the second gets used (for @image)
                // "@image <argument1> <argument2>"
                const QString argument1 = doxygenInlineCommandRegExp.cap(2);
                const QString argument2 = doxygenInlineCommandRegExp.cap(3);
                line.replace( pos, doxygenInlineCommandRegExp.matchedLength() -
                              inWordAfterCommandName.length(),
                              markers.begin + /*argument1 + ' ' +*/ argument2 + markers.end );
            } else {
                // Expects one argument, a matched second argument of the regular expression
                // is not removed from line
                // "@<command> <argument>"
                const QString argument = doxygenInlineCommandRegExp.cap(2);
                const QString wordAfterArgument = doxygenInlineCommandRegExp.cap(3);
                line.replace( pos, doxygenInlineCommandRegExp.matchedLength() -
                              wordAfterArgument.length() - inWordAfterCommandName.length(),
                              markers.begin + argument + markers.end );
            }
        } else if ( flags.testFlag(DoxygenCommandBegin) ) {
            // Found a doxygen begin command, eg. @verbatim
            // TODO error handling
            // Store the new opened, ie. unclosed begin command type
            unclosedBeginCommand = type;

            // Check if it is an inline command
            if ( flags.testFlag(DoxygenCommandInline) ) {
                // An inline doxygen begin command, eg. @verbatim..@endverbatim
                // Replace by a marker (when the end command is read, it gets replaced by
                // an end marker)
                line.replace( pos, commandName.length() + 1, MarkerPair::fromCommand(type) );
            } else {
                // Not an inline doxygen begin command, eg. @code..@endcode
                // End unclosed comment, if any
                lastCommentClosed = true;

                // Remove the begin command name and the rest of the line as first line of
                // the multiline begin-end part
                line.remove( pos, commandName.length() + 1 );
                const QString firstLine = MarkerPair::fromCommand(type) + line.mid( pos + 1 );
                line = line.left( pos );

                // Start a new begin-end-command
                newComment = DoxygenComment::createDoxygenComment( firstLine, type );
            }
        } else if ( flags.testFlag(DoxygenCommandEnd) ) {
            // Found a doxygen end command, eg. @endverbatim
            if ( unclosedBeginCommand == InvalidDoxygenCommand ) {
                qWarning() << "Inline doxygen end command" << type
                           << "found without a begin command at line"
                           << lineNumber << line;
            } else if ( !beginMatchesEnd(unclosedBeginCommand, type) ) {
                qWarning() << "Last doxygen begin command" << unclosedBeginCommand
                           << "does not match current doxygen end command"
                           << commandName << "at line" << lineNumber << line;
            } else if ( flags.testFlag(DoxygenCommandInline) ) {
                // There is a matching doxygen begin command for the found end command
                // and it is an inline begin-end command
                // Replace by a marker
                line.replace( pos, commandName.length() + 1,
                                         MarkerPair::fromCommand(type) );
                unclosedBeginCommand = InvalidDoxygenCommand;
            } else {
                // There is a matching doxygen begin command for the found end command
                // but it is not an inline begin-end command
                lastCommentClosed = true;
                unclosedBeginCommand = InvalidDoxygenCommand;
            }
        } else if ( type == DoxygenNewline ) {
            if ( unclosedComment ) {
                // Append new line to unclosed comments
                line.replace( pos, commandName.length() + 1,
                              MarkerPair::fromCommand(DoxygenNewline) );
            } else {
                // Or create a new comment paragraph starting with a new line
                newComment = DoxygenComment::createDoxygenComment(
                        MarkerPair::fromCommand(DoxygenNewline) );
            }
        } else if ( type != UnknownDoxygenCommand &&
                    !flagsFromCommand(unclosedBeginCommand).testFlag(DoxygenCommandVerbatim) &&
                    doxygenInlineCommandRegExp.pos() > 0 )
        {
            const QString text = line.left( doxygenInlineCommandRegExp.pos() );
            if ( !text.trimmed().isEmpty() ) {
                // Found a non-inline command which does not begin at position 0
                if ( unclosedComment ) {
                    // Append new line to unclosed comments
                    line.replace( pos, commandName.length() + 1, text );
                } else {
                    // Or create a new comment paragraph starting with a new line
                    unclosedComment = DoxygenComment::createDoxygenComment( text );
                }

                // Return the found non-inline command as new line
                lastCommentClosed = true;
                closeOpenComment();
                line = line.mid( doxygenInlineCommandRegExp.pos() );
                break;
            }
        }

        pos += doxygenInlineCommandRegExp.matchedLength() - inWordAfterCommandName.length();
    }

    return QString();
}

void ParseInfo::parseNoninlineCommands()
{
    // Regular expressions to find non-inline doxygen commands
    QRegExp doxygenCommandRegExp( "^\\s*(?:@|\\\\)(\\w+)\\s+(.+)$" );
    QRegExp doxygenSingleCommandRegExp( "^\\s*(?:@|\\\\)(\\w+)$" ); // Without argument

    if ( line.trimmed().isEmpty() ) {
        // Empty lines close multiline doxygen commands from previous lines,
        // but only if there is no unclosed begin command, eg. @code
        if ( unclosedBeginCommand == InvalidDoxygenCommand ) {
            lastCommentClosed = unclosedComment != 0;
        } else if ( unclosedComment &&
                    flagsFromCommand(unclosedBeginCommand)
                    .testFlag(DoxygenCommandVerbatim) )
        {
            // Append new line to unclosed verbatim comments
            unclosedComment->appendCommentLine( "\n" );
        }
    } else if ( doxygenCommandRegExp.indexIn(line) != -1 ) {
        // A doxygen command over a whole line was found (and maybe more lines following)
        const DoxygenCommandType type = typeFromString( doxygenCommandRegExp.cap(1) );
        const DoxygenCommandFlags flags = flagsFromCommand( type );
        const QString commentText = doxygenCommandRegExp.cap(2);

        // Test if the found command is an inline command
        if ( flags.testFlag(DoxygenCommandInline) ) {
            QString prefix;
            if ( type == DoxygenListItem && !unclosedList ) {
                // First list item, insert begin list marker
                if ( !flags.testFlag(DoxygenCommandVerbatim) ) {
                    prefix += MarkerPair::fromCommand( DoxygenBeginList );
                }
                unclosedList = true;
            } // else: Following list items

            if ( unclosedComment ) {
                if ( !flags.testFlag(DoxygenCommandVerbatim) ) {
                    // Close unclosed begin-end command, if any
                    if ( unclosedBeginCommand != InvalidDoxygenCommand ) {
                        prefix += MarkerPair::fromCommand( type, true );
                    }

                    // Open inline comment
                    prefix += MarkerPair::fromCommand( type, false );
                }

                // Append new inline command text with markers
                // to the current unclosed command
                unclosedComment->appendCommentLine( prefix + commentText );
            } else {
                // Open inline comment in new standard comment
                if ( !flags.testFlag(DoxygenCommandVerbatim) ) {
                    prefix += MarkerPair::fromCommand( type, false );
                }
                newComment = DoxygenComment::createDoxygenComment( prefix + commentText );
                lastCommentClosed = true;
            }
        } else {
            // Create new doxygen command object
            newComment = DoxygenComment::createDoxygenComment( commentText, type );
            if ( newComment && !newComment->isMultiline() ) {
                // Found a new known single line doxygen comment
                // DoxygenBrief is currently the only supported non-inline command
                // without multiple lines
                if ( type == DoxygenBrief ) {
                    comment.brief = newComment->comment();
                } // TODO else { delete newDoxygenComment; }
                newComment = 0;
            }

            // New non-inline doxygen commands close multiline doxygen commands
            // from previous lines
            lastCommentClosed = unclosedComment != 0;
        }
    } else if ( doxygenSingleCommandRegExp.indexIn(line) != -1 ) {
        // A single doxygen command was found in the line without arguments
        // Ignore it
    } else if ( unclosedComment ) {
        // Not a doxygen command line and not an empty line, but there is an unclosed
        // doxygen command from a previous line, which gets continued in this line
        unclosedComment->appendCommentLine( line );
    } else {
        // Begin of a normal new comment, no unclosed comments
        newComment = DoxygenComment::createDoxygenComment( line );
        lastCommentClosed = true;
    }
}

DocumentationParser::ParseContext DocumentationParser::parseDocumentationBlock(
        QIODevice *dev, int *lineNumber, const ParseContext &context, ParseResults *parseResults )
{
    const QLatin1String namePattern( "[A-Za-z_][A-Za-z_0-9]+" );
    QRegExp methodSignatureRegExp(
        // Q_INVOKABLE          |return type|
        QString("(?:Q_INVOKABLE\\s+)?%1"
        // <template args, pointer?>    |   pointer? |method name|   '('
        "(?:\\s*<\\s*%1\\s*\\*?\\s*>\\s*)?\\s*\\*?\\s*(\\b%1\\b)\\s*\\(")
        .arg(namePattern) );
    QRegExp classRegExp( "class\\s+(" + namePattern + ")\\b" );
    QRegExp enumRegExp( "enum\\s+(" + namePattern + ")\\b" );
    QRegExp enumerableRegExp( "\\b(" + namePattern + ")\\b(?:\\s*=\\s*([^,\\n\\s]+)\\s*)?,?" );
    QRegExp commentStarCleanerRegExp( "^\\s*(?:\\*|/\\*\\*)" );

    // Read next line
    ParseInfo parseInfo( *lineNumber );
    parseInfo.startNewLine( dev->readLine() );

    // Used to support "/**<"
    QString preDeclaredEnumerable;
    QString preDeclaredEnumerableValue;

    // Find the next doxygen comment block in the source file, if any
    bool foundComment = false;
    while ( !dev->atEnd() ) {
        if ( parseInfo.line.startsWith("};") ) {
            // End of class/global enum declaration found (important not to trim line before here)
            break;
        } else if ( context.isInClassEnum() && parseInfo.line.trimmed().startsWith("};") ) {
            // Enumeration inside a class closed
            break;
        } else if ( parseInfo.line.trimmed().startsWith("/**") ) {
            // Beginning of a doxygen comment block found
            foundComment = true;
            break;
        } else if ( context.isInEnum() && enumerableRegExp.indexIn(parseInfo.line) != -1 ) {
            // Search for "/**<" after enumerable
            forever {
                if ( parseInfo.line.contains("/**<") ) {
                    // "/**<" directly follows the enumerable
                    preDeclaredEnumerable = enumerableRegExp.cap(1);
                    preDeclaredEnumerableValue = enumerableRegExp.cap(2);
                    foundComment = true;

                    // Cut enumerable declaration and "/**<"
                    parseInfo.line.remove( 0, parseInfo.line.indexOf("/**") + 4 );
                    break;
                } else if ( !parseInfo.line.isEmpty() ) {
                    // Not found
                    break;
                }
                parseInfo.startNewLine( dev->readLine() );
            }
        }

        // Read next line
        if ( preDeclaredEnumerable.isEmpty() ) {
            parseInfo.startNewLine( dev->readLine() );
        } else {
            break;
        }
    }
    if ( !foundComment ) {
        // Comment not found or class/enum declaration closed
        *lineNumber = parseInfo.lineNumber;

        ParseContext newContext = context;
        if ( newContext.isInEnum() ) {
            // Enum declaration closed, sort enumerables
            if ( newContext.isInClass() ) {
                parseResults->classComments[ context.className ].enumComments[ context.enumName ].sort();
            } else {
                parseResults->enumComments[ context.enumName ].sort();
            }
            newContext.enumName.clear();
        } else if ( newContext.isInClass() ) {
            // Class declaration closed
            newContext.className.clear();
        }
        return newContext;
    }

    // Read the doxygen documentation block
    while ( !dev->atEnd() ) {
        // Check for the end of the documentation block
        if ( parseInfo.line.endsWith('\n') ) {
            // Remove new line at the end the line
            parseInfo.line.remove( parseInfo.line.count() - 1, 1 );
        }
        const bool blockEnded = parseInfo.line.trimmed().endsWith("*/");
        if ( blockEnded ) {
            // End of multiline comment found
            // Remove closing "*/" or "**/" and parse the last line
            parseInfo.line.remove( QRegExp("\\*?\\*/\\s*$") );



        }
        parseInfo.line.remove( commentStarCleanerRegExp );
        Q_ASSERT( parseInfo.newComment == 0 && !parseInfo.lastCommentClosed );

        // Search for inline doxygen commands in the current line
        /*const QString foundNonInlineCommand = */parseInfo.parseInlineCommands();

        // Search for non-inline comments
        if ( !parseInfo.lastCommentClosed && !parseInfo.newComment ) {
            parseInfo.parseNoninlineCommands();
        }

        // Close unclosed comments
        parseInfo.closeOpenComment();

        if ( blockEnded ) {
            break;
        }

        // Read next line
        parseInfo.startNewLine( dev->readLine() );
    }

    if ( parseInfo.unclosedBeginCommand ) {
        qWarning() << "No closing doxygen command for command" << parseInfo.unclosedBeginCommand
                   << "found";
    }
    parseInfo.lastCommentClosed = true;
    parseInfo.closeOpenComment();

    // Find method signature directly after last found comment block
    bool foundEnumerable = false;
    if ( preDeclaredEnumerable.isEmpty() ) {
        parseInfo.startNewLine( dev->readLine().trimmed() );

        if ( methodSignatureRegExp.indexIn(parseInfo.line) != -1 ) {
            // Get method name and parameters as string using the matched regexp
            QString methodName = methodSignatureRegExp.cap(1);
            QString parameterString = parseInfo.line.mid( methodSignatureRegExp.pos() +
                                                        methodSignatureRegExp.matchedLength() - 1 );

            // Append following lines that belong to the method signature.
            // Simply count all '(' and ')' and stop when the numbers equal.
            int opening, closing;
            while ( (opening = parameterString.count('(')) > (closing = parameterString.count(')')) ) {
                // Read next line
                parseInfo.startNewLine( dev->readLine().trimmed() );

                // Append new line to the parameters string
                parameterString += parseInfo.line;
            }

            // Cut everything after the last ')'
            const int lastClosing = parameterString.lastIndexOf( ')' );
            parameterString = parameterString.left( lastClosing + 1 );

            // Remove opening/closing '(' / ')'
            parameterString = parameterString.mid( 1, parameterString.length() - 2 ).trimmed();

            // Split parameter list (NOTE won't work with template types..)
            QStringList parameterList = parameterString.split( ',' );
            QStringList cleanedParameterList;

            // Create new parameter list, remove everything except the type names and pointer '*'s if any
            QRegExp regExp( "^\\s*(?:const\\s+)?([a-zA-Z_][a-zA-Z0-9_]*)\\s*(\\*)?" );
            foreach ( const QString &parameter, parameterList ) {
                if ( regExp.indexIn(parameter) != -1 ) {
                    cleanedParameterList << (regExp.cap(1) + regExp.cap(2));
                }
            }

            // Join cleaned parameters gain, create a signature with the method name and normalize it.
            // The resulting signature should match the ones from QMetaMethod::signature().
            parameterString = cleanedParameterList.join(",");
            QString signature = methodName + '(' + parameterString + ')';
            signature = QMetaObject::normalizedSignature( signature.toUtf8() );
            parseResults->classComments[ context.className ].methodComments.insert(
                    signature, parseInfo.comment );

            *lineNumber = parseInfo.lineNumber;
            return context; // Still in the same class declaration
        } else if ( classRegExp.indexIn(parseInfo.line) != -1 ) {
            // Found a class declaration
            ParseContext newContext = context;
            newContext.className = classRegExp.cap(1);
            parseResults->classComments.insert( newContext.className, parseInfo.comment );

            *lineNumber = parseInfo.lineNumber;
            return newContext; // Return context with updated class context
        } else if ( enumRegExp.indexIn(parseInfo.line) != -1 ) {
            // Found an enum declaration
            ParseContext newContext = context;
            newContext.enumName = enumRegExp.cap(1); // Update context
            EnumComment enumComment( newContext.enumName, parseInfo.comment.brief,
                                    parseInfo.comment.otherComments );
            if ( newContext.isInClass() ) {
                // Enumeration inside a class
                parseResults->classComments[ newContext.className ].enumComments.insert(
                        newContext.enumName, enumComment );
            } else {
                // Global enumeration
                parseResults->enumComments.insert( newContext.enumName, enumComment );
            }

            *lineNumber = parseInfo.lineNumber;
            return newContext; // Return context with updated enum context
        } else if ( context.isInEnum() && enumerableRegExp.indexIn(parseInfo.line) != -1 ) {
            // Found an enumerable declaration after a comment block
            foundEnumerable = true;
        } else if ( parseInfo.line.isEmpty() ) {
            // No declaration found after comment block, add to global comments
            parseResults->globalComments << parseInfo.comment;
            *lineNumber = parseInfo.lineNumber;
            return context;
        }
    } else if ( context.isInEnum() && !preDeclaredEnumerable.isEmpty() ) {
        // Enumerable comment block is referring to previous enumerable declaration ("/**<")
        foundEnumerable = true;
    }

    if ( foundEnumerable ) {
        EnumComment *enumComment = 0;
        if ( context.isInClass() ) {
            // Found an enumerable of an enum inside a class
            enumComment = &(parseResults->classComments[ context.className ].enumComments[ context.enumName ]);
//                     .enumerables[ enumerableName ] = enumerableComment;
        } else {
            // Found an enumerable of a global enum
            enumComment = &(parseResults->enumComments[ context.enumName ]);
//             parseResults->enumComments[ context.enumName ].enumerables[ enumerableName ]
//                     = enumerableComment;
        }

        const QString enumerableName = preDeclaredEnumerable.isEmpty()
                ? enumerableRegExp.cap(1) : preDeclaredEnumerable;
        QString enumerableValueString = (preDeclaredEnumerableValue.isEmpty()
                ? enumerableRegExp.cap(2) : preDeclaredEnumerableValue).trimmed();
        int enumerableValue;
        if ( enumerableValueString.startsWith("0x") ) {
            enumerableValueString.remove( 0, 2 );
            enumerableValue = enumerableValueString.toInt( 0, 16 );
        } else if ( enumerableValueString.isEmpty() ) {
            enumerableValue = enumComment->lastEnumerableValue + 1;
        } else {
            enumerableValue = enumerableValueString.toInt();
        }
        EnumerableComment enumerableComment( enumerableName, enumerableValue,
                                             parseInfo.comment.brief,
                                             parseInfo.comment.otherComments );
        enumComment->enumerables[ enumerableName ] = enumerableComment;
//         if ( context.isInClass() ) {
//             // Found an enumerable of an enum inside a class
//             parseResults->classComments[ context.className ].enumComments[ context.enumName ]
//                     .enumerables[ enumerableName ] = enumerableComment;
//         } else {
//             // Found an enumerable of a global enum
//             parseResults->enumComments[ context.enumName ].enumerables[ enumerableName ]
//                     = enumerableComment;
//         }
    } else if (
        // Constructors
        parseInfo.line.indexOf(QRegExp("^\\s*\\w+\\s*\\(")) == -1 &&
        // Destructors
        parseInfo.line.indexOf(QRegExp("^\\s*(?:virtual\\s*)?~\\w+\\s*\\(")) == -1 &&
        // Constants
        parseInfo.line.indexOf(QRegExp("^\\s*static\\s+const\\s+(?:u?int|char\\s*\\*?)\\s*\\w+(?:\\s*=)?")) == -1 )
    {
        qDebug() << "Unknown declaration found after comment block in line"
                 << parseInfo.lineNumber << parseInfo.line;
    }

    *lineNumber = parseInfo.lineNumber;
    return context;
}

Methods DocumentationParser::getMethods( const QMetaObject &obj, const MethodComments &comments )
{
    Methods methods;
    for( int i = obj.methodOffset(); i < obj.methodCount(); ++i ) {
        QMetaMethod metaMethod = obj.method( i );
        if( !DocumentationParser::checkMethod(metaMethod) ) {
            // Normal method is not public or method is a constructor
            continue;
        }
        Method method( metaMethod, this );
        const QString signature = method.metaMethod.signature();
        if ( methods.contains(signature) ) {
            qDebug() << "Ambiguous method signatures found" << signature;
            continue;
        }
        if ( comments.contains(signature) ) {
            method.comment = comments.value( signature );
            methods.insert( signature, method );
        } else {
            // No comment found for signature, maybe it is just different versions of the same method
            // with default parameters
        }
    }
    return methods;
}
