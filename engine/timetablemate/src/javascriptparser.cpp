/*
 *   Copyright 2012 Friedrich Pülz <fieti1983@gmx.de>
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

#include "javascriptparser.h"
#include "javascriptcompletiongeneric.h"

#include <QHash>
#include <KTextEditor/Cursor>
#include <KLocalizedString>

CodeNode::CodeNode( const QString& text, int line, int colStart, int colEnd ) : m_parent( 0 )
{
    m_text = text;
    m_line = line;
    m_col = colStart;
    m_colEnd = colEnd;

    m_id = text;
}

CodeNode* CodeNode::topLevelParent() const
{
    CodeNode *node = const_cast<CodeNode*>( this );
    while ( node->parent() ) {
        node = node->parent();
    }
    return node;
}

bool CodeNode::isInRange( int lineNumber, int column ) const
{
    if ( lineNumber == m_line && lineNumber == endLine() ) {
        // lineNumber is in a one line node
        return column == -1 || (column >= m_col && column <= m_colEnd);
    } else if ( lineNumber == m_line ) {
        // lineNumber is at the beginning of a multiline node
        return column == -1 || column >= m_col;
    } else if ( lineNumber == endLine() )  {
        // lineNumber is at the end of a multiline node
        return column == -1 || column <= m_colEnd;
    } else if ( lineNumber >= m_line && lineNumber <= endLine() ) {
        // lineNumber is inside a multiline node
        return true;
    } else {
        return false;
    }
}

CodeNode *CodeNode::childFromPosition( int lineNumber, int column ) const
{
    QList< CodeNode::Ptr > childList = children();
    foreach ( const CodeNode::Ptr &child, childList ) {
        if ( child->isInRange(lineNumber, column) ) {
            return child->childFromPosition( lineNumber, column );
        }
    }

    if ( isInRange(lineNumber, column) ) {
        return const_cast< CodeNode* >( this );
    } else {
        return 0;
    }
}

JavaScriptParser::JavaScriptParser( const QString &code )
{
    m_code = code;
    m_hasError = false;
    m_errorLine = -1;
    m_errorColumn = 0;
    m_nodes = parse();
}

JavaScriptParser::~JavaScriptParser()
{
    qDeleteAll( m_token );
}

QString JavaScriptParser::Token::whitespacesBetween( const JavaScriptParser::Token* token1,
                                                     const JavaScriptParser::Token* token2 )
{
    QString whitespaces;
    int newLines = token2->line - token1->line;
    if ( newLines > 0 ) {
        while ( --newLines > 0 ) {
            whitespaces += '\n';
        }
        return whitespaces;
    }

    int spaces = token2->posStart - token1->posEnd;
    while ( --spaces > 0 ) {
        whitespaces += ' ';
    }
    return whitespaces;
}

bool JavaScriptParser::tryMoveToNextToken()
{
    moveToNextToken();
    if ( atEnd() ) {
        setErrorState( i18nc("@info/plain", "Unexpected end of file."),
                       m_lastToken->line, m_lastToken->posEnd );
        return false;
    } else {
        return true;
    }
}

void JavaScriptParser::moveToNextToken()
{
    m_lastToken = currentToken();
    ++m_it;
}

CodeNode::Ptr JavaScriptParser::parseComment()
{
    if ( atEnd() ) {
        return CommentNode::Ptr( 0 );
    }
    Token *curToken = currentToken();
    if ( !curToken->isChar('/') || !tryMoveToNextToken() ) {
        return CommentNode::Ptr( 0 );
    }

    QString text;
    if ( Token::whitespacesBetween(curToken, currentToken()).length() == 0 ) {
        if ( currentToken()->isChar('/') ) {
            for ( moveToNextToken(); !atEnd() && currentToken()->line == curToken->line;
                  moveToNextToken() )
                {
                text += whitespaceSinceLastToken();
                text += currentToken()->text;
            }
            return CommentNode::Ptr(
                    new CommentNode(text.trimmed(), curToken->line, curToken->posStart,
                                    m_lastToken->line, m_lastToken->posEnd) );
        } else if ( currentToken()->isChar('*') ) {
            // TODO: Remove '*' from the beginning of comment lines for [text]
            moveToNextToken();
            while ( !atEnd() ) {
                Token *nextCommentToken = currentToken();
                if ( nextCommentToken->isChar('*') ) {
                    moveToNextToken();
                    if ( atEnd() ) {
                        text += Token::whitespacesBetween( m_lastToken, nextCommentToken );
                        text += '*';
                        setErrorState( i18nc("@info/plain", "Unclosed multiline comment"),
                                       nextCommentToken->line, nextCommentToken->posEnd );
                        break;
                    } else if ( currentToken()->isChar('/') ) {
                        moveToNextToken();
                        return CommentNode::Ptr(
                                new CommentNode(text.trimmed(), curToken->line, curToken->posStart,
                                                m_lastToken->line, m_lastToken->posEnd) );
                    } else {
                        text += Token::whitespacesBetween( m_lastToken, nextCommentToken );
                        text += '*';
                    }
                } else {
                    text += Token::whitespacesBetween( m_lastToken, currentToken() );
                    text += currentToken()->text;
                    moveToNextToken();
                }
            } // while ( it != token.constEnd() )
        } else {
            --m_it; // go back to the first '/' that wasn't part of a comment, ie. '//' or '/*'
        }
    } else {
        --m_it; // go back to the first '/' that wasn't part of a comment, ie. '//' or '/*'
    }

    // No comment found
    return CommentNode::Ptr( 0 );
}

CodeNode::Ptr JavaScriptParser::parseBracketed()
{
    if ( atEnd() ) {
        return CodeNode::Ptr( 0 );
    }
    Token *beginToken = currentToken();
    m_lastToken = beginToken;
    // '{' is matched by parseBlock
    if ( !beginToken->isChar('(') && !beginToken->isChar('[') ) {
        return CodeNode::Ptr( 0 );
    }

    // Get the end character (closing bracket)
    QChar endChar = beginToken->text.at( 0 ) == '(' ? ')' : ']';

    Token *lastCommaToken = beginToken;
    moveToNextToken();
    QList< CodeNode::Ptr > children;
    QString text, textSinceLastComma;
    while ( !atEnd() ) {
        CodeNode::Ptr node( 0 );
        text += Token::whitespacesBetween( m_lastToken, currentToken() );
        // Check for end character
        if ( currentToken()->isChar(endChar) ) {
            if ( !textSinceLastComma.isEmpty() ) {
                children << CodeNode::Ptr(
                        new UnknownNode(textSinceLastComma, lastCommaToken->line,
                                        lastCommaToken->posStart, m_lastToken->posEnd) );
            }
            moveToNextToken();
            return BracketedNode::Ptr(
                    new BracketedNode(beginToken->text.at(0), text, beginToken->line,
                                      beginToken->posStart, m_lastToken->line, m_lastToken->posEnd,
                                      children) );
        } else if ( currentToken()->isChar('}') ) {
//             qDeleteAll( children );
            setErrorState( i18nc("@info/plain", "Unclosed bracket, expected '%1'.", endChar),
                beginToken->line, beginToken->posEnd );
            return CodeNode::Ptr( 0 );
        } else if ( (node = parseComment()) || (node = parseString()) || (node = parseBracketed())
            || (node = parseBlock()) || (node = parseFunction()) )
        {
            text += node->toString();
            children << node;
        } else if ( !atEnd() ) { // Could be at the end after the last else block (parse...)
            text += currentToken()->text;
            if ( currentToken()->isChar(',') ) {
                lastCommaToken = currentToken();
                textSinceLastComma.clear();
                children << CodeNode::Ptr( new UnknownNode(",", currentToken()->line,
                            currentToken()->posStart, currentToken()->posEnd) );
            } else {
                textSinceLastComma += currentToken()->text;
                children << CodeNode::Ptr( new UnknownNode(textSinceLastComma, lastCommaToken->line,
                                lastCommaToken->posStart, currentToken()->posEnd) );
            }
            moveToNextToken();
        }
    }

//     qDeleteAll( children );
    setErrorState( i18nc("@info/plain", "Unclosed bracket, expected '%1'.", endChar),
           beginToken->line, beginToken->posEnd );
    return CodeNode::Ptr( 0 );
}

CodeNode::Ptr JavaScriptParser::parseString()
{
    if ( atEnd() ) {
        return CodeNode::Ptr( 0 );
    }
    Token *beginToken = currentToken();
    m_lastToken = beginToken;
    if ( !beginToken->isChar('\"') && !beginToken->isChar('\'') ) {
        if ( !beginToken->isChar('/') ) {
            return CodeNode::Ptr( 0 ); // Not a reg exp and not a string
        }

        // Get the previous token
        if ( m_it != m_token.constBegin() ) {
            --m_it;
            Token *prevToken = currentToken();
            ++m_it;

            // If one of these comes before '/', it's the start of a regular expression
            QString allowed("=(:?");
            if ( prevToken->text.length() != 1 || !allowed.contains(prevToken->text) ) {
                // Not a reg exp and not a string
                return CodeNode::Ptr( 0 );
            }
        } else {
            return CodeNode::Ptr( 0 );
        }
    }
    // The end character is the same as the beginning character (", ' or / for reg exps)
    QChar endChar = beginToken->text.at( 0 );

    moveToNextToken();
    QString text;
    while ( !atEnd() ) {
        text += whitespaceSinceLastToken();
        // Check for non-escaped end character
        if ( currentToken()->isChar(endChar) && !m_lastToken->isChar('\\') ) {
            int columnEnd = currentToken()->posEnd;
            moveToNextToken();
            return CodeNode::Ptr( new StringNode(text, beginToken->line,
                                                 beginToken->posStart, columnEnd) );
        } else if ( (*m_it)->line != beginToken->line ) {
            if ( endChar == '/' ) {
                setErrorState( i18nc("@info/plain", "Unclosed regular expression, "
                                     "missing %1 at end.", endChar),
                               m_lastToken->line, m_lastToken->posEnd );
            } else {
                setErrorState( i18nc("@info/plain", "Unclosed string, "
                                     "missing %1 at end.", endChar),
                               m_lastToken->line, m_lastToken->posEnd );
            }
            return CodeNode::Ptr( 0 );
        } else {
            text += currentToken()->text;
            moveToNextToken();
        }
    }

    setErrorState( i18nc("@info/plain", "Unexpected end of file."),
                   m_lastToken->line, m_lastToken->posEnd );
    return CodeNode::Ptr( 0 );
}

void JavaScriptParser::checkFunctionCall( const QString &object, const QString &function,
        const BracketedNode::Ptr &bracketedNode, int line, int column )
{
    Q_UNUSED( bracketedNode );
//     QStringList timetableInfoStrings;
//     timetableInfoStrings << "DepartureDateTime" << "DepartureDate" << "DepartureTime"
//             << "TypeOfVehicle" << "TransportLine" << "FlightNumber" << "Target"
//             << "Platform" << "Delay" << "DelayReason" << "JourneyNews"
//             << "JourneyNewsOther" << "JourneyNewsLink" << "Operator" << "Status"
//             << "RouteStops" << "RouteTimes" << "RouteTimesDeparture"
//             << "RouteTimesArrival" << "RouteExactStops" << "RouteTypesOfVehicles"
//             << "RouteTransportLines" << "RoutePlatformsDeparture" << "IsNightLine"
//             << "RoutePlatformsArrival" << "RouteTimesDepartureDelay"
//             << "RouteTimesArrivalDelay" << "Duration" << "StartStopName"
//             << "StartStopID" << "TargetStopName" << "TargetStopID"
//             << "ArrivalDateTime" << "ArrivalDate" << "ArrivalTime" << "Changes"
//             << "TypesOfVehicleInJourney" << "Pricing" << "StopName" << "StopID"
//             << "StopWeight";
//
//     if ( !timetableInfoStrings.contains(string->content()) ) {
//         setErrorState( i18nc("@info/plain", "'%1' is not a valid info name.",
//                                 string->content()), string->line(), string->column() );
//     }

    QHash< QString, QStringList > methods;
    JavaScriptCompletionGeneric::addAvailableMethods( &methods );
    if ( methods.contains(object) && !methods[object].contains(function) ) {
        setErrorState( i18nc("@info/plain", "The object '%1' has no method '%2' "
                             "(available methods: %3).",
                       object, function, methods[object].join(", ")), line, column );
    }
}

CodeNode::Ptr JavaScriptParser::parseStatement()
{
    if ( atEnd() ) {
        return CodeNode::Ptr( 0 );
    }
    Token *firstToken = currentToken();
    if ( m_lastToken == firstToken ) {
        moveToNextToken();
        firstToken = currentToken();
    }
    m_lastToken = firstToken;

    QString text;
    QList<Token*> lastTokenList;
    QList< CodeNode::Ptr > children;
    while ( !atEnd() ) {
        if ( currentToken()->isChar(';') ) {
            // End of statement found
            if ( !lastTokenList.isEmpty() ) {
                text += Token::whitespacesBetween( lastTokenList.last(), currentToken() );
            }
            text += currentToken()->text;
            moveToNextToken();
            return CodeNode::Ptr(
                    new StatementNode(text, firstToken->line, firstToken->posStart,
                                      m_lastToken->line, m_lastToken->posEnd, children) );
        } else if ( currentToken()->isChar('}') ) {
            // '}' without previous '{' found in statement => ';' is missing
//             Token *errorToken = lastTokenList.isEmpty() ? currentToken() : lastTokenList.last();
//             setErrorState( i18nc("@info/plain", "Missing ';' at the end of the statement."),
//                 errorToken->line, errorToken->posEnd );
//             m_lastToken = errorToken;
//             if ( m_it != m_token.constBegin() ) {
//                 --m_it; // Go before the '}'
//             }
            // Found a closing '}', should be part of a block, parsed in parseBlock(),
            // which called this function to parse the block contents
            if ( !lastTokenList.isEmpty() ) {
                text += Token::whitespacesBetween( lastTokenList.last(), currentToken() );
            }
            // TODO Do not go to next token, because the current '}'-token should be read by a
            // calling function
//             moveToNextToken();
            return CodeNode::Ptr(
                    new StatementNode(text, firstToken->line, firstToken->posStart,
                                      m_lastToken->line, m_lastToken->posEnd, children) );
        }

        CodeNode::Ptr node( 0 );
        if ( (node = parseComment()) || (node = parseString()) ) {
            text += node->toString();
            children << node;
            lastTokenList << m_lastToken;
        } else if ( (node = parseBracketed()) ) {
            text += node->toString();
            BracketedNode::Ptr bracketedNode = node.dynamicCast<BracketedNode>();
            if ( bracketedNode->openingBracketChar() != '('
                || lastTokenList.count() != 3 || !lastTokenList.at(0)->isName
                || !lastTokenList.at(1)->isChar('.') || !lastTokenList.at(2)->isName )
            {
                children << node;
            } else {
                // Is a member function call "object.function(...)"
                Token *objectToken = lastTokenList.at(0);
                Token *pointToken = lastTokenList.at(1);
                Token *functionToken = lastTokenList.at(2);

                QString object = objectToken->text;
                QString function = functionToken->text;
                checkFunctionCall( object, function, bracketedNode,
                        objectToken->line, objectToken->posStart );
                // TODO This UnknownNode could get an ObjectNode or VariableNode
                CodeNode::Ptr objectNode( new UnknownNode(object + ".",
                    objectToken->line, objectToken->posStart, pointToken->posEnd) );
                children << objectNode;
                node = CodeNode::Ptr( new FunctionCallNode(object, function, functionToken->line,
                        functionToken->posStart, currentToken()->posStart, bracketedNode) );
                children << node;
            }
            lastTokenList << m_lastToken;
        } else if ( (node = parseBlock()) || (node = parseFunction()) ) {
            text += node->toString();
            children << node;
            lastTokenList << m_lastToken;

            if ( !atEnd() && currentToken()->isChar(';') ) {
                lastTokenList << currentToken();
                text += ';';
                moveToNextToken();
            }

            return CodeNode::Ptr( new StatementNode(text, firstToken->line, firstToken->posStart,
                    lastTokenList.last()->line, lastTokenList.last()->posEnd, children) );
        } else if ( atEnd() ) {
            setErrorState( i18nc("@info/plain", "Unexpected end of file.") );
        } else {
            if ( !lastTokenList.isEmpty() ) {
                text += Token::whitespacesBetween( lastTokenList.last(), currentToken() );
            }
            text += currentToken()->text;
            lastTokenList << currentToken();
            moveToNextToken();
        }
    }

    if ( !lastTokenList.isEmpty() ) {
        m_lastToken = lastTokenList.last();
        setErrorState( i18nc("@info/plain", "Unexpected end of file."),
                       lastTokenList.last()->line, lastTokenList.last()->posEnd );
    } else if ( m_lastToken ) {
        setErrorState( i18nc("@info/plain", "Unexpected end of file."),
                       m_lastToken->line, m_lastToken->posEnd );
    } else {
        setErrorState( i18nc("@info/plain", "Unexpected end of file."), -1, -1 );
    }
    return CodeNode::Ptr( 0 );
}

CodeNode::Ptr JavaScriptParser::parseFunction()
{
    if ( atEnd() ) {
        return CodeNode::Ptr( 0 );
    }
    m_lastToken = currentToken();
    if ( atEnd() ) {
        return CodeNode::Ptr( 0 );
    }

    Token *firstToken = currentToken();
    if ( firstToken->text != "function" || !tryMoveToNextToken() ) {
        return CodeNode::Ptr( 0 );
    }

    // Parse function name if any
    QString name;
    if ( !currentToken()->isChar('(') ) {
        name = currentToken()->text;
        if ( !tryMoveToNextToken() ) {
            return CodeNode::Ptr( 0 );
        }
    }

    // Parse arguments
    if ( currentToken()->isChar('(') ) {
        QList< ArgumentNode::Ptr > arguments;
        bool argumentNameExpected = true;
        bool isComma = false;
        if ( !tryMoveToNextToken() ) {
            return CodeNode::Ptr( 0 );
        }

        // Parse until a ')' is read or EOF
        while ( !atEnd() && !currentToken()->isChar(')') ) {
            isComma = currentToken()->isChar(',');
            if ( argumentNameExpected ) {
                if ( isComma ) {
                    setErrorState( i18nc("@info/plain", "Expected argument or ')'."),
                                currentToken()->line, currentToken()->posStart );
                    break;
                }

                arguments << ArgumentNode::Ptr(
                        new ArgumentNode(currentToken()->text, currentToken()->line,
                                         currentToken()->posStart, currentToken()->posEnd) );
            } else if ( !isComma ) {
                setErrorState( i18nc("@info/plain", "Expected ',' or ')'."),
                            currentToken()->line, currentToken()->posStart );
                break;
            }

            argumentNameExpected = !argumentNameExpected;
            if ( !tryMoveToNextToken() ) {
                return CodeNode::Ptr( 0 );
            }
        }

        if ( isComma ) { // argument list ended with ','
            setErrorState( i18nc("@info/plain", "Expected argument or ')'."),
                    m_lastToken->line, m_lastToken->posStart );
        }

        // Read definition block
        BlockNode::Ptr definition( 0 );
        if ( !tryMoveToNextToken() ) {
            return CodeNode::Ptr( 0 );
        }
        if ( /*tryMoveToNextToken() &&*/ !(definition = parseBlock()) ) {
            setErrorState( i18nc("@info/plain", "Function definition is missing."),
                    m_lastToken->line, m_lastToken->posStart );
        }

        return FunctionNode::Ptr( new FunctionNode(name, firstToken->line, firstToken->posStart,
                                               m_lastToken->posEnd, arguments, definition) );
    } else {
        setErrorState( i18nc("@info/plain", "Expected '('."),
                currentToken()->line, currentToken()->posStart );
        moveToNextToken();

        return CodeNode::Ptr( 0 );
    }
}

