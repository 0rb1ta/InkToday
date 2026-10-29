#pragma once

#include "Catalog.h"
#include "Widgets.h"

#include <memory>

class AppContext;
class QLineEdit;
class QHBoxLayout;
class WorkGrid;
class WorkList;

// "Каталог" tab: the app's only search field on top, filter buttons and paged tiles below.
// Empty field: browsing with filters. A query: authors, the matching tag and books by title
// (through the site, where catalog filters do not apply, so the filter buttons are hidden).
class CatalogScreen : public Screen {
    Q_OBJECT
public:
    explicit CatalogScreen(AppContext *ctx, QWidget *parent = nullptr);
    ~CatalogScreen() override;
    void pageKey(int direction) override;
    void shown() override;
    void setGenre(const QString &code, const QString &title);
    void setTag(const QString &tag); // browse mode filtered by this exact tag
    void focusSearch();              // the magnifier on Home lands here

signals:
    void pickGenre(const QString &currentCode);

private:
    void search(const QString &text);
    void endSearch();
    void showSearchMode(bool on);
    void loadAuthors(const QString &text, int request);
    void checkTag(const QString &text, int request);
    void clearExtras();
    void reload();
    void paintFilters();
    void paintFound();

    AppContext *m_ctx;
    CatalogQuery m_query;
    QString m_genreTitle;
    QString m_searchText; // non-empty: search mode
    int m_request = 0;    // drops answers that belong to an older query
    bool m_hideAdult = false;
    bool m_dnlSetting = true; // settings value the query was built from
    QLineEdit *m_input;
    QWidget *m_filters;
    QWidget *m_countRow;
    QWidget *m_searchRow;
    QLabel *m_searchLabel;
    QWidget *m_extras;          // one row of chips: the matching tag, then found authors
    QHBoxLayout *m_extrasLayout;
    QPushButton *m_genreBtn, *m_sortBtn, *m_stateBtn, *m_accessBtn, *m_dnlBtn, *m_reset;
    QLabel *m_found;
    WorkGrid *m_grid;
    std::unique_ptr<WorkList> m_list;
};

// Genre picker: top-level genres, then a genre's subgenres. "Все жанры" clears the filter.
class GenreScreen : public Screen {
    Q_OBJECT
public:
    GenreScreen(const QList<Genre> &all, const QString &currentCode, QWidget *parent = nullptr);
    void pageKey(int direction) override { m_pager->flip(direction); }

signals:
    void chosen(const QString &code, const QString &title); // empty code: all genres
    void back();

private:
    void showLevel(int parentId);

    QList<Genre> m_all;
    QString m_current;
    int m_parent = 0;
    struct Row {
        QString code;    // catalog code; "#<id>" opens that genre's subgenres
        QString caption; // button text
        QString title;   // what the catalog shows once chosen
    };
    QList<Row> m_rows;
    TopBar *m_top;
    Pager *m_pager;
};
