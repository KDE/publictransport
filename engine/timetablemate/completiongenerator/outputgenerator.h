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

#ifndef OUTPUTGENERATOR_HEADER
#define OUTPUTGENERATOR_HEADER

// Own includes
#include "global_generator.h"

// Qt includes
#include <QStringList>

class QFile;
class QIODevice;

struct TableOfContentsEntry;
typedef QList< TableOfContentsEntry > TableOfContentsEntries;
struct TableOfContentsEntry {
    TableOfContentsEntry( const QString &id, const QString &title, bool isSubSection = false )
            : id(id), title(title), isSubSection(isSubSection) {};

    QString id;
    QString title;
    bool isSubSection;
    TableOfContentsEntries subEntries;
};

/** @brief An interface for generators. */
class AbstractGenerator {
public:
    /** @brief Types of references. */
    enum ReferenceType {
        AllPossibleReferences  = 0x0000, /**< Use all reference types. */
        ScopedReferences       = 0x0001, /**< Method references need to be scoped,
                                           * ie. "class::method()" or "class.method()". */
        ReferencesWithBrackets = 0x0002  /**< Method references need to be followed by brackets "()". */
    };
    Q_DECLARE_FLAGS( ReferenceTypes, ReferenceType )

    AbstractGenerator() {};
    virtual ~AbstractGenerator() {};

    // Get the list of ClassInformation objects, used for references
    ClassInformationList classInformationList() const { return m_classInformations.values(); };
    ClassInformationListByName classInformationListByName() const { return m_classInformations; };

    // Set the list of parsed ClassInformation objects, used for references
    void setClassInformation( const ClassInformationList &classInformationList );

    TableOfContentsEntries tableOfContents() const { return m_tableOfContents; };
    void buildTableOfContents( const Comments &comments );

    // Simply removes all markers instead of replacing them
    QString removeAllMakers( const QString &input ) const;

    // Calls all other generator methods
    QString transform( const QString &input,
                       Transformations transformations = DefaultTransformations ) const;

    /**
     * @brief Replace occurences of markers, eg. for doxygen verbatim ranges.
     *
     * The marker strings can be retrieved using DoxygenComment::markerForDoxygenCommand()
     * and DoxygenComment::markersForInlineDoxygenCommand().
     *
     * @param input The input string.
     **/
    virtual QString replaceInlineMarkers( const QString &input ) const = 0;

    // Transform references "function()" to links "<a href='#class-function'>function()</a>"
    // The default implementation does nothing and returns @p input.
    virtual QString transformReferences( const QString &input,
            ReferenceTypes referenceTypes = ReferencesWithBrackets ) const
    {
        Q_UNUSED( referenceTypes );
        return input;
    };

    // Encodes @p input for the generated output. Gets called for all text from the input.
    // The default implementation does nothing and returns @p input.
    virtual QString encodeString( const QString &input ) const { return input; };

    void addEnumerationReference( const QString &enumerationName );
    void addEnumerableReference( const QString &enumerationName, const QString &enumerableName );

protected:
    QString transformSureReferences( const QString &input ) const {
            return transformReferences(input, ReferencesWithBrackets); };
    QString transformAllReferences( const QString &input ) const {
            return transformReferences(input, AllPossibleReferences); };

    // Encode HTML entities to be displayed in a Browser, eg. "&nbsp;" => "&amp;nbsp;".
    // Can be used for the implementation of encodeString()
    QString encodeHtmlEntities( const QString &input ) const;

    // Encode HTML source code to be displayed in a Browser, eg. "<tag>" => "&lt;tag&gt;".
    QString encodeHtmlTags( const QString &input ) const;

    QString replaceAllMarkers( const QString &input, const MarkerPair &marker,
                               const MarkerPair &replacement,
                               QString (*customTransformFunction)(const QString&),
                               Transformations transformations = NoTransformation ) const;

    QString replaceAllMarkers( const QString &input, const MarkerPair &marker,
                               const MarkerPair &replacement,
                               Transformations transformations = NoTransformation ) const;

    QString replaceInlineMarker(
            const QString &input, DoxygenCommandType type,
            const QLatin1String &markerReplacement ) const;

    // Replace inline doxygen command markers and perform @p transformations on text between
    // found begin/end markers.
    QString replaceInlineMarkerPair(
            const QString &input, DoxygenCommandType inlineType,
            const QLatin1String &beginMarkerReplacement,
            const QLatin1String &endMarkerReplacement,
            Transformations transformations = NoTransformation ) const;

    QString replaceInlineMarkerPair(
            const QString &input, DoxygenCommandType inlineType,
            Transformations transformations ) const;

    QString replaceSectionMarkerPair(
            const QString &input, DoxygenCommandType sectionType,
            const QLatin1String &beginMarkerReplacement,
            const QLatin1String &endMarkerReplacement,
            Transformations transformations = NoTransformation ) const;
    QString removeSectionMarkerPair(
            const QString &input, DoxygenCommandType sectionType ) const;

    // Replace begin-end doxygen command markers and perform @p transformations on text between
    // the markers.
    QString replaceBeginEndMarkerPair(
            const QString &input, DoxygenCommandType beginType,
            const QLatin1String &beginMarkerReplacement,
            const QLatin1String &endMarkerReplacement,
            Transformations transformations = NoTransformation ) const;

    QString replaceBeginEndMarkerPair(
            const QString &input, DoxygenCommandType beginType,
            Transformations transformations ) const;

    QString highlightCodeSyntax( const QString &input ) const;
    QString addVisibilityToggle( const QString &input ) const;

