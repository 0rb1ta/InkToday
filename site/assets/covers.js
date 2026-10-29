/* Covers of made-up books for the app screens: black and white drawings, 100x147 (book proportions).
   window.InkCovers[key] = { title, author, svg } */
(function () {
  "use strict";

  var F = '"InkToday Sans", "Segoe UI", Roboto, sans-serif';

  // lines of text: [[text, y, size, weight]], centered unless x given
  function txt(lines, fill, x, anchor) {
    return lines.map(function (l) {
      return '<text x="' + (x == null ? 50 : x) + '" y="' + l[1] + '" font-size="' + l[2] + '" font-weight="' + (l[3] || 400) +
        '" text-anchor="' + (anchor || "middle") + '" fill="' + fill + '" font-family=\'' + F + "' letter-spacing=\"" + (l[4] || 0) + '">' + l[0] + "</text>";
    }).join("");
  }

  function svg(body) {
    return '<svg viewBox="0 0 100 147" preserveAspectRatio="xMidYMid slice" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">' + body + "</svg>";
  }

  function stars(seed, n, fill) {
    var s = "", r = seed;
    for (var k = 0; k < n; k++) {
      r = (r * 9301 + 49297) % 233280; var x = r / 233280 * 100;
      r = (r * 9301 + 49297) % 233280; var y = r / 233280 * 90;
      r = (r * 9301 + 49297) % 233280; var z = .3 + r / 233280 * .8;
      s += '<circle cx="' + x.toFixed(1) + '" cy="' + y.toFixed(1) + '" r="' + z.toFixed(2) + '" fill="' + fill + '"/>';
    }
    return s;
  }

  var C = {};

  C.trakt = { title: "Северный тракт", author: "Дарья Лебедева", svg: svg(
    '<rect width="100" height="147" fill="#f4f4f1"/>' +
    '<circle cx="72" cy="58" r="10" fill="none" stroke="#000" stroke-width="1.6"/>' +
    '<path d="M0 98 18 74l9 9 17-24 14 17 10-11 32 33v49H0z" fill="#000"/>' +
    '<path d="M44 147h12l-5.2-50h-1.6z" fill="#f4f4f1"/><path d="M47 147h6l-2.6-50z" fill="#000" opacity=".25"/>' +
    txt([["Дарья Лебедева", 14, 6.5, 400, .3]], "#000") +
    txt([["Северный", 30, 14, 300], ["тракт", 44, 15, 900]], "#000") +
    txt([["книга третья", 140, 5.5, 700, .6]], "#fff")) };

  C.mayak = { title: "Хранитель маяка", author: "Ирина Северова", svg: svg(
    '<rect width="100" height="147" fill="#000"/>' +
    '<path d="M52 58 100 34v40z" fill="#fff" opacity=".2"/><path d="M48 58 0 44v26z" fill="#fff" opacity=".12"/>' +
    '<path d="M43 118h14l-3-58h-8z" fill="#fff"/><path d="M45 60h10l-5-8z" fill="#fff"/>' +
    '<rect x="44.6" y="78" width="10.8" height="4" fill="#000"/><rect x="44" y="96" width="12" height="4" fill="#000"/>' +
    '<rect y="118" width="100" height="29" fill="#333"/>' +
    '<path d="M0 124q6-2.5 12 0t12 0 12 0 12 0 12 0 12 0 12 0 16 0M0 132q6-2.5 12 0t12 0 12 0 12 0 12 0 12 0 12 0 16 0" stroke="#999" stroke-width=".8" fill="none"/>' +
    txt([["Ирина Северова", 13, 6.2, 400, .3]], "#bbb") +
    txt([["Хранитель", 27, 13, 300], ["маяка", 41, 15, 900]], "#fff")) };

  C.vyshgorod = { title: "Дорога на Вышгород", author: "Павел Громов", svg: svg(
    '<rect width="100" height="147" fill="#ccc"/>' +
    '<path d="M0 80h100v67H0z" fill="#999"/>' +
    '<path d="M18 80V70h5v-6h4v6h6V60l4-6 4 6v20h6V66h8v14h6V58h3v-6h3v6h3v22h6V72h7v8z" fill="#000"/>' +
    '<path d="M50 147c-30-20 34-38 4-54-6-3-6-8 0-13" fill="none" stroke="#f4f4f1" stroke-width="7"/>' +
    '<path d="M50 147c-30-20 34-38 4-54-6-3-6-8 0-13" fill="none" stroke="#000" stroke-width=".6" stroke-dasharray="2 2"/>' +
    txt([["Павел Громов", 14, 6.5, 700, .3]], "#000") +
    txt([["Дорога", 30, 14, 900], ["на Вышгород", 42, 9.5, 300]], "#000")) };

  C.lekar = { title: "Лекарь из Нижнего города", author: "Алексей Ветров", svg: svg(
    '<rect width="100" height="147" fill="#2a2a2a"/>' +
    '<circle cx="50" cy="78" r="26" fill="none" stroke="#fff" stroke-width="1.5"/>' +
    '<circle cx="50" cy="78" r="21" fill="none" stroke="#fff" stroke-width=".5"/>' +
    '<path d="M45 62h10v11h11v10H55v11H45V83H34V73h11z" fill="#fff"/>' +
    '<path d="M28 104q22 10 44 0M24 99q-6 14 8 18M76 99q6 14-8 18" fill="none" stroke="#bbb" stroke-width="1"/>' +
    txt([["Алексей Ветров", 14, 6.3, 400, .3]], "#ccc") +
    txt([["Лекарь", 30, 15, 900], ["из Нижнего города", 40, 7.5, 300]], "#fff") +
    txt([["том пятый", 136, 5.5, 700, .6]], "#ccc")) };

  C.pochtalon = { title: "Звёздный почтальон", author: "Мира Орлова", svg: svg(
    '<rect width="100" height="147" fill="#000"/>' + stars(7, 40, "#fff") +
    '<circle cx="62" cy="96" r="18" fill="#ddd"/><path d="M62 78a18 18 0 0 1 0 36z" fill="#999"/>' +
    '<ellipse cx="62" cy="96" rx="32" ry="7" fill="none" stroke="#fff" stroke-width="1.4" transform="rotate(-18 62 96)"/>' +
    '<g transform="rotate(-12 28 70)"><rect x="14" y="62" width="26" height="17" fill="#fff"/><path d="M14 62l13 10 13-10" fill="none" stroke="#000" stroke-width="1.2"/></g>' +
    '<path d="M42 66q8-8 18-6" fill="none" stroke="#fff" stroke-width=".7" stroke-dasharray="1.5 2"/>' +
    txt([["Мира Орлова", 13, 6.3, 400, .3]], "#aaa") +
    txt([["Звёздный", 28, 13, 300], ["почтальон", 41, 13.5, 900]], "#fff")) };

  C.vedma = { title: "Ведьма с улицы Часовщиков", author: "Елена Миронова", svg: svg(
    '<rect width="100" height="147" fill="#f4f4f1"/>' +
    '<circle cx="50" cy="86" r="30" fill="#fff" stroke="#000" stroke-width="2.4"/>' +
    '<circle cx="50" cy="86" r="25" fill="none" stroke="#000" stroke-width=".5"/>' +
    [0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330].map(function (a) {
      return '<rect x="49.3" y="58" width="1.4" height="' + (a % 90 ? 3 : 5) + '" transform="rotate(' + a + ' 50 86)"/>';
    }).join("") +
    '<path d="M50 86V67M50 86l12 7" stroke="#000" stroke-width="2.2" stroke-linecap="round"/>' +
    '<path d="M36 55 50 34l14 21z" fill="#000"/><path d="M30 56h40" stroke="#000" stroke-width="2.4"/>' +
    txt([["Елена Миронова", 13, 6.3, 700, .3]], "#000") +
    txt([["Ведьма", 27, 13, 900]], "#000") +
    txt([["с улицы Часовщиков", 132, 7.2, 300]], "#000")) };

  C.akademiya = { title: "Академия тумана", author: "Кирилл Зорин", svg: svg(
    '<rect width="100" height="147" fill="#ddd"/><g transform="translate(0 14)">' +
    '<path d="M22 122V70l8-12 8 12v16h22V54l10-16 10 16v68z" fill="#000"/>' +
    '<path d="M30 58V46M70 38V26" stroke="#000" stroke-width="1.4"/><path d="M70 26l6 2-6 2z" fill="#000"/>' +
    '<g fill="#fff"><rect x="66" y="62" width="3.5" height="7"/><rect x="72" y="62" width="3.5" height="7"/><rect x="27.5" y="76" width="5" height="7"/></g></g>' +
    '<g fill="#ddd" opacity=".9"><rect y="92" width="100" height="5"/><rect y="104" width="100" height="8"/><rect y="118" width="100" height="29"/></g>' +
    txt([["Кирилл Зорин", 14, 6.5, 400, .3]], "#000") +
    txt([["Академия", 28, 13, 900], ["тумана", 40, 11, 300]], "#000")) };

  C.mehanik = { title: "Механик и дракон", author: "Сергей Ястребов", svg: svg(
    '<rect width="100" height="147" fill="#fff"/>' +
    '<g transform="translate(58 76)"><g fill="#000">' + [0, 45, 90, 135].map(function (a) {
      return '<rect x="-4" y="-27" width="8" height="54" transform="rotate(' + a + ')"/>';
    }).join("") + '</g><circle r="21"/><circle r="11" fill="#fff"/><circle r="4"/></g>' +
    '<path d="M4 124q26-34 46-28t40-22q-6 32-32 42T4 124z" fill="#fff" stroke="#000" stroke-width="2"/>' +
    '<path d="M22 116q14-8 24-6M34 120q10-6 22-6" fill="none" stroke="#000" stroke-width=".8"/>' +
    txt([["Сергей Ястребов", 14, 6.3, 400, .3]], "#000") +
    txt([["Механик", 29, 14, 900], ["и дракон", 40, 9, 300]], "#000")) };

  C.gamma = { title: "Последний рейс на Гамму", author: "Лев Дёмин", svg: svg(
    '<rect width="100" height="147" fill="#111"/>' + stars(31, 26, "#ddd") +
    '<circle cx="80" cy="118" r="34" fill="#555"/><circle cx="80" cy="118" r="34" fill="none" stroke="#aaa" stroke-width=".6"/>' +
    '<path d="M8 130Q30 60 76 52" fill="none" stroke="#fff" stroke-width=".8" stroke-dasharray="2 2.5"/>' +
    '<g transform="translate(76 52) rotate(62)"><path d="M0-9c3 3 4 8 3 14h-6c-1-6 0-11 3-14z" fill="#fff"/><path d="M-3 3l-3 4h3zM3 3l3 4H3z" fill="#bbb"/></g>' +
    txt([["Лев Дёмин", 13, 6.3, 400, .3]], "#aaa") +
    txt([["Последний рейс", 27, 9.6, 300], ["на Гамму", 40, 14, 900]], "#fff")) };

  C.tihiy = { title: "Тихий город", author: "Ольга Ранцева", svg: svg(
    '<rect width="100" height="147" fill="#666"/>' +
    '<circle cx="70" cy="58" r="13" fill="#f4f4f1"/><circle cx="76" cy="54" r="11" fill="#666"/>' +
    '<path d="M0 147V104h10V92l6-6 6 6v12h8V84h12v20h6V96l7-7 7 7v8h8V80h10v24h20v43z" fill="#000"/>' +
    '<g fill="#ddd"><rect x="33" y="88" width="2.5" height="3"/><rect x="37" y="94" width="2.5" height="3"/><rect x="74" y="86" width="2.5" height="3"/><rect x="13" y="96" width="2.5" height="3"/></g>' +
    txt([["Ольга Ранцева", 14, 6.3, 400, .3]], "#ddd") +
    txt([["Тихий", 30, 15, 300], ["город", 44, 15, 900]], "#fff")) };

  C.bilet = { title: "Счастливый билет", author: "Нина Белова", svg: svg(
    '<rect width="100" height="147" fill="#f4f4f1"/>' +
    '<g transform="rotate(-8 50 82)"><path d="M16 64h68v12a5 5 0 0 0 0 10v12H16V86a5 5 0 0 0 0-10z" fill="#fff" stroke="#000" stroke-width="1.8"/>' +
    '<path d="M64 66v30" stroke="#000" stroke-width=".8" stroke-dasharray="1.5 1.5"/>' +
    txt([["№ 123 321", 84, 8, 900]], "#000", 40) + txt([["ряд 7", 79, 4.5, 700], ["место 7", 87, 4.5, 700]], "#000", 74) + "</g>" +
    txt([["Нина Белова", 14, 6.5, 400, .3]], "#000") +
    txt([["Счастливый", 31, 12, 300], ["билет", 44, 15, 900]], "#000")) };

  C.rassvet = { title: "Сто шагов до рассвета", author: "Роман Ким", svg: svg(
    '<rect width="100" height="147" fill="#000"/>' +
    '<circle cx="50" cy="104" r="22" fill="#ddd"/><rect y="104" width="100" height="43" fill="#333"/>' +
    '<path d="M0 104h100" stroke="#fff" stroke-width=".6"/>' +
    '<g fill="#000">' + [[44, 140], [54, 132], [45, 124], [53, 117], [47, 111]].map(function (p, k) {
      var s = 1 - k * .14;
      return '<ellipse cx="' + p[0] + '" cy="' + p[1] + '" rx="' + (2 * s).toFixed(2) + '" ry="' + (3.4 * s).toFixed(2) + '" fill="#999"/>';
    }).join("") + "</g>" +
    txt([["Роман Ким", 13, 6.3, 400, .3]], "#aaa") +
    txt([["Сто шагов", 28, 13, 900], ["до рассвета", 40, 10.5, 300]], "#fff")) };

  window.InkCovers = C;
})();
