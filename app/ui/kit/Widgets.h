#pragma once

#include "BookStatus.h"
#include "Models.h"

#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>

#include <functional>

class QToolButton;

class CoverCache;
class QGridLayout;
class QHBoxLayout;

// The single action button of a book: same size everywhere; label and icon follow ActionState.
class ActionButton : public QPushButton {
    Q_OBJECT
public:
    explicit ActionButton(QWidget *parent = nullptr);
    void setState(ActionState state);
    ActionState state() const { return m_state; }

private:
    ActionState m_state = ActionState::Unknown;
};

// Label that elides to a fixed number of lines (titles on tiles and cards).
class ElidedLabel : public QLabel {
    Q_OBJECT
public:
    ElidedLabel(int maxLines, QWidget *parent = nullptr);
    void setFullText(const QString &text);
    // Tiles keep the height of maxLines so buttons line up; a card title takes only what it uses.
    void setReserveAllLines(bool on) { m_reserveAll = on; relayout(); }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void relayout();
    int m_maxLines;
    bool m_reserveAll = true;
    QString m_full;
};

// Button whose text ends with "…" when it does not fit (chips sharing a row, long names):
// a plain QPushButton centres and clips it on both sides.
class ElidedButton : public QPushButton {
    Q_OBJECT
public:
    explicit ElidedButton(const QString &text, QWidget *parent = nullptr);
    void setFullText(const QString &text);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void relayout();
    QString m_full;
};

// Book tile: cover (3:4), up to two title lines, optional badge, action button.
class Tile : public QWidget {
    Q_OBJECT
public:
    Tile(CoverCache *covers, bool loadCovers, QWidget *parent = nullptr);
    void setWork(const WorkMeta &work, ActionState state, const QString &badge = {});
    void setShowAuthor(bool on); // one muted line under the title (grids; not Home, not an author's page)
    qint64 workId() const { return m_id; }

signals:
    void opened(qint64 id);
    void actionClicked(qint64 id, ActionState state);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    CoverCache *m_covers;
    bool m_loadCovers;
    qint64 m_id = 0;
    QLabel *m_cover;
    QLabel *m_badge;
    ElidedLabel *m_title;
    ElidedLabel *m_author;
    ActionButton *m_button;
    QImage m_image;
};

// Page-by-page view of a list (no scrolling on e-ink): grid of cols×rows items built on demand,
// "стр. N из M" footer, tap on the left/right 15 % edge flips; full refresh every 6 flips.
class Pager : public QWidget {
    Q_OBJECT
public:
    Pager(int cols, int rows, QWidget *parent = nullptr);
    void setFactory(std::function<QWidget *(int index)> factory) { m_factory = std::move(factory); }
    void setCount(int count);
    // New total while the visible items stay the same: footer only, no e-ink redraw.
    void setCountQuiet(int count);
    void setEmptyText(const QString &text) { m_emptyText = text; }
    void flip(int direction); // +1 / -1
    int page() const { return m_page; }
    void rebuild();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int m_cols, m_rows;
    int m_count = 0;
    int m_page = 0;
    int m_flips = 0;
    QString m_emptyText = QStringLiteral("Пусто");
    std::function<QWidget *(int)> m_factory;
    QWidget *m_grid;
    QLabel *m_footer;
};

// Every screen: optional pager to receive the hardware page keys.
class Screen : public QWidget {
    Q_OBJECT
public:
    using QWidget::QWidget;
    virtual void pageKey(int direction) { Q_UNUSED(direction); }
    virtual void shown() {} // when it becomes the visible screen again (refresh data)
};

// Header: optional back arrow, title, icon buttons on the right.
class TopBar : public QFrame {
    Q_OBJECT
public:
    TopBar(const QString &title, bool withBack, QWidget *parent = nullptr);
    QPushButton *addIconButton(const QString &icon);
    void setTitle(const QString &title);

signals:
    void back();

private:
    QLabel *m_title;
    QHBoxLayout *m_right;
};

class TabBar : public QFrame {
    Q_OBJECT
public:
    explicit TabBar(QWidget *parent = nullptr);
    void setCurrent(int index);

signals:
    void tabSelected(int index);

private:
    QList<QToolButton *> m_tabs;
    QStringList m_icons;
};

// Screen stack with a full e-ink refresh on every change.
class Navigator : public QStackedWidget {
    Q_OBJECT
public:
    using QStackedWidget::QStackedWidget;
    void setRoot(Screen *screen);
    void push(Screen *screen);
    void pop();
    Screen *current() const;
    int depth() const { return count(); }
};

namespace Ui {

// Modal QR dialog (buy / share / site-only). Returns true when `actionText` was pressed.
bool showQr(QWidget *parent, const QString &title, const QString &subtitle, const QString &url,
            const QString &hint, const QString &actionText = {});
bool confirm(QWidget *parent, const QString &text, const QString &okText);
// Returns the chosen index or -1.
int choose(QWidget *parent, const QString &title, const QStringList &options);
// A question with two answers, the first one dark: 0 or 1, -1 when closed without an answer.
int ask(QWidget *parent, const QString &title, const QString &text, const QString &first, const QString &second);
void toast(QWidget *parent, const QString &text);
// Closes the firmware on-screen keyboard after a search (it is modal: the app does not react while it is open).
void hideKeyboard(QWidget *input);
// The InkToday logo (":/logo.svg") rendered `width` pixels wide.
QLabel *logo(int width, QWidget *parent);

} // namespace Ui
