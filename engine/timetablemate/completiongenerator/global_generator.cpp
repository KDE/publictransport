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

#include "global_generator.h"

DoxygenCommandType endTypeFromBeginType( DoxygenCommandType beginType )
{
    if( beginType == DoxygenBeginCode ) {
        return DoxygenEndCode;
    } else if( beginType == DoxygenBeginVerbatim ) {
        return DoxygenEndVerbatim;
    } else if( beginType == DoxygenBeginList ) {
        return DoxygenEndList;
    } else if( beginType == DoxygenBeginListItem ) {
        return DoxygenEndListItem;
    } else {
        qWarning() << "No end type for begin type" << beginType;
        return UnknownDoxygenCommand;
    }
}

DoxygenCommandType beginTypeFromEndType( DoxygenCommandType endType )
{
    if( endType == DoxygenEndCode ) {
        return DoxygenBeginCode;
    } else if( endType == DoxygenEndVerbatim ) {
        return DoxygenBeginVerbatim;
    } else if( endType == DoxygenEndList ) {
        return DoxygenBeginList;
    } else if( endType == DoxygenEndListItem ) {
        return DoxygenBeginListItem;
    } else {
        qWarning() << "No begin type for begin type" << endType;
        return UnknownDoxygenCommand;
    }
}

QLatin1String MarkerPair::fromCommand( DoxygenCommandType type, bool getEndMarker )
{
    switch( type ) {
    case DoxygenBeginVerbatim:
        return QLatin1String( "%BEGIN_VERBATIM%" );
    case DoxygenEndVerbatim:
        return QLatin1String( "%END_VERBATIM%" );
    case DoxygenBeginCode:
        return QLatin1String( "%BEGIN_CODE%" );
    case DoxygenEndCode:
        return QLatin1String( "%END_CODE%" );
    case DoxygenListItem:
        return fromCommand( getEndMarker ? DoxygenEndListItem : DoxygenBeginListItem );
    case DoxygenBeginListItem:
        return QLatin1String( "%BEGIN_LIST_ITEM%" );
    case DoxygenEndListItem:
        return QLatin1String( "%END_LIST_ITEM%" );
    case DoxygenBeginList:
        return QLatin1String( "%BEGIN_LIST%" );
    case DoxygenEndList:
        return QLatin1String( "%END_LIST%" );
    case DoxygenNewline:
        return QLatin1String( "%NEW_LINE%" );

    case UnknownDoxygenCommand:
    default:
        qWarning() << "No marker for" << type;
        return QLatin1String( "" );
    }
}

MarkerPair MarkerPair::fromInlineCommand( DoxygenCommandType inlineType )
{
    Q_ASSERT_X( flagsFromCommand(inlineType).testFlag( DoxygenCommandInline ),
                "DoxygenComment::markersForInlineDoxygenCommand()",
                "Only inline doxygen command types are allowed here, flag DoxygenCommandInline." );
    switch( inlineType ) {
    case DoxygenItalic:
        return MarkerPair( QLatin1String("%BEGIN_ITALIC%"), QLatin1String("%END_ITALIC%") );
    case DoxygenBold:
        return MarkerPair( QLatin1String("%BEGIN_BOLD%"), QLatin1String("%END_BOLD%") );
    case DoxygenVerbatim: // Uses the same markers as DoxygenBeginVerbatim .. DoxygenEndVerbatim
        return MarkerPair( QLatin1String("%BEGIN_VERBATIM%"), QLatin1String("%END_VERBATIM%") );
    case DoxygenInlineParam:
        return MarkerPair( QLatin1String("%BEGIN_PARAM%"), QLatin1String("%END_PARAM%") );
    case DoxygenImage:
        return MarkerPair( QLatin1String("%BEGIN_IMAGE%"), QLatin1String("%END_IMAGE%") );
    case DoxygenRef:
        return MarkerPair( QLatin1String("%BEGIN_REF%"), QLatin1String("%END_REF%") );
    case DoxygenSection:
        return MarkerPair( QLatin1String("%BEGIN_SECTION%"), QLatin1String("%END_SECTION%") );
    case DoxygenSubSection:
        return MarkerPair( QLatin1String("%BEGIN_SUB_SECTION%"), QLatin1String("%END_SUB_SECTION%") );
    case DoxygenSubSubSection:
        return MarkerPair( QLatin1String("%BEGIN_SUB_SUB_SECTION%"), QLatin1String("%END_SUB_SUB_SECTION%") );
    case DoxygenListItem:
        return MarkerPair( fromCommand(DoxygenBeginListItem), fromCommand(DoxygenEndListItem) );
    case DoxygenBeginVerbatim:
        return MarkerPair( fromCommand(DoxygenBeginVerbatim), fromCommand(DoxygenEndVerbatim) );
    case DoxygenBeginCode: // Not inline
        return MarkerPair( fromCommand(DoxygenBeginCode), fromCommand(DoxygenEndCode) );

    default:
        qWarning() << "No MarkerPair for" << inlineType;
        return MarkerPair();
    }
}

