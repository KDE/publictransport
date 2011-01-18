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

/** @file
 * @brief This file contains widgets to edit filters for departures/arrivals/journeys.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef FILTERWIDGET_HEADER
#define FILTERWIDGET_HEADER

#include <QWidget>
#include <QCheckBox>
#include <QToolButton>
#include <QListView>
#include <QTimeEdit>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QApplication>
#include <QStandardItemModel>

#include <KLineEdit>
#include <KNumInput>
#include <KDebug>

#include "global.h"
#include "filter.h"
#include <publictransporthelper/dynamicwidget.h>

class CheckCombobox;
class KComboBox;
/**
 * @brief Base class for widgets allowing to edit a single constraint.
 * @ingroup filterSystem
 **/
class ConstraintWidget : public QWidget {
	Q_OBJECT

public:
	/**
	 * @brief Creates a new constraint widget to edit a single constraint.
	 * @note Use one of the derived classes to create a constraint widget. This class can be
	 * used to create new types of constraint widgets by deriving from it.
	 *
	 * @param type The type of this constraint, ie. what to filter.
	 * @param availableVariants A list of available variants for this constraint.
	 * Defaults to QList<FilterVariant>().
	 * @param initialVariant The initially set variant for this constraint.
	 * Defaults to FilterNoVariant.
	 * @param parent The parent object. Defaults to 0.
	 **/
	explicit ConstraintWidget( FilterType type,
			QList<FilterVariant> availableVariants = QList<FilterVariant>(),
			FilterVariant initialVariant = FilterNoVariant, QWidget* parent = 0 );

	/** @return The type of this constraint. */
	FilterType type() const { return m_constraint.type; };
	/** @return The variant of this constraint, like contains / equals, etc. */
	FilterVariant variant() const { return m_constraint.variant; };
	/** @return The value of this constraint. */
	virtual QVariant value() const = 0;
	/** Set the value of this constraint to @p value.
	  * @param value The new value for this constraint. */
	virtual void setValue( const QVariant &value ) = 0;
	/** @return The Constraint-object for this widget. */
	Constraint constraint() {
		m_constraint.value = value();
		return m_constraint;
	};

	void addWidget( QWidget *w ) {
		QFormLayout *l = dynamic_cast< QFormLayout* >( layout() );
		QLayoutItem *item = l->itemAt( 0 );
		if ( item ) {
			l->removeItem( item );
			l->addRow( item->widget(), w );
		}
	};

	inline static ConstraintWidget *create( Constraint constraint, QWidget *parent = 0 ) {
	    return create( constraint.type, constraint.variant, constraint.value, parent );
	};
	static ConstraintWidget *create( FilterType type, FilterVariant variant = FilterNoVariant,
									 const QVariant &value = QVariant(), QWidget *parent = 0 );

signals:
	/** Emitted when the value of this constraint has changed. */
	void changed();

protected slots:
	virtual void variantChanged( int index );

private:
	QString filterVariantName( FilterVariant filterVariant ) const;

	Constraint m_constraint;
	KComboBox *m_variantsCmb;
};

/**
 * @brief A widget allowing to edit a single constraint where the user can select a list of values.
 * @ingroup filterSystem
 **/
class ConstraintListWidget : public ConstraintWidget {
	Q_OBJECT

public:
	/**
	 * @brief Helper structure to hold information about one selectable value in the list.
	 **/
	struct ListItem {
		QString text; /**< The text of this item. */
		QVariant value; /**< The value represented by this item. */
		KIcon icon; /**< The icon to show for this item. */

		ListItem() {};
		ListItem( const QString &text, const QVariant &value, const KIcon &icon ) {
			this->text = text;
			this->value = value;
			this->icon = icon;
		};
	};

	/**
	 * @brief Creates a new constraint widget where the user can select a list of values.
	 *
	 * @param type The type of this constraint, ie. what to filter.
	 * @param initialVariant The initial variant of this constraint, eg. equals/doesn't equals.
	 * @param values A list of selectable values for this constraint.
	 * @param initialValues The initially selected values for this constraint.
	 * @param parent The parent object. Defaults to 0.
	 **/
	ConstraintListWidget( FilterType type, FilterVariant initialVariant,
						  const QList< ListItem > &values, const QVariantList& initialValues,
						  QWidget* parent = 0 );

	/** @returns the @ref CheckCombobox which is used by this ConstraintListWidget. */
	CheckCombobox *list() const { return m_list; };

	/** @returns a @ref QVariantList with the currently selected values. */
	virtual QVariant value() const { return m_values; };
	/**
	 * @brief Sets the list of selected values.
	 * @param value A @ref QVariantList with the values to be selected.
	 **/
	virtual void setValue( const QVariant &value );

	QModelIndex indexFromValue( const QVariant &value );

protected slots:
	void checkedItemsChanged();

private:
	CheckCombobox *m_list;
	QVariantList m_values;
};

