#include "timetablebackend.h"

TimetableBackend::TimetableBackend(QObject *parent)
    : QObject(parent), m_helper()
{
}

TimetableBackend::~TimetableBackend()
{
}

void TimetableBackend::ghnsDialogRequested()
{
    m_helper->showDialog();
}