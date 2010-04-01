/*
*   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef DYNAMICWIDGET_HEADER
#define DYNAMICWIDGET_HEADER

#include <QWidget>

class QLayoutItem;
class QToolButton;
class AbstractDynamicWidgetContainer;
class DynamicWidgetPrivate;
/** @brief Wraps widgets in AbstractDynamicWidgetContainer and adds buttons.
* Provides a widget which embeds a "content widget" and a remove and/or add
* button. It's used by @ref DynamicWidgetContainer. To get the content widget 
* use @ref contentWidget. The buttons can be taken out of the DynamicWidget
* by using @ref takeRemoveButton / @ref takeAddButton. They can then be put into
* another widget, the connections remain.
* @note You cannot create DynamicWidgets yourself and mostly don't need to use 
* it at all, that's up to @ref DynamicWidgetContainer.
* @see AbstractDynamicWidgetContainer */
class DynamicWidget : public QWidget {
    Q_OBJECT
    Q_ENUMS( ButtonType )
    friend class AbstractDynamicWidgetContainer; // To access the constructor
    
    public:
	/** Button types. */
	enum ButtonType {
	    ButtonSpacer, /**< A spacer item with the size of a button */
	    RemoveButton, /**< A remove button */
	    AddButton /**< An add button */
	};

	/** The destructor deleted the content widget. */
	~DynamicWidget();

	/** Gets the content widget of this dynamic widget. */
	QWidget *contentWidget() const;
	/** Gets the content widget of this dynamic widget casted to WidgetType. */
	template< class WidgetType >
	WidgetType contentWidget() const {
	    return qobject_cast< WidgetType >( contentWidget() );
	};
	/** Replaces the current content widget with @p contentWidget. The old
	* content widget gets deleted. */
	void replaceContentWidget( QWidget *contentWidget );
	
	/** Adds a button of type @p buttonType.
	* @return A pointer to the added button or NULL if no button was added.
	* For example @ref ButtonSpacer adds a spacer but returns NULL. */
	QToolButton *addButton( AbstractDynamicWidgetContainer *container,
				ButtonType buttonType );
	
	/** Returns a pointer to the remove button if any. */
	QToolButton *removeButton() const;
	/** Returns a pointer to the add button if any. */
	QToolButton *addButton() const;

	/** Takes the remove button out of the DynamicWidget's layout if there is
	* a remove button for this DynamicWidget. It can then be put into another
	* widget, without disconnecting it's clicked signal. @ref removeButton
	* will still return a pointer to the remove button, you can also still 
	* use @ref setRemoveButtonIcon. It can also be deleted.
	* @return The remove button or NULL, if there is no remove button for 
	* this DynamicWidget or the remove button has already been taken. */
	QToolButton *takeRemoveButton();
	
	/** Takes the add button out of the DynamicWidget's layout if there is
	* an add button for this DynamicWidget. It can then be put into another
	* widget, without disconnecting it's clicked signal. @ref addButton
	* will still return a pointer to the remove button, you can also still
	* use @ref setAddButtonIcon. It can also be deleted.
	* @return The add button or NULL, if there is no add button for this 
	* DynamicWidget or the add button has already been taken. */
	QToolButton *takeAddButton();

	/** Sets the spacing between buttons. */
	void setButtonSpacing( int spacing = 1 );
	/** Sets the alignment of the buttons. */
	void setButtonAlignment( Qt::Alignment alignment = Qt::AlignRight | Qt::AlignTop );
	/** Sets auto-raising of the buttons to @p autoRaiseButtons. */
	void setAutoRaiseButtons( bool autoRaiseButtons = true );
	/** Sets the icon of the remove button to @p removeButtonIcon if any. */
	void setRemoveButtonIcon( const QString &removeButtonIcon = "list-remove" );
	/** Sets the icon of the add button to @p addButtonIcon if any. */
	void setAddButtonIcon( const QString &addButtonIcon = "list-add" );

    Q_SIGNALS:
	/** The content widget was replaced by @p newContentWidget. */
	void widgetReplaced( QWidget *newContentWidget );
	/** The remove button was clicked. */
	void removeClicked();
	/** The add button was clicked. */
	void addClicked();

