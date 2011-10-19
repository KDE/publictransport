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

/** @file
 * @brief Contains widgets where the user can dynamically add/remove widgets.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef DYNAMICWIDGET_HEADER
#define DYNAMICWIDGET_HEADER

#include "publictransporthelper_export.h"

#include <QWidget>

class QLayoutItem;
class QToolButton;
class AbstractDynamicWidgetContainer;
class DynamicWidgetPrivate;

/**
 * @brief Wraps widgets in AbstractDynamicWidgetContainer and adds buttons.
 *
 * Provides a widget which embeds a "content widget" and a remove and/or add
 * button. It's used by @ref AbstractDynamicWidgetContainer. To get the contained widget
 * use @ref contentWidget. The buttons can be taken out of the DynamicWidget
 * by using @ref takeRemoveButton / @ref takeAddButton. They can then be put into
 * another widget, the connections remain.
 *
 * @note You cannot create DynamicWidgets yourself and mostly don't need to use
 *   it at all, that's up to @ref AbstractDynamicWidgetContainer.
 *
 * @see AbstractDynamicWidgetContainer
 **/
class PUBLICTRANSPORTHELPER_EXPORT DynamicWidget : public QWidget {
    Q_OBJECT
    Q_ENUMS( ButtonType )
    friend class AbstractDynamicWidgetContainer; // To access the constructor

public:
    /** @brief Button types. */
    enum ButtonType {
        ButtonSpacer, /**< A spacer item with the size of a button */
        RemoveButton, /**< A remove button */
        AddButton /**< An add button */
    };

    /** @brief The destructor deleted the content widget. */
    ~DynamicWidget();

    /** @brief Gets the content widget of this dynamic widget. */
    QWidget *contentWidget() const;

    /** @brief Gets the content widget of this dynamic widget casted to WidgetType. */
    template< class WidgetType >
    WidgetType contentWidget() const {
        return qobject_cast< WidgetType >( contentWidget() );
    };

    /**
     * @brief Replaces the current content widget with @p contentWidget.
     *
     * The old content widget gets deleted.
     **/
    void replaceContentWidget( QWidget *contentWidget );

    /**
     * @brief Adds a button of type @p buttonType.
     *
     * @return A pointer to the added button or NULL if no button was added.
     *   For example @ref ButtonSpacer adds a spacer but returns NULL.
     **/
    QToolButton *addButton( AbstractDynamicWidgetContainer *container, ButtonType buttonType );

    /** @brief Returns a pointer to the remove button if any. */
    QToolButton *removeButton() const;

    /** @brief Returns a pointer to the add button if any. */
    QToolButton *addButton() const;

    /**
     * @brief Takes the remove button out of the DynamicWidget's layout if there is
     *   a remove button for this DynamicWidget.
     *
     * It can then be put into another widget, without disconnecting it's clicked signal.
     * @ref removeButton will still return a pointer to the remove button, you can also still
     * use @ref setRemoveButtonIcon. It can also be deleted.
     * @return The remove button or NULL, if there is no remove button for
     * this DynamicWidget or the remove button has already been taken.
     **/
    QToolButton *takeRemoveButton();

    /**
     * @brief Takes the add button out of the DynamicWidget's layout if there is
     *   an add button for this DynamicWidget.
     *
     * It can then be put into another widget, without disconnecting it's clicked signal.
     * @ref addButton will still return a pointer to the remove button, you can also still
     * use @ref setAddButtonIcon. It can also be deleted.
     * @return The add button or NULL, if there is no add button for this
     * DynamicWidget or the add button has already been taken.
     **/
    QToolButton *takeAddButton();

    /** @brief Sets the spacing between buttons. */
    void setButtonSpacing( int spacing = 1 );

    /** @brief Sets the alignment of the buttons. */
    void setButtonAlignment( Qt::Alignment alignment = Qt::AlignRight | Qt::AlignTop );

    /** @brief Sets auto-raising of the buttons to @p autoRaiseButtons. */
    void setAutoRaiseButtons( bool autoRaiseButtons = true );

    /** @brief Sets the icon of the remove button to @p removeButtonIcon if any. */
    void setRemoveButtonIcon( const QString &removeButtonIcon = "list-remove" );

