// Turns user actions into workload text for the real engine, and the engine's trace into steps.
import { loadEngine, runSync, genAllocUniform, genRefsLocality, type TraceLine, type AllocUniform, type RefsLocality } from './engine';

export type Block = {
  start: number;
  size: number;
  state: 'free' | 'allocated';
  pid?: string;
  id?: string;
  requested?: number;
};
export type Frag = {
  total_free: number;
  largest_hole: number;
  hole_count: number;
  ext_frag: number;
  used: number;
  live_blocks: number;
};
export type Frame = { frame: number; pid: string | null; vpn: number | null; referenced: boolean; dirty: boolean };
export type Step = { step: number; events: TraceLine[]; blocks: Block[]; frag?: Frag; frames: Frame[] };
export type Run = { ok: true; steps: Step[]; summary: Record<string, number> } | { ok: false; error: string };

export type ContOp =
  | { kind: 'alloc'; pid: string; id: string; size: number }
  | { kind: 'free'; pid: string; id: string }
  | { kind: 'terminate'; pid: string }
  | { kind: 'compact' };

export const PLACEMENT = ['first-fit', 'best-fit', 'worst-fit'] as const;
export const REPLACEMENT = ['fifo', 'lru', 'opt'] as const;
export const POLICY_LABEL: Record<string, string> = {
  'first-fit': 'First-Fit',
  'best-fit': 'Best-Fit',
  'worst-fit': 'Worst-Fit',
  fifo: 'FIFO',
  lru: 'LRU',
  opt: 'OPT',
};

const engine = () => loadEngine(() => fetch(`${import.meta.env.BASE_URL}memsim.wasm`).then((r) => r.arrayBuffer()));

export function contiguousText(memorySize: number, ops: ContOp[]): string {
  const lines = ['schema 1', 'kind contiguous', `memory_size ${memorySize}`, ''];
  for (const op of ops) {
    if (op.kind === 'alloc') lines.push(`alloc ${op.pid} ${op.id} ${op.size}`);
    else if (op.kind === 'free') lines.push(`free ${op.pid} ${op.id}`);
    else if (op.kind === 'terminate') lines.push(`terminate ${op.pid}`);
    else lines.push('compact');
  }
  return lines.join('\n') + '\n';
}

// One reference: [P<n>:]<page> or [P<n>:]@<virtual address>, then an optional w for a write.
export type Ref = { pid: string; vpn: number; va: number | null; write: boolean };
export function parseRefs(text: string, pageSize: number): { refs: Ref[]; bad: string[] } {
  const refs: Ref[] = [];
  const bad: string[] = [];
  for (const tok of text.split(/[\s,]+/).filter(Boolean)) {
    const m = /^(?:(P\d{1,2}):)?(@?)(\d{1,7})(w?)$/i.exec(tok);
    if (!m) {
      bad.push(tok);
      continue;
    }
    const n = Number(m[3]);
    const va = m[2] ? n : null;
    const vpn = va === null ? n : Math.floor(n / pageSize);
    if (vpn > 255) bad.push(tok);
    else refs.push({ pid: (m[1] ?? 'P0').toUpperCase(), vpn, va, write: m[4] !== '' });
  }
  return { refs, bad };
}

// Each process is spawned first with just enough pages for its highest reference.
export function processesOf(refs: Ref[]): { pid: string; pages: number }[] {
  const out = new Map<string, number>();
  for (const r of refs) out.set(r.pid, Math.max(out.get(r.pid) ?? 1, r.vpn + 1));
  if (!out.size) out.set('P0', 1);
  return [...out].map(([pid, pages]) => ({ pid, pages }));
}

export function pagingText(frames: number, pageSize: number, refs: Ref[]): string {
  const lines = ['schema 1', 'kind paging', `page_size ${pageSize}`, `frames ${frames}`, ''];
  for (const p of processesOf(refs)) lines.push(`spawn ${p.pid} ${p.pages}`);
  for (const r of refs) lines.push(`access ${r.pid} ${r.va === null ? r.vpn : '@' + r.va}${r.write ? ' w' : ''}`);
  return lines.join('\n') + '\n';
}

export function toSteps(lines: TraceLine[]): Run {
  const steps: Step[] = [];
  let pending: TraceLine[] = [];
  let summary: Record<string, number> = {};
  for (const l of lines) {
    if (l.type === 'TraceHeader') continue;
    if (l.type === 'RunSummary') summary = l.metrics;
    else if (l.type === 'Snapshot') {
      const c = l.contiguous;
      const p = l.paging;
      steps.push({
        step: l.step as number,
        events: pending,
        blocks: c ? c.blocks : [],
        frag: c ? c.frag : undefined,
        frames: p ? p.frames : [],
      });
      pending = [];
    } else pending.push(l);
  }
  return { ok: true, steps, summary };
}

