#!/usr/bin/env bash
# Run a command (or an interactive shell) inside the Fulcrum Linux dev container.
#   scripts/dev.sh                      -> shell
#   scripts/dev.sh ctest --preset asan  -> one command
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
image="fulcrum-dev"

# Rebuild the image only when the Dockerfile changed (docker caches layers).
docker build -q -t "$image" "$repo/.devcontainer" >/dev/null

flags=(--rm -v "$repo:/work" -w /work
       -v fulcrum-cache:/root/.cache
       --cap-add=NET_ADMIN --cap-add=SYS_PTRACE
       --ulimit nofile=65536:65536)
[ -t 0 ] && [ -t 1 ] && flags+=(-it)

if [ $# -eq 0 ]; then
  exec docker run "${flags[@]}" "$image" bash
else
  exec docker run "${flags[@]}" "$image" bash -c "$*"
fi