    /** @brief Sets the icon of the add button to @p addButtonIcon if any. */
    void setAddButtonIcon( const QString &addButtonIcon = "list-add" );

Q_SIGNALS:
    /** @brief The content widget was replaced by @p newContentWidget. */
    void widgetReplaced( QWidget *newContentWidget );

    /** @brief The remove button was clicked. */
    void removeClicked();

    /** @brief The add button was clicked. */
    void addClicked();

protected Q_SLOTS:
    /**
     * @brief Used if the add and/or remove buttons are taken out of the layout
     * using @ref takeRemoveButton / @ref takeAddButton to be notified if the
     * buttons get deleted.
     **/
    void buttonDestroyed( QObject *object );

protected:
    /**
     * @brief Creates a new DynamicWidget with @p contentWidget.
     *
     * @param contentWidget The content widget.
     *
     * @param container Used as parent and to get initial icons for the
     *   add/remove buttons.
     *
     * @param buttonTypes A list of buttons to be created.
     *
     * @see ButtonType
     * @see AbstractDynamicWidgetContainer
     **/
    DynamicWidget( QWidget *contentWidget, AbstractDynamicWidgetContainer *container,
                   QList<ButtonType> buttonTypes = QList<ButtonType>() << RemoveButton );

    DynamicWidgetPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( DynamicWidget )
    Q_DISABLE_COPY( DynamicWidget )
};

class AbstractDynamicWidgetContainerPrivate;

/**
 * @brief A widget containing a dynamic list of widgets.
 *
 * This widget contains a dynamic list of widgets. The user can add/remove widgets using buttons,
 * that are automatically added based on the options given in the constructor. Override
 * @ref createNewWidget to create new a QWidget instance that should be added when the add button
 * is clicked.
 *
 * To show a text on separators (activated with the ShowSeparators option), override
 * @ref createSeparator and call the base implementation with the text as argument.
 *
 * Add/remove buttons can be added besides widgets or after the last widget. By default new widgets
 * are added to the bottom of the widget list and global add/remove buttons
 * (@ref RemoveButtonAfterLastWidget, @ref AddButtonAfterLastWidget) are shown below the last
 * widget. Use @ref AddWidgetsAtTop to add new widgets to the top of the widget list and show
 * global buttons above the last (top) widget.
 *
 * To change the add button use the @ref addButton function to get a pointer to the button. For
 * example you can turn it into a menu button and offer menu items to add widgets with different
 * predefined values. By default the clicked signal of the add button is connected to the
 * @ref createAndAddWidget slot.
 *
 * The remove button's clicked signal is connected to the @ref removeLastWidget slot by default.
 * If the @ref RemoveButtonsBesideWidgets option is used you can override @ref createDynamicWidget
 * to change new remove buttons.
 *
 * If you want to only do the adding/removing of widgets programatically, use the
 * @ref NoRemoveButton and @ref NoAddButton options.
 *
 * @note Buttons can also be taken out of the dynamic widgets using
 *   @ref DynamicWidget::takeAddButton / @ref DynamicWidget::takeRemoveButton. That way you can
 *   place them somewhere else.
 **/
class PUBLICTRANSPORTHELPER_EXPORT AbstractDynamicWidgetContainer : public QWidget {
    Q_OBJECT
    Q_ENUMS( ContainerLayout SeparatorOptions RemoveButtonOptions AddButtonOptions NewWidgetPosition )
    Q_PROPERTY( int buttonSpacing READ buttonSpacing WRITE setButtonSpacing )
    Q_PROPERTY( Qt::Alignment buttonAlignment READ buttonAlignment WRITE setButtonAlignment )
    Q_PROPERTY( bool autoRaiseButtons READ autoRaiseButtons WRITE setAutoRaiseButtons )
    Q_PROPERTY( Qt::Alignment buttonAlignment READ buttonAlignment WRITE setButtonAlignment )
    Q_PROPERTY( SeparatorOptions separatorOptions READ separatorOptions WRITE setSeparatorOptions )
    friend class DynamicWidget; // To access the constructor of DynamicWidget

public:
    /** @brief Options for separators between widgets. */
    enum SeparatorOptions {
        NoSeparator = 0, /**< Don't add separators between widgets. */
        ShowSeparators /**< Add separators between widgets. */
    };

