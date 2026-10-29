#include "StartScreen.h"

#include "AuthService.h"
#include "Device.h"
#include "Theme.h"
#include "Widgets.h"

#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const char *kDisclaimer = "Неофициальный клиент author.today. Не связан с порталом. Книги и оплата — на сайте.";

QLabel *centered(const QString &text, const QFont &font, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setFont(font);
    l->setAlignment(Qt::AlignCenter);
    l->setWordWrap(true);
    return l;
}

QPushButton *bigButton(const QString &icon, const QString &text, bool dark, QWidget *parent)
{
    auto *b = new QPushButton(Theme::icon(icon), text, parent);
    b->setMinimumHeight(Theme::mm(10));
    b->setFont(Theme::bodyFont());
    b->setIconSize(QSize(Theme::mm(4), Theme::mm(4)));
    if (dark)
        b->setProperty("variant", "dark");
    return b;
}

QWidget *page(QWidget *parent, QVBoxLayout **layout)
{
    auto *w = new QWidget(parent);
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(Theme::mm(8), Theme::mm(10), Theme::mm(8), Theme::mm(4));
    v->setSpacing(Theme::mm(2.5));
    *layout = v;
    return w;
}

void addDisclaimer(QVBoxLayout *v, QWidget *parent, const QString &text = QString::fromUtf8(kDisclaimer))
{
    v->addStretch(1);
    QLabel *d = centered(text, Theme::smallFont(), parent);
    d->setProperty("role", "muted");
    v->addWidget(d);
}

} // namespace

QWidget *StartScreen::header(const QString &title)
{
    auto *w = new QWidget(this);
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    // The logo already says "InkToday": a title is only added where the page needs one (login).
    v->addWidget(Ui::logo(title.isEmpty() ? Theme::mm(58) : Theme::mm(36), w), 0, Qt::AlignHCenter);
    if (!title.isEmpty()) {
        v->addSpacing(Theme::mm(3));
        v->addWidget(centered(title, Theme::titleFont(), w));
    }
    return w;
}

