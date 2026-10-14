**Español** · [English](dns.en.md) · [Estado](estado-proyecto.md)

# Contrato DNS local — 14/10/2026

Corrección del defecto observado en la versión física basada en `7c197bc`:
`manuals.playstation.net AAAA` devolvía un registro A. El constructor ahora
comprueba tipo y clase para el dominio principal y los cinco dominios de
sondeo ya existentes. No se añaden dominios, recursión ni salida a Internet.

## Respuestas y precedencia

Para una pregunta analizable, se aplica la primera fila que corresponda:

| Consulta | Respuesta |
|---|---|
| OPCODE distinto de QUERY (0) | NOTIMP (4), sin respuestas |
| Clase distinta de IN (1), incluida ANY (255) | REFUSED (5), sin respuestas |
| QTYPE 0, OPT (41), IXFR (251) o AXFR (252) | NOTIMP (4), sin respuestas |
| Nombre ajeno a la lista local | NXDOMAIN (3), sin respuestas |
| Nombre local, IN, A o ANY | NOERROR, un A con la IPv4 del AP; TTL 300 |
| Nombre local, IN, AAAA o cualquier otro tipo ordinario | NOERROR/NODATA, cero respuestas |

`manuals.playstation.net` y los dominios exactos de `wk_connectivity_host`
son locales. La comparación ignora mayúsculas ASCII y no incluye subdominios
ni sufijos parecidos. Se mantienen A y ANY por compatibilidad con los clientes
actuales; TXT, MX, SVCB, HTTPS y tipos ordinarios desconocidos no reciben A.
NODATA significa que existe el nombre local, pero no datos del tipo pedido.
No se fabrican SOA, NS, CNAME ni registros IPv6; los negativos no aportan SOA
para caché negativa. La política NXDOMAIN externa es deliberadamente local.

## Flags, formato y límites

- Se conserva ID, pregunta original, OPCODE y el valor solicitado de RD.
- QR=1; RA=0 siempre. No hay servidor recursivo ni consultas a DNS externos.
- AA, TC, AD, CD y el bit reservado se emiten a cero. No se anuncia autoridad
  pública sobre los dominios ni validación DNSSEC.
- Se acepta una sola pregunta no comprimida, paquete de hasta 512 bytes,
  etiquetas de hasta 63 bytes y nombre en formato de red de hasta 255 bytes.
- QR=1 de entrada, TC=1, QDCOUNT distinto de uno, truncamiento, compresión,
  longitud inválida o etiquetas con NUL/punto interno se descartan sin respuesta.
  Estas últimas se rechazan para que la conversión a cadena no cree alias de
  un nombre permitido. No es un analizador de todos los nombres DNS binarios.
- Los adicionales de una consulta (p. ej. OPT) no se interpretan ni se reflejan;
  ARCOUNT de respuesta es cero. Esto conserva la aceptación de preguntas de
  clientes que adjuntan EDNS, pero no implementa EDNS, DNSSEC ni TCP.
- Solo respuestas A y NXDOMAIN construidas correctamente incrementan sus
  contadores. NODATA, REFUSED, NOTIMP y descartes no los incrementan.

## Pruebas y referencias corregidas

Se conservan los 16 vectores heredados y todas las pruebas ajenas a DNS.
Los vectores dejan de exigir equivalencia con `pc-host/host.py`: su respuesta
A ante AAAA/TXT y RA permanente reproducían el defecto, no un contrato válido.
Se corrigen los casos QR y QDCOUNT=0, cuyos bytes antes eran consultas normales
pese al nombre del caso. El vector OPT como QTYPE espera NOTIMP; el caso EDNS
real lleva OPT en adicionales y comprueba su omisión en la respuesta.

La matriz independiente comprueba nombres locales/ajenos, A/ANY/AAAA/TXT/MX/
SVCB/HTTPS/tipo desconocido, clases IN/CH/HS/ANY, RD=0/1, opcodes, flags,
contadores, pregunta, bytes del A, capacidad y límites del analizador.
Las nuevas expectativas se ejecutaron contra el código anterior: fallaron;
la entrada nula adicional también provocó un fallo del proceso de pruebas.
El registro previo se conserva como evidencia negativa, no como prueba pasada.

Recetas completas, desde ESP-IDF **5.4.2** activado:

```sh
python3 tools/build.py test --target esp32s3-n16r8 --require-fat
python3 tools/build.py test-asan --target esp32s3-n16r8 --require-fat
python3 tools/build.py build --target esp32s3-n16r8
```

El estado de ejecución y el alcance físico constan en [Estado](estado-proyecto.md).
Una suite de Mac no valida por sí sola el DNS servido por la placa.

Referencias del protocolo: [RFC 1035 §4.1](https://www.rfc-editor.org/rfc/rfc1035.html#section-4.1),
[AAAA en RFC 3596 §2](https://www.rfc-editor.org/rfc/rfc3596.html#section-2) y
[NODATA en RFC 2308 §2.2](https://www.rfc-editor.org/rfc/rfc2308.html#section-2.2).
Estas referencias justifican las distinciones; el subconjunto y la política
local descritos aquí son decisiones explícitas del proyecto.

## Corrección del transporte UDP

La revisión detectó que recvfrom de lwIP devuelve la longitud copiada, ocultando
datagramas grandes tras un búfer de 512 bytes. Ahora se reciben hasta 513:
el analizador rechaza longitudes mayores de 512 incluso si un datagrama mayor
se truncó. Una aserción de compilación protege este requisito. La regresión
simula recepción de 512/513/1024 bytes y verifica respuesta y contadores.
