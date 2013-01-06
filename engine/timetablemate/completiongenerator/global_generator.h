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

#ifndef GLOBALGENERATOR_HEADER
#define GLOBALGENERATOR_HEADER

#include <QString>
#include <QDebug>

enum Transformation {
    NoTransformation        = 0x0000, /**< Do nothing. */
    TransformEncode         = 0x0001, /**< Encode input for the generated output. */
    TransformRemoveRemainingMarkers
                            = 0x0002, /**< Remove all (remaining) marker strings from the input. */
    TransformInlineMarkers  = 0x0004, /**< Replace inline markers. */
    TransformSureReferences = 0x0008, /**< Find sure references and add links. The name of a
                                         * function must match and it must be followed by "()" or
                                         * it must be given with scope (class::function or
                                         * class.function). */
    TransformAllReferences  = 0x0010 | TransformSureReferences,
                                      /**< Find all references and add links. Only the name of a
                                         * function must match for a reference to be generated. */
    TransformHighlightCode  = 0x0020, /**< Simple syntax highlighting for JavaScript code. */
    TransformAddVisibilityToggle
                            = 0x0040, /**< Add a little button to toggle visibility, if the content
                                         * is long enough. */
    TransformEncodeHtmlTags = 0x0080, /**< Encode HTML tags in the input. */

    DefaultTransformations = TransformEncode | TransformInlineMarkers | TransformSureReferences,
            /**< Transformations that should be used most of the time. */
    AllTransformations = DefaultTransformations | TransformAllReferences | TransformHighlightCode
            /**< All available transformations. */
};
Q_DECLARE_FLAGS( Transformations, Transformation )
Q_DECLARE_OPERATORS_FOR_FLAGS( Transformations )

/** @brief Supported types of doxygen commands. */
enum DoxygenCommandType {
    InvalidDoxygenCommand = 0,
    UnknownDoxygenCommand,
    StandardCommentParagraph, // A normal comment paragraph without a beginning doxygen tag
    DoxygenBrief,
    DoxygenParam,
    DoxygenReturn,
    DoxygenBug,
    DoxygenRef,
    DoxygenNote,
    DoxygenSee,
    DoxygenSince,
    DoxygenTodo,
    DoxygenDeprecated,
    DoxygenWarning,
    DoxygenNewline,
    DoxygenSection,
    DoxygenSubSection,
    DoxygenSubSubSection,
    DoxygenImage, // without size arguments
    DoxygenFile,

    // Inline doxygen commands
    DoxygenItalic,
    DoxygenBold,
    DoxygenInlineParam,
    DoxygenVerbatim,

    // Inline, but also multiline doxygen commands
    DoxygenListItem,

    // Virtual commands without corresponding doxygen command
    DoxygenBeginListItem, // Virtual commands to mark begin/end of inline multiline commands
    DoxygenEndListItem,
    DoxygenBeginList, // Virtual commands to encapsulate DoxygenListItem commands
    DoxygenEndList,

    // Begin-end doxygen commands
    DoxygenBeginCode,
    DoxygenEndCode,
    DoxygenBeginVerbatim,
    DoxygenEndVerbatim
};

DoxygenCommandType endTypeFromBeginType( DoxygenCommandType beginType );
DoxygenCommandType beginTypeFromEndType( DoxygenCommandType endType );

/**
 * @brief Flags for doxygen commands, which control it's behaviour.
 * @see flagsForDoxygenCommand
 **/
enum DoxygenCommandFlag {
    NoDoxygenCommandFlag                = 0x0000,
    DoxygenCommandVirtual               = 0x0001, // Has no corresponding doxygen command,
                                                  // gets used to eg. combine list items in a
                                                  // virtual list element
    DoxygenCommandMultiline             = 0x0002, // Can extend over multiple lines
    DoxygenCommandExpectsArgument       = 0x0004, // Command expects an argument
    DoxygenCommandExpectsTwoArguments   = 0x0008, // Command expects two arguments
    DoxygenCommandIsSection             = 0x0010, // Command expects two arguments, the first is
                                                  // one word, the second gets read until newline
    DoxygenCommandInline                = 0x0020, // Is an inline command, can start/end anywhere
                                                  // in a line. Non-inline commands start at the
                                                  // beginning of a line and read everything until
                                                  // line end
    DoxygenCommandBegin                 = 0x0040, // Is a begin command, eg. @code
    DoxygenCommandEnd                   = 0x0080, // Is an end command, eg. @endcode
    DoxygenCommandInWord                = 0x0100, // Does not need whitespaces before or after the
                                                  // command name, eg. @verbatim
    DoxygenCommandVerbatim              = 0x0200  // Content gets read without changing whitespace
};
Q_DECLARE_FLAGS( DoxygenCommandFlags, DoxygenCommandFlag )
Q_DECLARE_OPERATORS_FOR_FLAGS( DoxygenCommandFlags )

template<class T >
class QList;

template<class T1, class T2 >
struct QPair;

class ClassInformation;
typedef QList< ClassInformation > ClassInformationList;
typedef QHash< QString, ClassInformation > ClassInformationListByName;

class DoxygenComment;
typedef QList< DoxygenComment* > DoxygenComments;

class DoxygenCommentWithArguments;
typedef QList< DoxygenCommentWithArguments* > DoxygenCommentsWithArguments;

typedef DoxygenCommentWithArguments DoxygenParameter;
typedef QList< DoxygenParameter* > DoxygenParameters;

