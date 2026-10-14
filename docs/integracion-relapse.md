**Español** · [English](relapse-integration.en.md) · [Estado](estado-proyecto.md)

# Integración mínima de Relapse para PS5 13.60

Preparada el 14 de octubre de 2026. La compatibilidad indicada es la declarada
por los proyectos y contrastada con las fuentes conservadas. **La prueba mínima
en la PS5 13.60 de David está superada:** sus fotos muestran WebKit preparado,
kernel con lectura/escritura y privilegios listos, elfldr retornando `0x0` y
el cierre de TRAMOYA con opcionales desactivados. Se registra como validación
física de la cadena mínima y del arranque del cargador ELF en esa ejecución.
Su observación directa previa confirmó también la página inocua de TRAMOYA en
la Guía, conectada a `tramoya_host` solo por Wi-Fi y sin LAN.

## Inventario y selección

| Componente conservado | Evidencia para 13.60 | Decisión para esta entrega |
|---|---|---|
| Relapse `d8e6896b5cb33b04f1e038a5d698d3d4aee5947c` | Su README declara 7.00–13.60; los 53 archivos locales coinciden con el commit. La cadena mínima terminó correctamente en la PS5 13.60 de David. | Prueba física superada. |
| `elfldr-ps5-1360.elf` y `kexp_2026_05_25.bin` | La ejecución física llegó a privilegios listos, retorno elfldr `0x0` y cierre correcto. No se ha reproducido su compilación independiente. | Incluidos y ejecutados como parte de la cadena mínima; el perfil posterior probó kstuff y ShadowMount+ (véase payloads.md). |
| WebKit Autoloader `f5e57c492a4a7b5ccd28ecddb43f8c325cf93aa2` | Su tabla selecciona Relapse para 13.60 y el README declara soporte. El árbol conservado aún carece de parte de las dependencias preparadas. | Conservado como ruta futura; esta prueba no instala el acceso directo en PS5. |
| Unified Autoloader `915a65e232e03e293829e34bca18866659b2bcf4` y Payload Manager `e4e72dc9d831dd09ee8fabc222ad1e7f5139050e` | Hay fuentes; faltan binarios preparados y parte de las dependencias de esa ruta. | No se incluyen en la prueba mínima. |
| UMTX2 y Slopkit/Poops | En la tabla del frontend conservado, UMTX2 llega a 5.50 y Poops a 12.00; sus gitlinks no equivalen a fuentes adquiridas. | Fuera del objetivo 13.60. |
| `kstuff.elf` local | La cadena de texto indica Kstuff Lite 1.11. La release oficial v1.11 declara 1.00–13.60, pero su digest publicado difiere del archivo local. | Compatibilidad exacta de este archivo sin acreditar; excluido. |
| `shadowmountplus.elf` local | Contiene `1.7beta1`. No coincide con los digests de las 30 releases consultadas; el soporte actual del proyecto no identifica estos bytes. | Compatibilidad exacta pendiente; excluido. |
| `etaHEN.elf` local | Es un envoltorio ELF, distinto del `.bin` oficial de etaHEN 2.5B. No se ha establecido versión y soporte 13.60 para ese archivo exacto. | Compatibilidad exacta pendiente; excluido. |

