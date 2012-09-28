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

// Header
#include "variablemodel.h"

// Own includes
#include "debuggerstructures.h"
#include "debugger.h"

// Public Transport engine includes
#include <engine/script/serviceproviderscript.h>
#include <engine/global.h>

// KDE includes
#include <KDebug>
#include <KLocalizedString>
#include <KColorScheme>
#include <KGlobalSettings>
#include <KStringHandler>

// Qt includes
#include <QScriptValueIterator>

namespace Debugger
{

void VariableItemList::remove( const QString &name )
{
    if ( !nameToVariable.contains(name) ) {
        kDebug() << "Name not contained" << name;
        return;
    }

    VariableItem *item = nameToVariable.take( name );
    variables.removeOne( item );
}

void VariableItemList::remove( const QStringList &names )
{
    foreach ( const QString &name, names ) {
        remove( name );
    }
}

void VariableItemList::append( VariableItem *item )
{
    const QString name = item->name();
    if ( nameToVariable.contains(name) ) {
        const int index = variables.indexOf( nameToVariable[name] );
        variables[ index ]->setValuesOf( item );
    } else {
        variables.append( item );
    }
    nameToVariable.insert( name, item );
}

void VariableItemList::append( const QList< VariableItem * >& items )
{
    foreach ( VariableItem *item, items ) {
        append( item );
    }
}

void VariableItemList::append( const VariableItemList &items )
{
    for( int i = 0; i < items.count(); ++i ) {
        append( items[i] );
    }
}

VariableItem *VariableItemList::takeAt( int index )
{
    VariableItem *item = variables.takeAt( index );
    nameToVariable.remove( item->name() );
    return item;
}

bool VariableItem::isSimpleValueType() const
{
    switch ( type() ) {
    case BooleanVariable:
    case NumberVariable:
    case StringVariable:
    case RegExpVariable:
        return true;
    case InvalidVariable:
    case NullVariable:
    case ErrorVariable:
    case FunctionVariable:
    case ArrayVariable:
    case ObjectVariable:
    case DateVariable:
    case SpecialVariable:
    default:
        return false;
    }
}

VariableModel::VariableModel( QObject *parent )
        : QAbstractItemModel( parent ), m_depthIndex(-1)
{
}

VariableModel::~VariableModel()
{
    while ( !m_variableStack.isEmpty() ) {
        popVariableStack();
    }
}

VariableItem::~VariableItem()
{
    qDeleteAll( m_children.variables );
}

VariableItemList::~VariableItemList()
{
}

QVariant VariableModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
        switch ( static_cast<Column>(section) ) {
        case NameColumn:
            return i18nc("@title:column", "Name");
        case ValueColumn:
            return i18nc("@title:column", "Value");
        default:
            return QVariant();
        }
    }

    return QVariant();
}

QString VariableItem::displayValueString() const
{
    if ( !valueString().isEmpty() ) {
        return valueString();
    } else if ( !value().toString().isEmpty() ) {
        return value().toString();
    } else {
        return QString();
    }
}

QVariant VariableModel::data( const QModelIndex &index, int role ) const
{
    if ( !index.isValid() ) {
        return QVariant();
    }
    if ( index.model() != this ) {
        kWarning() << "Invalid model";
        return QVariant();
    }

    VariableItem *item = variableFromIndex( index );
    switch ( role ) {
    case Qt::DisplayRole:
        switch ( index.column() ) {
        case NameColumn:
            return item->name();
        case ValueColumn:
            return item->displayValueString();
        default:
            kWarning() << "Unknown variable model column" << index.column();
            break;
        }
        break;
    case Qt::EditRole:
        switch ( index.column() ) {
        case ValueColumn:
            return item->completeValueString();
        default:
            break;
        }
        break;
    case Qt::DecorationRole:
        switch ( index.column() ) {
        case NameColumn:
            return item->icon();
        case ValueColumn:
        default:
            break;
        }
        break;
    case Qt::BackgroundRole:
        switch ( index.column() ) {
        case NameColumn:
        case ValueColumn: {
            KColorScheme::BackgroundRole role;
            switch ( item->type() ) {
            case NullVariable:
                role = KColorScheme::NormalBackground;
                break;
            case ErrorVariable:
                role = KColorScheme::NegativeBackground;
                break;
            default:
                if ( item->isHelperObject() ) {
                    role = KColorScheme::ActiveBackground;
                } else if ( item->isDefinedInParentContext() ) {
                    // Use alternate background for variables from a parent context
                    role = KColorScheme::AlternateBackground;
                } else if ( item->hasErroneousValue() ) {
                    role = KColorScheme::NegativeBackground;
                } else if ( item->isChanged() ) {
                    role = KColorScheme::ActiveBackground;
                } else {
                    role = KColorScheme::NormalBackground;
                }
                break;
            }
            return KColorScheme( QPalette::Active ).background( role );
        }
        default:
            break;
        }
        break;
    case Qt::ForegroundRole:
        switch ( index.column() ) {
        case NameColumn:
        case ValueColumn: {
            KColorScheme::ForegroundRole role;
            switch ( item->type() ) {
            case NullVariable:
                role = KColorScheme::InactiveText;
                break;
            case ErrorVariable:
                role = KColorScheme::NegativeText;
                break;
            default:
                if ( item->hasErroneousValue() ) {
                    role = KColorScheme::NegativeText;
                } else if ( item->isChanged() ) {
                    role = KColorScheme::ActiveText;
                } else {
                    role = KColorScheme::NormalText;
                }
                break;
            }
            return KColorScheme( QPalette::Active ).foreground( role );
        }
        default:
            break;
        }
        break;
    case Qt::ToolTipRole:
        switch ( index.column() ) {
        case ValueColumn:
            return item->description();
        case NameColumn:
        default:
            break;
        }
        break;
    case CompleteValueRole:
        if ( item->scriptValue().isVariant() && !item->value().toByteArray().isNull() ) {
            return item->value().toByteArray();
        } else {
            return item->completeValueString();
        }
    case ContainsBinaryDataRole:
        return item->scriptValue().isVariant() && !item->value().toByteArray().isNull();
    }

    return QVariant();
}

