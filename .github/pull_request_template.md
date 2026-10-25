## What and why

<!-- One or two sentences. Link the issue: "Closes #123". -->

## Module and owner

<!-- e.g. contiguous (P2). Cross-module changes need P1 as well. -->

## Checklist

- [ ] `ctest --preset dev` passes locally, and `pre-commit` ran on every commit
- [ ] New behaviour has unit tests; golden expectations changed only with P8's approval
- [ ] No new I/O, clock or randomness in `common`, `contiguous` or `paging`
- [ ] Changes a shared header in `src/common/`, an exported interface, an event or a file format: RFC issue linked here: <!-- #… -->
- [ ] Docs updated (`docs/ARCHITECTURE.md`, `docs/CONVENTIONS.md`) if behaviour or a contract changed