    protected Q_SLOTS:
	/** Used if the add and/or remove buttons are taken out of the layout
	* using @ref takeRemoveButton / @ref takeAddButton to be notified if the
	* buttons get deleted. */
	void buttonDestroyed( QObject *object );

    protected:
	/** Creates a new DynamicWidget with @p contentWidget. 
	* @param contentWidget The content widget.
	* @param container Used as parent and to get initial icons for the 
	* add/remove buttons.
	* @param buttonTypes A list of buttons to be created.
	* @see ButtonType
	* @see AbstractDynamicWidgetContainer */
	DynamicWidget( QWidget *contentWidget, AbstractDynamicWidgetContainer *container,
		       QList<ButtonType> buttonTypes = QList<ButtonType>() << RemoveButton );
	
	DynamicWidgetPrivate* const d_ptr;

    private:
	Q_DECLARE_PRIVATE( DynamicWidget )
	Q_DISABLE_COPY( DynamicWidget )
};

class AbstractDynamicWidgetContainerPrivate;
/** @brief A widget containing a dynamic list of widgets.
* This widget contains a dynamic list of widgets. The user can add/remove widgets
* using buttons, that are automatically added. Override @ref createNewWidget to 
* create new a QWidget instance that should be added when the add button is clicked. */
class AbstractDynamicWidgetContainer : public QWidget {
    Q_OBJECT
    Q_ENUMS( ContainerLayout SeparatorOptions RemoveButtonOptions AddButtonOptions )
    Q_PROPERTY( int buttonSpacing READ buttonSpacing WRITE setButtonSpacing )
    Q_PROPERTY( Qt::Alignment buttonAlignment READ buttonAlignment WRITE setButtonAlignment )
    Q_PROPERTY( bool autoRaiseButtons READ autoRaiseButtons WRITE setAutoRaiseButtons )
    Q_PROPERTY( Qt::Alignment buttonAlignment READ buttonAlignment WRITE setButtonAlignment )
    Q_PROPERTY( SeparatorOptions separatorOptions READ separatorOptions WRITE setSeparatorOptions )
    friend class DynamicWidget; // To access the constructor of DynamicWidget

    public:
	/** Options for separators between widgets. */
	enum SeparatorOptions {
	    NoSeparator = 0, /**< Don't add separators between widgets. */
	    ShowSeparators /**< Add separators between widgets. */
	};

	/** Options for the buttons to remove widgets. */
	enum RemoveButtonOptions {
	    NoRemoveButton = 0, /**< Don't show remove buttons. You can manually remove
		* widgets by calling @ref removeWidget(). */
	    RemoveButtonsBesideWidgets, /**< Show a remove button beside each widget. */
	    RemoveButtonAfterLastWidget /**< Show a remove button after the last widget.
		* @note This creates only one remove button to remove the last widget. */
	};
	
	/** Options for the buttons to add new widgets. */
	enum AddButtonOptions {
	    NoAddButton = 0, /**< Don't show add buttons. You can manually add widgets
		* by calling @ref addWidget(). */
	    AddButtonBesideFirstWidget, /**< Show an add button beside the first widget. */
	    AddButtonAfterLastWidget /**< Show an add button after the last widget. */
	};
	
	AbstractDynamicWidgetContainer(
		RemoveButtonOptions removeButtonOptions = RemoveButtonsBesideWidgets,
		AddButtonOptions addButtonOptions = AddButtonBesideFirstWidget,
		SeparatorOptions separatorOptions = NoSeparator, QWidget *parent = 0 );
	~AbstractDynamicWidgetContainer();

	/** @warning The new @p separatorOptions will only be applied for newly 
	* added widgets. */
	void setSeparatorOptions( SeparatorOptions separatorOptions = NoSeparator );
	/** Returns the current separator options. */
	SeparatorOptions separatorOptions() const;
	
	/** Set a custom add button, to let it automatically be set enabled/disabled
	* depending on the current/maximum numbers of widgets. It also uses the
	* clicked signal of @p addButton to add new widgets. */
	void setCustomAddButton( QToolButton *addButton );
	/** Gets the add button or NULL if none is set. If created with 
	* @ref AddButtonBesideFirstWidget, the button can be deleted, when the 
	* first widget gets removed.
	* @note This also returns a pointer to the add button, if a custom add 
	* button has been set using @ref setCustomAddButton. */
	QToolButton *addButton() const;
	/** Gets the remove button if only one is set (using
	* @ref RemoveButtonAfterLastWidget). Otherwise this returns NULL. */
	QToolButton *removeButton() const;

