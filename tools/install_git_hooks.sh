#!/bin/bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(git -C "$DIR" rev-parse --show-toplevel)"
EXPECTED="$(cat "$ROOT/.gitleaks-version")"
if ! command -v gitleaks >/dev/null || [ "$(gitleaks version)" != "$EXPECTED" ]; then
  echo "Se requiere Gitleaks $EXPECTED; consulta README.md." >&2
  exit 1
fi
if [ ! -x "$ROOT/.githooks/pre-commit" ]; then
  echo "Falta el hook versionado ejecutable." >&2; exit 1
fi
git -C "$ROOT" config --local core.hooksPath .githooks
printf 'Hook local activado en %s
' "$ROOT"