bool VariableModel::setData( const QModelIndex &index, const QVariant &value, int role )
{
    Q_UNUSED( index );
    Q_UNUSED( value );
    Q_UNUSED( role );
    return false; // TODO
//     if ( !index.isValid() || index.column() == ValueColumn || role != Qt::EditRole ) {
//         return false;
//     }
//
//     VariableItem *item = variableFromIndex( index );
//     item->setValue( value );
// //     emit updateVariable( VariableChange(
//     return true;
}

Qt::ItemFlags VariableModel::flags( const QModelIndex &index ) const
{
    if ( !index.isValid() ) {
        return Qt::NoItemFlags;
    }

    const Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
//     if ( rowCount(index) == 0 ) { TODO
//         // Leaf items are editable
//         VariableItem *variable = variableFromIndex( index );
//         if ( variable->isSimpleValueType() ) {
//             flags |= Qt::ItemIsEditable;
//         }
//     }
    return flags;
}

int VariableModel::depthToIndex( int depth ) const
{
    if ( depth > m_variableStack.count() - 1 ) {
        // Cannot go to the wanted depth, -1 is used as index to a virtual empty variable list
        return -1;
    }
    return m_variableStack.count() - 1 - depth;
}

void VariableModel::switchToVariableStack( int depth )
{
    const int depthIndex = depthToIndex( depth );
    const int oldDepthIndex = m_depthIndex;

    if ( depthIndex == oldDepthIndex ) {
        // Depth was not changed
        return;
    }

    // Tell views that the model changed and all items are removed
    const int oldVariableCount = isIndexValid( oldDepthIndex )
            ? m_variableStack.at(oldDepthIndex).count() : 0;
    const int newVariableCount = isIndexValid( depthIndex )
            ? m_variableStack.at(depthIndex).count() : 0;

    if ( oldVariableCount > 0 ) {
        // Tell views to remove all variables of the old depth,
        // by switching to the virtual empty variable list
        beginRemoveRows( QModelIndex(), 0, oldVariableCount - 1 );
        m_depthIndex = -1;
        endRemoveRows();
    }

    // Go to the new depth and inform views if variables get inserted
    if ( newVariableCount > 0 ) {
        // There are variables in the new stack depth, tell views how many variables get added
        beginInsertRows( QModelIndex(), 0, newVariableCount - 1 );
        m_depthIndex = depthIndex;
        endInsertRows();
    } else {
        // The model is already empty and is still empty in the new stack depth
        m_depthIndex = depthIndex;
    }
}

int VariableModel::variableStackCount() const
{
    return m_variableStack.count();
}

void VariableModel::pushVariableStack()
{
    m_variableStack.push( VariableItemList() );
    switchToVariableStack( 0 );
}

void VariableModel::popVariableStack()
{
    if ( m_variableStack.isEmpty() ) {
        kWarning() << "Cannot pop, variable stack is empty";
        return;
    }

    switchToVariableStack( 1 );
    qDeleteAll( m_variableStack.pop().variables );
}

VariableTreeData::operator VariableData() const
{
    VariableData data( type, name, scriptValue, icon, flags );
    data.value = value;
    data.valueString = valueString;
    data.description = description;
    data.completeValueString = completeValueString;
    return data;
}

VariableChange VariableChange::fromContext( QScriptContext *context ) {
    QStack< QList<VariableTreeData> > newVariableStack;
    while ( context ) {
        const QScriptValue &activationObject = context->activationObject();
        newVariableStack.append( VariableModel::variablesFromScriptValue(activationObject) );
        context = context->parentContext();
    }
    return VariableChange( UpdateVariables, newVariableStack );
}

typedef QPair<VariableItem*, VariableTreeData> VariableItemPair;

void VariableModel::updateVariableStack( const QStack< QList<VariableTreeData> > &newVariableStack,
                                         VariableItem *parent )
{

    for ( int currentIndex = 0; currentIndex < newVariableStack.count(); ++currentIndex ) {
        // Update variables
        // kDebug() << "Update variables at" << currentIndex << "current depth:" << m_depthIndex;
        updateVariables( newVariableStack.at(currentIndex), parent,
                         newVariableStack.count() - 1 - currentIndex != m_depthIndex );
    }

    // Sort by variable names
    sort( 0 );
}