	/** Removes all contained widgets but leaves @ref minimumWidgetCount() widgets. */
	void removeAllWidgets();

	/** The minimum number of widgets. If this number is reached no more widgets
	* can be removed and all remove buttons get disabled. */
	int minimumWidgetCount() const;
	/** The maximum number of widgets. If this number is reached no more widgets
	* can be added and the add button gets disabled. */
	int maximumWidgetCount() const;
	/** Sets the minimum and maximum widget count. It also makes sure that
	* the actual widget count is in the given range by adding or removing
	* widgets if needed if @p putIntoRange is true. 
	* @return The number of added widgets (negative, if widgets were removed). */
	int setWidgetCountRange( int minWidgetCount = 0, int maxWidgetCount = -1,
				  bool putIntoRange = true );
	int buttonSpacing() const;
	Qt::Alignment buttonAlignment() const;
	bool autoRaiseButtons() const;
	QString removeButtonIcon() const;
	QString addButtonIcon() const;
	void setButtonSpacing( int spacing = 1 );
	void setButtonAlignment( Qt::Alignment alignment = Qt::AlignRight | Qt::AlignTop );
	void setAutoRaiseButtons( bool autoRaiseButtons = true );
	void setRemoveButtonIcon( const QString &removeButtonIcon = "list-remove" );
	void setAddButtonIcon( const QString &addButtonIcon = "list-add" );
	
    Q_SIGNALS:
	/** A @p widget has been added. */
	void added( QWidget *widget );
	/** A @p widget has been removed. */
	void removed( QWidget *widget, int widgetIndex );

    protected Q_SLOTS:
	/** This slot is connected to the removeClicked signal of every contained
	* @ref DynamicWidget. It uses sender() to find out which widget to remove. */
	void removeWidget();
	/** Removes the last widget. This slot is connected to the remove button, 
	* if created using @ref RemoveButtonAfterLastWidget. */
	void removeLastWidget();
	/** This slot is connected to the clicked()-signal of the add button to 
	* add a new widget. The new widget is created using @ref createNewWidget
	* and added using @ref addWidget. */
	void createAndAddWidget();
	
    protected:
	AbstractDynamicWidgetContainerPrivate* const d_ptr;
	AbstractDynamicWidgetContainer( AbstractDynamicWidgetContainerPrivate &dd,
					RemoveButtonOptions removeButtonOptions,
					AddButtonOptions addButtonOptions, QWidget *parent );
	
	/** Reimplemented to update the enabled state of the add button. */
	virtual void changeEvent( QEvent *event );
	
	/** Override to create a new QWidget instance that should be added 
	* when the add button is clicked. */
	virtual QWidget *createNewWidget() = 0;
	/** Adds @p widget to the layout, which gets wrapped by a DynamicWidget.
	* The new widget is also focused.
	* @return The new DynamicWidget or NULL, if the maximum widget count is 
	* already reached. */
	virtual DynamicWidget *addWidget( QWidget *widget );
	/** Removes @p widget and it's DynamicWidget from the layout.
	* This also removes separators if needed.
	* @return The index of the widget or -1, if the minimum widget count is
	* already reached. */
	virtual int removeWidget( QWidget *widget );
	
	virtual QWidget *createSeparator( const QString &separatorText = QString() );
	virtual void removeSeparator( QLayoutItem *separator );
	
	virtual DynamicWidget *createDynamicWidget( QWidget *widget );
	DynamicWidget *dynamicWidgetForWidget( QWidget *widget ) const;
	/** Gets the index of @p widget. */
	int indexOf( QWidget *widget ) const;
	
