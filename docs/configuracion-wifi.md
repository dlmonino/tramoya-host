**Español** · [English](wifi-configuration.en.md) · [TRAMOYA Host](../README.md)

# Configurar la red Wi-Fi desde USB

El firmware lee **`WIFI.TXT` en la raíz del volumen USB** una vez al arrancar.
Su contenido inicial es:

```ini
ssid=tramoya_host
password=tramoya_host
```

La red utiliza **WPA2-PSK con AES/CCMP**. Puedes cambiar ambos valores sin
recompilar ni volver a grabar el firmware. Son las credenciales del punto de
acceso creado por TRAMOYA, no las de un router al que deba conectarse.

## Cambiar el nombre o la contraseña

1. Conecta el dispositivo al ordenador y abre `WIFI.TXT` en el volumen USB.
2. Edita `ssid=` y `password=` con un editor de texto plano y guarda en UTF-8.
   En TextEdit utiliza texto sin formato; no guardes como RTF ni como `WIFI.TXT.txt`.
3. Expulsa el volumen de forma segura. Si falla la expulsión, corrige el error
   antes de desconectar.
4. Desconecta y vuelve a conectar la alimentación USB. El siguiente arranque
   aplicará el fichero. Si cambiaste las credenciales, vuelve a conectar el cliente.

La plantilla de las nuevas imágenes está en
[tools/usb_root/WIFI.TXT](../tools/usb_root/WIFI.TXT). Un volumen existente
necesita que crees el fichero manualmente si aún no lo contiene. La aplicación
no lo crea ni lo sobrescribe al arrancar. Regenerar y grabar storage sustituye
sus archivos; editar el fichero no requiere esa operación.

## Formato admitido

| Campo o regla | Requisito |
|---|---|
| Nombre del fichero | `WIFI.TXT`, en la raíz del volumen |
| `ssid` | Entre 1 y 32 bytes UTF-8, sin caracteres de control |
| `password` | Entre 8 y 63 caracteres ASCII imprimibles, incluidos espacios |
| Autenticación | WPA2-PSK; no se admite red abierta ni clave hexadecimal de 64 caracteres |
| Tamaño del fichero | Máximo 1024 bytes, incluidos comentarios y BOM |
| Líneas | LF o CRLF; se admite BOM UTF-8 inicial y última línea sin salto final |
| Comentarios | Líneas que empiezan por `#`; se admiten líneas vacías |
| Claves | Exactamente `ssid` y `password`, una vez cada una, en cualquier orden |

Escribe las claves en minúsculas y sin espacios antes del signo `=`.
Los espacios, `#`, `=` y comillas dentro del valor se toman literalmente:
no añadas comillas para delimitarlo ni comentarios al final de una asignación.
El límite del SSID se mide en bytes; una letra acentuada puede ocupar más de uno.

La contraseña está en texto plano en el volumen USB, visible para quien tenga
acceso a él. La aplicación no la imprime en sus logs ni la guarda como
configuración Wi-Fi en NVS.

## Si falta el fichero o contiene errores

**El Wi-Fi permanece apagado.** No se crea una red abierta, no se reutilizan
credenciales anteriores ni se aplican automáticamente los valores de fábrica.
El volumen USB sigue disponible para crear o corregir `WIFI.TXT`; después debes
expulsarlo y reiniciar. No arrancan DHCP, DNS ni los servidores HTTP/HTTPS.
Los logs indican el tipo de error sin mostrar la contraseña.

Los valores `tramoya_host` / `tramoya_host` se incluyen en la plantilla de
storage. No constituyen una red de recuperación cuando el fichero es inválido.
Si el propio almacenamiento no puede montarse, es un fallo diferente de un
error de configuración; el firmware no formatea el volumen automáticamente.

## Orden de arranque y alcance

1. Montar FAT para la aplicación, sin presentar todavía el volumen al ordenador.
2. Leer, cerrar y validar `WIFI.TXT`.
3. Con datos válidos, copiar el ZIP íntegro a PSRAM y arrancar WPA2; cerrar FAT
   y exponer MSC con propiedad exclusiva de USB. Solo si MSC arranca correctamente,
   iniciar DNS y HTTP/HTTPS, que sirven la copia de PSRAM y el testigo MSC.
4. Con datos inválidos, cerrar FAT de la aplicación y habilitar únicamente
   el acceso USB para corregir el fichero.

La lectura de credenciales termina antes de que el ordenador pueda escribir.
No se relee el fichero durante el funcionamiento. La candidata nueva sirve el
ZIP guardado en PSRAM; expulsa el volumen y reinicia para usar un ZIP editado.
Si no cabe en PSRAM o no puede indexarse, USB sigue disponible para corregirlo.

Validación física del 14 de octubre de 2026 (versión basada en `7c197bc`):
cambio de credenciales, comportamiento sin archivo, rechazo de contraseña
inválida y recuperación USB superados. Se restauraron los valores originales
y se comprobó HTTP/HTTPS. La candidata con DNS corregido superó después su
ensayo físico DNS y controles HTTP/HTTPS/aislamiento; véase
[Estado](estado-proyecto.md). La actualización del frontend con FAT exclusiva
superó su prueba física el 14 de octubre con ZIP inocuo y WIFI.TXT íntegro.
La misma candidata superó físicamente 30 minutos de DNS, HTTP/HTTPS, lecturas
MSC y aislamiento. David observó directamente la web del ESP32 en la Guía del
usuario de la PS5 y la conexión Wi-Fi a tramoya_host sin LAN. La foto muestra
IPv4 192.168.1.3, puerta de enlace y DNS 192.168.1.1. El transporte de la
Guía no se ha registrado.
Los periodos más largos siguen pendientes.