void VariableItem::setChanged( bool changed )
{
    if ( changed ) {
        m_data.flags | VariableIsChanged;
    } else {
        m_data.flags &= ~VariableIsChanged;
    }

    foreach ( VariableItem *child, m_children.variables ) {
        child->setChanged( changed );
    }
}

bool VariableData::operator==( const VariableData &other ) const
{
    return type == other.type && name == other.name && value == other.value &&
           completeValueString == other.completeValueString &&
           description == other.description;
}

bool VariableModel::updateVariables( const QList<VariableTreeData> &variables,
                                     VariableItem *parent, bool currentDepth )
{
    VariableItemList *currentVariables;
    if ( parent ) {
        currentVariables = parent->childrenPointer();
    } else if ( isIndexValid(m_depthIndex) ) {
        currentVariables = &m_variableStack[ m_depthIndex ];
    } else {
        kDebug() << "Invalid depth" << m_depthIndex;
        return false;
    }

    if ( currentVariables->isEmpty() && variables.isEmpty() ) {
        return false;
    }

    bool hasChanges = false;
    QHash< QString, VariableTreeData > newVariables;
    foreach ( const VariableTreeData &item, variables ) {
        newVariables.insert( item.name, item );
    }

    QList< VariableItemPair > newItems, changedItems;
    QStringList removedNames;
    if ( currentVariables ) {
        foreach ( VariableItem *item, currentVariables->variables ) {
            // Do not remove helper objects or special type variables
            // Only remove variables from the current depth
            if ( !item->isHelperObject() && item->type() != SpecialVariable &&
                 (item->isDefinedInParentContext() ^ currentDepth) )
            {
                removedNames << item->name();
            }
        }
    }

    for ( QHash< QString, VariableTreeData >::ConstIterator it = newVariables.constBegin();
          it != newVariables.constEnd(); ++it )
    {
        VariableTreeData data = it.value();
        if ( !currentDepth ) {
            data.flags |= VariableIsDefinedInParentContext;
        }
        removedNames.removeOne( it.key() );

        if ( currentVariables && currentVariables->nameToVariable.contains(it.key()) ) {
            VariableItem *variable = currentVariables->nameToVariable[ it.key() ];
            const bool childrenChanged = updateVariables( data.children, variable )
                    && (!data.scriptValue.isFunction() || data.scriptValue.isRegExp());

            const bool changed = !data.scriptValue.isVariant() &&
                    (!data.scriptValue.isFunction() || data.scriptValue.isRegExp()) &&
                    variable->data() != data;
            if ( !changed && !childrenChanged ) {
                if ( !data.flags.testFlag(VariableIsChanged) ) {
                    // Nothing changed
                    continue;
                }

                // Remove change mark,
                // do not set hasChanges to true, because only the change mark gets removed
                data.flags &= ~VariableIsChanged;
            } else {
                // Mark as changed
                data.flags |= VariableIsChanged;
                hasChanges = true;
            }
            changedItems << qMakePair( variable, data ); // Change values of variable to values of it.value()
        } else {
            VariableItem *variable = new VariableItem( this, data, parent );
            newItems << qMakePair( variable, data );
        }
    }

    // Remove old variables, if any
    if ( !removedNames.isEmpty() ) {
        foreach ( const QString &removedName, removedNames ) {
            VariableItem *item = currentVariables->nameToVariable[ removedName ];
            const QModelIndex index = item->index();
            beginRemoveRows( index.parent(), index.row(), index.row() );
            currentVariables->remove( removedName );
            endRemoveRows();
        }
    }

    // Insert new variables, if any
    if ( !newItems.isEmpty() ) {
        // No parent for the new items
        const int count = currentVariables->count();
        const QModelIndex parentIndex = indexFromVariable( parent );

        // Append copied variable item
        beginInsertRows( parentIndex, count, count + newItems.count() - 1 );
        foreach ( const VariableItemPair &newItem, newItems ) {
            currentVariables->append( newItem.first );
        }
        endInsertRows();

        // Recursively add children
        foreach ( const VariableItemPair &newItemPair, newItems ) {
            updateVariables( newItemPair.second.children, newItemPair.first );
        }
    }

    // Change changed variables, if any
    // (delete items in changedItems after applying changes to the existing items)
    if ( !changedItems.isEmpty() ) {
        QModelIndexList indexes;
        foreach ( const VariableItemPair &changedItemPair, changedItems ) {
            VariableItem *modelItem = changedItemPair.first;
            const VariableTreeData &changedItem = changedItemPair.second;

            modelItem->setData( changedItem );
            currentVariables->append( modelItem );
            QModelIndex index = indexFromVariableAlreadyLocked( modelItem );
            indexes << indexFromVariableAlreadyLocked( modelItem );
        }

        foreach ( const QModelIndex &index, indexes ) {
            dataChanged( index, index );
        }
    }

    // Return true, if changes have been made
    return hasChanges || !removedNames.isEmpty() || !newItems.isEmpty();
}

void VariableModel::applyChange( const VariableChange &change )
{
    switch ( change.type ) {
    case PushVariableStack:
        pushVariableStack();
        break;
    case PopVariableStack:
        popVariableStack();
        break;
    case UpdateVariables:
        updateVariableStack( change.variableStack );
        break;
    default:
        kWarning() << "VariableChange type not implemented" << change.type;
        break;
    }
}

