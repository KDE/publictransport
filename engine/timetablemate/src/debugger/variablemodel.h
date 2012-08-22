
#ifndef VARIABLEMODEL_H
#define VARIABLEMODEL_H

// Own includes
#include "debuggerstructures.h"

// PublicTransport engine includes
#include <engine/enums.h>

// KDE includes
#include <KIcon>

// Qt includes
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>

class Project;
class QMutex;

namespace Debugger {

class Debugger;
class VariableModel;

class VariableItem;
struct VariableItemList {
    friend class VariableItem;

    void remove( const QString &name );
    void remove( const QStringList &names );

    inline bool isEmpty() const { return variables.isEmpty(); };
    inline int count() const { return variables.count(); };
    inline void clear() {
        variables.clear();
        nameToVariable.clear();
    };
    void append( VariableItem *item );
    void append( const QList< VariableItem* > &items );
    void append( const VariableItemList &items );
    inline VariableItemList &operator <<( VariableItem *item ) {
        append( item );
        return *this;
    };
    inline VariableItemList &operator <<( const VariableItemList &itemList ) {
        append( itemList );
        return *this;
    };

    inline VariableItem *const &at( int index ) const { return variables.at(index); };
    inline VariableItem *takeAt( int index );
    inline VariableItem *const &operator []( int index ) const { return variables.operator[](index); };
    inline VariableItem *&operator []( int index ) { return variables.operator[](index); };
    inline int indexOf( VariableItem *item ) const { return variables.indexOf(item); };

    inline operator QList< VariableItem* >() const { return variables; };

    virtual ~VariableItemList();

    QList< VariableItem* > variables;
    QHash< QString, VariableItem* > nameToVariable;
};
typedef QStack< VariableItemList > VariableStack;

/**
 * @brief Variable types.
 * The order of the enumberables (ie. it's values) defines the sort order in a VariableModel.
 **/
enum VariableType {
    InvalidVariable = 0, /**< Invalid variable object. */

    SpecialVariable, /**< Used for special information objects,
            * generated for some default script objects. */
    ErrorVariable, /**< An error. */
    FunctionVariable, /**< A function. */
    ObjectVariable, /**< An object. */
    RegExpVariable, /**< A regular expression. */
    DateVariable, /**< A date. */
    ArrayVariable, /**< An array / list. */
    BooleanVariable, /**< A boolean. */
    StringVariable, /**< A string. */
    NumberVariable, /**< A number. */
    NullVariable /**< Null/undefined. */
};

enum VariableFlag {
    NoVariableFlags             = 0x0000, /**< No flags. */
    VariableIsHelperObject      = 0x0001, /**< Whether or not this variable is a helper script
                                             * object, eg. the 'result', 'network', 'storage'
                                             * (etc.) script objects. */
    VariableHasErroneousValue   = 0x0002, /**< The value of the variable is erroneous. */
    VariableIsDefinedInParentContext
                                = 0x0004, /**< The variable was defined in a parent context. */
    VariableIsChanged           = 0x0008  /**< The variable was just changed. */
};
Q_DECLARE_FLAGS( VariableFlags, VariableFlag );

/** @brief Contains data for a variable, used by VariableItem. */
struct VariableData {
    VariableData( const QString &name = QString() )
            : type(InvalidVariable), flags(NoVariableFlags), name(name) {};
    VariableData( VariableType type, const QString &name,
                  const QScriptValue &value = QScriptValue(), const KIcon &icon = KIcon(),
                  VariableFlags flags = NoVariableFlags )
            : type(type), flags(flags), name(name),
              scriptValue(value), value(value.toVariant()), icon(icon) {};

    VariableType type; /**< The type of the variable. */
    VariableFlags flags; /**< Flags of the variable. */
    QString name; /**< The name of the variable. */
    QScriptValue scriptValue;
    QVariant value; /**< The current value of the variable. */
    QString valueString;
    QString completeValueString;
    KIcon icon; /**< An icon for the variable. */
    QString description; /**< A description for the variable, eg. for tooltips. */

