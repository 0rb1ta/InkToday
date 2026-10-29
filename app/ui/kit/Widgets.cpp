#include "Widgets.h"

#include "Log.h"

#include "CoverCache.h"
#include "Device.h"
#include "PageMath.h"
#include "Qr.h"
#include "Theme.h"

#include <QGridLayout>
#include <QGuiApplication>
#include <QInputMethod>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

const int kFullRefreshEvery = 6;

void setVariant(QWidget *w, const char *variant)
{
    w->setProperty("variant", QString::fromLatin1(variant));
    w->style()->unpolish(w);
    w->style()->polish(w);
}

QPixmap placeholderCover(const QSize &size)
{
    QPixmap pm(size);
    pm.fill(QColor(0xe6, 0xe6, 0xe6));
    QPainter p(&pm);
    p.setPen(QPen(QColor(0x55, 0x55, 0x55), 1));
    p.drawRect(pm.rect().adjusted(0, 0, -1, -1));
    return pm;
}

// By name, not by enum position: a state added in the middle must not shift the icons.
QString actionIcon(ActionState state)
{
    switch (state) {
    case ActionState::Read: return QStringLiteral("book-open");
    case ActionState::Download: return QStringLiteral("download");
    case ActionState::Update: return QStringLiteral("rotate");
    case ActionState::Buy: return QStringLiteral("qrcode");
    case ActionState::SiteOnly: return QStringLiteral("lock");
    case ActionState::Subscribers: return QStringLiteral("user");
    case ActionState::Unknown: return QStringLiteral("clock");
    }
    return QStringLiteral("clock");
}

} // namespace

// ---------- ElidedButton ----------

ElidedButton::ElidedButton(const QString &text, QWidget *parent)
    : QPushButton(parent)
    , m_full(text)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    relayout();
}

void ElidedButton::setFullText(const QString &text)
{
    m_full = text;
    relayout();
}

void ElidedButton::resizeEvent(QResizeEvent *event)
{
    QPushButton::resizeEvent(event);
    relayout();
}

void ElidedButton::relayout()
{
    // Room for the text: the width minus the style's side padding (about the font's height).
    const QFontMetrics fm(font());
    const int room = width() - fm.height() - (icon().isNull() ? 0 : iconSize().width() + 4);
    const QString shown = room > fm.height() ? fm.elidedText(m_full, Qt::ElideRight, room) : m_full;
    if (text() != shown)
        setText(shown);
    setToolTip(m_full);
}

// ---------- ActionButton ----------

ActionButton::ActionButton(QWidget *parent)
    : QPushButton(parent)
{
    setMinimumHeight(Theme::mm(8));
    setFont(Theme::smallFont());
    setIconSize(QSize(Theme::mm(3), Theme::mm(3)));
    // Ignore the text width: four tiles must fit on the narrowest screen; the label is clipped instead.
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setMinimumWidth(Theme::mm(12));
    setState(ActionState::Unknown);
}

void ActionButton::setState(ActionState state)
{
    m_state = state;
    setText(actionText(state));
    setIcon(Theme::icon(actionIcon(state)));
    // "Подписчикам" looks like "На сайте" (dashed) but opens the way to subscribe.
    setVariant(this, state == ActionState::Read ? "dark"
                     : state == ActionState::SiteOnly || state == ActionState::Subscribers ? "muted" : "normal");
}

// ---------- ElidedLabel ----------

ElidedLabel::ElidedLabel(int maxLines, QWidget *parent)
    : QLabel(parent)
    , m_maxLines(maxLines)
{
    setWordWrap(true);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setMinimumWidth(1);
}

void ElidedLabel::setFullText(const QString &text)
{
    m_full = text;
    relayout();
}

void ElidedLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    relayout();
}

void ElidedLabel::relayout()
{
    const QFontMetrics fm(font());
    const int width = qMax(10, contentsRect().width());
    QStringList lines;
    QString rest = m_full.simplified();
    while (!rest.isEmpty() && lines.size() < m_maxLines) {
        if (lines.size() == m_maxLines - 1 || fm.horizontalAdvance(rest) <= width) {
            lines << fm.elidedText(rest, Qt::ElideRight, width);
            break;
        }
        // Longest prefix that fits, broken at a space when possible.
        int cut = rest.size();
        while (cut > 1 && fm.horizontalAdvance(rest.left(cut)) > width)
            --cut;
        const int space = rest.left(cut).lastIndexOf(QLatin1Char(' '));
        if (space > 0)
            cut = space;
        lines << rest.left(cut).trimmed();
        rest = rest.mid(cut).trimmed();
    }
    setText(lines.join(QLatin1Char('\n')));
    setFixedHeight(fm.lineSpacing() * (m_reserveAll ? m_maxLines : qMax(1, lines.size())) + 2);
    setToolTip(m_full);
}