void VariableItem::setValuesOf( const VariableItem *other )
{
    m_data = other->m_data;
}

void VariableItem::setData( const VariableTreeData &changedItem )
{
    m_data = changedItem;
}

void VariableItem::setValue( const QVariant &value )
{
    m_data.value = value;

    const QString valueString = value.toString();
    const int cutPos = qMin( 200, valueString.indexOf('\n') );
    if ( cutPos != -1 ) {
        m_data.valueString = valueString.left( cutPos ) + " ...";
    } else {
        m_data.valueString = value.toString().left( 200 ) + " ...";
    }
}

int variableTypeSort( VariableType type ) {
    switch ( type ) {
    case SpecialVariable:
        return 0;
    case ErrorVariable:
        return 1;
    case ObjectVariable:
    case RegExpVariable:
    case DateVariable:
    case ArrayVariable:
    case BooleanVariable:
    case StringVariable:
    case NumberVariable:
        return 5;
    case NullVariable:
        return 9;
    case InvalidVariable:
    case FunctionVariable:
    default:
        return 10;
    }
}

bool variableItemLessThan( VariableItem *item1, VariableItem *item2 ) {
    if ( item1->isHelperObject() != item2->isHelperObject() ) {
        return item1->isHelperObject();
    }

    const int s1 = variableTypeSort( item1->type() );
    const int s2 = variableTypeSort( item2->type() );
    if ( s1 != s2 ) {
        return s1 < s2;
    } else if ( item1->isDefinedInParentContext() != item2->isDefinedInParentContext() ) {
        return item1->isDefinedInParentContext(); // Sort variables from parent context to the beginning
    } else {
        // Sort by variable name
        if ( KGlobalSettings::naturalSorting() ) {
            return KStringHandler::naturalCompare( item1->name(), item2->name(),
                                                   Qt::CaseInsensitive ) < 0;
        } else {
            return QString::localeAwareCompare( item1->name(), item2->name() ) < 0;
        }
    }
}

bool variableItemSortLessThan( const QPair<VariableItem*, int> &item1,
                               const QPair<VariableItem*, int> &item2 )
{
    return variableItemLessThan( item1.first, item2.first );
}

void VariableModel::sort( int column, Qt::SortOrder order )
{
    Q_UNUSED( column );

    const int depthIndex = m_depthIndex;
    if ( !isIndexValid(depthIndex) ) {
        return;
    }

    // Store old persistent indexes
    emit layoutAboutToBeChanged();
    const QModelIndexList persistentIndexes = persistentIndexList();

    QModelIndexList oldPersistentIndexes, newPersistentIndexes;
    VariableItemList *variables = &m_variableStack[m_depthIndex];
    sortVariableItemList( variables, &oldPersistentIndexes, &newPersistentIndexes,
                          persistentIndexes, order );

    changePersistentIndexList( oldPersistentIndexes, newPersistentIndexes );
    emit layoutChanged();
}

void VariableModel::sortVariableItemList( VariableItemList *variables,
        QModelIndexList *oldPersistentIndexes, QModelIndexList *newPersistentIndexes,
        const QModelIndexList &persistentIndexes, Qt::SortOrder order )
{
    Q_ASSERT( variables );

    // TODO Create a function for this "childVariableList( VariableItem *parent )"
//     QList< VariableItem* > *variables = parent
//             ? &parent->childrenPointer()->variables : &(m_variableStack[m_depthIndex].variables);
    const int count = variables->count();
    if ( count == 0 ) {
        // There are no children to sort
        return;
    }

    QVector< QPair<VariableItem*, int> > sortable( count );
    for ( int row = 0; row < count; ++row ) {
        sortable[ row ] = qMakePair( variables->operator[](row), row );
    }

    qStableSort( sortable.begin(), sortable.end(), variableItemSortLessThan );

    // Clear unsorted old variable list
    variables->clear();

    // And fill it again in sorted order
    for ( int newRow = 0; newRow < count; ++newRow ) {
        const QPair<VariableItem*, int> sortableItem = sortable[ newRow ];
        VariableItem *item = sortableItem.first;
        const int oldRow = sortableItem.second;
        variables->append( item );

        for ( int column = 0; column < columnCount(); ++column ) {
            const QModelIndex oldIndex = createIndex( oldRow, column, item );
            if ( persistentIndexes.contains(oldIndex) && oldRow != newRow ) {
                const QModelIndex newIndex = createIndex( newRow, column, item );
                oldPersistentIndexes->append( oldIndex );
                newPersistentIndexes->append( newIndex );
            }
        }
    }

    // Sort children of the children
    foreach ( VariableItem *childVariable, variables->variables ) {
        sortVariableItemList( childVariable->childrenPointer(),
                              oldPersistentIndexes, newPersistentIndexes, persistentIndexes, order );
    }
}

