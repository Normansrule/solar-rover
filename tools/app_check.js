#!/usr/bin/env node
// Checks the phone app and the website in a real browser, against a fake car.
//
//   node tools/app_check.js            run the checks (CI does this)
//   node tools/app_check.js --shots    also redraw docs/img/app.png, app-light.png, site.png
//
// The app is served the way the car serves it (plain http, the page opens
// ws://<same host>/ws). Playwright answers that WebSocket itself, so the "car"
// is a few lines below: it records every line the app sends and can push
// telemetry back. Needs the playwright npm package and its Chromium.
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '..');
const SHOTS = process.argv.includes('--shots');
const PHONE = { width: 390, height: 844 };
const TYPES = { '.html': 'text/html', '.css': 'text/css', '.js': 'text/javascript', '.woff2': 'font/woff2',
  '.png': 'image/png', '.gif': 'image/gif', '.svg': 'image/svg+xml', '.json': 'application/json' };

// One telemetry line as firmware/car/src/telemetry.cpp writes it.
const TEL = {
  bat_pct: 72, bat_v: 7.86, chg: 'solar', sol_mw: 3420, chg_ma: 410, temp_c: 24, tof: 820,
  spd: [210, 205, 215, 200], imu: [0, 0, 9.8, 0, 0, 12], side: [310, -1], src: 'APP', ovr: false, mode: 'manual',
  arm: [0, 60, 120, 90, 0], low: false, crit: false,
};

// ---------- tiny static server for the repo ----------
function serve() {
  const srv = http.createServer((req, res) => {
    let p = decodeURIComponent(new URL(req.url, 'http://x').pathname);
    if (p.endsWith('/')) p += 'index.html';
    const f = path.join(ROOT, p);
    if (!f.startsWith(ROOT + path.sep)) { res.writeHead(403).end(); return; }
    fs.readFile(f, (err, data) => {
      if (err) { res.writeHead(404).end('not found'); return; }
      res.writeHead(200, { 'content-type': TYPES[path.extname(f)] || 'application/octet-stream' }).end(data);
    });
  });
  return new Promise(ok => srv.listen(0, '127.0.0.1', () => ok(srv)));
}

// ---------- the fake car ----------
async function fakeCar(page) {
  const car = { got: [], sockets: [], connects: 0 };
  await page.routeWebSocket(/\/ws$/, ws => {
    car.connects++; car.sockets.push(ws);
    ws.onMessage(m => car.got.push(String(m)));
  });
  car.tel = extra => car.sockets.at(-1).send(JSON.stringify({ ...TEL, ...extra }));
  car.drop = () => car.sockets.at(-1).close();
  // the lines the app sent, without the 60 ms "H" heartbeats
  car.lines = () => car.got.filter(l => l !== 'H');
  car.clear = () => { car.got.length = 0; };
  return car;
}

// ---------- reporting ----------
let failed = 0;
async function check(name, fn) {
  try { await fn(); console.log('  ok   ' + name); }
  catch (e) { failed++; console.log('  FAIL ' + name + '\n       ' + String(e.message || e).split('\n')[0]); }
}
function expect(cond, msg) { if (!cond) throw new Error(msg); }
async function until(fn, msg, ms = 2000) {
  const end = Date.now() + ms;
  for (;;) { const v = await fn(); if (v) return v; if (Date.now() > end) throw new Error(msg); await new Promise(r => setTimeout(r, 25)); }
}
const text = (page, sel) => page.locator(sel).innerText();

// Layout rules every page must keep at phone width.
async function phoneLayout(page) {
  return page.evaluate(() => {
    const out = [];
    const sw = document.documentElement.scrollWidth, cw = document.documentElement.clientWidth;
    if (sw > cw) out.push(`page scrolls sideways (${sw} px wide in a ${cw} px screen)`);
    for (const el of document.querySelectorAll('button, [role=button], input, select, a.btn')) {
      // a segmented control's buttons sit inside one padded track: the track is the target
      const r = (el.parentElement.classList.contains('seg') ? el.parentElement : el).getBoundingClientRect();
      if (!r.width || getComputedStyle(el).visibility === 'hidden') continue;
      const name = el.getAttribute('aria-label') || el.innerText.trim() || el.id || el.tagName;
      const tall = el.type === 'range' ? 24 : 44;   // a slider's thumb is bigger than its track
      if (r.height < tall - 0.5) out.push(`"${name}" is ${Math.round(r.height)} px tall, under ${tall} px`);
      if (el.tagName === 'BUTTON' && !name) out.push('a button has no name');
    }
    return out;
  });
}

