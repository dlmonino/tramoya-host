'use strict';
// Solo se ejecuta la puerta de entrada; ningún código de Relapse se evalúa.
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const code = fs.readFileSync(process.argv[2], 'utf8');
function simulate(userAgent) {
  const calls = [];
  const button = {disabled: true, focus() {}, addEventListener(event, fn, opts) {
    assert.strictEqual(event, 'click');
    assert.strictEqual(opts.once, true);
    this.click = fn;
  }};
  const status = {};
  const frame = {set src(url) {calls.push(url);}};
  const context = {
    navigator: {userAgent: userAgent},
    document: {getElementById(id) {
      return id === 'iniciar' ? button : id === 'relapse' ? frame : status;
    }}
  };
  vm.runInNewContext(code, context, {timeout: 1000});
  assert.deepStrictEqual(calls, ['about:blank'], 'Debe limpiar el marco sin iniciar la cadena');
  return {button, status, calls};
}
for (const userAgent of ['Mozilla/5.0 Macintosh', 'PlayStation 5/13.42',
  'PlayStation 5/14.00', 'PlayStation 5/13.600', 'PlayStation 4/13.60']) {
  const result = simulate(userAgent);
  assert.strictEqual(result.button.disabled, true, userAgent);
  assert.strictEqual(result.button.click, undefined, userAgent);
}
const result = simulate('Mozilla/5.0 (PlayStation; PlayStation 5/13.60) AppleWebKit/605.1.15');
assert.strictEqual(result.button.disabled, false);
result.button.click();
assert.deepStrictEqual(result.calls, ['about:blank', '/relapse/index.html']);
assert.strictEqual(result.button.disabled, true);
console.log('PASS: marco limpiado; cinco navegadores rechazados; 13.60 exige pulsación antes de cargar');
