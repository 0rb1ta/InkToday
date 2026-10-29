#include "StartupFlow.h"

#include "Log.h"

#include <QTimer>

StartupFlow::StartupFlow(Deps deps, QObject *parent)
    : QObject(parent)
    , m_deps(std::move(deps))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        if (m_state != State::ConnectingNetwork)
            return;
        ++m_attempt; // the connect callback, if it ever comes, belongs to a finished attempt
        Log::write(QStringLiteral("startup: no network after %1 ms").arg(m_deps.connectTimeoutMs));
        setState(State::NoNetwork);
    });
}

void StartupFlow::setState(State s)
{
    m_state = s;
    emit stateChanged(s);
}

void StartupFlow::start()
{
    const int attempt = ++m_attempt;
    setState(State::CheckingNetwork);
    if (m_deps.networkConnected()) {
        checkServer(m_deps.serverRetries);
        return;
    }
    setState(State::ConnectingNetwork);
    m_timeout->start(m_deps.connectTimeoutMs);
    m_deps.connectNetwork([this, attempt](bool ok) {
        if (attempt != m_attempt || m_state != State::ConnectingNetwork)
            return;
        m_timeout->stop();
        if (ok)
            checkServer(m_deps.serverRetries);
        else
            setState(State::NoNetwork);
    });
}

void StartupFlow::retry()
{
    start();
}

void StartupFlow::checkServer(int retriesLeft)
{
    const int attempt = m_attempt;
    if (m_state != State::CheckingServer)
        setState(State::CheckingServer);
    m_deps.pingServer([this, attempt, retriesLeft](bool ok) {
        if (attempt != m_attempt)
            return;
        if (!ok && retriesLeft > 0) {
            QTimer::singleShot(m_deps.serverRetryDelayMs, this, [this, attempt, retriesLeft] {
                if (attempt == m_attempt)
                    checkServer(retriesLeft - 1);
            });
            return;
        }
        if (!ok) {
            setState(State::ServerDown);
        } else if (!m_deps.hasSession()) {
            setState(State::NeedLogin);
        } else if (!m_deps.refreshSession) {
            setState(State::Ready);
        } else {
            m_deps.refreshSession([this, attempt](bool fresh) {
                if (attempt == m_attempt)
                    setState(fresh ? State::Ready : State::NeedLogin);
            });
        }
    });
}