StartScreen::StartScreen(StartupFlow *flow, AuthService *auth, QWidget *parent)
    : QWidget(parent)
    , m_flow(flow)
    , m_auth(auth)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_pages = new QStackedWidget(this);
    outer->addWidget(m_pages);

    // 0: progress
    QVBoxLayout *v;
    QWidget *progress = page(m_pages, &v);
    v->addStretch(1);
    v->addWidget(header(QString()));
    QLabel *tag = centered(QStringLiteral("неофициальный клиент author.today"), Theme::smallFont(), progress);
    tag->setProperty("role", "muted");
    v->addWidget(tag);
    v->addSpacing(Theme::mm(6));
    m_progressText = centered(QString(), Theme::bodyFont(), progress);
    v->addWidget(m_progressText);
    m_steps = new QLabel(progress);
    m_steps->setFont(Theme::smallFont());
    m_steps->setAlignment(Qt::AlignHCenter);
    v->addWidget(m_steps);
    addDisclaimer(v, progress);
    m_pages->addWidget(progress);

    // 1: no network / server down
    QWidget *problem = page(m_pages, &v);
    v->addWidget(header(QString()));
    v->addSpacing(Theme::mm(4));
    m_problemTitle = centered(QString(), Theme::titleFont(), problem);
    m_problemText = centered(QString(), Theme::bodyFont(), problem);
    v->addWidget(m_problemTitle);
    v->addWidget(m_problemText);
    v->addSpacing(Theme::mm(4));
    auto *wifi = bigButton(QStringLiteral("wifi"), QStringLiteral("Подключить Wi-Fi"), true, problem);
    auto *retry = bigButton(QStringLiteral("rotate"), QStringLiteral("Повторить"), false, problem);
    auto *offlineBtn = bigButton(QStringLiteral("tablet-screen-button"), QStringLiteral("Без сети: скачанные книги"), false, problem);
    v->addWidget(wifi);
    v->addWidget(retry);
    v->addWidget(offlineBtn);
    addDisclaimer(v, problem, QStringLiteral("«Подключить Wi-Fi» открывает системный выбор сети PocketBook."));
    m_pages->addWidget(problem);
    connect(wifi, &QPushButton::clicked, this, [this] {
        device::openNetworkDialog();
        m_flow->retry();
    });
    connect(retry, &QPushButton::clicked, m_flow, &StartupFlow::retry);
    connect(offlineBtn, &QPushButton::clicked, this, &StartScreen::offline);

    // 2: login
    QWidget *login = page(m_pages, &v);
    v->addWidget(header(QStringLiteral("Вход в author.today")));
    m_login = new QLineEdit(login);
    m_login->setPlaceholderText(QStringLiteral("Логин или e-mail"));
    m_login->setFont(Theme::bodyFont());
    m_password = new QLineEdit(login);
    m_password->setPlaceholderText(QStringLiteral("Пароль"));
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setFont(Theme::bodyFont());
    auto *show = new QPushButton(Theme::icon(QStringLiteral("regular:eye")), QString(), login);
    show->setFixedWidth(Theme::mm(12));
    show->setCheckable(true);
    connect(show, &QPushButton::toggled, this, [this](bool on) { m_password->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });
    auto *pwRow = new QHBoxLayout;
    pwRow->addWidget(m_password, 1);
    pwRow->addWidget(show);
    m_loginError = centered(QString(), Theme::smallFont(), login);
    m_loginError->setStyleSheet(QStringLiteral("border: 1.5px dashed #000; padding: 4px;"));
    m_loginError->hide();
    m_loginButton = bigButton(QStringLiteral("right-to-bracket"), QStringLiteral("Войти"), true, login);
    auto *guest = bigButton(QStringLiteral("user"), QStringLiteral("Продолжить без входа"), false, login);
    auto *forgot = new QPushButton(QStringLiteral("Забыли пароль? Восстановить на сайте"), login);
    forgot->setFlat(true);
    forgot->setFont(Theme::smallFont());
    v->addWidget(m_login);
    v->addLayout(pwRow);
    v->addWidget(m_loginError);
    v->addWidget(m_loginButton);
    v->addWidget(guest);
    v->addWidget(forgot);
    addDisclaimer(v, login, QStringLiteral("Неофициальный клиент author.today. Пароль отправляется только на api.author.today и не сохраняется."));
    m_pages->addWidget(login);
    connect(m_loginButton, &QPushButton::clicked, this, &StartScreen::submitLogin);
    connect(m_password, &QLineEdit::returnPressed, this, &StartScreen::submitLogin);
    connect(guest, &QPushButton::clicked, this, [this] {
        clearFields();
        emit finished();
    });
    connect(forgot, &QPushButton::clicked, this, [this] {
        Ui::showQr(this, QStringLiteral("Восстановление пароля"), QStringLiteral("Откройте на телефоне"),
                   QStringLiteral("https://author.today/account/password/recovery"), QString());
    });

    // 3: e-mail code
    QWidget *code = page(m_pages, &v);
    v->addWidget(header(QStringLiteral("Код подтверждения")));
    m_codeText = centered(QString(), Theme::bodyFont(), code);
    m_code = new QLineEdit(code);
    m_code->setPlaceholderText(QStringLiteral("Код из письма"));
    m_code->setAlignment(Qt::AlignCenter);
    m_code->setFont(Theme::titleFont());
    m_code->setInputMethodHints(Qt::ImhDigitsOnly);
    auto *confirm = bigButton(QStringLiteral("check"), QStringLiteral("Подтвердить"), true, code);
    auto *resend = bigButton(QStringLiteral("paper-plane"), QStringLiteral("Отправить код ещё раз"), false, code);
    auto *back = bigButton(QStringLiteral("arrow-left"), QStringLiteral("Назад"), false, code);
    v->addWidget(m_codeText);
    v->addWidget(m_code);
    v->addWidget(confirm);
    v->addWidget(resend);
    v->addWidget(back);
    addDisclaimer(v, code, QStringLiteral("После подтверждения код больше спрашивать не будут."));
    m_pages->addWidget(code);
    connect(confirm, &QPushButton::clicked, this, [this] {
        sendLogin(m_code->text().trimmed());
    });
    connect(resend, &QPushButton::clicked, this, [this] { sendLogin(QString()); });
    connect(back, &QPushButton::clicked, this, [this] { showLogin(); });

    connect(m_flow, &StartupFlow::stateChanged, this, [this](StartupFlow::State s) {
        switch (s) {
        case StartupFlow::State::NoNetwork:
        case StartupFlow::State::ServerDown:
            showProblem(s);
            break;
        case StartupFlow::State::NeedLogin:
            showLogin();
            break;
        case StartupFlow::State::Ready:
            emit finished();
            break;
        default:
            showProgress(s);
        }
    });
    connect(m_auth, &AuthService::loggedIn, this, [this] {
        clearFields(); // not left in memory and on screen for the next "Войти"
        emit finished();
    });
    connect(m_auth, &AuthService::loginFailed, this, [this](const QString &msg) {
        if (m_pages->currentIndex() == 3)
            m_codeText->setText(msg);
        else
            showLogin(msg);
    });
    connect(m_auth, &AuthService::codeRequired, this, [this](const QString &, const QString &msg) { showCode(msg); });
    showProgress(StartupFlow::State::CheckingNetwork);
}

