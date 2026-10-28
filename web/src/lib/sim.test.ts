// The browser build must behave exactly like the CLI. Run: npm test (needs public/memsim.wasm and build/release).
import { readFileSync, existsSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { resolve } from 'node:path';
import { describe, expect, it, beforeAll } from 'vitest';
import { loadEngine, runSync } from './engine';
import { parseRefs, pagingText, toSteps, contiguousText, narrate, MULTI_REFS, VA_REFS, AC3_REFS, AC4_REFS, SILBERSCHATZ_SETUP, SILBERSCHATZ_REQUESTS, type Run } from './sim';

const root = resolve(__dirname, '../../..');
let ex: Awaited<ReturnType<typeof loadEngine>>;
beforeAll(async () => {
  ex = await loadEngine(async () => readFileSync(resolve(root, 'web/public/memsim.wasm')).buffer as ArrayBuffer);
});
const go = (text: string, p: string) => {
  const r = runSync(ex, text, p);
  if (!r.ok) throw new Error(r.message);
  return toSteps(r.lines) as Extract<Run, { ok: true }>;
};
const refs = (s: string) => parseRefs(s, 4096).refs;
const said = (t: { t: string }[]) => t.map((x) => x.t).join('');

describe('WebAssembly engine', () => {
  const cli = resolve(root, 'build/release/bin/memsim');
  it.runIf(existsSync(cli))('produces the same trace as the CLI on every fixture', () => {
    const cases: [string, string[]][] = [
      ['ac1_silberschatz', ['first-fit', 'best-fit', 'worst-fit']],
      ['ac2_compact', ['first-fit', 'best-fit', 'worst-fit']],
      ['ac5_contiguous', ['first-fit']],
      ['ac3_refs', ['fifo', 'lru', 'opt']],
      ['ac4_belady', ['fifo', 'lru', 'opt']],
      ['ac5_paging', ['fifo']],
    ];
    for (const [f, ps] of cases)
      for (const p of ps) {
        const file = resolve(root, `tests/fixtures/${f}.wl`);
        const out = '/tmp/memforge-web-test.jsonl';
        execFileSync(cli, ['run', file, '--policy', p, '--trace', out]);
        const want = readFileSync(out, 'utf8').trim().split('\n').map((l) => JSON.parse(l)).slice(1);
        const got = runSync(ex, readFileSync(file, 'utf8'), p);
        expect(got.ok).toBe(true);
        if (got.ok) expect(got.lines.slice(1), `${f} ${p}`).toEqual(want);
      }
  });

  it('AC-1 placements and the fragmentation failure', () => {
    const r = go(contiguousText(1740, [...SILBERSCHATZ_SETUP, ...SILBERSCHATZ_REQUESTS]), 'first-fit');
    expect(said(narrate(r.steps.at(-1)!.events, 'first-fit'))).toContain('largest hole is only 300');
  });

  it('AC-3 and AC-4 through the paging lab path', () => {
    expect(['fifo', 'lru', 'opt'].map((p) => go(pagingText(3, 4096, refs(AC3_REFS)), p).summary.faults)).toEqual([15, 12, 9]);
    expect([3, 4].map((f) => go(pagingText(f, 4096, refs(AC4_REFS)), 'fifo').summary.faults)).toEqual([9, 10]);
  });

  it('two processes with writes produce write-backs', () => {
    const r = go(pagingText(4, 4096, refs(MULTI_REFS)), 'fifo');
    expect(r.summary.writebacks).toBeGreaterThan(0);
    expect(r.steps.some((s) => s.events.some((e) => e.type === 'PageWrittenBack'))).toBe(true);
  });

  it('virtual addresses translate page + offset to frame + offset', () => {
    const r = go(pagingText(2, 4096, refs(VA_REFS)), 'fifo');
    const tr = r.steps.flatMap((s) => s.events.filter((e) => e.type === 'AddressTranslated').map((e) => e.data));
    expect(tr.length).toBe(8);
    for (const t of tr) {
      expect(t.vpn).toBe(Math.floor(t.va / 4096));
      expect(t.offset).toBe(t.va % 4096);
      expect(t.pa).toBe(t.frame * 4096 + t.offset);
    }
  });

  it('terminate frees every block of the process', () => {
    const r = go(contiguousText(1740, [...SILBERSCHATZ_SETUP, ...SILBERSCHATZ_REQUESTS.slice(0, 2), { kind: 'terminate', pid: 'P1' }]), 'first-fit');
    const last = r.steps.at(-1)!;
    expect(last.blocks.some((b) => b.pid === 'P1')).toBe(false);
    expect(said(narrate(last.events, 'first-fit'))).toContain('P1 terminates');
  });

  it('parses the reference syntax', () => {
    expect(parseRefs('7 7w P2:3 @6700 P1:@100w x', 4096)).toEqual({
      refs: [
        { pid: 'P0', vpn: 7, va: null, write: false },
        { pid: 'P0', vpn: 7, va: null, write: true },
        { pid: 'P2', vpn: 3, va: null, write: false },
        { pid: 'P0', vpn: 1, va: 6700, write: false },
        { pid: 'P1', vpn: 0, va: 100, write: true },
      ],
      bad: ['x'],
    });
  });
});
