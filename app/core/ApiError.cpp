#include "ApiError.h"

#include <QJsonDocument>
#include <QJsonObject>

ApiError ApiError::fromHttp(int status, const QByteArray &body)
{
    ApiError e;
    e.httpStatus = status;
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    e.code = o.value(QStringLiteral("code")).toString();
    e.message = o.value(QStringLiteral("message")).toString();
    if (o.contains(QStringLiteral("retryAfter")))
        e.retryAfterSec = o.value(QStringLiteral("retryAfter")).toInt();
    e.rateLimit = o.value(QStringLiteral("rateLimit")).toInt();
    const QString period = o.value(QStringLiteral("period")).toString();
    if (period == QLatin1String("Second"))
        e.periodMs = 1000;
    else if (period == QLatin1String("Minute"))
        e.periodMs = 60000;
    else if (period == QLatin1String("Hour"))
        e.periodMs = 3600000;
    if (e.code.isEmpty())
        e.code = QStringLiteral("Http%1").arg(status);
    if (e.message.isEmpty())
        e.message = QStringLiteral("Ошибка сервера (%1)").arg(status);
    return e;
}

ApiError ApiError::networkError(const QString &detail, bool timedOut)
{
    ApiError e;
    e.network = true;
    e.code = QStringLiteral("NetworkError");
    e.detail = detail;
    e.message = timedOut ? QStringLiteral("author.today не отвечает. Попробуйте ещё раз.")
                         : QStringLiteral("Нет связи с author.today. Проверьте Wi-Fi и попробуйте ещё раз.");
    return e;
}

ApiError ApiError::local(const QString &code, const QString &msg)
{
    ApiError e;
    e.code = code;
    e.message = msg;
    return e;
}
