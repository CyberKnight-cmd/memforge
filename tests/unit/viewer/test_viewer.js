/**
 * MemForge P9 Trace Viewer Test Suite
 * Validates TraceParser, PlaybackEngine, metrics calculations,
 * boundary behaviors, step capping, and error handling.
 */

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const {
  TraceParser,
  PlaybackEngine,
  MAX_VIEWER_STEPS,
  SUPPORTED_SCHEMA_VERSION
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

console.log('=== Running MemForge P9 Viewer Test Suite ===\n');

// 1. Valid Contiguous Traces
test('Parse valid contiguous trace (AC-1)', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/ac1_first_fit.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);

  assert.strictEqual(res.ok, true, 'Parsing should succeed');
  assert.strictEqual(res.trace.subsystem, 'contiguous');
  assert.strictEqual(res.trace.header.workload, 'silberschatz_5_holes');
  assert.strictEqual(res.trace.header.policy, 'first-fit');
  assert.strictEqual(res.trace.totalSteps, 5); // steps 0, 1, 2, 3, 4
  assert.strictEqual(res.trace.summary !== null, true, 'Summary should be present');
  assert.strictEqual(res.trace.isCapped, false);
});

// 2. Valid Paging Traces
test('Parse valid paging trace (AC-3)', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/ac3_paging_fifo.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);

  assert.strictEqual(res.ok, true, 'Parsing should succeed');
  assert.strictEqual(res.trace.subsystem, 'paging');
  assert.strictEqual(res.trace.header.policy, 'fifo');
  assert.strictEqual(res.trace.totalSteps, 21); // steps 0 to 20
  assert.strictEqual(res.trace.summary.metrics.faults, 15);
  assert.strictEqual(res.trace.summary.metrics.hits, 5);
});

// 3. Invalid JSON
test('Reject invalid JSON with exact line number', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/invalid_syntax.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);

  assert.strictEqual(res.ok, false, 'Should fail on invalid JSON');
  assert.ok(res.error.includes('line 3'), 'Error should specify line 3');
});

// 4. Missing TraceHeader
test('Reject trace missing TraceHeader', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/invalid_missing_header.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);

  assert.strictEqual(res.ok, false, 'Should fail when header is missing');
  assert.ok(res.error.includes('Missing TraceHeader'), 'Error should mention missing TraceHeader');
});

// 5. Unsupported Schema Version
test('Reject unsupported schema version', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/invalid_unsupported_schema.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);

  assert.strictEqual(res.ok, false, 'Should fail on unsupported schema');
  assert.ok(res.error.includes('Unsupported schema version: 999'), 'Error should cite schema version');
});

// 6. Empty Files
test('Reject empty or whitespace trace file', () => {
  const emptyRes = TraceParser.parseTrace('');
  assert.strictEqual(emptyRes.ok, false);
  assert.ok(emptyRes.error.includes('Empty trace file'));

  const whitespaceRes = TraceParser.parseTrace('   \n  \n  ');
  assert.strictEqual(whitespaceRes.ok, false);
});

// 7. Single-Step Trace
test('Handle single-step trace gracefully', () => {
  const singleStep = `{"type":"TraceHeader","schema":1,"engine":"0.1.0","git":"test","workload":"single","policy":"first-fit","config":{"memory_size":100},"trace_level":1}
{"seq":0,"step":0,"type":"Snapshot","step":0,"contiguous":{"memory_size":100,"blocks":[{"start":0,"size":100,"state":"free"}],"frag":{"total_free":100,"largest_hole":100,"hole_count":1,"ext_frag":0.0}}}
{"type":"RunSummary","metrics":{"alloc_requests":0}}`;
  const res = TraceParser.parseTrace(singleStep);

  assert.strictEqual(res.ok, true);
  assert.strictEqual(res.trace.totalSteps, 1);
  assert.strictEqual(res.trace.steps[0].stepIndex, 0);
});

