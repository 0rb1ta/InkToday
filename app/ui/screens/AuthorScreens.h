#pragma once

#include "Widgets.h"

#include <memory>

class AppContext;
class WorkGrid;
class WorkList;

// All e-books of one author (site page /u/<login>/works).
class AuthorWorksScreen : public Screen {
    Q_OBJECT
public:
    AuthorWorksScreen(AppContext *ctx, const QString &login, const QString &name, QWidget *parent = nullptr);
    ~AuthorWorksScreen() override;
    void pageKey(int direction) override;

signals:
    void back();

private:
    WorkGrid *m_grid;
    std::unique_ptr<WorkList> m_list;
};
