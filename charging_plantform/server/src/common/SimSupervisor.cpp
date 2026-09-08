#include "SimSupervisor.h"

#include <QProcess>
#include <QTimer>
#include <QDebug>
#include <QtGlobal>

SimSupervisor::SimSupervisor(QObject* parent) : QObject(parent) {}

void SimSupervisor::configure(const QString& bin, const QString& host, quint16 port,
                              const QString& deviceId, int pileCount) {
    m_bin = bin;
    m_host = host;
    m_port = port;
    m_deviceId = deviceId;
    m_piles = qBound(0, pileCount, 200);
}

void SimSupervisor::start() {
    if (m_started || m_piles <= 0 || m_bin.isEmpty()) return;
    m_started = true;
    launch();
}

void SimSupervisor::stop() {
    m_shutting = true;
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        if (!m_proc->waitForFinished(1500)) m_proc->kill();
    }
    if (m_proc) m_proc->deleteLater();
    m_proc = nullptr;
}

void SimSupervisor::launch() {
    if (m_shutting || m_piles <= 0 || m_bin.isEmpty()) return;

    auto* proc = new QProcess(this);
    proc->setProgram(m_bin);
    proc->setArguments(QStringList{
        m_host,
        QString::number(m_port),
        m_deviceId,
        QString::number(m_piles),
    });
    // 子进程（桩模拟终端）日志直接透传到 server 控制台，便于看到“绑定 N 台”
    proc->setProcessChannelMode(QProcess::ForwardedChannels);
    m_proc = proc;

    connect(proc, &QProcess::errorOccurred, this,
            [this, proc](QProcess::ProcessError err) {
                if (err == QProcess::FailedToStart) {
                    qWarning().noquote()
                        << QStringLiteral("[托管] 无法启动模拟终端：%1")
                               .arg(m_bin);
                    if (proc == m_proc) m_proc = nullptr;
                    proc->deleteLater();
                }
            });
    connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, proc](int, QProcess::ExitStatus) {
                if (proc != m_proc) return;
                m_proc = nullptr;
                if (m_shutting) return;
                qInfo().noquote()
                    << QStringLiteral("[托管] 模拟终端已退出，2s 后自动重启…");
                QTimer::singleShot(2000, this, [this]() { launch(); });
            });

    qInfo().noquote()
        << QStringLiteral("[托管] 启动模拟终端：%1  %2:%3  device=%4  piles=%5")
               .arg(m_bin, m_host)
               .arg(m_port)
               .arg(m_deviceId)
               .arg(m_piles);
    proc->start();
}