// ---------- Tile ----------

Tile::Tile(CoverCache *covers, bool loadCovers, QWidget *parent)
    : QWidget(parent)
    , m_covers(covers)
    , m_loadCovers(loadCovers)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(Theme::mm(0.8));
    m_cover = new QLabel(this);
    m_cover->setAlignment(Qt::AlignCenter);
    m_cover->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_cover->setMinimumHeight(Theme::mm(18));
    m_badge = new QLabel(m_cover);
    m_badge->setProperty("role", "badge");
    m_badge->setFont(Theme::smallFont());
    m_badge->hide();
    m_title = new ElidedLabel(2, this);
    m_title->setFont(Theme::smallFont());
    m_author = new ElidedLabel(1, this);
    QFont authorFont = Theme::smallFont();
    authorFont.setWeight(QFont::Normal);
    m_author->setFont(authorFont);
    m_author->setProperty("role", "muted");
    m_author->hide();
    m_button = new ActionButton(this);
    v->addWidget(m_cover, 1);
    v->addWidget(m_title);
    v->addWidget(m_author);
    v->addWidget(m_button);
    connect(m_button, &QPushButton::clicked, this, [this] { emit actionClicked(m_id, m_button->state()); });
}

void Tile::setWork(const WorkMeta &work, ActionState state, const QString &badge)
{
    m_id = work.id;
    m_title->setFullText(work.title);
    m_author->setFullText(work.authors);
    m_button->setState(state);
    m_badge->setText(badge);
    m_badge->setVisible(!badge.isEmpty());
    m_badge->adjustSize();
    m_image = QImage();
    m_cover->setPixmap(placeholderCover(m_cover->size().isEmpty() ? QSize(60, 80) : m_cover->size()));
    if (m_loadCovers && m_covers && !work.coverUrl.isEmpty()) {
        QPointer<Tile> self(this);
        const qint64 id = work.id;
        m_covers->get(work.coverUrl, [self, id](const QImage &img) {
            if (!self || self->m_id != id || img.isNull())
                return;
            self->m_image = img;
            self->resizeEvent(nullptr);
        });
    }
}

void Tile::setShowAuthor(bool on)
{
    m_author->setVisible(on);
}

void Tile::resizeEvent(QResizeEvent *event)
{
    if (event)
        QWidget::resizeEvent(event);
    const QSize box = m_cover->size();
    if (box.isEmpty())
        return;
    QPixmap pm = m_image.isNull() ? placeholderCover(QSize(box.height() * 3 / 4, box.height()).boundedTo(box))
                                  : QPixmap::fromImage(m_image.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_cover->setPixmap(pm);
    // Pin the badge to the image's top-right corner (it used to sit at the label edge, off the cover).
    m_badge->adjustSize();
    const int imageLeft = (box.width() - pm.width()) / 2;
    const int imageTop = (box.height() - pm.height()) / 2;
    m_badge->move(qMax(0, imageLeft + pm.width() - m_badge->width() - 3), imageTop + 3);
    m_badge->raise();
}

void Tile::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept(); // a tap on a tile opens the book; it must not also flip the page
    if (m_id)
        emit opened(m_id);
}

// ---------- Pager ----------

Pager::Pager(int cols, int rows, QWidget *parent)
    : QWidget(parent)
    , m_cols(cols)
    , m_rows(rows)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_grid = new QWidget(this);
    m_footer = new QLabel(this);
    m_footer->setAlignment(Qt::AlignCenter);
    m_footer->setFont(Theme::smallFont());
    v->addWidget(m_grid, 1);
    v->addWidget(m_footer);
}

void Pager::setCount(int count)
{
    m_count = count;
    m_page = PageMath::clampPage(m_page, m_count, m_cols * m_rows);
    rebuild();
}

void Pager::setCountQuiet(int count)
{
    m_count = count;
    const int pages = PageMath::pageCount(m_count, m_cols * m_rows);
    m_footer->setText(pages > 1 ? QStringLiteral("◀  стр. %1 из %2  ▶").arg(m_page + 1).arg(pages) : QString());
}

void Pager::flip(int direction)
{
    const int target = PageMath::clampPage(m_page + direction, m_count, m_cols * m_rows);
    if (target == m_page)
        return;
    m_page = target;
    rebuild();
    if (++m_flips % kFullRefreshEvery == 0)
        QTimer::singleShot(0, [] { device::requireFullUpdate(); });
}

