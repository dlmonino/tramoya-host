**Español** · [English](project-status.en.md) · [TRAMOYA Host](../README.md)

# Estado y validación de TRAMOYA

Actualizado: **9 de octubre de 2026**, cierre de pruebas a las 23:29
(Europe/Madrid). Este registro resume los resultados aportados por el
responsable del proyecto y revisados junto con los registros locales.

## Qué está validado

Se ha grabado y probado físicamente `esp32s3-servidor-diagnostico`, un proyecto
independiente de este repositorio, en una ESP32-S3-WROOM-1 N16R8 de revisión
v0.2. El firmware contiene páginas inocuas y no integra contenido PS5.
La segunda placa queda como referencia con `hello_world`, ya ejecutado.

| Prueba | Resultado observado |
|---|---|
| Grabación | Bootloader, tabla y aplicación verificados por esptool; hashes comprobados antes de grabar; sin borrado total. |
| Arranque | El diagnóstico arrancó tras pulsar RST con BOOT suelto. No fue necesario regrabar. |
| PSRAM | 8.388.608 bytes detectados; prueba interna correcta, con test de memoria del SDK habilitado. No es una verificación exhaustiva de todas las celdas. |
| Wi-Fi | AP `ESP32S3-Diagnostico` con WPA2 accesible desde el Mac. |
| DHCP | Cliente `192.168.4.2/24`, servidor y DNS `192.168.4.1`; sin opción de router. |
| DNS | `diagnostico.test A`: NOERROR y `192.168.4.1`; AAAA: NOERROR sin direcciones; `example.com A`: NXDOMAIN. |
| HTTP | `/status.json` responde con identidad, versión IDF y pruebas internas correctas. |
| HTTPS | TLS 1.2 y HTTP 200; certificado y nombre `diagnostico.test` verificados con confianza explícita en el proceso de prueba; `otro.test` rechazado. |
| Aislamiento | Sin rutas públicas IPv4/IPv6 por la interfaz Wi-Fi probada; reenvío IP del Mac desactivado; conexiones externas fallidas mientras los servicios locales respondían. |
| USB MSC | Dispositivo `ESP32-S3 Diagnostico`, VID:PID `303A:4002`; medio de unos 13,5 MB y partición FAT12 de unos 13,2 MB. |
| Archivos MSC | Lectura de `DIAG.TXT`, escritura y lectura de 65.536 bytes con sincronización, expulsión y verificación posterior del mismo SHA-256. |
| Estabilidad | Diez minutos de HTTP, HTTPS y lectura MSC: cero errores y ningún reinicio detectado. |

La persistencia se verificó después del paso indicado de desconexión y
reconexión. El hash acredita la integridad del contenido; el ciclo físico
se toma del contexto de la ejecución del usuario, sin medición independiente.

### Ensayo de estabilidad

- Intervalo: 23:19:00–23:29:01; duración registrada: 601,06 segundos.
- 61 muestras, 122 consultas HTTP/HTTPS y 61 lecturas MSC de 65.536 bytes.
- HTTP seguido de HTTPS y lectura MSC concurrentes cada diez segundos;
  sin escrituras MSC durante este ensayo.
- Lecturas MSC con `F_NOCACHE` solicitado y aceptado por macOS.
- Uptime creciente de 234.057 a 835.065 ms, sin reinicios detectados.
- Memoria interna observada por HTTP tras dos minutos de calentamiento:
  mínimo, máximo y medianas de 227.659 bytes; variación entre medianas: 0.

El resultado acredita ese intervalo y esa carga. No demuestra ausencia de
fugas lentas, fiabilidad prolongada, comportamiento de archivos grandes ni
funcionamiento en PS5. No se ha medido una reducción de consumo o temperatura.

### Identificación del diagnóstico

| Dato | Valor |
|---|---|
| SDK | ESP-IDF 5.4.2 |
| Memoria y CPU | Flash 16 MB; PSRAM octal a 80 MHz; CPU fija a 160 MHz |
| Aplicación | 963.312 bytes |
| Bootloader / tabla / aplicación | `0x0` / `0x8000` / `0x10000` |
| Partición de aplicación | 3 MiB |
| Storage | `0x310000`–`0x1000000`, 13.565.952 bytes |
| Logs configurados | UART0, 115200 baudios, TX GPIO43 y RX GPIO44 |