BlockNode::Ptr JavaScriptParser::parseBlock()
{
    if ( atEnd() ) {
        return BlockNode::Ptr( 0 );
    }

    if ( currentToken()->isChar('{') ) {
        Token *firstToken = currentToken();
        QList< CodeNode::Ptr > children;
    //     moveToNextToken();
        if ( !tryMoveToNextToken() ) {
            return BlockNode::Ptr( 0 );
        }
        while ( !atEnd() ) {
            CodeNode::Ptr node( 0 );
            if ( (node = parseComment())
                || (!m_hasError && (node = parseString()))
                || (!m_hasError && (node = parseBracketed()))
                || (!m_hasError && (node = parseFunction()))
                || (!m_hasError && (node = parseBlock())) )
            {
                children << node;
            } else if ( !atEnd() && currentToken()->isChar('}') ) {
                moveToNextToken(); // Move to next token, ie. first token after the function definition (or EOF)
                return BlockNode::Ptr( new BlockNode(firstToken->line, firstToken->posEnd,
                        m_lastToken->line, m_lastToken->posEnd, children) );
            } else if ( (node = parseStatement()) ) {
                children << node;
            } else if ( !atEnd() ) {
                moveToNextToken();
            }
        }

        setErrorState( i18nc("@info/plain", "Unclosed block, missing '}'. Block started "
                             "at line %1.", firstToken->line),
                       m_lastToken->line, m_lastToken->posEnd );
    }

    return BlockNode::Ptr( 0 );
}