DoxygenCommandFlags flagsFromCommand( DoxygenCommandType type )
{
    switch( type ) {
    case DoxygenBrief:
    case DoxygenFile:
        return NoDoxygenCommandFlag;
    case DoxygenParam:
        return DoxygenCommandExpectsArgument | DoxygenCommandMultiline;
    case DoxygenNewline:
        return DoxygenCommandInline;
    case DoxygenItalic:
    case DoxygenBold:
    case DoxygenInlineParam:
    case DoxygenRef:
        return DoxygenCommandExpectsArgument | DoxygenCommandInline;
    case DoxygenImage:
        return DoxygenCommandExpectsTwoArguments | DoxygenCommandInline;
    case DoxygenVerbatim:
        return DoxygenCommandExpectsArgument | DoxygenCommandInline | DoxygenCommandVerbatim;
    case DoxygenSection:
    case DoxygenSubSection:
    case DoxygenSubSubSection:
        return DoxygenCommandIsSection | DoxygenCommandInline;
    case DoxygenReturn:
    case DoxygenTodo:
    case DoxygenWarning:
    case DoxygenSee:
    case DoxygenNote:
    case DoxygenSince:
    case DoxygenBug:
    case DoxygenDeprecated:
        return DoxygenCommandMultiline;

    case DoxygenBeginCode:
        return DoxygenCommandBegin | DoxygenCommandMultiline | DoxygenCommandInWord |
               DoxygenCommandVerbatim;
    case DoxygenBeginVerbatim:
        return DoxygenCommandBegin | DoxygenCommandMultiline | DoxygenCommandInWord |
               DoxygenCommandVerbatim | DoxygenCommandInline;
    case DoxygenEndVerbatim:
        return DoxygenCommandEnd | DoxygenCommandInWord;
    case DoxygenEndCode:
        return DoxygenCommandEnd | DoxygenCommandInWord | DoxygenCommandInline;

    case StandardCommentParagraph:
        // Normal comments without a leading doxygen command can have multiple lines of course
        return DoxygenCommandMultiline;

    case DoxygenListItem:
        return DoxygenCommandMultiline | DoxygenCommandInline;

        // Virtual commands without corresponding doxygen commands
    case DoxygenBeginListItem:
    case DoxygenBeginList:
        return DoxygenCommandVirtual | DoxygenCommandBegin;
    case DoxygenEndListItem:
    case DoxygenEndList:
        return DoxygenCommandVirtual | DoxygenCommandEnd;

    case UnknownDoxygenCommand:
    case InvalidDoxygenCommand:
    default:
        return NoDoxygenCommandFlag;
    }
}

DoxygenCommandType typeFromString( const QString &doxygenCommand )
{
    const QString command = doxygenCommand.toLower();
    if ( command == QLatin1String("brief") ) {
        return DoxygenBrief;
    } else if ( command == QLatin1String("param") ) {
        return DoxygenParam;
    } else if ( command == QLatin1String("return") || command == QLatin1String("returns") ) {
        return DoxygenReturn;
    } else if ( command == QLatin1String("todo") ) {
        return DoxygenTodo;
    } else if ( command == QLatin1String("warning") ) {
        return DoxygenWarning;
    } else if ( command == QLatin1String("note") ) {
        return DoxygenNote;
    } else if ( command == QLatin1String("see") ) {
        return DoxygenSee;
    } else if ( command == QLatin1String("deprecated") ) {
        return DoxygenDeprecated;
    } else if ( command == QLatin1String("bug") ) {
        return DoxygenBug;
    } else if ( command == QLatin1String("since") ) {
        return DoxygenSince;
    } else if ( command == QLatin1String("em") ) {
        return DoxygenItalic;
    } else if ( command == QLatin1String("b") ) {
        return DoxygenBold;
    } else if ( command == QLatin1String("c") ) {
        return DoxygenVerbatim;
    } else if ( command == QLatin1String("li") ) {
        return DoxygenListItem;
    } else if ( command == QLatin1String("image") ) {
        return DoxygenImage;
    } else if ( command == QLatin1String("p") ) {
        return DoxygenInlineParam;
    } else if ( command == QLatin1String("n") ) {
        return DoxygenNewline;
    } else if ( command == QLatin1String("ref") ) {
        return DoxygenRef;
    } else if ( command == QLatin1String("section") ) {
        return DoxygenSection;
    } else if ( command == QLatin1String("subsection") ) {
        return DoxygenSubSection;
    } else if ( command == QLatin1String("file") ) {
        return DoxygenFile;
    } else if ( command == QLatin1String("code") ) {
        return DoxygenBeginCode;
    } else if ( command == QLatin1String("verbatim") ) {
        return DoxygenBeginVerbatim;
    } else if ( command == QLatin1String("endcode") ) {
        return DoxygenEndCode;
    } else if ( command == QLatin1String("endverbatim") ) {
        return DoxygenEndVerbatim;
    } else if ( command.isEmpty() ) {
        return StandardCommentParagraph;
    } else {
        return UnknownDoxygenCommand;
    }
}

DoxygenCommandType typeFromBeginningOfString( QString *doxygenCommand )
{
    // Test only command with flag DoxygenCommandInWord
    const QString command = doxygenCommand->toLower();
    if ( command.startsWith(QLatin1String("code")) ) {
        *doxygenCommand = "code";
        return DoxygenBeginCode;
    } else if ( command.startsWith(QLatin1String("endcode")) ) {
        *doxygenCommand = "endcode";
        return DoxygenEndCode;
    } else if ( command.startsWith(QLatin1String("verbatim")) ) {
        *doxygenCommand = "verbatim";
        return DoxygenBeginVerbatim;
    } else if ( command.startsWith(QLatin1String("endverbatim")) ) {
        *doxygenCommand = "endverbatim";
        return DoxygenEndVerbatim;
    } else if ( command.isEmpty() ) {
        return StandardCommentParagraph;
    } else {
        return UnknownDoxygenCommand;
    }
}

bool beginMatchesEnd( DoxygenCommandType beginType, DoxygenCommandType endType )
{
    return (beginType == DoxygenBeginVerbatim && endType == DoxygenEndVerbatim) ||
           (beginType == DoxygenBeginCode && endType == DoxygenEndCode);
}