    ClassInformationListByName m_classInformations;
    TableOfContentsEntries m_tableOfContents;
    QHash< QString, QString > m_enumerableReferences;
};
Q_DECLARE_OPERATORS_FOR_FLAGS( AbstractGenerator::ReferenceTypes )

class DocumentationOutputGenerator : public AbstractGenerator {
public:
    DocumentationOutputGenerator() : AbstractGenerator() {};

    virtual QString replaceInlineMarkers( const QString &input ) const;
    virtual QString encodeString( const QString &input ) const;

    /**
     * @brief Transform references in @p input.
     *
     * Links are generated for all found references.
     *
     * References can be in these forms, depending on @p referenceType:
     * @li @b UseEveryWordAsPossibleReference: "class::function", "class.function" or "function"
     * @li @b UseSurelyReferenceWordsAsPossibleReference:
     **/
    virtual QString transformReferences( const QString &input,
            ReferenceTypes referenceTypes = ReferencesWithBrackets ) const;
};

class CompletionOutputGenerator : public AbstractGenerator {
public:
    CompletionOutputGenerator() : AbstractGenerator() {};

    virtual QString replaceInlineMarkers( const QString &input ) const;
    virtual QString encodeString( const QString &input ) const;
};

/**
 * @brief Contains static methods to generate output files.
 *
 * Use DocumentationParser to parse a source file for documentation.
 **/
class OutputGenerator : public QObject {
public:
    /** @brief Create an OutputGenerator object which uses @p generator. */
    OutputGenerator( AbstractGenerator *generator ) : m_generator(generator) {
        Q_ASSERT( generator );
    };

    /** @brief A comment line added to all output files. */
    static const char *OUTPUT_FILE_GENERATOR_COMMENT;

    /** @brief Returns a pointer to the generator used by this OutputGenerator. */
    inline AbstractGenerator *generator() const { return m_generator; };

    /** @brief Returns a list of all generated files (file paths). */
    inline QStringList generatedFiles() const { return m_generatedFiles; };

    /**
     * @brief Write source files for code completion for the given @p completionClasses.
     *
     * The file names are generated using @p completionClassName.
     **/
    bool writeCompletionSource( const ClassInformationList &completionClasses,
                                const QString &outputDirectory,
                                const QString &completionClassName = "JavaScriptCompletionGeneric" );

    /** @brief Write HTML documentation for the given @p documentationClasses. */
    bool writeDocumentation( const ClassInformationList &documentationClasses,
                             const Comments &globalComments, const EnumCommentList &enumComments,
                             const QString &outputDirectory,
                             const QString &htmlHomeFileName = "index.html",
                             const QString &cssFileName = "default.css" );

    inline QString transform( const QString &input,
                              Transformations transformations = DefaultTransformations ) const
    {
        return m_generator->transform(input, transformations);
    };

    static QString fileNameFromClassDocumentation( const ClassInformation &classInformation );

private:
    /** @brief Write the source header file for the completion class with the name @p className. */
    bool writeSourceHeaderOutput( const QString &headerFilePath, const QString &className );

    /** @brief Write the source file for the completion class with the name  @p className. */
    bool writeSourceOutput( const QString &sourceFilePath, const QString &headerFileName,
                            const ClassInformationList &completionClasses,
                            const QString &className );

    /** @brief Write the CSS file for the HTML documentation. */
    bool writeCssOutput( const QString &cssFilePath );

    /** @brief Write the HTML documentation file. */
    bool writeHtmlOutput( const QString &outputDirectory, const QString &htmlHomeFileName,
                          const QString &cssFileName,
                          const ClassInformationList &documentationClasses,
                          const Comments &globalComments, const EnumCommentList &enumComments );

    void writeHtmlPrefix( QIODevice *dev, const QString &title, const QString &cssFileName );
    void writeHtmlPostfix( QIODevice *dev );

    /** @brief Write the implementation of the CompletionClassName::addCompletions() method. */
    void writeAddCompletionsImplementation( QIODevice *dev, const ClassInformation &classInformation );

    /** @brief Write the implementation of the CompletionClassName::addAvailableMethods() method. */
    void writeAddAvailableMethodsImplementation( QIODevice *dev, const ClassInformation &classInformation );

    /** @brief Write HTML for @p classInformation. */
    void writeHtmlClassDocumentation( QIODevice *dev, const ClassInformation &classInformation );

    QString commentToOutput( const Comment &comment, bool ignoreBriefComment = false ) const;
    QString commentListToOutput( const Comments &comments, bool ignoreBriefComments = false ) const;
    QString enumCommentToOutput( const EnumComment &enumComment ) const;
    QString commentToOutput( const DoxygenComments &otherComments ) const;

    /** @brief Write HTML for each ClassInformation object in @p classInformationList. */
    void writeHtmlClassDocumentation( QIODevice *dev, const ClassInformationList &classInformationList );

    /** @brief Create a parameter list output string. */
    QString parameterListOutput( QStringList typedParameters );

    /**
     * @brief Open @p fileName in @p file and append it to the list of generated files.
     *
     * @p fileName The name of the file to open. A canonical version of this file name gets
     *   appended to the list of generated files.
     * @p file A pointer to a QFile object in which the file should be opened.
     **/
    bool openGenerationFile( const QString &fileName, QFile *file );

    QStringList splitLongTextToMultipleLines( const QString &string, int maxColumns = 80 );

    AbstractGenerator *m_generator;
    QStringList m_generatedFiles; ///< The list of generated files
};

#endif // Multiple inclusion guard
