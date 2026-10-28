// Turns user actions into workload text for the real engine, and the engine's trace into steps.
import { loadEngine, runSync, type TraceLine } from './engine';

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
export const engineReady = engine();

export function contiguousText(memorySize: number, ops: ContOp[]): string {
  const lines = ['schema 1', 'kind contiguous', `memory_size ${memorySize}`, ''];
  for (const op of ops) {
    if (op.kind === 'alloc') lines.push(`alloc ${op.pid} ${op.id} ${op.size}`);
    else if (op.kind === 'free') lines.push(`free ${op.pid} ${op.id}`);
    else lines.push('compact');
  }
  return lines.join('\n') + '\n';
}

export function pagingText(frames: number, refs: number[]): string {
  const pages = Math.max(1, ...refs.map((r) => r + 1));
  const lines = ['schema 1', 'kind paging', 'page_size 4096', `frames ${frames}`, '', `spawn P0 ${pages}`];
  for (const r of refs) lines.push(`access P0 ${r}`);
  return lines.join('\n') + '\n';
}

function toSteps(lines: TraceLine[]): Run {
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
  const violation = by('AccessViolation');
  if (violation) return [s('Access rejected: '), { t: violation.reason === 'OUT_OF_RANGE' ? 'page outside the address space' : 'no such process', k: 'bad' }, s('.')];
  if (hit) return [s('Page '), n(hit.vpn), s(' is in frame '), n(hit.frame), s(': '), { t: 'hit', k: 'ok' }, s('.')];
  if (fault && loaded) {
    const out: Tok[] = [s('Page '), n(fault.vpn), s(' is not in memory: '), { t: 'page fault', k: 'bad' }, s('. ')];
    if (victim) out.push(s(`No free frame, so ${label} evicts page `), n(victim.vpn), s(' from frame '), n(victim.frame), s(`${why(policy)}. `));
    out.push(s('Page '), n(loaded.vpn), s(' is loaded into frame '), n(loaded.frame), s('.'));
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
export const AC3_REFS = [7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1];
export const AC4_REFS = [1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5];