void Pager::rebuild()
{
    // Deferred deletion: rebuild can run from a tap that a child is still handling.
    delete m_grid->layout();
    for (QWidget *child : m_grid->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
        child->hide();
        child->setParent(nullptr);
        child->deleteLater();
    }
    auto *g = new QGridLayout(m_grid);
    g->setContentsMargins(0, 0, 0, 0);
    g->setSpacing(Theme::mm(1.5));
    const int perPage = m_cols * m_rows;
    if (m_count == 0) {
        auto *empty = new QLabel(m_emptyText, m_grid);
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        empty->setFont(Theme::bodyFont());
        g->addWidget(empty, 0, 0);
        m_footer->clear();
        return;
    }
    const auto range = PageMath::slice(m_page, m_count, perPage);
    for (int i = range.first; i < range.second; ++i) {
        const int pos = i - range.first;
        if (QWidget *w = m_factory ? m_factory(i) : nullptr) {
            w->setParent(m_grid);
            g->addWidget(w, pos / m_cols, pos % m_cols);
        }
    }
    for (int r = 0; r < m_rows; ++r)
        g->setRowStretch(r, 1);
    for (int c = 0; c < m_cols; ++c)
        g->setColumnStretch(c, 1);
    const int pages = PageMath::pageCount(m_count, perPage);
    m_footer->setText(pages > 1 ? QStringLiteral("◀  стр. %1 из %2  ▶").arg(m_page + 1).arg(pages) : QString());
}

void Pager::mouseReleaseEvent(QMouseEvent *event)
{
    const int edge = width() * 15 / 100;
    if (event->pos().x() < edge)
        flip(-1);
    else if (event->pos().x() > width() - edge)
        flip(+1);
    else
        QWidget::mouseReleaseEvent(event);
}

// ---------- TopBar / TabBar ----------

TopBar::TopBar(const QString &title, bool withBack, QWidget *parent)
    : QFrame(parent)
{
    setStyleSheet(QStringLiteral("TopBar { border-bottom: 2px solid #000; }"));
    setFixedHeight(Theme::mm(12)); // same header height on every screen
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(Theme::mm(2), 0, Theme::mm(2), 0);
    h->setAlignment(Qt::AlignVCenter);
    if (withBack) {
        auto *back = new QPushButton(Theme::icon(QStringLiteral("arrow-left")), QString(), this);
        back->setFixedSize(Theme::mm(10), Theme::mm(8));
        back->setIconSize(QSize(Theme::mm(4), Theme::mm(4)));
        connect(back, &QPushButton::clicked, this, &TopBar::back);
        h->addWidget(back, 0, Qt::AlignVCenter);
    }
    m_title = new QLabel(title, this);
    m_title->setFont(Theme::titleFont());
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    h->addWidget(m_title, 1);
    m_right = new QHBoxLayout;
    m_right->setSpacing(Theme::mm(2));
    h->addLayout(m_right);
}

QPushButton *TopBar::addIconButton(const QString &icon)
{
    // A framed look, only centred vertically in the fixed-height bar.
    auto *b = new QPushButton(Theme::icon(icon), QString(), this);
    b->setFixedSize(Theme::mm(10), Theme::mm(8));
    b->setIconSize(QSize(Theme::mm(4), Theme::mm(4)));
    m_right->addWidget(b, 0, Qt::AlignVCenter);
    return b;
}

void TopBar::setTitle(const QString &title)
{
    m_title->setText(title);
}

TabBar::TabBar(QWidget *parent)
    : QFrame(parent)
{
    // Icon above the label, tall tabs, thin dividers, the current one inverted.
    setStyleSheet(QStringLiteral("TabBar { border-top: 2px solid #000; }"
                                 "TabBar QToolButton { border: none; border-right: 1px solid #000; border-radius: 0;"
                                 " background: #fff; color: #000; padding: %1px 0 %2px 0; }"
                                 "TabBar QToolButton[last=\"true\"] { border-right: none; }"
                                 "TabBar QToolButton[variant=\"dark\"] { background: #000; color: #fff; font-weight: bold; }")
                      .arg(Theme::mm(1.5))
                      .arg(Theme::mm(1)));
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    const QList<QPair<QString, QString>> tabs{{QStringLiteral("house"), QStringLiteral("Главная")},
                                              {QStringLiteral("layer-group"), QStringLiteral("Каталог")},
                                              {QStringLiteral("book-bookmark"), QStringLiteral("Моя библ.")},
                                              {QStringLiteral("tablet-screen-button"), QStringLiteral("Скачанные")}};
    for (int i = 0; i < tabs.size(); ++i) {
        auto *b = new QToolButton(this);
        b->setText(tabs[i].second);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setFont(Theme::smallFont());
        b->setIconSize(QSize(Theme::mm(5.5), Theme::mm(5.5)));
        b->setMinimumHeight(Theme::mm(15));
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        b->setFocusPolicy(Qt::NoFocus);
        b->setProperty("last", i == tabs.size() - 1);
        connect(b, &QToolButton::clicked, this, [this, i] { emit tabSelected(i); });
        h->addWidget(b, 1);
        m_tabs << b;
        m_icons << tabs[i].first;
    }
    setCurrent(0);
}

