TRAMOYA Host — Español / English

ESPAÑOL

Coloca aquí los payloads ELF que correspondan a tu configuración.
Los nombres etaHEN.elf y kstuff.elf son ejemplos, no una declaración
de compatibilidad con PS5 13.60.

Un autoload.txt en este directorio enumera las entradas en orden:

    etaHEN.elf        # nombres relativos a este directorio
    !1500            # espera 1500 ms antes de la siguiente línea
    kstuff.elf

Se omiten las líneas vacías y los comentarios iniciados por #.
Sin autoload.txt, el autoloader unificado inicia su Payload Manager.
La instrucción opcional @sync copia el directorio a /data de la PS5,
lo verifica y lo retira del USB después de una carga sin errores.
Es experimental: escribe en el volumen desde la consola.

Actualización prevista: desconectar de PS5, conectar al ordenador,
editar los archivos, expulsar y volver a conectar. Los cambios requieren
un ciclo de alimentación para recargar el índice del firmware.
El firmware completo de TRAMOYA todavía no se ha probado en PS5.
Consulta README.md y docs/estado-proyecto.md en el repositorio.

ENGLISH

Place payload ELFs appropriate for your setup here.
etaHEN.elf and kstuff.elf are examples, not a compatibility claim
for PS5 firmware 13.60.

An autoload.txt in this directory lists entries in order:

    etaHEN.elf        # names are relative to this directory
    !1500            # wait 1500 ms before the next line
    kstuff.elf

Blank lines and comments starting with # are ignored.
Without autoload.txt, the unified autoloader starts its Payload Manager.
The optional @sync directive copies the directory to the PS5's /data,
verifies it and removes it from USB after an error-free load.
It is experimental: it writes to the volume from the console.

Intended update flow: unplug from PS5, connect to a computer, edit files,
eject and reconnect. A power cycle reloads the firmware's cached index.
TRAMOYA's full firmware has not yet been tested on PS5.
See README.en.md and docs/project-status.en.md in the repository.
