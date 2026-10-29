#pragma once

#include "StartupFlow.h"

#include <QWidget>

#include <functional>

class AuthService;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

// Start screen: network/server progress, no network, login, e-mail code.
class StartScreen : public QWidget {
    Q_OBJECT
public:
    StartScreen(StartupFlow *flow, AuthService *auth, QWidget *parent = nullptr);
    void requestLogin() { showLogin(); } // from "Скачать" as a guest or from settings
    void clearFields();                   // password and code: leaving the login page
    // Login requests bring Wi-Fi up first (the firmware turns it off on its own).
    void setNetworkGate(std::function<void(std::function<void(bool)>)> gate) { m_gate = std::move(gate); }

signals:
    void finished();      // logged in or continuing as guest
    void offline();       // "Без сети: скачанные книги"

private:
    void sendLogin(const QString &code);
    std::function<void(std::function<void(bool)>)> m_gate;
    void showProgress(StartupFlow::State state);
    void showProblem(StartupFlow::State state);
    void showLogin(const QString &error = {});
    void showCode(const QString &message);
    void submitLogin();
    QWidget *header(const QString &title);

    StartupFlow *m_flow;
    AuthService *m_auth;
    QStackedWidget *m_pages;
    QLabel *m_progressText;
    QLabel *m_steps;
    QLabel *m_problemTitle;
    QLabel *m_problemText;
    QLineEdit *m_login;
    QLineEdit *m_password;
    QLabel *m_loginError;
    QLineEdit *m_code;
    QLabel *m_codeText;
    QPushButton *m_loginButton;
};
