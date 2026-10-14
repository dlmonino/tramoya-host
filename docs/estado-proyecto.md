**Español** · [English](project-status.en.md) · [TRAMOYA Host](../README.md)

# Estado y validación de TRAMOYA

## Actualización vigente — 14 de octubre de 2026

### Validado físicamente: TRAMOYA anterior a la corrección DNS

Compilación N16R8 basada en `7c197bc`, aplicación de **925.568 bytes**, SHA-256
`0a7e3144bece1178d6ae8f516d6f6734335fd114750c8e4004b4db7604d0d458`.
Se grabó con frontend diagnóstico inocuo, sin payloads homebrew.
Se mantienen WPA2 y valores iniciales `tramoya_host` / `tramoya_host`.

- WIFI.TXT: cambio de credenciales, archivo ausente, contraseña inválida y
  recuperación USB superados. Valores iniciales restaurados y diez peticiones
  HTTP/HTTPS finales correctas.
- DHCP: 192.168.1.0/24, AP/DNS/router 192.168.1.1. Resolución A local y rechazo
  de dominios externos probados; AAAA devolvía indebidamente A en esa versión.
- HTTP/HTTPS y certificado local explícito superaron tandas breves. Aislamiento
  IPv4/IPv6 comprobado para la interfaz y los destinos públicos ensayados.
- HTTP curl 56 sigue sin causa identificada. Curl 63 fue una prueba en el Wi-Fi
  doméstico y se excluye de fallos del firmware. Una tanda correcta no cierra 56.

### Candidata de software: corrección DNS

[Contrato DNS y regresiones](dns.md): AAAA devuelve NOERROR/NODATA, A/ANY siguen
siendo locales, clases no soportadas se rechazan, RD se refleja y RA queda a cero.
Entradas malformadas y operaciones no soportadas tienen comportamiento explícito.
Se demostró que las nuevas pruebas fallaban con el código anterior, conservando
la cobertura existente. Los resultados de pruebas normales, ASan/UBSan, FAT
obligatoria y compilación N16R8 figuran debajo. Se revisó
independientemente el diff final. **Ensayo físico DNS superado el 14 de octubre,
20:38 (Europe/Madrid): 75 comprobaciones, cero fallos.** Incluye 69 de DNS
(AAAA sin direcciones, A local, rechazo externo, tipos, clases, RD/RA y tamaños),
cuatro controles HTTP/HTTPS con integridad y dos de aislamiento IPv4/IPv6.
El registro local `dns-fisico-20261014-203833-590203.json` conserva los paquetes
y métricas. Identidad por certificado/contenido; el hash esperado del binario
es una referencia local, no una lectura de la flash. No acredita estabilidad
prolongada ni resuelve HTTP 56. Los resultados de WIFI.TXT se conservan,
sin atribuirlos automáticamente al binario nuevo.

Verificación de software ejecutada con ESP-IDF 5.4.2: normal y ASan/UBSan
superan, cada una, **49 pruebas C, 9.203 aserciones, cinco pruebas de energía y
seis comprobaciones FAT**, con cero fallos y sin omisiones. Compilación N16R8
correcta, incluido enlace miniz, certificado/clave embebidos y particiones.
Aplicación: **925.696 bytes**, SHA-256 `befcb546d27f031aef79d126af0b4a80db1e8c32f0239143775caa941fd68602`.
La revisión detectó truncamiento UDP: recibir 513 bytes permite rechazar
datagramas demasiado grandes. Se añadió una regresión del transporte y se
repitieron ambas suites completas y la compilación, sin fallos. El dictamen
final de revisión se conserva con las evidencias locales.

### Captura de HTTP 56 y candidata de frontend USB

En la candidata DNS probada físicamente se ejecutaron 61 peticiones controladas
(30 HTTP y 31 HTTPS), con 61 respuestas 200 y contenido íntegro. La consola CDC
registró la asociación Wi-Fi y las conexiones HTTPS sin errores observados
durante la captura. **HTTP curl 56 no se reprodujo; su causa sigue abierta.**
La captura empezó después del arranque y no demuestra ausencia de reinicios.
Evidencia: `outputs/http56-20261014/captura-20261014-205317-233225` en el
archivo local de la sesión, con trazas, cuerpos, cabeceras y consola.

