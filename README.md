**Español** · [English](README.en.md)

# TRAMOYA Host

TRAMOYA es un proyecto de servidor Wi-Fi autónomo sobre ESP32-S3, alimentado
por USB, para servir contenido homebrew a una PS5 sin darle acceso a Internet.
El objetivo de desarrollo es PS5 con firmware **13.60** y una placa **N16R8**
(16 MB de flash y 8 MB de PSRAM), con actualización de contenido mediante
almacenamiento USB MSC. El formato dongle se explorará más adelante.

Este repositorio es un fork de
[kxn/ps5-wkal-esp32s3](https://github.com/kxn/ps5-wkal-esp32s3), basado en la
revisión `70aed11faa1a657cd25cc5a52781c2b3764398f5`. Conserva su historial,
autorías y licencias. El nombre interno de los artefactos sigue siendo
`ps5_wkal_host`.

## Configuración Wi-Fi por USB

El firmware lee `WIFI.TXT` en la raíz del volumen USB al arrancar. Las nuevas
imágenes incluyen estos valores iniciales:

```ini
ssid=tramoya_host
password=tramoya_host
```

Se usa WPA2-PSK. Edita el fichero, expulsa el volumen y reinicia para aplicar
los cambios. Si falta o contiene datos inválidos, el Wi-Fi queda apagado y el
USB permite corregirlo. Consulta el [formato y procedimiento](docs/configuracion-wifi.md).
Los escenarios de cambio de credenciales, archivo ausente/inválido y recuperación USB se validaron físicamente el 14 de octubre; véase el estado detallado.

## Estado actual — 14 de octubre de 2026

**Validación funcional de WIFI.TXT cerrada para los escenarios ensayados.**
El firmware N16R8 basado en `7c197bc` se grabó con el ZIP de diagnóstico inocuo:
se probaron WPA2, DHCP, resolución A local, rechazo de dominios externos,
HTTP/HTTPS, aislamiento por la interfaz utilizada, cambios de credenciales y
recuperación de configuración por USB.
El defecto AAAA está corregido en software conforme al [contrato DNS](docs/dns.md);
su ensayo físico DNS y controles HTTP/HTTPS/aislamiento pasaron el 14 de octubre
(75 comprobaciones). El error HTTP curl 56
continúa sin causa identificada; curl 63 se debió al Wi-Fi doméstico del Mac.

El diagnóstico independiente del 9 de octubre y sus diez minutos de estabilidad
son evidencia histórica exclusiva de aquel firmware. Su placa se conserva como
referencia. La investigación controlada de HTTP 56 completó 61 peticiones sin reproducirlo;
la causa sigue abierta. La candidata con copia del frontend en PSRAM superó
la actualización USB con página inocua y conservación de WIFI.TXT el 14 de octubre.
La misma candidata superó físicamente 30 minutos de DNS, HTTP/HTTPS, lecturas
MSC y controles de aislamiento: 181 muestras completas, cero fallos y ningún
reinicio observado por CDC. El error HTTP curl 56 histórico sigue sin explicación.
David confirmó que la Guía del usuario de la PS5 muestra la web inocua del
ESP32. La foto de estado muestra `192.168.1.3/24`, puerta de enlace y DNS
`192.168.1.1`; David confirmó `tramoya_host` por Wi-Fi y ningún cable LAN.
Junto con el modo AP sin enlace ascendente y las pruebas previas, esto valida
el aislamiento de la topología ensayada. No se probó salida exterior directamente
desde la consola, transporte HTTPS ni USB MSC en PS5. La [integración mínima de
Relapse para 13.60](docs/integracion-relapse.md) superó su prueba física:
WebKit, kernel y arranque de elfldr confirmados en la PS5. La actualización USB
verificó los 51 recursos y conservó WIFI.TXT. La repetibilidad de la cadena
sigue pendiente; se conserva el ZIP inocuo. El perfil posterior con kstuff y
ShadowMount+ tiene su resultado físico registrado a continuación.
Véase [Estado y evidencias](docs/estado-proyecto.md).

El 15 de octubre se preparó el [perfil autónomo kstuff + ShadowMount+](docs/payloads.md),
sin etaHEN: ZIP de 56 archivos y 2.297.244 bytes, firmware sin cambios.
Sus pruebas de software pasan. David confirmó después la carga de los payloads
y que ShadowMount+ se ejecutó y funcionó en la PS5 13.60. Repetibilidad y
estabilidad de esta combinación siguen pendientes; recuperación conservada.

## Arquitectura del proyecto base

La base combina el flujo de servidor para ordenador de
[itsPLK/ps5-webkit-autoloader](https://github.com/itsPLK/ps5-webkit-autoloader)
con un volumen USB en el ESP32-S3:

1. **USB MSC:** TinyUSB expone una partición FAT con nivelación de desgaste.
   Contiene `frontend.zip` y el directorio `ps5_autoloader/`.
2. **Wi-Fi:** TRAMOYA crea el AP WPA2 definido en `WIFI.TXT`.
   DHCP anuncia la dirección del AP como DNS; el servidor DNS resuelve
   `manuals.playstation.net` hacia el dispositivo y rechaza dominios ajenos.
   El contrato de tipos, clases y flags está en [DNS](docs/dns.md).
3. **HTTP :80 y HTTPS :443:** la candidata nueva copia `frontend.zip` en PSRAM
   al arrancar y sirve esa copia con tinfl. Cierra FAT antes de que MSC permita
   acceso al ordenador. El ZIP no se incorpora a la imagen de la aplicación;
   debe caber en PSRAM dejando 512 KiB de margen en el bloque libre mayor.

El flujo previsto es alimentar el dispositivo por USB desde la consola,
conectarse a su AP y cargar el frontend mediante la Guía del usuario. David
confirmó que la PS5 mostró la página del ESP32; alimentar el dispositivo
desde la consola y comprobar allí MSC siguen pendientes.

## Hardware y particiones

Cada perfil de [targets/boards.json](targets/boards.json) tiene su tabla de
particiones, configuración y directorio de compilación:

| Perfil | Flash | PSRAM | Modo PSRAM | Partición storage (MiB aprox.) |
|---|---|---|---|---|
| `esp32s3-n4r2` | 4 MB | 2 MB | quad | 2,44 |
| `esp32s3-n8r2` | 8 MB | 2 MB | quad | 5,81 |
| `esp32s3-n8r8` | 8 MB | 8 MB | octal | 5,81 |
| `esp32s3-n16r2` | 16 MB | 2 MB | quad | 13,81 |
| `esp32s3-n16r8` | 16 MB | 8 MB | octal | 13,81 |
| `esp32s3-n32r8` | 32 MB | 8 MB | octal | 29,81 |

Los perfiles son configuraciones disponibles, no resultados de validación
física de todas las placas. Para consultar la lista:

```sh
python3 tools/build.py targets
```

**La tabla N16R8 del repositorio difiere de la del diagnóstico probado:**
storage empieza en `0x230000` en el proyecto base y en `0x310000` en el
diagnóstico. La aplicación ocupa una partición de 2 MiB en la base y de 3 MiB
en el diagnóstico. Utilizar siempre bootloader, tabla y aplicación del mismo
juego de imágenes. Conservar bytes de flash al cambiar la tabla no garantiza
conservar un volumen FAT utilizable.

## Volumen USB y carga de payloads

Distribución prevista en el volumen del firmware completo:

```text
frontend.zip            frontend web servido por el AP
WIFI.TXT                SSID y contraseña WPA2 leídos al arrancar
ps5_autoloader/         directorio de payloads leído desde la PS5
  README.txt            instrucciones en español e inglés
  autoload.txt          lista opcional de carga
  etaHEN.elf            ejemplos de archivos aportados por el usuario
  kstuff.elf
```

El frontend del proyecto base utiliza un autoloader unificado, empaquetado
como `payloads/payload.elf`. Su búsqueda de `autoload.txt` sigue este orden:

1. `ps5_autoloader_<app>/autoload.txt` en USB: `<app>` es `bdjb` para la
   aplicación de Blu-ray o el Title ID de la aplicación de entrada.
2. El mismo directorio específico de aplicación en `/data` de la PS5.
3. `ps5_autoloader/autoload.txt` en USB, la ubicación habitual del dispositivo.
4. `ps5_autoloader/autoload.txt` en `/data` de la PS5.

Si encuentra la lista, ejecuta las entradas en orden; sin ella, utiliza el
Payload Manager incluido. Estos comportamientos se conservan como referencia
del proyecto base y no se han probado en la PS5 de TRAMOYA.

### Formato de autoload.txt

```text
# Los comentarios empiezan por #; se omiten las líneas vacías.
etaHEN.elf              # ruta relativa al directorio de esta lista
!1500                  # espera 1500 ms antes de la siguiente entrada
kstuff.elf
```

Las rutas que comienzan por `/` son absolutas. La instrucción opcional `@sync`
copia el directorio activo a `/data`, verifica los archivos y los retira del
USB después de una carga sin errores. Es experimental: escribe en el volumen
USB desde la PS5 y queda fuera del flujo de actualización soportado por la
base. La actualización prevista se realiza desde un ordenador, con el
dispositivo desconectado de la consola.

Los ejemplos de payloads no implican compatibilidad con el firmware objetivo.
Los ELF del usuario no se incluyen en este repositorio. `--payload-dir` o
`PAYLOAD_DIR` permite incorporarlos al generar una imagen de almacenamiento.

### Actualizar el contenido

Flujo del proyecto base: desconectar de PS5 → conectar al ordenador → sustituir
`frontend.zip` o editar los archivos → expulsar el volumen → desconectar y
volver a conectar. La candidata nueva conserva una copia inmutable del ZIP en PSRAM hasta
el siguiente arranque; los cambios requieren expulsar y reiniciar. FAT se
desmonta de la aplicación antes de enumerar MSC. Si la copia no cabe en memoria
o el ZIP no puede indexarse, USB permanece como vía de corrección y la web no sirve
el frontend. La actualización con ZIP inocuo, recarga e integridad pasó en
la placa; el ensayo de 30 minutos pasó, mientras que usos más largos, ZIP grandes
y el flujo completo de USB y contenido en PS5 siguen pendientes.
El comprobador exige el testigo HTTPS `/__tramoya_storage` de esta candidata
antes de escribir o restaurar el ZIP; la versión DNS anterior no lo ofrece.

## Preparación del frontend

El repositorio no incluye `frontend.zip` ni las fuentes del frontend externo.
Las herramientas heredadas resuelven una versión de `ps5-webkit-autoloader`,
la adquieren con sus submódulos, ejecutan los scripts de preparación y parches,
y empaquetan `frontend/autoloader` con `pc-host/overrides`.

Cada ZIP genera `frontend.zip-source.json` con procedencia, revisión y SHA-256;
el manifiesto del firmware incorpora esa información. La selección por defecto
es la última release disponible. Para una prueba reproducible hay que fijar
la versión y comprobar su revisión y dependencias; un número de versión en un
ejemplo no confirma compatibilidad con PS5 13.60.

Ejemplo de sintaxis con una etiqueta de referencia heredada:

```sh
python3 tools/build_frontend_zip.py --target esp32s3-n16r8 --tag v0.5.2
```

Esta herramienta requiere Python 3, Git y Bash, sin ESP-IDF. Descarga y prepara
fuentes externas. La preparación completa de TRAMOYA sigue pendiente según
el [registro de estado](docs/estado-proyecto.md).

## Compilar y empaquetar

Se utiliza **ESP-IDF 5.4.2**. Si ya está instalado, activar ese entorno; no es
necesario volver a instalarlo. Ejemplo para la ubicación del SDK usada en macOS:

```sh
export PATH="$HOME/.espressif/python_env/idf5.4_py3.12_env/bin:$PATH"
. "$HOME/esp/esp-idf-v5.4.2/export.sh"
python3 tools/build.py build --target esp32s3-n16r8
```

Para generar un paquete con un ZIP previamente preparado y comprobado:

```sh
python3 tools/build.py package --target esp32s3-n16r8 --frontend frontend.zip
```

Otras entradas del wrapper:

```sh
make targets
make build TARGET=esp32s3-n16r8
make package TARGET=esp32s3-n16r8 FRONTEND_ZIP=frontend.zip
make zip TARGET=esp32s3-n16r8 UPSTREAM_TAG=v0.5.2
make storage TARGET=esp32s3-n16r8 FRONTEND_ZIP=frontend.zip
```

Sin `--frontend`, `package` exige fijar el frontend externo con `--upstream-tag`;
si faltan ambas opciones falla antes de descargar o compilar. La alternativa `--frontend-source diagnostic`
crea la página inocua de este repositorio, distinta del firmware de diagnóstico
ya validado. Las herramientas pueden adquirir dependencias durante la ejecución.

`package` produce `build/<perfil>/ps5_wkal_host-full.bin`, con bootloader,
tabla, aplicación y almacenamiento. La imagen completa se escribe en `0x0`,
reinicia NVS y sustituye el contenido USB. No se debe usar como actualización
que preserve archivos. No hay OTA: la aplicación tiene una sola partición.

### Grabación y recuperación

El puerto debe identificarse antes de cada grabación. La siguiente sintaxis
es una referencia del proyecto base, no un paso para regrabar ahora el diagnóstico:

```sh
# Sustituir el valor por el puerto confirmado de la placa de prueba.
TRAMOYA_PORT=/dev/cu.PUERTO_CONFIRMADO
make flash TARGET=esp32s3-n16r8 PORT="$TRAMOYA_PORT"
make monitor TARGET=esp32s3-n16r8 PORT="$TRAMOYA_PORT"
```

`flash` conserva los sectores de storage, pero eso solo conserva el volumen
si se mantiene una distribución compatible. `make flash-all` o
`python3 tools/build.py flash --erase-storage` escribe también storage y
sustituye sus archivos; requiere el perfil y el puerto correspondientes.

El acceso al cargador ROM se recupera manteniendo BOOT, pulsando y soltando
RST, y soltando BOOT. Para arrancar la aplicación, pulsar RST con BOOT suelto.
Cuando TinyUSB MSC usa el USB nativo del diagnóstico, la desaparición del
puerto USB Serial/JTAG es esperable. Los logs del diagnóstico están configurados
en UART0 a 115200 baudios; no se ha validado el posible puente UART de la placa.
El juego de recuperación `hello_world` se conserva por separado, fuera de este
repositorio. No mezclar sus imágenes con las del diagnóstico o las de la base.

## Consumo y temperatura

La base configura CPU a 80 MHz en reposo y 160 MHz cuando hay tareas ejecutables,
con APB a 80 MHz para USB OTG y PSRAM. No utiliza suspensión automática ligera
ni profunda. El diagnóstico validado utiliza CPU fija a 160 MHz: sus pruebas
no validan el ahorro de energía del firmware completo.

El proyecto base elimina el mensaje periódico de cinco segundos, lee los
registros de fallo al arrancar y reduce los logs DNS/HTTP al nivel de depuración.
Un AP Wi-Fi activo sigue consumiendo energía de radio. Reducir tareas no
demuestra por sí solo una reducción de corriente o temperatura.

`tools/build.py` sincroniza los ajustes de energía de configuraciones existentes
en `build/package/flash` y comprueba el resultado. Una compilación directa con
`idf.py` requiere una configuración equivalente. Sustituir `frontend.zip` no
cambia esos ajustes del firmware. La comparación de consumo exige la misma
alimentación, carcasa, temperatura ambiente y estado de conexión, con medidas
tras estabilizar la temperatura; esas medidas están pendientes en TRAMOYA.

## Pruebas y CI

Pruebas de host disponibles en la base, con `make`, compilador C y zlib:

```sh
export PATH="$HOME/.espressif/python_env/idf5.4_py3.12_env/bin:$PATH"
source "$HOME/esp/esp-idf-v5.4.2/export.sh"
python3 tools/build.py test --target esp32s3-n16r8 --require-fat
python3 tools/build.py test-asan --target esp32s3-n16r8 --require-fat
```

La suite de [tools/tests](tools/tests) cubre DNS y traducción de rutas frente
a una referencia, índices ZIP, descompresión y CRC, manejadores HTTP simulados,
configuración de energía y lectura de imágenes FAT. La prueba completa de FAT
requiere ESP-IDF. Estos comandos pueden adquirir dependencias; no son los
ensayos físicos del diagnóstico descritos en el registro de estado.

El workflow [build.yml](.github/workflows/build.yml) permite seleccionar perfiles
y una etiqueta del frontend. Publica artefactos por perfil y ejecuta pruebas
unitarias, sanitizadores e integración en ESP-IDF 5.4.2. El envío de una etiqueta
`v*` activa además la publicación de una release. La existencia del workflow
no demuestra que esta versión de TRAMOYA haya superado esas ejecuciones.

## Detalles de implementación heredados

- miniz utiliza nombres privados para evitar enlazar con la implementación
  incompatible de tinfl en la ROM del ESP32-S3. La compilación verifica el enlace.
- Los certificados se incorporan con `EMBED_TXTFILES`; se comprueban los bytes
  y la correspondencia de certificado y clave en el ELF. Se generan en la
  primera compilación si faltan. No equivale a aceptación del certificado en PS5.
- La base incluye respuestas locales para pruebas de conectividad de la consola
  y otros clientes. Su aceptación en PS5 13.60 sigue pendiente de prueba.
- Los componentes `esp_tinyusb` y `tinyusb` se adquieren del registro de Espressif.
- El presupuesto heredado de la placa de 4 MB es de unos 894 KiB de aplicación
  en 1408 KiB y 2,4375 MiB de storage. Son cifras de la base, no de la N16R8
  diagnosticada. El frontend cambia de tamaño según versión y contenido;
  verificar el presupuesto antes de empaquetar.

## Autoría y licencia

TRAMOYA parte del trabajo de [kxn](https://github.com/kxn/ps5-wkal-esp32s3)
y del frontend de [itsPLK](https://github.com/itsPLK/ps5-webkit-autoloader).
Se conservan el historial, los avisos y la [licencia GPL-3.0](LICENSE).
La documentación heredada declara los fuentes de `main/` y `tools/` bajo
GPL-3.0-or-later y miniz como dominio público. Los componentes externos
conservan sus licencias y deben revisarse al preparar una distribución.

## Trabajo local y controles

Consulta la [guía de desarrollo](DESARROLLO.md). El Makefile selecciona el
enlazador de macOS/Linux y conserva ASan/UBSan; `--require-fat` evita aceptar
una ejecución sin FAT. Las pruebas escriben solo artefactos ignorados y temporales.
Sin ESP-IDF puede ejecutarse la suite rápida omitiendo esa opción: no equivale
a validación completa. No se necesita PS5 ni flashear una placa.

Tras un clon, instala el hook local con `bash tools/install_git_hooks.sh`;
requiere Gitleaks de `.gitleaks-version`. Escanea staged y bloquea rutas privadas.
No equivale a revisión humana ni a permisos de acceso. No uses `--no-verify`.

`checks.yml` añade pruebas rápidas macOS/Linux e integración FAT para PR y
push a main. Consulta los resultados de cada ejecución en GitHub Actions.
La publicación completa exige `upstream_tag` en lanzamiento manual o la variable
de repositorio `TRAMOYA_UPSTREAM_TAG` para tags: elegir y verificar la versión
requiere una decisión de release, nunca se selecciona latest silenciosamente.
Para empaquetar localmente usa `--target esp32s3-n16r8` y un `--frontend`
verificado, o `--upstream-tag` explícito. Registra el SHA-256 del ZIP y el
manifiesto de firmware. Esto fija entradas; no garantiza binarios idénticos
si se regeneran certificados o cambian otras entradas del toolchain.

Los comandos de build.py (salvo targets/deps) exigen --target. El wrapper make
exige TARGET para las mismas operaciones. No se elige una placa por defecto.
