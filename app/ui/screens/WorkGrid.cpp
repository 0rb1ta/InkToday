#include "WorkGrid.h"

#include "AppContext.h"
#include "PageMath.h"
#include "Theme.h"
#include "Widgets.h"
#include "WorkList.h"

#include <QHBoxLayout>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const int kPerPage = 8;
}

WorkGrid::WorkGrid(AppContext *ctx, QWidget *parent)
    : QWidget(parent)
    , m_ctx(ctx)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_pager = new Pager(4, 2, this);
    m_pager->setFactory([this](int i) -> QWidget * {
        if (!m_list || i >= m_list->works().size())
            return nullptr;
        const QList<WorkMeta> &works = m_list->works();
        const auto range = PageMath::slice(m_pager->page(), works.size(), kPerPage);
        if (i == range.first) {
            m_ctx->requestStatus(works.mid(range.first, range.second - range.first));
            // Last loaded screen on display: fetch the next one now (deferred: we are inside a rebuild).
            if (range.second >= works.size() && !m_list->atEnd()) {
                const int want = (m_pager->page() + 2) * kPerPage;
                QPointer<WorkList> list = m_list;
                QTimer::singleShot(0, this, [list, want] {
                    if (list)
                        list->ensure(want);
                });
            }
        }
        return m_ctx->makeTile(works[i], nullptr, QString(), m_showAuthor);
    });
    v->addWidget(m_pager, 1);

    auto *row = new QHBoxLayout;
    m_status = new QLabel(this);
    m_status->setFont(Theme::smallFont());
    m_status->setProperty("role", "muted");
    m_retry = new QPushButton(Theme::icon(QStringLiteral("rotate")), QStringLiteral("Повторить"), this);
    m_retry->setFont(Theme::smallFont());
    m_retry->setFocusPolicy(Qt::NoFocus);
    connect(m_retry, &QPushButton::clicked, this, [this] {
        if (m_list)
            m_list->retry();
    });
    row->addWidget(m_status, 1);
    row->addWidget(m_retry);
    v->addLayout(row);
    refresh();
}

void WorkGrid::setList(WorkList *list)
{
    if (m_list)
        disconnect(m_list, nullptr, this, nullptr);
    m_list = list;
    m_shown = -1;
    m_emptyShown.clear();
    if (!m_list)
        return; // a new list follows right away (reload, new search): build the screen once, then
    connect(m_list, &WorkList::changed, this, &WorkGrid::refresh);
    m_list->ensure(2 * kPerPage); // the first screen and the next one
    refresh();
}

void WorkGrid::flip(int direction)
{
    m_pager->flip(direction);
}

void WorkGrid::refresh()
{
    const int count = m_list ? m_list->works().size() : 0;
    QString empty;
    if (!m_list)
        empty = QString(); // nothing asked yet (search before the first query)
    else if (m_list->loading())
        empty = QStringLiteral("Загрузка…");
    else if (!m_list->error().isEmpty())
        empty = m_list->error();
    else if (m_list->unrecognized())
        empty = QStringLiteral("Поиск временно не работает: сайт ответил не так, как ожидалось. Попробуйте поиск по тегу.");
    else
        empty = QStringLiteral("Ничего не найдено.");
    // Rebuild only when the tiles or the empty text change: every rebuild redraws the e-ink screen.
    if (count != m_shown || (count == 0 && empty != m_emptyShown)) {
        // A page loaded in the background usually leaves the visible tiles as they are: then only
        // the footer changes, and the screen is not redrawn.
        const bool sameTiles = m_shown > 0 && count > 0
            && PageMath::slice(m_pager->page(), m_shown, kPerPage) == PageMath::slice(m_pager->page(), count, kPerPage);
        m_shown = count;
        m_emptyShown = empty;
        m_pager->setEmptyText(empty);
        if (sameTiles)
            m_pager->setCountQuiet(count);
        else
            m_pager->setCount(count);
    }
    const bool failed = m_list && !m_list->error().isEmpty();
    m_retry->setVisible(failed);
    if (count == 0)
        m_status->clear();
    else if (failed)
        m_status->setText(QStringLiteral("Не удалось загрузить продолжение."));
    else if (m_list->unrecognized())
        m_status->setText(QStringLiteral("Дальше сайт ответил не так, как ожидалось: поиск временно не работает."));
    else if (m_list->loading())
        m_status->setText(QStringLiteral("Загрузка…"));
    else
        m_status->clear();
}