// 8. Missing Optional Fields
test('Handle missing optional fields without crashing', () => {
  const missingFields = `{"type":"TraceHeader","schema":1,"engine":"0.1.0","workload":"minimal","policy":"first-fit","config":{}}
{"seq":0,"step":0,"type":"Snapshot","step":0,"contiguous":{"blocks":[]}}
{"seq":1,"step":1,"type":"BlockAllocated","data":{}}
{"type":"RunSummary","metrics":{}}`;
  const res = TraceParser.parseTrace(missingFields);

  assert.strictEqual(res.ok, true);
  assert.strictEqual(res.trace.totalSteps, 2);
  const step0 = res.trace.steps[0];
  assert.deepStrictEqual(step0.snapshot.contiguous.blocks, []);
});

// 9. Playback at Boundaries and Determinism
test('Playback engine boundaries and deterministic stepping', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/ac1_first_fit.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);
  assert.strictEqual(res.ok, true);

  const engine = new PlaybackEngine();
  engine.loadTrace(res.trace);

  assert.strictEqual(engine.currentStepIndex, 0);

  // Stepping backward at start is a no-op
  engine.prevStep();
  assert.strictEqual(engine.currentStepIndex, 0, 'Cannot step before 0');

  // Step forward sequentially
  engine.nextStep();
  assert.strictEqual(engine.currentStepIndex, 1);
  engine.nextStep();
  assert.strictEqual(engine.currentStepIndex, 2);
  engine.nextStep();
  assert.strictEqual(engine.currentStepIndex, 3);
  engine.nextStep();
  assert.strictEqual(engine.currentStepIndex, 4);

  // Stepping forward past last step is a no-op
  engine.nextStep();
  assert.strictEqual(engine.currentStepIndex, 4, 'Cannot step past last step');

  // Reset
  engine.reset();
  assert.strictEqual(engine.currentStepIndex, 0, 'Reset returns to 0');
});

// 10. Jump to Next Failure / Fault
test('Jump to next failure / fault finds AllocationFailed in AC-1', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/ac1_first_fit.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);
  const engine = new PlaybackEngine();
  engine.loadTrace(res.trace);

  assert.strictEqual(engine.currentStepIndex, 0);
  const targetStep = engine.jumpToNextFailureOrFault();
  assert.strictEqual(targetStep, 4, 'Should jump directly to step 4 (AllocationFailed)');
  assert.strictEqual(engine.currentStepIndex, 4);
});

// 11. Viewer Step Cap (5,000 steps limit)
test('Enforce 5,000-step viewer cap and set warning on oversized trace', () => {
  const lines = [
    JSON.stringify({
      type: 'TraceHeader',
      schema: 1,
      engine: '0.1.0',
      workload: 'massive',
      policy: 'first-fit',
      config: { memory_size: 1000 }
    })
  ];

  // Generate 5,005 steps
  for (let s = 0; s < 5005; s++) {
    lines.push(JSON.stringify({
      seq: s,
      step: s,
      type: 'Snapshot',
      contiguous: { memory_size: 1000, blocks: [{ start: 0, size: 1000, state: 'free' }], frag: {} }
    }));
  }
  lines.push(JSON.stringify({ type: 'RunSummary', metrics: { total_steps: 5005 } }));

  const rawTrace = lines.join('\n');
  const res = TraceParser.parseTrace(rawTrace);

  assert.strictEqual(res.ok, true);
  assert.strictEqual(res.trace.isCapped, true, 'isCapped should be true');
  assert.strictEqual(res.trace.totalStepsInFile, 5005);
  assert.strictEqual(res.trace.totalSteps, 5000, 'Viewer steps must be capped at 5000');
  assert.ok(
    res.trace.warnings.some(w => w.includes('5,000-step viewer limit')),
    'Warning must explicitly notify user of the cap'
  );
});

// 12. Incomplete Trace Detection
test('Detect incomplete trace when RunSummary is missing', () => {
  const filePath = path.join(__dirname, '../../../viewer/traces/incomplete_trace.jsonl');
  const content = fs.readFileSync(filePath, 'utf8');
  const res = TraceParser.parseTrace(content);

  assert.strictEqual(res.ok, true);
  assert.strictEqual(res.trace.summary, null);
  assert.ok(
    res.trace.warnings.some(w => w.includes('incomplete (missing RunSummary record)')),
    'Warning must flag missing RunSummary'
  );
});

console.log(`\nTest Summary: ${passedTests} passed, ${failedTests} failed.`);
if (failedTests > 0) {
  process.exit(1);
}
