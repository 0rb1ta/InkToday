#include "AuthorScreens.h"

#include "AppContext.h"
#include "Settings.h"
#include "Theme.h"
#include "WorkGrid.h"
#include "WorkList.h"

#include <QVBoxLayout>


// ---------- AuthorWorksScreen ----------

AuthorWorksScreen::AuthorWorksScreen(AppContext *ctx, const QString &login, const QString &name, QWidget *parent)
    : Screen(parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto *top = new TopBar(name.isEmpty() ? login : name, true, this);
    ctx->addHeaderButtons(top);
    connect(top, &TopBar::back, this, &AuthorWorksScreen::back);
    v->addWidget(top);
    m_grid = new WorkGrid(ctx, this);
    m_grid->setShowAuthor(false); // the same author on every tile
    m_grid->setContentsMargins(Theme::mm(2), Theme::mm(2), Theme::mm(2), 0);
    v->addWidget(m_grid, 1);
    m_list.reset(new WorkList(authorWorksSource(ctx->client, login), ctx->settings->hideAdult, ctx->settings->hideAi));
    m_list->setNetworkGate(ctx->networkGate());
    m_grid->setList(m_list.get());
}

AuthorWorksScreen::~AuthorWorksScreen() = default;

void AuthorWorksScreen::pageKey(int direction)
{
    m_grid->flip(direction);
}