    inline bool isChanged() const { return flags.testFlag(VariableIsChanged); };
    inline bool isHelperObject() const { return flags.testFlag(VariableIsHelperObject); };
    inline bool hasErroneousValue() const { return flags.testFlag(VariableHasErroneousValue); };
    inline bool isDefinedInParentContext() const {
            return flags.testFlag(VariableIsDefinedInParentContext); };

    bool operator ==( const VariableData &other ) const;
    inline bool operator !=( const VariableData &other ) const {
        return !this->operator==( other );
    };
};

/**
 * @brief Contains data for a variable including data objects for it's child variables.
 *
 * This class gets used to construct a single object that contains all data for a variable and all
 * it's children. It gets used by VariableChange.
 *
 * In a VariableModel parent-child relationships are added by wrapping VariableData with
 * VariableItem, which contains pointers to the parent/children VariableItem's.
 **/
struct VariableTreeData : VariableData {
    VariableTreeData( const QString &name = QString() ) : VariableData(name) {};
    VariableTreeData( VariableType type, const QString &name,
                  const QScriptValue &value = QScriptValue(), const KIcon &icon = KIcon() )
            : VariableData(type, name, value, icon) {};

    static VariableTreeData fromScripValue( const QString &name, const QScriptValue &value );
    static VariableTreeData fromTimetableData( Enums::TimetableInformation info,
                                               const QVariant &data );
    operator VariableData() const;

    QList< VariableTreeData > children;
};

/**
 * @brief A variable item.
 *
 * Uses VariableData to store
 **/
class VariableItem {
    friend class VariableModel; // VariableModel uses constructors of VariableItem
    friend struct VariableItemList;

public:
    static QString variableValueTooltip( const QString &completeValueString,
                                         bool encodeHtml, const QChar &endCharacter );

    /** @brief The VariableModel this variable item belongs to. */
    inline VariableModel *model() const { return m_model; };

    /** @brief The parent variable item of this one, if any. */
    inline VariableItem *parent() const { return m_parent; };

    /** @brief Child variable items of this variable item. */
    inline const VariableItemList &children() const { return m_children; };

    /**
     * @brief The QModelIndex of this variable item in a VariableModel.
     * @note If this item was not added to a model, this function always returns an invalid index.
     **/
    QModelIndex index();

    /** @brief The type of the variable. */
    inline VariableType type() const { return m_data.type; };

    /** @brief Flags of the variable. */
    inline VariableFlags flags() const { return m_data.flags; };

    bool isSimpleValueType() const;
    QString displayValueString() const;

    /** @brief The name of the variable. */
    inline QString name() const { return m_data.name; };

    /** @brief The QScriptValue object from which this VariableItem was created. */
    inline QScriptValue scriptValue() const { return m_data.scriptValue; };

    /** @brief The current value of the variable. */
    inline QVariant value() const { return m_data.value; };

    /** @brief A string describing the value of the variable, to be displayed to users. */
    inline QString valueString() const { return m_data.valueString; };

    /** @brief The complete value of the variable as string, can be very long. */
    inline QString completeValueString() const { return m_data.completeValueString; };

    /** @brief An icon for the variable. */
    inline KIcon icon() const { return m_data.icon; };

    /** @brief A description for the variable, eg. for tooltips. */
    inline QString description() const { return m_data.description; };

    inline bool isChanged() const { return m_data.isChanged(); };
    void setChanged( bool changed = true );

    /**
     * @brief True, if this variable is a helper script object.
     * Eg. the 'result', 'network', 'storage' (etc.) script objects.
     **/
    inline bool isHelperObject() const { return m_data.isHelperObject(); };

    inline bool hasErroneousValue() const { return m_data.hasErroneousValue(); };
    inline bool isDefinedInParentContext() const { return m_data.isDefinedInParentContext(); };

    /** @brief The VariableData object which contains all data for the variable. */
    inline const VariableData &data() const { return m_data; };
    inline VariableData &data() { return m_data; };

    ~VariableItem();

