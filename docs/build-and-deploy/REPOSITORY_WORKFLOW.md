# Repository workflow

The canonical repository is [mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi),
branch `main`. Use `origin` for that repository in a normal development checkout.
Build and test the source from the branch you intend to publish.

## One source tree

The same checkout contains the native hook, Java patches, renderers, installer,
documentation, wireless experiments and CarPlay lab. Their presence in Git does
not enable them in a release. The wired hook is built explicitly with
`HOOK_WIRELESS=0 HOOK_AUDIO=1`.

Keep generated binaries and reports in ignored output directories. A directory
under `build/` is not an alternative source branch. If an experiment produces
source changes that should survive, commit them on a named branch before relying
on its build results.

Use `git worktree list`, `git branch -vv` and `git status --short` to check which
checkout owns a branch and whether its source matches a commit. Local archive
branches preserve earlier work; they are not release inputs. Do not merge an
archive merely because it has commits absent from `main`: a published snapshot
can already contain equivalent changes with different commit IDs.

## Build and verify

Follow the [build system](BUILD_SYSTEM.md) and [release instructions](DEPLOY_AND_RELEASE.md).
The wired verification includes the native hook, both renderers, regional Java
variants, host tests and installer checks. Package those outputs from the same
source tree. A passing host build does not establish physical-HU acceptance or
production readiness of optional wireless code.

Documentation links must work from a clean checkout. Describe ignored logs and
reports as local generated artifacts rather than linking to them as repository
files. Check the map and links with:

```sh
python3 scripts/gen_docs_map.py --check
python3 scripts/check_docs.py
```

Publishing is a separate action from building or committing. Review the target
remote, branch and diff before pushing; push the intended branch explicitly.