void StartScreen::showProgress(StartupFlow::State s)
{
    const bool netDone = s == StartupFlow::State::CheckingServer;
    m_progressText->setText(s == StartupFlow::State::CheckingServer ? QStringLiteral("Проверяем author.today…")
                                                                    : QStringLiteral("Подключаемся к Wi-Fi…"));
    m_steps->setText(QStringLiteral("%1  Wi-Fi\n%2  author.today\n○  Вход в аккаунт")
                         .arg(netDone ? QStringLiteral("✓") : QStringLiteral("…"),
                              netDone ? QStringLiteral("…") : QStringLiteral("○")));
    m_pages->setCurrentIndex(0);
    QTimer::singleShot(0, [] { device::requireFullUpdate(); });
}

void StartScreen::showProblem(StartupFlow::State s)
{
    if (s == StartupFlow::State::NoNetwork) {
        m_problemTitle->setText(QStringLiteral("Нет подключения к интернету"));
        m_problemText->setText(QStringLiteral("Wi-Fi выключен или сеть недоступна."));
    } else {
        m_problemTitle->setText(QStringLiteral("author.today недоступен"));
        m_problemText->setText(QStringLiteral("Сайт не отвечает. Попробуйте позже."));
    }
    m_pages->setCurrentIndex(1);
    QTimer::singleShot(0, [] { device::requireFullUpdate(); });
}

void StartScreen::showLogin(const QString &error)
{
    m_loginError->setText(error);
    m_loginError->setVisible(!error.isEmpty());
    m_loginButton->setEnabled(true);
    m_pages->setCurrentIndex(2);
    QTimer::singleShot(0, [] { device::requireFullUpdate(); });
}

void StartScreen::showCode(const QString &message)
{
    m_codeText->setText(message.isEmpty() ? QStringLiteral("author.today отправил код на вашу почту.\nЕсли письма нет — проверьте «Спам».")
                                          : message);
    m_code->clear();
    m_pages->setCurrentIndex(3);
    QTimer::singleShot(0, [] { device::requireFullUpdate(); });
}

void StartScreen::submitLogin()
{
    if (m_login->text().trimmed().isEmpty() || m_password->text().isEmpty()) {
        showLogin(QStringLiteral("Введите логин и пароль."));
        return;
    }
    m_loginError->hide();
    m_loginButton->setEnabled(false);
    sendLogin(QString());
}

void StartScreen::sendLogin(const QString &code)
{
    auto send = [this, code] { m_auth->login(m_login->text().trimmed(), m_password->text(), code); };
    if (!m_gate) {
        send();
        return;
    }
    QPointer<StartScreen> self(this);
    m_gate([self, send](bool online) {
        if (!self)
            return;
        if (online) {
            send();
            return;
        }
        self->m_loginButton->setEnabled(true);
        self->showLogin(QStringLiteral("Нет подключения к интернету"));
    });
}

void StartScreen::clearFields()
{
    m_password->clear();
    m_code->clear();
}
