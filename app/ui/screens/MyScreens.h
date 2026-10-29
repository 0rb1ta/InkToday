#pragma once

#include "Models.h"
#include "Widgets.h"

class AppContext;
class BookActions;
class QPushButton;

// "Моя библиотека": the site library filtered by shelf, optionally only downloaded books.
class LibraryScreen : public Screen {
    Q_OBJECT
public:
    explicit LibraryScreen(AppContext *ctx, QWidget *parent = nullptr);
    void pageKey(int direction) override { m_pager->flip(direction); }
    void shown() override { rebuild(); }

signals:
    void loginRequested();

private:
    void rebuild();

    AppContext *m_ctx;
    QString m_shelf; // empty: all
    bool m_onlyDownloaded = false;
    QList<QPushButton *> m_filters;
    QPushButton *m_downloadedToggle;
    QPushButton *m_login;
    Pager *m_pager;
    QList<WorkMeta> m_works;
};

// "Скачанные": what the app put on the reader, books with updates first.
class DownloadsScreen : public Screen {
    Q_OBJECT
public:
    DownloadsScreen(AppContext *ctx, BookActions *actions, QWidget *parent = nullptr);
    void pageKey(int direction) override { m_pager->flip(direction); }
    void shown() override { rebuild(); }

private:
    void rebuild();
    WorkMeta metaFor(qint64 id) const;
    QList<qint64> withUpdates() const; // books whose tile shows an update badge
    QWidget *row(int index);

    AppContext *m_ctx;
    BookActions *m_actions;
    QLabel *m_summary;
    QPushButton *m_updateAll;
    Pager *m_pager;
    QList<qint64> m_ids;
};

// Settings: account, home shelves, downloading, catalog filters, maintenance.
class SettingsScreen : public Screen {
    Q_OBJECT
public:
    explicit SettingsScreen(AppContext *ctx, QWidget *parent = nullptr);
    void shown() override { rebuild(); }

signals:
    void back();
    void logout();
    void login();
    void openAbout();
    void openLog();

private:
    void rebuild();

    AppContext *m_ctx;
    QWidget *m_body;
};

// "О программе": unofficial-client notice, version, third-party licenses.
class AboutScreen : public Screen {
    Q_OBJECT
public:
    explicit AboutScreen(QWidget *parent = nullptr);
    void pageKey(int direction) override { m_pager->flip(direction); }

signals:
    void back();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Block {
        enum Kind { Logo, Heading, Text } kind;
        QString text;
    };
    void paginate();
    QWidget *blockWidget(const Block &b) const;

    QList<Block> m_blocks;
    QList<QPair<int, int>> m_pages; // [from, to) block ranges, packed by measured height
    QSize m_laidOutFor;
    Pager *m_pager;
};
