// Writes the crane drawing from firmware/car/web/crane.js to tools/rover_parts.json, so
// tools/progress.py can put the very same rover in the README banner without a browser.
//   node tools/rover_svg.js           # write it
//   node tools/rover_svg.js --check   # exit 1 if it's out of date (CI)
const fs = require('fs'), path = require('path');
const root = path.join(__dirname, '..');
global.window = {matchMedia: () => ({matches: false})};
eval(fs.readFileSync(path.join(root, 'firmware/car/web/crane.js'), 'utf8'));
const out = path.join(__dirname, 'rover_parts.json');
const text = JSON.stringify({body: window.SolarCrane.bodySVG(), arm: window.SolarCrane.armSVG()}, null, 1) + '\n';
if (process.argv.includes('--check')) {
  const same = fs.existsSync(out) && fs.readFileSync(out, 'utf8') === text;
  console.log(same ? 'rover drawing is up to date' : 'tools/rover_parts.json is out of date: run node tools/rover_svg.js');
  process.exit(same ? 0 : 1);
}
fs.writeFileSync(out, text);
console.log('wrote tools/rover_parts.json');