QList< CodeNode::Ptr > JavaScriptParser::parse()
{
    clearError();

    // Get token from the code, with line number and column begin/end
    m_token.clear();
    QString alpha( "abcdefghijklmnopqrstuvwxyz" );
    QRegExp rxTokenBegin( "[^\\s]" );
    QRegExp rxTokenEnd( "\\s|[-=#!$%&~;:,<>^`´/\\.\\+\\*\\\\\\(\\)\\{\\}\\[\\]'\"\\?\\|]" );
    QStringList lines2 = m_code.split( '\n' );
    for ( int lineNr = 0; lineNr < lines2.count(); ++lineNr ) {
        QString line = lines2.at( lineNr );
        QStringList words;

        int posStart = 0;
        while ( (posStart = rxTokenBegin.indexIn(line, posStart)) != -1 ) {
            int posEnd;
            bool isName;
            // Only words beginning with a letter ([a-z]) may be concatenated
            if ( !alpha.contains(line.at(posStart).toLower()) ) {
                posEnd = posStart + 1;
                isName = false;
            } else {
                posEnd = rxTokenEnd.indexIn( line, posStart + 1 );
                isName = true;
            }
            if ( posEnd == -1 ) {
                posEnd = line.length();
            }

            QString word;
            word = line.mid( posStart, posEnd - posStart );
            if ( !word.isEmpty() ) {
                m_token << new Token( word, lineNr + 1, posStart, posEnd - 1, isName );
            }

            // Set minimal next start position
            posStart = posEnd;
        }
    }

    // Get nodes from the token
    QList< CodeNode::Ptr > nodes;
    m_it = m_token.constBegin();
    while ( !atEnd() ) {
        CodeNode::Ptr node( 0 );
        if ( (node = parseComment())
            || (!m_hasError && (node = parseString()))
            || (!m_hasError && (node = parseBracketed()))
            || (!m_hasError && (node = parseFunction()))
            || (!m_hasError && (node = parseBlock()))
            || (!m_hasError && (node = parseStatement())) )
        {
            nodes << CodeNode::Ptr(node);

            if ( m_hasError ) {
                break;
            }
        } else if ( !atEnd() ) {
            moveToNextToken();
        }
    } // for ( ...all tokens... )

    // Done with the tokens, delete them
    qDeleteAll( m_token );
    m_token.clear();

    // Check for multiple definitions
    QHash< QString, FunctionNode::Ptr > functions;
    foreach ( const CodeNode::Ptr &node, nodes ) {
        FunctionNode::Ptr function = node.dynamicCast<FunctionNode>();
        if ( function ) {
            QString newFunctionName = function->toString( true );
            if ( functions.contains(newFunctionName) ) {
                FunctionNode::Ptr previousImpl = functions.value( newFunctionName );
                setErrorState( i18nc("@info/plain", "Multiple definitions of function '%1', "
                                     "previously defined at line %2",
                                     function->text(), previousImpl->line()),
                               function->line(), function->column(), previousImpl->line() );
            } else {
                functions.insert( newFunctionName, function );
            }
        }
    }

    return nodes;
}

