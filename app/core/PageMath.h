#pragma once

#include <QPair>

// Splitting lists into e-ink pages (no scrolling). An empty list is one empty page.
struct PageMath {
    static int pageCount(int items, int perPage);
    static QPair<int, int> slice(int page, int items, int perPage); // [begin, end)
    static int clampPage(int page, int items, int perPage);
};
