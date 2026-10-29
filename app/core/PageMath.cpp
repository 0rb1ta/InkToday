#include "PageMath.h"

#include <QtGlobal>

int PageMath::pageCount(int items, int perPage)
{
    if (perPage <= 0 || items <= 0)
        return 1;
    return (items + perPage - 1) / perPage;
}

int PageMath::clampPage(int page, int items, int perPage)
{
    return qBound(0, page, pageCount(items, perPage) - 1);
}

QPair<int, int> PageMath::slice(int page, int items, int perPage)
{
    const int p = clampPage(page, items, perPage);
    const int begin = qMin(items, p * qMax(1, perPage));
    return qMakePair(begin, qMin(items, begin + qMax(1, perPage)));
}
