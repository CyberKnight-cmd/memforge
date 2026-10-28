# Submission checklist (due Sunday, November 1, 2026)

- [ ] `main` is green: the **CI passed** check on the last commit.
- [ ] `bash scripts/experiments/run_all.sh` and `bash scripts/report/verify_report.sh` rerun on the
      submitted commit; `git status` shows no changes to `docs/report/`.
- [ ] All 8 rows of `docs/report/tables/verification.md` read PASS.
- [ ] Names, roll numbers, university, course and supervisor filled in
      (`docs/report/sections/01-front-page.md`, `02-declaration.md`, and the author block of
      `docs/report/journal/memforge-ieee.md`).
- [ ] The 9 screenshots in `docs/report/figures/screenshots/` captured (see its README).
- [ ] Every report section reviewed by its owner (P1 to P10).
- [ ] Demo rehearsed end to end from `docs/DEMO.md`; a screen recording kept as fallback.
- [ ] Tag the submitted commit: `git tag -a v1.0 -m "Submission"` and push the tag.
- [ ] Submit the repository link (or archive of the tag), the report and the paper.
