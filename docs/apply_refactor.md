# Applying the standalone refactor

Run `web_htop_refactor.sh` from the root of the supplied project on Linux or WSL2.
The script embeds its source payload as readable text and uses only Bash/Python
standard-library facilities to apply it. The normal build gate needs a C++20
compiler, CMake, CTest and Python. It does not download dependencies.

```bash
# Optional inspection; these commands do not modify source files.
bash /path/to/web_htop_refactor.sh --check
bash /path/to/web_htop_refactor.sh --diff

# Default: validate, back up, apply, build Release, run core/integration tests.
bash /path/to/web_htop_refactor.sh
```

The source baseline is the supplied archive, whose Git HEAD is
`0c9fc59ec72f49f7dd0f4c09ea1380760c815d95`. Actual compatibility is checked with
per-file SHA-256 hashes, not just Git HEAD. An already-updated file is accepted;
a conflicting local edit is refused. Unrelated files, the Git index and Git history
are untouched. Symlinked source paths are refused.

Backups and a build log are kept under `.web_htop-refactor/backup-...`. Each source
replacement uses an atomic rename. If application or the build/test gate fails,
the script attempts to restore source files from that backup. Builds and backup
files are retained for diagnosis. This is a file transaction with crash recovery,
not a filesystem-wide atomic transaction.

Successful binaries are placed in `build-senior`:

```bash
./build-senior/server/web_htop_server
./build-senior/client/web_htop_client localhost 9999 8080
```

Use `--no-build` only when source-only application is intended. It does not claim
that the local toolchain can build the result. `--jobs N` controls build parallelism.
The script does not start a service, install packages, load BPF or create a commit.

```bash
# Restore the latest backup, or pass the exact backup directory name.
bash /path/to/web_htop_refactor.sh --rollback
```

Rollback checks for later edits and refuses to overwrite them. Back up/reconcile
such edits before retrying; there is deliberately no force/reset option. A hard
kill during application can leave a mixed source tree: the recorded backup remains
available to this same rollback command. Empty created directories may remain.
