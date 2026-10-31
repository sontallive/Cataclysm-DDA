#!/usr/bin/env bash
set -euo pipefail
[[ "$BUILD_RUN" =~ ^[0-9]+$ ]]
gh api "repos/$GH_REPO/actions/runs/$BUILD_RUN" > run.json
python3 - <<'CHECK'
import json
r = json.load(open('run.json'))
assert r['conclusion'] == 'success' and r['status'] == 'completed'
assert r['head_branch'] == 'stable'
assert r['path'] == '.github/workflows/linux-release.yml'
assert r['event'] == 'workflow_dispatch'
CHECK
gh run download "$BUILD_RUN" --name cdda-stable-linux-x86_64 --dir dist
(cd dist && sha256sum --check SHA256SUMS)
python3 - <<'CHECK'
import json, re
from pathlib import Path
r = json.load(open('run.json'))
v = dict(line.split(': ', 1) for line in Path('dist/VERSION.txt').read_text().splitlines())
assert v['fork commit'] == r['head_sha']
assert re.fullmatch(r'0\.[A-Z](?:-[0-9]+)?', v['upstream stable'])
assert v['build type'] == 'stable Linux x86_64 with graphics and sounds'
tag = v['build number']
assert tag == f"catazoo-{v['upstream stable']}-linux-{r['run_number']}-{r['run_attempt']}"
assert (Path('dist') / f"cdda-{v['upstream stable']}-catazoo-linux-x86_64.tar.gz").is_file()
Path('release-tag').write_text(tag)
Path('release-commit').write_text(r['head_sha'])
CHECK
cat dist/VERSION.txt > notes.txt
cat >> notes.txt <<'NOTES'

Opt-in contextual touch shortcuts: launch with `--touch-shortcuts`; disabled by default.
This is a custom preview based on the official stable release, not an upstream official build.
Steam Deck hardware acceptance is pending. macOS preview has an unresolved redraw issue
also observed with the shortcut flag disabled; Linux visual acceptance is still pending.
NOTES
gh release create "$(cat release-tag)" dist/* --target "$(cat release-commit)" \
  --title "CDDA stable + CataZoo touch shortcuts · $(cat release-tag)" \
  --prerelease --notes-file notes.txt
