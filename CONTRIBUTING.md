# Contributing

The workflow is in the [README](README.md#contributing) and the C rules are in
[docs/CONVENTIONS.md](docs/CONVENTIONS.md). In short: branch as `feat/<module>-<topic>` from `main`,
use Conventional Commits, run `pre-commit run --all-files` and `ctest --preset dev` (and `npm test`
in `web/` when touching the web app) before opening a pull request, and get one approval. Merging
needs the **CI passed** check.