La candidata N16R8 con **actualización USB probada físicamente** copia el ZIP en
PSRAM antes de cerrar FAT y entregar el medio a MSC. HTTP/HTTPS leen la copia
inmutable. La aplicación compilada mide **927.920 bytes**, SHA-256
`334f23534285f706fdf69e12b9210b5f0e6566770711f851d64cb887fac42e0a`.
Con ESP-IDF 5.4.2, cada receta normal y ASan/UBSan pasó 54 pruebas C, 9.252
aserciones, cinco de energía y seis FAT; compilación N16R8 correcta. Once
pruebas simuladas del cliente de actualización USB y cuatro del grabador pasaron. Una revisión
independiente del firmware y las pruebas no encontró bloqueantes confirmados.
El ZIP debe caber en el bloque libre mayor de PSRAM dejando 512 KiB de margen;
los archivos grandes y la estabilidad superior a 30 minutos requieren validación. Un
ZIP que no pueda indexarse deja la web sin frontend y mantiene USB para
corregirlo; CRC y DEFLATE se comprueban al servir cada entrada. DNS y web solo
arrancan tras iniciar MSC; `GET /__tramoya_storage` devuelve un testigo
independiente del ZIP que el cliente exige antes de escribirlo o restaurarlo.
La versión DNS anterior carece del testigo y no debe recibir escrituras USB.

David confirmó la prueba USB superada el 14 de octubre: el ensayo de las 21:24
respaldó WIFI.TXT/ZIP y verificó que HTTPS seguía sirviendo la copia antigua
durante la escritura. Después se observó la página nueva. La verificación de
las 21:31 confirmó HTTPS, ZIP nuevo íntegro (1.140 bytes) y WIFI.TXT original
íntegro (269 bytes), sin nuevas escrituras. Registro local:
`outputs/frontend-20261014/verificacion-20261014-213134-152195/resultado.json`.
Una repetición de `escribir` a las 21:25 se detuvo porque ya se servía la página
nueva; fue una guarda previa a escritura. No se registra como fallo del firmware.
Los logs no identifican por sí solos el tipo de reinicio realizado.

El ensayo físico de la misma candidata **superó 30 minutos** el 14 de octubre:
`outputs/estabilidad-tramoya-20261014/ensayo-20261014-215646-779755/resumen.json`.
Registró 181 muestras durante 1.802,04 segundos; 561 peticiones web, 1.224
comprobaciones DNS, 183 lecturas dobles de MSC y cuatro controles de aislamiento,
todos correctos. Las regresiones iniciales y finales pasaron. El CDC registró
25.087 bytes y 367 líneas con reloj, sin reinicios observados. El preparador
pasó 20 pruebas simuladas y revisión independiente. No lee la flash ni la memoria
libre, no descarta fugas más lentas y lee dos archivos pequeños: no valida un
ZIP grande ni todas las celdas de flash. El curl 56 histórico sigue sin causa.
David observó directamente que la Guía del usuario de su PS5 13.60 mostró la
web del ESP32 con la candidata inocua. Se registra como **prueba física del
acceso a la página**. Una foto del estado de conexión muestra IPv4
`192.168.1.3/24`, puerta de enlace y DNS `192.168.1.1`. David confirmó la red
`tramoya_host`, solo Wi-Fi y sin cable LAN. El firmware funciona como punto de
acceso sin enlace ascendente y los controles previos desde el Mac no alcanzaron
Internet. La topología ensayada queda aislada. No hubo prueba directa de salida
desde la PS5, captura de tráfico ni confirmación de HTTP frente a HTTPS o de la
aceptación del certificado. Tampoco se probó MSC en la PS5 ni homebrew. El
procedimiento y la evidencia se conservan en `outputs/ps5-inocua-20261014/`
del archivo local.

La placa elegida ejecuta TRAMOYA. La otra conserva el diagnóstico independiente.
Se mantienen imágenes anteriores congeladas y un juego separado de recuperación
hello_world. El registro histórico siguiente corresponde solo al diagnóstico.

### Relapse 13.60: prueba mínima superada