void TabBar::setCurrent(int index)
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        const bool on = i == index;
        setVariant(m_tabs[i], on ? "dark" : "normal");
        m_tabs[i]->setIcon(on ? Theme::icon(m_icons[i], Qt::white) : Theme::icon(m_icons[i]));
    }
}

// ---------- Navigator ----------

void Navigator::setRoot(Screen *screen)
{
    while (count() > 0) {
        QWidget *w = widget(0);
        removeWidget(w);
        w->deleteLater();
    }
    push(screen);
}

void Navigator::push(Screen *screen)
{
    Log::debug(QStringLiteral("push %1").arg(QString::fromLatin1(screen->metaObject()->className())));
    addWidget(screen);
    setCurrentWidget(screen);
    screen->shown();
}

void Navigator::pop()
{
    if (count() <= 1)
        return;
    QWidget *top = currentWidget();
    removeWidget(top);
    top->deleteLater();
    setCurrentIndex(count() - 1);
    if (Screen *s = current())
        s->shown();
}

Screen *Navigator::current() const
{
    return qobject_cast<Screen *>(currentWidget());
}

// ---------- dialogs ----------

namespace {

QDialog *frameDialog(QWidget *parent)
{
    auto *d = new QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog);
    d->setModal(true);
    d->setStyleSheet(QStringLiteral("QDialog { border: 3px solid #000; border-radius: %1px; }").arg(Theme::mm(1.5)));
    d->setMinimumWidth(parent ? parent->window()->width() * 85 / 100 : Theme::mm(110));
    return d;
}

void finishDialog(QDialog *d)
{
    d->adjustSize();
    if (QWidget *w = d->parentWidget()) {
        const QRect r = w->window()->geometry();
        d->move(r.center() - QPoint(d->width() / 2, d->height() / 2));
    }
}

QLabel *label(const QString &text, const QFont &font, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setFont(font);
    l->setWordWrap(true);
    l->setAlignment(Qt::AlignCenter);
    return l;
}

} // namespace

bool Ui::showQr(QWidget *parent, const QString &title, const QString &subtitle, const QString &url,
                const QString &hint, const QString &actionText)
{
    QDialog *d = frameDialog(parent);
    auto *v = new QVBoxLayout(d);
    v->setContentsMargins(Theme::mm(4), Theme::mm(4), Theme::mm(4), Theme::mm(4));
    v->addWidget(label(title, Theme::titleFont(), d));
    if (!subtitle.isEmpty())
        v->addWidget(label(subtitle, Theme::bodyFont(), d));
    auto *qr = new QLabel(d);
    qr->setAlignment(Qt::AlignCenter);
    qr->setPixmap(QPixmap::fromImage(makeQrImage(url, qMax(3, Theme::mm(1.0)))));
    v->addWidget(qr);
    QString shortUrl = url;
    shortUrl.remove(QStringLiteral("https://"));
    v->addWidget(label(shortUrl, Theme::smallFont(), d));
    if (!hint.isEmpty()) {
        QLabel *h = label(hint, Theme::smallFont(), d);
        h->setAlignment(Qt::AlignLeft);
        v->addWidget(h);
    }
    auto *row = new QHBoxLayout;
    auto *close = new QPushButton(Theme::icon(QStringLiteral("xmark")), QStringLiteral("Закрыть"), d);
    row->addWidget(close);
    QObject::connect(close, &QPushButton::clicked, d, &QDialog::reject);
    if (!actionText.isEmpty()) {
        auto *act = new QPushButton(Theme::icon(QStringLiteral("rotate")), actionText, d);
        setVariant(act, "dark");
        row->addWidget(act);
        QObject::connect(act, &QPushButton::clicked, d, &QDialog::accept);
    }
    v->addLayout(row);
    finishDialog(d);
    const bool accepted = d->exec() == QDialog::Accepted;
    d->deleteLater();
    return accepted;
}

