"""Resolve and stage the upstream ps5-webkit-autoloader source tree.

The frontend is never committed to this repository. Packaging fetches the
upstream project at its latest GitHub release tag (or a pinned tag, or an
existing local clone) and prepares the serving tree exactly like upstream's
own Makefile does before staging the frontend:

    git clone --depth 1 -b <tag> --recurse-submodules --shallow-submodules
    tools/download_deps.sh         pinned elfldr/kexp/payload release assets
    tools/apply_relapse_patch.sh   third_party/relapse -> frontend copy
    tools/apply_slopkit_patch.sh   third_party/slopkit -> frontend copy
    tools/apply_umtx2_patch.sh     third_party/umtx2 -> frontend copy

The staged tree is cached under .deps/upstream/<tag>/ and reused when its
recorded commit still matches the tag, so repeated builds stay offline and
a re-tagged upstream name is not silently served from stale content.

Requires: git and bash on PATH (the upstream preparation scripts are bash).
Network: api.github.com for release resolution (GITHUB_TOKEN honoured),
github.com for the clone and the pinned payload release assets.
"""
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEPS = ROOT / ".deps/upstream"
REFERENCE = json.loads((ROOT / "tools/dependencies.json").read_text())["frontend_reference"]
# dependencies.json stores a full clone URL; accept both URL and owner/name.
REPO = REFERENCE["repository"].removeprefix("https://github.com/").strip("/").removesuffix(".git")
REPO_URL = f"https://github.com/{REPO}.git"
PREPARE_SCRIPTS = ("tools/download_deps.sh",
                   "tools/apply_relapse_patch.sh",
                   "tools/apply_slopkit_patch.sh",
                   "tools/apply_umtx2_patch.sh")
TAG_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")


class UpstreamError(RuntimeError):
    pass


def _run(args, cwd=None, env=None):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if result.returncode != 0:
        raise UpstreamError(f"command failed ({result.returncode}): {' '.join(str(a) for a in args)}\n"
                            + result.stdout.strip())
    return result.stdout


def git_env(base=None):
    """Environment for every git subprocess: worktrees must keep the exact
    bytes upstream committed. checkout CRLF conversion (Git for Windows
    defaults to autocrlf=true) breaks the upstream `git apply` patch steps.
    GIT_CONFIG_* injects into subcommand-spawned git processes too (submodule
    clones, git apply inside the upstream scripts)."""
    env = dict(base if base is not None else os.environ)
    count = int(env.get("GIT_CONFIG_COUNT", "0") or 0)
    existing = {env.get(f"GIT_CONFIG_KEY_{i}") for i in range(count)}
    for key, value in (("core.autocrlf", "false"), ("core.eol", "lf")):
        if key not in existing:
            env[f"GIT_CONFIG_KEY_{count}"] = key
            env[f"GIT_CONFIG_VALUE_{count}"] = value
            count += 1
    env["GIT_CONFIG_COUNT"] = str(count)
    return env


def _github_api(path):
    request = urllib.request.Request("https://api.github.com" + path, headers={
        "User-Agent": "ps5-wkal-esp32s3-build",
        "Accept": "application/vnd.github+json",
    })
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        request.add_header("Authorization", "Bearer " + token)
    last = None
    for attempt in range(4):
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                return json.loads(response.read())
        except Exception as exc:  # rate limits and transient CDN drops
            last = exc
            time.sleep(2 * attempt + 1)
    raise UpstreamError(f"GitHub API request failed: {path} ({last})")


def _peeled_commit(tag):
    """Commit a tag points at, including annotated tags ('tag^{}')."""
    out = _run(["git", "ls-remote", REPO_URL,
                f"refs/tags/{tag}", f"refs/tags/{tag}^{{}}"])
    commit = None
    for line in out.splitlines():
        sha, _, ref = line.partition("\t")
        commit = sha.strip()  # the peeled ^{} line wins when present
    if not commit:
        raise UpstreamError(f"tag {tag} not found in {REPO}")
    return commit


def latest_release_tag():
    """Latest stable upstream release tag, e.g. 'v0.4.0'.

    Falls back to the highest plain vX.Y.Z tag known to git when the API
    is unreachable or rate-limited; -pre/-dev/-suffix tags never win.
    """
    try:
        release = _github_api(f"/repos/{REPO}/releases/latest")
        tag = release.get("tag_name")
        if tag:
            return tag
    except UpstreamError as exc:
        print(f"note: release API unavailable ({exc}); falling back to git tags", file=sys.stderr)
    out = _run(["git", "ls-remote", "--tags", REPO_URL])
    tags = {line.split("refs/tags/")[-1].strip() for line in out.splitlines()
            if "refs/tags/" in line and "^{}" not in line}
    stable = sorted((t for t in tags if TAG_RE.match(t)),
                    key=lambda t: tuple(int(p) for p in TAG_RE.match(t).groups()))
    if not stable:
        raise UpstreamError(f"no stable vX.Y.Z release tag found in {REPO}")
    return stable[-1]


def _require_tool(name, hint):
    if not shutil.which(name):
        raise UpstreamError(f"'{name}' is required but not on PATH. {hint}")


