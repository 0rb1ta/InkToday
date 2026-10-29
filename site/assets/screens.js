/* InkToday app screens for the site. Usage:
     <div class="pb" data-screen="home"></div>   -> PocketBook 970 frame with the screen inside
   Screens follow the app's design mockups (docs/superpowers/specs/mockups); books, authors
   and covers are made up for the site. */
(function () {
  "use strict";

  var base = (document.currentScript && document.currentScript.src || "").replace(/[^/]*$/, "");

  function i(name) {
    var id = name.indexOf("-") > 0 && /^(regular|solid)-/.test(name) ? name : "solid-" + name;
    return '<svg class="i" aria-hidden="true"><use href="' + base + "icons.svg#" + id + '"/></svg>';
  }

  // made-up books, covers drawn in covers.js
  function cover(key) {
    var c = window.InkCovers && InkCovers[key];
    return '<div class="cov svgc">' + (c ? c.svg : "") + "</div>";
  }

  var BTN = {
    read: '<div class="btn dark">' + i("book-open") + "Читать</div>",
    get: '<div class="btn">' + i("download") + "Скачать</div>",
    upd: '<div class="btn">' + i("rotate") + "Обновить</div>",
    buy: '<div class="btn">' + i("qrcode") + "Купить</div>",
    site: '<div class="btn muted">' + i("lock") + "На сайте</div>"
  };

  function tile(n, title, author, btn, badge) {
    var c = cover(n);
    if (badge) c = c.replace("</div>", '<span class="badge">' + badge + "</span></div>");
    return '<div class="tile">' + c + '<div class="t">' + title + "</div>" + BTN[btn] + "</div>";
  }

  function tabs(on) {
    var t = [["house", "Главная"], ["layer-group", "Каталог"], ["book-bookmark", "Моя библ."], ["tablet-screen-button", "Скачанные"]];
    return '<div class="tabs">' + t.map(function (x, k) {
      return "<div" + (k === on ? ' class="on"' : "") + ">" + i(x[0]) + x[1] + "</div>";
    }).join("") + "</div>";
  }

  function bar(title, right) {
    return '<div class="bar"><span class="back">' + i("arrow-left") + title + "</span>" + (right || "") + "</div>";
  }

  function item(num, n, title, meta, btn, cur) {
    return '<div class="item' + (cur ? " cur" : "") + '">' + (num ? '<div class="num">' + num + "</div>" : "") +
      cover(n) + '<div class="it"><div class="tt">' + title + '</div><div class="m">' + meta + "</div></div>" + BTN[btn] + "</div>";
  }

  var LOGO = '<img class="logo" src="' + base + 'logo.svg" alt="InkToday">';

  var S = {};

  S.start =
    '<div class="center">' + LOGO +
      '<div class="tag">неофициальный клиент author.today</div>' +
      '<div class="status">' + i("wifi") + "Подключаемся к Wi-Fi…</div>" +
      '<div class="steps2"><div><b>' + i("check") + "Wi-Fi включён</b></div>" +
        "<div>" + i("regular-clock") + "Проверяем author.today…</div>" +
        '<div style="color:#888">' + i("regular-user") + "Вход в аккаунт</div></div>" +
      '<div class="disc">Не связано с порталом author.today. Книги и оплата — на сайте.</div>' +
    "</div>";

  S.login =
    '<div class="center" style="justify-content:flex-start;padding-top:56px">' + LOGO +
      '<div style="font-weight:700;font-size:20px">Вход в author.today</div>' +
      '<div class="form">' +
        '<div class="field focus">' + i("regular-user") + "Логин или e-mail</div>" +
        '<div class="field">' + i("lock") + 'Пароль<span style="margin-left:auto">' + i("regular-eye") + "</span></div>" +
        '<div class="btn dark">' + i("right-to-bracket") + "Войти</div>" +
        '<div class="btn light">Продолжить без входа</div>' +
      "</div>" +
      '<div class="disc">Неофициальный клиент author.today. Пароль отправляется только на api.author.today и не сохраняется.</div>' +
    "</div>";

  S.home =
    '<div class="bar">InkToday<div class="icons"><div class="ib">' + i("circle-up") + '</div><div class="ib">' + i("rotate") +
      '</div><div class="ib">' + i("magnifying-glass") + '</div><div class="ib">' + i("gear") + "</div></div></div>" +
    '<div class="body">' +
      '<div class="lib"><div class="sec">Моя библиотека <span>все 37 ›</span></div><div class="grid">' +
        tile("trakt", "Северный тракт. Книга 3", "", "upd", "+2 гл.") +
        tile("mayak", "Хранитель маяка", "", "read") +
        tile("vyshgorod", "Дорога на Вышгород", "", "get") +
        tile("lekar", "Лекарь из Нижнего города. Том 5", "", "site") +
      "</div></div>" +
      '<div class="sec">АТ рекомендует <span>все ›</span></div><div class="grid">' +
        tile("pochtalon", "Звёздный почтальон", "", "get") +
        tile("vedma", "Ведьма с улицы Часовщиков", "", "buy") +
        tile("akademiya", "Академия тумана", "", "get") +
        tile("mehanik", "Механик и дракон", "", "get") +
      "</div>" +
      '<div class="sec">Бестселлеры <span>все ›</span></div><div class="grid">' +
        tile("gamma", "Последний рейс на Гамму", "", "get") +
        tile("tihiy", "Тихий город", "", "site") +
        tile("bilet", "Счастливый билет", "", "get") +
        tile("rassvet", "Сто шагов до рассвета", "", "get") +
      "</div>" +
    "</div>" + tabs(0);

  S.book =
    bar("Назад") +
    '<div class="body" style="padding-top:9px">' +
      '<div class="top">' + cover("trakt") +
        '<div><div class="title">Северный тракт. Книга 3</div><div class="author">Дарья Лебедева</div>' +
        '<div class="meta"><div>' + i("list-ol") + "Цикл «Северный тракт», книга 3</div>" +
          "<div>" + i("pen-nib") + "В процессе, обновлено 2 дня назад</div>" +
          "<div>" + i("file-lines") + "412 тыс. знаков, около 10 а. л.</div>" +
          "<div>" + i("tag") + "Фэнтези, Приключения</div>" +
          "<div>" + i("heart") + "1 284 &nbsp;" + i("gift") + "Бесплатно</div></div></div>" +
      "</div>" +
      '<div class="btn dark main">' + i("rotate") + "Обновить и читать (+2 главы)</div>" +
      '<div class="row2"><div class="btn">' + i("bookmark") + 'В библиотеке</div><div class="btn">' + i("wand-magic-sparkles") + "Похожие</div></div>" +
      '<div class="h">Аннотация</div>' +
      '<div class="ann">Караван уходит на север за неделю до первого снега. Ника нанялась в него писарем, чтобы добраться до города, где пропал её брат, и в первую же ночь поняла, что груз в третьей повозке живой…</div>' +
      '<div class="more">Читать аннотацию полностью ›</div>' +
      '<div class="h">Главы (34), новые сверху</div>' +
      '<div class="ch new"><span>● Глава 34. Перевал</span><span>25 окт</span></div>' +
      '<div class="ch new"><span>● Глава 33. Чужие огни</span><span>23 окт</span></div>' +
      '<div class="ch"><span>Глава 32. Брод</span><span>20 окт</span></div>' +
    "</div>" +
    '<div class="pager">◀ стр. 1 из 3 ▶</div>';

  S.closed =
    bar("Назад") +
    '<div class="body" style="padding-top:9px">' +
      '<div class="top">' + cover("lekar") +
        '<div><div class="title">Лекарь из Нижнего города. Том 5</div><div class="author">Алексей Ветров</div>' +
        '<div class="meta"><div>' + i("list-ol") + "Цикл «Лекарь из Нижнего города», книга 5</div>" +
          "<div>" + i("pen-nib") + "В процессе, обновлено вчера</div>" +
          "<div>" + i("file-lines") + "280 тыс. знаков, около 7 а. л.</div>" +
          "<div>" + i("tag") + "Попаданцы, Фэнтези</div>" +
          "<div>" + i("heart") + "5 902 &nbsp;" + i("ruble-sign") + "Подписка 189 ₽</div></div></div>" +
      "</div>" +
      '<div class="btn muted main">' + i("lock") + "Скачивание закрыто автором</div>" +
      '<div class="why">' + i("circle-info") + " Автор разрешает читать эту книгу только на сайте или в приложении author.today. Когда автор откроет скачивание, кнопка станет активной.</div>" +
      '<div class="row2"><div class="btn">' + i("regular-bookmark") + 'В библиотеку</div><div class="btn">' + i("wand-magic-sparkles") + "Похожие</div></div>" +
      '<div class="h">Аннотация</div>' +
      '<div class="ann">Пятый том о городском лекаре, который лечит тех, к кому не ходят врачи из Верхнего города. Теперь его зовут во дворец, и отказаться нельзя.</div>' +
    "</div>" +
    '<div class="pager">◀ стр. 1 из 2 ▶</div>';

  S.series =
    bar("Назад", '<span style="font-weight:400;font-size:12px">Цикл</span>') +
    '<div class="body" style="padding-top:9px">' +
      '<div class="stitle">Лекарь из Нижнего города</div>' +
      '<div class="smeta">Алексей Ветров, 6 книг, цикл продолжается</div>' +
      '<div class="btn wide">' + i("qrcode") + "Купить весь цикл на сайте</div>" +
      item(1, "lekar", "Лекарь из Нижнего города", "Завершено, куплено", "read") +
      item(2, "lekar", "Лекарь из Нижнего города. Том 2", "Завершено, куплено", "get") +
      item(3, "lekar", "Лекарь из Нижнего города. Том 3", "Завершено, 189 ₽", "buy") +
      item(4, "lekar", "Лекарь из Нижнего города. Том 4", "Завершено, 189 ₽", "buy") +
      item(5, "lekar", "Лекарь из Нижнего города. Том 5", "Вы здесь", "site", true) +
      item(6, "lekar", "Лекарь из Нижнего города. Том 6", "В процессе, 189 ₽", "buy") +
    "</div>" +
    '<div class="pager">◀ стр. 1 из 1 ▶</div>';

  S.qr =
    '<div class="bar">InkToday<div class="icons"><div class="ib">' + i("circle-up") + '</div><div class="ib">' + i("rotate") +
      '</div><div class="ib">' + i("magnifying-glass") + '</div><div class="ib">' + i("gear") + "</div></div></div>" +
    '<div class="body" style="opacity:.35"><div class="lib"><div class="sec">Моя библиотека <span>все 37 ›</span></div><div class="grid">' +
      tile("trakt", "Северный тракт. Книга 3", "", "upd") + tile("mayak", "Хранитель маяка", "", "read") +
      tile("vyshgorod", "Дорога на Вышгород", "", "get") + tile("lekar", "Лекарь из Нижнего города", "", "site") +
    "</div></div></div>" + tabs(0) +
    '<div class="dim"></div>' +
    '<div class="dlg"><div class="dt">' + i("circle-up") + "Вышла новая версия InkToday</div>" +
      '<div class="sub">Откройте страницу релиза на телефоне</div>' +
      '<img class="qr" src="' + base + 'qr-releases.svg" alt="QR-код страницы релизов">' +
      '<div class="url">github.com/0rb1ta/InkToday/releases</div>' +
      '<div class="steps">Скачайте архив и скопируйте его на читалку поверх старых файлов. Вход и скачанные книги сохранятся.</div>' +
      '<div class="btns"><div class="btn">' + i("xmark") + 'Закрыть</div><div class="btn dark">' + i("check") + "Понятно</div></div>" +
    "</div>";

  S.buy =
    bar("Назад") +
    '<div class="body" style="opacity:.35"><div class="top">' + cover("vedma") +
      '<div><div class="title">Ведьма с улицы Часовщиков</div><div class="author">Елена Миронова</div></div></div></div>' +
    '<div class="pager">◀ стр. 1 из 2 ▶</div>' +
    '<div class="dim"></div>' +
    '<div class="dlg"><div class="dt">' + i("qrcode") + "Купить на сайте</div>" +
      '<div class="sub">«Ведьма с улицы Часовщиков», 189 ₽</div>' +
      '<img class="qr" src="' + base + 'qr-site.svg" alt="QR-код author.today">' +
      '<div class="url">author.today</div>' +
      '<div class="steps">1. Наведите камеру телефона на код.<br>2. Оплатите книгу на сайте.<br>3. Нажмите «Я оплатил», и появится «Скачать и читать».</div>' +
      '<div class="btns"><div class="btn">' + i("xmark") + 'Закрыть</div><div class="btn dark">' + i("rotate") + "Я оплатил</div></div>" +
    "</div>";

  S.downloads =
    '<div class="bar">Скачанные<div class="ib">' + i("rotate") + "</div></div>" +
    '<div class="body">' +
      '<div class="sum"><span><b>12 книг</b> на читалке, у 2 есть обновления</span><div class="btn">' + i("rotate") + "Обновить все</div></div>" +
      item(0, "trakt", "Северный тракт. Книга 3", "+2 главы, скачано 20 окт", "upd") +
      item(0, "gamma", "Последний рейс на Гамму", "+1 глава, скачано 18 окт", "upd") +
      item(0, "mayak", "Хранитель маяка", "Завершено, актуальная версия", "read") +
      item(0, "akademiya", "Академия тумана", "Завершено, актуальная версия", "read") +
      item(0, "mehanik", "Механик и дракон", "Автор закрыл скачивание после загрузки, файл остаётся", "read") +
      item(0, "vyshgorod", "Дорога на Вышгород", "Файл удалён с читалки", "get") +
    "</div>" +
    '<div class="pager">◀ стр. 1 из 2 ▶</div>' + tabs(3);

  function cb(on, label) {
    return '<div class="row"><div class="cb' + (on ? " on" : "") + '">' + (on ? i("check") : "") + '</div><span class="l">' + label + "</span></div>";
  }

  S.settings =
    bar("Настройки") +
    '<div class="body set">' +
      '<div class="row"><span>' + i("user") + '</span><span class="l"><b>reader</b></span><div class="btn" style="height:22px;margin:0;padding:0 8px;font-weight:400">Выйти</div></div>' +
      '<div class="sec">Подборки на главной</div>' +
      '<div class="two">' + cb(1, "АТ рекомендует") + cb(1, "Бестселлеры") + cb(1, "Популярное") + cb(0, "Горячие новинки") +
        cb(1, "Новые публикации") + cb(0, "Недавно обновлённые") + cb(1, "Сейчас добавляют") + cb(0, "Недавно понравившиеся") + "</div>" +
      '<div class="sec">Скачивание</div>' +
      '<div class="row"><span class="l">Формат файла</span><div class="seg"><span class="on">EPUB</span><span>FB2</span></div></div>' +
      cb(1, "Открывать книгу после скачивания") + cb(1, "Проверять обновления книг при синхронизации") +
      '<div class="sec">Каталог и подборки</div>' +
      cb(0, "Только книги, которые можно скачать") + cb(0, "Скрывать книги 18+") + cb(0, "Скрывать книги, написанные с ИИ") + cb(1, "Загружать обложки") +
      '<div class="sec">Обслуживание</div>' +
      '<div class="grid2"><div class="btn">' + i("xmark") + 'Очистить кэш</div><div class="btn">' + i("tablet-screen-button") +
        'Иконка в меню</div><div class="btn">' + i("file-lines") + 'Журнал</div><div class="btn">' + i("circle-info") + "О программе</div></div>" +
    "</div>" + tabs(0);

  function fit(glass) {
    var scr = glass.querySelector(".scr");
    if (scr) scr.style.setProperty("--k", glass.clientWidth / 412);
  }

  var ro = "ResizeObserver" in window ? new ResizeObserver(function (es) { es.forEach(function (e) { fit(e.target); }); }) : null;

  function mount(pb, name) {
    var glass = pb.querySelector(".pb-glass");
    if (!glass) {
      glass = document.createElement("div");
      glass.className = "pb-glass";
      pb.appendChild(glass);
      if (ro) ro.observe(glass); else window.addEventListener("resize", function () { fit(glass); });
    }
    // replace only the picture: anything else on the glass (the refresh flash) stays
    var scr = document.createElement("div");
    scr.className = "scr";
    scr.dataset.name = name;
    scr.innerHTML = S[name] || "";
    var old = glass.querySelector(".scr");
    if (old) glass.replaceChild(scr, old); else glass.insertBefore(scr, glass.firstChild);
    fit(glass);
    return scr;
  }

  window.InkScreens = { list: S, mount: mount, device: device, html: function (name) { return S[name]; } };

  // <div class="device" data-screen="home"> -> slices for the edge + the front face
  function device(el) {
    el.insertAdjacentHTML("afterbegin",
      '<div class="back"></div><div class="side l"></div><div class="side t"></div><div class="side b"></div>' +
      '<div class="side r"><i class="part sd"></i><i class="part led"></i><i class="part usb"></i></div>');
    var pb = document.createElement("div");
    pb.className = "pb";
    el.appendChild(pb);
    mount(pb, el.getAttribute("data-screen"));
    return pb;
  }

  function init() {
    Array.prototype.forEach.call(document.querySelectorAll(".device[data-screen]"), device);
    Array.prototype.forEach.call(document.querySelectorAll(".pb[data-screen]"), function (pb) {
      mount(pb, pb.getAttribute("data-screen"));
    });
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init); else init();
})();
