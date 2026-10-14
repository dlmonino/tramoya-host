**Español** · [English](payloads.en.md) · [Relapse mínimo](integracion-relapse.md)

# Payloads autónomos para PS5 13.60

Actualizado el 15 de octubre de 2026. David eligió mantener TRAMOYA autónomo:
**Relapse → elfldr → kstuff Lite → ShadowMount+**, sin etaHEN. **Prueba física
superada en su PS5 13.60:** confirma que los payloads han cargado y que ejecutó
ShadowMount+ y funcionó. Se acepta su observación directa como resultado válido
de esa ejecución; las pruebas de repetibilidad y estabilidad siguen pendientes.

## Componentes fijados

| Componente | Versión y función | SHA-256 del ELF |
|---|---|---|
| kstuff Lite | v1.11; parches de ejecución que necesita ShadowMount+. Release declara firmware 1.00–13.60. | `ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779` |
| ShadowMount+ | 1.7-beta5fix1; detecta y monta contenido desde almacenamiento. Requiere kstuff y esta release pide no usar etaHEN. | `e85fd63b705498488f39c09c9aea6f9fa22e0df2e932865de49a8cedf25172d5` |

Fuentes: [kstuff v1.11](https://github.com/EchoStretch/kstuff-lite/releases/tag/v1.11)
y [ShadowMount+ 1.7-beta5fix1](https://github.com/drakmor/ShadowMountPlus/releases/tag/1.7-beta5fix1).
Los commits, tamaños, assets y hashes están fijados en
`tools/payloads-13.60.lock.json`; el constructor no consulta «latest» ni descarga.
Los bytes coinciden con los digests publicados por GitHub. No se han recompilado
los ELF ni probado todas sus funciones. El paquete conserva los avisos MIT de
Relapse, añade la licencia GPL de ShadowMount y registra ambas procedencias.
Los binarios permanecen fuera de Git. No se publica nada con esta entrega.

## etaHEN: alternativa separada

El port no oficial [etaHEN Unified r1](https://github.com/GronedWaffel/etahen-11.00-13.60/releases/tag/v2.5B-unified.1)
ya incorpora kstuff v1.11: no se debe añadir una segunda copia. Proporciona
Toolbox, plugins, FPS y otras herramientas que no aporta esta combinación.
Su ELF exacto mide 36.746.616 bytes, SHA-256
`46c161473b27d12f1fa5e2d1d10466ce587db841ed9e8d71a219424d8fcf637f`;
comprimido en una entrada ZIP con DEFLATE9 ocupa 12.297.655 bytes. Supera los
8 MiB físicos de PSRAM: el firmware actual no puede guardar su instantánea.
Se conserva descargado fuera del paquete; no se ha ejecutado aquí.

La exclusión con etaHEN corresponde a ShadowMount **1.7-beta5fix1**, no a todas
sus versiones. Existe un par adaptado publicado por el port en
[v2.5B-13.60-r3](https://github.com/GronedWaffel/etahen-11.00-13.60/releases/tag/v2.5B-13.60-r3).
Es otra combinación, no seleccionada ni validada por David; no se midió aquí
su tamaño comprimido. No se sustituyen versiones ni mezclan esos payloads.

## Construcción y cambios mínimos

```sh
python3 tools/build_payload_frontend.py --target esp32s3-n16r8 \
  --source "$TRAMOYA_RELAPSE_SOURCE" \
  --payload-dir "$TRAMOYA_PAYLOAD_DIR" --out build/payloads/frontend.zip
```

Las variables deben apuntar a las fuentes originales fijadas de Relapse y a
los dos ELF oficiales del lock. El constructor valida ambos conjuntos y
rechaza alteraciones, enlaces, destinos existentes y resultados en las fuentes.
No modifica el empaquetador mínimo, las fuentes conservadas ni el firmware.

Paquete candidato: **56 archivos, 2.297.244 bytes**, SHA-256
`5cb0b53e9aa285f4882bb5019b8b6cc5f23a84fb70732e8b73de0054becb1c90`.
El perfil limita el ZIP a 3 MiB; el presupuesto mínimo anterior de 1 MiB no cambia.
Ese techo es una guarda de empaquetado, no una medida de memoria libre. El
firmware sigue exigiendo que el ZIP quepa en el bloque mayor libre de PSRAM
con 512 KiB adicionales de margen. El uso físico del perfil con este ZIP mayor queda confirmado por David;
no se ha medido el margen libre de PSRAM.
Aplicación N16R8: 927.920 bytes, SHA-256
`334f23534285f706fdf69e12b9210b5f0e6566770711f851d64cb887fac42e0a`,
idéntica a la candidata física. No es necesario grabar flash.

Se sustituye el gancho conjunto R2 por botones manuales tras elfldr. Un envío
no habilita el siguiente automáticamente: David debe observar el inicio de
kstuff y confirmarlo antes de enviar ShadowMount. La lógica de WebKit/kernel,
los dos binarios mínimos y el transporte original se conservan; el adaptador
solo acepta los dos nombres fijados y libera la copia de envío al terminar.
«Envío terminado» acredita transferencia TCP, no arranque ni funcionamiento.
Un error bloquea los envíos de esa página; no se reintenta en la misma sesión.
Los botones evitan duplicados dentro de la página, no detectan payloads que se
hubieran cargado desde otra página o antes de recargar. Siempre empezar con
arranque limpio y no reabrir/repetir la cadena dentro de esa sesión.

## Verificaciones y recuperación

Normal y ASan/UBSan con FAT obligatoria: cada receta pasa 54 pruebas C/9.252
aserciones, cinco de energía y seis FAT. Compilación N16R8 con ESP-IDF 5.4.2 pasa.
Las diez pruebas de la integración mínima se conservan y pasan en ambos modos.
El nuevo perfil pasa seis pruebas por modo: inventario/hashes/repetibilidad,
fuentes preservadas, rechazo de alteraciones y enlaces, no sobrescritura y
presupuesto, controles manuales simulados y cada recurso en cuatro familias
de rutas mediante el núcleo HTTP C real. Los controles incluyen doble clic,
orden, confirmación, errores y liberación. No ejecutan código en la PS5.
FAT candidata: ZIP y WIFI.TXT íntegros, 112 lecturas C tras extracción.

El kit USB separado conserva como recuperación el ZIP mínimo físicamente
probado, SHA-256 `8335f216cc9d590130fe567e5862de092a53e59b4f6e540e3deafb97f0934529`.
Exige en0 local, certificado, testigo de cesión MSC y contenido anterior exacto;
respalda ZIP/WIFI antes de escribir y comprueba la instantánea anterior mientras
se escribe FAT desde el Mac. WIFI.TXT no se reescribe. Tras expulsar/reiniciar
compara los 56 recursos HTTPS, sin ejecutarlos. Los recursos grandes disponen
de hasta 90 segundos por petición. Los kits inocuos y hello_world anteriores,
la otra placa, WPA2 y tramoya_host/tramoya_host se conservan.

Evidencias y secuencia agrupada en el archivo local de la sesión:
`outputs/payloads-20261015/kit-usb/PROCEDIMIENTO.txt`. El kit y sus pruebas se
identifican mediante SHA256SUMS. La restauración del ZIP solo revierte el
contenido servido por el ESP32; no desinstala componentes o archivos de la PS5.

**Resultado físico — 15 de octubre:** David confirma «Han cargado los playloads,
he ejecutado shadowmount y ha funcionado». La carga de los payloads y el
funcionamiento observado de ShadowMount+ quedan registrados como PASS. El
ensayo USB local figura ESCRITO, con testigo MSC, HTTPS anterior y durante la
escritura correctos, y respaldos ZIP/WIFI íntegros. No consta allí la ejecución
posterior de `verificar`: la comprobación individual de los 56 recursos no se
presenta como realizada. Esto no invalida la prueba funcional directa de David.
El resultado amplía los manifiestos de preparación congelados, en el archivo
local `outputs/payloads-20261015/prueba-fisica-ps5/resultado.json`.

**Pendiente:** operaciones concretas sobre contenido y formatos no detalladas
en esta confirmación, repetibilidad, restauración de sesión y estabilidad de
esta combinación. El ensayo previo de 30 minutos corresponde al frontend inocuo.
HTTP56 sigue sin causa identificada. Se conserva el ZIP mínimo de recuperación.

El 15 de octubre David decidió no realizar la prueba adicional propuesta de
repetibilidad y estabilidad en esta etapa. No se programa como requisito para
continuar; el alcance de la validación física anterior se conserva.