KTextEditor::Cursor JavaScriptParser::errorCursor() const
{
    return KTextEditor::Cursor( m_errorLine - 1, m_errorColumn );
}

void JavaScriptParser::setErrorState( const QString& errorMessage, int errorLine,
                      int errorColumn, int affectedLine )
{
    if ( m_hasError ) {
        return; // Don't override existing errors
    }

    m_hasError = true;
    m_errorLine = errorLine;
    m_errorAffectedLine = affectedLine;

    m_errorColumn = errorColumn;
    m_errorMessage = errorMessage;
}

void JavaScriptParser::clearError() {
    m_hasError = false;
    m_errorLine = -1;
    m_errorAffectedLine = -1;
    m_errorColumn = 0;
    m_errorMessage.clear();
}

MultilineNode::MultilineNode( const QString& text, int line, int colStart, int lineEnd, int colEnd )
        : CodeNode( text, line, colStart, colEnd )
{
    m_endLine = lineEnd;
}

ChildListNode::ChildListNode( const QString& text, int line, int colStart,
                              int lineEnd, int colEnd, const QList< CodeNode::Ptr > &children )
        : MultilineNode( text, line, colStart, lineEnd, colEnd ), m_children(children)
{
    foreach ( const CodeNode::Ptr &child, children ) {
        child->m_parent = this;
    }
}

