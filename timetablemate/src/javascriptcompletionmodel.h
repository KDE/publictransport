/*
 *   Copyright 2011 Friedrich Pülz <fieti1983@gmx.de>
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

#ifndef JAVASCRIPTCOMPLETIONMODEL_HEADER
#define JAVASCRIPTCOMPLETIONMODEL_HEADER

#include <KTextEditor/CodeCompletionModel>

struct CompletionItem {
    /** @brief Creates an invalid CompletionItem. */
    CompletionItem() {
    };

    CompletionItem( KTextEditor::CodeCompletionModel::CompletionProperties properties,
            const QString &name, const QString &description,
            const QString &completion, bool isTemplate = false,
            const QString &prefix = QString(),
            const QString &postfix = QString() ) {
    this->properties = properties;
    this->name = name;
    this->description = description;
    this->completion = completion;
    this->isTemplate = isTemplate;
    this->prefix = prefix;
    this->postfix = postfix;
    };

    bool isValid() const { return !name.isNull(); };

    QString name;
    QString description;
    QString completion;
    QString prefix;
    QString postfix;
    KTextEditor::CodeCompletionModel::CompletionProperties properties;

    bool isTemplate;
};

class JavaScriptCompletionModel : public KTextEditor::CodeCompletionModel {
    Q_OBJECT
public:
    JavaScriptCompletionModel( const QString &completionShortcut, QObject *parent );

    void initGlobalFunctionCompletion();
    void initHelperCompletion();
    void initTimetableInfoCompletion();
    void initFunctionCallCompletion();

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    virtual void executeCompletionItem( KTextEditor::Document* document,
                        const KTextEditor::Range& word, int row ) const;
    virtual void completionInvoked( KTextEditor::View *view, const KTextEditor::Range &range,
                    InvocationType invocationType );

    CompletionItem completionItemFromId( const QString id );

private:
    QString stripComments( const QString &text ) const;

    QString m_completionShortcut;
    QList< CompletionItem > m_completions; // Stores the current completions

    // These only store completions
    QHash< QString, CompletionItem > m_completionsGlobalFunctions;
    QHash< QString, CompletionItem > m_completionsTimetableInfo;
    QHash< QString, CompletionItem > m_completionsHelper;
    QHash< QString, CompletionItem > m_completionsCalls;
};

#endif // Multiple inclusion guard