QList<VariableTreeData> VariableModel::variablesFromScriptValue( const QScriptValue &value,
        int maxDepth, const QList<QScriptValue> &parents )
{
    QScriptValueIterator it( value );

    // Limit depth of variable children
    if ( it.hasNext() && maxDepth <= 0 ) {
        kWarning() << "Maximum variable depth reached";
        return QList<VariableTreeData>();
    }

    // Prevent recursively adding the same child
    foreach ( const QScriptValue &parent, parents ) {
        if ( parent.strictlyEquals(value) ) {
            // Recursive variable found
            return QList<VariableTreeData>()
                    << VariableTreeData( i18nc("@info/plain", "(Recursive variable)") );
        }
    }

    QList<VariableTreeData> variables;
    while ( it.hasNext() ) {
        if ( variables.count() >= 500 ) {
            // Add a dummy variable instead of adding more than 500 variables
            kWarning() << "Only show up to 500 variables per level";
            variables << VariableTreeData( "..." );
            break;
        }

        it.next();

        const QString childName = it.name();
        const QScriptValue childValue = it.value();
//         if ( (childValue.isFunction() && childValue.property("prototype").isValid()) || // Constructors
        if ( it.flags().testFlag(QScriptValue::SkipInEnumeration) ||
             childName == QLatin1String("Qt")|| // Too many enumerables in there, bad performance
             childName == QLatin1String("QtConcurrent")|| // Unused
             childName == QLatin1String("NaN") || childName == QLatin1String("undefined") ||
             childName == QLatin1String("Infinity") || childName == QLatin1String("objectName") ||
             childName == QLatin1String("callee") ||
             childName.startsWith(QLatin1String("__")) ) // Hide variables starting with "__"
        {
            continue;
        }

        // Create variable item
        VariableTreeData item = VariableTreeData::fromScripValue( childName, childValue );

        // Recursively add children
        item.children << variablesFromScriptValue( childValue, maxDepth - 1,
                                                   QList<QScriptValue>() << parents << value );

        // Add variable item to the return list
        variables << item;
    }

    return variables;
}

VariableItem &VariableItem::operator=( const VariableItem &item )
{
    // The model to which the item belongs cannot be changed and shouldn't
    m_parent = item.m_parent;
    m_children = item.m_children;
    m_data = item.m_data;
    return *this;
}

