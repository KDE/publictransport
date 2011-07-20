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
 * @brief Contains a combobox in which each element can be checked.
 * 
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef CHECKCOMBOBOX_HEADER
#define CHECKCOMBOBOX_HEADER

#include "publictransporthelper_export.h"

#include <KComboBox>
#include <KLocalizedString>

class CheckComboboxPrivate;

/**
 * @brief A combobox to select multiple items of the list by adding check boxes.
 * 
 * If no or one item is selected KComboBox paints the combobox in the default way.
 * If more than one item is selected, the icons of all selected items are painted
 * and the text shows how many items are selected ("x/y").
 **/
class PUBLICTRANSPORTHELPER_EXPORT CheckCombobox : public KComboBox {
	Q_OBJECT
	Q_PROPERTY( int allowNoCheckedItem READ allowNoCheckedItem WRITE setAllowNoCheckedItem )
	Q_PROPERTY( MultipleSelectionOptions multipleSelectionOptions READ multipleSelectionOptions WRITE setMultipleSelectionOptions )
	Q_PROPERTY( QString separator READ separator WRITE setSeparator )
	Q_PROPERTY( QString noSelectionText READ noSelectionText WRITE setNoSelectionText )
	Q_PROPERTY( QModelIndexList checkedItems READ checkedItems WRITE setCheckedItems )

public:
    /** @brief Options for how to handle/visualize multiple selected items. */
	enum MultipleSelectionOptions {
		ShowStringList, /**< Show the texts of selected items. */
		ShowIconList /**< Show all icons of selected items. */
	};

	/** @brief Creates a new CheckCombobox. */
	CheckCombobox( QWidget* parent = 0 );

	/** @brief Destructor. */
	~CheckCombobox();

    /** @brief Gets the current options for how to handle/visualize multiple selected items. */
    MultipleSelectionOptions multipleSelectionOptions() const;

    /** @brief Set the options for how to handle/visualize multiple selected items. */
	void setMultipleSelectionOptions( MultipleSelectionOptions multipleSelectionOptions );

	/**
     * @brief Gets the separating text between checked item texts.
     * 
     * Only used with @ref ShowStringList.
     **/
	QString separator() const;

    /**
     * @brief Sets the separating text between checked item texts.
     *
     * Only used with @ref ShowStringList.
     **/
	void setSeparator( const QString &separator = ", " );

	/** @brief Gets the text that is shown if no item is checked. */
	QString noSelectionText() const;

	/** @brief Sets the text that is shown if no item is checked to @p noSelectionText. */
	void setNoSelectionText( const QString &noSelectionText = i18nc(
		"@info/plain Default text of a CheckCombobox if no item is checked", "(none)") );

	/** @brief Gets the text that is shown if all items are checked. */
	QString allSelectedText() const;

	/** @brief Sets the text that is shown if all items are checked to @p allSelectedText. */
	void setAllSelectedText( const QString &allSelectedText = i18nc(
		"@info/plain Default text of a CheckCombobox if all items are checked", "(all)") );

	/** @brief Adds an item with the given @p text. */
	void addItem( const QString &text );

	/** @brief Adds items with the given @p texts. */
	void addItems( const QStringList &texts );

	/**
     * @brief Gets whether or not it's allowed that no item is checked.
     * 
	 * If this is false, the last checked item can't be unchecked.
     **/
	bool allowNoCheckedItem() const;

	/**
     * @brief Sets whether or not it's allowed that no item is checked.
     * 
	 * If set to false, the last checked item can't be unchecked (true is default)
     **/
	void setAllowNoCheckedItem( bool allow = true );

	/** @brief Returns a list of indices of the model that are currently checked. */
	QModelIndexList checkedItems() const;

	/** @brief Returns a list of rows of the model that are currently checked. */
	QList<int> checkedRows() const;

    /** @brief Returns a list of texts of the model that are currently checked. */
    QStringList checkedTexts() const;

	/** @brief Sets all items for the given @p indices checked. All other items get unchecked. */
	void setCheckedItems( const QModelIndexList &indices );

	/** @brief Sets all items at the given @p rows checked. All other items get unchecked. */
	void setCheckedRows( const QList<int> &rows );

    /** @brief Sets all items with the given @p texts checked. All other items get unchecked. */
    void setCheckedTexts( const QStringList &texts );

	/** @brief Sets the check state of the given @p index to @p checkState. */
	void setItemCheckState( const QModelIndex &index, Qt::CheckState checkState );

	/** @brief Checks if the model has at least @p count checked items. */
	bool hasCheckedItems( int count = 1 ) const;

signals:
	/** @brief Emitted when an item's check state changes. */
	void checkedItemsChanged();

protected:
	CheckComboboxPrivate* const d_ptr;

	/** @brief Reimplemented to change the check state of the current item when space is pressed. */
	virtual void keyPressEvent( QKeyEvent *event );

	/** @brief Reimplemented to not close the drop down list if an item is clicked but instead
     * toggle it's check state. */
	virtual bool eventFilter( QObject *object, QEvent *event );

	/** @brief Reimplemented to paint multiple checked items. */
	virtual void paintEvent( QPaintEvent* );

	/** @brief Reimplemented to give enough space for multiple selected item's icons. */
	virtual QSize sizeHint() const;

private:
	Q_DECLARE_PRIVATE( CheckCombobox )
};

#endif // Multiple inclusion guard
