#include "BookStatus.h"

#include "DownloadIndex.h"
#include "Models.h"

ActionState resolveAction(const BookFacts &f)
{
    if (f.downloaded && !f.fileMissing) {
        // A file we already have stays readable even if the author closed downloads later.
        return f.siteNewer && f.haveDetails && f.allowDownloads ? ActionState::Update : ActionState::Read;
    }
    if (!f.haveDetails) // a paid book that is not bought needs buying whatever the download setting
        return f.price > 0 && !f.isPurchased ? ActionState::Buy : ActionState::Unknown;
    if (f.allowDownloads)
        return ActionState::Download; // guests are asked to log in when they press it
    if (f.price > 0 && !f.isPurchased)
        return ActionState::Buy;
    return f.subscribersOnly ? ActionState::Subscribers : ActionState::SiteOnly;
}

QString actionText(ActionState state)
{
    switch (state) {
    case ActionState::Read: return QStringLiteral("Читать");
    case ActionState::Download: return QStringLiteral("Скачать");
    case ActionState::Update: return QStringLiteral("Обновить");
    case ActionState::Buy: return QStringLiteral("Купить");
    case ActionState::SiteOnly: return QStringLiteral("На сайте");
    case ActionState::Subscribers: return QStringLiteral("Подписчикам");
    case ActionState::Unknown: return QStringLiteral("Проверка…"); // details still loading
    }
    return QString();
}

QString updateBadge(const BookFacts &f, int newChapters)
{
    if (!f.downloaded || f.fileMissing || !f.siteNewer)
        return QString();
    if (f.haveDetails && !f.allowDownloads)
        return QString();
    return newChapters > 0 ? QStringLiteral("+%1 гл.").arg(newChapters) : QStringLiteral("обновлено");
}

int newChapters(const DownloadEntry &entry, const WorkDetails &details)
{
    if (entry.chapterCount <= 0)
        return 0; // count unknown (downloaded by an older build)
    return qMax(0, details.chapterTotal() - entry.chapterCount);
}