// Generated workloads, parsed back into lab state. The text is exactly what `memsim gen` writes.
export async function generateContiguous(p: AllocUniform): Promise<{ memory: number; ops: ContOp[] } | null> {
  const text = genAllocUniform(await engine(), p);
  if (text === null) return null;
  const ops: ContOp[] = [];
  for (const line of text.split('\n')) {
    const t = line.split('#')[0].trim().split(/\s+/);
    if (t[0] === 'alloc') ops.push({ kind: 'alloc', pid: t[1], id: t[2], size: Number(t[3]) });
    else if (t[0] === 'free') ops.push({ kind: 'free', pid: t[1], id: t[2] });
  }
  return { memory: p.memorySize, ops };
}

export async function generateReferences(p: RefsLocality): Promise<string | null> {
  const text = genRefsLocality(await engine(), p);
  if (text === null) return null;
  return text
    .split('\n')
    .map((l) => l.split('#')[0].trim().split(/\s+/))
    .filter((t) => t[0] === 'access')
    .map((t) => `${t[2]}${t[3] === 'w' ? 'w' : ''}`)
    .join(' ');
}

export async function run(text: string, policy: string): Promise<Run> {
  const r = runSync(await engine(), text, policy);
  if (!r.ok) return { ok: false, error: r.line ? `line ${r.line}: ${r.message}` : r.message };
  return toSteps(r.lines);
}

// Narration: one sentence per step, as tokens so numbers and ids can be styled.
export type Tok = { t: string; k?: 'num' | 'id' | 'bad' | 'ok'; pid?: string };
const n = (v: number | string): Tok => ({ t: String(v), k: 'num' });
const id = (v: string, pid?: string): Tok => ({ t: v, k: 'id', pid });
const s = (t: string): Tok => ({ t });

export function narrate(events: TraceLine[], policy: string): Tok[] {
  const by = (type: string) => events.find((e) => e.type === type)?.data;
  const label = POLICY_LABEL[policy] ?? policy;
  const placed = by('PlacementDecided');
  const failed = by('AllocationFailed');
  const allocated = by('BlockAllocated');
  const split = by('BlockSplit');
  const freed = by('BlockFreed');
  const merged = by('BlocksCoalesced');
  const compacted = by('CompactionCompleted');
  const rejAlloc = by('AllocRejected');
  const rejFree = by('FreeRejected');
  const skipped = by('FreeSkipped');
  if (rejAlloc) return [s('Rejected: '), id(rejAlloc.id, rejAlloc.pid), s(` ${reason(rejAlloc.reason)}. Memory is unchanged.`)];
  if (rejFree) return [id(rejFree.pid, rejFree.pid), s(' cannot free '), id(rejFree.id), s(`: ${reason(rejFree.reason)}. Memory is unchanged.`)];
  if (skipped) return [s('Free of '), id(skipped.id, skipped.pid), s(' is skipped: its allocation had failed.')];
  if (failed) {
    const frag = failed.cause === 'FRAGMENTATION';
    return [
      id(failed.pid, failed.pid), s(' requests '), n(failed.requested), s('. '),
      n(failed.total_free), s(' units are free, but '),
      ...(frag
        ? [s('the largest hole is only '), n(failed.largest_hole), s(', so it fails: '), { t: 'external fragmentation', k: 'bad' as const }, s('.')]
        : [s('that is not enough in total, so it fails: '), { t: 'out of memory', k: 'bad' as const }, s('.')]),
    ];
  }
  if (allocated && placed) {
    const rest: Tok[] = split ? [s(', leaving a hole of '), n(split.remainder.size)] : [s(', an exact fit')];
    return [
      id(allocated.pid, allocated.pid), s(' requests '), n(allocated.size), s(`. ${label} examined `), n(placed.scanned),
      s(placed.scanned === 1 ? ' hole and placed it at ' : ' holes and placed it at '), n(allocated.start),
      s(' in the '), n(placed.hole.size), s('-unit hole'), ...rest, s('.'),
    ];
  }
  if (compacted) {
    if (compacted.blocks_moved === 0) return [s('Compaction: memory is already packed, nothing moves.')];
    return [
      s('Compaction slides '), n(compacted.blocks_moved), s(' blocks down ('), n(compacted.units_moved),
      s(' units copied), leaving one hole of '), n(compacted.largest_after), s('.'),
    ];
  }
  const term = by('ProcessTerminated');
  if (term && term.freed_blocks !== undefined && !by('ProcessSpawned')) {
    if (!term.freed_blocks) return [id(term.pid, term.pid), s(' terminates; it held no memory.')];
    return [id(term.pid, term.pid), s(' terminates; the OS frees all '), n(term.freed_blocks), s(term.freed_blocks === 1 ? ' block' : ' of its blocks'), s(', merging any neighbouring holes.')];
  }
  if (freed) {
    const out: Tok[] = [id(freed.pid, freed.pid), s(' frees '), id(freed.id, freed.pid), s(' ('), n(freed.size), s(' at '), n(freed.start), s(')')];
    if (merged) out.push(s('; it merges with its free neighbours into one hole of '), n(merged.result.size));
    out.push(s('.'));
    return out;
  }
  const hit = by('PageHit');
  const fault = by('PageFault');
  const victim = by('VictimSelected');
  const loaded = by('PageLoaded');
  const wb = by('PageWrittenBack');
  const tr = by('AddressTranslated');
  const violation = by('AccessViolation');
  const spawned = by('ProcessSpawned');
  if (spawned) return [id(spawned.pid), s(' starts with an address space of '), n(spawned.pages), s(spawned.pages === 1 ? ' page; nothing is in memory yet.' : ' pages; nothing is in memory yet.')];
  if (violation) return [id(violation.pid), s(' is stopped: '), { t: violation.reason === 'OUT_OF_RANGE' ? 'that address is outside its address space' : 'no such process', k: 'bad' }, s('.')];
  const who = (x: { pid: string; vpn: number; write: boolean }): Tok[] => [id(x.pid), s(x.write ? ' writes page ' : ' reads page '), n(x.vpn)];
  const where: Tok[] = tr ? [s(' (address '), n(tr.va), s(' = page '), n(tr.vpn), s(' + offset '), n(tr.offset), s(')')] : [];
  if (hit) return [...who(hit), ...where, s(': it is in frame '), n(hit.frame), s(', a '), { t: 'hit', k: 'ok' }, s(tr ? `, so the physical address is ${tr.pa}.` : '.')];
  if (fault && loaded) {
    const out: Tok[] = [...who(fault), ...where, s(': '), { t: fault.kind === 'COMPULSORY' ? 'page fault (first use)' : 'page fault', k: 'bad' }, s('. ')];
    if (victim) {
      out.push(s(`Memory is full, so ${label} evicts `), id(victim.pid), s(' page '), n(victim.vpn), s(' from frame '), n(victim.frame), s(`${why(policy)}`));
      out.push(wb ? s('; it was modified, so it is written back to disk first. ') : s('. '));
    } else out.push(s('Frame '), n(loaded.frame), s(' is free. '));
    out.push(s('The page is loaded into frame '), n(loaded.frame), s(tr ? `; physical address ${tr.pa}.` : '.'));
    return out;
  }
  return [s(events.length ? events.map((e) => e.type).join(', ') : 'Initial state.')];
}