Fuentes primarias consultadas: [Relapse](https://github.com/ntfargo/Relapse-Exploit),
[WebKit Autoloader](https://github.com/itsPLK/ps5-webkit-autoloader),
[Kstuff Lite v1.11](https://github.com/EchoStretch/kstuff-lite/releases/tag/v1.11),
[ShadowMount+](https://github.com/drakmor/ShadowMountPlus/releases) y
[etaHEN](https://github.com/etaHEN/etaHEN/releases/tag/2.5B).
Las respuestas de la API y los hashes locales quedan en el archivo de la sesión,
`outputs/integracion-relapse-20261014/`. No se descargaron nuevas versiones de
payloads durante la preparación. La ejecución física posterior se documenta
por separado en `prueba-fisica-ps5/resultado.json`.

## Paquete preparado

`tools/build_relapse_frontend.py` valida los 53 archivos con
`tools/relapse-13.60.lock.json`. Produce un ZIP de 51 archivos y **260.378 bytes**,
SHA-256 `8335f216cc9d590130fe567e5862de092a53e59b4f6e540e3deafb97f0934529`.
El ZIP queda muy por debajo del presupuesto conservador de 1 MiB del empaquetador.
La carga física de este ZIP pasó; no se midió el margen libre de PSRAM ni se
probaron ZIP mayores.

La página raíz espera una pulsación y solo habilita el botón en el navegador
identificado como PS5 13.60. El botón carga `/relapse/index.html` dentro del
marco de registro; esa página interna ejecuta Relapse al abrirse. El marco se
limpia antes de cargar el controlador para descartar una ruta restaurada tras
un fallo del navegador. La puerta inicial no es una frontera de
seguridad: abrir directamente esa ruta también inicia la cadena.
La restauración real de sesión de la Guía y del marco queda pendiente de
comprobación física; la simulación verifica la limpieza al ejecutar el código.

La única adaptación de la lógica de Relapse desactiva el gancho R2 que cargaría
kstuff, ShadowMount+ y etaHEN conjuntamente. La cadena WebKit/kernel y sus dos
binarios se conservan. Los nombres del ZIP se convierten a minúsculas para el
índice del firmware, conservando los bytes y los avisos MIT. El cargador ELF
original queda escuchando en 9021; no se afirma que esté limitado a localhost.
Se usa la red local aislada ya validada.

La construcción no modifica las fuentes conservadas, no sobrescribe un resultado
existente y no hace descargas, commits ni acceso al USB o a la PS5. Ejemplo:

```sh
python3 tools/build_relapse_frontend.py \
  --source "$TRAMOYA_RELAPSE_SOURCE" \
  --out build/integracion-relapse/frontend.zip --target esp32s3-n16r8
```

La variable debe apuntar a la copia local de Relapse fijada en el lock.
El archivo `frontend.zip-source.json` registra procedencia, adaptación, archivos
y entorno zlib. Los binarios no se incorporan a Git; se conservan fuera de él.

## Verificación y recuperación

Las suites normales y ASan/UBSan con FAT obligatoria conservan su cobertura.
Las pruebas adicionales comprueban fuentes alteradas, enlaces, no sobrescritura,
repetibilidad, puerta manual, sintaxis JavaScript sin ejecutar Relapse y todos
los recursos con el núcleo HTTP C real en rutas directas, `/app/` y Guía en
español e inglés. Se verifica también el ZIP candidato tras un recorrido FAT
real. La carga física del ZIP y la cadena en PS5 pasaron después: el registro
USB VERIFICADO de las 23:43 del 14 de octubre conserva 51 respuestas HTTPS
íntegras y WIFI.TXT comprobado. Las fotos IMG_4373/4374 muestran la finalización
en la consola. Los hashes del firmware son referencias locales; no se leyó flash.

El firmware N16R8 no cambia: aplicación **927.920 bytes**, SHA-256
`334f23534285f706fdf69e12b9210b5f0e6566770711f851d64cb887fac42e0a`.
La actualización se prepara por USB, sin grabar flash. El kit local conserva
el ZIP inocuo probado, certificado público y referencia de WIFI.TXT. Antes de
escribir exige el testigo de cesión MSC, el contenido HTTPS anterior y los
hashes del volumen. Respalda ZIP/WIFI, sincroniza la copia y verifica que la web
sigue sirviendo la instantánea anterior hasta expulsar y reiniciar. Después
compara por HTTPS los 51 recursos con el ZIP, sin ejecutar JavaScript.

WIFI.TXT no se reescribe. Se conserva `tramoya_host` / `tramoya_host`, WPA2,
la placa de referencia y los juegos de recuperación anteriores. La restauración
del ZIP devuelve la página inocua al ESP32; no promete revertir acciones de
payloads en la consola. El error HTTP 56 histórico sigue sin causa identificada.

La secuencia física agrupada y los manifiestos de preparación se conservan
congelados en el kit; los resultados posteriores los amplían. En la prueba
mínima no se cargó otro ELF. El 15 de octubre David confirmó la carga de los
payloads y el funcionamiento de ShadowMount+ en el [perfil autónomo](payloads.md)
posterior. Siguen pendientes repetibilidad, estabilidad y restauración real de
sesión de la Guía. Los 30 minutos previos corresponden al ZIP inocuo.


## Siguiente perfil autónomo

[Siguiente perfil autónomo: kstuff + ShadowMount+](payloads.md).
