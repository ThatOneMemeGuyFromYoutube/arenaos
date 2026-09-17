/* Windows NT 3.51 recreation — Program Manager shell, from memory */
(() => {
  const $ = (s, r = document) => r.querySelector(s);
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  let z = 10;
  const windows = [];
  let active = null;

  const beep = (f = 880, d = 0.08) => {
    try {
      const c = new (window.AudioContext || window.webkitAudioContext)();
      const o = c.createOscillator();
      const g = c.createGain();
      o.frequency.value = f;
      o.type = "square";
      g.gain.value = 0.04;
      o.connect(g);
      g.connect(c.destination);
      o.start();
      o.stop(c.currentTime + d);
    } catch (_) {}
  };

  async function boot() {
    const t = $("#boot-text");
    const lines = [
      "PhoenixBIOS 4.0 Release 6.0",
      "Copyright 1985-1995 Phoenix Technologies Ltd.",
      "All Rights Reserved",
      "",
      "CPU = Intel Pentium 90MHz",
      "640K Base Memory, 15360K Extended",
      "",
      "Detecting IDE Primary Master  ... WDC AC21200H",
      "Detecting IDE Primary Slave   ... None",
      "",
      "OS Loader V3.51",
      "",
      "  Microsoft (R) Windows NT (TM) Workstation Version 3.51",
      "  (C) 1981-1995 Microsoft Corp.",
      "",
      "Loading...",
    ];
    for (const line of lines) {
      t.textContent += line + "\n";
      await sleep(90);
    }
    await sleep(500);
    $("#boot").classList.add("hidden");
    $("#splash").classList.remove("hidden");
    await sleep(1800);
    $("#splash").classList.add("hidden");
    $("#logon").classList.remove("hidden");
  }

  $("#btn-ok").onclick = () => {
    beep(1200, 0.05);
    $("#logon").classList.add("hidden");
    $("#desktop").classList.remove("hidden");
    startShell();
  };
  $("#btn-cancel").onclick = () => beep(400, 0.12);
  $("#btn-shutdown").onclick = () => {
    document.body.innerHTML =
      '<div style="background:#000;color:#c0c0c0;height:100%;padding:24px;font-family:monospace">It is now safe to turn off your computer.</div>';
  };
  $("#btn-help").onclick = () => alert("Press Ctrl+Alt+Del to log on.\n\nThis workstation is ARENA-NTWS.");

  function iconSvg(kind) {
    const c = {
      fm: "#f4e04d",
      cmd: "#000",
      cp: "#c0c0c0",
      note: "#fff",
      calc: "#c0c0c0",
      clock: "#fff",
      paint: "#fff",
      help: "#00a",
      game: "#0a0",
      user: "#44a",
      disk: "#aaa",
      event: "#fa0",
      mail: "#fff",
      term: "#000",
      clip: "#eee",
      print: "#ddd",
    }[kind] || "#00a";
    return `<svg class="pic" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><rect width="32" height="32" fill="${c}" stroke="#000"/><text x="16" y="20" text-anchor="middle" font-size="8" fill="${kind === "cmd" || kind === "term" ? "#0f0" : "#000"}" font-family="sans-serif">${kind.slice(0, 3).toUpperCase()}</text></svg>`;
  }

  function makeWindow(opts) {
    const el = document.createElement("div");
    el.className = "window";
    el.style.left = (opts.x || 40) + "px";
    el.style.top = (opts.y || 30) + "px";
    el.style.width = (opts.w || 420) + "px";
    el.style.height = (opts.h || 300) + "px";
    el.style.zIndex = ++z;
    el.innerHTML = `
      <div class="win-chrome">
        <span class="sysbtn" title="Control menu">-</span>
        <span class="caption">${opts.title}</span>
        <span class="wbtn min">▼</span>
        <span class="wbtn max">▲</span>
      </div>
      ${opts.menu ? `<div class="menu">${opts.menu.map((m) => `<span data-m="${m}">${m}</span>`).join("")}</div>` : ""}
      <div class="client ${opts.gray ? "gray" : ""}"></div>
      ${opts.status !== false ? `<div class="status">${opts.status || ""}</div>` : ""}
      <div class="resize"></div>
    `;
    $("#windows").appendChild(el);
    const rec = { el, title: opts.title, minimized: false };
    windows.push(rec);
    focusWin(el);
    el.addEventListener("mousedown", () => focusWin(el));
    drag(el, el.querySelector(".win-chrome"));
    el.querySelector(".max").onclick = (e) => {
      e.stopPropagation();
      el.classList.toggle("maximized");
    };
    el.querySelector(".min").onclick = (e) => {
      e.stopPropagation();
      el.style.display = "none";
      rec.minimized = true;
    };
    el.querySelector(".sysbtn").ondblclick = () => closeWin(el);
    const rs = el.querySelector(".resize");
    rs.onmousedown = (e) => {
      e.preventDefault();
      const r = el.getBoundingClientRect();
      const sx = e.clientX, sy = e.clientY, sw = r.width, sh = r.height;
      const mv = (ev) => {
        el.style.width = Math.max(220, sw + ev.clientX - sx) + "px";
        el.style.height = Math.max(120, sh + ev.clientY - sy) + "px";
      };
      const up = () => {
        window.removeEventListener("mousemove", mv);
        window.removeEventListener("mouseup", up);
      };
      window.addEventListener("mousemove", mv);
      window.addEventListener("mouseup", up);
    };
    if (opts.menu) bindMenus(el, opts.cmds || {});
    rec.client = el.querySelector(".client");
    rec.statusEl = el.querySelector(".status");
    rec.close = () => closeWin(el);
    return rec;
  }

  function closeWin(el) {
    const i = windows.findIndex((w) => w.el === el);
    if (i >= 0) windows.splice(i, 1);
    el.remove();
  }
  function focusWin(el) {
    document.querySelectorAll(".window").forEach((w) => w.classList.add("inactive"));
    el.classList.remove("inactive");
    el.style.zIndex = ++z;
    active = el;
    const rec = windows.find((w) => w.el === el);
    if (rec) rec.minimized = false;
    el.style.display = "";
  }

  function drag(el, handle) {
    handle.onmousedown = (e) => {
      if (e.target.closest(".wbtn, .sysbtn")) return;
      if (el.classList.contains("maximized")) return;
      const r = el.getBoundingClientRect();
      const ox = e.clientX - r.left, oy = e.clientY - r.top;
      const mv = (ev) => {
        el.style.left = ev.clientX - ox + "px";
        el.style.top = ev.clientY - oy + "px";
      };
      const up = () => {
        window.removeEventListener("mousemove", mv);
        window.removeEventListener("mouseup", up);
      };
      window.addEventListener("mousemove", mv);
      window.addEventListener("mouseup", up);
    };
  }

  function bindMenus(el, cmds) {
    el.querySelectorAll(".menu > span").forEach((sp) => {
      sp.onclick = (e) => {
        e.stopPropagation();
        document.querySelectorAll(".menu-drop").forEach((d) => d.remove());
        const name = sp.dataset.m;
        const items = cmds[name];
        if (!items) return;
        const drop = document.createElement("div");
        drop.className = "menu-drop";
        const r = sp.getBoundingClientRect();
        const wr = el.getBoundingClientRect();
        drop.style.left = r.left - wr.left + "px";
        drop.style.top = r.bottom - wr.top + "px";
        items.forEach((it) => {
          if (it === "-") {
            const s = document.createElement("div");
            s.className = "sep";
            drop.appendChild(s);
            return;
          }
          const d = document.createElement("div");
          d.textContent = it.label;
          d.onclick = (ev) => {
            ev.stopPropagation();
            drop.remove();
            it.fn && it.fn();
          };
          drop.appendChild(d);
        });
        el.appendChild(drop);
        sp.classList.add("open");
        const hide = () => {
          drop.remove();
          sp.classList.remove("open");
          document.removeEventListener("mousedown", hide);
        };
        setTimeout(() => document.addEventListener("mousedown", hide), 0);
      };
    });
  }

  function icons(list, host, dbl) {
    host.innerHTML = "";
    host.classList.add("icon-grid");
    list.forEach((it) => {
      const d = document.createElement("div");
      d.className = "icon";
      d.innerHTML = iconSvg(it.icon) + `<div class="lab">${it.name}</div>`;
      d.onclick = () => {
        host.querySelectorAll(".icon").forEach((x) => x.classList.remove("sel"));
        d.classList.add("sel");
      };
      d.ondblclick = () => dbl(it);
      host.appendChild(d);
    });
  }

  function startShell() {
    openProgman();
  }

  function openProgman() {
    const w = makeWindow({
      title: "Program Manager",
      x: 16, y: 12, w: Math.min(780, innerWidth - 40), h: Math.min(520, innerHeight - 40),
      gray: true,
      menu: ["File", "Options", "Window", "Help"],
      status: "Windows NT Workstation 3.51",
      cmds: {
        File: [
          { label: "New...", fn: () => beep() },
          { label: "Open", fn: () => {} },
          { label: "Move...", fn: () => {} },
          { label: "Copy...", fn: () => {} },
          { label: "Delete", fn: () => {} },
          { label: "Properties...", fn: () => {} },
          "-",
          { label: "Run...", fn: runDlg },
          "-",
          { label: "Exit Windows NT...", fn: () => $("#btn-shutdown").onclick() },
        ],
        Options: [
          { label: "Auto Arrange", fn: () => {} },
          { label: "Minimize on Use", fn: () => {} },
          { label: "Save Settings on Exit", fn: () => {} },
        ],
        Window: [
          { label: "Cascade", fn: () => {} },
          { label: "Tile", fn: () => {} },
          { label: "Arrange Icons", fn: () => {} },
        ],
        Help: [
          { label: "Contents", fn: openHelp },
          { label: "Search for Help on...", fn: openHelp },
          { label: "How to Use Help", fn: openHelp },
          "-",
          { label: "About Program Manager...", fn: about },
        ],
      },
    });
    const mdi = document.createElement("div");
    mdi.className = "mdi";
    w.client.appendChild(mdi);

    const groups = [
      {
        name: "Main", x: 8, y: 8, w: 360, h: 220,
        items: [
          { name: "File Manager", icon: "fm", run: openFileManager },
          { name: "Control Panel", icon: "cp", run: openControlPanel },
          { name: "Print Manager", icon: "print", run: () => stub("Print Manager", "No printers are installed.") },
          { name: "Command Prompt", icon: "cmd", run: openCmd },
          { name: "Clipboard Viewer", icon: "clip", run: openClip },
          { name: "Windows NT Setup", icon: "cp", run: () => stub("Windows NT Setup", "Setup cannot run while the system is in use.") },
        ],
      },
      {
        name: "Accessories", x: 380, y: 8, w: 360, h: 240,
        items: [
          { name: "Notepad", icon: "note", run: openNotepad },
          { name: "Write", icon: "note", run: openWrite },
          { name: "Paintbrush", icon: "paint", run: openPaint },
          { name: "Calculator", icon: "calc", run: openCalc },
          { name: "Clock", icon: "clock", run: openClock },
          { name: "Terminal", icon: "term", run: openCmd },
          { name: "Character Map", icon: "note", run: openCharmap },
          { name: "Chat", icon: "mail", run: () => stub("Chat", "WinPopup/Chat requires NetBIOS.") },
        ],
      },
      {
        name: "Games", x: 8, y: 240, w: 280, h: 160,
        items: [
          { name: "Solitaire", icon: "game", run: openSol },
          { name: "Minesweeper", icon: "game", run: openMine },
        ],
      },
      {
        name: "Administrative Tools", x: 300, y: 260, w: 420, h: 180,
        items: [
          { name: "User Manager", icon: "user", run: openUsers },
          { name: "Disk Administrator", icon: "disk", run: openDisk },
          { name: "Event Viewer", icon: "event", run: openEvents },
          { name: "Performance Monitor", icon: "cp", run: openPerf },
          { name: "Backup", icon: "disk", run: () => stub("Ntbackup", "Insert tape in \\Tape0") },
          { name: "Windows NT Diagnostics", icon: "cp", run: openWinmsd },
        ],
      },
    ];

    groups.forEach((g) => {
      const gw = document.createElement("div");
      gw.className = "group-win";
      gw.style.left = g.x + "px";
      gw.style.top = g.y + "px";
      gw.style.width = g.w + "px";
      gw.style.height = g.h + "px";
      gw.innerHTML = `<div class="sb">${g.name}</div><div class="client gray"></div>`;
      mdi.appendChild(gw);
      icons(g.items, gw.querySelector(".client"), (it) => it.run());
      drag(gw, gw.querySelector(".sb"));
    });
  }

  function stub(title, msg) {
    const w = makeWindow({ title, x: 120, y: 80, w: 380, h: 160, gray: true, status: false });
    w.client.innerHTML = `<div style="padding:16px">${msg}</div><div class="dlg-btns"><button class="btn default">OK</button></div>`;
    w.client.querySelector("button").onclick = w.close;
  }

  function about() {
    const w = makeWindow({
      title: "About Program Manager",
      x: 180, y: 90, w: 420, h: 240, gray: true, status: false,
    });
    w.client.innerHTML = `<div class="about">
      <div class="logon-icon">NT</div>
      <div>
        <b>Microsoft Windows NT</b><br>
        Version 3.51 (Build 1057)<br>
        Copyright © 1981-1995 Microsoft Corp.<br><br>
        This product is licensed to:<br>
        <b>ARENA-NTWS / Administrator</b><br><br>
        Memory: 16,384 KB RAM<br>
        System Resources: 86% free
      </div>
    </div>
    <div class="dlg-btns"><button class="btn default">OK</button></div>`;
    w.client.querySelector("button").onclick = w.close;
  }

  function openHelp() {
    const w = makeWindow({
      title: "Windows NT Help",
      x: 80, y: 40, w: 480, h: 360,
      menu: ["File", "Edit", "Bookmark", "Help"],
      status: "WINNT.HLP",
    });
    w.client.innerHTML = `<div class="help-body">
      <h1>Windows NT Workstation 3.51</h1>
      <p>The Program Manager is the shell. Double-click a program item to start it.</p>
      <p><b>Ctrl+Esc</b> opens Task List. There is no Start menu and no taskbar — those arrive with the Shell Update Release / 4.0.</p>
      <p>This recreation is an impression from memory: 3-D gray chrome, navy title bars, MDI groups, File Manager, and the NT administrative tools.</p>
      <ul>
        <li>HAL: Arena PC</li>
        <li>Kernel: AI-NT 3.51</li>
        <li>Win32 subsystem present</li>
      </ul>
    </div>`;
  }

  function openNotepad() {
    const w = makeWindow({
      title: "Untitled - Notepad",
      x: 60, y: 50, w: 480, h: 320,
      menu: ["File", "Edit", "Search", "Help"],
      status: "Ln 1",
    });
    w.client.innerHTML = `<textarea class="notepad" spellcheck="false">Windows NT Workstation 3.51

Build 1057
Service Pack: none

This notepad is the Win32 Notepad.exe stand-in.
Word wrap was an option under Edit.</textarea>`;
    const ta = w.client.querySelector("textarea");
    bindMenus(w.el, {});
    w.el.querySelectorAll(".menu > span")[0].onclick = null;
    const cmds = {
      File: [
        { label: "New", fn: () => (ta.value = "") },
        { label: "Open...", fn: () => {} },
        { label: "Save", fn: () => download("UNTITLED.TXT", ta.value) },
        { label: "Save As...", fn: () => download("UNTITLED.TXT", ta.value) },
        "-",
        { label: "Print...", fn: () => print() },
        "-",
        { label: "Exit", fn: w.close },
      ],
      Edit: [
        { label: "Undo", fn: () => document.execCommand("undo") },
        "-",
        { label: "Cut", fn: () => document.execCommand("cut") },
        { label: "Copy", fn: () => document.execCommand("copy") },
        { label: "Paste", fn: () => document.execCommand("paste") },
      ],
      Search: [{ label: "Find...", fn: () => {} }],
      Help: [{ label: "About Notepad...", fn: about }],
    };
    // rebind
    w.el.querySelector(".menu").remove();
    const m = document.createElement("div");
    m.className = "menu";
    m.innerHTML = ["File", "Edit", "Search", "Help"].map((x) => `<span data-m="${x}">${x}</span>`).join("");
    w.el.querySelector(".win-chrome").after(m);
    bindMenus(w.el, cmds);
  }

  function openWrite() {
    const w = makeWindow({
      title: "Write - [Untitled]",
      x: 50, y: 40, w: 520, h: 360,
      menu: ["File", "Edit", "Find", "Character", "Paragraph", "Document", "Help"],
      status: "Page 1",
    });
    w.client.innerHTML = `<textarea class="notepad" style="font-family: 'Times New Roman', serif; font-size:14px" spellcheck="false">Windows Write

A simple rich-text precursor. NT 3.51 still shipped Write.exe rather than WordPad (WordPad is a 4.0 / Win95 piece).</textarea>`;
  }

  function openCalc() {
    const w = makeWindow({ title: "Calculator", x: 200, y: 80, w: 230, h: 280, gray: true, status: false });
    let acc = "0", op = null, fresh = true;
    w.client.innerHTML = `<div class="calc"><div class="calc-disp">0</div><div class="calc-keys"></div></div>`;
    const disp = w.client.querySelector(".calc-disp");
    const keys = ["C", "CE", "BS", "/", "7", "8", "9", "*", "4", "5", "6", "-", "1", "2", "3", "+", "±", "0", ".", "="];
    const host = w.client.querySelector(".calc-keys");
    keys.forEach((k) => {
      const b = document.createElement("button");
      b.className = "btn";
      b.textContent = k;
      b.onclick = () => {
        if (/\d/.test(k) || k === ".") {
          if (fresh) { acc = k === "." ? "0." : k; fresh = false; }
          else acc += k;
        } else if (k === "C" || k === "CE") { acc = "0"; op = null; fresh = true; }
        else if (k === "BS") acc = acc.slice(0, -1) || "0";
        else if (k === "±") acc = String(-parseFloat(acc));
        else if (k === "=") { if (op) acc = String(op(parseFloat(acc))); op = null; fresh = true; }
        else {
          const n = parseFloat(acc);
          const fns = {
            "+": (x) => n + x, "-": (x) => n - x, "*": (x) => n * x, "/": (x) => n / x,
          };
          op = fns[k];
          fresh = true;
        }
        disp.textContent = acc;
      };
      host.appendChild(b);
    });
  }

  function openClock() {
    const w = makeWindow({ title: "Clock", x: 240, y: 60, w: 240, h: 280, menu: ["Settings", "Help"], status: false });
    w.client.classList.add("gray");
    w.client.innerHTML = `<div class="clock-face" id="cf-${z}"></div><div style="text-align:center" class="clk-digital"></div>`;
    const face = w.client.querySelector(".clock-face");
    const dig = w.client.querySelector(".clk-digital");
    const hour = document.createElement("div");
    hour.className = "hand";
    hour.style.width = "4px"; hour.style.height = "50px"; hour.style.marginLeft = "-2px"; hour.style.marginTop = "-50px";
    const min = document.createElement("div");
    min.className = "hand";
    min.style.width = "2px"; min.style.height = "70px"; min.style.marginLeft = "-1px"; min.style.marginTop = "-70px";
    const sec = document.createElement("div");
    sec.className = "hand";
    sec.style.width = "1px"; sec.style.height = "78px"; sec.style.marginLeft = "0"; sec.style.marginTop = "-78px"; sec.style.background = "#c00";
    face.append(hour, min, sec);
    const tick = () => {
      const d = new Date();
      const s = d.getSeconds(), m = d.getMinutes(), h = d.getHours() % 12;
      sec.style.transform = `rotate(${s * 6}deg)`;
      min.style.transform = `rotate(${m * 6 + s * 0.1}deg)`;
      hour.style.transform = `rotate(${h * 30 + m * 0.5}deg)`;
      dig.textContent = d.toLocaleTimeString();
    };
    tick();
    setInterval(tick, 1000);
  }

  function openPaint() {
    const w = makeWindow({
      title: "Paintbrush - [Untitled]",
      x: 40, y: 30, w: 560, h: 400,
      menu: ["File", "Edit", "View", "Text", "Options", "Help"],
      status: "Untitled",
    });
    w.client.innerHTML = `<div class="paint-bar">
      <button class="btn" data-c="#000">Blk</button>
      <button class="btn" data-c="#000080">Nvy</button>
      <button class="btn" data-c="#800000">Mar</button>
      <button class="btn" data-c="#008000">Grn</button>
      <button class="btn" data-c="#fff">Wht</button>
    </div><canvas id="paint" width="520" height="300"></canvas>`;
    const cv = w.client.querySelector("canvas");
    const ctx = cv.getContext("2d");
    ctx.fillStyle = "#fff";
    ctx.fillRect(0, 0, cv.width, cv.height);
    let col = "#000", down = false;
    w.client.querySelectorAll("[data-c]").forEach((b) => (b.onclick = () => (col = b.dataset.c)));
    const pos = (e) => {
      const r = cv.getBoundingClientRect();
      return { x: e.clientX - r.left, y: e.clientY - r.top };
    };
    cv.onmousedown = (e) => { down = true; const p = pos(e); ctx.beginPath(); ctx.moveTo(p.x, p.y); };
    cv.onmouseup = () => (down = false);
    cv.onmousemove = (e) => {
      if (!down) return;
      const p = pos(e);
      ctx.strokeStyle = col;
      ctx.lineWidth = 2;
      ctx.lineTo(p.x, p.y);
      ctx.stroke();
    };
  }

  function openFileManager() {
    const w = makeWindow({
      title: "File Manager",
      x: 30, y: 20, w: 640, h: 400,
      menu: ["File", "Disk", "Tree", "View", "Options", "Window", "Help"],
      status: "C:\\WINNT35",
    });
    const tree = `[-] C:\\
  [-] WINNT35
        SYSTEM32
        SYSTEM
        REPAIR
        CONFIG
        PROFILES
  [-] USERS
        ADMINI~1
  [-] TEMP
[-] D:\\
      I386`;
    const files = [
      "NTOSKRNL.EXE   812,032  05/26/95",
      "NTDLL.DLL      307,200  05/26/95",
      "KERNEL32.DLL   337,408  05/26/95",
      "USER32.DLL     332,800  05/26/95",
      "GDI32.DLL      107,520  05/26/95",
      "PROGMAN.EXE    115,200  05/26/95",
      "WINFILE.EXE    174,080  05/26/95",
      "CMD.EXE         99,328  05/26/95",
      "NOTEPAD.EXE     45,056  05/26/95",
      "WINMSD.EXE      78,336  05/26/95",
      "EXPLORER.EXE        —   (not in 3.51)",
    ].join("\n");
    w.client.innerHTML = `<div class="fm"><pre class="tree">${tree}</pre><pre class="files">${files}</pre></div>`;
  }

  function openCmd() {
    const w = makeWindow({
      title: "Command Prompt",
      x: 70, y: 70, w: 560, h: 320,
      status: "C:\\WINNT35\\SYSTEM32\\CMD.EXE",
    });
    const host = document.createElement("div");
    host.className = "prompt";
    w.client.appendChild(host);
    host.style.background = "#000";
    host.style.color = "#c0c0c0";
    let buf = "Microsoft(R) Windows NT(TM)\n(C) Copyright 1985-1995 Microsoft Corp.\n\nC:\\>";
    const render = () => {
      host.innerHTML = "";
      const pre = document.createElement("span");
      pre.textContent = buf;
      const inp = document.createElement("input");
      host.append(pre, inp);
      inp.focus();
      inp.onkeydown = (e) => {
        if (e.key !== "Enter") return;
        const cmd = inp.value;
        buf += cmd + "\n" + runCmd(cmd) + "C:\\>";
        render();
      };
    };
    const runCmd = (c) => {
      const x = c.trim().toLowerCase();
      if (x === "ver") return "Windows NT Version 3.51\n\n";
      if (x === "dir") return " Directory of C:\\\n\n05/26/95  01:30p    <DIR>          WINNT35\n05/26/95  01:30p    <DIR>          USERS\n               2 File(s)          0 bytes\n\n";
      if (x === "help" || x === "?") return "Internal commands: DIR VER CLS HELP EXIT SET DATE TIME\n\n";
      if (x === "cls") { buf = ""; return ""; }
      if (x === "exit") { w.close(); return ""; }
      if (x.startsWith("echo ")) return c.slice(5) + "\n";
      if (x === "set") return "OS=Windows_NT\nPROCESSOR_ARCHITECTURE=x86\nSystemRoot=C:\\WINNT35\nUSERNAME=Administrator\n\n";
      if (!x) return "";
      return `'${c}' is not recognized as an internal or external command,\noperable program or batch file.\n\n`;
    };
    render();
  }

  function openClip() {
    const w = makeWindow({ title: "Clipboard Viewer", x: 100, y: 80, w: 400, h: 240, menu: ["Display", "Help"] });
    navigator.clipboard?.readText?.().then((t) => {
      w.client.innerHTML = `<pre style="padding:8px;margin:0">${t || "(clipboard empty)"}</pre>`;
    }).catch(() => {
      w.client.innerHTML = `<pre style="padding:8px">(cannot read clipboard)</pre>`;
    });
  }

  function openCharmap() {
    const w = makeWindow({ title: "Character Map", x: 90, y: 70, w: 420, h: 260, gray: true, status: "Arial" });
    let s = "";
    const box = document.createElement("div");
    box.style.padding = "8px";
    box.style.display = "grid";
    box.style.gridTemplateColumns = "repeat(16, 1fr)";
    for (let i = 32; i < 128; i++) {
      const b = document.createElement("button");
      b.className = "btn";
      b.style.minWidth = "0";
      b.textContent = String.fromCharCode(i);
      b.onclick = () => { s += b.textContent; w.statusEl.textContent = s; };
      box.appendChild(b);
    }
    w.client.appendChild(box);
  }

  function openControlPanel() {
    const w = makeWindow({
      title: "Control Panel",
      x: 50, y: 40, w: 480, h: 320,
      menu: ["Settings", "View", "Help"],
      gray: true,
      status: "Control Panel",
    });
    const applets = [
      { name: "Color", icon: "paint", run: openColors },
      { name: "Fonts", icon: "note", run: () => stub("Fonts", "Arial, Courier New, Times New Roman, System, Terminal, Small Fonts, MS Sans Serif, MS Serif") },
      { name: "Ports", icon: "cp", run: () => stub("Ports", "COM1: 9600,8,N,1") },
      { name: "Mouse", icon: "cp", run: () => stub("Mouse", "Microsoft PS/2 Port Mouse") },
      { name: "Desktop", icon: "paint", run: openDesktop },
      { name: "Keyboard", icon: "cp", run: () => stub("Keyboard", "US 101") },
      { name: "Printers", icon: "print", run: () => stub("Printers", "No printers installed.") },
      { name: "International", icon: "cp", run: () => stub("International", "English (United States)") },
      { name: "Date/Time", icon: "clock", run: openClock },
      { name: "Network", icon: "cp", run: () => stub("Network", "Workstation: ARENA-NTWS\nDomain: WORKGROUP\nNWLink, NetBEUI, TCP/IP") },
      { name: "Server", icon: "user", run: () => stub("Server", "This computer is not sharing resources.") },
      { name: "Services", icon: "cp", run: openServices },
      { name: "Devices", icon: "disk", run: () => stub("Devices", "atapi, disk, i8042prt, serial, parallel, vga, beep") },
      { name: "System", icon: "cp", run: openSystem },
      { name: "Sound", icon: "cp", run: () => { beep(523, 0.1); stub("Sound", "Default beep assigned to Asterisk, Critical Stop, Question, Exclamation."); } },
    ];
    icons(applets, w.client, (it) => it.run());
  }

  function openColors() {
    const w = makeWindow({ title: "Color", x: 140, y: 80, w: 360, h: 220, gray: true, status: false });
    w.client.innerHTML = `<div style="padding:12px">Color Scheme:
      <select id="scheme">
        <option>Windows Default</option>
        <option>Arizona</option>
        <option>Black Leather Jacket</option>
        <option>High Contrast Black</option>
        <option>Hot Dog Stand</option>
        <option>Maple</option>
        <option>Red White and Blue</option>
        <option>The Blues</option>
        <option>Tweed</option>
      </select>
      <p>Active Title Bar: Navy (#000080)<br>Button Face: Silver (#C0C0C0)<br>Desktop: Teal (#008080)</p>
      <div class="dlg-btns"><button class="btn default" id="c-ok">OK</button></div>
    </div>`;
    const apply = () => {
      const v = w.client.querySelector("#scheme").value;
      if (v === "Hot Dog Stand") $("#wallpaper").style.background = "#ffff00";
      else if (v === "High Contrast Black") $("#wallpaper").style.background = "#000";
      else if (v === "The Blues") $("#wallpaper").style.background = "#000080";
      else if (v === "Arizona") $("#wallpaper").style.background = "#c06020";
      else $("#wallpaper").style.background = "#008080";
    };
    w.client.querySelector("#scheme").onchange = apply;
    w.client.querySelector("#c-ok").onclick = () => { apply(); w.close(); };
  }

  function openDesktop() {
    const w = makeWindow({ title: "Desktop", x: 150, y: 90, w: 340, h: 200, gray: true, status: false });
    w.client.innerHTML = `<div style="padding:12px">Pattern: <select>
      <option>(None)</option><option>Boxes</option><option>Critters</option><option>Diamonds</option>
      <option>Paisley</option><option>Scottie</option><option>Spinner</option><option>Waffle</option>
    </select><br><br>Wallpaper: <select>
      <option>(None)</option><option>ARCADE.BMP</option><option>ARGYLE.BMP</option>
      <option>CARS.BMP</option><option>CASTLE.BMP</option><option>HONEY.BMP</option>
      <option>LEAVES.BMP</option><option>REDBRICK.BMP</option><option>RIVETS.BMP</option>
      <option>TARTAN.BMP</option>
    </select>
    <div class="dlg-btns"><button class="btn default">OK</button></div></div>`;
    w.client.querySelector("button").onclick = w.close;
  }

  function openSystem() {
    stub("System", "Microsoft Windows NT Workstation\nVersion 3.51  Build 1057\n\nRegistered to Administrator / ARENA\n\nComputer: Intel x86 Family 5 Model 2\nHAL: PC Compatible EISA/ISA HAL\nTotal Physical Memory: 16 MB");
  }

  function openServices() {
    const w = makeWindow({ title: "Services", x: 80, y: 50, w: 520, h: 300, menu: ["Options", "Help"], status: "Server: ARENA-NTWS" });
    const rows = [
      ["Alerter", "Started", "Automatic"],
      ["Computer Browser", "Started", "Automatic"],
      ["EventLog", "Started", "Automatic"],
      ["License Logging Service", "Started", "Automatic"],
      ["Messenger", "Started", "Automatic"],
      ["Net Logon", "", "Manual"],
      ["NT LM Security Support Provider", "Started", "Automatic"],
      ["RPC Service", "Started", "Automatic"],
      ["Schedule", "", "Manual"],
      ["Server", "Started", "Automatic"],
      ["Spooler", "Started", "Automatic"],
      ["TCP/IP NetBIOS Helper", "Started", "Automatic"],
      ["Workstation", "Started", "Automatic"],
    ];
    w.client.innerHTML = `<table style="width:100%;border-collapse:collapse;font-size:11px">
      <tr style="background:#c0c0c0"><th align=left>Service</th><th>Status</th><th>Startup</th></tr>
      ${rows.map((r) => `<tr><td>${r[0]}</td><td>${r[1]}</td><td>${r[2]}</td></tr>`).join("")}
    </table>`;
  }

  function openUsers() {
    const w = makeWindow({ title: "User Manager", x: 70, y: 50, w: 480, h: 300, menu: ["User", "Policies", "Options", "Help"], status: "\\\\ARENA-NTWS" });
    w.client.innerHTML = `<pre style="margin:8px">Username        Full Name              Description
Administrator                      Built-in account for administering
Guest                              Built-in account for guest access

Groups:
Administrators, Backup Operators, Guests, Power Users,
Replicator, Users, Everyone</pre>`;
  }

  function openDisk() {
    const w = makeWindow({ title: "Disk Administrator", x: 40, y: 80, w: 600, h: 240, menu: ["Partition", "Tools", "View", "Options", "Help"], gray: true, status: "Disk 0" });
    w.client.innerHTML = `<div style="padding:12px">
      <div style="display:flex;height:70px;border:1px solid #000">
        <div style="flex:3;background:#00a;color:#fff;display:flex;align-items:center;justify-content:center">C: NTFS 1023 MB (System)</div>
        <div style="flex:1;background:#888;display:flex;align-items:center;justify-content:center">Free 24 MB</div>
      </div>
      <p>Disk 0: 1048 MB  IDE  WDC AC21200H<br>CD-ROM 0: NEC CD-ROM DRIVE:40x</p>
    </div>`;
  }

  function openEvents() {
    const w = makeWindow({ title: "Event Viewer - System Log", x: 50, y: 40, w: 560, h: 300, menu: ["Log", "View", "Help"], status: "System" });
    w.client.innerHTML = `<pre style="margin:8px">Date       Time     Source     Event
05/26/95   1:31:02  EventLog   6005  The Event log service was started.
05/26/95   1:31:08  BROWSER    8015  The browser has forced an election.
05/26/95   1:31:14  NETLOGON   5706  Netlogon could not create share NETLOGON
05/26/95   1:32:01  Service Control Manager  7035  The Workstation service was sent a start control
05/26/95   1:32:40  Print      10    No printers</pre>`;
  }

  function openPerf() {
    const w = makeWindow({ title: "Performance Monitor", x: 60, y: 40, w: 500, h: 300, menu: ["File", "Edit", "View", "Add", "Options", "Help"], status: "Chart" });
    const cv = document.createElement("canvas");
    cv.width = 480; cv.height = 220;
    w.client.appendChild(cv);
    const ctx = cv.getContext("2d");
    const hist = Array(80).fill(10);
    setInterval(() => {
      hist.push(8 + Math.random() * 40);
      hist.shift();
      ctx.fillStyle = "#000";
      ctx.fillRect(0, 0, cv.width, cv.height);
      ctx.strokeStyle = "#0f0";
      ctx.beginPath();
      hist.forEach((v, i) => ctx.lineTo(i * 6, 220 - v * 4));
      ctx.stroke();
      ctx.fillStyle = "#0f0";
      ctx.fillText("% Processor Time  _Total", 8, 12);
    }, 400);
  }

  function openWinmsd() {
    const w = makeWindow({ title: "Windows NT Diagnostics", x: 80, y: 40, w: 480, h: 340, menu: ["File", "Edit", "Help"], gray: true, status: "Version" });
    w.client.innerHTML = `<div style="padding:10px">
      <b>Version</b><br>
      Windows NT Workstation Version 3.51<br>
      Build Number: 1057<br>
      Service Pack: None<br>
      CSD Version:<br><br>
      <b>System</b><br>
      BIOS Date: 10/10/94<br>
      BIOS Version: Phoenix 4.0<br>
      CPU: x86 Family 5 Model 2 Stepping 4<br>
      Identifier: x86 Family 5 Model 2 Stepping 4<br>
      HAL: EISA/ISA PC<br><br>
      <b>Display</b><br>
      VGA Compatible  640 x 480  16 colors (emulated 800x600)
    </div>`;
  }

  function openMine() {
    const w = makeWindow({ title: "Minesweeper", x: 180, y: 60, w: 220, h: 280, menu: ["Game", "Help"], gray: true, status: "Beginner" });
    const rows = 9, cols = 9, mines = 10;
    const grid = Array.from({ length: rows }, () => Array(cols).fill(0));
    let placed = 0;
    while (placed < mines) {
      const r = (Math.random() * rows) | 0, c = (Math.random() * cols) | 0;
      if (grid[r][c] !== "m") { grid[r][c] = "m"; placed++; }
    }
    const count = (r, c) => {
      let n = 0;
      for (let dr = -1; dr <= 1; dr++)
        for (let dc = -1; dc <= 1; dc++) {
          const rr = r + dr, cc = c + dc;
          if (rr >= 0 && rr < rows && cc >= 0 && cc < cols && grid[rr][cc] === "m") n++;
        }
      return n;
    };
    const table = document.createElement("table");
    table.className = "mine";
    const open = (r, c, td) => {
      if (td.classList.contains("open")) return;
      td.classList.add("open");
      if (grid[r][c] === "m") {
        td.textContent = "*";
        td.style.background = "#c00";
        beep(200, 0.3);
        return;
      }
      const n = count(r, c);
      td.textContent = n || "";
      const colors = ["", "#00f", "#008000", "#c00", "#000080", "#800000", "#008080", "#000", "#808080"];
      td.style.color = colors[n];
    };
    for (let r = 0; r < rows; r++) {
      const tr = document.createElement("tr");
      for (let c = 0; c < cols; c++) {
        const td = document.createElement("td");
        td.onmousedown = (e) => {
          e.preventDefault();
          if (e.button === 2) td.textContent = td.textContent === "F" ? "" : "F";
          else open(r, c, td);
        };
        td.oncontextmenu = (e) => e.preventDefault();
        tr.appendChild(td);
      }
      table.appendChild(tr);
    }
    w.client.appendChild(table);
  }

  function openSol() {
    const w = makeWindow({ title: "Solitaire", x: 40, y: 20, w: 620, h: 400, menu: ["Game", "Help"], status: "Score: 0" });
    const suits = ["♠", "♥", "♦", "♣"];
    const ranks = ["A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"];
    const deck = [];
    suits.forEach((s) => ranks.forEach((r) => deck.push({ s, r, red: s === "♥" || s === "♦" })));
    for (let i = deck.length - 1; i > 0; i--) {
      const j = (Math.random() * (i + 1)) | 0;
      [deck[i], deck[j]] = [deck[j], deck[i]];
    }
    const board = document.createElement("div");
    board.className = "sol";
    const stock = document.createElement("div");
    stock.style.marginBottom = "12px";
    const back = document.createElement("div");
    back.className = "card back";
    back.textContent = "NT";
    stock.appendChild(back);
    board.appendChild(stock);
    const row = document.createElement("div");
    for (let t = 0; t < 7; t++) {
      const pile = document.createElement("div");
      pile.style.display = "inline-block";
      pile.style.verticalAlign = "top";
      pile.style.width = "56px";
      for (let k = 0; k <= t; k++) {
        const card = deck.pop();
        const el = document.createElement("div");
        el.className = "card" + (card.red ? " red" : "") + (k < t ? " back" : "");
        el.textContent = k < t ? "" : card.r + card.s;
        el.style.marginTop = k ? "-40px" : "0";
        pile.appendChild(el);
      }
      row.appendChild(pile);
    }
    board.appendChild(row);
    w.client.appendChild(board);
    back.onclick = () => {
      const c = deck.pop();
      if (!c) return;
      const el = document.createElement("div");
      el.className = "card" + (c.red ? " red" : "");
      el.textContent = c.r + c.s;
      stock.appendChild(el);
    };
  }

  function runDlg() {
    const w = makeWindow({ title: "Run", x: 200, y: 140, w: 360, h: 150, gray: true, status: false });
    w.client.innerHTML = `<div style="padding:12px">Command Line:<br><input id="runin" style="width:100%" />
      <div class="dlg-btns"><button class="btn default" id="rgo">OK</button><button class="btn" id="rcx">Cancel</button></div></div>`;
    w.client.querySelector("#rcx").onclick = w.close;
    w.client.querySelector("#rgo").onclick = () => {
      const v = w.client.querySelector("#runin").value.toLowerCase();
      w.close();
      if (v.includes("notepad")) openNotepad();
      else if (v.includes("winfile")) openFileManager();
      else if (v.includes("cmd")) openCmd();
      else if (v.includes("calc")) openCalc();
      else stub("Run", "Cannot find file " + v);
    };
  }

  function download(name, text) {
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([text]));
    a.download = name;
    a.click();
  }

  function taskList() {
    const tl = $("#tasklist");
    const sel = $("#task-select");
    sel.innerHTML = "";
    windows.forEach((w, i) => {
      const o = document.createElement("option");
      o.value = i;
      o.textContent = w.title + (w.minimized ? " (minimized)" : "");
      sel.appendChild(o);
    });
    tl.classList.remove("hidden");
    tl.style.zIndex = ++z;
  }
  $("#task-cancel").onclick = () => $("#tasklist").classList.add("hidden");
  $("#task-switch").onclick = () => {
    const i = +$("#task-select").value;
    if (windows[i]) focusWin(windows[i].el);
    $("#tasklist").classList.add("hidden");
  };
  $("#task-end").onclick = () => {
    const i = +$("#task-select").value;
    if (windows[i]) closeWin(windows[i].el);
    $("#tasklist").classList.add("hidden");
  };

  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") taskList();
    if (e.ctrlKey && e.key.toLowerCase() === "esc") taskList();
  });

  boot();
})();
