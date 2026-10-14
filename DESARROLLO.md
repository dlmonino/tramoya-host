# Desarrollo de TRAMOYA Host

El proyecto utiliza ESP-IDF 5.4.2. La placa validada es ESP32-S3 N16R8;
selecciona siempre el perfil explícitamente. Las dependencias y las fuentes
externas deben conservar sus versiones fijadas, atribuciones y licencias.

## Entorno y comprobaciones

Con el SDK instalado en la ubicación habitual de macOS:

```sh
export PATH="$HOME/.espressif/python_env/idf5.4_py3.12_env/bin:$PATH"
source "$HOME/esp/esp-idf-v5.4.2/export.sh"
python3 tools/build.py test --target esp32s3-n16r8 --require-fat
python3 tools/build.py test-asan --target esp32s3-n16r8 --require-fat
python3 tools/build.py build --target esp32s3-n16r8
```

`--require-fat` impide aceptar una ejecución sin las comprobaciones FAT.
ASan/UBSan comprueba errores de memoria y comportamiento indefinido. Las
pruebas generan artefactos ignorados por Git y archivos temporales. Estos
comandos no graban la placa ni acceden a una PS5.

La preparación y las comprobaciones adicionales del frontend se describen en
[Relapse](docs/integracion-relapse.md) y [payloads](docs/payloads.md). Distingue
los resultados de software de los ensayos físicos de cada versión.

## Cambios y publicación

Conserva los cambios locales y revisa el diff antes de preparar una entrega.
No incluyas claves privadas, sesiones, configuración privada, binarios de
payloads, artefactos de compilación ni registros internos.

Tras clonar, instala el control local con `bash tools/install_git_hooks.sh`;
utiliza la versión de Gitleaks fijada en `.gitleaks-version`. Comprueba las
rutas de los enlaces y `git diff --check` antes de cerrar cambios documentales.

Para construir paquetes, selecciona un frontend verificado o una versión
externa explícita. Registra los hashes del ZIP y del firmware. La compilación
no acredita funcionamiento físico en la PS5. Conserva un juego coherente de
bootloader, particiones, aplicación y recuperación para cada versión.