    /** @brief Options for the buttons to remove widgets. */
    enum RemoveButtonOptions {
        NoRemoveButton = 0, /**< Don't show remove buttons. You can manually remove
                * widgets by calling @ref removeWidget(). */
        RemoveButtonsBesideWidgets, /**< Show a remove button beside each widget. */
        RemoveButtonAfterLastWidget /**< Show a remove button after the last widget. If
                * @ref AddWidgetsAtBottom is used (the default) the button gets placed under all
                * widgets in the list. If @ref AddWidgetsAtTop is used, the button gets placed on
                * top of all widgets.
                * @note This creates only one remove button to remove the last widget. */
    };

    /** @brief Options for the buttons to add new widgets. */
    enum AddButtonOptions {
        NoAddButton = 0, /**< Don't show add buttons. You can manually add widgets
                * by calling @ref addWidget(). */
        AddButtonBesideFirstWidget, /**< Show an add button beside the first widget. */
        AddButtonAfterLastWidget /**< Show an add button after the last widget. If
                * @ref AddWidgetsAtBottom is used (the default) the button gets placed under all
                * widgets in the list. If @ref AddWidgetsAtTop is used, the button gets placed on
                * top of all widgets. */
    };

    /** @brief Positions for new widgets. */
    enum NewWidgetPosition {
        AddWidgetsAtBottom = 0, /**< Add new widgets at the buttom of the widget list. This is
                * the default. When the remove button is clicked the last widget gets removed. */
        AddWidgetsAtTop = 1 /**< Add new widgets at the top of the widget list. When the remove
                * button is clicked the widget on top gets removed, which then is the last widget
                * in the list. Buttons added after the last widget get placed on top of all
                * widgets (after the last widget = over the last widget, ie. widget on top). */
    };

//     /** @brief Other options. */
//     enum DynamicWidgetContainerOption {
//         NoOption = 0x0000
//     };
//     Q_DECLARE_FLAGS( DynamicWidgetContainerOptions, DynamicWidgetContainerOption );

    /**
     * @brief Creates a new AbstractDynamicWidgetContainer instance.
     *
     * @param parent The parent widget. Default is 0.
     * @param removeButtonOptions Options for remove button(s). Default is RemoveButtonsBesideWidgets.
     * @param addButtonOptions Options for the add button. Default is AddButtonBesideFirstWidget.
     * @param separatorOptions Options for separators. Default is NoSeparator.
     * @param newWidgetPosition The position of newly added widgets. Default is AddWidgetsAtBottom.
     **/
    explicit AbstractDynamicWidgetContainer( QWidget *parent = 0,
            RemoveButtonOptions removeButtonOptions = RemoveButtonsBesideWidgets,
            AddButtonOptions addButtonOptions = AddButtonBesideFirstWidget,
            SeparatorOptions separatorOptions = NoSeparator,
            NewWidgetPosition newWidgetPosition = AddWidgetsAtBottom );
    ~AbstractDynamicWidgetContainer();

    /**
     * @brief Gets the DynamicWidget at the given @p index.
     **/
    DynamicWidget *dynamicWidget( int index ) const;

    /**
     * @brief Changes the separator options.
     *
     * @warning The new @p separatorOptions will only be applied for newly added widgets.
     **/
    void setSeparatorOptions( SeparatorOptions separatorOptions = NoSeparator );

    /** @brief Returns the current separator options. */
    SeparatorOptions separatorOptions() const;

    /**
     * @brief Set a custom add button, to let it automatically be set enabled/disabled
     *   depending on the current/maximum numbers of widgets.
     *
     * It also uses the clicked signal of @p addButton to add new widgets. */
    void setCustomAddButton( QToolButton *addButton );

    /**
     * @brief Gets the add button or NULL if none is set.
     *
     * If created with @ref AddButtonBesideFirstWidget, the button can be deleted, when the
     * first widget gets removed.
     *
     * @note This also returns a pointer to the add button, if a custom add
     *   button has been set using @ref setCustomAddButton.
     **/
    QToolButton *addButton() const;

    /** @brief Gets the remove button if only one is set (using @ref RemoveButtonAfterLastWidget).
     *   Otherwise this returns NULL.
     *
     * If @ref RemoveButtonsBesideWidgets is used you can get the remove buttons from the
     * dynamic widgets. You can access the dynamic widgets using @ref dynamicWidget.
     **/
    QToolButton *removeButton() const;

    /** @brief Removes all contained widgets but leaves @ref minimumWidgetCount() widgets. */
    void removeAllWidgets();

