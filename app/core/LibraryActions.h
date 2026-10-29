#pragma once

#include <QObject>

#include <functional>

class ApiClient;
class LibraryStore;
struct ApiError;

// Moves a book between the user's site shelves (Reading / Saved / Finished / Disliked; None removes it).
class LibraryActions : public QObject {
    Q_OBJECT
public:
    LibraryActions(ApiClient *client, LibraryStore *store, QObject *parent = nullptr);
    void setState(qint64 workId, const QString &state, std::function<void(const ApiError *)> done);

private:
    ApiClient *m_client;
    LibraryStore *m_store;
};