class Comment;
typedef QList< Comment > Comments;

class ClassComment;
class EnumComment;
class EnumerableComment;
typedef QList< EnumComment > EnumCommentList;
typedef QList< EnumerableComment > EnumerableCommentList;
typedef QHash< QString, EnumerableComment > EnumerableComments;
typedef QHash< QString, ClassComment > ClassComments;
typedef QHash< QString, Comment > MethodComments;
typedef QHash< QString, EnumComment > EnumComments;

// Get flags for type, this controls the handling of the different commands
DoxygenCommandFlags flagsFromCommand( DoxygenCommandType type );

DoxygenCommandType typeFromString( const QString &doxygenCommandName );
DoxygenCommandType typeFromBeginningOfString( QString *doxygenCommandName );
bool beginMatchesEnd( DoxygenCommandType beginType, DoxygenCommandType endType );

struct MarkerPair {
    MarkerPair() : begin(""), end("") {};
    MarkerPair( const QLatin1String &begin, const QLatin1String &end ) : begin(begin), end(end) {};

    static QLatin1String fromCommand( DoxygenCommandType type, bool getEndMarker = false );
    static MarkerPair fromInlineCommand( DoxygenCommandType inlineType );
    static inline MarkerPair fromBeginCommand( DoxygenCommandType beginType ) {
        return MarkerPair( fromCommand(beginType), fromCommand(endTypeFromBeginType(beginType)) );
    };

    QLatin1String begin;
    QLatin1String end;
};

/* Functions for nicer debug output */
inline QDebug &operator <<( QDebug debug, DoxygenCommandFlag flag )
{
    switch ( flag ) {
    case NoDoxygenCommandFlag:
        return debug << "NoDoxygenCommandFlag";
    case DoxygenCommandVirtual:
        return debug << "DoxygenCommandVirtual";
    case DoxygenCommandMultiline:
        return debug << "DoxygenCommandMultiline";
    case DoxygenCommandExpectsArgument:
        return debug << "DoxygenCommandExpectsArgument";
    case DoxygenCommandExpectsTwoArguments:
        return debug << "DoxygenCommandExpectsTwoArguments";
    case DoxygenCommandIsSection:
        return debug << "DoxygenCommandIsSection";
    case DoxygenCommandInline:
        return debug << "DoxygenCommandInline";
    case DoxygenCommandBegin:
        return debug << "DoxygenCommandBegin";
    case DoxygenCommandEnd:
        return debug << "DoxygenCommandEnd";
    case DoxygenCommandInWord:
        return debug << "DoxygenCommandInWord";
    case DoxygenCommandVerbatim:
        return debug << "DoxygenCommandVerbatim";

    default:
        return debug << "DoxygenCommandFlag unknown" << static_cast<int>(flag);
    }
}

inline QDebug &operator <<( QDebug debug, DoxygenCommandType type )
{
    switch ( type ) {
    case InvalidDoxygenCommand:
        return debug << "InvalidDoxygenCommand";
    case UnknownDoxygenCommand:
        return debug << "UnknownDoxygenCommand";
    case StandardCommentParagraph:
        return debug << "StandardCommentParagraph";
    case DoxygenBrief:
        return debug << "DoxygenBrief";
    case DoxygenParam:
        return debug << "DoxygenParam";
    case DoxygenNewline:
        return debug << "DoxygenNewline";
    case DoxygenSection:
        return debug << "DoxygenSection";
    case DoxygenSubSection:
        return debug << "DoxygenSubSection";
    case DoxygenSubSubSection:
        return debug << "DoxygenSubSubSection";
    case DoxygenFile:
        return debug << "DoxygenFile";
    case DoxygenReturn:
        return debug << "DoxygenReturn";
    case DoxygenRef:
        return debug << "DoxygenRef";
    case DoxygenNote:
        return debug << "DoxygenNote";
    case DoxygenSee:
        return debug << "DoxygenSee";
    case DoxygenSince:
        return debug << "DoxygenSince";
    case DoxygenTodo:
        return debug << "DoxygenTodo";
    case DoxygenDeprecated:
        return debug << "DoxygenDeprecated";
    case DoxygenWarning:
        return debug << "DoxygenWarning";
    case DoxygenItalic:
        return debug << "DoxygenItalic";
    case DoxygenBold:
        return debug << "DoxygenBold";
    case DoxygenVerbatim:
        return debug << "DoxygenVerbatim";
    case DoxygenListItem:
        return debug << "DoxygenListItem";
    case DoxygenBeginListItem:
        return debug << "DoxygenBeginListItem";
    case DoxygenEndListItem:
        return debug << "DoxygenEndListItem";
    case DoxygenBeginList:
        return debug << "DoxygenBeginList";
    case DoxygenEndList:
        return debug << "DoxygenEndList";
    case DoxygenInlineParam:
        return debug << "DoxygenInlineParam";
    case DoxygenBug:
        return debug << "DoxygenBug";
    case DoxygenBeginCode:
        return debug << "DoxygenBeginCode";
    case DoxygenBeginVerbatim:
        return debug << "DoxygenBeginVerbatim";
    case DoxygenEndCode:
        return debug << "DoxygenEndCode";
    case DoxygenEndVerbatim:
        return debug << "DoxygenEndVerbatim";

    default:
        return debug << "DoxygenCommandType unknown" << static_cast<int>(type);
    }
}

#endif // Multiple inclusion guard