/**
 * @brief A widget allowing to edit a single constraint where the user can enter a string value.
 * @ingroup filterSystem
 **/
class ConstraintStringWidget : public ConstraintWidget {
	Q_OBJECT

public:
	/**
	 * @brief Creates a new constraint widget where the user can enter a string value.
	 *
	 * @param type The type of this constraint, ie. what to filter.
	 * @param initialVariant The initial variant of this constraint, eg. equals/doesn't equals.
	 * @param filterText The value of this constraint. Defaults to QString().
	 * @param parent The parent object. Defaults to 0.
	 **/
	ConstraintStringWidget( FilterType type, FilterVariant initialVariant,
							const QString &filterText = QString(), QWidget* parent = 0 );

	/** @returns a @ref QString with the current value of this constraint. */
	virtual QVariant value() const { return m_string->text(); };
	/**
	 * @brief Sets the string value of this constraint.
	 * @param value A @ref QString with the string to be set for this constraint.
	 **/
	virtual void setValue( const QVariant &value ) {
		m_string->setText(value.toString());
	};

protected slots:
	void stringChanged( const QString &newString ) {
		Q_UNUSED( newString);
		emit changed();
	};

private:
	KLineEdit *m_string;
};

/**
 * @brief A widget allowing to edit a single constraint where the user can enter an integer value.
 * @ingroup filterSystem
 **/
class ConstraintIntWidget : public ConstraintWidget {
	Q_OBJECT

public:
	/**
	 * @brief Creates a new constraint widget where the user can enter an integer value.
	 *
	 * @param type The type of this constraint, ie. what to filter.
	 * @param initialVariant The initial variant of this constraint, eg. equals/doesn't equals.
	 * @param value The value of this constraint. Defaults to 0.
	 * @param min Minimal allowed value for this constraint. Defaults to 0.
	 * @param max Maximal allowed value for this constraint. Defaults to 10000.
	 * @param parent The parent object. Defaults to 0.
	 **/
	ConstraintIntWidget( FilterType type, FilterVariant initialVariant,
						 int value = 0, int min = 0, int max = 10000, QWidget* parent = 0 );

	/** @returns an integer with the current value of this constraint. */
	virtual QVariant value() const { return m_num->value(); };
	/**
	 * @brief Sets the integer value of this constraint.
	 * @param value An integer with the value to be set for this constraint.
	 **/
	virtual void setValue( const QVariant &value ) { m_num->setValue(value.toInt()); };

protected slots:
	void intChanged( int newInt ) {
		Q_UNUSED( newInt );
		emit changed();
	};

private:
	KIntSpinBox *m_num;
};

/**
 * @brief A widget allowing to edit a single constraint where the user can enter a time value.
 * @ingroup filterSystem
 **/
class ConstraintTimeWidget : public ConstraintWidget {
	Q_OBJECT

public:
	/**
	 * @brief Creates a new constraint widget where the user can enter a time value.
	 *
	 * @param type The type of this constraint, ie. what to filter.
	 * @param initialVariant The initial variant of this constraint, eg. equals/doesn't equals.
	 * @param value The value of this constraint. Defaults to QTime::currentTime().
	 * @param parent The parent object. Defaults to 0.
	 **/
	ConstraintTimeWidget( FilterType type, FilterVariant initialVariant,
						  QTime value = QTime::currentTime(), QWidget* parent = 0 );

	/** @returns a @ref QTime with the current time value of this constraint. */
	virtual QVariant value() const { return m_time->time(); };
	/**
	 * @brief Sets the time value of this constraint.
	 * @param value A @ref QTime with the time value to be set for this constraint.
	 **/
	virtual void setValue( const QVariant &value ) { m_time->setTime(value.toTime()); };

protected slots:
	void timeChanged( const QTime &newTime ) {
		Q_UNUSED( newTime );
		emit changed();
	};

private:
	QTimeEdit *m_time;
};

/**
 * @brief A widget allowing to edit a filter, which is a list of constraints.
 * Constraints can be dynamically added / removed, buttons are added for that task.
 * @ingroup filterSystem
 **/
class FilterWidget : public AbstractDynamicLabeledWidgetContainer {
	Q_OBJECT
	Q_PROPERTY( QString separatorText READ separatorText WRITE setSeparatorText )

public:
	FilterWidget( QWidget *parent,
				  AbstractDynamicWidgetContainer::SeparatorOptions seperatorOptions
				  = AbstractDynamicLabeledWidgetContainer::NoSeparator );
	explicit FilterWidget( const QList<FilterType> &allowedFilterTypes = QList<FilterType>(),
						   QWidget* parent = 0,
						   AbstractDynamicWidgetContainer::SeparatorOptions seperatorOptions
						   = AbstractDynamicLabeledWidgetContainer::NoSeparator );

