'use strict';
const fs = require('fs');
const vm = require('vm');
const assert = require('assert').strict;
const path = require('path');
const root = path.resolve(__dirname, '..');

function interfaz(cargar) {
  const nodes = [];
  const doc = {
    createElement(tag) {
      const node = {tag, style: {}, children: [], appendChild(n) { this.children.push(n); },
        addEventListener(event, action) { this.action = action; }, focus() {}};
      nodes.push(node);
      return node;
    },
    body: {insertBefore() {}, firstChild: null}
  };
  const ctx = {document: doc};
  vm.createContext(ctx);
  vm.runInContext(fs.readFileSync(path.join(root, 'payload_entry/controles.js'), 'utf8')
    .replace('export function', 'function') + ';this.install = instalarControles;', ctx);
  const logs = [];
  ctx.install(doc, cargar, (...args) => logs.push(args));
  return {buttons: nodes.filter(n => n.tag === 'button'), logs};
}

async function main() {
  let calls = [];
  const ui = interfaz(async name => calls.push(name));
  const [k, confirmar, sm] = ui.buttons;
  assert.deepEqual(calls, []);
  assert.equal(k.disabled, false);
  assert.equal(confirmar.disabled, true);
  assert.equal(sm.disabled, true);
  await sm.action(); confirmar.action();
  assert.deepEqual(calls, []);
  await k.action();
  assert.deepEqual(calls, ['kstuff.elf']);
  assert.equal(confirmar.disabled, false);
  assert.equal(sm.disabled, true);
  await k.action(); await sm.action();
  assert.deepEqual(calls, ['kstuff.elf']);
  confirmar.action();
  assert.equal(sm.disabled, false);
  await sm.action(); await sm.action(); confirmar.action(); await k.action();
  assert.deepEqual(calls, ['kstuff.elf', 'shadowmountplus.elf']);
  assert(ui.buttons.every(b => b.disabled));
  assert(ui.logs.some(l => l[0].includes('no acredita funcionamiento')));

  let release;
  calls = [];
  const parallel = interfaz(name => { calls.push(name); return new Promise(r => { release = r; }); });
  const pending = parallel.buttons[0].action();
  await parallel.buttons[0].action();
  parallel.buttons[1].action(); await parallel.buttons[2].action();
  assert.deepEqual(calls, ['kstuff.elf']);
  assert(parallel.buttons.every(b => b.disabled));
  release(); await pending;

  for (const failureAt of ['kstuff.elf', 'shadowmountplus.elf']) {
    const failed = interfaz(async name => { if (name === failureAt) throw new Error('simulado'); });
    await failed.buttons[0].action();
    if (failureAt !== 'kstuff.elf') { failed.buttons[1].action(); await failed.buttons[2].action(); }
    assert(failed.buttons.every(b => b.disabled));
    assert(failed.logs.some(l => l[0].startsWith('DETENIDO:') && l[1] === 'error'));
  }
  // Ejecutar el adaptador con transporte simulado; no se carga código PS5.
  const context = {SYS_MUNMAP: 73};
  vm.createContext(context);
  vm.runInContext(fs.readFileSync(path.join(root, 'payload_entry/cargar.js'), 'utf8')
    .replace('export async function', 'async function') + ';this.load = cargarPayloadTramoya;', context);
  const operations = [];
  context.mapElf = async name => { operations.push(['map', name]); return {base: 123, size: 17000}; };
  context.sendElf = async name => { operations.push(['send', name]); };
  const chain = {syscall: async (...args) => operations.push(args)};
  await assert.rejects(() => context.load('etaHEN.elf', {}, chain), /fuera del perfil/);
  assert.deepEqual(operations, []);
  await context.load('kstuff.elf', {}, chain);
  assert.deepEqual(operations, [['map', 'kstuff.elf'], ['send', 'kstuff.elf'], [73, 123, 32768]]);
  operations.length = 0;
  context.sendElf = async () => { throw new Error('socket simulado'); };
  await assert.rejects(() => context.load('shadowmountplus.elf', {}, chain), /socket simulado/);
  assert.deepEqual(operations, [['map', 'shadowmountplus.elf'], [73, 123, 32768]]);
  console.log('PASS: inicio manual, orden y confirmación, exclusión etaHEN, doble clic, errores y liberación.');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
