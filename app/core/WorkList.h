#pragma once

#include "Catalog.h"
#include "Models.h"

#include <QObject>

#include <functional>
#include <memory>

class ApiClient;

// One page of some list of books (catalog, site search, an author's books).
class WorkSource {
public:
    virtual ~WorkSource() = default;
    // total: -1 when unknown; error: empty on success (user-facing text otherwise);
    // unrecognized: a site page that is neither results nor "nothing found" (DDoS-Guard, new layout).
    using Done = std::function<void(const QList<WorkMeta> &works, int total, bool isLast, const QString &error,
                                    bool unrecognized)>;
    virtual void fetch(int page, Done done) = 0; // page from 1
};

std::unique_ptr<WorkSource> catalogSource(ApiClient *client, const CatalogQuery &q); // 8 per page
std::unique_ptr<WorkSource> siteSearchSource(ApiClient *client, const QString &text); // site page -> meta-info
std::unique_ptr<WorkSource> authorWorksSource(ApiClient *client, const QString &login);

// Results loaded page by page as the screen asks for more; adult / AI books dropped on the way.
class WorkList : public QObject {
    Q_OBJECT
public:
    WorkList(std::unique_ptr<WorkSource> source, bool hideAdult, bool hideAi, QObject *parent = nullptr);

    const QList<WorkMeta> &works() const { return m_works; }
    int total() const { return m_total; }
    bool atEnd() const { return m_atEnd; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    bool unrecognized() const { return m_unrecognized; }

    // Called before every page request; done(false) ends with "no internet" (Retry asks again).
    // The device passes "reconnect Wi-Fi if the firmware switched it off".
    using NetworkGate = std::function<void(std::function<void(bool ok)> done)>;
    void setNetworkGate(NetworkGate gate) { m_gate = std::move(gate); }

    void ensure(int count); // load until works().size() >= count or the source ends; no duplicate requests
    void retry();           // after an error: the same source page again

signals:
    void changed();

private:
    void pump();
    void fetchNext();

    std::unique_ptr<WorkSource> m_source;
    NetworkGate m_gate;
    bool m_hideAdult;
    bool m_hideAi;
    QList<WorkMeta> m_works;
    int m_total = -1;
    int m_nextPage = 1;
    int m_target = 0;
    bool m_atEnd = false;
    bool m_loading = false;
    bool m_unrecognized = false;
    QString m_error;
};