EmptyNode::EmptyNode() : CodeNode( QString(), -1, 0, 0 )
{
}

StatementNode::StatementNode( const QString &text, int line, int colStart, int lineEnd, int colEnd,
                              const QList< CodeNode::Ptr > &children )
        : ChildListNode( text, line, colStart, lineEnd, colEnd, children )
{
}

BracketedNode::BracketedNode( const QChar &openingBracketChar, const QString &text,
                              int line, int colStart, int lineEnd, int colEnd,
                              const QList< CodeNode::Ptr > &children )
        : ChildListNode( text, line, colStart, lineEnd, colEnd, children )
{
    m_bracketChar = openingBracketChar;
}

QChar BracketedNode::closingBracketChar() const
{
    if ( m_bracketChar == '(' ) {
        return ')';
    } else if ( m_bracketChar == '[' ) {
        return ']';
    } else {
        return ' '; // should not happen
    }
}

int BracketedNode::commaSeparatedCount() const
{
    int count = 1;
    foreach ( const CodeNode::Ptr &child, m_children ) {
        if ( child->type() == UnknownNodeType && child->text() == QLatin1String(",") ) {
            ++count;
        }
    }
    return count;
}

QList< CodeNode::Ptr > BracketedNode::commaSeparated( int pos ) const
{
    QList< CodeNode::Ptr > separated;

    int curPos = 0;
    foreach ( const CodeNode::Ptr &child, m_children ) {
    if ( child->type() == UnknownNodeType && child->text() == QLatin1String(",") ) {
        ++curPos;
        if ( curPos > pos ) {
            return separated;
        }
    }

    if ( curPos == pos )
        separated << child;
    }

    return separated;
}