    /**
     * @brief The minimum number of widgets.
     *
     * If this number is reached no more widgets can be removed and all remove buttons
     * get disabled.
     **/
    int minimumWidgetCount() const;

    /**
     * @brief The maximum number of widgets.
     *
     * If this number is reached no more widgets can be added and the add button gets disabled.
     **/
    int maximumWidgetCount() const;

    /**
     * @brief Sets the minimum and maximum widget count.
     *
     * It also makes sure that the actual widget count is in the given range by adding or removing
     * widgets if needed if @p putIntoRange is true.
     *
     * @return The number of added widgets (negative, if widgets were removed).
     **/
    int setWidgetCountRange( int minWidgetCount = 0, int maxWidgetCount = -1,
                             bool putIntoRange = true );

    /**
     * @brief @brief Gets the number of widgets in this dynamic widget container.
     *
     * @return The number of widgets.
     **/
    int widgetCount() const;

    /** @brief Gets the spacing of (add/remove) buttons in DynamicWidgets. */
    int buttonSpacing() const;

    /** @brief Gets the alignment of the add/remove button layout. */
    Qt::Alignment buttonAlignment() const;

    /** @brief Gets the value used for @ref QToolButton::setAutoRaise for add/remove buttons. */
    bool autoRaiseButtons() const;

    /** @brief Gets the name of the icon used for remove buttons. */
    QString removeButtonIcon() const;

    /** @brief Gets the name of the icon used for the add button. */
    QString addButtonIcon() const;

    /** @brief Sets the spacing of (add/remove) buttons in DynamicWidgets to @p spacing. */
    void setButtonSpacing( int spacing = 1 );

    /** @brief Sets the alignment of the add/remove button layout to @p alignment. */
    void setButtonAlignment( Qt::Alignment alignment = Qt::AlignRight | Qt::AlignTop );

    /** @brief Sets the value to use for @ref QToolButton::setAutoRaise for existing and new
     *   add/remove buttons. */
    void setAutoRaiseButtons( bool autoRaiseButtons = true );

    /** @brief Sets the name of the icon used for remove buttons to @p removeButtonIcon. */
    void setRemoveButtonIcon( const QString &removeButtonIcon = "list-remove" );

    /** @brief Sets the name of the icon used for the add button to @p addButtonIcon. */
    void setAddButtonIcon( const QString &addButtonIcon = "list-add" );

Q_SIGNALS:
    /**
     * @brief A new @p widget has been added.
     *
     * To get the add/remove buttons that may have been added for the new widget, the DynamicWidget
     * containing them can be retrieved using @ref dynamicWidgetForWidget. The @ref DynamicWidget
     * class has functions to get pointers to the buttons: @ref DynamicWidget::addButton and
     * @ref DynamicWidget::removeButton. Buttons are only added if @ref AddButtonBesideFirstWidget
     * and/or @ref RemoveButtonsBesideWidgets are used.
     **/
    void added( QWidget *widget );

    /** @brief The @p widget at @p widgetIndex in the widget list has been removed. */
    void removed( QWidget *widget, int widgetIndex );

protected Q_SLOTS:
    /**
     * @brief This slot is connected to the removeClicked signal of every contained @ref DynamicWidget.
     *
     * It uses sender() to find out which remove button has been clicked. The DynamicWidget
     * containing the sender button gets removed.
     **/
    void removeWidget();

    /**
     * @brief Removes the last widget.
     *
     * This slot is connected to the remove button, if created using
     * @ref RemoveButtonAfterLastWidget.
     **/
    void removeLastWidget();

    /**
     * @brief This slot is connected to the clicked()-signal of the add button to add a new widget.
     *
     * The new widget is created using @ref createNewWidget and added using @ref addWidget.
     **/
    void createAndAddWidget();

protected:
    AbstractDynamicWidgetContainerPrivate* const d_ptr;
    AbstractDynamicWidgetContainer( QWidget *parent, AbstractDynamicWidgetContainerPrivate &dd,
                    RemoveButtonOptions removeButtonOptions, AddButtonOptions addButtonOptions,
                    NewWidgetPosition newWidgetPosition );

    /** @brief Reimplemented to update the enabled state of the add button. */
    virtual void changeEvent( QEvent *event );

    /**
     * @brief Override to create a new QWidget instance that should be added when the add button
     *   is clicked.
     **/
    virtual QWidget *createNewWidget() = 0;

