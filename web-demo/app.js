/* SPDX-License-Identifier: GPL-3.0-or-later */
(() => {
  "use strict";
  const D = window.DemoData;
  const $ = (s) => document.querySelector(s);
  const esc = (v) =>
    String(v).replace(
      /[&<>"']/g,
      (c) =>
        ({
          "&": "&amp;",
          "<": "&lt;",
          ">": "&gt;",
          '"': "&quot;",
          "'": "&#39;",
        })[c],
    );
  const attr = esc;
  const KEY = "traineros.website.demo.v1";
  const defaults = () => ({
    version: 1,
    name: "Alex",
    theme: "turquoise",
    reduced: false,
    selected: { vale: "bloom", coast: "sky", orbit: "orbit", multi: "rally" },
    money: 2400,
    bag: {},
    party: [0, 1, 2],
    boxes: [3],
    health: D.creatures.map((c) => c.hp),
    favorites: [],
    volume: 55,
    brightness: 80,
    wifi: true,
    bluetooth: true,
    airplane: false,
    dnd: false,
    messages: { Mira: [], Jules: [], Campfire: [], Lounge: [] },
    renames: {},
    hidden: [],
  });
  function load() {
    try {
      const p = JSON.parse(localStorage.getItem(KEY));
      if (!p || p.version !== 1) return defaults();
      // Only import known state; a broken/older demo must never break startup.
      const s = defaults();
      if (typeof p.name === "string") s.name = p.name.slice(0, 24);
      if (["turquoise", "red", "green", "blue", "orange"].includes(p.theme))
        s.theme = p.theme;
      for (const k of ["reduced", "wifi", "bluetooth", "airplane", "dnd"])
        if (typeof p[k] === "boolean") s[k] = p[k];
      for (const k of ["volume", "brightness"])
        if (Number.isFinite(p[k])) s[k] = Math.max(0, Math.min(100, p[k]));
      if (Number.isFinite(p.money))
        s.money = Math.max(0, Math.min(100000, p.money));
      if (
        p.party &&
        p.boxes &&
        [...p.party, ...p.boxes].length === 4 &&
        new Set([...p.party, ...p.boxes]).size === 4 &&
        [...p.party, ...p.boxes].every(
          (n) => Number.isInteger(n) && n >= 0 && n < 4,
        ) &&
        p.party.length > 0
      ) {
        s.party = p.party;
        s.boxes = p.boxes;
      }
      if (Array.isArray(p.health) && p.health.length === 4)
        s.health = p.health.map((v, i) =>
          Number.isFinite(v)
            ? Math.max(0, Math.min(D.creatures[i].max, v))
            : s.health[i],
        );
      if (Array.isArray(p.favorites))
        s.favorites = p.favorites.filter(
          (n) => Number.isInteger(n) && n >= 0 && n < 4,
        );
      for (const c of D.collections)
        if (
          D.games.some((g) => g.id === p.selected?.[c.id] && g.series === c.id)
        )
          s.selected[c.id] = p.selected[c.id];
      for (const g of D.games)
        if (typeof p.renames?.[g.id] === "string")
          s.renames[g.id] = p.renames[g.id].slice(0, 48);
      if (Array.isArray(p.hidden))
        s.hidden = p.hidden.filter((id) => D.games.some((g) => g.id === id));
      for (const shop of D.shops)
        for (const item of shop.items)
          if (Number.isInteger(p.bag?.[item.name]))
            s.bag[item.name] = Math.max(0, Math.min(999, p.bag[item.name]));
      for (const person of Object.keys(s.messages))
        if (Array.isArray(p.messages?.[person]))
          s.messages[person] = p.messages[person]
            .slice(-30)
            .filter(
              (m) => typeof m.text === "string" && typeof m.mine === "boolean",
            )
            .map((m) => ({ text: m.text.slice(0, 500), mine: m.mine }));
      return s;
    } catch {
      return defaults();
    }
  }
  let s = load();
  let page = "home",
    face = "vale",
    collection = null,
    wheelIndex = 0,
    selectedCreature = 0,
    shop = 0,
    cart = {},
    contact = "Mira",
    modal = null,
    modalData = {},
    category = "Appearance";
  let gameId = null,
    joined = false,
    call = false,
    muted = false,
    healing = false,
    bubble = -1,
    search = "",
    guideSearch = "",
    battleRound = 0,
    scanlines = false;
  let scene = null,
    sceneToken = 0,
    sceneTimers = [],
    auxTimers = [],
    toastTimer,
    priorFocus = null;
  const drafts = {};
  const collectionGames = (id) =>
    D.games.filter((g) => g.series === id && !s.hidden.includes(g.id));
  const currentGame = () =>
    D.games.find((g) => g.id === (gameId || s.selected[face])) || D.games[0];
  const title = (g) => s.renames[g.id] || `${g.title}: ${g.edition}`;
  const image = (name) => `assets/${name}.webp`;
  const creature = (i, extra = "") =>
    `<span role="img" aria-label="${D.creatures[i].name}" class="creature c${i} ${extra}"></span>`;
  const button = (label, action, value = "", cls = "") =>
    `<button class="cap ${cls}" data-action="${action}" data-value="${attr(value)}">${label}</button>`;
  const foot = (key, label, action, cls = "") =>
    `<button class="foot-button" data-action="${action}"><span class="keycap ${cls}">${key}</span>${label}</button>`;
  const head = (name, sub = "") =>
    `<header class="page-head"><h2>${name}</h2><small>${sub}</small></header>`;
  const health = (i) =>
    `<div class="health"><i style="width:${(s.health[i] / D.creatures[i].max) * 100}%"></i></div>`;
  const later = (fn, ms) => {
    const timer = setTimeout(fn, ms);
    auxTimers.push(timer);
    return timer;
  };
  function persist() {
    try {
      localStorage.setItem(KEY, JSON.stringify(s));
    } catch {
      /* Embedded/private browsing may forbid storage; the demo still works. */
    }
  }
  function toast(text) {
    clearTimeout(toastTimer);
    $("#toast").textContent = text;
    toastTimer = setTimeout(() => ($("#toast").textContent = ""), 3500);
  }
  function theme() {
    const palette = {
      turquoise: ["#297e79", "#175755", "#103e43", "#76b3a4"],
      red: ["#984b51", "#713b45", "#432a37", "#cb9293"],
      green: ["#497c52", "#345b40", "#203d32", "#92b287"],
      blue: ["#3d739d", "#305477", "#233b53", "#87b2ca"],
      orange: ["#976038", "#72472d", "#49372b", "#c9a375"],
    }[s.theme];
    ["--top", "--body", "--dark", "--rim"].forEach((k, i) =>
      $("#device").style.setProperty(k, palette[i]),
    );
    $("#device").classList.toggle("reduced", s.reduced);
  }
  function go(p, f) {
    cancelScene(false);
    clearAux();
    $("#toast").textContent = "";
    page = p;
    face =
      f ||
      {
        home: "vale",
        worlds: "collections",
        companions: "guide",
        trainer: "profile",
        social: "messages",
      }[p];
    collection = null;
    modal = null;
    gameId = null;
    healing = false;
    render();
  }
  function clearAux() {
    auxTimers.forEach(clearTimeout);
    auxTimers = [];
    bubble = -1;
  }
  function faceTo(f) {
    cancelScene(false);
    clearAux();
    $("#toast").textContent = "";
    face = f;
    collection = null;
    modal = null;
    healing = false;
    if (page === "social")
      contact =
        f === "groups" ? "Campfire" : f === "communities" ? "Lounge" : "Mira";
    render();
  }
  function explanation() {
    let key = page === "companions" ? face : page;
    if (modal === "settings") key = "settings";
    const e = D.explanations[key] || D.explanations.home;
    $("#eyebrow").textContent = e[0];
    $("#explain-title").textContent = e[1];
    $("#explain-text").textContent = e[2];
    const ex = exampleForPage();
    $("#example").textContent = scene ? "■ Остановить" : "▶ Показать пример";
    $("#example-name").textContent = scene
      ? "Можно прервать в любой момент"
      : ex.label;
    $("#scene-status").textContent = scene?.status || "";
  }
  function render() {
    theme();
    $("#tabs").innerHTML = ["Home", "Worlds", "Companions", "Trainer", "Social"]
      .map(
        (name, i) =>
          `<button data-action="page" data-value="${name.toLowerCase()}" style="--tint:${["#f4cc62", "#b4d8a0", "#98cadd", "#eba9ad", "#c6b4df"][i]}" class="${page === name.toLowerCase() ? "active" : ""}" ${page === name.toLowerCase() ? 'aria-current="page"' : ""}>${name}</button>`,
      )
      .join("");
    let faces = [];
    if (page === "home") faces = D.collections.map((c) => [c.id, c.name]);
    if (page === "worlds")
      faces = [
        ["collections", "Collections"],
        ["systems", "Multiverse"],
      ];
    if (page === "companions")
      faces = [
        ["guide", "Guide"],
        ["party", "Party"],
        ["boxes", "Boxes"],
        ["center", "Center"],
        ["playroom", "Playroom"],
        ["shops", "Shops"],
      ];
    if (page === "trainer")
      faces = [
        ["profile", "Profile"],
        ["journey", "Journey"],
        ["hall", "Hall"],
        ["ra", "Achievements"],
      ];
    if (page === "social")
      faces = [
        ["messages", "Messages"],
        ["groups", "Groups"],
        ["communities", "Communities"],
        ["search", "Search"],
      ];
    $("#faces").innerHTML =
      `<span class="nav-icon">L1 R1</span><span class="muted small">Sections</span>${faces.length ? '<span class="nav-icon">L2 R2</span>' : ""}` +
      faces
        .map(
          ([id, name]) =>
            `<button data-action="face" data-value="${id}" class="${face === id ? "active" : ""}" ${face === id ? 'aria-current="true"' : ""}>${name}</button>`,
        )
        .join("");
    $("#choose").hidden = ["worlds", "social", "game"].includes(page);
    const renderers = {
      home: home,
      worlds: worlds,
      companions: companions,
      trainer: trainer,
      social: social,
      game: game,
    };
    $("#screen").innerHTML = (renderers[page] || home)();
    let actions = foot("⌂", "Home menu", "home-menu", "blue");
    if (page === "worlds" && collection)
      actions =
        foot("A", "Play", "wheel-play") +
        foot("Y", "Choose for Home", "wheel-choose", "gold") +
        foot("B", "Collections", "world-back", "pink");
    if (page === "home") actions = foot("A", "Play", "play") + actions;
    if (page === "game") actions = foot("⌂", "Home menu", "home-menu", "blue");
    $("#footer-actions").innerHTML =
      actions +
      (page !== "game" ? foot("Start", "System", "system", "gold") : "");
    renderModal();
    explanation();
    const msgs = $(".messages");
    if (msgs) msgs.scrollTop = msgs.scrollHeight;
  }
  function home() {
    const g = currentGame(),
      c = D.collections.find((c) => c.id === face) || D.collections[0],
      hasParty = face === "vale";
    return `<div class="home-bg" style="background-image:url(${image(g.image)})"></div><div class="home-copy"><div class="row spread"><span class="eyeline">WELCOME BACK, ${esc(s.name)}</span><div class="row"><button class="plain" data-action="cycle" data-value="-1" aria-label="Previous collection">◀</button><span class="small">${c.name}</span><button class="plain" data-action="cycle" data-value="1" aria-label="Next collection">▶</button></div></div><h2>${esc(g.world)}</h2><div class="home-subtitle">${esc(title(g))}</div><p class="muted small">${hasParty ? "The conservatory · Last in-game save" : "Your next adventure is waiting."}</p><div class="home-facts"><div class="stat-card" style="flex:1"><small>${hasParty ? "VALLEY TOKENS" : "MILESTONES"}</small><div class="crystals">${["#89b8e3", "#e998a5", "#e5c259", "#9bc987", "#b59cdb", "#78cbd0"].map((color) => `<span class="crystal" style="--gem:${color}"></span>`).join("")}</div></div><div class="stat-card"><b>${hasParty ? "27" : "12"}</b><small>${hasParty ? "DISCOVERED" : "ACHIEVEMENTS"}</small></div><div class="stat-card"><b>8h 42m</b><small>PLAY TIME</small></div></div><p class="muted small">Last played today · Your progress, ready to continue</p></div><div class="play-control"><small>READY WHEN YOU ARE</small><button class="big-play" data-action="play" aria-label="Start Adventure">A</button><span>Start Adventure</span></div>${hasParty ? `<div class="home-party">${s.party.map((i) => `<button class="plain" data-action="party-detail" data-value="${i}" aria-label="${D.creatures[i].name}">${creature(i)}</button>`).join("")}</div>` : ""}`;
  }
  function worlds() {
    if (collection) {
      const games =
        collection === "all"
          ? D.games.filter((g) => !s.hidden.includes(g.id))
          : collection.startsWith("system:")
            ? D.games.filter(
                (g) =>
                  g.platform === collection.slice(7) &&
                  !s.hidden.includes(g.id),
              )
            : collectionGames(collection);
      if (!games.length)
        return (
          head("Library") +
          `<div class="discovery"><p>No games in this demo collection.</p>${button("Back to collections", "world-back")}</div>`
        );
      wheelIndex = ((wheelIndex % games.length) + games.length) % games.length;
      const g = games[wheelIndex];
      // Index modulo makes the wheel cyclic, including small collections.
      return (
        head(
          collection.startsWith("system:")
            ? collection.slice(7)
            : D.collections.find((c) => c.id === collection)?.name ||
                "All games",
          `${wheelIndex + 1} / ${games.length}`,
        ) +
        `<div class="page-body wheel-layout"><div class="wheel"><button class="wheel-arrow" data-action="wheel" data-value="-1" aria-label="Previous game">⌃</button>${[
          -2, -1, 0, 1, 2,
        ]
          .map((offset) => {
            const idx = (wheelIndex + offset + games.length * 2) % games.length;
            const item = games[idx];
            return `<button class="${offset === 0 ? "current" : Math.abs(offset) === 1 ? "near" : "distant"}" data-action="wheel-select" data-value="${idx}" aria-label="Select ${attr(title(item))}"><span class="game-logo" style="color:${item.color}">${esc(s.renames[item.id] || item.title)}<small>${esc(item.edition)}</small></span></button>`;
          })
          .join(
            "",
          )}<button class="wheel-arrow" data-action="wheel" data-value="1" aria-label="Next game">⌄</button></div><div class="game-detail"><h3>${esc(title(g))}</h3><div class="game-meta"><img src="${image(g.image)}" alt="${attr(g.world)}"><aside><span class="platform">${g.platform}</span><span>${g.year}</span><span>${g.genre}</span><span>${g.players} players</span>${g.online ? '<span class="tag blue">◉ Online play</span>' : '<span class="tag">Solo adventure</span>'}</aside></div><p class="description">${g.description}</p><div class="detail-bottom">${button("▶ Play", "launch", g.id, "gold")}${button("Choose for Home", "select-home", g.id)}<button class="plain" data-action="properties" data-value="${g.id}" aria-label="Manage game">⋯</button></div></div></div>`
      );
    }
    if (face === "systems")
      return (
        head("Multiverse", "Games beyond your collections") +
        `<div class="page-body collection-grid">${["GBA", "GBC", "SNES", "PS1"].map((platform, i) => `<button class="collection" data-action="collection" data-value="system:${platform}"><img src="${image(["valley", "garden", "library", "coast"][i])}" alt=""><span class="collection-label"><b>${platform}</b><small>${D.games.filter((g) => g.platform === platform && !s.hidden.includes(g.id)).length} games</small></span></button>`).join("")}</div>`
      );
    return (
      head("Your worlds", "A collection for every kind of adventure") +
      `<div class="page-body collection-grid">${D.collections.map((c) => `<button class="collection" data-action="collection" data-value="${c.id}"><img src="${image(c.image)}" alt=""><span class="collection-label"><b>${c.name}</b><small>${collectionGames(c.id).length} adventures</small></span></button>`).join("")}</div>`
    );
  }
  function wheelGame() {
    let games = collection?.startsWith("system:")
      ? D.games.filter(
          (g) => g.platform === collection.slice(7) && !s.hidden.includes(g.id),
        )
      : collectionGames(collection || "vale");
    return games[((wheelIndex % games.length) + games.length) % games.length];
  }
  function companions() {
    if (face === "guide") {
      const c = D.creatures[selectedCreature];
      return (
        head(
          "Field Guide",
          `<button class="plain" data-action="guide-search">⌕ Search</button> · 4 companions`,
        ) +
        `<div class="page-body split"><div class="side-list">${D.creatures.map((c, i) => (!c.name.toLowerCase().includes(guideSearch.toLowerCase()) ? "" : `<button class="cap green ${selectedCreature === i ? "selected" : ""}" data-action="creature" data-value="${i}">${creature(i)}<span>${c.name}<small>#${c.number} · ${c.kind}</small></span></button>`)).join("") || "<p>No companions found.</p>"}${guideSearch ? button("Clear search", "guide-clear", "", "pink") : ""}</div><article class="guide-detail"><div class="guide-top"><h3>${c.name}</h3><span class="muted">#${c.number}</span></div><div class="guide-art">${creature(selectedCreature)}<div><span class="tag" style="background:${c.color}">${c.kind}</span><p>${c.description}</p><p class="muted">${c.height} &nbsp; · &nbsp; ${c.weight}</p><button class="plain" data-action="favorite" data-value="${selectedCreature}">${s.favorites.includes(selectedCreature) ? "★ Favorite" : "☆ Add favorite"}</button></div></div><div class="stats">${["HP", "Attack", "Defense", "Sp. Atk", "Sp. Def", "Speed"].map((label, i) => `<div class="stat-chip" style="--chip:${["#f5dc93", "#c6dea7", "#b5dce4", "#eebbc7", "#d2c1e9", "#f2c990"][i]}"><small>${label}</small><strong>${c.stats[i]}</strong></div>`).join("")}</div></article></div>`
      );
    }
    if (face === "party")
      return (
        head("Your party", `${s.party.length} companions · Bloom Trails`) +
        `<div class="page-body party-grid">${s.party.map((i) => `<button class="cap party-card" style="--cap:${D.creatures[i].color}" data-action="party-detail" data-value="${i}">${creature(i)}<div><h3>${D.creatures[i].name}</h3><small>Lv. ${D.creatures[i].level} · ${D.creatures[i].kind}</small>${health(i)}<small>${s.health[i]} / ${D.creatures[i].max} HP</small></div></button>`).join("")}${Array.from({ length: 6 - s.party.length }, () => '<div class="empty-slot">＋</div>').join("")}</div>`
      );
    if (face === "boxes")
      return (
        head("The conservatory", "Box 1 · Garden friends") +
        `<div class="page-body box-grid">${s.boxes.map((i) => `<button class="cap green" data-action="box-detail" data-value="${i}">${creature(i)}<small>${D.creatures[i].name} · Lv. ${D.creatures[i].level}</small></button>`).join("")}${Array.from({ length: 12 - s.boxes.length }, () => '<div class="empty-slot">·</div>').join("")}</div>`
      );
    if (face === "center")
      return (
        head("Care Center", "A little rest goes a long way") +
        `<div class="page-body clinic ${healing ? "healing" : ""}"><div class="clinic-wall"><span class="clinic-sign">✦ VALLEY CARE ✦</span></div><div class="nurse" aria-label="Care assistant"><div class="head"></div><div class="coat"></div><div class="hat">✦</div></div><div class="counter"></div><div class="clinic-party">${s.party.map((i) => creature(i)).join("")}</div><div class="scene-actions">${button(healing ? "Resting…" : "♥ Heal party", "heal", "", "gold")}${button("⇄ Link Counter", "link")}${button("Save care", "save-care", "", "green")}</div></div>`
      );
    if (face === "playroom")
      return (
        head(
          "Party Playroom",
          bubble >= 0
            ? `${D.creatures[bubble].name} is happy to see you`
            : "A quiet afternoon in the valley",
        ) +
        `<div class="page-body scene meadow"><div class="meadow-floor"></div>${s.party.map((i, n) => `<button class="walker" style="left:${13 + n * 22}%;top:${22 + (n % 2) * 25}%;--duration:${14 + n * 5}s" data-action="greet" data-value="${i}" aria-label="Call ${D.creatures[i].name}">${creature(i)}${bubble === i ? '<span class="bubble">♥</span>' : n === 1 ? '<span class="bubble">z z</span>' : ""}</button>`).join("")}<div class="scene-actions">${button("✿ Treat", "treat", "", "pink")}${button("Play together", "play-together", "", "gold")}${button("Practice", "practice")}</div></div>`
      );
    if (face === "shops") return shops();
    return "";
  }
  function shops() {
    const items = D.shops[shop].items;
    const lines = cartItems();
    const total = lines.reduce((sum, l) => sum + l.item.price * l.qty, 0);
    return (
      head("Valley shops", `Wallet · ${s.money.toLocaleString("en")} ◈`) +
      `<div class="page-body shop-layout"><div class="shop-nav"><p class="eyeline">SUPPLIES & GIFTS</p>${D.shops.map((sh, i) => button(`${sh.name}<small>${sh.subtitle}</small>`, "shop", i, i === shop ? "gold" : "")).join("")}${button("Your bag", "bag", "", "green")}</div><div class="stock">${items.map((item, i) => `<div class="stock-item"><span class="item-icon" style="--chip:${item.color}">${item.icon}</span><span style="flex:1">${item.name}<small>${item.price} ◈</small></span><button class="cap gold" data-action="cart-add" data-value="${shop}:${i}" aria-label="Add ${item.name}">＋</button></div>`).join("")}</div><aside class="basket"><h3>Your basket</h3><div class="basket-lines">${lines.length ? lines.map((l) => `<div class="basket-line"><span>${l.item.name}<br><small>${l.qty} × ${l.item.price} ◈</small></span><button data-action="cart-remove" data-value="${l.id}" aria-label="Remove one ${l.item.name}">−</button></div>`).join("") : '<p class="muted small">Something for the road?</p>'}</div><div class="basket-total">Total <b>${total} ◈</b></div><button class="cap gold" data-action="checkout" ${!total || total > s.money ? "disabled" : ""}>${total > s.money ? "Not enough coins" : "Buy basket"}</button></aside></div>`
    );
  }
  function cartItems() {
    return Object.entries(cart)
      .filter(([, qty]) => qty > 0)
      .map(([id, qty]) => {
        const [a, b] = id.split(":").map(Number);
        return { id, qty, item: D.shops[a].items[b] };
      });
  }
  function trainer() {
    if (face === "profile")
      return (
        head("Your Trainer", "Make yourself at home") +
        `<div class="page-body profile-layout"><div class="trainer-pass"><div class="row"><div class="avatar">${esc(s.name[0] || "A")}</div><div><h3>${esc(s.name)}</h3><small>VALLEY EXPLORER</small></div></div>${creature(0)}<div class="row spread"><span>Since October 2026</span><span>✦ 12</span></div></div><div class="timeline"><div class="timeline-row"><strong>A home for your adventures</strong><small>4 collections · 8 games · 3 memories</small></div><div class="timeline-row"><strong>Your companions</strong><small>${s.party.length} traveling with you · ${s.boxes.length} resting in the garden</small></div><div class="timeline-row"><strong>Time well spent</strong><small>8h 42m in the valley</small></div><div>${button("Edit profile", "edit-profile", "", "gold")}</div></div></div>`
      );
    if (face === "journey" || face === "hall")
      return (
        head(
          face === "hall" ? "Hall of memories" : "Your journey",
          face === "hall"
            ? "Little moments worth keeping"
            : "Bloom Trails · Chapter 3",
        ) +
        `<div class="page-body profile-layout"><img src="${image(face === "hall" ? "coast" : "valley")}" alt="A remembered adventure" style="width:43%;height:300px;object-fit:cover;border:3px solid #91a389;border-radius:15px"><div class="timeline">${[
          [
            "Today",
            "A new friend at the conservatory",
            "Coralie joined your party.",
          ],
          [
            "Yesterday",
            "The northern aqueduct",
            "You reached the upper gardens together.",
          ],
          [
            "Three days ago",
            "The first valley token",
            "A little courage, a long way from home.",
          ],
        ]
          .map(
            ([date, name, desc]) =>
              `<div class="timeline-row"><small>${date}</small><strong>${name}</strong><span class="small muted">${desc}</span></div>`,
          )
          .join("")}</div></div>`
      );
    return (
      head("Achievements", "RetroAchievements · illustrative account") +
      `<div class="page-body achievement-grid">${[
        ["✧", "First steps", "Leave the conservatory", true],
        ["◆", "Valley keeper", "Restore all six waterways", true],
        ["❋", "Good company", "Meet four companions", true],
        ["♜", "The long way home", "Find every hidden path", false],
        ["✦", "Skywatcher", "Reach the observatory", false],
        ["♧", "A perfect afternoon", "Help every gardener", false],
      ]
        .map(
          ([icon, name, desc, won]) =>
            `<div class="achievement" style="opacity:${won ? 1 : 0.5}"><span class="medal">${won ? icon : "◇"}</span><div><b>${name}</b><small>${desc}</small><small style="display:block;margin-top:8px">${won ? "Unlocked · 10 points" : "Locked"}</small></div></div>`,
        )
        .join("")}</div>`
    );
  }
  function social() {
    if (face === "search")
      return (
        head("Discover", "Powered by Fluxer") +
        `<div class="page-body discovery"><form id="search-form" class="search-box"><input name="query" aria-label="Find people or communities" placeholder="Find people or communities…" value="${attr(search)}"><button class="cap gold" type="submit">Search</button></form><p class="eyeline">${search ? "DEMO RESULTS" : "PEOPLE & PLACES TO MEET"}</p><div class="search-results">${
          [
            ["Mira", "Friend · Playing Bloom Trails"],
            ["Jules", "Friend · Online"],
            ["Campfire", "Group · 4 friends"],
            ["Lounge", "Community · Valley explorers"],
          ]
            .filter(
              ([name]) =>
                !search || name.toLowerCase().includes(search.toLowerCase()),
            )
            .map(([name, sub]) =>
              button(
                `${name}<small>${sub}</small>`,
                "open-chat",
                name,
                "green",
              ),
            )
            .join("") ||
          "<p>No match in the demo directory. Try Mira or Campfire.</p>"
        }</div></div>`
      );
    const people =
      face === "groups"
        ? ["Campfire"]
        : face === "communities"
          ? ["Lounge"]
          : ["Mira", "Jules"];
    if (!people.includes(contact)) contact = people[0];
    const initial =
      contact === "Mira"
        ? [
            ["Hey! Made it to the glass gardens yet?", false],
            ["Just arrived. The view is incredible.", true],
            ["I’m around if you want some company ✨", false],
          ]
        : contact === "Jules"
          ? [["Want to try the coast later?", false]]
          : contact === "Campfire"
            ? [
                ["Mira: I’m in the valley. Anyone joining?", false],
                ["Jules: Finishing a race — I’ll stay on voice!", false],
              ]
            : [
                [
                  "Welcome to the valley! Share a discovery or say hello.",
                  false,
                ],
              ];
    return (
      head(
        face === "messages"
          ? "Messages"
          : face === "groups"
            ? "Groups"
            : "Communities",
        "Powered by Fluxer",
      ) +
      `<div class="page-body social-layout"><div class="contacts">${people.map((name, i) => `<button class="cap contact ${contact === name ? "selected" : ""}" data-action="contact" data-value="${name}"><span class="avatar" style="background:${i ? "#c8b4df" : "#efc995"}">${name[0]}</span><span>${name}<small>${name === "Mira" ? "◉ Bloom Trails" : "● Online"}</small></span></button>`).join("")}</div><section class="conversation"><div class="chat-head"><div><strong>${contact}</strong><small class="muted"> · ${face === "groups" ? "4 members" : face === "communities" ? "# general" : "Online"}</small></div><div class="row">${call ? `<span class="call-pill">◉ Group call <button class="plain" data-action="mute" aria-label="Toggle microphone">${muted ? "Muted" : "Mic on"}</button><button class="plain" data-action="call" aria-label="Leave call">×</button></span>` : button("♫ Call", "call", "", "green")}${button("Invite to play", "invite", "", "gold")}</div></div><div class="messages">${[...initial.map(([text, mine]) => ({ text, mine })), ...s.messages[contact]].map((m) => `<div class="message ${m.mine ? "mine" : ""}">${esc(m.text)}</div>`).join("")}</div><form id="composer" class="composer"><button class="plain" type="button" data-action="attach" aria-label="Attachment">＋</button><input name="message" aria-label="Message ${contact}" maxlength="500" placeholder="Message ${contact}…" value="${attr(drafts[contact] || "")}"><button class="plain" type="button" data-action="emoji" aria-label="Insert smile">☺</button><button class="cap gold" type="submit">Send</button></form></section></div>`
    );
  }
  function game() {
    const g = currentGame();
    return `<div class="game-scene ${scanlines ? "scanlines" : ""}" style="background-image:url(${image(g.image)})"><div class="game-hud"><strong>${esc(title(g))}</strong><small>${g.world} · Demo scene</small></div>${joined ? '<div class="joined">● Mira joined your adventure</div>' : ""}<button class="walker" data-action="game-walk" aria-label="Explore">${creature(0)}</button><div class="game-note">${joined ? "A friend makes the road feel shorter." : "Take a breath. You’re back in your adventure."}</div></div>`;
  }
  function openModal(type, data = {}) {
    if (!modal) priorFocus = document.activeElement;
    modal = type;
    modalData = data;
    renderModal();
    explanation();
    requestAnimationFrame(() =>
      $("#overlay input, #overlay .close")?.focus({ preventScroll: true }),
    );
  }
  function closeModal() {
    modal = null;
    renderModal();
    explanation();
    if (priorFocus?.isConnected) priorFocus.focus({ preventScroll: true });
  }
  function modalShell(title, body, footer = "", size = "") {
    return `<section class="modal ${size}" role="dialog" aria-modal="true" aria-label="${attr(title)}"><header class="modal-head"><h2>${title}</h2><button class="close" data-action="close" aria-label="Close">×</button></header><div class="modal-body">${body}</div>${footer ? `<footer class="modal-foot">${footer}</footer>` : ""}</section>`;
  }
  function renderModal() {
    const host = $("#overlay");
    if (!modal) {
      host.innerHTML = "";
      setBackgroundInert(false);
      return;
    }
    setBackgroundInert(true);
    let body = "",
      footer = "",
      name = "",
      size = "compact";
    if (modal === "choose") {
      name = "Choose Adventure";
      size = "drawer";
      const games = collectionGames(page === "home" ? face : "vale");
      body = `<div class="choices">${games.map((g) => `<button class="cap adventure-card" data-action="select-home" data-value="${g.id}"><img src="${image(g.image)}" alt=""><span>${esc(g.title)}</span><small>${esc(g.edition)} · ${g.platform}</small></button>`).join("")}</div>`;
    }
    if (modal === "system") {
      name = "System";
      body = `<div class="system-quick">${button(`Wi-Fi ${s.wifi ? "●" : "○"}`, "toggle", "wifi", s.wifi ? "green" : "")}${button(`Bluetooth ${s.bluetooth ? "●" : "○"}`, "toggle", "bluetooth", s.bluetooth ? "green" : "")}${button(`Airplane ${s.airplane ? "●" : "○"}`, "toggle", "airplane", s.airplane ? "gold" : "")}</div>${slider("volume", "Volume", s.volume)}${slider("brightness", "Brightness", s.brightness)}<div class="system-actions">${button("Settings", "settings", "", "gold")}${button("Switch Trainer", "edit-profile")}${button("Notifications", "notifications", "", "pink")}${button("System modes", "modes", "", "purple")}</div>`;
    }
    if (modal === "settings") {
      name = "Settings";
      size = "wide";
      const cats = [
        "Appearance",
        "Sound",
        "Trainer",
        "Library",
        "Saves",
        "Connections",
        "Communication",
        "Credits",
      ];
      body = `<div class="settings-layout"><nav class="settings-nav" aria-label="Settings sections">${cats.map((c) => `<button data-action="setting-category" data-value="${c}" class="${category === c ? "active" : ""}">${c}</button>`).join("")}</nav><div class="settings-content">${settingsContent()}</div></div>`;
    }
    if (modal === "edit-profile") {
      name = "Your Trainer";
      body = `<label for="trainer-name">Trainer name</label><input class="text-input" style="width:100%;margin-top:13px" id="trainer-name" maxlength="24" value="${attr(s.name)}">`;
      footer = button("Save profile", "save-profile", "", "gold");
    }
    if (modal === "home-menu") {
      name = page === "game" ? title(currentGame()) : "Quick access";
      body = `<div class="system-actions">${page === "game" ? button("▶ Continue", "close", "", "gold") + button("Invite to play", "invite") + button("Picture & display", "picture", "", "green") + button("Exit game", "exit", "", "pink") : button("Home", "page", "home", "gold") + button("Messages", "page", "social") + button("Notifications", "notifications", "", "pink") + button("Friends", "open-chat", "Mira", "green")}</div>${call ? '<p class="small muted">♫ Your group call continues in the background.</p>' : ""}`;
    }
    if (modal === "invite") {
      name = "Invite to play";
      const g =
        page === "game"
          ? currentGame()
          : D.games.find((g) => g.id === s.selected.vale);
      body = g.online
        ? `<p>${esc(title(g))}<br><span class="muted small">${g.players} players · Choose a connection</span></p><div class="system-actions">${button("⌁ Nearby", "nearby", "", "green")}${button("◉ Online friend", "online", "", "gold")}</div>`
        : "<p>This adventure is single-player. Choose a multiplayer game from Worlds.</p>";
    }
    if (modal === "online" || modal === "nearby") {
      name = modal === "online" ? "Online friend" : "Nearby players";
      body = `<p class="muted small">${modal === "online" ? "Friends ready to join you" : "Searching nearby… one Trainer found"}</p>${button('<span class="row"><span class="avatar">M</span><span>Mira<small>Compatible game · Ready to join</small></span></span>', "send-invite", "", "gold")}`;
    }
    if (modal === "inviting") {
      name = "Invitation sent";
      body =
        '<div class="row"><div class="avatar">M</div><div><strong>Mira</strong><p>Waiting for your friend…</p></div></div>';
      footer = button("Cancel invitation", "cancel-invite", "", "pink");
    }
    if (modal === "picture") {
      name = "Picture & display";
      body = `<p class="muted">Preview the display controls without leaving the game.</p>${settingToggle("Scanlines", "scanlines", scanlines)}<div class="setting-line"><span>Aspect ratio</span><span class="tag">Original</span></div><div class="setting-line"><span>Decoration</span><span class="tag">Game artwork</span></div>`;
    }
    if (modal === "exit") {
      name = "Return to TrainerOS?";
      body =
        "<p>In TrainerOS, this is where a game that needs manual saving asks whether you saved. This demo has no game save file.</p>";
      footer =
        button("Keep playing", "close") +
        button("I saved · Exit", "exit-confirm", "", "gold");
    }
    if (modal === "notifications") {
      name = "Notifications";
      body =
        button(
          "Mira is ready for a new adventure<small>Open conversation</small>",
          "open-chat",
          "Mira",
          "gold",
        ) +
        button(
          "A new memory from the glass gardens<small>View your journey</small>",
          "journey",
          "",
          "green",
        );
    }
    if (modal === "modes") {
      name = "System modes";
      body =
        '<p>On a handheld, you can switch between TrainerOS, Steam and the desktop. In this browser demo you stay here.</p><div class="badge-row"><span class="tag">TrainerOS</span><span class="tag blue">Steam</span><span class="tag pink">Desktop</span></div>';
    }
    if (modal === "party-detail" || modal === "box-detail") {
      const i = Number(modalData.i),
        c = D.creatures[i];
      name = c.name;
      body = `<div class="portrait-row">${creature(i)}<div>Lv. ${c.level}<p><span class="tag" style="background:${c.color}">${c.kind}</span></p><p class="small">${c.height} · ${c.weight}</p>${modal === "party-detail" ? health(i) : ""}</div></div><p class="small muted">${c.description}</p>`;
      footer =
        modal === "box-detail"
          ? button("Add to party", "to-party", i, "gold")
          : button("Move to box", "to-box", i, "green");
    }
    if (modal === "link") {
      name = "Link Counter";
      body =
        '<p>Mira is nearby and ready to play.</p><div class="system-actions">' +
        button("⇄ Trade", "trade", "", "gold") +
        button("⚔ Friendly battle", "practice") +
        "</div>";
    }
    if (modal === "trade") {
      name = "Trade with Mira";
      body = `<div class="portrait-row"><div>${creature(s.party[0])}<p>${D.creatures[s.party[0]].name} · You</p></div><span class="trade-arrow">⇄</span><div>${creature(s.boxes[0] ?? 3)}<p>${D.creatures[s.boxes[0] ?? 3].name} · Mira</p></div></div><p class="small muted">Both players confirm before an exchange. This illustrative exchange does not create or duplicate companions in your demo collection.</p>`;
      footer = button("Show exchange", "trade-example", "", "gold");
    }
    if (modal === "practice") {
      name = "Friendly battle";
      size = "";
      const c = s.party[0];
      body = `<div class="battle-arena ${modalData.flash ? "attack-flash" : ""}"><div class="fighter">${creature(c)}<h3>${D.creatures[c].name}</h3><div class="health"><i style="width:${Math.max(15, 100 - battleRound * 18)}%"></i></div></div><div class="muted">${battleRound >= 3 ? "Well played!" : `Turn ${battleRound + 1}`}</div><div class="fighter">${creature(3)}<h3>Mira’s Flintlet</h3><div class="health"><i style="width:${Math.max(0, 100 - battleRound * 34)}%"></i></div></div></div><div class="battle-actions">${button(battleRound >= 3 ? "Play again" : "✦ Shimmer", "attack", "", "gold")}${button("Team", "battle-team")}${button("Bag", "bag", "", "green")}</div><p class="small muted">${battleRound >= 3 ? "A friendly finish. Your saved team is unchanged." : "A demonstration of turns, switching and items; not a full battle engine."}</p>`;
    }
    if (modal === "battle-team") {
      name = "Choose a companion";
      body = `<div class="row">${s.party.map((i) => button(creature(i), "battle-switch", i)).join("")}</div>`;
    }
    if (modal === "bag") {
      name = "Your bag";
      body =
        Object.entries(s.bag)
          .filter(([, n]) => n > 0)
          .map(
            ([name, n]) =>
              `<div class="setting-line"><span>${esc(name)}</span><b>× ${n}</b></div>`,
          )
          .join("") ||
        "<p>Your bag is empty. Visit Valley Supply for something useful.</p>";
    }
    if (modal === "save-care") {
      name = "Save care";
      body =
        '<p>Your last recovery copy is ready.</p><div class="setting-line"><span>Bloom Trails</span><span>Today · 14:32</span></div><p class="small muted">TrainerOS protects supported save operations with a recovery copy. This is a demonstration; no save files are read or written here.</p>';
      footer = button("Create demo backup", "backup", "", "gold");
    }
    if (modal === "properties") {
      const g = D.games.find((g) => g.id === modalData.id);
      name = "Game properties";
      body = `<h3>${esc(title(g))}</h3><div class="badge-row"><span class="tag">${g.platform}</span><span class="tag">${g.players} players</span><span class="tag">${g.genre}</span></div><p class="small muted">Library / ${g.platform.toLowerCase()} / ${g.id}.demo</p><div class="system-actions">${button("Rename", "rename", g.id)}${button("Choose for Home", "select-home", g.id, "gold")}${button("Read reviews", "reviews", g.id, "green")}${button("Delete", "delete", g.id, "pink")}</div>`;
    }
    if (modal === "rename") {
      const g = D.games.find((g) => g.id === modalData.id);
      name = "Rename game";
      body = `<input class="text-input" id="game-name" aria-label="Game name" maxlength="48" style="width:100%" value="${attr(title(g))}">`;
      footer = button("Save name", "save-name", g.id, "gold");
    }
    if (modal === "delete") {
      name = "Delete this demo game?";
      body =
        "<p>The card will disappear from this browser’s demo library. No actual ROM exists here. “Начать заново” restores the complete collection.</p>";
      footer =
        button("Keep game", "close") +
        button("Delete", "delete-confirm", modalData.id, "pink");
    }
    if (modal === "reviews") {
      name = "Adventure reviews";
      body =
        '<div class="message"><strong>Mira · ★★★★★</strong><p>Best enjoyed slowly. There is a lovely surprise past the northern gardens.</p></div><p class="small muted">Example review. In TrainerOS, reading is separate from verified completion requirements for writing.</p>';
    }
    if (modal === "guide-search") {
      name = "Find a companion";
      body = `<input class="text-input" id="guide-query" aria-label="Companion name" style="width:100%" placeholder="Name…" value="${attr(guideSearch)}">`;
      footer = button("Search", "guide-apply", "", "gold");
    }
    if (modal === "attach") {
      name = "Share a moment";
      body = `<button class="cap adventure-card" style="width:100%" data-action="send-snapshot"><img src="${image("valley")}" alt="The glass gardens"><span>The glass gardens</span><small>Send this demo snapshot</small></button>`;
    }
    if (modal === "reset") {
      name = "Start again?";
      body =
        "<p>Reset your demo profile, theme, basket, messages and collection changes. Nothing outside this browser demo is affected.</p>";
      footer =
        button("Cancel", "close") +
        button("Reset demo", "reset-confirm", "", "gold");
    }
    host.innerHTML = modalShell(name, body, footer, size);
    if (modal === "trade" && modalData.exchanging)
      host.querySelector(".modal").classList.add("exchanging");
  }
  function setBackgroundInert(on) {
    ["#tabs", "#faces", "#screen", "#choose", ".chassis-footer"].forEach(
      (sel) => ($(sel).inert = on),
    );
  }
  function slider(id, label, value) {
    return `<div class="slider-row"><label for="${id}">${label}</label><input type="range" id="${id}" data-setting="${id}" min="0" max="100" value="${value}"><output for="${id}">${value}%</output></div>`;
  }
  function settingToggle(label, key, value) {
    return `<div class="setting-line"><span>${label}</span><button class="switch ${value ? "on" : ""}" role="switch" aria-checked="${value}" aria-label="${label}" data-action="toggle" data-value="${key}"></button></div>`;
  }
  function settingsContent() {
    if (category === "Appearance")
      return `<h3>A little more you.</h3><p>Choose the color of your handheld shell.</p><div class="swatches">${Object.entries(
        {
          turquoise: "#297e79",
          red: "#984b51",
          green: "#497c52",
          blue: "#3d739d",
          orange: "#976038",
        },
      )
        .map(
          ([name, color]) =>
            `<button class="swatch ${s.theme === name ? "selected" : ""}" style="background:${color}" aria-label="${name} theme" data-action="theme" data-value="${name}"></button>`,
        )
        .join(
          "",
        )}</div>${settingToggle("Reduce motion", "reduced", s.reduced)}<p>Theme and motion apply to this demo and are remembered on this browser.</p>`;
    if (category === "Sound")
      return `<h3>Sound</h3>${slider("volume", "Volume", s.volume)}${settingToggle("Do not disturb", "dnd", s.dnd)}<p>Illustrative controls. This demo stays silent and does not change device volume.</p>`;
    if (category === "Trainer")
      return `<h3>Your Trainer</h3><div class="row"><div class="avatar">${esc(s.name[0] || "A")}</div><span>${esc(s.name)}</span></div><p>A name that goes with you through your library.</p>${button("Edit profile", "edit-profile", "", "gold")}`;
    if (category === "Library")
      return `<h3>Game storage</h3><p>Games belong in platform folders. TrainerOS discovers them without per-game setup.</p>${button("▣ Game library<small>383 GB free · SD card · In use</small>", "storage-info", "", "green")}<div class="setting-line"><span>Demo library</span><span>${D.games.length - s.hidden.length} games</span></div><p>No folders on your computer are accessed.</p>`;
    if (category === "Saves")
      return `<h3>Your progress</h3>${button("Bloom Trails<small>Latest recovery copy · Today</small>", "save-care", "", "green")}<p>Supported save operations keep a recovery copy. The website only demonstrates that experience.</p>`;
    if (category === "Connections")
      return `<h3>Connections</h3>${settingToggle("Wi-Fi", "wifi", s.wifi)}${settingToggle("Bluetooth", "bluetooth", s.bluetooth)}${settingToggle("Airplane mode", "airplane", s.airplane)}<p>Demo connection state. Your computer’s radios stay unchanged.</p>`;
    if (category === "Communication")
      return `<h3>Communication</h3><div class="setting-line"><span>Demo profile</span><span>${esc(s.name)}</span></div>${settingToggle("Do not disturb", "dnd", s.dnd)}<div class="setting-line"><label for="demo-microphone">Microphone</label><select id="demo-microphone"><option>Default input (demo)</option><option>Headset (demo)</option></select></div><p>No microphone, account or network permission is requested. Calls and invitations are scripted examples.</p>`;
    return "<h3>Made for your adventures.</h3><p>TrainerOS · Interactive website demo</p><p>Original demonstration creatures and environments generated for this project. Interface fonts: Fredoka, Chakra Petch and Bungee, under the SIL Open Font License.</p><p>Original code: GPL-3.0-or-later. No commercial game images, music, ROMs or saves are included.</p>";
  }
  function launch(id) {
    cancelScene(false);
    clearAux();
    gameId = id;
    page = "game";
    modal = null;
    joined = false;
    render();
  }
  function selectHome(id) {
    const g = D.games.find((g) => g.id === id);
    s.selected[g.series] = id;
    persist();
    page = "home";
    face = g.series;
    gameId = null;
    collection = null;
    modal = null;
    render();
  }
  function checkout() {
    const lines = cartItems(),
      total = lines.reduce((a, l) => a + l.item.price * l.qty, 0);
    if (!total || total > s.money) return;
    for (const l of lines)
      s.bag[l.item.name] = (s.bag[l.item.name] || 0) + l.qty;
    s.money -= total;
    cart = {};
    persist();
    render();
    toast("All packed! Your items are in the bag.");
  }
  function heal() {
    if (healing) return;
    healing = true;
    render();
    later(() => {
      s.party.forEach((i) => (s.health[i] = D.creatures[i].max));
      healing = false;
      persist();
      render();
      toast("Everyone is feeling better. Safe travels!");
    }, 2200);
  }
  function sendInvite() {
    openModal("inviting");
    later(() => {
      if (modal !== "inviting") return;
      joined = true;
      if (page !== "game") {
        gameId = s.selected.vale;
        page = "game";
      }
      modal = null;
      render();
      toast("Mira accepted. Your adventure is ready for two.");
    }, 2100);
  }
  function action(type, value) {
    switch (type) {
      case "page":
        go(value);
        break;
      case "face":
        faceTo(value);
        break;
      case "cycle": {
        const i = D.collections.findIndex((c) => c.id === face);
        faceTo(D.collections[(i + Number(value) + 4) % 4].id);
        break;
      }
      case "choose":
        openModal("choose");
        break;
      case "collection":
        collection = value;
        wheelIndex = 0;
        render();
        break;
      case "world-back":
        collection = null;
        render();
        break;
      case "wheel":
        wheelIndex += Number(value);
        render();
        break;
      case "wheel-select":
        wheelIndex = Number(value);
        render();
        break;
      case "wheel-play":
        if (wheelGame()) launch(wheelGame().id);
        break;
      case "wheel-choose":
        if (wheelGame()) selectHome(wheelGame().id);
        break;
      case "play":
        launch(currentGame().id);
        break;
      case "launch":
        launch(value);
        break;
      case "select-home":
        selectHome(value);
        break;
      case "creature":
        selectedCreature = Number(value);
        render();
        break;
      case "favorite": {
        const i = Number(value);
        s.favorites = s.favorites.includes(i)
          ? s.favorites.filter((n) => n !== i)
          : [...s.favorites, i];
        persist();
        render();
        break;
      }
      case "party-detail":
      case "box-detail":
        openModal(type, { i: Number(value) });
        break;
      case "to-box":
        if (s.party.length === 1) {
          toast("Keep one companion in your party.");
          break;
        }
        s.party = s.party.filter((i) => i !== Number(value));
        s.boxes.push(Number(value));
        persist();
        closeModal();
        render();
        break;
      case "to-party":
        s.boxes = s.boxes.filter((i) => i !== Number(value));
        s.party.push(Number(value));
        persist();
        closeModal();
        render();
        break;
      case "shop":
        shop = Number(value);
        render();
        break;
      case "cart-add":
        cart[value] = Math.min(99, (cart[value] || 0) + 1);
        render();
        break;
      case "cart-remove":
        cart[value] = Math.max(0, (cart[value] || 0) - 1);
        render();
        break;
      case "checkout":
        checkout();
        break;
      case "heal":
        heal();
        break;
      case "greet":
        bubble = Number(value);
        render();
        later(() => {
          bubble = -1;
          render();
        }, 2500);
        break;
      case "treat":
        toast("A berry biscuit to share. Everyone comes a little closer.");
        bubble = s.party[0];
        render();
        later(() => {
          bubble = -1;
          render();
        }, 3000);
        break;
      case "play-together":
        startScene("playroom");
        break;
      case "practice":
        battleRound = 0;
        openModal("practice");
        break;
      case "attack":
        battleRound = battleRound >= 3 ? 0 : battleRound + 1;
        modalData.flash = true;
        renderModal();
        later(() => {
          modalData.flash = false;
          if (modal === "practice") renderModal();
        }, 680);
        break;
      case "battle-switch": {
        const i = s.party.indexOf(Number(value));
        [s.party[0], s.party[i]] = [s.party[i], s.party[0]];
        persist();
        openModal("practice");
        break;
      }
      case "trade-example":
        startScene("trade");
        break;
      case "contact":
        contact = value;
        render();
        break;
      case "open-chat":
        cancelScene(false);
        clearAux();
        page = "social";
        face =
          value === "Campfire"
            ? "groups"
            : value === "Lounge"
              ? "communities"
              : "messages";
        contact = value;
        modal = null;
        gameId = null;
        render();
        break;
      case "call":
        call = !call;
        render();
        toast(
          call
            ? "Demo group call connected. No microphone is used."
            : "You left the demo call.",
        );
        break;
      case "mute":
        muted = !muted;
        render();
        break;
      case "emoji": {
        const input = $("#composer input");
        if (input) {
          input.value += " ☺";
          drafts[contact] = input.value;
          input.focus();
        }
        break;
      }
      case "send-snapshot":
        s.messages[contact].push({
          text: "▧ Shared a moment: The glass gardens",
          mine: true,
        });
        persist();
        closeModal();
        render();
        break;
      case "send-invite":
        sendInvite();
        break;
      case "cancel-invite":
        clearAux();
        closeModal();
        break;
      case "game-walk":
        toast("Demo scene · Open Home menu to try an invitation.");
        break;
      case "settings":
        category = "Appearance";
        openModal("settings");
        break;
      case "setting-category":
        category = value;
        renderModal();
        break;
      case "theme":
        s.theme = value;
        persist();
        theme();
        renderModal();
        break;
      case "toggle":
        if (value === "scanlines") {
          scanlines = !scanlines;
        } else {
          s[value] = !s[value];
          if (value === "airplane" && s.airplane) {
            s.wifi = false;
            s.bluetooth = false;
          }
          if ((value === "wifi" || value === "bluetooth") && s[value])
            s.airplane = false;
          persist();
        }
        render();
        break;
      case "save-profile": {
        const name = $("#trainer-name").value.trim();
        if (!name) {
          toast("Please enter a Trainer name.");
          break;
        }
        s.name = name.slice(0, 24);
        persist();
        closeModal();
        render();
        break;
      }
      case "properties":
      case "rename":
      case "delete":
      case "reviews":
        openModal(type, { id: value });
        break;
      case "save-name": {
        const name = $("#game-name").value.trim();
        if (!name) {
          toast("Please enter a name.");
          break;
        }
        s.renames[value] = name.slice(0, 48);
        persist();
        closeModal();
        render();
        break;
      }
      case "delete-confirm": {
        const g = D.games.find((g) => g.id === value);
        if (collectionGames(g.series).length <= 1) {
          toast(
            "Keep one game per demo collection. Other games can be removed.",
          );
          break;
        }
        s.hidden.push(value);
        if (s.selected[g.series] === value)
          s.selected[g.series] = collectionGames(g.series)[0].id;
        persist();
        closeModal();
        render();
        break;
      }
      case "exit-confirm": {
        const g = currentGame();
        gameId = null;
        joined = false;
        page = "home";
        face = g.series;
        s.selected[g.series] = g.id;
        persist();
        closeModal();
        render();
        break;
      }
      case "journey":
        go("trainer", "journey");
        break;
      case "guide-apply":
        guideSearch = $("#guide-query").value.trim();
        closeModal();
        render();
        break;
      case "guide-clear":
        guideSearch = "";
        render();
        break;
      case "backup":
        toast("Demo recovery copy created just now.");
        closeModal();
        break;
      case "storage-info":
        toast(
          "Demo library · gba / gbc / snes / psx. No local files accessed.",
        );
        break;
      case "close":
        closeModal();
        break;
      case "reset-confirm":
        cancelScene(false);
        clearAux();
        s = defaults();
        cart = {};
        Object.keys(drafts).forEach((k) => delete drafts[k]);
        call = false;
        joined = false;
        scanlines = false;
        guideSearch = "";
        search = "";
        persist();
        go("home");
        break;
      default:
        if (
          [
            "system",
            "home-menu",
            "invite",
            "online",
            "nearby",
            "picture",
            "exit",
            "notifications",
            "modes",
            "edit-profile",
            "link",
            "trade",
            "battle-team",
            "bag",
            "save-care",
            "guide-search",
            "attach",
          ].includes(type)
        )
          openModal(type);
    }
  }
  function exampleForPage() {
    if (modal === "settings")
      return { type: "theme", label: "Смена темы корпуса" };
    if (page === "worlds")
      return { type: "library", label: "От коллекции до игры" };
    if (page === "companions") {
      const m = {
        guide: ["guide", "Знакомство со спутником"],
        party: ["party", "Из команды в хранилище"],
        boxes: ["party", "Из команды в хранилище"],
        center: ["heal", "Лечение команды"],
        playroom: ["playroom", "Прогулка и общение"],
        shops: ["shop", "Несколько товаров за раз"],
      };
      return { type: m[face][0], label: m[face][1] };
    }
    if (page === "trainer")
      return { type: "journey", label: "Профиль, воспоминания, награды" };
    return { type: "invite", label: "Приглашение друга в игру" };
  }
  function cancelScene(announce = true) {
    sceneToken++;
    sceneTimers.forEach(clearTimeout);
    sceneTimers = [];
    if (scene) {
      scene = null;
      if (announce) toast("Example stopped. You are back in control.");
    }
    document
      .querySelectorAll(".demo-highlight")
      .forEach((el) => el.classList.remove("demo-highlight"));
    explanation();
  }
  function startScene(type) {
    cancelScene(false);
    clearAux();
    healing = false;
    const token = sceneToken;
    scene = { type, status: "Пример запускается…" };
    let elapsed = 0;
    const step = (delay, status, fn, selector) => {
      elapsed += delay;
      sceneTimers.push(
        setTimeout(() => {
          if (token !== sceneToken) return;
          scene.status = status;
          fn?.();
          render();
          if (selector) $(selector)?.classList.add("demo-highlight");
          explanation();
        }, elapsed),
      );
    };
    const place = (p, f) => {
      page = p;
      face = f;
      collection = null;
      modal = null;
      gameId = null;
    };
    if (type === "invite") {
      step(
        0,
        "1 / 5 · Разговор уже открыт — можно сразу написать.",
        () => {
          place("social", "messages");
          contact = "Mira";
        },
        "#composer",
      );
      step(
        1900,
        "2 / 5 · Приглашение доступно из разговора и меню Home.",
        () => {
          modal = "invite";
        },
      );
      step(
        2000,
        "3 / 5 · Выбираем Online friend. Адреса серверов вводить не нужно.",
        () => {
          modal = "online";
        },
      );
      step(
        2000,
        "4 / 5 · Друг получает приглашение и сам решает, присоединяться ли.",
        () => {
          modal = "inviting";
        },
      );
      step(
        2300,
        "5 / 5 · Принято. Игровая группа готова; разговор остаётся на связи.",
        () => {
          modal = null;
          gameId = s.selected.vale;
          page = "game";
          joined = true;
          call = true;
        },
      );
    } else if (type === "library") {
      step(
        0,
        "1 / 3 · Игры собраны по коллекциям. Остальные живут в Multiverse.",
        () => place("worlds", "collections"),
      );
      step(
        1700,
        "2 / 3 · Названия листаются по кругу; справа — подробности.",
        () => {
          collection = "vale";
          wheelIndex = 0;
        },
      );
      step(1700, "3 / 3 · Один Play — и вы в игре.", () => {
        gameId = "bloom";
        page = "game";
      });
    } else if (type === "guide") {
      step(0, "1 / 3 · Всё о спутнике помещается на одном экране.", () =>
        place("companions", "guide"),
      );
      step(
        1500,
        "2 / 3 · Выбор в списке сразу меняет иллюстрацию и характеристики.",
        () => {
          selectedCreature = 1;
        },
      );
      step(1700, "3 / 3 · Любимого спутника можно отметить звёздочкой.", () => {
        if (!s.favorites.includes(1)) s.favorites.push(1);
        persist();
      });
    } else if (type === "party") {
      step(0, "1 / 3 · Выбираем участника команды.", () =>
        place("companions", "party"),
      );
      step(1700, "2 / 3 · Действие рядом с выбранным спутником.", () => {
        modal = "party-detail";
        modalData = { i: s.party[0] };
      });
      step(
        1900,
        "3 / 3 · Перемещён в хранилище. Состав команды обновлён.",
        () => {
          if (s.party.length > 1) {
            s.boxes.push(s.party.shift());
            persist();
          }
          modal = null;
          face = "boxes";
        },
      );
    } else if (type === "heal") {
      step(0, "1 / 3 · На остановке видна вся команда.", () => {
        place("companions", "center");
        s.party.forEach(
          (i) => (s.health[i] = Math.floor(D.creatures[i].max * 0.55)),
        );
      });
      step(1800, "2 / 3 · Немного заботы — все отдыхают вместе.", () => {
        healing = true;
      });
      step(2600, "3 / 3 · Команда здорова и готова снова в путь.", () => {
        healing = false;
        s.party.forEach((i) => (s.health[i] = D.creatures[i].max));
        persist();
      });
    } else if (type === "shop") {
      step(
        0,
        "1 / 4 · Магазин сразу показывает товары, без приветственного экрана.",
        () => {
          place("companions", "shops");
          shop = 0;
          cart = {};
          if (s.money < 420) s.money = 2400;
        },
      );
      step(1500, "2 / 4 · Три тоника одним заказом.", () => {
        cart["0:0"] = 3;
      });
      step(1600, "3 / 4 · Добавляем печенье в ту же корзину.", () => {
        cart["0:1"] = 1;
      });
      step(
        1800,
        "4 / 4 · Вся корзина оплачена. В сумке четыре предмета.",
        () => {
          checkout();
          modal = "bag";
        },
      );
    } else if (type === "playroom") {
      step(0, "1 / 3 · Спутники свободно гуляют по полянке.", () =>
        place("companions", "playroom"),
      );
      step(2300, "2 / 3 · На зов откликается выбранный спутник.", () => {
        bubble = s.party[0];
      });
      step(
        2500,
        "3 / 3 · Кто-то играет, а кто-то предпочитает вздремнуть.",
        () => {
          bubble = s.party[1] ?? s.party[0];
        },
      );
    } else if (type === "trade") {
      step(0, "1 / 3 · Видны оба участника и оба предложения.", () => {
        place("companions", "center");
        modal = "trade";
        modalData = {};
      });
      step(
        2200,
        "2 / 3 · Обмен начинается после подтверждения обеих сторон.",
        () => {
          modal = "trade";
          modalData = { exchanging: true };
        },
      );
      step(
        2200,
        "3 / 3 · Обмен завершён. В этом примере реальное сохранение не меняется.",
        () => {
          modal = null;
          toast("Mira: Thank you! Take good care of your new friend.");
        },
      );
    } else if (type === "theme") {
      step(0, "1 / 3 · Все настройки в одной двухпанельной форме.", () => {
        modal = "settings";
        category = "Appearance";
      });
      step(1700, "2 / 3 · Другой цвет — тот же знакомый корпус.", () => {
        s.theme = "blue";
        persist();
      });
      step(1700, "3 / 3 · Тема применяется сразу и запоминается.", () => {
        s.theme = "turquoise";
        persist();
      });
    } else {
      step(0, "1 / 3 · Личный профиль объединяет ваши приключения.", () =>
        place("trainer", "profile"),
      );
      step(1700, "2 / 3 · Путешествие сохраняет важные моменты.", () => {
        face = "journey";
      });
      step(1800, "3 / 3 · Достижения находятся рядом с историей.", () => {
        face = "ra";
      });
    }
    sceneTimers.push(
      setTimeout(() => {
        if (token === sceneToken) {
          scene = null;
          healing = false;
          bubble = -1;
          render();
          $("#scene-status").textContent =
            "Пример завершён. Можно повторить или продолжить самому.";
        }
      }, elapsed + 2600),
    );
    explanation();
  }
  document.addEventListener("click", (e) => {
    const target = e.target.closest("[data-action]");
    if (!target) return;
    if (scene) cancelScene(false);
    action(target.dataset.action, target.dataset.value || "");
  });
  $("#choose").addEventListener("click", () => {
    cancelScene(false);
    openModal("choose");
  });
  $("#example").addEventListener("click", () => {
    if (scene) {
      cancelScene();
      healing = false;
      render();
    } else startScene(exampleForPage().type);
  });
  $("#reset").addEventListener("click", () => {
    cancelScene(false);
    openModal("reset");
  });
  $(".wordmark").addEventListener("click", (e) => {
    e.preventDefault();
    go("home");
  });
  $("#overlay").addEventListener("click", (e) => {
    if (e.target === $("#overlay")) {
      cancelScene(false);
      closeModal();
    }
  });
  document.addEventListener("submit", (e) => {
    if (e.target.id === "composer") {
      e.preventDefault();
      const input = e.target.elements.message,
        text = input.value.trim();
      if (!text) return;
      const recipient = contact;
      s.messages[recipient].push({ text: text.slice(0, 500), mine: true });
      s.messages[recipient] = s.messages[recipient].slice(-30);
      drafts[recipient] = "";
      persist();
      render();
      $("#composer input")?.focus();
      later(() => {
        s.messages[recipient].push({
          text: "Sounds good! This is a demo reply — see you in the valley ✨",
          mine: false,
        });
        persist();
        if (page === "social") render();
      }, 1200);
    }
    if (e.target.id === "search-form") {
      e.preventDefault();
      search = e.target.elements.query.value.trim();
      render();
    }
  });
  document.addEventListener("input", (e) => {
    if (e.target.matches("#composer input")) drafts[contact] = e.target.value;
    if (e.target.dataset.setting) {
      s[e.target.dataset.setting] = Number(e.target.value);
      e.target.nextElementSibling.textContent = e.target.value + "%";
      persist();
    }
  });
  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") {
      cancelScene(false);
      clearAux();
      healing = false;
      if (modal) closeModal();
      else if (page === "game") openModal("home-menu");
      else if (page === "worlds" && collection) {
        collection = null;
        render();
      }
      return;
    }
    if (modal && e.key === "Tab") {
      const items = [
        ...$("#overlay").querySelectorAll(
          "button:not([disabled]),input,select,a[href]",
        ),
      ];
      const first = items[0],
        last = items[items.length - 1];
      if (e.shiftKey && document.activeElement === first) {
        e.preventDefault();
        last?.focus();
      } else if (!e.shiftKey && document.activeElement === last) {
        e.preventDefault();
        first?.focus();
      }
    }
  });
  function resize() {
    const ratio = $("#viewport").clientWidth / 1280;
    $("#device").style.transform = `scale(${ratio})`;
  }
  new ResizeObserver(resize).observe($("#viewport"));
  window.addEventListener("resize", resize);
  render();
  resize();
})();