VariableTreeData VariableTreeData::fromScripValue( const QString &name, const QScriptValue &value )
{
    VariableTreeData data;
    if ( name == QLatin1String("helper") || name == QLatin1String("network") ||
         name == QLatin1String("storage") || name == QLatin1String("result") ||
         name == QLatin1String("provider") ||
         name == QLatin1String("PublicTransport") || name == QLatin1String("enum") )
    {
        data.flags |= VariableIsHelperObject;
    }

    bool encodeValue = false;
    QChar endCharacter;
    if ( value.isArray() ) {
        data.description = data.completeValueString = data.valueString =
                QString("[%0]").arg( value.toVariant().toStringList().join(", ") );
        endCharacter = ']';
    } else if ( value.isString() ) {
        data.description = data.completeValueString = value.toString();
        data.valueString = QString("\"%1\"").arg( value.toString() );
        encodeValue = true;
        endCharacter = '\"';
    } else if ( value.isRegExp() ) {
        data.description = data.completeValueString = data.valueString =
                QString("/%1/%2").arg( value.toRegExp().pattern() )
                .arg( value.toRegExp().caseSensitivity() == Qt::CaseSensitive ? "" : "i" );
        encodeValue = true;
    } else if ( value.isFunction() ) {
        data.valueString = QString(); //QString("function %1").arg( name ); // it.value() is the function definition
    } else if ( value.isVariant() && !value.toVariant().toByteArray().isNull() ) {
        const QByteArray hex = value.toVariant().toByteArray().toHex();
        const int rowLength = 32;
        int pos = 0;
        while ( pos < hex.length() ) {
            // Get next part of the byte array
            const QString hexPart = hex.mid( pos, rowLength );
            QString row;
            int i = 0;
            do {
                if ( !row.isEmpty() ) {
                    row += ' ';
                }
                row += hexPart.mid( i, 2 );
                i += 2;
            } while ( i + 2 <= hexPart.length() );

            if ( !data.completeValueString.isEmpty() ) {
                data.description += "<br />";
                data.completeValueString += ' ';
                data.valueString += ' ';
            }
            data.description += QString("<i>%1:</i> %2").arg(pos).arg(row);
            data.completeValueString += row;
            data.valueString += row;
            pos += row.length();
        }
    } else {
        data.description = data.completeValueString = data.valueString = value.toString();
    }

    const int cutPos = qMin( 200, data.valueString.indexOf('\n') );
    if ( cutPos != -1 ) {
        data.valueString = data.valueString.left( cutPos ) + " ...";
    }

    data.name = name;
    data.scriptValue = value;
    data.value = value.toVariant();
    data.description = VariableItem::variableValueTooltip( data.description,
                                                           encodeValue, endCharacter );

    if ( value.isRegExp() ) {
        data.icon = KIcon("code-variable");
        data.type = RegExpVariable;
    } else if ( value.isFunction() ) {
        data.icon = KIcon("code-function");
        data.type = FunctionVariable;
    } else if ( value.isArray() || value.isBool() || value.isBoolean() ||
                value.isDate() || value.isNull() || value.isNumber() ||
                value.isString() || value.isUndefined() )
    {
        data.icon = KIcon("code-variable");
        if ( value.isDate() ) {
            data.type = DateVariable;
        } else if ( value.isNumber() ) {
            data.type = NumberVariable;
        } else if ( value.isNull() || value.isUndefined() ) {
            data.type = NullVariable;
        } else if ( value.isArray() ) {
            data.type = ArrayVariable;
        } else if ( value.isBool() ) {
            data.type = BooleanVariable;
        } else if ( value.isString() ) {
            data.type = StringVariable;
        }
    } else if ( value.isObject() || value.isQMetaObject() ) {
        data.icon = KIcon("code-class");
        data.type = ObjectVariable;
    } else if ( value.isError() ) {
        data.icon = KIcon("dialog-error");
        data.type = ErrorVariable;
    } else {
        data.icon = KIcon("code-context");
    }

    if ( name == QLatin1String("result") ) {
        // Add special items for the "result" script object, which is an exposed ResultObject object
        ResultObject *result = qobject_cast< ResultObject* >( value.toQObject() );
        if ( !result ) {
            data.description = i18nc("@info/plain", "(invalid)");
        } else {
            data.description = i18ncp("@info/plain", "%1 result", "%1 results", result->count());

            VariableTreeData dataItem( SpecialVariable, i18nc("@info/plain", "Data"),
                                    data.description, KIcon("documentinfo") );
            int i = 1;
            QList<Enums::TimetableInformation> shortInfoTypes = QList<Enums::TimetableInformation>()
                    << Enums::Target << Enums::TargetStopName << Enums::DepartureDateTime
                    << Enums::DepartureTime << Enums::StopName;
            foreach ( const TimetableData &timetableData, result->data() ) {
                QString shortInfo;
                foreach ( Enums::TimetableInformation infoType, shortInfoTypes) {
                    if ( timetableData.contains(infoType) ) {
                        shortInfo = timetableData[ infoType ].toString();
                        break;
                    }
                }
                VariableTreeData resultItem( SpecialVariable,
                        i18nc("@info/plain", "Result %1", i), QString("<%1>").arg(shortInfo),
                        KIcon("code-class") );
                for ( TimetableData::ConstIterator it = timetableData.constBegin();
                      it != timetableData.constEnd(); ++it )
                {
                    VariableTreeData timetableInformationItem =
                            VariableTreeData::fromTimetableData( it.key(), it.value() );
                    resultItem.children << timetableInformationItem;
                }
                ++i;
                dataItem.children << resultItem;
            }
            data.children << dataItem;
        }
        data.value = data.description;
        data.valueString = data.description;
    } else if ( name == QLatin1String("network") ) {
        // Add special items for the "network" script object, which is an exposed Network object
        Network *network = qobject_cast< Network* >( value.toQObject() );
        if ( !network ) {
            data.description = i18nc("@info/plain", "(invalid)");
        } else {
            data.description = i18ncp("@info/plain", "%1 request", "%1 requests",
                                      network->runningRequests().count());

            VariableTreeData requestsItem( SpecialVariable,  i18nc("@info/plain", "Running Requests"),
                                        data.description, KIcon("documentinfo") );
            int i = 1;
            foreach ( const QSharedPointer< NetworkRequest > &networkRequest,
                      network->runningRequests() )
            {
                VariableTreeData requestItem( SpecialVariable,
                        i18nc("@info/plain", "Request %1", i), networkRequest->url(),
                        KIcon("code-class") );
                requestsItem.children << requestItem;
                ++i;
            }
            data.children << requestsItem;
        }
        data.value = data.description;
        data.valueString = data.description;
    } else if ( name == QLatin1String("storage") ) {
        // Add special items for the "storage" script object, which is an exposed Storage object
        Storage *storage = qobject_cast< Storage* >( value.toQObject() );
        if ( !storage ) {
            data.description = i18nc("@info/plain", "(invalid)");
        } else {
            const QVariantMap memory = storage->read();
            data.description = i18ncp("@info/plain", "%1 value", "%1 values", memory.count());

            VariableTreeData memoryItem( SpecialVariable,
                    i18nc("@info/plain", "Memory"), data.description, KIcon("documentinfo") );
            int i = 1;
            for ( QVariantMap::ConstIterator it = memory.constBegin(); it != memory.constEnd(); ++it ) {
                VariableTreeData valueItem( SpecialVariable, it.key(), it.value().toString(),
                                            KIcon("code-variable") );
                memoryItem.children << valueItem;
                ++i;
            }
            data.children << memoryItem;
        }
        data.value = data.description;
        data.valueString = data.description;
    } else if ( name == QLatin1String("helper") ) {
        data.description = i18nc("@info/plain", "Offers helper functions to scripts");
        data.value = data.description;
        data.valueString = data.description;
    } else if ( name == QLatin1String("provider") ) {
        data.description = i18nc("@info/plain", "Exposes service provider information to scripts, "
                                 "which got read from the XML file");
        data.value = data.description;
        data.valueString = data.description;
    }

    return data;
}

