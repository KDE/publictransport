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

#include "dynamicwidget.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QToolButton>
#include <QStyleOption>
#include <QFrame>
#include <QLabel>
#include <QSignalMapper>
#include <qcoreevent.h>

#include <KIcon>
#include <KDebug>
#include <KLineEdit>


class DynamicWidgetPrivate
{
public:
	DynamicWidgetPrivate( QWidget *contentWidget )
			: contentWidget(contentWidget), buttonsWidget(0), removeButton(0), addButton(0) {};

	int toolButtonSpacing() const {
		int width;
		if ( removeButton )
			width = removeButton->width();
		else {
			QStyleOptionToolButton option;
			int iconSize = contentWidget->style()->pixelMetric( QStyle::PM_SmallIconSize );
			option.iconSize = QSize( iconSize, iconSize );
			option.toolButtonStyle = Qt::ToolButtonIconOnly;
			width = contentWidget->style()->sizeFromContents(
					QStyle::CT_ToolButton, &option, option.iconSize ).width();
		}

		int spacing = contentWidget->style()->pixelMetric( QStyle::PM_LayoutHorizontalSpacing );
		return width + ( spacing == -1 ? 1 : spacing );
	}

	QWidget *contentWidget, *buttonsWidget;
	QToolButton *removeButton, *addButton;
};

DynamicWidget::DynamicWidget( QWidget* contentWidget, AbstractDynamicWidgetContainer *container,
							  QList<ButtonType> buttonTypes )
		: QWidget(container), d_ptr(new DynamicWidgetPrivate(contentWidget))
{
	QHBoxLayout *l = new QHBoxLayout( this );
	l->setContentsMargins( 0, 0, 0, 0 );
	l->addWidget( contentWidget );

	if ( !buttonTypes.isEmpty() ) {
		d_ptr->buttonsWidget = new QWidget( this );
		QHBoxLayout *buttonLayout = new QHBoxLayout( d_ptr->buttonsWidget );
		buttonLayout->setSpacing( 1 );
		buttonLayout->setContentsMargins( 0, 0, 0, 0 );
		d_ptr->buttonsWidget->setLayout( buttonLayout );
		l->addWidget( d_ptr->buttonsWidget );
		l->setAlignment( d_ptr->buttonsWidget, Qt::AlignRight | Qt::AlignTop );

		foreach( ButtonType buttonType, buttonTypes ) {
			addButton( container, buttonType );
		}
	}
}

DynamicWidget::~DynamicWidget()
{
	delete contentWidget();
	delete d_ptr;
}

QToolButton* DynamicWidget::addButton( AbstractDynamicWidgetContainer *container,
									   ButtonType buttonType )
{
	Q_D( DynamicWidget );
	QHBoxLayout *buttonLayout = dynamic_cast< QHBoxLayout* >( d->buttonsWidget->layout() );
	switch ( buttonType ) {
	case RemoveButton:
		if ( d->removeButton ) {
			return NULL;
		}
		d->removeButton = new QToolButton( this );
		d->removeButton->setIcon( KIcon(container->removeButtonIcon()) );
		buttonLayout->addWidget( d_ptr->removeButton );
		connect( d_ptr->removeButton, SIGNAL(clicked()), this, SIGNAL(removeClicked()) );
		return d->removeButton;

	case AddButton:
		if ( d->addButton ) {
			return NULL;
		}
		d->addButton = new QToolButton( this );
		d->addButton->setIcon( KIcon(container->addButtonIcon()) );
		buttonLayout->addWidget( d->addButton );
		connect( d->addButton, SIGNAL(clicked()), this, SIGNAL(addClicked()) );
		return d->addButton;

	case ButtonSpacer:
		buttonLayout->addItem( new QSpacerItem( d->toolButtonSpacing(), 0 ) );
		return NULL;

	default:
		return NULL;
	}
}

void DynamicWidget::setButtonAlignment( Qt::Alignment alignment )
{
	Q_D( DynamicWidget );
	if ( d->buttonsWidget ) {
		layout()->setAlignment( d->buttonsWidget, alignment );
	}
}

void DynamicWidget::setButtonSpacing( int spacing )
{
	Q_D( DynamicWidget );
	if ( d->buttonsWidget ) {
		d->buttonsWidget->layout()->setSpacing( spacing );
	}
}

void DynamicWidget::setAutoRaiseButtons( bool autoRaiseButtons )
{
	Q_D( DynamicWidget );
	if ( d->removeButton ) {
		d->removeButton->setAutoRaise( autoRaiseButtons );
	}
	if ( d->addButton ) {
		d->addButton->setAutoRaise( autoRaiseButtons );
	}
}

void DynamicWidget::setRemoveButtonIcon( const QString& removeButtonIcon )
{
	Q_D( DynamicWidget );
	if ( d->removeButton ) {
		d->removeButton->setIcon( KIcon(removeButtonIcon) );
	}
}

void DynamicWidget::setAddButtonIcon( const QString& addButtonIcon )
{
	Q_D( DynamicWidget );
	if ( d->addButton ) {
		d->addButton->setIcon( KIcon(addButtonIcon) );
	}
}

void DynamicWidget::replaceContentWidget( QWidget* contentWidget )
{
	Q_D( DynamicWidget );
	QHBoxLayout *l = static_cast< QHBoxLayout* >( layout() );
	Q_ASSERT_X( l, "DynamicWidget::replaceContentWidget", "No QHBoxLayout found" );
	l->removeWidget( d->contentWidget );
	delete d->contentWidget;

	l->insertWidget( 0, contentWidget );
	d->contentWidget = contentWidget;

	emit widgetReplaced( contentWidget );
}

