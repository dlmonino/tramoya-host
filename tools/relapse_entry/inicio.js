'use strict';
(function () {
  var boton = document.getElementById('iniciar');
  var estado = document.getElementById('estado');
  var marco = document.getElementById('relapse');
  marco.src = 'about:blank';
  var version = /PlayStation 5\/(\d+\.\d+)/.exec(navigator.userAgent);
  if (!version || version[1] !== '13.60') {
    estado.textContent = 'Esta integración requiere PS5 13.60. Inicio desactivado.';
    return;
  }
  estado.textContent = 'PS5 13.60 detectada. Preparado para comenzar.';
  boton.disabled = false;
  boton.focus();
  // La cadena permanece dentro del marco y solo se carga al pulsar el botón.
  boton.addEventListener('click', function () {
    boton.disabled = true;
    marco.src = '/relapse/index.html';
  }, {once: true});
}());