FunctionCallNode::FunctionCallNode( const QString &object, const QString &function, int line,
                                    int colStart, int colEnd, const BracketedNode::Ptr &arguments )
        : CodeNode( object + '.' + function, line, colStart, colEnd )
{
    m_object = object;
    m_function = function;
    m_arguments = arguments;
}

FunctionNode::FunctionNode( const QString& text, int line, int colStart, int colEnd,
                            const QList< ArgumentNode::Ptr > &arguments,
                            const BlockNode::Ptr &definition )
        : MultilineNode( text, line, colStart, definition ? definition->endLine() : line, colEnd ),
        m_arguments(arguments), m_definition(definition)
{
    if ( m_text.isEmpty() ) {
        m_text = i18nc("@info/plain Display name for anonymous JavaScript functions", "[anonymous]");
    }

    foreach ( const CodeNode::Ptr &child, arguments ) {
        child->m_parent = this;
    }
    if ( definition ) {
        definition->m_parent = this;
    }
}

QList< CodeNode::Ptr > FunctionNode::children() const
{
    QList< CodeNode::Ptr > ret;
    for ( QList<ArgumentNode::Ptr>::const_iterator it = m_arguments.constBegin();
          it != m_arguments.constEnd(); ++it )
    {
        ret << it->dynamicCast< CodeNode >();
    }
    if ( m_definition ) {
        ret << m_definition.dynamicCast< CodeNode >();
    }
    return ret;
}

