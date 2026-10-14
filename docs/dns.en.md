[Español](dns.md) · **English** · [Status](project-status.en.md)

# Local DNS contract — 14 October 2026

The physical build based on `7c197bc` returned an A record for a
`manuals.playstation.net AAAA` query. The fix checks both type and class for
the main domain and the five existing connectivity probe names. No new
allowlisted domains, recursion or external resolution are introduced.

For a parseable single question, apply the first matching rule:

| Query | Response |
|---|---|
| OPCODE other than QUERY (0) | NOTIMP (4), no answers |
| Class other than IN (1), including ANY class | REFUSED (5), no answers |
| QTYPE 0, OPT (41), IXFR (251), AXFR (252) | NOTIMP (4), no answers |
| Name outside the existing local list | NXDOMAIN (3), no answers |
| Local name, IN, A or ANY type | NOERROR, one A at the AP IPv4, TTL 300 |
| Local name, IN, any other ordinary type, including AAAA | NOERROR/NODATA, no answers |

Exact name matching is ASCII case insensitive. No subdomain/suffix matching.
No SOA, NS, CNAME or IPv6 records are invented. Negative replies carry no SOA
for negative caching; external NXDOMAIN is an intentional offline policy.

The response echoes ID, question, OPCODE and RD. QR is set; RA, AA, TC, AD,
CD and the reserved bit are clear. No public authority or DNSSEC validation
is claimed. QR/TC input, QDCOUNT other than one, compressed questions,
truncated packets and unsupported names are silently dropped. Limits are
512 bytes per packet, 63 per label and 255 per wire-format name. Embedded NUL
or dots within labels are rejected to prevent aliases when converting to
strings. Additional records, including OPT, are ignored and not echoed;
EDNS, DNSSEC and TCP are not implemented. Only successfully built A and
NXDOMAIN replies increment the corresponding counters.

All 16 historical vectors remain, with corrected expectations: the inherited
host.py behavior was not normative. AAAA/TXT must not return A; RA must not
claim recursion; OPT as QTYPE receives NOTIMP. The QR and zero-question
vectors now actually set their named fields. Independent matrix/boundary
tests cover classes, types, flags, opcodes, counters, lengths and EDNS input.
The new tests failed against the old code; the negative evidence is retained.

Use the full recipes with activated ESP-IDF **5.4.2**:

```sh
python3 tools/build.py test --target esp32s3-n16r8 --require-fat
python3 tools/build.py test-asan --target esp32s3-n16r8 --require-fat
python3 tools/build.py build --target esp32s3-n16r8
```

See [status](project-status.en.md) for executed checks and hardware scope.
Protocol references: [RFC 1035 §4.1](https://www.rfc-editor.org/rfc/rfc1035.html#section-4.1),
[RFC 3596 §2](https://www.rfc-editor.org/rfc/rfc3596.html#section-2),
[RFC 2308 §2.2](https://www.rfc-editor.org/rfc/rfc2308.html#section-2.2).

## UDP transport correction

Review found that recvfrom on lwIP returns the copied length, hiding oversized
datagrams behind a 512-byte buffer. Reception now reserves 513 bytes; the
parser rejects lengths above 512 even when a larger packet was truncated.
A compile-time assertion protects this requirement. A regression simulates
512/513/1024-byte reception and verifies response lengths and counters.