QWidget* DynamicWidget::contentWidget() const
{
	Q_D( const DynamicWidget );
	return d->contentWidget;
}

QToolButton* DynamicWidget::removeButton() const
{
	Q_D( const DynamicWidget );
	return d->removeButton;
}

QToolButton* DynamicWidget::addButton() const
{
	Q_D( const DynamicWidget );
	return d->addButton;
}

QToolButton* DynamicWidget::takeRemoveButton()
{
	Q_D( DynamicWidget );
	if ( !d->buttonsWidget || !d->removeButton
				|| !d->buttonsWidget->children().contains( d->removeButton ) ) {
		return NULL;
	}

	QHBoxLayout *buttonLayout = dynamic_cast< QHBoxLayout* >( d->buttonsWidget->layout() );
	buttonLayout->removeWidget( d->removeButton );

	// Watch for destroy of the remove button
	connect( d->removeButton, SIGNAL(destroyed(QObject*)), this, SLOT(buttonDestroyed(QObject*)) );
	return d->removeButton;
}

QToolButton* DynamicWidget::takeAddButton()
{
	Q_D( DynamicWidget );
	if ( !d->buttonsWidget || !d->addButton
				|| !d->buttonsWidget->children().contains( d->addButton ) ) {
		return NULL;
	}

	QHBoxLayout *buttonLayout = dynamic_cast< QHBoxLayout* >( d->buttonsWidget->layout() );
	buttonLayout->removeWidget( d->addButton );

	// Watch for destroy of the add button
	connect( d->addButton, SIGNAL(destroyed(QObject*)), this, SLOT(buttonDestroyed(QObject*)) );
	return d->addButton;
}

void DynamicWidget::buttonDestroyed( QObject* object )
{
	Q_D( DynamicWidget );
	if ( object == d->removeButton ) {
		d->removeButton = NULL;
	} else if ( object == d->addButton ) {
		d->addButton = NULL;
	}
}


class AbstractDynamicWidgetContainerPrivate
{
	Q_DECLARE_PUBLIC( AbstractDynamicWidgetContainer )

public:
	AbstractDynamicWidgetContainerPrivate( AbstractDynamicWidgetContainer *q )
				: contentWidget(q), addButton(0), removeButton(0), q_ptr(q) {
		addButtonIcon = "list-add";
		removeButtonIcon = "list-remove";
		autoRaiseButtons = false;
		minWidgetCount = 0;
		maxWidgetCount = -1; // unlimited
	};

	virtual ~AbstractDynamicWidgetContainerPrivate() {
	};

	virtual QLayout* createContentLayout( QWidget *parent ) {
		QVBoxLayout *vBoxLayout = new QVBoxLayout( parent );
		vBoxLayout->setSpacing( 0 );
		vBoxLayout->setContentsMargins( 0, 0, 0, 0 );
		return vBoxLayout;
	};

	void createLayout(
	    AbstractDynamicWidgetContainer::RemoveButtonOptions removeButtonOptions,
	    AbstractDynamicWidgetContainer::AddButtonOptions addButtonOptions ) {
		Q_Q( AbstractDynamicWidgetContainer );
		if ( removeButtonOptions == AbstractDynamicWidgetContainer::RemoveButtonAfterLastWidget
				|| addButtonOptions == AbstractDynamicWidgetContainer::AddButtonAfterLastWidget ) {
			contentWidget = new QWidget( q );

			QHBoxLayout *buttonLayout = new QHBoxLayout;
			buttonLayout->setContentsMargins( 0, 0, 0, 0 );
			if ( addButtonOptions == AbstractDynamicWidgetContainer::AddButtonAfterLastWidget ) {
				addButton = new QToolButton( q );
				addButton->setIcon( KIcon("list-add") );
				buttonLayout->addWidget( addButton );
				q->connect( addButton, SIGNAL(clicked()), q, SLOT(createAndAddWidget()) );
			}
			if ( removeButtonOptions == AbstractDynamicWidgetContainer::RemoveButtonAfterLastWidget ) {
				removeButton = new QToolButton( q );
				removeButton->setIcon( KIcon("list-remove") );
				buttonLayout->addWidget( removeButton );
				q->connect( removeButton, SIGNAL(clicked()), q, SLOT(removeLastWidget()) );
			}
			buttonLayout->addSpacerItem( new QSpacerItem(0, 0, QSizePolicy::Expanding) );

			QVBoxLayout *mainLayout = new QVBoxLayout( q );
			mainLayout->setContentsMargins( 0, 0, 0, 0 );
			mainLayout->addWidget( contentWidget );
			if ( newWidgetPosition == AbstractDynamicWidgetContainer::AddWidgetsAtTop ) {
				mainLayout->insertLayout( 0, buttonLayout );
			} else {
				mainLayout->addLayout( buttonLayout );
			}

			updateButtonStates();
		}

		createContentLayout( contentWidget );
	};

