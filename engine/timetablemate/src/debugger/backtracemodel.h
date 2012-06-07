
#ifndef BACKTRACEMODEL_H
#define BACKTRACEMODEL_H

#include <QAbstractListModel>

// Own includes
#include "debuggerstructures.h"

class QMutex;
class Project;

namespace Debugger {

enum BacktraceChangeType {
    NoOpBacktraceChange = 0,

    PushBacktraceFrame,
    PopBacktraceFrame,
    UpdateBacktraceFrame
};

struct BacktraceChange {
    BacktraceChange( BacktraceChangeType type = NoOpBacktraceChange ) : type(type) {};
    BacktraceChange( BacktraceChangeType type, const Frame &frame )
            : type(type), frame(frame) {};

    BacktraceChangeType type;
    Frame frame;
};

/**
 * @brief A model for backtraces.
 *
 * Each Debugger uses a BacktraceModel to store the current backtrace. It gets updated through
 * queued connections.
 **/
class BacktraceModel : public QAbstractListModel
{
    Q_OBJECT
    friend class Frame;

public:
    enum Column {
        DepthColumn = 0,
        FunctionColumn,
        SourceColumn,

        ColumnCount
    };

    BacktraceModel( QObject *parent = 0 );
    virtual ~BacktraceModel();

    virtual int columnCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual QModelIndex index( int row, int column,
                               const QModelIndex &parent = QModelIndex() ) const;
    virtual QVariant headerData( int section, Qt::Orientation orientation,
                                 int role = Qt::DisplayRole ) const;
    virtual QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const;
    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;
    virtual bool removeRows( int row, int count, const QModelIndex &parent = QModelIndex() );
    void clear();
    bool isEmpty();

    QModelIndex indexFromRow( int row ) const;
    QModelIndex indexFromFrame( Frame *frame ) const;

    Frame *frameFromRow( int row ) const;
    Frame *frameFromIndex( const QModelIndex &index ) const;

    /** @brief Get the complete frame stack, ie. backtrace. */
    FrameStack frameStack() const;

    /** @brief Get the top frame of the stack of frames. */
    Frame *topFrame();

public slots:
    void applyChange( const BacktraceChange &change );

    /** @brief Push @p frame to the stack of frames in this model. */
    void pushFrame( Frame *frame );

    /** @brief Remove a frame from the top of the stack of frames. */
    void popFrame();

protected slots:
//     void backtraceChanged( const FrameStack &frameStack );
    void frameChanged( Frame *frame );

private:
    inline int rowToIndex( int row ) const { return m_frames.count() - 1 - row; };

    FrameStack m_frames;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