function reason(r: string): string {
  return (
    {
      INVALID_SIZE: 'the size must be positive',
      DUPLICATE_ID: 'that id was used before',
      UNKNOWN_ID: 'no such allocation',
      DOUBLE_FREE: 'it was already freed',
      WRONG_OWNER: 'it belongs to another process',
    }[r] ?? r.toLowerCase()
  );
}

function why(policy: string): string {
  return { fifo: ', the page loaded longest ago', lru: ', the page used longest ago', opt: ', the page needed furthest in the future' }[policy] ?? '';
}

// Classic examples, kept identical to tests/fixtures so the demo matches the acceptance tests.
export const SILBERSCHATZ_SETUP: ContOp[] = (() => {
  const ops: ContOp[] = [];
  [100, 500, 200, 300, 600].forEach((size, i) => {
    ops.push({ kind: 'alloc', pid: 'SYS', id: `h${i + 1}`, size });
    if (i < 4) ops.push({ kind: 'alloc', pid: 'SYS', id: `s${i + 1}`, size: 10 });
  });
  for (let i = 1; i <= 5; i++) ops.push({ kind: 'free', pid: 'SYS', id: `h${i}` });
  return ops;
})();
export const SILBERSCHATZ_REQUESTS: ContOp[] = [
  { kind: 'alloc', pid: 'P1', id: 'a1', size: 212 },
  { kind: 'alloc', pid: 'P2', id: 'a2', size: 417 },
  { kind: 'alloc', pid: 'P3', id: 'a3', size: 112 },
  { kind: 'alloc', pid: 'P4', id: 'a4', size: 426 },
];
export const AC3_REFS = '7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1';
export const AC4_REFS = '1 2 3 4 1 2 5 1 2 3 4 5';
export const MULTI_REFS = 'P1:0 P1:1w P2:0 P2:1 P1:2 P2:0w P1:0 P2:2 P1:1 P2:3w P1:3 P2:0';
export const VA_REFS = '@6700 @120 @9000w @6800 @14000 @200w @6750 @9100';