VariableTreeData VariableTreeData::fromTimetableData( Enums::TimetableInformation info,
                                                      const QVariant &data )
{
    QString subValueString;
    const bool isList = data.isValid() && data.canConvert( QVariant::List );
    if ( isList ) {
        const bool isVehicleTypeList = info == Enums::TypesOfVehicleInJourney ||
                                        info == Enums::RouteTypesOfVehicles;
        const bool isSubJourneyList = info == Enums::RouteSubJourneys;
        const QVariantList list = data.toList();
        QStringList stringList;
        int count = 0;
        for ( int i = 0; i < list.count(); ++i ) {
            QString str;
            if ( isVehicleTypeList ) {
                Enums::VehicleType vehicleType =
                        static_cast<Enums::VehicleType>( list[i].toInt() );
                str = "PublicTransport." + Enums::toString( vehicleType );
            } else if ( isSubJourneyList ) {
                str = i18nc("@info/plain", "%1 sub journeys",
                            list[i].toMap()[Global::timetableInformationToString(Enums::RouteStops)]
                            .toList().count());
            } else {
                str = list[i].toString();
            }
            count += str.length();
            if ( count > 100 ) {
                stringList << "...";
                break;
            }
            stringList << str;
        }
        subValueString = '[' + stringList.join(", ") + ']';
    } else if ( info == Enums::TypeOfVehicle ) {
        Enums::VehicleType vehicleType =
                static_cast<Enums::VehicleType>( data.toInt() );
        subValueString = "PublicTransport." + Enums::toString( vehicleType );
    } else {
        subValueString = data.toString();
    }

    VariableTreeData treeData( SpecialVariable, Global::timetableInformationToString(info),
                               subValueString, KIcon("code-variable") );
    if ( !Global::checkTimetableInformation(info, data) ) {
        treeData.flags |= VariableHasErroneousValue;
    }

    if ( data.canConvert(QVariant::List) ) {
        const QVariantList list = data.toList();
        for ( int i = 0; i < list.count(); ++i ) {
            VariableTreeData listItem( StringVariable,
                    QString::number(i + 1), list[i].toString(), KIcon("code-variable") );
            if ( list[i].isValid() && list[i].canConvert(QVariant::Map) ) {
                const QVariantMap map = list[i].toMap();
                for ( QVariantMap::ConstIterator subIt = map.constBegin();
                        subIt != map.constEnd(); ++subIt )
                {
                    Enums::TimetableInformation subInfo =
                            Global::timetableInformationFromString(subIt.key());
                    VariableTreeData hashItem =
                            VariableTreeData::fromTimetableData(subInfo, subIt.value());
                    if ( info == Enums::RouteSubJourneys &&
                         subInfo != Enums::RouteStops &&
                         subInfo != Enums::RouteStopsShortened &&
                         subInfo != Enums::RouteTimes &&
                         subInfo != Enums::RouteTimesArrival &&
                         subInfo != Enums::RouteTimesDeparture &&
                         subInfo != Enums::RoutePlatformsArrival &&
                         subInfo != Enums::RoutePlatformsDeparture &&
                         subInfo != Enums::RouteExactStops &&
                         subInfo != Enums::RouteNews )
                    {
                        hashItem.flags |= VariableHasErroneousValue;
                    }
                    listItem.children << hashItem;
                }
            }
            treeData.children << listItem;
        }
    }
    return treeData;
}

QString VariableItem::variableValueTooltip( const QString &completeValueString,
                                            bool encodeHtml, const QChar &endCharacter )
{
    if ( completeValueString.isEmpty() ) {
        return QString();
    }

    QString tooltip = completeValueString.left( 1000 );
    if ( encodeHtml ) {
        if ( !endCharacter.isNull() ) {
            tooltip += endCharacter; // Add end character (eg. a quotation mark), which got cut off
        }
        tooltip = Global::encodeHtmlEntities( tooltip );
    }
    if ( tooltip.length() < completeValueString.length() ) {
        tooltip.prepend( i18nc("@info Always plural",
                         "<emphasis strong='1'>First %1 characters:</emphasis><nl />", 1000) )
               .append( QLatin1String("...") );
    }
    return tooltip.prepend( QLatin1String("<p>") ).append( QLatin1String("</p>") );
}

QModelIndex VariableItem::index()
{
    return m_model ? m_model->indexFromVariable(this) : QModelIndex();
}

void VariableItem::addChild( VariableItem *item )
{
    m_model->addChild( this, item );
}

void VariableItem::addChildren( const VariableItemList &items )
{
    m_model->addChildren( this, items );
}

void VariableModel::addChild( VariableItem *parentItem, VariableItem *item )
{
    addChildren( parentItem, VariableItemList() << item );
}

void VariableModel::addChildren( VariableItem *parentItem, const VariableItemList &items )
{
    const QModelIndex index = parentItem->index();
    const int childCount = parentItem->children().count();
    beginInsertRows( index, childCount, childCount + items.count() - 1 );

    foreach ( VariableItem *item, items.variables ) {
        parentItem->childrenPointer()->append( item );
        item->setParent( parentItem );
    }

    endInsertRows();
}

void VariableModel::addChildrenAlreadyLocked( VariableItem *parentItem, const VariableItemList &items )
{
    foreach ( VariableItem *item, items.variables ) {
        parentItem->childrenPointer()->append( item );
        item->setParent( parentItem );
    }
}