bool Ui::confirm(QWidget *parent, const QString &text, const QString &okText)
{
    QDialog *d = frameDialog(parent);
    auto *v = new QVBoxLayout(d);
    v->setContentsMargins(Theme::mm(4), Theme::mm(4), Theme::mm(4), Theme::mm(4));
    v->addWidget(label(text, Theme::bodyFont(), d));
    auto *row = new QHBoxLayout;
    auto *no = new QPushButton(QStringLiteral("Отмена"), d);
    auto *yes = new QPushButton(okText, d);
    setVariant(yes, "dark");
    row->addWidget(no);
    row->addWidget(yes);
    v->addLayout(row);
    QObject::connect(no, &QPushButton::clicked, d, &QDialog::reject);
    QObject::connect(yes, &QPushButton::clicked, d, &QDialog::accept);
    finishDialog(d);
    const bool ok = d->exec() == QDialog::Accepted;
    d->deleteLater();
    return ok;
}

int Ui::ask(QWidget *parent, const QString &title, const QString &text, const QString &first, const QString &second)
{
    QDialog *d = frameDialog(parent);
    auto *v = new QVBoxLayout(d);
    v->setContentsMargins(Theme::mm(4), Theme::mm(4), Theme::mm(4), Theme::mm(4));
    v->addWidget(label(title, Theme::titleFont(), d));
    v->addWidget(label(text, Theme::bodyFont(), d));
    int chosen = -1;
    auto *a = new QPushButton(first, d);
    auto *b = new QPushButton(second, d);
    setVariant(a, "dark");
    v->addWidget(a);
    v->addWidget(b);
    QObject::connect(a, &QPushButton::clicked, d, [d, &chosen] {
        chosen = 0;
        d->accept();
    });
    QObject::connect(b, &QPushButton::clicked, d, [d, &chosen] {
        chosen = 1;
        d->accept();
    });
    finishDialog(d);
    d->exec();
    d->deleteLater();
    return chosen;
}

int Ui::choose(QWidget *parent, const QString &title, const QStringList &options)
{
    QDialog *d = frameDialog(parent);
    auto *v = new QVBoxLayout(d);
    v->setContentsMargins(Theme::mm(4), Theme::mm(4), Theme::mm(4), Theme::mm(4));
    v->addWidget(label(title, Theme::titleFont(), d));
    int chosen = -1;
    for (int i = 0; i < options.size(); ++i) {
        auto *b = new QPushButton(options[i], d);
        v->addWidget(b);
        QObject::connect(b, &QPushButton::clicked, d, [d, &chosen, i] {
            chosen = i;
            d->accept();
        });
    }
    auto *cancel = new QPushButton(QStringLiteral("Отмена"), d);
    v->addWidget(cancel);
    QObject::connect(cancel, &QPushButton::clicked, d, &QDialog::reject);
    finishDialog(d);
    d->exec();
    d->deleteLater();
    return chosen;
}

void Ui::toast(QWidget *parent, const QString &text)
{
    QWidget *host = parent ? parent->window() : nullptr;
    if (!host)
        return;
    auto *l = new QLabel(text, host);
    l->setStyleSheet(QStringLiteral("background: #000; color: #fff; padding: %1px; border-radius: %2px;")
                         .arg(Theme::mm(2))
                         .arg(Theme::mm(1)));
    l->setFont(Theme::bodyFont());
    l->setWordWrap(true);
    l->setMaximumWidth(host->width() * 9 / 10);
    l->adjustSize();
    l->move((host->width() - l->width()) / 2, host->height() - l->height() - Theme::mm(16));
    l->show();
    l->raise();
    QTimer::singleShot(2500, l, &QObject::deleteLater);
}

QLabel *Ui::logo(int width, QWidget *parent)
{
    QSvgRenderer renderer(QStringLiteral(":/logo.svg"));
    const QSize size = renderer.defaultSize().scaled(width, width * 4, Qt::KeepAspectRatio);
    QPixmap pm(size);
    pm.fill(Qt::white);
    QPainter p(&pm);
    renderer.render(&p);
    p.end();
    auto *l = new QLabel(parent);
    l->setPixmap(pm);
    l->setFixedSize(size);
    return l;
}

void Ui::hideKeyboard(QWidget *input)
{
    if (input)
        input->clearFocus();
    QGuiApplication::inputMethod()->hide();
}