def _force_rmtree(path):
    """rmtree that clears the read-only bit git puts on pack files (Windows/SMB)."""
    def relax(function, target, _exc):
        os.chmod(target, stat.S_IWRITE)
        function(target)
    handler = relax
    if sys.version_info >= (3, 12):
        shutil.rmtree(path, onexc=handler)
    else:
        shutil.rmtree(path, onerror=lambda function, target, exc: relax(function, target, exc))


def _prepared(stage):
    """True when a cached tree is complete for its recorded commit."""
    record = stage / ".wkal-upstream.json"
    frontend = stage / "frontend/autoloader"
    markers = [stage / "include/wkali.h", stage / "tools/gen_icons.py",
               frontend / "index.html", frontend / "app.js",
               frontend / "relapse/src", frontend / "slopkit",
               frontend / "umtx2", frontend / "payloads/payload.elf"]
    return record.is_file() and all(p.exists() for p in markers)


def _prepare(stage):
    bash = shutil.which("bash")
    scripts = " ".join(PREPARE_SCRIPTS)
    if not bash:
        raise UpstreamError("bash is required to run the upstream preparation "
                            f"scripts ({scripts}); on Windows use Git Bash")
    env = git_env()
    for script in PREPARE_SCRIPTS:
        print(f"upstream: preparing {script} ...")
        output = _run([bash, script], cwd=stage, env=env)
        tail = "\n".join(output.strip().splitlines()[-3:])
        if tail:
            print("  " + tail.replace("\n", "\n  "))
    (stage / ".wkal-prepared").write_text("")


def stage(spec=None):
    """Return (path, provenance) for the requested upstream source tree.

    spec: None/'release' -> latest upstream release tag; 'vX.Y.Z' -> that
    tag; an existing directory -> used as-is (already-prepared local clone).
    """
    env = dict(os.environ)
    if spec in (None, "", "release", "latest"):
        tag = env.get("WKAL_UPSTREAM_TAG") or latest_release_tag()
    elif Path(spec).is_dir():
        local = Path(spec).resolve()
        if not _prepared(local):
            _prepare(local)
        commit = None
        if (local / ".git").exists():
            try:
                commit = _run(["git", "rev-parse", "HEAD"], cwd=local).strip()
            except UpstreamError:
                pass
        return local, {"repository": REPO, "source": str(local),
                       "tag": None, "commit": commit, "mode": "local"}
    else:
        tag = spec

    _require_tool("git", "Needed to clone the upstream release.")
    env = git_env()
    commit = _peeled_commit(tag)
    stage = DEPS / tag
    record = stage / ".wkal-upstream.json"
    if record.is_file():
        cached = json.loads(record.read_text())
        if cached.get("commit") == commit:
            if not _prepared(stage):
                _prepare(stage)
            record.write_text(json.dumps(cached, indent=2) + "\n")
            return stage, cached
        print(f"upstream: {tag} moved ({cached.get('commit')[:12]} -> {commit[:12]}); re-cloning")
        _force_rmtree(stage)
    elif stage.exists():
        # Leftover from an interrupted earlier run. Reuse the clone when it
        # still points at the tag and just (re)run the idempotent prepare
        # steps; wipe it when the clone itself is broken.
        try:
            staged_commit = _run(["git", "rev-parse", "HEAD"], cwd=stage, env=env).strip()
        except UpstreamError:
            staged_commit = None
        if staged_commit == commit:
            print(f"upstream: reusing interrupted tree at {tag}; re-running prepare")
            _prepare(stage)
            provenance = {"repository": REPO, "source": REPO_URL, "tag": tag,
                          "commit": commit, "mode": "release-tag"}
            record.write_text(json.dumps(provenance, indent=2) + "\n")
            return stage, provenance
        _force_rmtree(stage)
    print(f"upstream: cloning {REPO_URL} at {tag} ({commit[:12]}) with submodules ...")
    DEPS.mkdir(parents=True, exist_ok=True)
    _run(["git", "clone", "--depth", "1", "--branch", tag,
          "--recurse-submodules", "--shallow-submodules", REPO_URL, stage], env=env)
    staged_commit = _run(["git", "rev-parse", "HEAD"], cwd=stage, env=env).strip()
    if staged_commit != commit:
        _force_rmtree(stage)
        raise UpstreamError(f"cloned {tag} at {staged_commit}, expected {commit}")
    _prepare(stage)
    provenance = {"repository": REPO, "source": REPO_URL, "tag": tag,
                  "commit": commit, "mode": "release-tag"}
    (stage / ".wkal-upstream.json").write_text(json.dumps(provenance, indent=2) + "\n")
    return stage, provenance


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Stage the upstream frontend source tree and print its provenance.")
    parser.add_argument("spec", nargs="?", default=None,
                        help="'release' (default), an upstream tag like v0.4.0, or a local clone path")
    args = parser.parse_args()
    try:
        path, info = stage(args.spec)
    except UpstreamError as error:
        sys.exit(f"error: {error}")
    print(json.dumps({"path": str(path), **info}, indent=2))