	void init( AbstractDynamicWidgetContainer::RemoveButtonOptions removeButtonOptions,
	           AbstractDynamicWidgetContainer::AddButtonOptions addButtonOptions,
	           AbstractDynamicWidgetContainer::SeparatorOptions separatorOptions,
			   AbstractDynamicWidgetContainer::NewWidgetPosition newWidgetPosition ) {
		showRemoveButtons = removeButtonOptions ==
				AbstractDynamicWidgetContainer::RemoveButtonsBesideWidgets;
		showAddButton = addButtonOptions ==
				AbstractDynamicWidgetContainer::AddButtonBesideFirstWidget;
		showSeparators = separatorOptions ==
				AbstractDynamicWidgetContainer::ShowSeparators;
		this->newWidgetPosition = newWidgetPosition;
	};

	void updateButtonStates() {
		Q_Q( AbstractDynamicWidgetContainer );
		if ( addButton ) {
			addButton->setEnabled( q->isEnabled() && ( maxWidgetCount == -1
									|| dynamicWidgets.count() < maxWidgetCount ) );
		}

		if ( removeButton ) {
			removeButton->setEnabled( q->isEnabled() && !dynamicWidgets.isEmpty()
									&& dynamicWidgets.count() > minWidgetCount );
		} else if ( showRemoveButtons ) {
			// Remove buttons are shown beside the content widgets
			bool enable = q->isEnabled() && dynamicWidgets.count() > minWidgetCount;
			foreach( DynamicWidget *dynamicWidget, dynamicWidgets ) {
				QToolButton *removeButton = dynamicWidget->removeButton();
				if ( removeButton ) {
					removeButton->setEnabled( enable );
				}
			}
		}
	};

	QWidget *contentWidget;
	QList< DynamicWidget* > dynamicWidgets;
	QToolButton *addButton, *removeButton;
	int minWidgetCount, maxWidgetCount, buttonSpacing;
	bool showRemoveButtons, showAddButton, showSeparators;
	bool autoRaiseButtons;
	QString removeButtonIcon, addButtonIcon;
	Qt::Alignment buttonAlignment;
	AbstractDynamicWidgetContainer::NewWidgetPosition newWidgetPosition;

protected:
	AbstractDynamicWidgetContainer* const q_ptr;
};

AbstractDynamicWidgetContainer::AbstractDynamicWidgetContainer( QWidget *parent,
	RemoveButtonOptions removeButtonOptions, AddButtonOptions addButtonOptions,
	SeparatorOptions separatorOptions, NewWidgetPosition newWidgetPosition )
		: QWidget(parent), d_ptr(new AbstractDynamicWidgetContainerPrivate(this))
{
	d_ptr->init( removeButtonOptions, addButtonOptions, separatorOptions, newWidgetPosition );
	d_ptr->createLayout( removeButtonOptions, addButtonOptions );
}

AbstractDynamicWidgetContainer::AbstractDynamicWidgetContainer( QWidget* parent,
	AbstractDynamicWidgetContainerPrivate& dd,
	RemoveButtonOptions removeButtonOptions, AddButtonOptions addButtonOptions,
	NewWidgetPosition newWidgetPosition )
		: QWidget(parent), d_ptr(&dd)
{
	d_ptr->createLayout( removeButtonOptions, addButtonOptions );
	d_ptr->newWidgetPosition = newWidgetPosition;
}

AbstractDynamicWidgetContainer::~AbstractDynamicWidgetContainer()
{
	delete d_ptr;
}

DynamicWidget* AbstractDynamicWidgetContainer::dynamicWidget(int index) const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->dynamicWidgets[ index ];
}

void AbstractDynamicWidgetContainer::setSeparatorOptions( SeparatorOptions separatorOptions )
{
	Q_D( AbstractDynamicWidgetContainer );
	d->showSeparators = separatorOptions == AbstractDynamicWidgetContainer::ShowSeparators;
}

AbstractDynamicWidgetContainer::SeparatorOptions AbstractDynamicWidgetContainer::separatorOptions() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->showSeparators ? ShowSeparators : NoSeparator;
}

void AbstractDynamicWidgetContainer::changeEvent( QEvent* event )
{
	Q_D( AbstractDynamicWidgetContainer );
	if ( event->type() == QEvent::EnabledChange ) {
		d->updateButtonStates();
	}
	QWidget::changeEvent( event );
}

void AbstractDynamicWidgetContainer::createAndAddWidget()
{
	addWidget( createNewWidget() );
}

void AbstractDynamicWidgetContainer::setCustomAddButton( QToolButton* addButton )
{
	Q_D( AbstractDynamicWidgetContainer );
	if ( d->addButton ) {
		disconnect( d->addButton, SIGNAL( clicked() ), this, SLOT( createAndAddWidget() ) );
	}

	d->addButton = addButton;
	d->updateButtonStates();
	connect( d->addButton, SIGNAL( clicked() ), this, SLOT( createAndAddWidget() ) );
}

void AbstractDynamicWidgetContainer::setButtonSpacing( int spacing )
{
	Q_D( AbstractDynamicWidgetContainer );
	d->buttonSpacing = spacing;
	foreach( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
		dynamicWidget->setButtonSpacing( spacing );
	}
}

void AbstractDynamicWidgetContainer::setButtonAlignment( Qt::Alignment alignment )
{
	Q_D( AbstractDynamicWidgetContainer );
	d->buttonAlignment = alignment;
	foreach( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
		dynamicWidget->setButtonAlignment( alignment );
	}
}

void AbstractDynamicWidgetContainer::setAutoRaiseButtons( bool autoRaiseButtons )
{
	Q_D( AbstractDynamicWidgetContainer );
	d->autoRaiseButtons = autoRaiseButtons;
	foreach( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
		dynamicWidget->setAutoRaiseButtons( autoRaiseButtons );
	}
}