    /**
     * @brief Adds @p widget to the layout, which gets wrapped by a DynamicWidget.
     *
     * The new @p widget is also focused. If you want to focus some sub-widget of the @p widget
     * you can override this function and set the focus after calling the base implementation.
     *
     * A separator widget is automatically added if needed (see ShowSeparators).
     *
     * @return The new DynamicWidget or NULL, if the maximum widget count is already reached.
     **/
    virtual DynamicWidget *addWidget( QWidget *widget );

    /**
     * @brief Removes @p widget and it's DynamicWidget from the layout.
     *
     * This also removes separators if needed.
     *
     * @return The index of the widget or -1, if the minimum widget count is already reached.
     **/
    virtual int removeWidget( QWidget *widget );

    /**
     * @brief Creates a new separator widget with the given @p separatorText.
     *
     * The base implementation always calls this function with an empty string, ie. it doesn't show
     * any text on separator items. If you want to add some text, override this function and call
     * the base implementation with the text that should be shown in the new separator widget.
     *
     * This function sets the object name of the newly created separator widget to "separator"
     * to easily identify it as a separator. This is used eg. by @ref removeSeparator.
     *
     * @param separatorText The text to be shown in the separator widget. Default is QString().
     *
     * @return The new separator widget.
     **/
    virtual QWidget *createSeparator( const QString &separatorText = QString() );

    /**
     * @brief Removes the separator widget in the given QLayoutItem.
     *
     * If there is no separator widget in @p separator that was created using @ref createSeparator
     * the given @p separator won't get removed.
     * @note This function gets called automatically if needed.
     *
     * @param separator The QLayoutItem containing the separator.
     **/
    virtual void removeSeparator( QLayoutItem *separator );

    /**
     * @brief Creates a new @ref DynamicWidget wrapping the given @p widget.
     *
     * This function gets called by @ref addWidget.
     *
     * Overwrite this function to change the DynamicWidget after calling the base implementation
     * to create it. You can eg. change add/remove buttons added besides the @p widget
     * (@ref RemoveButtonsBesideWidgets and/or @ref AddButtonBesideFirstWidget).
     **/
    virtual DynamicWidget *createDynamicWidget( QWidget *widget );

    /**
     * @brief Gets the DynamicWidget wrapping the given @p widget.
     *
     * The DynamicWidget may also contain an add and/or remove button, depending on the options
     * given in the constructor.
     *
     * @param widget The widget to return the wrapping DynamicWidget for.
     *
     * @return The DynamicWidget wrapping the given @p widget.
     **/
    DynamicWidget *dynamicWidgetForWidget( QWidget *widget ) const;

    /** @brief Gets the index of @p widget. */
    int indexOf( QWidget *widget ) const;

    /** @brief Gets a list of all contained dynamic widgets. */
    QList< DynamicWidget* > dynamicWidgets() const;

    /** @brief Gets the content widget of the dynamic widget at @p index casted to WidgetType. */
    template< class WidgetType >
    inline WidgetType *widget( int index ) const {
        return dynamicWidget( index )->contentWidget< WidgetType >();
    };

    /** @brief Gets the content widgets of all dynamic widgets casted to WidgetType. */
    template< class WidgetType >
    QList< WidgetType > widgets() const {
        QList< WidgetType > widgetList;
        foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
            widgetList << dynamicWidget->contentWidget< WidgetType >();
        }
        return widgetList;
    };

    /** @brief Gets the content widget that currently has focus, or NULL if none has focus. */
    template< class WidgetType >
    WidgetType focusedWidget() const {
        foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
            if ( dynamicWidget->contentWidget()->hasFocus() ) {
                return dynamicWidget->contentWidget< WidgetType >();
            }
        }
        return NULL; // No content widget currently has focus
    };

private:
    Q_DECLARE_PRIVATE( AbstractDynamicWidgetContainer )
    Q_DISABLE_COPY( AbstractDynamicWidgetContainer )
};
// Q_DECLARE_OPERATORS_FOR_FLAGS( AbstractDynamicWidgetContainer::DynamicWidgetContainerOptions );

class QFormLayout;
class AbstractDynamicLabeledWidgetContainerPrivate;

