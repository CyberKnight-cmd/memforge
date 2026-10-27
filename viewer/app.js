/**
 * MemForge Trace Viewer (P9)
 * Static HTML/JavaScript Trace Replay & Visualization Interface
 * Strictly follows docs/ARCHITECTURE.md specification
 */

(function (global) {
  'use strict';

  // Constants & Limits
  const MAX_VIEWER_STEPS = 5000;
  const SUPPORTED_SCHEMA_VERSION = 1;

  /**
   * TraceParser: Validates and parses MemForge JSONL trace files.
   */
  class TraceParser {
    static parseTrace(jsonlContent) {
      if (!jsonlContent || typeof jsonlContent !== 'string') {
        return { ok: false, error: 'Empty trace file. Please provide a valid MemForge JSONL trace.' };
      }

      const lines = jsonlContent.split(/\r?\n/);
      const parsedRecords = [];
      let lineNumber = 0;

      for (let i = 0; i < lines.length; i++) {
        lineNumber = i + 1;
        const line = lines[i].trim();
        if (!line || line.startsWith('#')) {
          continue; // skip empty lines and comments
        }

        try {
          const record = JSON.parse(line);
          parsedRecords.push({ lineNum: lineNumber, record });
        } catch (err) {
          return {
            ok: false,
            error: `Malformed JSON on line ${lineNumber}: ${err.message}`
          };
        }
      }

      if (parsedRecords.length === 0) {
        return { ok: false, error: 'Empty trace file. No valid JSON records found.' };
      }

      // First record MUST be TraceHeader
      const firstEntry = parsedRecords[0];
      const header = firstEntry.record;
      if (!header || header.type !== 'TraceHeader') {
        return {
          ok: false,
          error: `Missing TraceHeader record on line ${firstEntry.lineNum}. Traces must begin with a TraceHeader.`
        };
      }

      if (header.schema !== SUPPORTED_SCHEMA_VERSION) {
        return {
          ok: false,
          error: `Unsupported schema version: ${header.schema} on line ${firstEntry.lineNum} (expected schema ${SUPPORTED_SCHEMA_VERSION}).`
        };
      }

      // Process subsequent records
      const events = [];
      const snapshotsByStep = new Map();
      let summary = null;
      let warnings = [];

      for (let i = 1; i < parsedRecords.length; i++) {
        const { lineNum, record } = parsedRecords[i];
        if (!record || typeof record !== 'object') {
          return {
            ok: false,
            error: `Invalid record structure on line ${lineNum}: record must be an object.`
          };
        }

        const type = record.type;
        if (type === 'Snapshot') {
          const step = typeof record.step === 'number' ? record.step : 0;
          snapshotsByStep.set(step, record);
        } else if (type === 'RunSummary') {
          summary = record;
        } else {
          // Assume event record
          if (typeof record.seq !== 'number') {
            record.seq = events.length;
          }
          if (typeof record.step !== 'number') {
            record.step = 0;
          }
          events.push(record);
        }
      }

      // Check completeness
      if (!summary) {
        warnings.push('Trace is incomplete (missing RunSummary record). Replaying available steps.');
      }

      // Determine subsystem
      let subsystem = 'contiguous';
      if (header.config && (header.config.page_size !== undefined || header.config.frames !== undefined)) {
        subsystem = 'paging';
      } else if (header.config && header.config.memory_size !== undefined) {
        subsystem = 'contiguous';
      } else {
        // Fallback: inspect first snapshot
        for (const snap of snapshotsByStep.values()) {
          if (snap.paging) {
            subsystem = 'paging';
            break;
          }
          if (snap.contiguous) {
            subsystem = 'contiguous';
            break;
          }
        }
      }

      // Aggregate steps
      const stepIndexSet = new Set();
      for (const step of snapshotsByStep.keys()) {
        stepIndexSet.add(step);
      }
      for (const ev of events) {
        stepIndexSet.add(ev.step);
      }

      let stepNumbers = Array.from(stepIndexSet).sort((a, b) => a - b);
      if (stepNumbers.length === 0) {
        stepNumbers = [0];
      }

      const totalStepsInFile = stepNumbers.length;
      let isCapped = false;

      // Enforce 5,000-step viewer cap
      if (totalStepsInFile > MAX_VIEWER_STEPS) {
        isCapped = true;
        warnings.push(
          `Trace exceeds the ${MAX_VIEWER_STEPS.toLocaleString()}-step viewer limit (found ${totalStepsInFile.toLocaleString()} steps). Capped at first ${MAX_VIEWER_STEPS.toLocaleString()} steps. Later steps are omitted.`
        );
        stepNumbers = stepNumbers.slice(0, MAX_VIEWER_STEPS);
      }

      // Group events by step
      const eventsByStep = new Map();
      for (const s of stepNumbers) {
        eventsByStep.set(s, []);
      }
      for (const ev of events) {
        if (eventsByStep.has(ev.step)) {
          eventsByStep.get(ev.step).push(ev);
        }
      }

      // Build consolidated Step items
      let lastKnownSnapshot = null;
      const steps = stepNumbers.map((stepNum, idx) => {
        let snap = snapshotsByStep.get(stepNum);
        if (snap) {
          lastKnownSnapshot = snap;
        } else if (lastKnownSnapshot) {
          snap = lastKnownSnapshot; // Carry forward previous state if step snapshot was omitted
        } else {
          // Empty initial snapshot fallback
          snap = {
            type: 'Snapshot',
            step: stepNum,
            [subsystem]: subsystem === 'contiguous'
              ? { memory_size: header.config?.memory_size || 1000, blocks: [], frag: {} }
              : { frames: [], page_tables: {}, policy_state: {} }
          };
        }

        const stepEvents = eventsByStep.get(stepNum) || [];
        return {
          stepNumber: stepNum,
          stepIndex: idx,
          snapshot: snap,
          events: stepEvents,
          subsystem
        };
      });

      return {
        ok: true,
        trace: {
          header,
          summary,
          subsystem,
          steps,
          warnings,
          isCapped,
          totalStepsInFile,
          totalSteps: steps.length
        }
      };
    }
  }

  /**
   * PlaybackEngine: Manages deterministic trace playback and navigation.
   */
  class PlaybackEngine {
    constructor() {
      this.trace = null;
      this.currentStepIndex = 0;
      this.isPlaying = false;
      this.playbackSpeedMs = 500;
      this.timerId = null;
      this.listeners = new Set();
    }

    loadTrace(traceData) {
      this.pause();
      this.trace = traceData;
      this.currentStepIndex = 0;
      this.notifyListeners('load');
      this.notifyListeners('step');
    }

    addListener(listenerFn) {
      this.listeners.add(listenerFn);
      return () => this.listeners.delete(listenerFn);
    }

    notifyListeners(eventType) {
      for (const fn of this.listeners) {
        try {
          fn(eventType, this);
        } catch (e) {
          console.error('Error in playback listener:', e);
        }
      }
    }

    get currentStep() {
      if (!this.trace || !this.trace.steps || this.trace.steps.length === 0) {
        return null;
      }
      return this.trace.steps[this.currentStepIndex] || null;
    }

    get totalSteps() {
      return this.trace && this.trace.steps ? this.trace.steps.length : 0;
    }

    goToStep(index) {
      if (!this.trace || this.totalSteps === 0) return;
      const target = Math.max(0, Math.min(index, this.totalSteps - 1));
      if (this.currentStepIndex !== target) {
        this.currentStepIndex = target;
        this.notifyListeners('step');
      }
    }

    nextStep() {
      if (!this.trace || this.totalSteps === 0) return;
      if (this.currentStepIndex < this.totalSteps - 1) {
        this.currentStepIndex++;
        this.notifyListeners('step');
      } else if (this.isPlaying) {
        this.pause();
      }
    }

    prevStep() {
      if (!this.trace || this.totalSteps === 0) return;
      if (this.currentStepIndex > 0) {
        this.currentStepIndex--;
        this.notifyListeners('step');
      }
    }

    reset() {
      this.pause();
      this.goToStep(0);
    }

    play() {
      if (this.isPlaying || !this.trace || this.totalSteps === 0) return;
      if (this.currentStepIndex >= this.totalSteps - 1) {
        this.currentStepIndex = 0;
        this.notifyListeners('step');
      }
      this.isPlaying = true;
      this.notifyListeners('playstate');
      this.timerId = setInterval(() => {
        if (this.currentStepIndex < this.totalSteps - 1) {
          this.nextStep();
        } else {
          this.pause();
        }
      }, this.playbackSpeedMs);
    }

    pause() {
      if (!this.isPlaying && !this.timerId) return;
      this.isPlaying = false;
      if (this.timerId) {
        clearInterval(this.timerId);
        this.timerId = null;
      }
      this.notifyListeners('playstate');
    }

    togglePlay() {
      if (this.isPlaying) {
        this.pause();
      } else {
        this.play();
      }
    }

    setSpeed(speedMs) {
      this.playbackSpeedMs = Math.max(50, speedMs);
      if (this.isPlaying) {
        this.pause();
        this.play();
      }
    }

    jumpToNextFailureOrFault() {
      if (!this.trace || this.totalSteps === 0) return -1;
      const steps = this.trace.steps;
      
      // Look forward from next step
      for (let i = this.currentStepIndex + 1; i < steps.length; i++) {
        if (this.stepHasFailureOrFault(steps[i])) {
          this.goToStep(i);
          return i;
        }
      }

      // If not found forward, wrap around from start
      for (let i = 0; i <= this.currentStepIndex; i++) {
        if (this.stepHasFailureOrFault(steps[i])) {
          this.goToStep(i);
          return i;
        }
      }

      return -1;
    }

    stepHasFailureOrFault(step) {
      if (!step || !step.events) return false;
      return step.events.some(ev => 
        ev.type === 'AllocationFailed' ||
        ev.type === 'PageFault' ||
        ev.type === 'AllocRejected' ||
        ev.type === 'FreeRejected' ||
        ev.type === 'AccessViolation'
      );
    }
  }

  /**
   * ContiguousRenderer: Renders the contiguous memory bar, blocks, and ruler.
   */
  class ContiguousRenderer {
    static getPidColor(pid) {
      if (!pid) return '#334155';
      const pidStr = String(pid).toUpperCase();
      if (pidStr === 'SYS') return 'var(--pid-sys)';
      if (pidStr === 'P0' || pidStr === '0') return 'var(--pid-p0)';
      if (pidStr === 'P1' || pidStr === '1') return 'var(--pid-p1)';
      if (pidStr === 'P2' || pidStr === '2') return 'var(--pid-p2)';
      if (pidStr === 'P3' || pidStr === '3') return 'var(--pid-p3)';
      if (pidStr === 'P4' || pidStr === '4') return 'var(--pid-p4)';

      // Deterministic HSL color for any dynamic PID
      let hash = 0;
      for (let i = 0; i < pidStr.length; i++) {
        hash = (hash << 5) - hash + pidStr.charCodeAt(i);
        hash |= 0;
      }
      const hue = Math.abs(hash) % 360;
      return `hsl(${hue}, 75%, 45%)`;
    }

    static renderMemoryMap(container, step, onSelectBlock) {
      container.innerHTML = '';
      if (!step || !step.snapshot || !step.snapshot.contiguous) {
        container.innerHTML = '<div style="color:var(--text-muted);padding:1rem;">No contiguous memory snapshot available for this step.</div>';
        return;
      }

      const cont = step.snapshot.contiguous;
      const memSize = cont.memory_size || 1000;
      const blocks = cont.blocks || [];

      // Check current step events for relocated or allocated blocks
      const stepEvents = step.events || [];
      const relocatedIds = new Set();
      const allocatedIds = new Set();
      for (const ev of stepEvents) {
        if (ev.type === 'BlockRelocated' && ev.data && ev.data.id) {
          relocatedIds.add(String(ev.data.id));
        }
        if (ev.type === 'BlockAllocated' && ev.data && ev.data.id) {
          allocatedIds.add(String(ev.data.id));
        }
      }

      // Memory Bar
      const bar = document.createElement('div');
      bar.className = 'memory-bar';
      bar.setAttribute('role', 'region');
      bar.setAttribute('aria-label', `Memory Map 0 to ${memSize}`);

      blocks.forEach((block, idx) => {
        const blockEl = document.createElement('div');
        blockEl.className = 'mem-block';
        const isFree = block.state === 'free';

        // Calculate proportional width
        const pct = Math.max(0, (block.size / memSize) * 100);
        blockEl.style.width = `${pct}%`;
        blockEl.style.flexShrink = '0';

        if (isFree) {
          blockEl.classList.add('mem-block-free');
          blockEl.innerHTML = `
            <div class="block-label-id">FREE</div>
            <div class="block-label-size">${block.size}</div>
          `;
          blockEl.title = `Free Hole [${block.start}, ${block.start + block.size}) · Size: ${block.size}`;
        } else {
          blockEl.classList.add('mem-block-allocated');
          const color = ContiguousRenderer.getPidColor(block.pid);
          blockEl.style.backgroundColor = color;
          const blockId = block.id ? `${block.pid}:${block.id}` : (block.pid || 'USED');
          blockEl.innerHTML = `
            <div class="block-label-id">${blockId}</div>
            <div class="block-label-size">${block.size}</div>
          `;
          blockEl.title = `${blockId} [${block.start}, ${block.start + block.size}) · Size: ${block.size}${block.requested ? ` (req: ${block.requested})` : ''}`;

          if (relocatedIds.has(String(block.id)) || allocatedIds.has(String(block.id))) {
            blockEl.classList.add('highlight-step');
          }
        }

        blockEl.addEventListener('click', () => {
          // Deselect other blocks
          const prev = bar.querySelector('.mem-block.selected');
          if (prev) prev.classList.remove('selected');
          blockEl.classList.add('selected');
          if (onSelectBlock) {
            onSelectBlock(block, memSize);
          }
        });

        bar.appendChild(blockEl);
      });

      container.appendChild(bar);
    }
  }

  /**
   * PagingRenderer: Renders the frame table, page tables, and policy state.
   */
  class PagingRenderer {
    static renderPaging(container, step) {
      container.innerHTML = '';
      if (!step || !step.snapshot || !step.snapshot.paging) {
        container.innerHTML = '<div style="color:var(--text-muted);padding:1rem;">No paging snapshot available for this step.</div>';
        return;
      }

      const pag = step.snapshot.paging;
      const frames = pag.frames || [];
      const pageTables = pag.page_tables || {};
      const policyState = pag.policy_state || {};

      // Check for accessed frame in current step
      const stepEvents = step.events || [];
      let accessedFrame = null;
      let accessKind = null;
      for (const ev of stepEvents) {
        if (ev.type === 'PageHit' && ev.data && ev.data.frame !== undefined) {
          accessedFrame = ev.data.frame;
          accessKind = 'hit';
        } else if (ev.type === 'PageLoaded' && ev.data && ev.data.frame !== undefined) {
          accessedFrame = ev.data.frame;
          accessKind = 'loaded';
        } else if (ev.type === 'PageEvicted' && ev.data && ev.data.frame !== undefined) {
          accessedFrame = ev.data.frame;
          accessKind = 'evicted';
        }
      }

      const wrapper = document.createElement('div');
      wrapper.className = 'paging-container';

      // 1. Physical Frame Table Section
      const frameSection = document.createElement('div');
      frameSection.innerHTML = '<div class="panel-subtitle" style="margin-bottom:0.75rem;font-weight:700;">PHYSICAL FRAMES</div>';
      
      const frameGrid = document.createElement('div');
      frameGrid.className = 'frame-grid';

      frames.forEach((f) => {
        const frameCard = document.createElement('div');
        frameCard.className = 'frame-card';
        const isOccupied = f.pid !== null && f.pid !== undefined && f.vpn !== null && f.vpn !== undefined;

        if (f.frame === accessedFrame) {
          frameCard.classList.add('active-access');
          if (accessKind === 'evicted') frameCard.classList.add('evicted');
        }

        frameCard.innerHTML = `
          <div class="frame-header">
            <span class="frame-num">F${f.frame}</span>
            <span class="frame-status-dot ${isOccupied ? 'occupied' : ''}"></span>
          </div>
          <div class="frame-body">
            <div class="frame-page-id">${isOccupied ? `${f.pid}:p${f.vpn}` : '<span style="color:var(--text-muted);font-weight:400;">[Free]</span>'}</div>
            <div class="frame-bits">
              <span class="bit-badge ${f.referenced ? 'active' : ''}">R:${f.referenced ? '1' : '0'}</span>
              <span class="bit-badge ${f.dirty ? 'dirty' : ''}">D:${f.dirty ? '1' : '0'}</span>
            </div>
          </div>
        `;
        frameGrid.appendChild(frameCard);
      });

      frameSection.appendChild(frameGrid);
      wrapper.appendChild(frameSection);

      // 2. Policy State & Page Tables Row
      const lowerRow = document.createElement('div');
      lowerRow.style.display = 'grid';
      lowerRow.style.gridTemplateColumns = 'repeat(auto-fit, minmax(260px, 1fr))';
      lowerRow.style.gap = '1.25rem';

      // Policy State
      const policyBox = document.createElement('div');
      policyBox.className = 'policy-state-box';
      const polName = (policyState.policy || 'Replacement Policy').toUpperCase();
      let policyDetailsHtml = '';

      if (policyState.queue && Array.isArray(policyState.queue)) {
        policyDetailsHtml = `
          <div style="font-size:0.75rem;color:var(--text-muted);">Queue (Load Order):</div>
          <div class="queue-chips">
            ${policyState.queue.length > 0
              ? policyState.queue.map(q => `<span class="queue-chip">Frame ${q}</span>`).join('')
              : '<span style="font-size:0.75rem;color:var(--text-muted);">Empty</span>'
            }
          </div>
        `;
      } else if (policyState.order && Array.isArray(policyState.order)) {
        policyDetailsHtml = `
          <div style="font-size:0.75rem;color:var(--text-muted);">Order (LRU to MRU):</div>
          <div class="queue-chips">
            ${policyState.order.map(q => `<span class="queue-chip">${q}</span>`).join('')}
          </div>
        `;
      } else {
        policyDetailsHtml = `<div style="font-size:0.75rem;color:var(--text-muted);">${JSON.stringify(policyState)}</div>`;
      }

      policyBox.innerHTML = `
        <div style="font-size:0.75rem;font-weight:700;color:var(--text-muted);letter-spacing:0.05em;">POLICY STATE (${polName})</div>
        ${policyDetailsHtml}
      `;
      lowerRow.appendChild(policyBox);

      // Process Page Tables
      const ptWrapper = document.createElement('div');
      ptWrapper.className = 'page-table-wrapper';
      const pids = Object.keys(pageTables);

      if (pids.length === 0) {
        ptWrapper.innerHTML = '<div style="color:var(--text-muted);font-size:0.75rem;">No active page tables.</div>';
      } else {
        let ptHtml = '<div style="font-size:0.75rem;font-weight:700;color:var(--text-muted);margin-bottom:0.5rem;letter-spacing:0.05em;">PAGE TABLES</div>';
        pids.forEach(pid => {
          const entries = pageTables[pid] || [];
          const residentEntries = entries.filter(e => e.present);
          ptHtml += `
            <div style="margin-bottom:0.5rem;font-size:0.75rem;"><strong>Process ${pid}</strong> (${residentEntries.length} resident)</div>
            <table class="data-table">
              <thead>
                <tr>
                  <th>VPN</th>
                  <th>Frame</th>
                  <th>P</th>
                  <th>R</th>
                  <th>D</th>
                </tr>
              </thead>
              <tbody>
                ${entries.map(e => `
                  <tr>
                    <td>vpn ${e.vpn}</td>
                    <td>${e.frame !== null && e.frame !== undefined ? `F${e.frame}` : '—'}</td>
                    <td>${e.present ? '✓' : '✗'}</td>
                    <td>${e.referenced ? '1' : '0'}</td>
                    <td>${e.dirty ? '1' : '0'}</td>
                  </tr>
                `).join('')}
              </tbody>
            </table>
          `;
        });
        ptWrapper.innerHTML = ptHtml;
      }

      lowerRow.appendChild(ptWrapper);
      wrapper.appendChild(lowerRow);

      container.appendChild(wrapper);
    }
  }

  /**
   * MetricsRenderer: Displays live metric cards and timeline sparklines.
   */
  class MetricsRenderer {
    static renderMetrics(container, step, traceSession) {
      container.innerHTML = '';
      if (!step || !step.snapshot) return;

      const sub = step.subsystem;
      const cardsWrapper = document.createElement('div');
      cardsWrapper.className = 'metric-card-group';

      if (sub === 'contiguous') {
        const cont = step.snapshot.contiguous || {};
        const frag = cont.frag || {};
        const memSize = cont.memory_size || 0;
        const totalFree = frag.total_free !== undefined ? frag.total_free : 0;
        const largestHole = frag.largest_hole !== undefined ? frag.largest_hole : 0;
        const holeCount = frag.hole_count !== undefined ? frag.hole_count : 0;
        const used = frag.used !== undefined ? frag.used : (memSize - totalFree);
        const extFrag = frag.ext_frag !== undefined ? frag.ext_frag : 0;
        const utilPct = memSize > 0 ? ((used / memSize) * 100).toFixed(1) : '0.0';

        // Ext Frag Color
        let extFragColor = 'var(--accent-green)';
        if (extFrag > 0.5) extFragColor = 'var(--accent-red)';
        else if (extFrag > 0.2) extFragColor = 'var(--accent-amber)';

        cardsWrapper.innerHTML = `
          <div class="metric-card">
            <span class="metric-title">External Frag</span>
            <span class="metric-number" style="color:${extFragColor};">${(extFrag * 100).toFixed(1)}%</span>
            <span class="metric-sub">${extFrag.toFixed(4)}</span>
            <div class="metric-gauge"><div class="gauge-fill" style="width:${Math.min(100, extFrag * 100)}%;background:${extFragColor};"></div></div>
          </div>
          <div class="metric-card">
            <span class="metric-title">Utilization</span>
            <span class="metric-number">${utilPct}%</span>
            <span class="metric-sub">${used} / ${memSize} units</span>
            <div class="metric-gauge"><div class="gauge-fill" style="width:${utilPct}%;background:var(--accent-cyan);"></div></div>
          </div>
          <div class="metric-card">
            <span class="metric-title">Total Free</span>
            <span class="metric-number">${totalFree.toLocaleString()}</span>
            <span class="metric-sub">${holeCount} holes</span>
          </div>
          <div class="metric-card">
            <span class="metric-title">Largest Hole</span>
            <span class="metric-number">${largestHole.toLocaleString()}</span>
            <span class="metric-sub">${largestHole >= 0 ? `${((largestHole / (totalFree || 1)) * 100).toFixed(0)}% of free` : 'None'}</span>
          </div>
        `;
      } else {
        // Paging Subsystem Metrics
        // Count hits, faults up to current step
        const allSteps = traceSession.steps || [];
        let hits = 0;
        let faults = 0;
        let evictions = 0;
        let writebacks = 0;

        for (let i = 0; i <= step.stepIndex; i++) {
          const evs = allSteps[i]?.events || [];
          for (const ev of evs) {
            if (ev.type === 'PageHit') hits++;
            if (ev.type === 'PageFault') faults++;
            if (ev.type === 'PageEvicted') evictions++;
            if (ev.type === 'PageWrittenBack') writebacks++;
          }
        }

        const totalRefs = hits + faults;
        const faultRate = totalRefs > 0 ? ((faults / totalRefs) * 100).toFixed(1) : '0.0';
        const hitRate = totalRefs > 0 ? ((hits / totalRefs) * 100).toFixed(1) : '0.0';

        cardsWrapper.innerHTML = `
          <div class="metric-card">
            <span class="metric-title">Fault Rate</span>
            <span class="metric-number" style="color:var(--accent-amber);">${faultRate}%</span>
            <span class="metric-sub">${faults} of ${totalRefs} refs</span>
            <div class="metric-gauge"><div class="gauge-fill" style="width:${faultRate}%;background:var(--accent-amber);"></div></div>
          </div>
          <div class="metric-card">
            <span class="metric-title">Hit Rate</span>
            <span class="metric-number" style="color:var(--accent-green);">${hitRate}%</span>
            <span class="metric-sub">${hits} hits</span>
            <div class="metric-gauge"><div class="gauge-fill" style="width:${hitRate}%;background:var(--accent-green);"></div></div>
          </div>
          <div class="metric-card">
            <span class="metric-title">Evictions</span>
            <span class="metric-number">${evictions}</span>
            <span class="metric-sub">${writebacks} write-backs</span>
          </div>
          <div class="metric-card">
            <span class="metric-title">References</span>
            <span class="metric-number">${totalRefs}</span>
            <span class="metric-sub">Step ${step.stepIndex + 1} of ${traceSession.totalSteps}</span>
          </div>
        `;
      }

      container.appendChild(cardsWrapper);

      // Render Step Trend Sparkline
      const trendCard = document.createElement('div');
      trendCard.className = 'trend-card';
      trendCard.innerHTML = `
        <div style="font-size:0.7rem;font-weight:700;color:var(--text-muted);text-transform:uppercase;letter-spacing:0.05em;">
          ${sub === 'contiguous' ? 'External Fragmentation Timeline' : 'Cumulative Page Faults'}
        </div>
      `;

      const sparklineSvg = MetricsRenderer.createSparklineSvg(traceSession, step.stepIndex);
      trendCard.appendChild(sparklineSvg);
      container.appendChild(trendCard);
    }

    static createSparklineSvg(traceSession, currentStepIdx) {
      const steps = traceSession.steps || [];
      const n = steps.length;
      const width = 340;
      const height = 50;
      const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
      svg.setAttribute('viewBox', `0 0 ${width} ${height}`);
      svg.className = 'sparkline-svg';

      if (n <= 1) {
        svg.innerHTML = `<text x="50%" y="50%" fill="#64748b" font-size="10" text-anchor="middle" dominant-baseline="middle">Single step trace</text>`;
        return svg;
      }

      // Gather points
      const points = [];
      let maxVal = 0.001;

      if (traceSession.subsystem === 'contiguous') {
        for (let i = 0; i < n; i++) {
          const val = steps[i].snapshot?.contiguous?.frag?.ext_frag || 0;
          points.push(val);
          if (val > maxVal) maxVal = val;
        }
      } else {
        let cumulativeFaults = 0;
        for (let i = 0; i < n; i++) {
          const evs = steps[i].events || [];
          for (const ev of evs) {
            if (ev.type === 'PageFault') cumulativeFaults++;
          }
          points.push(cumulativeFaults);
          if (cumulativeFaults > maxVal) maxVal = cumulativeFaults;
        }
      }

      const pathData = points.map((val, idx) => {
        const x = (idx / (n - 1)) * width;
        const y = height - (val / maxVal) * (height - 10) - 5;
        return `${idx === 0 ? 'M' : 'L'} ${x.toFixed(1)} ${y.toFixed(1)}`;
      }).join(' ');

      const currentX = (currentStepIdx / (n - 1)) * width;
      const currentY = height - (points[currentStepIdx] / maxVal) * (height - 10) - 5;

      svg.innerHTML = `
        <path d="${pathData}" fill="none" stroke="#00d2ff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" />
        <line x1="${currentX.toFixed(1)}" y1="0" x2="${currentX.toFixed(1)}" y2="${height}" stroke="rgba(255,255,255,0.4)" stroke-dasharray="2,2" />
        <circle cx="${currentX.toFixed(1)}" cy="${currentY.toFixed(1)}" r="4" fill="#00d2ff" stroke="#ffffff" stroke-width="1.5" />
      `;

      return svg;
    }
  }

  /**
   * EventRenderer: Formats and displays event details and policy reasons.
   */
  class EventRenderer {
    static renderEvents(container, step) {
      container.innerHTML = '';
      if (!step) return;

      const events = step.events || [];
      const stepNum = step.stepNumber;

      // Determine step summary message & status
      let summaryTitle = `Step ${stepNum}`;
      let summaryDesc = 'No state changing events in this step.';
      let bannerClass = 'step-summary-banner';

      for (const ev of events) {
        if (ev.type === 'AllocationFailed') {
          summaryTitle = `Allocation Failed (${ev.data?.cause || 'REJECTED'})`;
          summaryDesc = `Process ${ev.data?.pid || ''} requested ${ev.data?.requested} units. Largest hole was ${ev.data?.largest_hole}, failing due to ${ev.data?.cause}.`;
          bannerClass += ' failure';
          break;
        } else if (ev.type === 'BlockAllocated') {
          summaryTitle = `Block Allocated (${ev.data?.pid}:${ev.data?.id})`;
          summaryDesc = `Placed ${ev.data?.size} units at address [${ev.data?.start}, ${ev.data?.start + ev.data?.size}).`;
          break;
        } else if (ev.type === 'CompactionCompleted') {
          summaryTitle = `Compaction Completed`;
          summaryDesc = `Relocated ${ev.data?.blocks_moved} blocks, moving ${ev.data?.units_moved} units into 1 contiguous hole.`;
          break;
        } else if (ev.type === 'PageFault') {
          summaryTitle = `Page Fault (${ev.data?.kind || 'FAULT'})`;
          summaryDesc = `Access to VPN ${ev.data?.vpn} by process ${ev.data?.pid} resulted in a ${ev.data?.kind} fault.`;
          bannerClass += ' failure';
          break;
        } else if (ev.type === 'PageHit') {
          summaryTitle = `Page Hit (Frame ${ev.data?.frame})`;
          summaryDesc = `Access to VPN ${ev.data?.vpn} hit in Frame ${ev.data?.frame}.`;
          bannerClass += ' hit';
          break;
        }
      }

      const summaryBanner = document.createElement('div');
      summaryBanner.className = bannerClass;
      summaryBanner.innerHTML = `
        <div class="step-summary-title">${summaryTitle}</div>
        <div class="step-summary-desc">${summaryDesc}</div>
      `;
      container.appendChild(summaryBanner);

      // Event List
      const listEl = document.createElement('div');
      listEl.className = 'events-list';

      if (events.length === 0) {
        listEl.innerHTML = '<div style="color:var(--text-muted);font-size:0.8rem;padding:0.5rem 0;">No individual event records for this step.</div>';
      } else {
        events.forEach(ev => {
          const item = document.createElement('div');
          item.className = 'event-item';
          const chipClass = `chip-${ev.type}`;

          const dataPairs = Object.entries(ev.data || {})
            .map(([k, v]) => `${k}: ${typeof v === 'object' && v !== null ? JSON.stringify(v) : v}`)
            .join(' · ');

          item.innerHTML = `
            <div class="event-item-header">
              <span class="event-chip ${chipClass}">${ev.type}</span>
              <span class="event-seq">seq ${ev.seq}</span>
            </div>
            <div class="event-params">${dataPairs || '—'}</div>
          `;
          listEl.appendChild(item);
        });
      }

      container.appendChild(listEl);
    }
  }

  /**
   * Sample Traces: Preloaded fixtures for AC-1, AC-2, AC-3, AC-4
   */
  const SAMPLE_TRACES = {
    ac1: `{"type":"TraceHeader","schema":1,"engine":"0.1.0","git":"3bcf43b","workload":"silberschatz_5_holes","policy":"first-fit","config":{"memory_size":1740,"alignment":1,"min_split":1,"compaction":"never"},"trace_level":1}
{"seq":0,"step":0,"type":"Snapshot","step":0,"contiguous":{"memory_size":1740,"blocks":[{"start":0,"size":100,"state":"free"},{"start":100,"size":10,"state":"allocated","pid":"SYS","id":"s1","requested":10},{"start":110,"size":500,"state":"free"},{"start":610,"size":10,"state":"allocated","pid":"SYS","id":"s2","requested":10},{"start":620,"size":200,"state":"free"},{"start":820,"size":10,"state":"allocated","pid":"SYS","id":"s3","requested":10},{"start":830,"size":300,"state":"free"},{"start":1130,"size":10,"state":"allocated","pid":"SYS","id":"s4","requested":10},{"start":1140,"size":600,"state":"free"}],"frag":{"total_free":1700,"largest_hole":600,"hole_count":5,"ext_frag":0.647059,"used":40,"live_blocks":4,"int_frag":0}}}
{"seq":1,"step":1,"type":"PlacementDecided","data":{"policy":"first-fit","requested":212,"granted":212,"hole":{"start":110,"size":500},"scanned":2}}
{"seq":2,"step":1,"type":"BlockSplit","data":{"hole":{"start":110,"size":500},"allocated":{"start":110,"size":212},"remainder":{"start":322,"size":288}}}
{"seq":3,"step":1,"type":"BlockAllocated","data":{"pid":"P1","id":"a1","start":110,"size":212,"requested":212}}
{"seq":4,"step":1,"type":"Snapshot","step":1,"contiguous":{"memory_size":1740,"blocks":[{"start":0,"size":100,"state":"free"},{"start":100,"size":10,"state":"allocated","pid":"SYS","id":"s1","requested":10},{"start":110,"size":212,"state":"allocated","pid":"P1","id":"a1","requested":212},{"start":322,"size":288,"state":"free"},{"start":610,"size":10,"state":"allocated","pid":"SYS","id":"s2","requested":10},{"start":620,"size":200,"state":"free"},{"start":820,"size":10,"state":"allocated","pid":"SYS","id":"s3","requested":10},{"start":830,"size":300,"state":"free"},{"start":1130,"size":10,"state":"allocated","pid":"SYS","id":"s4","requested":10},{"start":1140,"size":600,"state":"free"}],"frag":{"total_free":1488,"largest_hole":600,"hole_count":5,"ext_frag":0.596774,"used":252,"live_blocks":5,"int_frag":0}}}
{"seq":5,"step":2,"type":"PlacementDecided","data":{"policy":"first-fit","requested":417,"granted":417,"hole":{"start":1140,"size":600},"scanned":5}}
{"seq":6,"step":2,"type":"BlockSplit","data":{"hole":{"start":1140,"size":600},"allocated":{"start":1140,"size":417},"remainder":{"start":1557,"size":183}}}
{"seq":7,"step":2,"type":"BlockAllocated","data":{"pid":"P2","id":"a2","start":1140,"size":417,"requested":417}}
{"seq":8,"step":2,"type":"Snapshot","step":2,"contiguous":{"memory_size":1740,"blocks":[{"start":0,"size":100,"state":"free"},{"start":100,"size":10,"state":"allocated","pid":"SYS","id":"s1","requested":10},{"start":110,"size":212,"state":"allocated","pid":"P1","id":"a1","requested":212},{"start":322,"size":288,"state":"free"},{"start":610,"size":10,"state":"allocated","pid":"SYS","id":"s2","requested":10},{"start":620,"size":200,"state":"free"},{"start":820,"size":10,"state":"allocated","pid":"SYS","id":"s3","requested":10},{"start":830,"size":300,"state":"free"},{"start":1130,"size":10,"state":"allocated","pid":"SYS","id":"s4","requested":10},{"start":1140,"size":417,"state":"allocated","pid":"P2","id":"a2","requested":417},{"start":1557,"size":183,"state":"free"}],"frag":{"total_free":1071,"largest_hole":300,"hole_count":5,"ext_frag":0.719888,"used":669,"live_blocks":6,"int_frag":0}}}
{"seq":9,"step":3,"type":"PlacementDecided","data":{"policy":"first-fit","requested":112,"granted":112,"hole":{"start":322,"size":288},"scanned":2}}
{"seq":10,"step":3,"type":"BlockSplit","data":{"hole":{"start":322,"size":288},"allocated":{"start":322,"size":112},"remainder":{"start":434,"size":176}}}
{"seq":11,"step":3,"type":"BlockAllocated","data":{"pid":"P3","id":"a3","start":322,"size":112,"requested":112}}
{"seq":12,"step":3,"type":"Snapshot","step":3,"contiguous":{"memory_size":1740,"blocks":[{"start":0,"size":100,"state":"free"},{"start":100,"size":10,"state":"allocated","pid":"SYS","id":"s1","requested":10},{"start":110,"size":212,"state":"allocated","pid":"P1","id":"a1","requested":212},{"start":322,"size":112,"state":"allocated","pid":"P3","id":"a3","requested":112},{"start":434,"size":176,"state":"free"},{"start":610,"size":10,"state":"allocated","pid":"SYS","id":"s2","requested":10},{"start":620,"size":200,"state":"free"},{"start":820,"size":10,"state":"allocated","pid":"SYS","id":"s3","requested":10},{"start":830,"size":300,"state":"free"},{"start":1130,"size":10,"state":"allocated","pid":"SYS","id":"s4","requested":10},{"start":1140,"size":417,"state":"allocated","pid":"P2","id":"a2","requested":417},{"start":1557,"size":183,"state":"free"}],"frag":{"total_free":959,"largest_hole":300,"hole_count":5,"ext_frag":0.687174,"used":781,"live_blocks":7,"int_frag":0}}}
{"seq":13,"step":4,"type":"PlacementDecided","data":{"policy":"first-fit","requested":426,"granted":426,"hole":null,"scanned":5}}
{"seq":14,"step":4,"type":"AllocationFailed","data":{"pid":"P4","id":"a4","requested":426,"granted":426,"cause":"FRAGMENTATION","total_free":959,"largest_hole":300}}
{"seq":15,"step":4,"type":"Snapshot","step":4,"contiguous":{"memory_size":1740,"blocks":[{"start":0,"size":100,"state":"free"},{"start":100,"size":10,"state":"allocated","pid":"SYS","id":"s1","requested":10},{"start":110,"size":212,"state":"allocated","pid":"P1","id":"a1","requested":212},{"start":322,"size":112,"state":"allocated","pid":"P3","id":"a3","requested":112},{"start":434,"size":176,"state":"free"},{"start":610,"size":10,"state":"allocated","pid":"SYS","id":"s2","requested":10},{"start":620,"size":200,"state":"free"},{"start":820,"size":10,"state":"allocated","pid":"SYS","id":"s3","requested":10},{"start":830,"size":300,"state":"free"},{"start":1130,"size":10,"state":"allocated","pid":"SYS","id":"s4","requested":10},{"start":1140,"size":417,"state":"allocated","pid":"P2","id":"a2","requested":417},{"start":1557,"size":183,"state":"free"}],"frag":{"total_free":959,"largest_hole":300,"hole_count":5,"ext_frag":0.687174,"used":781,"live_blocks":7,"int_frag":0}}}
{"type":"RunSummary","metrics":{"alloc_requests":4,"alloc_failures":1,"failures_fragmentation":1,"failures_capacity":0,"ext_frag_final":0.687174,"hole_count_final":5,"total_free_final":959,"largest_hole_final":300,"used_final":781,"live_blocks_final":7}}`,

    ac2: `{"type":"TraceHeader","schema":1,"engine":"0.1.0","git":"3bcf43b","workload":"silberschatz_5_holes_compact","policy":"first-fit","config":{"memory_size":1740,"alignment":1,"min_split":1,"compaction":"on_failure"},"trace_level":1}
{"seq":0,"step":0,"type":"Snapshot","step":0,"contiguous":{"memory_size":1740,"blocks":[{"start":0,"size":100,"state":"free"},{"start":100,"size":10,"state":"allocated","pid":"SYS","id":"s1","requested":10},{"start":110,"size":212,"state":"allocated","pid":"P1","id":"a1","requested":212},{"start":322,"size":112,"state":"allocated","pid":"P3","id":"a3","requested":112},{"start":434,"size":176,"state":"free"},{"start":610,"size":10,"state":"allocated","pid":"SYS","id":"s2","requested":10},{"start":620,"size":200,"state":"free"},{"start":820,"size":10,"state":"allocated","pid":"SYS","id":"s3","requested":10},{"start":830,"size":300,"state":"free"},{"start":1130,"size":10,"state":"allocated","pid":"SYS","id":"s4","requested":10},{"start":1140,"size":417,"state":"allocated","pid":"P2","id":"a2","requested":417},{"start":1557,"size":183,"state":"free"}],"frag":{"total_free":959,"largest_hole":300,"hole_count":5,"ext_frag":0.687174,"used":781,"live_blocks":7,"int_frag":0}}}
{"seq":1,"step":1,"type":"CompactionStarted","data":{"trigger":"explicit","holes_before":5,"largest_before":300}}
{"seq":2,"step":1,"type":"BlockRelocated","data":{"id":"s1","pid":"SYS","src":100,"dst":0,"size":10}}
{"seq":3,"step":1,"type":"BlockRelocated","data":{"id":"a1","pid":"P1","src":110,"dst":10,"size":212}}
{"seq":4,"step":1,"type":"BlockRelocated","data":{"id":"a3","pid":"P3","src":322,"dst":222,"size":112}}
{"seq":5,"step":1,"type":"BlockRelocated","data":{"id":"s2","pid":"SYS","src":610,"dst":334,"size":10}}
{"seq":6,"step":1,"type":"BlockRelocated","data":{"id":"s3","pid":"SYS","src":820,"dst":344,"size":10}}
{"seq":7,"step":1,"type":"BlockRelocated","data":{"id":"s4","pid":"SYS","src":1130,"dst":354,"size":10}}
{"seq":8,"step":1,"type":"BlockRelocated","data":{"id":"a2","pid":"P2","src":1140,"dst":364,"size":417}}
{"seq":9,"step":1,"type":"CompactionCompleted","data":{"blocks_moved":7,"units_moved":781,"holes_after":1,"largest_after":959}}
{"seq":10,"step":1,"type":"Snapshot","step":1,"contiguous":{"memory_size":1740,"blocks":[{"start":0,"size":10,"state":"allocated","pid":"SYS","id":"s1","requested":10},{"start":10,"size":212,"state":"allocated","pid":"P1","id":"a1","requested":212},{"start":222,"size":112,"state":"allocated","pid":"P3","id":"a3","requested":112},{"start":334,"size":10,"state":"allocated","pid":"SYS","id":"s2","requested":10},{"start":344,"size":10,"state":"allocated","pid":"SYS","id":"s3","requested":10},{"start":354,"size":10,"state":"allocated","pid":"SYS","id":"s4","requested":10},{"start":364,"size":417,"state":"allocated","pid":"P2","id":"a2","requested":417},{"start":781,"size":959,"state":"free"}],"frag":{"total_free":959,"largest_hole":959,"hole_count":1,"ext_frag":0.0,"used":781,"live_blocks":7,"int_frag":0}}}
{"seq":11,"step":2,"type":"PlacementDecided","data":{"policy":"first-fit","requested":426,"granted":426,"hole":{"start":781,"size":959},"scanned":1}}
{"seq":12,"step":2,"type":"BlockSplit","data":{"hole":{"start":781,"size":959},"allocated":{"start":781,"size":426},"remainder":{"start":1207,"size":533}}}
{"seq":13,"step":2,"type":"BlockAllocated","data":{"pid":"P4","id":"a4-retry","start":781,"size":426,"requested":426}}
{"seq":14,"step":2,"type":"Snapshot","step":2,"contiguous":{"memory_size":1740,"blocks":[{"start":0,"size":10,"state":"allocated","pid":"SYS","id":"s1","requested":10},{"start":10,"size":212,"state":"allocated","pid":"P1","id":"a1","requested":212},{"start":222,"size":112,"state":"allocated","pid":"P3","id":"a3","requested":112},{"start":334,"size":10,"state":"allocated","pid":"SYS","id":"s2","requested":10},{"start":344,"size":10,"state":"allocated","pid":"SYS","id":"s3","requested":10},{"start":354,"size":10,"state":"allocated","pid":"SYS","id":"s4","requested":10},{"start":364,"size":417,"state":"allocated","pid":"P2","id":"a2","requested":417},{"start":781,"size":426,"state":"allocated","pid":"P4","id":"a4-retry","requested":426},{"start":1207,"size":533,"state":"free"}],"frag":{"total_free":533,"largest_hole":533,"hole_count":1,"ext_frag":0.0,"used":1207,"live_blocks":8,"int_frag":0}}}
{"type":"RunSummary","metrics":{"alloc_requests":1,"alloc_failures":0,"compactions":1,"blocks_moved":7,"units_moved":781,"ext_frag_final":0.0,"hole_count_final":1,"total_free_final":533,"largest_hole_final":533,"used_final":1207,"live_blocks_final":8}}`,

    ac3: `{"type":"TraceHeader","schema":1,"engine":"0.1.0","git":"3bcf43b","workload":"silberschatz_ac3_fifo","policy":"fifo","config":{"page_size":4096,"frames":3,"scope":"global"},"trace_level":1}
{"seq":0,"step":0,"type":"ProcessSpawned","data":{"pid":"P0","pages":8}}
{"seq":1,"step":0,"type":"Snapshot","step":0,"paging":{"frames":[{"frame":0,"pid":null,"vpn":null,"referenced":false,"dirty":false},{"frame":1,"pid":null,"vpn":null,"referenced":false,"dirty":false},{"frame":2,"pid":null,"vpn":null,"referenced":false,"dirty":false}],"page_tables":{"P0":[{"vpn":0,"frame":null,"present":false,"referenced":false,"dirty":false},{"vpn":7,"frame":null,"present":false,"referenced":false,"dirty":false}]},"policy_state":{"policy":"fifo","queue":[]}}}
{"seq":2,"step":1,"type":"PageFault","data":{"pid":"P0","vpn":7,"kind":"COMPULSORY","write":false}}
{"seq":3,"step":1,"type":"FrameAllocated","data":{"frame":0,"pid":"P0","vpn":7}}
{"seq":4,"step":1,"type":"PageLoaded","data":{"frame":0,"pid":"P0","vpn":7}}
{"seq":5,"step":1,"type":"Snapshot","step":1,"paging":{"frames":[{"frame":0,"pid":"P0","vpn":7,"referenced":true,"dirty":false},{"frame":1,"pid":null,"vpn":null,"referenced":false,"dirty":false},{"frame":2,"pid":null,"vpn":null,"referenced":false,"dirty":false}],"page_tables":{"P0":[{"vpn":7,"frame":0,"present":true,"referenced":true,"dirty":false}]},"policy_state":{"policy":"fifo","queue":[0]}}}
{"seq":6,"step":2,"type":"PageFault","data":{"pid":"P0","vpn":0,"kind":"COMPULSORY","write":false}}
{"seq":7,"step":2,"type":"FrameAllocated","data":{"frame":1,"pid":"P0","vpn":0}}
{"seq":8,"step":2,"type":"PageLoaded","data":{"frame":1,"pid":"P0","vpn":0}}
{"seq":9,"step":2,"type":"Snapshot","step":2,"paging":{"frames":[{"frame":0,"pid":"P0","vpn":7,"referenced":true,"dirty":false},{"frame":1,"pid":"P0","vpn":0,"referenced":true,"dirty":false},{"frame":2,"pid":null,"vpn":null,"referenced":false,"dirty":false}],"page_tables":{"P0":[{"vpn":0,"frame":1,"present":true,"referenced":true,"dirty":false},{"vpn":7,"frame":0,"present":true,"referenced":true,"dirty":false}]},"policy_state":{"policy":"fifo","queue":[0,1]}}}
{"seq":10,"step":3,"type":"PageFault","data":{"pid":"P0","vpn":1,"kind":"COMPULSORY","write":false}}
{"seq":11,"step":3,"type":"FrameAllocated","data":{"frame":2,"pid":"P0","vpn":1}}
{"seq":12,"step":3,"type":"PageLoaded","data":{"frame":2,"pid":"P0","vpn":1}}
{"seq":13,"step":3,"type":"Snapshot","step":3,"paging":{"frames":[{"frame":0,"pid":"P0","vpn":7,"referenced":true,"dirty":false},{"frame":1,"pid":"P0","vpn":0,"referenced":true,"dirty":false},{"frame":2,"pid":"P0","vpn":1,"referenced":true,"dirty":false}],"page_tables":{"P0":[{"vpn":0,"frame":1,"present":true,"referenced":true,"dirty":false},{"vpn":1,"frame":2,"present":true,"referenced":true,"dirty":false},{"vpn":7,"frame":0,"present":true,"referenced":true,"dirty":false}]},"policy_state":{"policy":"fifo","queue":[0,1,2]}}}
{"seq":14,"step":4,"type":"PageFault","data":{"pid":"P0","vpn":2,"kind":"COMPULSORY","write":false}}
{"seq":15,"step":4,"type":"VictimSelected","data":{"policy":"fifo","frame":0,"pid":"P0","vpn":7}}
{"seq":16,"step":4,"type":"PageEvicted","data":{"frame":0,"pid":"P0","vpn":7,"dirty":false}}
{"seq":17,"step":4,"type":"PageLoaded","data":{"frame":0,"pid":"P0","vpn":2}}
{"seq":18,"step":4,"type":"Snapshot","step":4,"paging":{"frames":[{"frame":0,"pid":"P0","vpn":2,"referenced":true,"dirty":false},{"frame":1,"pid":"P0","vpn":0,"referenced":true,"dirty":false},{"frame":2,"pid":"P0","vpn":1,"referenced":true,"dirty":false}],"page_tables":{"P0":[{"vpn":0,"frame":1,"present":true,"referenced":true,"dirty":false},{"vpn":1,"frame":2,"present":true,"referenced":true,"dirty":false},{"vpn":2,"frame":0,"present":true,"referenced":true,"dirty":false},{"vpn":7,"frame":null,"present":false,"referenced":false,"dirty":false}]},"policy_state":{"policy":"fifo","queue":[1,2,0]}}}
{"seq":19,"step":5,"type":"PageHit","data":{"pid":"P0","vpn":0,"frame":1,"write":false}}
{"seq":20,"step":5,"type":"Snapshot","step":5,"paging":{"frames":[{"frame":0,"pid":"P0","vpn":2,"referenced":true,"dirty":false},{"frame":1,"pid":"P0","vpn":0,"referenced":true,"dirty":false},{"frame":2,"pid":"P0","vpn":1,"referenced":true,"dirty":false}],"page_tables":{"P0":[{"vpn":0,"frame":1,"present":true,"referenced":true,"dirty":false},{"vpn":1,"frame":2,"present":true,"referenced":true,"dirty":false},{"vpn":2,"frame":0,"present":true,"referenced":true,"dirty":false}]},"policy_state":{"policy":"fifo","queue":[1,2,0]}}}
{"type":"RunSummary","metrics":{"references":5,"hits":1,"faults":4,"compulsory_faults":4,"capacity_faults":0,"evictions":1,"write_backs":0,"fault_rate":0.8,"hit_rate":0.2}}`
  };

  /**
   * UIController: Binds DOM elements, manages interactions, keyboard shortcuts, and view rendering.
   */
  class UIController {
    constructor() {
      this.engine = new PlaybackEngine();
      this.selectedBlock = null;

      this.initDomReferences();
      this.bindEvents();

      this.engine.addListener((eventType) => {
        this.updateView(eventType);
      });

      console.log('MemForge viewer initialized.');
    }

    initDomReferences() {
      // Buttons & Inputs
      this.fileInput = document.getElementById('trace-file-input');
      this.loadFileBtn = document.getElementById('btn-load-file');
      this.dropzone = document.getElementById('trace-dropzone');
      this.sampleBtnAc1 = document.getElementById('btn-sample-ac1');
      this.sampleBtnAc2 = document.getElementById('btn-sample-ac2');
      this.sampleBtnAc3 = document.getElementById('btn-sample-ac3');

      // Transport controls
      this.btnPlay = document.getElementById('btn-play');
      this.btnPrev = document.getElementById('btn-prev');
      this.btnNext = document.getElementById('btn-next');
      this.btnReset = document.getElementById('btn-reset');
      this.btnFailure = document.getElementById('btn-failure');
      this.scrubSlider = document.getElementById('scrub-slider');
      this.speedSelect = document.getElementById('playback-speed');
      this.stepDisplay = document.getElementById('step-display');
      this.stepTotal = document.getElementById('step-total');

      // Containers & Panels
      this.notificationContainer = document.getElementById('notification-container');
      this.metaWorkload = document.getElementById('meta-workload');
      this.metaPolicy = document.getElementById('meta-policy');
      this.metaSubsystem = document.getElementById('meta-subsystem');
      this.metaConfig = document.getElementById('meta-config');

      this.vizContainer = document.getElementById('viz-container');
      this.metricsContainer = document.getElementById('metrics-container');
      this.eventsContainer = document.getElementById('events-container');
      this.inspectorContainer = document.getElementById('inspector-container');
      this.rulerContainer = document.getElementById('ruler-container');
      this.emptyState = document.getElementById('empty-state');
      this.dashboardGrid = document.getElementById('dashboard-grid');
    }

    bindEvents() {
      // File Input Trigger
      if (this.loadFileBtn && this.fileInput) {
        this.loadFileBtn.addEventListener('click', () => this.fileInput.click());
        this.fileInput.addEventListener('change', (e) => {
          const file = e.target.files && e.target.files[0];
          if (file) this.readFile(file);
        });
      }

      // Drag and Drop
      if (this.dropzone) {
        ['dragenter', 'dragover'].forEach(name => {
          this.dropzone.addEventListener(name, (e) => {
            e.preventDefault();
            this.dropzone.style.borderColor = 'var(--border-focus)';
            this.dropzone.style.backgroundColor = 'rgba(0, 210, 255, 0.05)';
          });
        });
        ['dragleave', 'drop'].forEach(name => {
          this.dropzone.addEventListener(name, (e) => {
            e.preventDefault();
            this.dropzone.style.borderColor = 'var(--border-subtle)';
            this.dropzone.style.backgroundColor = 'transparent';
          });
        });
        this.dropzone.addEventListener('drop', (e) => {
          const dt = e.dataTransfer;
          if (dt && dt.files && dt.files.length > 0) {
            this.readFile(dt.files[0]);
          }
        });
      }

      // Sample Trace Buttons
      if (this.sampleBtnAc1) {
        this.sampleBtnAc1.addEventListener('click', () => this.loadRawTraceString(SAMPLE_TRACES.ac1, 'AC-1 (First-Fit Fragmentation)'));
      }
      if (this.sampleBtnAc2) {
        this.sampleBtnAc2.addEventListener('click', () => this.loadRawTraceString(SAMPLE_TRACES.ac2, 'AC-2 (Sliding Compaction)'));
      }
      if (this.sampleBtnAc3) {
        this.sampleBtnAc3.addEventListener('click', () => this.loadRawTraceString(SAMPLE_TRACES.ac3, 'AC-3 (Paging FIFO)'));
      }

      // Transport Control Actions
      if (this.btnPlay) this.btnPlay.addEventListener('click', () => this.engine.togglePlay());
      if (this.btnPrev) this.btnPrev.addEventListener('click', () => this.engine.prevStep());
      if (this.btnNext) this.btnNext.addEventListener('click', () => this.engine.nextStep());
      if (this.btnReset) this.btnReset.addEventListener('click', () => this.engine.reset());
      if (this.btnFailure) this.btnFailure.addEventListener('click', () => this.engine.jumpToNextFailureOrFault());

      if (this.speedSelect) {
        this.speedSelect.addEventListener('change', (e) => {
          this.engine.setSpeed(parseInt(e.target.value, 10));
        });
      }

      if (this.scrubSlider) {
        this.scrubSlider.addEventListener('input', (e) => {
          this.engine.goToStep(parseInt(e.target.value, 10));
        });
      }

      // Keyboard Shortcuts
      window.addEventListener('keydown', (e) => {
        if (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT')) {
          return; // Ignore inside form inputs
        }
        if (e.code === 'Space') {
          e.preventDefault();
          this.engine.togglePlay();
        } else if (e.code === 'ArrowRight' || e.key === 'j') {
          e.preventDefault();
          this.engine.nextStep();
        } else if (e.code === 'ArrowLeft' || e.key === 'k') {
          e.preventDefault();
          this.engine.prevStep();
        } else if (e.code === 'Home' || e.key === 'r') {
          e.preventDefault();
          this.engine.reset();
        } else if (e.key === 'f') {
          e.preventDefault();
          this.engine.jumpToNextFailureOrFault();
        }
      });
    }

    readFile(file) {
      this.clearNotifications();
      const reader = new FileReader();
      reader.onload = (e) => {
        const text = e.target.result;
        this.loadRawTraceString(text, file.name);
      };
      reader.onerror = () => {
        this.showError(`Failed to read file "${file.name}": ${reader.error?.message || 'Unknown read error.'}`);
      };
      reader.readAsText(file);
    }

    loadRawTraceString(rawText, sourceName) {
      this.clearNotifications();
      const result = TraceParser.parseTrace(rawText);
      if (!result.ok) {
        this.showError(result.error);
        return;
      }

      const trace = result.trace;
      trace.sourceName = sourceName;
      this.engine.loadTrace(trace);

      // Display warnings if trace capped or incomplete
      if (trace.warnings && trace.warnings.length > 0) {
        trace.warnings.forEach(w => this.showWarning(w));
      }
    }

    showError(msg) {
      if (!this.notificationContainer) return;
      const el = document.createElement('div');
      el.className = 'banner banner-error';
      el.innerHTML = `
        <span class="banner-icon">⚠️</span>
        <div class="banner-content">
          <div class="banner-title">Error Loading Trace</div>
          <div>${msg}</div>
        </div>
      `;
      this.notificationContainer.appendChild(el);
    }

    showWarning(msg) {
      if (!this.notificationContainer) return;
      const el = document.createElement('div');
      el.className = 'banner banner-warning';
      el.innerHTML = `
        <span class="banner-icon">⚡</span>
        <div class="banner-content">
          <div class="banner-title">Notice</div>
          <div>${msg}</div>
        </div>
      `;
      this.notificationContainer.appendChild(el);
    }

    clearNotifications() {
      if (this.notificationContainer) {
        this.notificationContainer.innerHTML = '';
      }
    }

    updateView(eventType) {
      const step = this.engine.currentStep;
      const trace = this.engine.trace;

      if (!trace || !step) {
        if (this.emptyState) this.emptyState.style.display = 'flex';
        if (this.dashboardGrid) this.dashboardGrid.style.display = 'none';
        return;
      }

      if (this.emptyState) this.emptyState.style.display = 'none';
      if (this.dashboardGrid) this.dashboardGrid.style.display = 'grid';

      // 1. Update Header Pills
      if (eventType === 'load') {
        const h = trace.header;
        if (this.metaWorkload) this.metaWorkload.innerHTML = `Workload: <strong>${h.workload || '—'}</strong>`;
        if (this.metaPolicy) this.metaPolicy.innerHTML = `Policy: <strong>${h.policy || '—'}</strong>`;
        if (this.metaSubsystem) {
          this.metaSubsystem.className = `meta-pill subsystem-${trace.subsystem}`;
          this.metaSubsystem.innerHTML = `Mode: <strong>${trace.subsystem.toUpperCase()}</strong>`;
        }
        if (this.metaConfig) {
          const cfg = h.config || {};
          const cfgStr = trace.subsystem === 'contiguous'
            ? `M=${cfg.memory_size || '?'}`
            : `F=${cfg.frames || '?'}, P=${cfg.page_size || '?'}`;
          this.metaConfig.innerHTML = `Config: <strong>${cfgStr}</strong>`;
        }

        // Configure slider bounds
        if (this.scrubSlider) {
          this.scrubSlider.min = '0';
          this.scrubSlider.max = String(Math.max(0, trace.totalSteps - 1));
        }
      }

      // 2. Update Transport Controls State
      const currentIdx = this.engine.currentStepIndex;
      const totalSteps = this.engine.totalSteps;

      if (this.stepDisplay) this.stepDisplay.textContent = currentIdx + 1;
      if (this.stepTotal) this.stepTotal.textContent = `/ ${totalSteps}`;
      if (this.scrubSlider) this.scrubSlider.value = String(currentIdx);

      if (this.btnPlay) {
        this.btnPlay.innerHTML = this.engine.isPlaying ? '❚❚' : '▶';
        this.btnPlay.setAttribute('aria-label', this.engine.isPlaying ? 'Pause' : 'Play');
      }

      if (this.btnPrev) this.btnPrev.disabled = currentIdx === 0;
      if (this.btnReset) this.btnReset.disabled = currentIdx === 0;
      if (this.btnNext) this.btnNext.disabled = currentIdx >= totalSteps - 1;

      // 3. Render Memory Map or Paging Grid
      if (trace.subsystem === 'contiguous') {
        ContiguousRenderer.renderMemoryMap(this.vizContainer, step, (block, memSize) => {
          this.updateBlockInspector(block, memSize);
        });
        this.renderRuler(step.snapshot?.contiguous?.memory_size || 1000);
      } else {
        PagingRenderer.renderPaging(this.vizContainer, step);
        if (this.rulerContainer) this.rulerContainer.innerHTML = '';
      }

      // 4. Render Metrics & Events
      MetricsRenderer.renderMetrics(this.metricsContainer, step, trace);
      EventRenderer.renderEvents(this.eventsContainer, step);
    }

    renderRuler(memSize) {
      if (!this.rulerContainer) return;
      const q1 = Math.round(memSize * 0.25);
      const q2 = Math.round(memSize * 0.5);
      const q3 = Math.round(memSize * 0.75);
      this.rulerContainer.innerHTML = `
        <span>0</span>
        <span>${q1}</span>
        <span>${q2}</span>
        <span>${q3}</span>
        <span>${memSize}</span>
      `;
    }

    updateBlockInspector(block, memSize) {
      if (!this.inspectorContainer) return;
      if (!block) {
        this.inspectorContainer.innerHTML = `
          <div class="inspector-header">Selected Block</div>
          <div style="color:var(--text-muted);font-size:0.8rem;">Click on any block in the memory map to inspect its details.</div>
        `;
        return;
      }

      const isFree = block.state === 'free';
      const range = `[${block.start}, ${block.start + block.size})`;
      const intFrag = (!isFree && block.requested) ? (block.size - block.requested) : 0;

      this.inspectorContainer.innerHTML = `
        <div class="inspector-header">
          <span>Selected Block</span>
          <span class="meta-pill" style="font-size:0.65rem;">${isFree ? 'FREE' : 'ALLOCATED'}</span>
        </div>
        <div class="inspector-grid">
          <div class="inspector-item">
            <span class="inspector-label">Process ID</span>
            <span class="inspector-val">${block.pid || '—'}</span>
          </div>
          <div class="inspector-item">
            <span class="inspector-label">Allocation ID</span>
            <span class="inspector-val">${block.id || '—'}</span>
          </div>
          <div class="inspector-item">
            <span class="inspector-label">Address Range</span>
            <span class="inspector-val">${range}</span>
          </div>
          <div class="inspector-item">
            <span class="inspector-label">Granted Size</span>
            <span class="inspector-val">${block.size} units</span>
          </div>
          <div class="inspector-item">
            <span class="inspector-label">Requested</span>
            <span class="inspector-val">${block.requested !== undefined ? `${block.requested} units` : '—'}</span>
          </div>
          <div class="inspector-item">
            <span class="inspector-label">Internal Frag</span>
            <span class="inspector-val">${intFrag > 0 ? `${intFrag} units` : '0'}</span>
          </div>
        </div>
      `;
    }
  }

  // Auto-initialize when loaded in browser
  if (typeof document !== 'undefined') {
    document.addEventListener('DOMContentLoaded', () => {
      global.memforgeViewer = new UIController();
    });
  }

  // Export for Node.js automated test suites
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
      TraceParser,
      PlaybackEngine,
      ContiguousRenderer,
      PagingRenderer,
      MetricsRenderer,
      EventRenderer,
      SAMPLE_TRACES,
      MAX_VIEWER_STEPS,
      SUPPORTED_SCHEMA_VERSION
    };
  }

})(typeof window !== 'undefined' ? window : global);