	/** Gets a list of all contained dynamic widgets. */
	QList< DynamicWidget* > dynamicWidgets() const;
	/** Gets the content widgets of all dynamic widgets casted to WidgetType. */
	template< class WidgetType >
	QList< WidgetType > widgets() const {
	    QList< WidgetType > widgetList;
	    foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() )
		widgetList << dynamicWidget->contentWidget< WidgetType >();
	    return widgetList;
	};

	/** Gets the content widget that currently has focus, or NULL if none has focus. */
	template< class WidgetType >
	WidgetType focusedWidget() const {
	    foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
		if ( dynamicWidget->contentWidget()->hasFocus() )
		    return dynamicWidget->contentWidget< WidgetType >();
	    }
	    return NULL; // No content widget currently has focus
	};
	
    private:
	Q_DECLARE_PRIVATE( AbstractDynamicWidgetContainer )
	Q_DISABLE_COPY( AbstractDynamicWidgetContainer )
};

class QFormLayout;
class AbstractDynamicLabeledWidgetContainerPrivate;
/** @brief A widget containing a dynamic list of widgets with labels.
* This widget contains a dynamic list of widgets with a label for each widget. 
* The label texts can contain their position in the list. @ref setLabelTexts can
* be used to set special texts for the first n labels. The user can add/remove 
* widgets using buttons, that are automatically added. Override @ref createNewWidget 
* to create new a QWidget instance that should be added when the add button is 
* clicked. You an also override @ref createNewLabelWidget to create a new QWidget 
* instance that should be added as label when the add button is clicked. The default 
* implementation creates a QLabel. */
class AbstractDynamicLabeledWidgetContainer : public AbstractDynamicWidgetContainer {
    Q_OBJECT
    Q_ENUMS( LabelNumberOptions )
    
    public:
	enum LabelNumberOptions {
	    IncludeSpecialLabelsInWidgetNumbering,
	    /**< Begin widget numbering with 1 for the first label, also if special labels are used. */
	    DontIncludeSpecialLabelsInWidgetNumbering
	    /**< Begin widget numbering with 1 for the first label without a special label text. */
	};
	
	AbstractDynamicLabeledWidgetContainer(
		RemoveButtonOptions removeButtonOptions = RemoveButtonsBesideWidgets,
		AddButtonOptions addButtonOptions = AddButtonBesideFirstWidget,
		SeparatorOptions separatorOptions = NoSeparator,
		const QString &labelText = "Item %1:",
		QWidget *parent = 0 );

	/** Gets the default label text. "%1" gets replaced by the widget number.
	* @see specialLabelTexts */
	QString labelText() const;
	/** Gets special label texts for widgets beginning with the first one.
	* @see labelText */
	QStringList specialLabelTexts() const;
	/** Sets the texts of labels. The first labels get a string from
	* @p specialLabelTexts if any, the others get @p labelText with "%1" 
	* replaced by the widget number.
	* @param labelText The default text, used for labels without a special text.
	* @param specialLabelTexts A list of special label texts.
	* @param labelNumberOptions Whether or not widgets with special labels
	* should be included in the numbering of widgets without special labels. */
	void setLabelTexts( const QString &labelText,
			    const QStringList &specialLabelTexts = QStringList(),
			    LabelNumberOptions labelNumberOptions
				= DontIncludeSpecialLabelsInWidgetNumbering  );

    protected:
	AbstractDynamicLabeledWidgetContainer( AbstractDynamicLabeledWidgetContainerPrivate &dd,
					       RemoveButtonOptions removeButtonOptions,
					       AddButtonOptions addButtonOptions,
					       const QString &labelText,
					       QWidget *parent );

	/** Adds @p widget to the layout, which gets wrapped by a DynamicWidget.
	* This also creates and adds a default label widget using 
	* @ref createNewLabelWidget. The new widget is also focused.
	* @return The new DynamicWidget or NULL, if the maximum widget count is
	* already reached. */
	virtual DynamicWidget *addWidget( QWidget *widget );
	/** Adds @p widget with @p labelWidget as label to the layout, @p widget
	* gets wrapped by a DynamicWidget. The new widget is also focused.
	* @return The new DynamicWidget or NULL, if the maximum widget count is
	* already reached. */
	virtual DynamicWidget *addWidget( QWidget *labelWidget, QWidget *widget );
	/** Removes @p widget, it's DynamicWidget and it's label widget from the layout.
	* This also removes separators if needed.
	* @return The index of the widget or -1, if the minimum widget count is
	* already reached. */
	virtual int removeWidget( QWidget *widget );
	/** Override to create a new QWidget instance that should be added as
	* label when the add button is clicked. The default implementation creates
	* a QLabel with the labelText given in the constructor or by @ref setLabelTexts. */
	virtual QWidget *createNewLabelWidget( int widgetIndex );
	/** Override to update the label widget to it's new widget index, eg.
	* update the text of a QLabel. The default implementation updates
	* the QLabel with the labelText given in the constructor. */
	virtual void updateLabelWidget( QWidget *labelWidget, int widgetIndex );
	/** Gets the label widget used for @p widget. */
	QWidget *labelWidgetFor( QWidget *widget ) const;