/**
 * @brief A widget containing a dynamic list of widgets with labels.
 *
 * This widget contains a dynamic list of widgets with a label for each widget.
 * The label texts can contain their position in the list. @ref setLabelTexts can
 * be used to set special texts for the first n labels. The user can add/remove
 * widgets using buttons, that are automatically added. Override @ref createNewWidget
 * to create new a QWidget instance that should be added when the add button is
 * clicked. You an also override @ref createNewLabelWidget to create a new QWidget
 * instance that should be added as label when the add button is clicked. The default
 * implementation creates a QLabel.
 **/
class PUBLICTRANSPORTHELPER_EXPORT AbstractDynamicLabeledWidgetContainer
    : public AbstractDynamicWidgetContainer
{
    Q_OBJECT
    Q_ENUMS( LabelNumberOptions )

public:
    /** @brief Options for numbering of widget labels. */
    enum LabelNumberOptions {
        IncludeSpecialLabelsInWidgetNumbering, /**< Begin widget numbering with 1
                * for the first label, also if special labels are used. */
        DontIncludeSpecialLabelsInWidgetNumbering /**< Begin widget numbering with 1
                * for the first label without a special label text. */
    };

    explicit AbstractDynamicLabeledWidgetContainer( QWidget* parent = 0,
        RemoveButtonOptions removeButtonOptions = RemoveButtonsBesideWidgets,
        AddButtonOptions addButtonOptions = AddButtonBesideFirstWidget,
        SeparatorOptions separatorOptions = NoSeparator,
        NewWidgetPosition newWidgetPosition = AddWidgetsAtBottom,
        const QString &labelText = "Item %1:" );

    /**
     * @brief Gets the default label text. "%1" gets replaced by the widget number.
     *
     * @see specialLabelTexts
     **/
    QString labelText() const;

    /**
     * @brief Gets special label texts for widgets beginning with the first one.
     *
     * @see labelText
     **/
    QStringList specialLabelTexts() const;

    /**
     * @brief Sets the texts of labels. The first labels get a string from @p specialLabelTexts
     *   if any, the others get @p labelText with "%1" replaced by the widget number.
     *
     * @param labelText The default text, used for labels without a special text.
     *
     * @param specialLabelTexts A list of special label texts.
     *
     * @param labelNumberOptions Whether or not widgets with special labels
     *   should be included in the numbering of widgets without special labels.
     **/
    void setLabelTexts( const QString &labelText,
            const QStringList &specialLabelTexts = QStringList(),
            LabelNumberOptions labelNumberOptions = DontIncludeSpecialLabelsInWidgetNumbering  );

protected:
    AbstractDynamicLabeledWidgetContainer( QWidget *parent,
            AbstractDynamicLabeledWidgetContainerPrivate &dd,
            RemoveButtonOptions removeButtonOptions, AddButtonOptions addButtonOptions,
            NewWidgetPosition newWidgetPosition,
            const QString &labelText );

    /**
     * @brief Adds @p widget to the layout, which gets wrapped by a DynamicWidget.
     *
     * This also creates and adds a default label widget using @ref createNewLabelWidget. The new
     * widget is also focused.
     *
     * @return The new DynamicWidget or NULL, if the maximum widget count is already reached.
     **/
    virtual DynamicWidget *addWidget( QWidget *widget );

    /**
     * @brief Adds @p widget with @p labelWidget as label to the layout, @p widget
     *   gets wrapped by a DynamicWidget.
     *
     * The new widget is also focused.
     *
     * @return The new DynamicWidget or NULL, if the maximum widget count is already reached.
     **/
    virtual DynamicWidget *addWidget( QWidget *labelWidget, QWidget *widget );

    /**
     * @brief Removes @p widget, it's DynamicWidget and it's label widget from the layout.
     *
     * This also removes separators if needed.
     *
     * @return The index of the widget or -1, if the minimum widget count is already reached.
     **/
    virtual int removeWidget( QWidget *widget );

    /**
     * @brief Override to create a new QWidget instance that should be added as label when the
     *   add button is clicked.
     *
     * The default implementation creates a QLabel with the labelText given in the constructor
     * or by @ref setLabelTexts.
     **/
    virtual QWidget *createNewLabelWidget( int widgetIndex );

    /**
     * @brief Override to update the label widget to it's new widget index, eg. update the text
     *   of a QLabel.
     *
     * The default implementation updates the QLabel with the labelText given in the constructor.
     **/
    virtual void updateLabelWidget( QWidget *labelWidget, int widgetIndex );

    /** @brief Gets the label widget used for @p widget. */
    QWidget *labelWidgetFor( QWidget *widget ) const;

private:
    Q_DECLARE_PRIVATE( AbstractDynamicLabeledWidgetContainer )
    Q_DISABLE_COPY( AbstractDynamicLabeledWidgetContainer )
};