void AbstractDynamicWidgetContainer::setRemoveButtonIcon( const QString& removeButtonIcon )
{
	Q_D( AbstractDynamicWidgetContainer );
	d->removeButtonIcon = removeButtonIcon;
	foreach( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
		dynamicWidget->setRemoveButtonIcon( removeButtonIcon );
	}
}

void AbstractDynamicWidgetContainer::setAddButtonIcon( const QString& addButtonIcon )
{
	Q_D( AbstractDynamicWidgetContainer );
	d->addButtonIcon = addButtonIcon;
	foreach( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
		dynamicWidget->setAddButtonIcon( addButtonIcon );
	}
}

DynamicWidget* AbstractDynamicWidgetContainer::createDynamicWidget( QWidget *widget )
{
	Q_D( AbstractDynamicWidgetContainer );

	QList< DynamicWidget::ButtonType > buttons;
	if ( d->showAddButton && d->dynamicWidgets.isEmpty() ) {
		buttons << DynamicWidget::AddButton;
	} else if ( d->showRemoveButtons ) {
// 	if ( d->dynamicWidgets.count() >= d->minWidgetCount )
		buttons << DynamicWidget::RemoveButton;
// 	else
// 	    buttons << DynamicWidget::ButtonSpacer;
	}

	DynamicWidget *dynamicWidget = new DynamicWidget( widget, this, buttons );
	dynamicWidget->setAutoRaiseButtons( d->autoRaiseButtons );
	connect( dynamicWidget, SIGNAL(removeClicked()), this, SLOT(removeWidget()) );
	d->dynamicWidgets << dynamicWidget;

	if ( !d->addButton && dynamicWidget->addButton() ) {
		d->addButton = dynamicWidget->addButton();
		connect( d->addButton, SIGNAL(clicked()), this, SLOT(createAndAddWidget()) );
	}
	d->updateButtonStates();

	return dynamicWidget;
}

DynamicWidget *AbstractDynamicWidgetContainer::addWidget( QWidget* widget )
{
	Q_D( AbstractDynamicWidgetContainer );
	
	// Check if maximum widget count is reached
	if ( d->dynamicWidgets.count() == d->maxWidgetCount ) {
		kDebug() << "Can't add the given widget because the maximum "
				"widget count of" << d->maxWidgetCount << "is reached";
		return NULL;
	}

	// Add a separator if needed
	if ( d->showSeparators && !d->dynamicWidgets.isEmpty() ) {
		if ( d->newWidgetPosition == AddWidgetsAtTop ) {
			dynamic_cast<QVBoxLayout*>(d->contentWidget->layout())->insertWidget( 0, createSeparator() );
		} else {
			d->contentWidget->layout()->addWidget( createSeparator() );
		}
	}
	
	// Create and add the dynamic widget that wraps the widget and add/remove buttons
	DynamicWidget *dynWidget = createDynamicWidget( widget );
	if ( d->newWidgetPosition == AddWidgetsAtTop ) {
		dynamic_cast<QVBoxLayout*>(d->contentWidget->layout())->insertWidget( 0, dynWidget );
	} else {
		d->contentWidget->layout()->addWidget( dynWidget );
	}
	
	// Set focus to the newly added widget
	widget->setFocus();
	
	// Inform connected objects about the new widget
	emit added( widget );
	return dynWidget;
}

DynamicWidget* AbstractDynamicWidgetContainer::dynamicWidgetForWidget( QWidget* widget ) const
{
	Q_D( const AbstractDynamicWidgetContainer );
	int index = indexOf( widget );
	return index == -1 ? NULL : d->dynamicWidgets[ index ];
}

int AbstractDynamicWidgetContainer::indexOf( QWidget* widget ) const
{
	Q_D( const AbstractDynamicWidgetContainer );
	if ( !widget ) {
		return -1;
	}

	DynamicWidget *dynamicWidget = qobject_cast< DynamicWidget* >( widget );
	if ( dynamicWidget ) {
		return d->dynamicWidgets.indexOf( dynamicWidget );
	} else {
		for ( int i = 0; i < d->dynamicWidgets.count(); ++i ) {
			if ( d->dynamicWidgets[i]->contentWidget() == widget ) {
				return i;
			}
		}
	}
	return -1;
}

void AbstractDynamicWidgetContainer::removeLastWidget()
{
	Q_D( AbstractDynamicWidgetContainer );
	if ( d->newWidgetPosition == AddWidgetsAtTop ) {
		removeWidget( d->dynamicWidgets.first() );
	} else {
		removeWidget( d->dynamicWidgets.last() );
	}
}

void AbstractDynamicWidgetContainer::removeWidget()
{
	DynamicWidget *dynamicWidget = qobject_cast< DynamicWidget* >( sender() );
	if ( dynamicWidget ) {
		removeWidget( dynamicWidget );
	} else {
		kDebug() << "Sender isn't a DynamicWidget" << sender();
		// TODO: Remove last? Or add removeLastWidget();
	}
}

