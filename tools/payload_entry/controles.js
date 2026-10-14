export function instalarControles(documento, cargar, log) {
  let estado = 'listo';
  const panel = documento.createElement('section');
  const aviso = documento.createElement('p');
  aviso.textContent = 'Solo tras un arranque limpio. No cargar etaHEN ni repetir payloads en esta sesión.';
  panel.appendChild(aviso);
  const botones = [];
  function boton(texto, accion) {
    const b = documento.createElement('button');
    b.textContent = texto;
    b.style.cssText = 'font:22px sans-serif;padding:18px;margin:12px;';
    b.disabled = true;
    b.addEventListener('click', accion);
    panel.appendChild(b);
    botones.push(b);
    return b;
  }
  function bloquear() { botones.forEach(b => { b.disabled = true; }); }
  async function enviar(nombre, esperado, siguiente) {
    if (estado !== esperado) return;
    estado = 'enviando';
    bloquear();
    try {
      await cargar(nombre);
      estado = siguiente;
      log(nombre + ': envío TCP terminado; comprueba la notificación en la PS5.');
      if (siguiente === 'confirmar') confirmar.disabled = false;
      else log('TRAMOYA: envíos terminados. El envío no acredita funcionamiento ni estabilidad.');
    } catch (error) {
      estado = 'detenido';
      bloquear();
      log('DETENIDO: ' + String(error) + '. No repetir ni cargar otros payloads; conserva el error.', 'error');
    }
  }
  const kstuff = boton('1. Enviar kstuff Lite 1.11', () => enviar('kstuff.elf', 'listo', 'confirmar'));
  const confirmar = boton('2. Confirmo que kstuff inició sin errores', () => {
    if (estado !== 'confirmar') return;
    estado = 'shadowmount';
    bloquear();
    shadowmount.disabled = false;
    shadowmount.focus();
  });
  const shadowmount = boton('3. Enviar ShadowMount+ 1.7beta5-fix1', () =>
    enviar('shadowmountplus.elf', 'shadowmount', 'terminado'));
  documento.body.insertBefore(panel, documento.body.firstChild);
  kstuff.disabled = false;
  kstuff.focus();
  log('TRAMOYA: cargador listo; los payloads requieren pulsaciones independientes.');
}
