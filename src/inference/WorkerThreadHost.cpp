#include "WorkerThreadHost.h"

#include <QThread>

namespace LAStudio {

WorkerThreadHost::WorkerThreadHost(QObject *parent)
    : QObject(parent)
{
}

WorkerThreadHost::~WorkerThreadHost()
{
    stop();
}

bool WorkerThreadHost::start(QObject *worker)
{
    if (m_thread || !worker) return false;
    m_thread = new QThread(this);
    m_worker = worker;
    worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, worker, &QObject::deleteLater);
    m_thread->start();
    return true;
}

void WorkerThreadHost::stop()
{
    if (!m_thread) return;
    m_thread->quit();
    m_thread->wait();
    m_thread = nullptr;
    m_worker = nullptr;
}

} // namespace LAStudio
