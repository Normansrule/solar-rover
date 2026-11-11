// Solar Rover website: the crane console and the theme switch.
(() => {
  const $ = id => document.getElementById(id);

  // ---------- theme: follows the system until you pick one ----------
  const themeBtn = $('theme');
  const current = () => document.documentElement.dataset.theme ||
    (matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark');
  const label = () => { const next = current() === 'dark' ? 'light' : 'dark';
    themeBtn.textContent = next === 'light' ? 'Light' : 'Dark';
    themeBtn.setAttribute('aria-label', 'Switch to the ' + next + ' theme'); };
  themeBtn.onclick = () => {
    const next = current() === 'dark' ? 'light' : 'dark';
    document.documentElement.dataset.theme = next;
    try { localStorage.setItem('theme', next); } catch (e) {}
    label();
  };
  label();

  // ---------- the crane ----------
  if (!window.SolarCrane) return;
  const status = $('craneStatus'), hint = $('craneHint');
  const fmt = v => (v < 0 ? '−' : '') + Math.abs(Math.round(v)) + '°';
  const poseBtns = document.querySelectorAll('[data-pose]');
  const markPose = name => poseBtns.forEach(b => { const on = b.dataset.pose === name; b.classList.toggle('sel', on); b.setAttribute('aria-pressed', on); });
  let readTimer = null;

  const crane = new SolarCrane.Crane($('stage'), {
    ball: true,
    label: 'Side view of Solar Rover with its crane arm and a ball on the ground ahead of it',
    onCommand: c => {
      $('rCmd').textContent = `A * ${Math.round(c.shoulder)} ${Math.round(c.elbow)} ${Math.round(c.wrist)} ${Math.round(c.grip)}`;
      const open = c.grip > 40;
      $('grip').textContent = open ? 'Close gripper' : 'Open gripper';
      $('grip').setAttribute('aria-pressed', open);
      hint.hidden = true;
      if (!readTimer) readTimer = requestAnimationFrame(readout);
    },
    onStatus: (text, err) => { status.textContent = text; status.classList.toggle('err', err); }
  });

  // the readout follows the drawn arm while it settles, not just the command
  function readout() {
    const a = crane.angles;
    $('rShoulder').textContent = fmt(a.shoulder); $('rElbow').textContent = fmt(a.elbow);
    $('rWrist').textContent = fmt(a.wrist); $('rGrip').textContent = Math.round(a.grip) + '%';
    const settled = ['shoulder', 'elbow', 'wrist', 'grip'].every(k => Math.abs(crane.j[k].x - crane.j[k].t) < 0.05);
    readTimer = settled ? null : requestAnimationFrame(readout);
  }

  poseBtns.forEach(b => b.onclick = () => { crane.pose(b.dataset.pose); markPose(b.dataset.pose); });
  $('grip').onclick = () => crane.toggleGrip();
  $('reset').onclick = () => { crane.reset(); markPose('ready'); hint.hidden = false; };
  $('stage').addEventListener('pointerdown', () => markPose(null));

  // the parts plate: the same drawing, still, with the parts numbered
  const plate = $('partsStage');
  if (plate) {
    const p = new SolarCrane.Crane(plate, {interactive: false, callouts: true, viewBox: '-196 -262 418 266',
      label: 'Side view of Solar Rover with numbered parts: 1 solar roof, 2 battery and charger, 3 gear motors, 4 power tray, 5 logic tray, 6 turret, 7 camera'});
    p.pose('park'); p.j.shoulder.x = 110; p.j.elbow.x = 120; p.j.wrist.x = -70; p.j.grip.x = 20;
  }

  // the one authored moment: the crane unfolds from its transport pose when the page opens
  crane.pose('park'); crane.j.shoulder.x = 110; crane.j.elbow.x = 120; crane.j.wrist.x = -70; crane.j.grip.x = 20;
  markPose('ready');
  setTimeout(() => { crane.pose('ready'); hint.hidden = false; status.textContent = ''; }, 500);
})();