SHA-256 de la aplicación grabada:

```text
e47eb8c2f56d2f828f11683e7a20eacb7768c3a7e2e690e6406d2716196a8e8d
```

SHA-256 del archivo MSC de prueba `TRMYTEST.BIN` (65.536 bytes):

```text
f9e123dd7c4b963886afa28e2153689634f7c31d6547c7bedb41068f2ee6ca07
```

Estos hashes identifican los artefactos de la sesión; no son una release del
firmware de este repositorio. El diagnóstico, sus imágenes y sus comprobadores
se conservan por separado y no se añaden con esta actualización documental.

## Qué sigue pendiente en este repositorio

La revisión base es `70aed11faa1a657cd25cc5a52781c2b3764398f5`. La preparación
del firmware completo, denominada etapa A en el registro de trabajo, sigue
sin cerrar. Se adquirieron y verificaron componentes USB, miniz y fuentes del
frontend, Relapse y Payload Manager. Quedan dependencias y preparación por
resolver, incluidas las traducciones de Payload Manager y otras fuentes y
binarios del flujo seleccionado. No hay una compilación completa validada de
ese flujo en esta sesión.

Las fuentes ya adquiridas no necesitan descargarse de nuevo por el mero hecho
de retomar el trabajo. Hay que fijar el alcance, resolver las dependencias
necesarias y registrar revisiones, hashes y licencias. La presencia de datos
para firmware 13.60 en unas fuentes no prueba su funcionamiento en la consola.

El diagnóstico desmonta FAT de la aplicación y cede el volumen al ordenador;
HTTP sirve contenido integrado. La base sirve `frontend.zip` desde FAT y usa
otro modelo de montaje. **La escritura MSC validada no demuestra todavía la
actualización del frontend servido ni la coordinación de accesos del firmware
completo.**

## Antes de probar en PS5

1. Completar la preparación del firmware completo y seleccionar el contenido
   y las versiones aplicables a PS5 13.60, con procedencia y licencias registradas.
2. Integrar y compilar para N16R8 con un conjunto coherente de imágenes y tabla
   de particiones. Revisar el AP, WPA2, DHCP, DNS y ausencia de reenvío a Internet;
   la base conserva un AP abierto y no hereda por sí sola el diagnóstico WPA2.
3. Validar la actualización real de `frontend.zip` desde el ordenador:
   propiedad exclusiva del volumen durante la escritura, expulsión, reinicio,
   integridad y contenido nuevo servido. Usar archivos de tamaño representativo.
4. Repetir arranque, PSRAM, red, HTTP/HTTPS, aislamiento, MSC y estabilidad
   sobre esa compilación final; ampliar la duración respecto a los diez minutos.
5. Preparar la entrada de navegador, DNS y tratamiento de HTTPS en la PS5.
   La aceptación explícita del certificado en el comprobador del Mac no implica
   que la consola lo acepte. Revisar además alimentación USB y recuperación.

La primera prueba en consola debe comprobar una página inocua y conectividad
local aislada antes de pasar al contenido PS5. No se ha probado la consola,
ningún payload ni una cadena homebrew con TRAMOYA. La segunda placa de
referencia y el juego separado de recuperación `hello_world` se conservan.

## Evidencias y continuidad

Los originales permanecen en el archivo local de trabajo del proyecto:

- `registro-pruebas-tramoya.txt`: cronología y salidas revisadas.
- `verificacion-artefactos.json`: comprobaciones previas a la grabación;
  su estado «pendiente de pruebas físicas» describe ese momento anterior.
- `estabilidad-20261009-231900-672050.jsonl` y
  `estabilidad-20261009-231900-672050-resumen.json`: muestras y resumen del ensayo.
- `tramoya-etapa-A-informe.txt`: estado de preparación del firmware completo.
- Bitácora de Drive actualizada con el cierre del diagnóstico y los pendientes.

Este documento es un resumen de esos registros. Los archivos locales no están
incluidos ni enlazados como si fueran accesibles desde este repositorio público.