BlockNode::BlockNode( int line, int colStart, int lineEnd, int colEnd,
                      const QList< CodeNode::Ptr > &children )
        : ChildListNode( QString(), line, colStart, lineEnd, colEnd, children )
{
}

QString BlockNode::toString( bool shortString ) const
{
    QString s( '{' );
    foreach ( const CodeNode::Ptr &child, m_children ) {
        s += child->toString(shortString) + '\n';
    }
    return s + '}';
}

QString FunctionNode::id() const
{
    QStringList arguments;
    foreach ( const CodeNode::Ptr &node, m_arguments ) {
        ArgumentNode::Ptr argument = node.dynamicCast< ArgumentNode >();
        if ( argument ) {
            arguments << argument->text();
        }
    }

    return QString("func:%1()").arg( m_text ); //, arguments.join(",") );
}

QString FunctionNode::toStringSignature() const
{
    QStringList arguments;
    foreach ( const CodeNode::Ptr &node, m_arguments ) {
        ArgumentNode::Ptr argument = node.dynamicCast< ArgumentNode >();
        if ( argument ) {
            arguments << argument->text();
        }
    }

    return QString("%1( %2 )").arg( m_text, arguments.join(", ") );
}

QString FunctionNode::toString( bool shortString ) const
{
    if ( shortString ) {
        return toStringSignature();
    } else {
        return toStringSignature() + ' ' + (m_definition ? m_definition->toString() : QString());
    }
}
