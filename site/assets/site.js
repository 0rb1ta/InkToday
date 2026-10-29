/* InkToday promo page: one reader flies to each section, text turns like a page, sheets drift behind.
   Without this script the page is a plain readable document. */
(function () {
  "use strict";

  var reduce = matchMedia("(prefers-reduced-motion: reduce)").matches;
  function $$(s) { return [].slice.call(document.querySelectorAll(s)); }

  var pages = $$(".page");
  var stops = pages;
  var flyer = document.getElementById("flyer");
  var ghosts = $$(".flyer.ghost");
  var pb;
  var flash = document.getElementById("flash");
  var num = document.getElementById("num");
  var ghostBox = document.getElementById("ghosts");
  var feats = $$("#feats button");
  var cur = -1, mx = 0, my = 0, flyTimer = 0;

  // where the reader stands for each side: x, y as a share of the window, tilt
  var POS = {
    r: [.74, .5, 4, -28],
    l: [.26, .5, -3, 24]
  };

  function phone() { return innerWidth <= 860; }

  function place(el, pos, lag) {
    var s = POS[pos] || POS.r;
    var w = el.offsetWidth, h = el.offsetHeight;
    var x = s[0] * innerWidth - w / 2 + mx * 16, y = s[1] * innerHeight - h / 2 + my * 12;
    el.style.transitionDelay = (lag || 0) + "ms";
    el.style.transform = "translate(" + x.toFixed(1) + "px," + y.toFixed(1) + "px) perspective(1600px) rotateY(" + s[3] + "deg) rotate(" + s[2] + "deg)";
  }

  // E Ink style screen change: black, white, then the new picture
  function show(name) {
    var scr = pb.querySelector(".scr");
    if (scr && scr.dataset.name === name) return;
    if (reduce) { InkScreens.mount(pb, name); return; }
    var glass = pb.querySelector(".pb-glass");
    var f = glass.querySelector(".refresh");
    if (!f) { f = document.createElement("div"); f.className = "refresh"; glass.appendChild(f); }
    f.classList.remove("go"); void f.offsetWidth; f.classList.add("go");
    setTimeout(function () { InkScreens.mount(pb, name); }, 90);
  }

  function screenFor(sec) {
    if (sec.id === "features") {
      var on = feats.filter(function (b) { return b.getAttribute("aria-pressed") === "true"; })[0];
      if (on) return on.dataset.screen;
    }
    return sec.dataset.screen;
  }

  function activate(k) {
    if (k === cur) return;
    var first = cur < 0;
    if (!first) ghost(stops[cur]);
    cur = k;
    var sec = stops[k];
    pages.forEach(function (p, i) {
      p.classList.toggle("on", i === k);
      p.classList.toggle("past", i < k);
      p.classList.toggle("next", i > k);
    });
    var dark = sec.dataset.theme === "dark";
    if (dark !== document.body.classList.contains("dark")) {
      if (!first && !reduce) {
        flash.classList.remove("go"); void flash.offsetWidth; flash.classList.add("go");
        setTimeout(function () { document.body.classList.toggle("dark", dark); }, 120);
      } else document.body.classList.toggle("dark", dark);
    }
    num.textContent = Math.min(k + 1, pages.length) + " из " + pages.length;
    document.body.classList.toggle("at-top", k === 0);

    if (phone()) return;
    var pos = sec.dataset.pos;
    place(flyer, pos);
    ghosts.forEach(function (g, i) { place(g, pos, 70 * (ghosts.length - i)); });
    if (!first && !reduce) {
      document.body.classList.add("flying");
      clearTimeout(flyTimer);
      flyTimer = setTimeout(function () { document.body.classList.remove("flying"); }, 900);
    }
    if (sec.dataset.screen) {
      var name = screenFor(sec);
      if (first) InkScreens.mount(pb, name);
      else setTimeout(function () { show(name); }, reduce ? 0 : 520);
    }
  }

  // the section under the middle of the window is the open one
  function pick() {
    var mid = innerHeight * (phone() ? .8 : .5), k = 0;
    for (var i = 0; i < stops.length; i++) {
      var r = stops[i].getBoundingClientRect();
      if (r.top <= mid) k = i;
    }
    return k;
  }

  var ticking = false;
  function frame() {
    ticking = false;
    activate(pick());
    if (reduce) return;
    ghostBox.style.transform = "translate3d(" + (mx * -24).toFixed(1) + "px," + (my * -16).toFixed(1) + "px,0)";
  }

  // E Ink ghosting: the heading of the section you leave stays on the panel, faint, then fades
  var ghostSpots = [["5vw", "7vh", -3], ["46vw", "60vh", 2], ["8vw", "56vh", -1], ["42vw", "6vh", 3]], ghostN = 0;
  function ghost(sec) {
    var h = sec && sec.querySelector("h1, h2");
    if (!h || reduce || phone()) return;
    var g = document.createElement("div");
    var s = ghostSpots[ghostN++ % ghostSpots.length];
    g.className = "ghost-h";
    g.style.cssText = "left:" + s[0] + ";top:" + s[1] + ";rotate:" + s[2] + "deg;opacity:.08";
    g.innerHTML = h.innerHTML;
    ghostBox.appendChild(g);
    requestAnimationFrame(function () { requestAnimationFrame(function () { g.style.opacity = ".035"; }); });
    var all = ghostBox.children;
    while (all.length > 3) ghostBox.removeChild(all[0]);
    for (var i = 0; i < all.length - 1; i++) all[i].style.opacity = ".015";
  }
  function onScroll() { if (!ticking) { ticking = true; requestAnimationFrame(frame); } }
  addEventListener("scroll", onScroll, { passive: true });
  addEventListener("resize", function () {
    if (!phone() && cur >= 0) {
      var pos = stops[cur].dataset.pos;
      place(flyer, pos); ghosts.forEach(function (g) { place(g, pos); });
    }
    onScroll();
  });

  // feature lines switch the reader's screen
  function pressFeat(b) {
    feats.forEach(function (x) { x.setAttribute("aria-pressed", x === b ? "true" : "false"); });
    if (!phone() && stops[cur] && stops[cur].id === "features") show(b.dataset.screen);
    var spot = document.querySelector("#features .spot .pb");
    if (spot && phone()) InkScreens.mount(spot, b.dataset.screen);
  }
  feats.forEach(function (b) {
    b.addEventListener("click", function () { pressFeat(b); });
    b.addEventListener("mouseenter", function () { pressFeat(b); });
    b.addEventListener("focus", function () { pressFeat(b); });
  });

  // one wheel turn, key or pager click = the next section, with a quick glide
  var gliding = false;
  function glideTo(top) {
    top = Math.max(0, Math.min(top, document.documentElement.scrollHeight - innerHeight));
    if (reduce) { scrollTo(0, top); return; }
    var from = scrollY, d = top - from, t0 = performance.now(), dur = 750;
    if (Math.abs(d) < 2) return;
    gliding = true;
    document.documentElement.style.scrollBehavior = "auto";
    (function step(now) {
      var t = Math.min(1, (now - t0) / dur);
      var e = t < .5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;
      scrollTo(0, from + d * e);
      if (t < 1) requestAnimationFrame(step);
      else { gliding = false; document.documentElement.style.scrollBehavior = ""; }
    })(t0);
  }
  function jump(d) {
    var tops = stops.map(function (s) { return s.getBoundingClientRect().top + scrollY; });
    var y = scrollY, k;
    if (d > 0) { for (k = 0; k < tops.length && tops[k] <= y + 4; k++); }
    else { for (k = tops.length - 1; k >= 0 && tops[k] >= y - 4; k--); }
    if (k < 0) return glideTo(0);
    if (k >= tops.length) return glideTo(document.documentElement.scrollHeight);
    glideTo(tops[k]);
  }

  if (!reduce) {
    var acc = 0, accT = 0, quiet = 0;
    addEventListener("wheel", function (e) {
      if (phone() || e.ctrlKey) return;
      e.preventDefault();
      var now = Date.now();
      // a touchpad keeps sending wheel events after the swipe: let them die out first
      if (gliding || now < quiet) { quiet = now + 250; acc = 0; return; }
      if (now - accT > 200) acc = 0;
      accT = now;
      acc += e.deltaMode === 1 ? e.deltaY * 30 : e.deltaY;
      if (Math.abs(acc) >= 30) { jump(acc > 0 ? 1 : -1); acc = 0; }
    }, { passive: false });
    addEventListener("keydown", function (e) {
      if (phone() || e.altKey || e.ctrlKey || e.metaKey) return;
      var t = e.target;
      if (t && (t.isContentEditable || /INPUT|TEXTAREA|SELECT/.test(t.tagName))) return;
      var down = ["ArrowDown", "PageDown", " "].indexOf(e.key) >= 0 && !(e.key === " " && e.shiftKey);
      var up = ["ArrowUp", "PageUp"].indexOf(e.key) >= 0 || (e.key === " " && e.shiftKey);
      if (down || up) { e.preventDefault(); if (!gliding) jump(down ? 1 : -1); }
    });
  }
  document.getElementById("prev").addEventListener("click", function (e) { e.preventDefault(); jump(-1); });
  document.getElementById("next").addEventListener("click", function (e) { e.preventDefault(); jump(1); });

  // the reader and the sheets lean after the mouse a little
  if (!reduce && matchMedia("(pointer: fine)").matches) {
    var raf = 0;
    addEventListener("mousemove", function (e) {
      mx = e.clientX / innerWidth - .5; my = e.clientY / innerHeight - .5;
      if (raf) return;
      raf = requestAnimationFrame(function () {
        raf = 0;
        if (document.body.classList.contains("flying") || phone() || cur < 0) return;
        flyer.style.transitionDuration = ".7s";
        place(flyer, stops[cur].dataset.pos);
        flyer.style.transitionDuration = "";
        frame();
      });
    });
  }

  function start() {
    pb = flyer.querySelector(".pb");
    // phones: each section shows its own reader instead of the flying one
    $$(".spot").forEach(function (s) {
      var p = document.createElement("div");
      p.className = "pb";
      s.appendChild(p);
      InkScreens.mount(p, s.dataset.screen);
    });
    var still = [flyer].concat(ghosts);
    still.forEach(function (el) { el.style.transition = "none"; });
    frame();
    requestAnimationFrame(function () { requestAnimationFrame(function () { still.forEach(function (el) { el.style.transition = ""; }); }); });
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start); else start();
})();
