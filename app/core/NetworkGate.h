#pragma once

#include <QList>

#include <functional>
#include <memory>

// Brings Wi-Fi up before a network call. The firmware turns Wi-Fi off on its own and may never
// answer a connection attempt, so an attempt that hangs longer than the timeout fails everyone
// waiting for it, and the next call starts a fresh one. Copies share one state.
class NetworkGate {
public:
    using Connect = std::function<void(std::function<void(bool ok)> done)>;
    NetworkGate(std::function<bool()> connected, Connect connect, int timeoutMs = 30000);

    // `done` runs exactly once: true when the network is (or came) up.
    void run(std::function<void(bool ok)> done) const;

private:
    struct State {
        std::function<bool()> connected;
        Connect connect;
        int timeoutMs = 0;
        QList<std::function<void(bool)>> waiters;
        int attempt = 0;
        bool pending = false;
    };
    static void finish(const std::shared_ptr<State> &s, int attempt, bool ok);

    std::shared_ptr<State> m_state;
};