    private:
	Q_DECLARE_PRIVATE( AbstractDynamicLabeledWidgetContainer )
	Q_DISABLE_COPY( AbstractDynamicLabeledWidgetContainer )
};

class KLineEdit;
class QLabel;
class DynamicLabeledLineEditListPrivate;
/** @brief A widget containing a dynamic list of KLineEdits. */
class DynamicLabeledLineEditList : public AbstractDynamicLabeledWidgetContainer {
    Q_OBJECT
    Q_PROPERTY( bool clearButtonsShown READ clearButtonsShown WRITE setClearButtonsShown )
    
    public:
	DynamicLabeledLineEditList( 
		RemoveButtonOptions removeButtonOptions = RemoveButtonsBesideWidgets,
		AddButtonOptions addButtonOptions = AddButtonBesideFirstWidget,
		SeparatorOptions separatorOptions = NoSeparator,
		const QString &labelText = "Item %1:", QWidget* parent = 0 );
		
	/** Gets whether or not the line edits in the list have a clear button. */
	bool clearButtonsShown() const;
	/** Sets whether or not the line edits in the list should have a clear button. */
	void setClearButtonsShown( bool clearButtonsShown = true );

	/** Creates and adds a new line edit widget. */
	KLineEdit *addLineEdit();
	
	/** Gets a list of texts of all contained line edit widgets. */
	QStringList lineEditTexts() const;
	/** Sets the texts of the contained line edit widgets. Line edits are 
	* added/removed to match the number of strings in @p lineEditTexts. 
	* If @p lineEditTexts contains more strings than the maximum number of 
	* line edits, the remaining strings are unused. If @p lineEditTexts
	* contains less strings than the minimum number of line edits, the texts
	* of the remaining line edits stay unchanged.
	* @see maximumWidgetCount */
	void setLineEditTexts( const QStringList &lineEditTexts );

	/** Convenience method to remove all empty line edits.
	* @note This leaves at least the minimum widget count of line edits.
	* @return The number of removed line edits. */
	inline int removeEmptyLineEdits() { return removeLineEditsByText( QString() ); };
	/** Removes all line edits that have @p text as content.
	* @note This leaves at least the minimum widget count of line edits. 
	* @return The number of removed line edits. */
	int removeLineEditsByText( const QString &text,
		Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive);

	/** Gets a list of all contained line edit widgets. */
	QList< KLineEdit* > lineEditWidgets() const;
	/** Gets the label used for @p lineEdit. */
	QLabel *labelFor( KLineEdit *lineEdit ) const;
	/** Gets the currently focused line edit or NULL, if none has focus. */
	KLineEdit *focusedLineEdit() const;

    Q_SIGNALS:
	/** This signal is emitted whenever a contained line edit widget emits
	* textEdited. @p lineEditIndex is the index of the line edit widget that
	* has emitted the signal. */
	void textEdited( const QString &text, int lineEditIndex );
	/** This signal is emitted whenever a contained line edit widget emits
	* textChanged. @p lineEditIndex is the index of the line edit widget that
	* has emitted the signal. */
	void textChanged( const QString &text, int lineEditIndex );
	
    protected Q_SLOTS:
	void textEdited( const QString &text );
	void textChanged( const QString &text );
	
    protected:
	virtual QWidget* createNewWidget();
	/** Removes @p widget, it's DynamicWidget and it's label widget from the layout.
	* This also removes separators if needed.
	* @return The index of the widget or -1, if the minimum widget count is
	* already reached. */
	virtual int removeWidget( QWidget* widget );

    private:
	Q_DECLARE_PRIVATE( DynamicLabeledLineEditList )
	Q_DISABLE_COPY( DynamicLabeledLineEditList )
};

#endif // Multiple inclusion guard
