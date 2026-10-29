#pragma once

#include "Models.h"
#include "Widgets.h"

class AppContext;
class BookActions;
class QLabel;
class QPushButton;
class QVBoxLayout;

// Book card: header, one big action button, library shelf + similar,
// annotation, chapters (newest first, new since download marked "●").
class BookScreen : public Screen {
    Q_OBJECT
public:
    BookScreen(AppContext *ctx, BookActions *actions, qint64 workId, QWidget *parent = nullptr);
    void pageKey(int direction) override { m_pager->flip(direction); }
    void shown() override { render(); }

signals:
    void back();
    void openSeries(const WorkDetails &details);
    void openShelf(const QString &title, const QList<WorkMeta> &works);
    void openText(const QString &title, const QString &text);

private:
    void render();
    void chooseShelf();
    void showSimilar();
    QWidget *chapterRow(int index);

    AppContext *m_ctx;
    BookActions *m_actions;
    qint64 m_id;
    WorkDetails m_details;
    QWidget *m_header;
    QVBoxLayout *m_headerLayout;
    Pager *m_pager;
    QString m_loadError;          // the details request failed: shown with «Повторить»
    bool m_similarPending = false;
};

// A series in order.
class SeriesScreen : public Screen {
    Q_OBJECT
public:
    SeriesScreen(AppContext *ctx, const WorkDetails &from, QWidget *parent = nullptr);
    void pageKey(int direction) override { m_pager->flip(direction); }

signals:
    void back();

private:
    void load(int from);
    QWidget *row(int index);

    AppContext *m_ctx;
    WorkDetails m_from;
    QList<WorkMeta> m_works; // in series order, filled as meta-info arrives
    Pager *m_pager;
};

// Long text (full annotation) split into pages.
class TextScreen : public Screen {
    Q_OBJECT
public:
    TextScreen(const QString &title, const QString &text, QWidget *parent = nullptr);
    void pageKey(int direction) override { m_pager->flip(direction); }

signals:
    void back();

private:
    QStringList m_pages;
    Pager *m_pager;
};