    void addChild( VariableItem *item );
    void addChildren( const VariableItemList &items );

    bool isValid() const { return m_model; };

    VariableItem &operator =( const VariableItem &item );

protected:
    VariableItem( VariableModel *model = 0 ) : m_model(model), m_parent(0) {};
    VariableItem( VariableModel *model, const VariableData &data, VariableItem *parent = 0 )
            : m_model(model), m_parent(parent), m_data(data) {};
    VariableItem( VariableModel *model, const VariableItem *other, VariableItem *parent = 0 )
            : m_model(model), m_parent(parent)
    {
        setValuesOf( other );
    };
    VariableItem( VariableModel *model, VariableType type, const QString &name, const QScriptValue &value,
                  const KIcon &icon = KIcon(), VariableItem *parent = 0 )
            : m_model(model), m_parent(parent), m_data(VariableData(type, name, value, icon))
    {};

    inline void setParent( VariableItem *parent ) { m_parent = parent; };

    void setValuesOf( const VariableItem *other );
    void setData( const VariableTreeData &changedItem );
    void setValue( const QVariant &value );

    inline VariableItemList *childrenPointer() { return &m_children; };

private:
    VariableModel *const m_model;
    VariableItem *m_parent;
    VariableItemList m_children;
    VariableData m_data;
};

/** @brief Types of variable changes. */
enum VariableChangeType {
    NoOpVariableChange = 0, /**< No variable change. */

    PushVariableStack, /**< Push another variable list onto the stack. */
    PopVariableStack, /**< Pop a variable list from the stack. */
    UpdateVariables /**< Update variables in the top variable list on the stack. */
};

/**
 * @brief Describes a change in a VariableModel.
 *
 * The debugger uses this class to keep connected variable models in sync with the variables in
 * the current execution context by emitting DebuggerAgent::variablesChanged() with instances of
 * this class. Connect that signal with the VariableModel::applyChange() slot of models that should
 * be updated.
 **/
struct VariableChange {
    VariableChange( VariableChangeType type = NoOpVariableChange ) : type(type) {};
    VariableChange( VariableChangeType type,
                    const QStack< QList<VariableTreeData> > &newVariableStack )
            : type(type), variableStack(newVariableStack) {};

    static VariableChange fromContext( QScriptContext *context );

    /** @brief The type of the change. */
    VariableChangeType type;

    /** @brief A list of variables associated with the change. */
    QStack< QList<VariableTreeData> > variableStack;
};

/**
 * @brief A model for variables.
 *
 * Each Debugger object uses a VariableModel to store the current variables. It gets updated
 * through queued connections with the DebuggerAgent used by the Debugger, ie. the
 * DebuggerAgent::variablesChanged() signal gets connected with the VariableModel::applyChange()
 * slot.
 **/
class VariableModel : public QAbstractItemModel
{
    Q_OBJECT
    friend class VariableItem;

public:
    /** @brief Columns in a VariableModel. */
    enum Column {
        NameColumn = 0, /**< Contains the names of the variables. */
        ValueColumn, /**< Contains the values of the variables. */

        ColumnCount /**< An invalid column, it's value is the number of columns. */
    };

    enum Roles {
        SortRole = Qt::UserRole + 1,
        CompleteValueRole = Qt::UserRole + 2,
        ContainsBinaryDataRole = Qt::UserRole + 3
    };

    VariableModel( QObject *parent = 0 );
    virtual ~VariableModel();

    virtual int columnCount( const QModelIndex &parent = QModelIndex() ) const {
            Q_UNUSED(parent); return ColumnCount; };
    virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual QModelIndex index( int row, int column,
                               const QModelIndex &parent = QModelIndex() ) const;

    /** @brief Get the index of the parent item of @p child, if any. */
    virtual QModelIndex parent( const QModelIndex &child ) const;

    virtual QVariant headerData( int section, Qt::Orientation orientation,
                                 int role = Qt::DisplayRole ) const;