class KLineEdit;
class QLabel;
class DynamicLabeledLineEditListPrivate;

/** @brief A widget containing a dynamic list of KLineEdits. */
class PUBLICTRANSPORTHELPER_EXPORT DynamicLabeledLineEditList
    : public AbstractDynamicLabeledWidgetContainer
{
    Q_OBJECT
    Q_PROPERTY( bool clearButtonsShown READ clearButtonsShown WRITE setClearButtonsShown )

public:
    explicit DynamicLabeledLineEditList( QWidget* parent = 0,
            RemoveButtonOptions removeButtonOptions = RemoveButtonsBesideWidgets,
            AddButtonOptions addButtonOptions = AddButtonBesideFirstWidget,
            SeparatorOptions separatorOptions = NoSeparator,
            NewWidgetPosition newWidgetPosition = AddWidgetsAtBottom,
            const QString &labelText = "Item %1:" );

    /** @brief Gets whether or not the line edits in the list have a clear button. */
    bool clearButtonsShown() const;

    /** @brief Sets whether or not the line edits in the list should have a clear button. */
    void setClearButtonsShown( bool clearButtonsShown = true );

    /** @brief Creates and adds a new line edit widget. */
    KLineEdit *addLineEdit();

    /** @brief Gets a list of texts of all contained line edit widgets. */
    QStringList lineEditTexts() const;

    /**
     * @brief Sets the texts of the contained line edit widgets.
     *
     * Line edits are added/removed to match the number of strings in @p lineEditTexts.
     * If @p lineEditTexts contains more strings than the maximum number of line edits, the
     * remaining strings are unused. If @p lineEditTexts contains less strings than the minimum
     * number of line edits, the texts of the remaining line edits stay unchanged.
     * @see maximumWidgetCount
     **/
    void setLineEditTexts( const QStringList &lineEditTexts );

    /**
     * @brief Convenience method to remove all empty line edits.
     *
     * @note This leaves at least the minimum widget count of line edits.
     *
     * @return The number of removed line edits.
     **/
    inline int removeEmptyLineEdits() { return removeLineEditsByText( QString() ); };

    /**
     * @brief Removes all line edits that have @p text as content.
     *
     * @note This leaves at least the minimum widget count of line edits.
     *
     * @return The number of removed line edits.
     **/
    int removeLineEditsByText( const QString &text,
                               Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive);

    /** @brief Gets a list of all contained line edit widgets. */
    QList< KLineEdit* > lineEditWidgets() const;

    /** @brief Gets the label used for @p lineEdit. */
    QLabel *labelFor( KLineEdit *lineEdit ) const;

    /** @brief Gets the currently focused line edit or NULL, if none has focus. */
    KLineEdit *focusedLineEdit() const;

Q_SIGNALS:
    /**
     * @brief This signal is emitted whenever a contained line edit widget emits textEdited.
     *
     * @p lineEditIndex is the index of the line edit widget that has emitted the signal.
     **/
    void textEdited( const QString &text, int lineEditIndex );

    /**
     * @brief This signal is emitted whenever a contained line edit widget emits textChanged.
     *
     * @p lineEditIndex is the index of the line edit widget that has emitted the signal.
     **/
    void textChanged( const QString &text, int lineEditIndex );

protected Q_SLOTS:
    void textEdited( const QString &text );
    void textChanged( const QString &text );

protected:
    virtual QWidget* createNewWidget();

    virtual KLineEdit* createLineEdit();

    /**
     * @brief Removes @p widget, it's DynamicWidget and it's label widget from the layout.
     *
     * This also removes separators if needed.
     *
     * @return The index of the widget or -1, if the minimum widget count is already reached.
     **/
    virtual int removeWidget( QWidget* widget );

private:
    Q_DECLARE_PRIVATE( DynamicLabeledLineEditList )
    Q_DISABLE_COPY( DynamicLabeledLineEditList )
};

#endif // Multiple inclusion guard
