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

#ifndef FILTERWIDGET_HEADER
#define FILTERWIDGET_HEADER

#include <QWidget>
#include <QCheckBox>
#include <QToolButton>
#include <QListView>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QApplication>
#include <QStandardItemModel>

#include <KLineEdit>
#include <KNumInput>
#include <KDebug>

#include "global.h"
#include "filter.h"
#include "dynamicwidget.h"

class CheckCombobox;
class KComboBox;
class ConstraintWidget : public QWidget {
    Q_OBJECT

    public:
	ConstraintWidget( FilterType type,
			  QList<FilterVariant> availableVariants = QList<FilterVariant>(),
			  FilterVariant initialVariant = FilterNoVariant,
			  QWidget* parent = 0 );

	FilterType type() const { return m_constraint.type; };
	FilterVariant variant() const { return m_constraint.variant; };
	virtual QVariant value() const = 0;
	virtual void setValue( const QVariant &value ) = 0;
	Constraint constraint() {
	    m_constraint.value = value();
	    return m_constraint; };
	
	void addWidget( QWidget *w ) {
	    w->setVisible( true ); //m_variant != FilterDisregard );
	    layout()->addWidget( w );
	    m_widgets << w;
	};

	inline static ConstraintWidget *create( Constraint constraint,
						QWidget *parent = 0 ) {
	    return create( constraint.type, constraint.variant,
			   constraint.value, parent );
	};
	static ConstraintWidget *create( FilterType type,
		FilterVariant variant = FilterNoVariant,
		const QVariant &value = QVariant(), QWidget *parent = 0 );

    signals:
	void changed();
					 
    protected slots:
	virtual void variantChanged( int index );
	
    private:
	QString filterVariantName( FilterVariant filterVariant ) const;

	Constraint m_constraint;
	KComboBox *m_variantsCmb;
	QWidget *m_containerWidget;
	QWidgetList m_widgets, m_permanentWidgets;
};

class ConstraintListWidget : public ConstraintWidget {
    Q_OBJECT

    public:
	struct ListItem {
	    QString text;
	    QVariant value;
	    KIcon icon;

	    ListItem() {};
	    ListItem( const QString &text, const QVariant &value, const KIcon &icon ) {
		this->text = text;
		this->value = value;
		this->icon = icon; };
	};
	
	ConstraintListWidget( FilterType type, FilterVariant initialVariant,
			  const QList< ListItem > &values,
			  const QVariantList& initialValues,
			  QWidget* parent = 0 );
			  
	virtual QVariant value() const { return m_values; };
	virtual void setValue( const QVariant &value );
	
	QModelIndex indexFromValue( const QVariant &value );

    protected slots:
	void checkedItemsChanged();

    private:
	CheckCombobox *m_list;
	QVariantList m_values;
};

class ConstraintStringWidget : public ConstraintWidget {
    Q_OBJECT

    public:
	ConstraintStringWidget( FilterType type, FilterVariant initialVariant,
				const QString &filterText = QString(),
				QWidget* parent = 0 );
			    
	virtual QVariant value() const { return m_string->text(); };
	virtual void setValue( const QVariant &value ) {
	    m_string->setText(value.toString()); };
	
    protected slots:
	void stringChanged( const QString &newString ) {
	    Q_UNUSED( newString);
	    emit changed();
	};
	
    private:
	KLineEdit *m_string;
};

class ConstraintIntWidget : public ConstraintWidget {
    Q_OBJECT

    public:
	ConstraintIntWidget( FilterType type, FilterVariant initialVariant,
			 int value = 0, int min = 0, int max = 10000, QWidget* parent = 0 );

	virtual QVariant value() const { return m_num->value(); };
	virtual void setValue( const QVariant &value ) { m_num->setValue(value.toInt()); };
	    
    protected slots:
	void intChanged( int newInt ) {
	    Q_UNUSED( newInt );
	    emit changed();
	};
	
    private:
	KIntSpinBox *m_num;
};

class FilterWidget : public AbstractDynamicLabeledWidgetContainer {
    Q_OBJECT

    public:
	FilterWidget( QWidget* parent = 0 );
	FilterWidget( QList<FilterType> filterTypes, QWidget* parent = 0 );

	QList< ConstraintWidget* > constraintWidgets() const {
	    QList< ConstraintWidget* > list;
	    foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() )
		list << qobject_cast< ConstraintWidget* >( dynamicWidget->contentWidget() );
	    return list;
	};
	Filter filter() const {
	    Filter f;
	    foreach ( ConstraintWidget *constraint, constraintWidgets() )
		f << constraint->constraint();
	    return f;
	};

	inline void addConstraint( FilterType filterType ) {
	    addConstraint( createFilter(filterType) ); };
	void addConstraint( const Constraint &constraint ) {
	    addConstraint( ConstraintWidget::create(constraint, this) );
	};
	void addConstraint( ConstraintWidget *constraint );
	virtual int removeWidget( QWidget* widget );

	static FilterWidget *create( const Filter &filter, QWidget *parent = 0 ) {
	    FilterWidget *filterWidget = new FilterWidget( parent );
	    foreach ( Constraint constraint, filter )
		filterWidget->addConstraint( constraint );
	    return filterWidget;
	};
	
    signals:
	void changed();
	void constraintAdded( ConstraintWidget *constraintWidget );
	void constraintRemoved( const Constraint &constraint );

    public slots:
	inline void addConstraint() {
	    addConstraint( qobject_cast<ConstraintWidget*>(createNewWidget()) ); };
	void removeConstraint( ConstraintWidget *widget );

    protected slots:
	void filterTypeChanged( int index );

    protected:
	virtual QWidget *createNewWidget() {
	    return createFilter( firstUnusedFilterType() ); };
	virtual QWidget* createNewLabelWidget( int );
	
	virtual DynamicWidget *addWidget( QWidget *widget ) {
	    return AbstractDynamicLabeledWidgetContainer::addWidget( widget ); };
	virtual DynamicWidget *addWidget( QWidget *labelWidget, QWidget *widget );

	virtual void updateLabelWidget( QWidget*, int ) {};

    private:
	ConstraintWidget *createFilter( FilterType type );
	QString filterName( FilterType filter ) const;
	FilterType firstUnusedFilterType() const;
	
	QList< KComboBox* > m_filterTypes;
};

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
	
	static FilterListWidget *create( const FilterList &filterList,
					 QWidget *parent = 0 ) {
	    FilterListWidget *filterListWidget = new FilterListWidget( parent );
	    foreach ( Filter filter, filterList )
		filterListWidget->addFilter( filter );
	    return filterListWidget;
	};
	
    signals:
	void changed();
	
    protected:
	virtual QWidget *createNewWidget() {
	    Filter filter;
	    filter << Constraint();
	    return FilterWidget::create( filter, this );
	};
	
	virtual DynamicWidget *addWidget( QWidget* widget );
	
	virtual int removeWidget( QWidget* widget ) {
	    int index = AbstractDynamicWidgetContainer::removeWidget( widget );
	    emit changed();
	    return index;
	};
	
	virtual QWidget *createSeparator( const QString &separatorText = QString() );
};

#endif // Multiple inclusion guard