    virtual QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const;
    virtual bool setData( const QModelIndex &index, const QVariant &value,
                          int role = Qt::EditRole );
    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;
    virtual bool removeRows( int row, int count, const QModelIndex &parent = QModelIndex() );
    void clear();

    /**
     * @brief Whether or not this model is empty.
     *
     * The model also returns true, if there are variables deeper in the stack.
     **/
    bool isEmpty() const;

    void addVariablesFromActivationObject( const QScriptValue &activationObject );

    QModelIndex indexFromVariable( VariableItem *variable, int column = 0 ) const;
    VariableItem *variableFromIndex( const QModelIndex &index ) const;

    VariableItemList variableStack( int depth = 0 ) const { return m_variableStack[depth]; };
    int variableStackCount() const;

    virtual void sort( int column, Qt::SortOrder order = Qt::AscendingOrder );

    static QList<VariableTreeData> variablesFromScriptValue( const QScriptValue &value,
            int maxDepth = 10, const QList<QScriptValue> &parents = QList<QScriptValue>() );

public slots:
    /** @brief Applies the given @p change. */
    void applyChange( const VariableChange &change );

    /** @brief Push an empty variable stack to the list of variable stacks in this model. */
    void pushVariableStack();

    /** @brief Removes the variable stack on the top of the list of variable stacks in this model. */
    void popVariableStack();

    /** @brief Shows variables in the variable stack at the given @p depth. */
    void switchToVariableStack( int depth = 0 );

    /** @brief Updates the variable stack with @p newVariableStack. */
    void updateVariableStack( const QStack< QList<VariableTreeData> > &newVariableStack,
                              VariableItem *parent = 0 );

protected slots:
    /**
     * @brief Updates the variable list in the current depth with those in @p newVariables.
     * @return @c True, if variables have changed, @c false otherwise.
     **/
    bool updateVariables( const QList<VariableTreeData> &newVariables,
                          VariableItem *parent = 0, bool currentDepth = true );

protected:
    void addChild( VariableItem *parentItem, VariableItem *item );
    void addChildren( VariableItem *parentItem, const VariableItemList &items );

private:
    int depthToIndex( int depth ) const;
    bool isIndexValid( int index ) const;

    QModelIndex indexFromVariableAlreadyLocked( VariableItem *variable, int column = 0 ) const;
    void addChildrenAlreadyLocked( VariableItem *parentItem, const VariableItemList &items );

    void addVariablesFromScriptValue( const QScriptValue &activationObject, VariableItem *parent );
    void sortVariableItemList( VariableItemList *variables,
                               QModelIndexList *oldPersistentIndexes,
                               QModelIndexList *newPersistentIndexes,
                               const QModelIndexList &persistentIndexes,
                               Qt::SortOrder order = Qt::AscendingOrder );

    VariableStack m_variableStack;
    VariableItemList m_allContextVariables; // Shown in all context depths
    int m_depthIndex;
};

class VariableFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    VariableFilterProxyModel( QObject *parent = 0 ) : QSortFilterProxyModel(parent) {};

protected:
    virtual bool filterAcceptsRow( int sourceRow, const QModelIndex &sourceParent ) const;
};

inline QDebug &operator <<( QDebug debug, VariableType type )
{
    switch( type ) {
    case NullVariable:
        return debug << "NullVariable";
    case ErrorVariable:
        return debug << "ErrorVariable";
    case FunctionVariable:
        return debug << "FunctionVariable";
    case ArrayVariable:
        return debug << "ArrayVariable";
    case ObjectVariable:
        return debug << "ObjectVariable";
    case BooleanVariable:
        return debug << "BooleanVariable";
    case NumberVariable:
        return debug << "NumberVariable";
    case StringVariable:
        return debug << "StringVariable";
    case RegExpVariable:
        return debug << "RegExpVariable";
    case DateVariable:
        return debug << "DateVariable";
    case SpecialVariable:
        return debug << "SpecialVariable";
    default:
        return debug << "Variable type unknown" << static_cast<int>( type );
    }
}

}; // namespace Debugger

#endif // Multiple inclusion guard
