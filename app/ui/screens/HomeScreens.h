#pragma once

#include "Models.h"
#include "Widgets.h"

class AppContext;

// Home: "Моя библиотека" shelf first, then the enabled site shelves.
// Rows are paged with the hardware keys like every other list.
class HomeScreen : public Screen {
    Q_OBJECT
public:
    HomeScreen(AppContext *ctx, QWidget *parent = nullptr);
    void pageKey(int direction) override { m_pager->flip(direction); }
    void shown() override { rebuild(); }
    void rebuild();

signals:
    void openShelf(const QString &title, const QList<WorkMeta> &works);
    void openLibraryTab();

private:
    QWidget *libraryRow();
    QWidget *shelfRow(const QString &title, const QList<WorkMeta> &works);
    QList<WorkMeta> libraryPreview() const;

    AppContext *m_ctx;
    Pager *m_pager;
    QList<std::function<QWidget *()>> m_rows;
};

// A whole list of books as a 4×2 paged grid ("все ›", "Похожие").
class ShelfScreen : public Screen {
    Q_OBJECT
public:
    ShelfScreen(AppContext *ctx, const QString &title, const QList<WorkMeta> &works, QWidget *parent = nullptr);
    void pageKey(int direction) override { m_pager->flip(direction); }

signals:
    void back();

private:
    AppContext *m_ctx;
    QList<WorkMeta> m_works;
    Pager *m_pager;
};

