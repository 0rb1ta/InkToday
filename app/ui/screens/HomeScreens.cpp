#include "HomeScreens.h"

#include "AppContext.h"
#include "AuthService.h"
#include "DownloadIndex.h"
#include "HomeFeed.h"
#include "LibraryStore.h"
#include "PageMath.h"
#include "Theme.h"
#include "UpdateCheck.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

const int kTilesPerRow = 4;

QWidget *rowHeader(const QString &title, const QString &more, QWidget *parent, std::function<void()> onMore)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto *t = new QLabel(title.toUpper(), w);
    t->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QFont f = Theme::smallFont();
    f.setBold(true);
    t->setFont(f);
    h->addWidget(t, 1);
    if (!more.isEmpty()) {
        auto *b = new QPushButton(more, w);
        b->setFlat(true);
        b->setFont(Theme::smallFont());
        b->setStyleSheet(QStringLiteral("border: none;"));
        QObject::connect(b, &QPushButton::clicked, w, [onMore] { onMore(); });
        h->addWidget(b);
    }
    return w;
}

} // namespace

HomeScreen::HomeScreen(AppContext *ctx, QWidget *parent)
    : Screen(parent)
    , m_ctx(ctx)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto *top = new TopBar(QStringLiteral("InkToday"), false, this);
    // Shown only when GitHub has a newer release; opens a QR with the release page.
    QPushButton *update = top->addIconButton(QStringLiteral("circle-up"));
    update->setVisible(m_ctx->updates->available());
    connect(m_ctx->updates, &UpdateCheck::found, update, [update] { update->setVisible(true); });
    connect(update, &QPushButton::clicked, this, [this] {
        Ui::showQr(this, QStringLiteral("Вышла InkToday %1").arg(m_ctx->updates->latestVersion()),
                   QStringLiteral("У вас версия %1").arg(QStringLiteral(APP_VERSION)), m_ctx->updates->releaseUrl(),
                   QStringLiteral("Откройте страницу релиза на телефоне или компьютере, скачайте архив и скопируйте его "
                                  "на читалку по USB поверх старых файлов. Вход и скачанные книги сохранятся."));
    });
    m_ctx->addHeaderButtons(top);
    v->addWidget(top);
    m_pager = new Pager(1, 3, this);
    m_pager->setContentsMargins(Theme::mm(2), Theme::mm(1), Theme::mm(2), 0);
    m_pager->setFactory([this](int i) { return m_rows.value(i) ? m_rows[i]() : nullptr; });
    v->addWidget(m_pager, 1);
    connect(m_ctx, &AppContext::libraryChanged, this, &HomeScreen::rebuild);
    rebuild();
}

QList<WorkMeta> HomeScreen::libraryPreview() const
{
    // Downloaded books with news first, then the library in site order.
    QList<WorkMeta> works = m_ctx->library->works();
    std::stable_sort(works.begin(), works.end(), [this](const WorkMeta &a, const WorkMeta &b) {
        return !m_ctx->badgeFor(a).isEmpty() && m_ctx->badgeFor(b).isEmpty();
    });
    return works.mid(0, kTilesPerRow);
}

QWidget *HomeScreen::libraryRow()
{
    auto *box = new QFrame;
    box->setProperty("frame", "box");
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(Theme::mm(1.5), Theme::mm(1), Theme::mm(1.5), Theme::mm(1.5));
    const int total = m_ctx->library->works().size();
    v->addWidget(rowHeader(QStringLiteral("Моя библиотека"), total ? QStringLiteral("все %1 ›").arg(total) : QString(), box,
                           [this] { emit openLibraryTab(); }));
    if (!m_ctx->auth->isLoggedIn() && total == 0) {
        auto *l = new QLabel(QStringLiteral("Войдите, чтобы видеть свою библиотеку."), box);
        l->setFont(Theme::bodyFont());
        l->setAlignment(Qt::AlignCenter);
        v->addWidget(l, 1);
        return box;
    }
    if (total == 0) {
        auto *l = new QLabel(QStringLiteral("В библиотеке пока нет книг. Добавьте их на сайте или из каталога."), box);
        l->setFont(Theme::bodyFont());
        l->setWordWrap(true);
        l->setAlignment(Qt::AlignCenter);
        v->addWidget(l, 1);
        return box;
    }
    auto *h = new QHBoxLayout;
    h->setSpacing(Theme::mm(1.5));
    const QList<WorkMeta> preview = libraryPreview();
    for (const WorkMeta &w : preview)
        h->addWidget(m_ctx->makeTile(w, box), 1);
    for (int i = preview.size(); i < kTilesPerRow; ++i)
        h->addStretch(1);
    v->addLayout(h, 1);
    m_ctx->requestStatus(preview);
    return box;
}

QWidget *HomeScreen::shelfRow(const QString &title, const QList<WorkMeta> &works)
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, Theme::mm(1), 0, 0);
    v->addWidget(rowHeader(title, QStringLiteral("все ›"), w, [this, title, works] { emit openShelf(title, works); }));
    auto *h = new QHBoxLayout;
    h->setSpacing(Theme::mm(1.5));
    const QList<WorkMeta> preview = works.mid(0, kTilesPerRow);
    for (const WorkMeta &work : preview)
        h->addWidget(m_ctx->makeTile(work, w), 1);
    if (preview.isEmpty()) {
        auto *l = new QLabel(QStringLiteral("Подборка пока не загружена."), w);
        l->setAlignment(Qt::AlignCenter);
        h->addWidget(l, 1);
    }
    v->addLayout(h, 1);
    m_ctx->requestStatus(preview);
    return w;
}

void HomeScreen::rebuild()
{
    m_rows.clear();
    m_rows << [this] { return libraryRow(); };
    for (const Shelf &s : m_ctx->feed->shelves()) {
        const Shelf copy = s;
        m_rows << [this, copy] { return shelfRow(copy.title, copy.works); };
    }
    m_pager->setCount(m_rows.size());
}

ShelfScreen::ShelfScreen(AppContext *ctx, const QString &title, const QList<WorkMeta> &works, QWidget *parent)
    : Screen(parent)
    , m_ctx(ctx)
    , m_works(works)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto *top = new TopBar(title, true, this);
    ctx->addHeaderButtons(top);
    connect(top, &TopBar::back, this, &ShelfScreen::back);
    v->addWidget(top);
    m_pager = new Pager(4, 2, this);
    m_pager->setContentsMargins(Theme::mm(2), Theme::mm(2), Theme::mm(2), 0);
    m_pager->setEmptyText(QStringLiteral("Здесь пока нет книг."));
    m_pager->setFactory([this](int i) {
        const auto range = PageMath::slice(m_pager->page(), m_works.size(), 8);
        if (i == range.first)
            m_ctx->requestStatus(m_works.mid(range.first, range.second - range.first));
        return m_ctx->makeTile(m_works[i], nullptr, QString(), true);
    });
    v->addWidget(m_pager, 1);
    m_pager->setCount(m_works.size());
}

