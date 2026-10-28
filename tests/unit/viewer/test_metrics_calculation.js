/**
 * MemForge P9 - Metrics & Invariants Verification Tests
 * Cross-checks metric calculation against Architecture section 8 & section 13
 */

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const {
  TraceParser,
  ContiguousRenderer,
  PlaybackEngine
} = require('../../../viewer/app.js');

let passedTests = 0;
let failedTests = 0;

function test(name, fn) {
  try {
    fn();
    console.log(`  ✓ PASS: ${name}`);
    passedTests++;
  } catch (err) {
    console.error(`  ✗ FAIL: ${name}`);
    console.error(err);
    failedTests++;
  }
}

console.log('=== Running MemForge P9 Metrics & Invariants Tests ===\n');

// 1. AC-1 First-Fit Hand-Calculated Invariants
test('AC-1 First-Fit metrics match architecture formulas', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/ac1_first_fit.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);
  assert.strictEqual(res.ok, true);

  const step4 = res.trace.steps[4];
  const cont = step4.snapshot.contiguous;
  const frag = cont.frag;

  // Expected from architecture: total free 959, largest hole 300, hole count 5
  assert.strictEqual(frag.total_free, 959);
  assert.strictEqual(frag.largest_hole, 300);
  assert.strictEqual(frag.hole_count, 5);

  // ext_frag = 1 - largest_hole / total_free = 1 - 300 / 959 = 0.687174...
  const expectedExtFrag = 1 - (300 / 959);
  assert.ok(Math.abs(frag.ext_frag - expectedExtFrag) < 0.001);

  // Total memory conservation invariant: used + free = memory_size
  assert.strictEqual(frag.used + frag.total_free, cont.memory_size);
  assert.strictEqual(cont.memory_size, 1740);

  // Allocation failure cause
  const failEv = step4.events.find(e => e.type === 'AllocationFailed');
  assert.ok(failEv, 'Step 4 must contain AllocationFailed');
  assert.strictEqual(failEv.data.cause, 'FRAGMENTATION');
  assert.strictEqual(failEv.data.requested, 426);
  assert.strictEqual(failEv.data.total_free, 959);
  assert.strictEqual(failEv.data.largest_hole, 300);
});

// 2. AC-2 Sliding Compaction Invariants
test('AC-2 Sliding Compaction metrics match architecture', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/ac2_compaction.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);
  assert.strictEqual(res.ok, true);

  // Step 1: Compaction
  const step1 = res.trace.steps[1];
  const compEv = step1.events.find(e => e.type === 'CompactionCompleted');
  assert.ok(compEv);
  assert.strictEqual(compEv.data.blocks_moved, 7);
  assert.strictEqual(compEv.data.units_moved, 781);
  assert.strictEqual(compEv.data.holes_after, 1);
  assert.strictEqual(compEv.data.largest_after, 959);

  // Step 1 snapshot: exactly 1 hole remaining of size 959, ext_frag = 0
  const frag1 = step1.snapshot.contiguous.frag;
  assert.strictEqual(frag1.hole_count, 1);
  assert.strictEqual(frag1.ext_frag, 0.0);
  assert.strictEqual(frag1.largest_hole, 959);
  assert.strictEqual(frag1.total_free, 959);

  // Step 2: 426 placed at 781, remaining hole of 533
  const step2 = res.trace.steps[2];
  const allocEv = step2.events.find(e => e.type === 'BlockAllocated');
  assert.ok(allocEv);
  assert.strictEqual(allocEv.data.start, 781);
  assert.strictEqual(allocEv.data.size, 426);
  assert.strictEqual(allocEv.data.id, 'a4-retry');

  const frag2 = step2.snapshot.contiguous.frag;
  assert.strictEqual(frag2.total_free, 533);
  assert.strictEqual(frag2.largest_hole, 533);
  assert.strictEqual(frag2.hole_count, 1);
  assert.strictEqual(frag2.ext_frag, 0.0);
  assert.strictEqual(frag2.used, 1207);
});

// 3. AC-3 Paging Reference String Invariants
test('AC-3 Paging FIFO faults and hits invariant: hits + faults = references', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/ac3_paging_fifo.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);
  assert.strictEqual(res.ok, true);

  const summary = res.trace.summary.metrics;
  assert.strictEqual(summary.faults, 15);
  assert.strictEqual(summary.hits, 5);
  assert.strictEqual(summary.references, 20);
  assert.strictEqual(summary.hits + summary.faults, summary.references);
  assert.strictEqual(summary.fault_rate, 0.75);
  assert.strictEqual(summary.hit_rate, 0.25);
  assert.strictEqual(summary.compulsory_faults, 5); // 5 distinct pages: 7, 0, 1, 2, 3, 4 -> compulsory
  assert.strictEqual(summary.evictions, 12);
});

// 4. Color assignment consistency
test('Color assignment for system and process IDs', () => {
  assert.strictEqual(ContiguousRenderer.getPidColor('SYS'), 'var(--pid-sys)');
  assert.strictEqual(ContiguousRenderer.getPidColor('P1'), 'var(--pid-p1)');
  assert.strictEqual(ContiguousRenderer.getPidColor('P2'), 'var(--pid-p2)');
  assert.strictEqual(ContiguousRenderer.getPidColor('P3'), 'var(--pid-p3)');
  assert.strictEqual(ContiguousRenderer.getPidColor('P4'), 'var(--pid-p4)');

  const dynColor = ContiguousRenderer.getPidColor('PROC_99');
  assert.ok(dynColor.startsWith('hsl('), 'Dynamic PID should yield deterministic HSL color');
});

console.log(`\nMetrics Test Summary: ${passedTests} passed, ${failedTests} failed.`);
if (failedTests > 0) {
  process.exit(1);
}