bool VariableModel::removeRows( int row, int count, const QModelIndex &parent )
{
    // Remove the frames from the model
    beginRemoveRows( parent, row, row + count - 1 );
    VariableItemList *variables = 0;
    if ( parent.isValid() ) {
        VariableItem *parentItem = variableFromIndex( parent );
        variables = parentItem->childrenPointer();
    } else {
        variables = &m_variableStack[ m_depthIndex ];
    }

    for ( int i = row + count - 1; i >= row; --i ) {
        // Remove frame and delete it
        delete variables->takeAt( i );
    }
    endRemoveRows();
    return true;
}

void VariableModel::clear()
{
    if ( !isIndexValid(m_depthIndex) ) {
        return;
    }

    const int variableCount = m_variableStack.at( m_depthIndex ).count();
    beginRemoveRows( QModelIndex(), 0, variableCount - 1 );
    while ( !m_variableStack.isEmpty() ) {
        qDeleteAll( m_variableStack.pop().variables );
    }
    endRemoveRows();
}

bool VariableModel::isEmpty() const
{
    return isIndexValid(m_depthIndex) ? m_variableStack.at(m_depthIndex).isEmpty() : true;
}

QModelIndex VariableModel::parent( const QModelIndex &child ) const
{
    if ( !child.isValid() ) {
        return QModelIndex();
    }

    VariableItem *childItem = variableFromIndex( child );
    VariableItem *parentItem = childItem->parent();
    if ( !parentItem ) {
        return QModelIndex();
    }

    if ( parentItem->parent() ) {
        // parentItem has a parent, get index of parentItem in it's parent's children
        return createIndex( parentItem->parent()->children().indexOf(parentItem), 0, parentItem );
    } else if ( isIndexValid(m_depthIndex) ) {
        // parentItem has no parent, get index of parentItem in the current top level variable list
        return createIndex( m_variableStack.at(m_depthIndex).indexOf(parentItem), 0, parentItem );
    } else {
        return QModelIndex();
    }
}

bool VariableModel::isIndexValid( int index ) const
{
    return index >= 0 && index < m_variableStack.count();
}

QModelIndex VariableModel::index( int row, int column, const QModelIndex &parent ) const
{
    if ( column < 0 || column >= ColumnCount || row < 0 ) {
        return QModelIndex();
    }

    if ( parent.isValid() ) {
        if ( parent.column() != 0 ) {
            // Only the first column has children
            return QModelIndex();
        }

        VariableItem *parentItem = variableFromIndex( parent );
        return row >= parentItem->children().count() ? QModelIndex()
                : createIndex(row, column, parentItem->children()[row]);
    } else {
        if ( isIndexValid(m_depthIndex) ) {
            const VariableItemList &variables = m_variableStack[ m_depthIndex ];
            return row < 0 || row >= variables.count()
                    ? QModelIndex() : createIndex(row, column, variables[row]);
        } else {
            return QModelIndex();
        }
    }
}

QModelIndex VariableModel::indexFromVariable( VariableItem *variable, int column ) const
{
    return indexFromVariableAlreadyLocked( variable, column );
}

QModelIndex VariableModel::indexFromVariableAlreadyLocked( VariableItem *variable, int column ) const
{
    if ( !variable ) {
        return QModelIndex();
    }
    Q_ASSERT( column >= 0 && column < ColumnCount );

    const VariableItemList *variables;
    if ( variable->parent() ) {
        variables = variable->parent()->childrenPointer();
    } else if ( isIndexValid(m_depthIndex) ) {
        variables = &m_variableStack[ m_depthIndex ];
    } else {
        return QModelIndex();
    }
    return createIndex( variables->indexOf(variable), column, variable );
}

VariableItem *VariableModel::variableFromIndex( const QModelIndex &index ) const
{
    return !index.isValid() ? 0 : static_cast< VariableItem* >( index.internalPointer() );
}

int VariableModel::rowCount( const QModelIndex &parent ) const
{
    if ( !isIndexValid(m_depthIndex) ) {
        // Model is currently at an invalid depth index, ie. -1
        return 0;
    } else if ( parent.isValid() ) {
        // Child item
        if ( parent.column() == 0 ) {
            const VariableItem *parentItem = variableFromIndex( parent );
            return parentItem->children().count();
        } else {
            // Only the first column has children
            return 0;
        }
    } else {
        // Top level item
        return m_variableStack.at( m_depthIndex ).count();
    }
}

bool VariableFilterProxyModel::filterAcceptsRow( int sourceRow,
                                                 const QModelIndex &sourceParent ) const
{
    if ( sourceParent.isValid() &&
         sourceModel()->data(sourceParent).toString().contains(filterRegExp()) )
    {
        return true;
    }

    const QModelIndex sourceIndex = sourceModel()->index( sourceRow, 0, sourceParent );
    const QString data = sourceModel()->data( sourceIndex ).toString();
    bool ret = data.contains( filterRegExp() );

    if ( sourceIndex.isValid() ) {
        for ( int i = 0; i < sourceModel()->rowCount(sourceIndex); ++i ) {
            ret = ret || filterAcceptsRow(i, sourceIndex );
        }
    }
    return ret;
}

} // namespace Debugger