async function main() {
  const srv = await serve();
  const base = `http://127.0.0.1:${srv.address().port}`;
  const browser = await chromium.launch();
  const errors = [];
  const newPage = async (opts = {}) => {
    const ctx = await browser.newContext({ viewport: PHONE, deviceScaleFactor: SHOTS ? 2 : 1, hasTouch: true, ...opts });
    const page = await ctx.newPage();
    page.on('pageerror', e => errors.push(e.message));
    page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
    page.on('requestfailed', r => errors.push('failed to load ' + r.url()));
    page.on('response', r => { if (r.status() >= 400) errors.push(`${r.status()} ${r.url()}`); });
    return page;
  };

  console.log('Phone app (firmware/car/web) against a fake car');
  const page = await newPage({ colorScheme: 'dark' });
  const car = await fakeCar(page);
  await page.goto(base + '/firmware/car/web/');

  await check('connects to ws://<host>/ws on its own', async () => {
    await until(() => car.connects === 1, 'the app never opened the WebSocket');
    await until(async () => (await text(page, '#link')).startsWith('WiFi'), 'the header never said WiFi');
  });
  await check('shows a waiting note until the first reading', async () => {
    expect(await page.locator('#waiting').isVisible(), 'no waiting note');
    expect(await text(page, '#pct') === '--', 'battery shows a number before any reading');
  });
  await check('sends a heartbeat while nobody drives', async () => {
    await until(() => car.got.includes('H'), 'no H heartbeat within 2 s');
  });
  await check('shows the telemetry the car sends', async () => {
    car.tel();
    await until(async () => (await text(page, '#pct')) === '72%', 'battery % not shown');
    const want = { '#volt': '7.86 V', '#chg': 'Sun', '#sol': '3.42 W', '#cur': '410 mA', '#temp': '24 °C',
      '#tof': '82 cm', '#side': '31 cm · clear', '#spd': '0.21 m/s', '#yawr': '12 °/s', '#driver': 'APP' };
    for (const [sel, v] of Object.entries(want)) expect(await text(page, sel) === v, `${sel} shows "${await text(page, sel)}", wanted "${v}"`);
    expect(await page.locator('#waiting').isHidden(), 'the waiting note stayed up');
    expect(await page.locator('[data-mode=manual]').getAttribute('aria-pressed') === 'true', 'the Manual button is not marked');
  });
  await check('corner readings hide for firmware without corner sensors', async () => {
    car.tel({ side: undefined });
    await until(() => page.locator('#sideRow').isHidden(), 'the corner row stayed up');
    car.tel();
    await until(() => page.locator('#sideRow').isVisible(), 'the corner row did not come back');
  });
  await check('arrow keys drive: forward, then a stop when released', async () => {
    await page.locator('#pad').focus(); car.clear();
    await page.keyboard.down('ArrowUp');
    await until(() => car.lines().includes('V 500 0'), `no "V 500 0" (got ${car.lines().slice(0, 3)})`);
    await page.keyboard.up('ArrowUp');
    await until(() => car.lines().at(-1) === 'V 0 0', 'no "V 0 0" after letting go');
  });
  await check('Fast doubles the speed, left turns left', async () => {
    await page.click('#fast'); await page.locator('#pad').focus(); car.clear();
    await page.keyboard.down('ArrowLeft');
    await until(() => car.lines().includes('V 0 800'), `no "V 0 800" (got ${car.lines().slice(0, 3)})`);
    await page.keyboard.up('ArrowLeft'); await page.click('#slow');
  });
  await check('dragging the stick drives, and it springs back', async () => {
    const b = await page.locator('#pad').boundingBox(); car.clear();
    await page.mouse.move(b.x + b.width / 2, b.y + b.height / 2);
    await page.mouse.down(); await page.mouse.move(b.x + b.width / 2, b.y + 4, { steps: 4 });
    await until(() => car.lines().some(l => /^V [34]\d\d /.test(l)), `no forward V line (got ${car.lines().slice(0, 3)})`);
    await page.mouse.up();
    await until(() => car.lines().at(-1) === 'V 0 0', 'no "V 0 0" after release');
  });
  await check('STOP button and the Escape key both send S', async () => {
    car.clear(); await page.click('#stop');
    await until(() => car.lines().includes('S'), 'STOP sent nothing');
    car.clear(); await page.keyboard.press('Escape');
    await until(() => car.lines().includes('S'), 'Escape sent nothing');
  });
  await check('mode buttons send M <mode>', async () => {
    for (const m of ['auto', 'agent', 'manual']) {
      car.clear(); await page.click(`[data-mode=${m}]`);
      await until(() => car.lines().includes('M ' + m), `no "M ${m}"`);
    }
  });
  await check('arm poses, gripper and base slider', async () => {
    for (const p of ['park', 'ready', 'pick', 'carry']) {
      car.clear(); await page.click(`[data-pose=${p}]`);
      await until(() => car.lines().includes('P ' + p), `no "P ${p}"`);
    }
    car.clear(); await page.click('#grip');
    await until(() => /^G (0|90)$/.test(car.lines().at(-1) || ''), 'the gripper button sent no G');
    car.clear(); await page.locator('#yaw').fill('45');
    await until(() => car.lines().includes('A 45 * * * *'), 'the slider sent no "A 45 * * * *"');
  });
  await check('dragging in the crane view sends a joint target', async () => {
    // grab the gripper where it is drawn and pull it a little out and down
    await page.waitForTimeout(800);   // let the arm settle after the pose buttons
    await page.locator('#crane').evaluate(e => e.scrollIntoView({ block: 'center' }));   // clear of the sticky header
    const h = await page.locator('#crane .handle').boundingBox(); car.clear();
    const x = h.x + h.width / 2, y = h.y + h.height / 2;
    await page.mouse.move(x, y); await page.mouse.down();
    await page.mouse.move(x + 12, y + 10, { steps: 5 }); await page.mouse.up();
    await until(() => car.lines().some(l => /^A \* -?\d+ -?\d+ -?\d+ \*$/.test(l)),
      `no "A * s e w *" (got [${car.lines().slice(0, 3)}], crane says "${await text(page, '#craneStatus')}", hit ${await page.evaluate(([x, y]) => document.elementFromPoint(x, y)?.outerHTML.slice(0, 60), [x, y])})`);
  });
  await check('low and critical battery banners', async () => {
    car.tel({ bat_pct: 22, low: true });
    await until(async () => (await text(page, '#warn')).startsWith('Battery low'), 'no low-battery banner');
    car.tel({ bat_pct: 8, low: true, crit: true });
    await until(async () => (await text(page, '#warn')).startsWith('Battery critical'), 'no critical banner');
    car.tel();
    await until(() => page.locator('#warn').isHidden(), 'the banner stayed after the battery recovered');
  });
  await check('a plain text reply shows under Connection', async () => {
    car.sockets.at(-1).send('ok WIFI saved');
    await until(async () => (await text(page, '#reply')) === 'ok WIFI saved', 'the reply was not shown');
  });
  await check('Save and restart refuses spaces, then sends WIFI and RESTART', async () => {
    await page.fill('#ssid', 'my net'); await page.fill('#pass', 'pw'); car.clear(); await page.click('#saveWifi');
    expect((await text(page, '#reply')).includes("can't contain spaces"), 'no warning about spaces');
    expect(!car.lines().some(l => l.startsWith('WIFI')), 'it sent a network name with a space');
    await page.fill('#ssid', 'mynet'); await page.click('#saveWifi');
    await until(() => car.lines().includes('WIFI mynet pw') && car.lines().includes('RESTART'), 'no WIFI + RESTART');
  });
  await check('phone layout: no sideways scroll, every control at least 44 px', async () => {
    const bad = await phoneLayout(page);
    expect(!bad.length, bad.join('; '));
  });
  await check('losing the car shows a banner, then it reconnects', async () => {
    car.drop();
    await until(() => page.locator('#linkBanner').isVisible(), 'no lost-car banner');
    expect((await text(page, '#linkText')).startsWith('Lost the car'), 'the banner does not say the car was lost');
    expect(await page.locator('#retry').isVisible(), 'no Retry button');
    await page.click('#retry');
    await until(() => car.connects === 2, 'Retry did not reconnect');
    await until(() => page.locator('#linkBanner').isHidden(), 'the banner stayed after reconnecting');
  });
  await check('no script errors and every file loaded', async () => {
    expect(!errors.length, errors.join('; '));
  });

  // screenshots from fresh pages, one per theme
  if (SHOTS) for (const [scheme, file] of [['dark', 'app.png'], ['light', 'app-light.png']]) {
    const p = await newPage({ colorScheme: scheme }); const c = await fakeCar(p);
    await p.goto(base + '/firmware/car/web/');
    await until(() => c.connects === 1, 'no connection'); await p.waitForTimeout(200);
    c.tel(); await p.click('[data-pose=ready]'); await p.waitForTimeout(1600);
    // the header names the host the page came from; on the car that is solar-rover.local, not this test server
    await p.evaluate(() => { document.getElementById('link').textContent = 'WiFi · solar-rover.local'; document.activeElement.blur(); scrollTo(0, 0); });
    await p.screenshot({ path: path.join(ROOT, 'docs/img/' + file) });
  }

  console.log('Website (docs/index.html)');
  errors.length = 0;
  const site = await newPage({ colorScheme: 'dark' });
  await site.goto(base + '/docs/index.html');
  await check('loads every picture and font', async () => {
    await site.evaluate(async () => { for (const i of document.images) { i.loading = 'eager'; } await document.fonts.ready; });
    await until(() => site.evaluate(() => [...document.images].every(i => i.complete)), 'pictures still loading');
    const broken = await site.evaluate(() => [...document.images].filter(i => !i.naturalWidth).map(i => i.getAttribute('src')));
    expect(!broken.length, 'broken pictures: ' + broken.join(', '));
  });
  await check('every #link on the page has a target', async () => {
    const missing = await site.evaluate(() => [...document.querySelectorAll('a[href^="#"]')]
      .map(a => a.getAttribute('href').slice(1)).filter(id => id && !document.getElementById(id)));
    expect(!missing.length, 'no target for #' + missing.join(', #'));
  });
  await check('the theme button switches dark and light', async () => {
    const before = await site.evaluate(() => getComputedStyle(document.documentElement).backgroundColor);
    await site.click('#theme');
    const after = await site.evaluate(() => getComputedStyle(document.documentElement).backgroundColor);
    expect(before !== after, 'the background did not change');
    await site.click('#theme');
  });
  await check('the crane poses move the arm', async () => {
    const pose = await site.locator('[data-pose]').nth(1);
    await pose.click();
    await until(async () => (await pose.getAttribute('aria-pressed')) === 'true', 'the pose button was not marked');
  });
  await check('phone layout: no sideways scroll, every control at least 44 px', async () => {
    const bad = await phoneLayout(site);
    expect(!bad.length, bad.join('; '));
  });
  await check('no script errors and every file loaded', async () => {
    expect(!errors.length, errors.join('; '));
  });
  if (SHOTS) {
    const p = await newPage({ viewport: { width: 1280, height: 800 }, deviceScaleFactor: 1, hasTouch: false, colorScheme: 'dark' });
    await p.goto(base + '/docs/index.html'); await p.waitForTimeout(1500);
    await p.screenshot({ path: path.join(ROOT, 'docs/img/site.png') });
    console.log('Redrew docs/img/app.png, app-light.png and site.png');
  }

  await browser.close(); srv.close();
  console.log(failed ? `${failed} check(s) failed` : 'All checks passed');
  process.exit(failed ? 1 : 0);
}

main().catch(e => { console.error(e); process.exit(1); });