int AbstractDynamicWidgetContainer::removeWidget( QWidget *contentWidget )
{
	Q_D( AbstractDynamicWidgetContainer );
	if ( d->dynamicWidgets.count() == d->minWidgetCount ) {
		kDebug() << "Can't remove the given Widget because the minimum "
		"widget count of" << d->minWidgetCount << "is reached";
		return -1;
	}

	QVBoxLayout *vBoxLayout = dynamic_cast< QVBoxLayout* >( d->contentWidget->layout() );
	Q_ASSERT_X( vBoxLayout, "AbstractDynamicWidgetContainer::removeWidget",
	            "No QVBoxLayout found" );

	int widgetIndex = indexOf( contentWidget );
	DynamicWidget *dynamicWidget = d->dynamicWidgets[ widgetIndex ];
	Q_ASSERT_X( dynamicWidget, "AbstractDynamicWidgetContainer::removeWidget",
	            "Dynamic widget for the given widget not found" );

	int index = d->contentWidget->layout()->indexOf( dynamicWidget );
	if ( index > 0 ) {
		removeSeparator( d->contentWidget->layout()->itemAt( index - 1 ) );
	} else if ( d->dynamicWidgets.count() > 1 ) {
		removeSeparator( d->contentWidget->layout()->itemAt( index + 1 ) );
	}

	// When the first widget gets removed move it's add button to the next widget
	if ( index == 0 && dynamicWidget->addButton() ) {
		if ( d->dynamicWidgets.count() >= 2 ) {
			d->addButton = d->dynamicWidgets[ 1 ]->addButton(
			                   this, DynamicWidget::AddButton );
			connect( d->addButton, SIGNAL( clicked() ), this, SLOT( createAndAddWidget() ) );
			delete d->dynamicWidgets[ 1 ]->takeRemoveButton();
		} else {
			d->addButton = NULL;
		}
	}

	if ( !d->dynamicWidgets.removeOne(dynamicWidget) ) {
		kDebug() << "Widget to be removed not found in list" << dynamicWidget;
	}
	vBoxLayout->removeWidget( dynamicWidget );
	emit removed( dynamicWidget->contentWidget(), widgetIndex );
	delete dynamicWidget;

	d->updateButtonStates();
	return widgetIndex;
}

void AbstractDynamicWidgetContainer::removeAllWidgets()
{
	Q_D( AbstractDynamicWidgetContainer );
	foreach( DynamicWidget *dynamicWidget, d->dynamicWidgets ) {
		removeWidget( dynamicWidget );
	}
}

QWidget* AbstractDynamicWidgetContainer::createSeparator( const QString& separatorText )
{
	if ( separatorText.isEmpty() ) {
		QFrame *separator = new QFrame( this );
		separator->setObjectName( "separator" );
		separator->setFrameShape( QFrame::HLine );
		separator->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
		return separator;
	} else {
		QWidget *separator = new QWidget( this );
		separator->setObjectName( "separator" );
		QFrame *separatorL = new QFrame( separator );
		QFrame *separatorR = new QFrame( separator );
		separatorL->setFrameShape( QFrame::HLine );
		separatorR->setFrameShape( QFrame::HLine );
		separatorL->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
		separatorR->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );

		QLabel *separatorLabel = new QLabel( separatorText, separator );
		separatorLabel->setForegroundRole( QPalette::Mid );
		separatorLabel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Maximum );

		QHBoxLayout *separatorLayout = new QHBoxLayout( separator );
		separatorLayout->setContentsMargins( 0, 0, 0, 0 );
		separatorLayout->addWidget( separatorL );
		separatorLayout->addWidget( separatorLabel );
		separatorLayout->addWidget( separatorR );
		separatorLayout->setAlignment( separatorL, Qt::AlignVCenter );
		separatorLayout->setAlignment( separatorLabel, Qt::AlignCenter );
		separatorLayout->setAlignment( separatorR, Qt::AlignVCenter );

		return separator;
	}
}

void AbstractDynamicWidgetContainer::removeSeparator( QLayoutItem *separator )
{
	Q_D( AbstractDynamicWidgetContainer );
	if ( separator && !qobject_cast<DynamicWidget*>(separator->widget()) ) {
		QWidget *widget = separator->widget();
		if ( widget && widget->objectName() == "separator" ) {
			d->contentWidget->layout()->removeWidget( widget );
			delete widget;
		} else {
			kDebug() << "Couldn't remove separator" << separator;
		}
	}
}

QList< DynamicWidget* > AbstractDynamicWidgetContainer::dynamicWidgets() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->dynamicWidgets;
}

QString AbstractDynamicWidgetContainer::addButtonIcon() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->addButtonIcon;
}

QString AbstractDynamicWidgetContainer::removeButtonIcon() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->removeButtonIcon;
}

bool AbstractDynamicWidgetContainer::autoRaiseButtons() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->autoRaiseButtons;
}

Qt::Alignment AbstractDynamicWidgetContainer::buttonAlignment() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->buttonAlignment;
}

int AbstractDynamicWidgetContainer::buttonSpacing() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->buttonSpacing;
}

QToolButton* AbstractDynamicWidgetContainer::addButton() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	if ( d->addButton ) {
		return d->addButton;
	} else if ( d->showAddButton && !d->dynamicWidgets.isEmpty() ) {
		return d->dynamicWidgets.first()->addButton();
	} else {
		return NULL;
	}
}

QToolButton* AbstractDynamicWidgetContainer::removeButton() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->removeButton;
}

int AbstractDynamicWidgetContainer::minimumWidgetCount() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->minWidgetCount;
}

int AbstractDynamicWidgetContainer::maximumWidgetCount() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->maxWidgetCount;
}

