#include "WorkList.h"

#include "ApiClient.h"
#include "SiteSearch.h"

#include <QHash>
#include <QJsonArray>
#include <QPointer>
#include <QUrlQuery>

namespace {

const int kCatalogPageSize = 8; // one screen of tiles (4x2)

QString errorText(const ApiError *e)
{
    if (e->network || e->message.isEmpty())
        return QStringLiteral("Не удалось загрузить.");
    return e->message;
}

class CatalogSource : public WorkSource {
public:
    CatalogSource(ApiClient *client, const CatalogQuery &q) : m_client(client), m_query(q) {}
    void fetch(int page, Done done) override
    {
        m_client->get(catalogPath(m_query, page, kCatalogPageSize), [done](const QJsonDocument &doc, const ApiError *e) {
            if (e) {
                done({}, -1, false, errorText(e), false);
                return;
            }
            const CatalogPage p = parseCatalogPage(doc.object());
            done(p.works, p.total, p.isLast, QString(), false);
        });
    }

private:
    ApiClient *m_client;
    CatalogQuery m_query;
};

// A site page gives book ids; one meta-info call turns them into WorkMeta in the site's order.
class SiteSource : public WorkSource {
public:
    using Url = std::function<QUrl(int page)>;
    using Parse = std::function<SitePage(const QByteArray &html, int page)>;
    SiteSource(ApiClient *client, Url url, Parse parse) : m_client(client), m_url(std::move(url)), m_parse(std::move(parse)) {}

    void fetch(int page, Done done) override
    {
        ApiClient *client = m_client;
        Parse parse = m_parse;
        m_client->getRaw(m_url(page), [client, parse, page, done](const HttpResponse &r, const ApiError *e) {
            // DDoS-Guard answers its challenge with 403/503 and HTML: that is "the site page is not
            // usable", not a retryable network error.
            if (e && !e->network && e->httpStatus != 429) {
                done({}, -1, true, QString(), true);
                return;
            }
            if (e) {
                done({}, -1, false, errorText(e), false);
                return;
            }
            const SitePage p = parse(r.body, page);
            if (!p.recognized) {
                done({}, -1, true, QString(), true);
                return;
            }
            if (p.ids.isEmpty()) {
                done({}, p.total, !p.hasNext, QString(), false);
                return;
            }
            QUrlQuery q;
            for (int i = 0; i < p.ids.size(); ++i)
                q.addQueryItem(QStringLiteral("ids[%1]").arg(i), QString::number(p.ids[i]));
            client->get(QStringLiteral("/v1/work/meta-info?") + q.toString(QUrl::FullyEncoded),
                        [p, done](const QJsonDocument &doc, const ApiError *e2) {
                if (e2) {
                    done({}, -1, false, errorText(e2), false);
                    return;
                }
                QHash<qint64, WorkMeta> byId;
                for (const QJsonValue &v : doc.array()) {
                    const QJsonObject o = v.toObject();
                    if (!o.value(QStringLiteral("isSuccessful")).toBool(true))
                        continue;
                    const WorkMeta w = WorkMeta::fromJson(o.value(QStringLiteral("data")).toObject());
                    if (w.id)
                        byId.insert(w.id, w);
                }
                QList<WorkMeta> works;
                for (qint64 id : p.ids)
                    if (byId.contains(id))
                        works << byId.value(id);
                done(works, p.total, !p.hasNext, QString(), false);
            });
        }, {{"User-Agent", siteUserAgent()}});
    }

private:
    ApiClient *m_client;
    Url m_url;
    Parse m_parse;
};

} // namespace

std::unique_ptr<WorkSource> catalogSource(ApiClient *client, const CatalogQuery &q)
{
    return std::unique_ptr<WorkSource>(new CatalogSource(client, q));
}

std::unique_ptr<WorkSource> siteSearchSource(ApiClient *client, const QString &text)
{
    return std::unique_ptr<WorkSource>(new SiteSource(
        client, [text](int page) { return siteSearchUrl(text, page); }, &parseSiteWorks));
}

std::unique_ptr<WorkSource> authorWorksSource(ApiClient *client, const QString &login)
{
    return std::unique_ptr<WorkSource>(new SiteSource(
        client, [login](int page) { return authorWorksUrl(login, page); },
        [login](const QByteArray &html, int page) { return parseAuthorWorks(html, login, page); }));
}

WorkList::WorkList(std::unique_ptr<WorkSource> source, bool hideAdult, bool hideAi, QObject *parent)
    : QObject(parent)
    , m_source(std::move(source))
    , m_hideAdult(hideAdult)
    , m_hideAi(hideAi)
{
}

void WorkList::ensure(int count)
{
    m_target = qMax(m_target, count);
    pump();
}

void WorkList::retry()
{
    m_error.clear();
    pump();
}

void WorkList::pump()
{
    if (m_loading || m_atEnd || !m_error.isEmpty() || m_works.size() >= m_target)
        return;
    m_loading = true;
    if (!m_gate) {
        fetchNext();
        return;
    }
    QPointer<WorkList> self(this);
    m_gate([self](bool ok) {
        if (!self)
            return;
        if (ok) {
            self->fetchNext();
            return;
        }
        self->m_loading = false;
        self->m_error = QStringLiteral("Нет подключения к интернету.");
        emit self->changed();
    });
}

void WorkList::fetchNext()
{
    QPointer<WorkList> self(this);
    m_source->fetch(m_nextPage, [self](const QList<WorkMeta> &works, int total, bool isLast, const QString &error,
                                       bool unrecognized) {
        if (!self)
            return;
        self->m_loading = false;
        if (!error.isEmpty()) {
            self->m_error = error;
            emit self->changed();
            return;
        }
        for (const WorkMeta &w : works) {
            if ((self->m_hideAdult && w.adultOnly) || (self->m_hideAi && w.usingNeuralNetworks))
                continue;
            self->m_works << w;
        }
        if (total >= 0)
            self->m_total = total;
        self->m_unrecognized = unrecognized;
        self->m_atEnd = isLast || unrecognized;
        ++self->m_nextPage;
        emit self->changed();
        self->pump();
    });
}
