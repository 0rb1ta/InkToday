#include "LibraryActions.h"

#include "ApiClient.h"
#include "LibraryStore.h"

#include <QJsonArray>

LibraryActions::LibraryActions(ApiClient *client, LibraryStore *store, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_store(store)
{
}

void LibraryActions::setState(qint64 workId, const QString &state, std::function<void(const ApiError *)> done)
{
    // The field is "workIds"; "ids" makes the server answer 500.
    const QJsonObject body{{QStringLiteral("workIds"), QJsonArray{double(workId)}}, {QStringLiteral("state"), state}};
    m_client->post(QStringLiteral("/v1/account/update-library-state"), body,
                   [this, workId, state, done](const QJsonDocument &, const ApiError *e) {
        if (!e)
            m_store->setState(workId, state);
        done(e);
    });
}