int AbstractDynamicWidgetContainer::setWidgetCountRange(
    int minWidgetCount, int maxWidgetCount, bool putIntoRange )
{
	Q_D( AbstractDynamicWidgetContainer );
	d->minWidgetCount = minWidgetCount;
	d->maxWidgetCount = maxWidgetCount;

	int added = 0;
	if ( putIntoRange ) {
		while ( d->dynamicWidgets.count() < minWidgetCount ) {
			createAndAddWidget();
			++added;
		}
		if ( maxWidgetCount != -1 ) {
			while ( d->dynamicWidgets.count() > maxWidgetCount ) {
				removeLastWidget();
				--added;
			}
		}
	}

	d->updateButtonStates();
	return added;
}

int AbstractDynamicWidgetContainer::widgetCount() const
{
	Q_D( const AbstractDynamicWidgetContainer );
	return d->dynamicWidgets.count();
}


class AbstractDynamicLabeledWidgetContainerPrivate : public AbstractDynamicWidgetContainerPrivate
{
	Q_DECLARE_PUBLIC( AbstractDynamicLabeledWidgetContainer )

public:
	AbstractDynamicLabeledWidgetContainerPrivate( AbstractDynamicLabeledWidgetContainer *q )
			: AbstractDynamicWidgetContainerPrivate( q ) {
		widgetNumberOffset = 1;
	};

	virtual QLayout* createContentLayout( QWidget *parent ) {
		QFormLayout *formLayout = new QFormLayout( parent );
		formLayout->setRowWrapPolicy( QFormLayout::WrapLongRows );
		formLayout->setVerticalSpacing( 2 );
		formLayout->setContentsMargins( 0, 0, 0, 0 );
		return formLayout;
	};

	QString labelTextFor( int widgetIndex ) {
		if ( widgetIndex >= specialLabelTexts.count() ) {
			return QString( labelText ).arg( widgetIndex + widgetNumberOffset );
		} else {
			return specialLabelTexts[ widgetIndex ];
		}
	};

	QWidgetList labelWidgets;
	QString labelText;
	QStringList specialLabelTexts;
	int widgetNumberOffset;
};

AbstractDynamicLabeledWidgetContainer::AbstractDynamicLabeledWidgetContainer( QWidget* parent,
	RemoveButtonOptions removeButtonOptions, AddButtonOptions addButtonOptions,
	SeparatorOptions separatorOptions, NewWidgetPosition newWidgetPosition, const QString &labelText  )
		: AbstractDynamicWidgetContainer( parent,
			*new AbstractDynamicLabeledWidgetContainerPrivate(this),
			removeButtonOptions, addButtonOptions, newWidgetPosition )
{
	Q_D( AbstractDynamicLabeledWidgetContainer );
	d->init( removeButtonOptions, addButtonOptions, separatorOptions, newWidgetPosition );
	d->labelText = labelText;
}

AbstractDynamicLabeledWidgetContainer::AbstractDynamicLabeledWidgetContainer( QWidget* parent,
	AbstractDynamicLabeledWidgetContainerPrivate& dd,
	RemoveButtonOptions removeButtonOptions, AddButtonOptions addButtonOptions,
	NewWidgetPosition newWidgetPosition,
	const QString &labelText )
		: AbstractDynamicWidgetContainer( parent, dd, removeButtonOptions, addButtonOptions, 
										  newWidgetPosition )
{
	Q_D( AbstractDynamicLabeledWidgetContainer );
	d->labelText = labelText;
}

QString AbstractDynamicLabeledWidgetContainer::labelText() const
{
	Q_D( const AbstractDynamicLabeledWidgetContainer );
	return d->labelText;
}

void AbstractDynamicLabeledWidgetContainer::setLabelTexts( const QString& labelText,
	const QStringList& specialLabelTexts, LabelNumberOptions labelNumberOptions )
{
	Q_D( AbstractDynamicLabeledWidgetContainer );
	d->labelText = labelText;
	d->specialLabelTexts = specialLabelTexts;
	switch ( labelNumberOptions ) {
	case DontIncludeSpecialLabelsInWidgetNumbering:
		d->widgetNumberOffset = 1 - specialLabelTexts.count();
		break;
	case IncludeSpecialLabelsInWidgetNumbering:
		d->widgetNumberOffset = 1;
		break;
	}

	// Update label texts of already existing labels
	for ( int i = 0; i < d->labelWidgets.count(); ++i ) {
		updateLabelWidget( d->labelWidgets[i], i );
	}
}

QStringList AbstractDynamicLabeledWidgetContainer::specialLabelTexts() const
{
	Q_D( const AbstractDynamicLabeledWidgetContainer );
	return d->specialLabelTexts;
}

DynamicWidget* AbstractDynamicLabeledWidgetContainer::addWidget( QWidget* widget )
{
	Q_D( AbstractDynamicLabeledWidgetContainer );
	return addWidget( createNewLabelWidget( d->dynamicWidgets.count() ), widget );
}