La [integración mínima](integracion-relapse.md) fija la copia conservada de
Relapse `d8e6896b5cb33b04f1e038a5d698d3d4aee5947c`, su kernel y su ELF loader.
La prueba física de David y sus fotos IMG_4373/4374 confirman WebKit preparado,
lectura/escritura del kernel, privilegios listos, retorno de elfldr `0x0` y
el cierre «prueba minima completa; payloads opcionales desactivados» en PS5
13.60. La cadena mínima y el arranque del cargador ELF quedan validados en esa
ejecución. En ese ensayo mínimo no se cargó otro ELF; el perfil posterior con
kstuff y ShadowMount+ tiene su resultado al final de este documento. La tasa de
éxito de la cadena sigue pendiente.
El ZIP tiene inicio manual y excluye los tres payloads
opcionales cuya versión exacta no se ha vinculado a una release verificable.
El registro USB de las 23:43 del 14 de octubre está VERIFICADO: 51 recursos
HTTPS coinciden en hash y tamaño con el ZIP, con HTTP200/curl0; ZIP y WIFI.TXT
se comprobaron desde el cliente. Se mantiene el firmware N16R8, sin nueva
grabación, y los respaldos de ZIP inocuo y WIFI.TXT. Esta cadena no necesita MSC
desde la PS5 ni instala un acceso directo del autoloader en la consola.
Evidencias, pruebas y revisión técnica: `outputs/integracion-relapse-20261014/`
en el archivo local; resultado físico en `prueba-fisica-ps5/resultado.json`.
Se conservan congelados los manifiestos de preparación. Los 30 minutos previos
con frontend inocuo no validan la estabilidad de la cadena Relapse.

## Registro del diagnóstico independiente

Actualizado: **9 de octubre de 2026**, cierre de pruebas a las 23:29
(Europe/Madrid). Este registro resume los resultados aportados por el
responsable del proyecto y revisados junto con los registros locales.

## Qué está validado

Se ha grabado y probado físicamente `esp32s3-servidor-diagnostico`, un proyecto
independiente de este repositorio, en una ESP32-S3-WROOM-1 N16R8 de revisión
v0.2. El firmware contiene páginas inocuas y no integra contenido PS5.
En aquel momento la segunda placa conservaba `hello_world`; la distribución
actual de las placas se describe en la actualización del 14 de octubre.

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

## Alcance actual

La carga de los payloads y el funcionamiento observado de ShadowMount+ están
confirmados por David en PS5 13.60. El 15 de octubre decidió no realizar la
prueba adicional propuesta de repetibilidad y estabilidad en esta etapa.
No se programa como requisito para continuar; tampoco se registra como superada.

La restauración de sesión de la Guía y MSC desde PS5 no se han ensayado. La
página inocua y la topología Wi-Fi aislada ya se comprobaron; sigue sin haber
una prueba directa de salida a Internet desde la consola.

El diagnóstico anterior desmonta FAT y sirve páginas integradas. La candidata
nueva copia frontend.zip en PSRAM, cierra FAT y después expone MSC. HTTP no
lee el medio mientras el ordenador puede escribir. La actualización inocua pasó;
el ensayo de 30 minutos pasó; periodos más largos y presión con ZIP grandes
siguen pendientes. La cadena mínima Relapse llegó a elfldr y el perfil posterior cargó los
payloads y ejecutó ShadowMount+ correctamente según David. MSC desde la consola
sigue sin probarse; operaciones de contenido específicas no se detallaron.
La confianza explícita del certificado en el Mac no acredita confianza en PS5.

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


## Preparación de payloads — 15 de octubre de 2026

David eligió mantener TRAMOYA autónomo, sin etaHEN. [Perfil preparado](payloads.md):
kstuff Lite v1.11 y ShadowMount+ 1.7-beta5fix1, con envíos manuales separados y
confirmación de kstuff antes de ShadowMount. Firmware idéntico; recuperación al
ZIP mínimo ya probado. Software normal/ASan/FAT, regresión mínima y compilación
N16R8 pasan. David confirmó después: «Han cargado los playloads, he ejecutado
shadowmount y ha funcionado». Se registra como PASS físico de carga y
funcionamiento observado de ShadowMount+ en PS5 13.60, sin etaHEN. No se infiere
un ensayo de montaje concreto ni una tasa de éxito. El registro USB local de
las 00:15 permanece ESCRITO: tres respuestas HTTPS y respaldos ZIP/WIFI
contrastados; no hay salida de verificación de los 56 recursos. Los manifiestos
de preparación quedan congelados. Resultado posterior:
`outputs/payloads-20261015/prueba-fisica-ps5/resultado.json` en el archivo local.
Pendientes: repetibilidad, estabilidad de esta combinación, restauración de sesión
y operaciones concretas de contenido. El ensayo de 30 minutos previo fue inocuo;
HTTP56 sigue abierto. El firmware y los juegos de recuperación se conservan.