	/** Returns a list of all contained constraint widgets. */
	QList< ConstraintWidget* > constraintWidgets() const {
		QList< ConstraintWidget* > list;
		foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
			list << qobject_cast< ConstraintWidget* >( dynamicWidget->contentWidget() );
		}
		return list;
	};

	/** Set a list of @ref FilterType which are allowed to be added to this FilterWidget. */
	void setAllowedFilterTypes( const QList<FilterType> &allowedFilterTypes );

	/** Set the text to be shown between constraints. It's only used for new separators. */
	void setSeparatorText( const QString &separatorText ) {
		m_separatorText = separatorText;
	};
	/** @return The text that is shown between constraints of this FilterWidget. */
	QString separatorText() const { return m_separatorText; };

	/** Returns a Filter object with all constraints of this FilterWidget. */
	Filter filter() const {
		Filter f;
		foreach ( ConstraintWidget *constraint, constraintWidgets() ) {
			f << constraint->constraint();
		}
		return f;
	};
	/** Sets all constraints in @p filter to this FilterWidget. */
	void setFilter( const Filter &filter );

	inline void addConstraint( FilterType filterType ) {
		addConstraint( createConstraint(filterType) );
	};
	void addConstraint( const Constraint &constraint ) {
		addConstraint( ConstraintWidget::create(constraint, this) );
	};
	void addConstraint( ConstraintWidget *constraint );
	virtual int removeWidget( QWidget* widget );

	static FilterWidget *create( const Filter &filter, QWidget *parent = 0 ) {
		FilterWidget *filterWidget = new FilterWidget( QList<FilterType>()
				<< FilterByVehicleType << FilterByTarget << FilterByVia << FilterByTransportLine
				<< FilterByTransportLineNumber << FilterByDelay, parent );
		filterWidget->setFilter( filter );
		return filterWidget;
	};

signals:
	/** Emitted, when this FilterWidget has changed, ie. a constraint value has changed
	  * or a constraint was added or removed. */
	void changed();
	/** Emitted, after the new constraint @p constraintWidget was added. */
	void constraintAdded( ConstraintWidget *constraintWidget );
	/** Emitted, after @p constraint was removed. */
	void constraintRemoved( const Constraint &constraint );

public slots:
	inline void addConstraint() {
		addConstraint( qobject_cast<ConstraintWidget*>(createNewWidget()) );
	};
	/** Removes the given @p widget from the list of constraints. */
	void removeConstraint( ConstraintWidget *widget );

protected slots:
	void filterTypeChanged( int index );

protected:
	virtual QWidget *createNewWidget() {
		return createConstraint( firstUnusedFilterType() );
	};
	virtual QWidget* createNewLabelWidget( int );
	virtual QWidget* createSeparator( const QString& separatorText = QString() );

	virtual DynamicWidget *addWidget( QWidget *widget ) {
		return AbstractDynamicLabeledWidgetContainer::addWidget( widget );
	};
	virtual DynamicWidget *addWidget( QWidget *labelWidget, QWidget *widget );

	virtual void updateLabelWidget( QWidget*, int ) {};

private:
	ConstraintWidget *createConstraint( FilterType type );
	QString filterName( FilterType filter ) const;
	FilterType firstUnusedFilterType() const;

	QList< KComboBox* > m_filterTypes;
	QList< FilterType > m_allowedFilterTypes;
	QString m_separatorText;
};

/**
 * @brief A widget allowing to edit a list of filters, which are lists of constraints.
 * Filters can be dynamically added / removed, buttons are added for that task.
 * @ingroup filterSystem
 **/
class FilterListWidget : public AbstractDynamicWidgetContainer {
	Q_OBJECT

public:
	FilterListWidget( QWidget* parent = 0 );

	QList< FilterWidget* > filterWidgets() const;
	FilterList filters() const;

	void addFilter() {
		Filter filter;
		filter << Constraint();
		addFilter( filter );
	};

	inline void addFilter( const Filter &filter ) {
		addFilter( FilterWidget::create(filter, this) );
	};

	void addFilter( FilterWidget *filterWidget ) {
		connect( filterWidget, SIGNAL(changed()), this, SIGNAL(changed()) );
		addWidget( filterWidget );
	};

	static FilterListWidget *create( const FilterList &filterList, QWidget *parent = 0 ) {
		FilterListWidget *filterListWidget = new FilterListWidget( parent );
		foreach ( Filter filter, filterList ) {
			filterListWidget->addFilter( filter );
		}
		return filterListWidget;
	};

signals:
	/** Emitted when the value of a constraint of a filter changes. */
	void changed();

protected:
	virtual QWidget *createNewWidget() {
		Filter filter;
		filter << Constraint();
		return FilterWidget::create( filter, this );
	};
	virtual DynamicWidget* createDynamicWidget( QWidget* widget );

	virtual DynamicWidget *addWidget( QWidget* widget );
	virtual int removeWidget( QWidget* widget ) {
		int index = AbstractDynamicWidgetContainer::removeWidget( widget );
		emit changed();
		return index;
	};

	virtual QWidget *createSeparator( const QString &separatorText = QString() );
};

#endif // Multiple inclusion guard