DynamicWidget *AbstractDynamicLabeledWidgetContainer::addWidget(
    QWidget* labelWidget, QWidget* widget )
{
	Q_D( AbstractDynamicLabeledWidgetContainer );
	if ( d->dynamicWidgets.count() == d->maxWidgetCount ) {
		kDebug() << "Can't add the given widget because the maximum "
		"widget count of" << d->maxWidgetCount << "is reached";
		return NULL;
	}
	if ( d->showSeparators && !d->dynamicWidgets.isEmpty() ) {
		QFormLayout *l = dynamic_cast<QFormLayout*>( d->contentWidget->layout() );
		if ( l ) {
			l->addRow( createSeparator() );
		} else {
			d->contentWidget->layout()->addWidget( createSeparator() );
		}
	}

	d->labelWidgets << labelWidget;
	DynamicWidget *dynWidget = createDynamicWidget( widget );
	QFormLayout *formLayout = dynamic_cast< QFormLayout* >( d->contentWidget->layout() );
	Q_ASSERT_X( formLayout, "AbstractDynamicLabeledWidgetContainer::addWidget",
	            "No form layout found" );
	formLayout->addRow( labelWidget, dynWidget );
	emit added( widget );
	widget->setFocus();
	return dynWidget;
}

int AbstractDynamicLabeledWidgetContainer::removeWidget( QWidget *widget )
{
	Q_D( AbstractDynamicLabeledWidgetContainer );
	if ( d->dynamicWidgets.count() == d->minWidgetCount ) {
		kDebug() << "Can't remove the given widget because the minimum "
		"widget count of" << d->minWidgetCount << "is reached";
		return -1;
	}

	DynamicWidget *dynamicWidget = dynamicWidgetForWidget( widget );
	Q_ASSERT_X( dynamicWidget, "AbstractDynamicLabeledWidgetContainer::removeWidget",
	            "Dynamic widget for the given widget not found" );
	int index = d->dynamicWidgets.indexOf( dynamicWidget );

	QFormLayout *formLayout = dynamic_cast< QFormLayout* >( d->contentWidget->layout() );
	Q_ASSERT_X( formLayout, "AbstractDynamicLabeledWidgetContainer::removeWidget",
	            "No form layout found" );
	int row;
	QFormLayout::ItemRole role;
	formLayout->getWidgetPosition( dynamicWidget, &row, &role );
	if ( row > 0 ) {
		removeSeparator( formLayout->itemAt( row - 1, QFormLayout::SpanningRole ) );
	} else if ( d->dynamicWidgets.count() > 1 ) {
		removeSeparator( formLayout->itemAt( row + 1, QFormLayout::SpanningRole ) );
	}

	// When the first widget gets removed move it's add button to the next widget
	if ( index == 0 && dynamicWidget->addButton() ) {
		if ( d->dynamicWidgets.count() >= 2 ) {
			d->addButton = d->dynamicWidgets[ 1 ]->addButton(
			                   this, DynamicWidget::AddButton );
			connect( d->addButton, SIGNAL( clicked() ), this, SLOT( createAndAddWidget() ) );
			delete d->dynamicWidgets[ 1 ]->takeRemoveButton();
		} else {
			d->addButton = NULL;
		}
	}

	QWidget *label = d->labelWidgets[ index ];
	formLayout->removeWidget( label );
	formLayout->removeWidget( dynamicWidget );
	emit removed( dynamicWidget->contentWidget(), index );
	d->labelWidgets.removeAt( index );
	d->dynamicWidgets.removeAt( index );
	delete label;
	delete dynamicWidget;

	d->updateButtonStates();

	// Update labels after the removed one
	for ( int i = index; i < d->dynamicWidgets.count(); ++i ) {
		updateLabelWidget( d->labelWidgets[i], i );
	}
	return index;
}

QWidget* AbstractDynamicLabeledWidgetContainer::createNewLabelWidget( int widgetIndex )
{
	Q_D( AbstractDynamicLabeledWidgetContainer );
	return new QLabel( d->labelTextFor( widgetIndex ), this );
}

void AbstractDynamicLabeledWidgetContainer::updateLabelWidget( QWidget* labelWidget, int widgetIndex )
{
	Q_D( AbstractDynamicLabeledWidgetContainer );
	QLabel *label = qobject_cast< QLabel* >( labelWidget );
	if ( label ) {
		label->setText( d->labelTextFor( widgetIndex ) );
	} else {
		kDebug() << "If you override createNewLabelWidget() you should also override "
				"updateLabelWidget() to not use the default implementation that "
				"tries to update the text of a QLabel.";
	}
}

QWidget* AbstractDynamicLabeledWidgetContainer::labelWidgetFor( QWidget* widget ) const
{
	Q_D( const AbstractDynamicLabeledWidgetContainer );
	int index = indexOf( widget );
	return d->labelWidgets[ index ];
}


class DynamicLabeledLineEditListPrivate : public AbstractDynamicLabeledWidgetContainerPrivate
{
	Q_DECLARE_PUBLIC( DynamicLabeledLineEditList )

public:
	DynamicLabeledLineEditListPrivate( DynamicLabeledLineEditList *q )
			: AbstractDynamicLabeledWidgetContainerPrivate( q ) {
		clearButtonsShown = true;
	};

	KLineEdit *createLineEdit() {
		Q_Q( DynamicLabeledLineEditList );
		KLineEdit *lineEdit = new KLineEdit( q );
		lineEdit->setClearButtonShown( clearButtonsShown );
		indexMapping.insert( lineEdit, dynamicWidgets.count() );
		q->connect( lineEdit, SIGNAL(textEdited(QString)), q, SLOT(textEdited(QString)) );
		q->connect( lineEdit, SIGNAL(textChanged(QString)), q, SLOT(textChanged(QString)) );
		return lineEdit;
	};

	bool clearButtonsShown;
	QHash< QWidget*, int > indexMapping;
};

