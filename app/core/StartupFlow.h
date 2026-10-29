#pragma once

#include <QObject>

#include <functional>

class QTimer;

// App start: network → server → login or home.
// All side effects are injected so the sequence is testable without a device.
class StartupFlow : public QObject {
    Q_OBJECT
public:
    enum class State { CheckingNetwork, ConnectingNetwork, CheckingServer, NoNetwork, ServerDown, NeedLogin, Ready };
    Q_ENUM(State)

    struct Deps {
        std::function<bool()> networkConnected;
        std::function<void(std::function<void(bool)>)> connectNetwork;
        std::function<void(std::function<void(bool)>)> pingServer;
        std::function<bool()> hasSession; // a token is stored (its time may have run out)
        // Renews the stored token before choosing Ready or NeedLogin; unset: hasSession decides.
        std::function<void(std::function<void(bool)>)> refreshSession;
        int connectTimeoutMs = 20000;
        // Right after Wi-Fi connects DNS may not answer yet: retry the server check first.
        int serverRetries = 2;
        int serverRetryDelayMs = 3000;
    };

    explicit StartupFlow(Deps deps, QObject *parent = nullptr);

    void start();
    void retry(); // from NoNetwork / ServerDown
    State state() const { return m_state; }

signals:
    void stateChanged(StartupFlow::State state);

private:
    void setState(State s);
    void checkServer(int retriesLeft);

    Deps m_deps;
    State m_state = State::CheckingNetwork;
    int m_attempt = 0; // late callbacks from an older attempt are ignored
    QTimer *m_timeout;
};
