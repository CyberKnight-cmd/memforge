// The real MemForge C engine, compiled to WebAssembly (scripts/build_wasm.sh).
// run() is one deterministic `memsim run` of a workload; it returns the parsed section 11 trace.

export type TraceLine = { type: string; seq?: number; step?: number; data?: any; [k: string]: any };
export type RunResult = { ok: true; lines: TraceLine[] } | { ok: false; line: number; message: string };

type Exports = {
  memory: WebAssembly.Memory;
  _initialize: () => void;
  memsim_alloc: (n: number) => number;
  memsim_free: (p: number) => void;
  memsim_run: (text: number, len: number, policy: number) => number;
};

let exportsPromise: Promise<Exports> | null = null;

// The simulator never touches files, clocks or the network; libc only needs these to link.
function wasiShim(memory: () => WebAssembly.Memory) {
  const ENOSYS = 52;
  const BADF = 8;
  return {
    fd_write: (fd: number, iovs: number, n: number, written: number) => {
      const view = new DataView(memory().buffer);
      let total = 0;
      for (let i = 0; i < n; i++) total += view.getUint32(iovs + i * 8 + 4, true);
      view.setUint32(written, total, true);
      return fd === 1 || fd === 2 ? 0 : BADF;
    },
    fd_close: () => 0,
    fd_seek: () => ENOSYS,
    fd_prestat_get: () => BADF,
    fd_prestat_dir_name: () => BADF,
    clock_time_get: (_id: number, _precision: bigint, out: number) => {
      new DataView(memory().buffer).setBigUint64(out, 0n, true);
      return 0;
    },
    proc_exit: (code: number) => {
      throw new Error(`memsim aborted (exit ${code}): a contract violation in the engine`);
    },
  };
}

export function loadEngine(source: () => Promise<ArrayBuffer>): Promise<Exports> {
  if (!exportsPromise) {
    exportsPromise = (async () => {
      let ex: Exports | null = null;
      const { instance } = await WebAssembly.instantiate(await source(), {
        wasi_snapshot_preview1: wasiShim(() => ex!.memory),
      });
      ex = instance.exports as unknown as Exports;
      ex._initialize();
      return ex;
    })();
  }
  return exportsPromise;
}

const enc = new TextEncoder();
const dec = new TextDecoder();

function putString(ex: Exports, s: string): [number, number] {
  const bytes = enc.encode(s);
  const p = ex.memsim_alloc(bytes.length + 1);
  const mem = new Uint8Array(ex.memory.buffer, p, bytes.length + 1);
  mem.set(bytes);
  mem[bytes.length] = 0;
  return [p, bytes.length];
}

export function runSync(ex: Exports, workload: string, policy: string): RunResult {
  const [tp, tl] = putString(ex, workload);
  const [pp] = putString(ex, policy);
  const rp = ex.memsim_run(tp, tl, pp);
  ex.memsim_free(tp);
  ex.memsim_free(pp);
  if (rp === 0) return { ok: false, line: 0, message: 'out of memory' };
  const heap = new Uint8Array(ex.memory.buffer);
  let end = rp;
  while (heap[end] !== 0) end++;
  const text = dec.decode(heap.subarray(rp, end));
  ex.memsim_free(rp);
  if (text.startsWith('!')) {
    const colon = text.indexOf(':');
    return { ok: false, line: Number(text.slice(1, colon)), message: text.slice(colon + 1) };
  }
  return { ok: true, lines: text.trim().split('\n').map((l) => JSON.parse(l)) };
}