DynamicLabeledLineEditList::DynamicLabeledLineEditList( QWidget* parent,
	AbstractDynamicWidgetContainer::RemoveButtonOptions removeButtonOptions,
	AbstractDynamicWidgetContainer::AddButtonOptions addButtonOptions,
	AbstractDynamicWidgetContainer::SeparatorOptions separatorOptions,
	AbstractDynamicWidgetContainer::NewWidgetPosition newWidgetPosition,
	const QString& labelText )
		: AbstractDynamicLabeledWidgetContainer( parent, *new DynamicLabeledLineEditListPrivate(this),
				removeButtonOptions, addButtonOptions, newWidgetPosition, labelText )
{
	Q_D( DynamicLabeledLineEditList );
	d->init( removeButtonOptions, addButtonOptions, separatorOptions, newWidgetPosition );
}

QWidget* DynamicLabeledLineEditList::createNewWidget()
{
	Q_D( DynamicLabeledLineEditList );
	return d->createLineEdit();
}

KLineEdit* DynamicLabeledLineEditList::addLineEdit()
{
	Q_D( DynamicLabeledLineEditList );
	KLineEdit *lineEdit = d->createLineEdit();
	addWidget( lineEdit );
	return lineEdit;
}

QList< KLineEdit* > DynamicLabeledLineEditList::lineEditWidgets() const
{
	return widgets< KLineEdit* >();
}

QLabel *DynamicLabeledLineEditList::labelFor( KLineEdit *lineEdit ) const
{
	return qobject_cast< QLabel* >( labelWidgetFor(lineEdit) );
}

KLineEdit* DynamicLabeledLineEditList::focusedLineEdit() const
{
	return focusedWidget< KLineEdit* >();
}

QStringList DynamicLabeledLineEditList::lineEditTexts() const
{
	QStringList texts;
	QList< KLineEdit* > lineEdits = widgets< KLineEdit* >();
	foreach( KLineEdit* lineEdit, lineEdits ) {
		texts << lineEdit->text();
	}

	return texts;
}

void DynamicLabeledLineEditList::setLineEditTexts( const QStringList& lineEditTexts )
{
	Q_D( DynamicLabeledLineEditList );
	while ( d->dynamicWidgets.count() < lineEditTexts.count()
	        && d->dynamicWidgets.count() != d->maxWidgetCount ) {
		createAndAddWidget();
	}
	while ( d->dynamicWidgets.count() > lineEditTexts.count()
	        && d->dynamicWidgets.count() != d->minWidgetCount ) {
		removeLastWidget();
	}

	QList< KLineEdit* > lineEdits = widgets< KLineEdit* >();
	for ( int i = 0; i < qMin( lineEdits.count(), lineEditTexts.count() ); ++i ) {
		KLineEdit *lineEdit = lineEdits[ i ];
		lineEdit->setText( lineEditTexts[i] );
	}
}

int DynamicLabeledLineEditList::removeLineEditsByText( const QString& text,
        Qt::CaseSensitivity caseSensitivity )
{
	int removed = 0;
	QList< KLineEdit* > lineEdits = lineEditWidgets();
	foreach( KLineEdit *lineEdit, lineEdits ) {
		if ( lineEdit->text().compare( text, caseSensitivity ) == 0 ) {
			if ( removeWidget( lineEdit ) != -1 ) {
				++removed;
			}
		}
	}

	return removed;
}

int DynamicLabeledLineEditList::removeWidget( QWidget* widget )
{
	Q_D( DynamicLabeledLineEditList );
	int index = AbstractDynamicLabeledWidgetContainer::removeWidget( widget );
	if ( index == -1 ) {
		return -1;
	}

	for ( int i = index; i < d->dynamicWidgets.count(); ++i ) {
		d->indexMapping[ d->dynamicWidgets[i]->contentWidget()] = i;
	}
	return index;
}

void DynamicLabeledLineEditList::textEdited( const QString& text )
{
	Q_D( const DynamicLabeledLineEditList );
	KLineEdit *lineEdit = qobject_cast< KLineEdit* >( sender() );
	Q_ASSERT_X( lineEdit, "DynamicLabeledLineEditList::textEdited",
				"sender() isn't a KLineEdit" );
	emit textEdited( text, d->indexMapping[lineEdit] );
}

void DynamicLabeledLineEditList::textChanged( const QString& text )
{
	Q_D( const DynamicLabeledLineEditList );
	KLineEdit *lineEdit = qobject_cast< KLineEdit* >( sender() );
	Q_ASSERT_X( lineEdit, "DynamicLabeledLineEditList::textChanged",
				"sender() isn't a KLineEdit" );
	emit textChanged( text, d->indexMapping[lineEdit] );
}

bool DynamicLabeledLineEditList::clearButtonsShown() const
{
	Q_D( const DynamicLabeledLineEditList );
	return d->clearButtonsShown;
}

void DynamicLabeledLineEditList::setClearButtonsShown( bool clearButtonsShown )
{
	Q_D( DynamicLabeledLineEditList );
	d->clearButtonsShown = clearButtonsShown;

	QList< KLineEdit* > lineEdits = widgets< KLineEdit* >();
//     foreach ( DynamicWidget *dynamicWidget, d->dynamicWidgets ) {
//     KLineEdit *lineEdit = qobject_cast< KLineEdit* >( dynamicWidget->contentWidget() );
	foreach( KLineEdit* lineEdit, lineEdits )
	lineEdit->setClearButtonShown( clearButtonsShown );
}
