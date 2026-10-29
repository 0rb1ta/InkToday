#pragma once

#include <QPointer>
#include <QWidget>

class AppContext;
class Pager;
class QLabel;
class QPushButton;
class WorkList;

// 4x2 tiles over a WorkList that loads more as the reader flips (catalog, search, author's books).
// The next screen is requested while the last loaded one is shown, so flipping rarely waits.
class WorkGrid : public QWidget {
    Q_OBJECT
public:
    explicit WorkGrid(AppContext *ctx, QWidget *parent = nullptr);
    void setList(WorkList *list); // not owned; nullptr clears
    void flip(int direction);
    void setShowAuthor(bool on) { m_showAuthor = on; } // off on an author's own page

private:
    void refresh();

    AppContext *m_ctx;
    QPointer<WorkList> m_list;
    Pager *m_pager;
    QLabel *m_status;
    QPushButton *m_retry;
    bool m_showAuthor = true;
    int m_shown = -1;
    QString m_emptyShown;
};
