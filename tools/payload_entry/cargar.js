export async function cargarPayloadTramoya(nombre, p, chain) {
  if (!['kstuff.elf', 'shadowmountplus.elf'].includes(nombre))
    throw new Error('Payload fuera del perfil fijado');
  const payload = await mapElf(nombre, p, chain);
  try {
    await sendElf(nombre, payload, p, chain);
  } finally {
    // El socket ya cerró; liberar la copia de envío, no la memoria del cargador.
    await chain.syscall(SYS_MUNMAP, payload.base, (payload.size + 0x3fff) & ~0x3fff);
  }
}
