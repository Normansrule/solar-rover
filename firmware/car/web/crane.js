/* Solar Rover crane: a side view of the car and its arm, drawn to scale, that you can drag.
   Shared by the phone app (index.html, inlined into the firmware by scripts/embed_web.py)
   and the website (docs/index.html).
   - Dimensions: hardware/scad/config.scad (mm).
   - Inverse kinematics: the same maths as agent/solar_rover_agent/arm_ik.py.
   - Poses and joint limits: firmware/car/src/arm.cpp and config.h.
   Angles are the firmware's logical ones: shoulder 0 = horizontal, + up; elbow 0 = straight,
   + folds down; wrist 0 = in line with the forearm, + up; grip 0 = closed .. 100 = open. */
(function (root) {
  'use strict';
  const L1 = 100, L2 = 90, L3 = 73.5, HAND_OFF = 5;   // mm
  const SH_X = 108, SH_Z = 147.6;                     // shoulder axis: turret_x, height above ground
  const LIMITS = {shoulder: [-35, 120], elbow: [0, 150], wrist: [-90, 90], grip: [0, 100]};
  const POSES = {                                     // shoulder, elbow, wrist, grip (arm.cpp)
    park: [110, 120, -70, 20], ready: [45, 60, -10, 60], pick: [10, 57, -43, 90], carry: [70, 120, -40, 0]
  };
  const JOINTS = ['shoulder', 'elbow', 'wrist', 'grip'];
  const BALL_R = 20, GRAVITY = 9810;                  // a 40 mm ping-pong ball, mm/s²
  const rad = d => d * Math.PI / 180, deg = r => r * 180 / Math.PI;
  const NS = 'http://www.w3.org/2000/svg';
  const reduced = () => root.matchMedia && root.matchMedia('(prefers-reduced-motion: reduce)').matches;

  // ---------- kinematics (x forward, z up, mm, origin on the ground under the deck centre) ----------
  function forward(a) {
    const a1 = rad(a.shoulder), a2 = a1 - rad(a.elbow), a3 = a2 + rad(a.wrist);
    const sh = [SH_X, SH_Z];
    const el = [sh[0] + L1 * Math.cos(a1), sh[1] + L1 * Math.sin(a1)];
    const wr = [el[0] + L2 * Math.cos(a2), el[1] + L2 * Math.sin(a2)];
    const tip = [wr[0] + L3 * Math.cos(a3) - HAND_OFF * Math.sin(a3), wr[1] + L3 * Math.sin(a3) + HAND_OFF * Math.cos(a3)];
    return {sh, el, wr, tip, hand: a3};
  }
  function solve(x, z, pitchDeg) {
    const r = x - SH_X, p = rad(pitchDeg);
    const wx = r - L3 * Math.cos(p) + HAND_OFF * Math.sin(p);
    const wz = z - SH_Z - L3 * Math.sin(p) - HAND_OFF * Math.cos(p);
    const d = Math.hypot(wx, wz);
    if (d > L1 + L2 - 1e-6) return {error: 'That\'s further than the arm reaches (' + Math.round((L1 + L2 + L3) / 10) + ' cm from the shoulder).'};
    if (d < Math.abs(L1 - L2) + 1e-6) return {error: 'That\'s too close to the shoulder to fold into.'};
    const clamp1 = v => Math.max(-1, Math.min(1, v));
    const elbow = Math.PI - Math.acos(clamp1((L1 * L1 + L2 * L2 - d * d) / (2 * L1 * L2)));
    const shoulder = Math.atan2(wz, wx) + Math.acos(clamp1((L1 * L1 + d * d - L2 * L2) / (2 * L1 * d)));
    const out = {shoulder: deg(shoulder), elbow: deg(elbow), wrist: deg(p - (shoulder - elbow))};
    for (const k of ['shoulder', 'elbow', 'wrist']) {
      const [lo, hi] = LIMITS[k];
      if (out[k] < lo - 0.5 || out[k] > hi + 0.5) return {error: `The ${k} would need ${Math.round(out[k])}°, past its ${lo} to ${hi}° limit.`};
    }
    return out;
  }
  // of every gripper angle that works, the one closest to where the hand already points,
  // leaning towards straight down (best for picking things up)
  function solveBest(x, z, current = -90) {
    let best = null, score = Infinity, first = null;
    for (let p = -180; p <= 30; p += 3) {
      const s = solve(x, z, p);
      if (s.error) { first = first || s; continue; }
      const sc = Math.abs(p - current) + 0.35 * Math.abs(p + 90);
      if (sc < score) { score = sc; best = s; }
    }
    return best || first;
  }

  // ---------- drawing ----------
  // Every part is built as SVG markup, so the same drawing can go on the page and be written
  // out for the README banner (tools/rover_svg.js). Body parts use world millimetres (x forward,
  // z up); arm parts use each link's own frame (x along the link, y towards its underside).
  function el(tag, attrs, parent) {
    const e = document.createElementNS(NS, tag);
    for (const k in attrs) e.setAttribute(k, attrs[k]);
    if (parent) parent.appendChild(e);
    return e;
  }
  const n2 = v => +v.toFixed(2);
  const attrs = o => Object.keys(o).map(k => `${k}="${typeof o[k] === 'number' ? n2(o[k]) : o[k]}"`).join(' ');
  const r = (c, x, y, w, h, rx = 0) => `<rect ${attrs({class: c, x, y, width: w, height: h, rx})}/>`;
  const c = (cl, cx, cy, rr) => `<circle ${attrs({class: cl, cx, cy, r: rr})}/>`;
  const poly = (cl, pts) => `<path class="${cl}" d="M${pts.map(p => n2(p[0]) + ',' + n2(p[1])).join(' L')} Z"/>`;
  const g = (a, inner) => `<g ${attrs(a)}>${inner}</g>`;
  const B = (cl, x0, z0, x1, z1, rx = 0) => r(cl, x0, -z1, x1 - x0, z1 - z0, rx);          // box, world mm
  const W = (cl, x, z, rr) => c(cl, x, -z, rr);                                           // circle, world mm
  const wire = pts => `<path class="wire" d="M${pts.map(p => n2(p[0]) + ',' + n2(-p[1])).join(' L')}"/>`;
  const range = n => Array.from({length: n}, (_, i) => i);

  // positions from hardware/scad/assembly.scad and config.scad
  const DECK = 48.6, ROOF = 123.6, PT = 51.6, LT = 83.6, CT = 75.6, AXLE = 32.5;
  const wheel = x => g({transform: `translate(${x} ${-AXLE})`},
    c('tire', 0, 0, 32.5) + c('tread nl', 0, 0, 30.2) + '<g class="hub">' + c('rim', 0, 0, 25) +
    range(6).map(i => c('hole', 15 * Math.cos(i * Math.PI / 3 + 0.5), 15 * Math.sin(i * Math.PI / 3 + 0.5), 4.2)).join('') +
    c('metal', 0, 0, 6) + c('hole', 0, 0, 2.2) + '</g>');
  const motor = s => {             // TT gear motor and its clamp, under the deck; s = +1 front, -1 rear
    const X = (a, b) => s > 0 ? [a, b] : [-b, -a];
    const [m0, m1] = X(47.3, 111.3), [c0, c1] = X(47.3, 68), [k0, k1] = X(54, 104);
    return B('motor', m0, 23.1, m1, 41.9, 2) + B('motor-can', c0, 24.5, c1, 40.5, 6) + B('hull-dark', k0, 41.9, k1, 44.6);
  };
  const post = (x, cl = 'post') => B(cl, x - 4, DECK, x + 4, ROOF) + W('bolt', x, DECK + 3, 1.6);
  const panel = (x0, x1) => B('hull-dark', x0, ROOF, x1, ROOF + 4) + B('hull', x0, ROOF, x0 + 3, ROOF + 8) +
    B('hull', x1 - 3, ROOF, x1, ROOF + 8) + B('cell', x0 + 5, ROOF + 4, x1 - 5, ROOF + 7) +
    range(10).map(i => B('cell-2 nl', x0 + 6 + i * 11, ROOF + 4.8, x0 + 16 + i * 11, ROOF + 6.2)).join('');
  const jumpers = (x0, x1, z0, z1) => range(Math.round((x1 - x0) / 6)).map(i => {
    const x = x0 + 3 + i * 6;
    return wire([[x, z0], [x + (i % 2 ? 2 : -2), z1 - 3]]) + B('plug', x - 1.6 + (i % 2 ? 2 : -2), z1 - 4, x + 1.6 + (i % 2 ? 2 : -2), z1);
  }).join('');

  function bodySVG() {
    return [
      post(-62, 'post back'), post(-62 + 4, 'post back'),
      B('hull-dark', -146, 40.6, 146, 44.6), B('hull-dark', -5, 34.6, 5, 44.6),            // ribs, joint flange
      motor(1), motor(-1),
      B('hull', -150, 44.6, 150, DECK, 1), W('bolt', -2, 38, 1.4), W('bolt', 2, 41, 1.4),
      poly('hull-dark', [[146, -49], [186, -26], [186, -16], [152, -16], [146, -30]]),      // nose
      W('bolt', 160, 30, 1.6), W('bolt', 176, 24, 1.6),
      // battery holder (four 18650 cells) and the charge tray above it
      B('chip', -144.5, DECK, -67.5, 69.6, 2), B('batt', -141, 51, -71, 67, 7), B('batt-2 nl', -128, 51.6, -112, 66.4),
      B('metal', -71, 55.5, -68.5, 62.5), B('metal', -143.5, 54, -141, 64),
      B('metal', -142, DECK, -139, 72.6), B('metal', -79, DECK, -76, 72.6), B('hull-dark', -144, 72.6, -68, CT),
      B('pcb', -143, CT, -102, CT + 1.6), B('chip', -138, CT + 1.6, -124, CT + 9, 3), B('metal', -118, CT + 1.6, -108, CT + 13, 2),
      B('metal-lt nl', -118, CT + 11, -108, CT + 13, 1), B('relay', -100, CT + 1.6, -82, CT + 18),
      B('pcb-2', -80, CT, -68, CT + 1.6), B('term', -79, CT + 1.6, -70, CT + 8),
      // power tray: buck converter, wire nuts, motor drivers with their jumpers
      B('hull-dark', -56, DECK, 56, PT),
      B('pcb-2', -31, 55.6, 31, 57.2), B('plug', -28, 57.2, 8, 63),
      B('pcb', -44, PT, -18, PT + 1.6), B('metal', -42, PT + 1.6, -31, 64), range(4).map(i => B('chip nl', -41 + i * 2.6, 60, -40 + i * 2.6, 64)).join(''),
      B('chip', -29, PT + 1.6, -20, 61, 2.5),
      B('plug', -15, PT, 15, 59.7, 1), range(5).map(i => B('lever nl', -13 + i * 6, 58, -9 + i * 6, 59.7)).join(''),
      B('pcb-2', -12.75, 57.6, 7.75, 59.2), B('pcb-2', 13.75, 57.6, 34.25, 59.2), B('chip', -6, 59.2, 2, 61.5), B('chip', 20, 59.2, 28, 61.5),
      jumpers(-12, 8, 61.5, 79), jumpers(14, 34, 61.5, 79),
      B('metal', -42, PT, -38, LT), B('metal', 38, PT, 42, LT), B('hull-dark', -56, LT, 56, LT + 3),
      // logic tray: ESP32 on its expansion board, the IMU, the Pi Zero in front
      B('pcb-2', -27, LT + 6, 27, LT + 7.6), B('term', -25, LT + 7.6, 25, LT + 14), B('pcb', -24, LT + 14, 24, LT + 15.6),
      B('metal', -8, LT + 15.6, 10, LT + 19), B('metal', -27, LT + 15.6, -21, LT + 18.6),
      B('metal', 38, LT, 42, LT + 2), B('pcb-2', 29, LT + 3, 45, LT + 4.6), B('chip', 33, LT + 4.6, 40, LT + 6.5),
      B('pcb', -32.5, LT + 7, 32.5, LT + 8.6), B('chip', -25, LT + 8.6, 25, LT + 11), jumpers(-24, 24, LT + 11, LT + 26),
      post(-140), post(-115), post(-40), post(40),
      panel(-180, -60), panel(-60, 60),
      // turret: base, its bolted flange, the top plate; camera on its mount; shoulder bracket and servo
      B('hull-dark', 62, DECK, 154, DECK + 4), B('hull-top', 66, DECK + 4, 82, 96.6), B('hull', 82, DECK + 4, 132, 96.6),
      B('hull-dark', 132, DECK + 4, 150, 96.6), W('bolt', 70, DECK + 2, 1.6), W('bolt', 146, DECK + 2, 1.6),
      B('nl hull-dark', 66, 70, 150, 71),
      B('boom', 64, 96.6, 152, 102.6, 1.5), B('boom-2 nl', 64, 96.6, 152, 98),
      B('chip', 136, 102.6, 148, 108, 1),
      g({transform: `rotate(15 147 -108)`}, B('chip', 145, 106, 149, 133, 1) + B('pcb', 149, 108, 150.6, 132) + B('chip', 150.6, 115, 157, 125, 1.5)),
      poly('boom-2', [[92, -102.6], [124, -102.6], [122, -140], [116, -156], [100, -156], [94, -140]]),
      W('bolt', 98, 108, 1.6), W('bolt', 118, 108, 1.6),
      B('servo-2', 91, 137.75, 145.5, 157.45, 1.5), B('servo', 98, 137.75, 138.7, 157.45, 2),
      wheel(-100), wheel(100),                                                                 // nearest to you
    ].join('');
  }

  // the arm, one nested group per joint so each part is drawn once and only the joints move
  const pivot = (rr) => c('metal', 0, 0, rr) + c('pin', 0, 0, 4) + c('metal-lt nl', 0, 0, 1.5);
  const stripes = (x0, x1, h) => range(Math.ceil((x1 - x0) / 12)).map(i => {
    const x = x0 + i * 12;
    return poly('band nl', [[x, h], [x + 6, h], [Math.min(x + 14, x1), -h], [Math.min(x + 8, x1), -h]]);
  }).join('');
  function armSVG() {
    const finger = s => `<g class="${s > 0 ? 'j-f2' : 'j-f1'}">` + r('finger', 38, -7.5, 35.5, 5, 1.5) +
      poly('finger nl', s > 0 ? [[67, -7.5], [73.5, -7.5], [73.5, -10.5]] : [[67, -2.5], [73.5, -2.5], [73.5, 0.5]]) + '</g>';
    const hand = r('boom', -8, -13, 46, 16, 3) + r('servo', 13, -11.5, 22.8, 12.2, 1.5) + c('metal', 19, -5.4, 3) +
      r('metal', 32, -9, 9, 3, 1) + finger(-1) + finger(1) + pivot(6);
    const fore = r('boom', -10, -8, 110, 16, 8) + r('boom-2 nl', 52, -3.5, 22, 7, 3.5) +
      r('servo', 12, -6, 32, 12, 2) + c('metal', 38, 0, 2.6) + r('servo', 62, -5, 20, 10, 1.5) + c('metal', 77, 0, 2.2);
    const upper = r('boom', -12, -12, 124, 24, 12) + stripes(16, 52, 12) + r('boom-2 nl', 62, -4.5, 30, 9, 4.5) +
      c('hole', 100 - 8, 7, 1.5) + c('hole', 8, 7, 1.5);
    return g({class: 'j-sh', transform: `translate(${SH_X} ${-SH_Z})`},
      upper + g({class: 'j-el', transform: `translate(${L1} 0)`},
        fore + g({class: 'j-wr', transform: `translate(${L2} 0)`}, hand) + pivot(7)) +
      c('metal', 0, 0, 11) + range(4).map(i => c('hole', 7.5 * Math.cos(i * Math.PI / 2 + 0.78), 7.5 * Math.sin(i * Math.PI / 2 + 0.78), 1.3)).join('') +
      c('pin', 0, 0, 4) + c('metal-lt nl', 0, 0, 1.5));
  }

  // numbered parts, for the website's key (opts.callouts)
  const CALLOUTS = [            // marker x, z -> the part it points at
    [-172, 200, -150, 131], [-128, 200, -139, 64], [-84, 200, -56, 32], [-40, 200, -30, 64],
    [4, 200, 0, 104], [48, 200, 85, 80], [204, 92, 153, 121]];
  function calloutsSVG() {
    return CALLOUTS.map(([mx, mz, x, z], i) => `<line class="leader" ${attrs({x1: mx, y1: -mz, x2: x, y2: -z})}/>` +
      W('callout-dot', x, z, 2) + W('callout', mx, mz, 9) + `<text class="callout-n" ${attrs({x: mx, y: -mz + 4.2})}>${i + 1}</text>`).join('');
  }

  class Crane {
    /* opts: interactive (drag and keys), ball (a ball to pick up), callouts (numbered parts), viewBox, follow ('target' = the drawing
       springs to where you drag; 'external' = it shows angles from setActual(), and dragging only
       moves the command marker), onCommand(angles), onStatus(text, isError), label (aria) */
    constructor(host, opts = {}) {
      this.o = Object.assign({interactive: true, ball: false, follow: 'target'}, opts);
      const start = POSES.ready;
      this.j = {}; JOINTS.forEach((k, i) => this.j[k] = {x: start[i], v: 0, t: start[i]});
      this.cmd = {shoulder: start[0], elbow: start[1], wrist: start[2], grip: start[3]};

      const svg = this.svg = el('svg', {class: 'crane', viewBox: this.o.viewBox || '-200 -262 590 278', role: 'group',
        'aria-label': opts.label || 'Side view of the rover and its crane arm'});
      el('rect', {class: 'floor', x: -200, y: 0, width: 590, height: 16}, svg);
      for (let x = 0; x <= 300; x += 100) {          // a ruler along the ground, from the turret
        el('rect', {class: 'scale', x: SH_X + x - 0.5, y: 2, width: 1, height: x % 100 ? 3 : 6}, svg);
        if (x) el('text', {class: 'scale', x: SH_X + x + 3, y: 13}, svg).textContent = x / 10 + ' cm';
      }
      this.reach = el('circle', {class: 'reach', cx: SH_X, cy: -SH_Z, r: L1 + L2 + L3}, svg);
      el('g', {class: 'parts'}, svg).innerHTML = bodySVG();
      if (this.o.callouts) el('g', {class: 'callouts', 'aria-hidden': 'true'}, svg).innerHTML = calloutsSVG();
      const arm = el('g', {class: 'parts'}, svg); arm.innerHTML = armSVG();
      [this.gSh, this.gEl, this.gWr, this.gF1, this.gF2] = ['sh', 'el', 'wr', 'f1', 'f2'].map(k => arm.querySelector('.j-' + k));
      this.ballEl = this.o.ball ? el('circle', {class: 'ball', r: BALL_R}, svg) : null;
      this.ghost = el('circle', {class: 'ghost', r: 7}, svg);
      this.marker = el('circle', {class: 'handle-ring', r: 15}, svg);   // where you last pointed (follow=external)
      this.handle = el('circle', {class: 'handle', r: 26, tabindex: this.o.interactive ? 0 : -1, role: 'slider',
        'aria-label': 'Gripper position. Arrow keys move it, Enter opens or closes it.'}, svg);
      this.ring = el('circle', {class: 'handle-ring', r: 15}, svg);
      host.appendChild(svg);

      if (this.o.ball) this.ball = {x: SH_X + 165, z: BALL_R, vz: 0, held: false};
      if (this.o.interactive) this._bind();
      this._last = 0; this._raf = null; this._kick();
      this._draw();
    }

    // ---------- public ----------
    pose(name) { const p = POSES[name]; if (!p) return; this._setCmd({shoulder: p[0], elbow: p[1], wrist: p[2], grip: p[3]}); }
    setGrip(pct) { this._setCmd(Object.assign({}, this.cmd, {grip: Math.max(0, Math.min(100, pct))})); }
    toggleGrip() { this.setGrip(this.cmd.grip > 40 ? 0 : 90); }
    get angles() { const a = {}; JOINTS.forEach(k => a[k] = this.j[k].x); return a; }
    /* angles the car reports (telemetry "arm": yaw, shoulder, elbow, wrist, grip) */
    setActual(a) { JOINTS.forEach(k => { if (a[k] != null) this.j[k].t = a[k]; }); this._kick(); }
    showReach(on) { this.svg.classList.toggle('show-reach', on); }
    reset() { if (this.ball) Object.assign(this.ball, {x: SH_X + 165, z: BALL_R, vz: 0, held: false}); this.pose('ready'); this._status(''); }

    // ---------- internals ----------
    _status(text, err) { this.svg.classList.toggle('blocked', !!err); this.o.onStatus && this.o.onStatus(text, !!err); }
    _setCmd(c) {
      this.cmd = c;
      if (this.o.follow === 'target') JOINTS.forEach(k => this.j[k].t = c[k]);
      this.o.onCommand && this.o.onCommand(Object.assign({}, c));
      this._kick();
    }
    _aim(x, z) {
      z = Math.max(z, 4);                       // not through the floor
      const s = solveBest(x, z, deg(forward(this.cmd).hand));
      const tip = forward(this.cmd).tip;
      if (s.error) {
        this.ghost.setAttribute('cx', x); this.ghost.setAttribute('cy', -z);
        this._status(s.error, true);
        return;
      }
      this._status('');
      this._setCmd(Object.assign({}, s, {grip: this.cmd.grip}));
      this.marker.setAttribute('cx', x); this.marker.setAttribute('cy', -z);
      return tip;
    }
    _toSvg(e) {
      const pt = this.svg.createSVGPoint(); pt.x = e.clientX; pt.y = e.clientY;
      const p = pt.matrixTransform(this.svg.getScreenCTM().inverse());
      return [p.x, -p.y];
    }
    _bind() {
      let id = null;
      const end = e => { if (id === null || (e && e.pointerId !== id)) return; id = null; this.svg.classList.remove('dragging'); };
      this.svg.addEventListener('pointerdown', e => {
        id = e.pointerId; this.svg.setPointerCapture(id); this.svg.classList.add('dragging');
        this._aim(...this._toSvg(e)); e.preventDefault();
      });
      this.svg.addEventListener('pointermove', e => { if (e.pointerId === id) this._aim(...this._toSvg(e)); });
      this.svg.addEventListener('pointerup', end); this.svg.addEventListener('pointercancel', end);
      this.svg.addEventListener('lostpointercapture', end);
      this.handle.addEventListener('keydown', e => {
        const step = e.shiftKey ? 20 : 5, t = forward(this.cmd).tip;
        const moves = {ArrowLeft: [-step, 0], ArrowRight: [step, 0], ArrowUp: [0, step], ArrowDown: [0, -step]};
        if (moves[e.key]) { this._aim(t[0] + moves[e.key][0], t[1] + moves[e.key][1]); e.preventDefault(); }
        else if (e.key === 'Enter' || e.key === ' ') { this.toggleGrip(); e.preventDefault(); }
      });
      this.handle.addEventListener('focus', () => this.showReach(true));
      this.handle.addEventListener('blur', () => this.showReach(false));
    }
    _kick() { if (!this._raf) { this._last = performance.now(); this._raf = requestAnimationFrame(t => this._tick(t)); } }
    _tick(now) {
      this._raf = null;
      const dt = Math.min(0.032, (now - this._last) / 1000); this._last = now;
      let moving = false;
      // hydraulic feel: a stiff, well-damped spring (about 4 % overshoot) on every joint
      const k = 170, c = 2 * 0.72 * Math.sqrt(k);
      for (const name of JOINTS) {
        const j = this.j[name];
        if (reduced()) { j.x = j.t; j.v = 0; continue; }
        j.v += (k * (j.t - j.x) - c * j.v) * dt; j.x += j.v * dt;
        if (Math.abs(j.t - j.x) > 0.05 || Math.abs(j.v) > 0.05) moving = true; else { j.x = j.t; j.v = 0; }
      }
      if (this.ball) moving = this._ballStep(dt) || moving;
      this._draw();
      if (moving) this._raf = requestAnimationFrame(t => this._tick(t));
    }
    _ballStep(dt) {
      const b = this.ball, f = forward(this.angles), tip = f.tip;
      const closed = this.j.grip.x < 35;
      const centre = [tip[0] + Math.cos(f.hand) * -8, tip[1] + Math.sin(f.hand) * -8];   // between the fingertips
      if (!b.held && closed && Math.hypot(centre[0] - b.x, centre[1] - b.z) < BALL_R + 6) {
        b.held = true; this._status('Got it. Open the gripper to drop it.');
      } else if (b.held && !closed) { b.held = false; b.vz = 0; }
      if (b.held) { b.x = centre[0]; b.z = Math.max(BALL_R, centre[1] - BALL_R * 0.4); return false; }
      if (b.z > BALL_R || b.vz) {
        if (reduced()) { b.z = BALL_R; b.vz = 0; return false; }
        b.vz -= GRAVITY * dt; b.z += b.vz * dt;
        if (b.z <= BALL_R) { b.z = BALL_R; b.vz = Math.abs(b.vz) > 250 ? -b.vz * 0.45 : 0; }   // bounce, then settle
        return b.vz !== 0 || b.z > BALL_R;
      }
      return false;
    }
    _draw() {
      const a = this.angles, spread = 6 + 0.28 * a.grip;        // grip 0..100 % opens the fingers 6..34 mm
      this.gSh.setAttribute('transform', `translate(${SH_X} ${-SH_Z}) rotate(${n2(-a.shoulder)})`);
      this.gEl.setAttribute('transform', `translate(${L1} 0) rotate(${n2(a.elbow)})`);
      this.gWr.setAttribute('transform', `translate(${L2} 0) rotate(${n2(-a.wrist)})`);
      this.gF1.setAttribute('transform', `translate(0 ${n2(-spread / 2)})`);
      this.gF2.setAttribute('transform', `translate(0 ${n2(spread / 2)})`);
      const tip = forward(this.cmd).tip;
      for (const e of [this.handle, this.ring]) { e.setAttribute('cx', tip[0]); e.setAttribute('cy', -tip[1]); }
      this.marker.style.display = this.o.follow === 'external' ? '' : 'none';
      if (this.ballEl) { this.ballEl.setAttribute('cx', this.ball.x); this.ballEl.setAttribute('cy', -this.ball.z); }
      this.handle.setAttribute('aria-valuetext',
        `shoulder ${Math.round(a.shoulder)}°, elbow ${Math.round(a.elbow)}°, wrist ${Math.round(a.wrist)}°, gripper ${a.grip > 40 ? 'open' : 'closed'}`);
    }
  }

  root.SolarCrane = {Crane, POSES, LIMITS, solve, solveBest, forward, bodySVG, armSVG, dims: {L1, L2, L3, SH_X, SH_Z}};
})(window);
