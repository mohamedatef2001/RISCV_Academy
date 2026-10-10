'use strict';

const state = {
  image: null,
  blurred: null,
  selectedX: 320,
  selectedY: 480,
  scalarStep: 0,
  rvvStep: 0,
};

const scalarStageCount = 12;
const rvvStages = [
  { kind: 'memory', title: 'Three ordered rows in memory', copy: 'Each row supplies vl active values plus two look-ahead values for offsets 1 and 2.' },
  { kind: 'loadTop', title: 'Load the top vector', copy: 'Load vl active top pixels plus two look-ahead pixels.' },
  { kind: 'loadMid', title: 'Load the middle vector', copy: 'Load the corresponding middle pixels once for this chunk.' },
  { kind: 'loadBot', title: 'Load the bottom vector', copy: 'Load the corresponding bottom pixels once. The three row vectors are now resident.' },
  { kind: 'verticalTopMid', title: 'Widen and add top + middle', copy: 'The first widening addition creates a 16-bit partial vertical sum.' },
  { kind: 'verticalBottom', title: 'Add bottom to the vertical sum', copy: 'The second widening addition completes one three-row sum in every lane.' },
  { kind: 'slide2', title: 'Slide vertical by two', copy: 'Expose the third vertical column for every output lane.' },
  { kind: 'addBase', title: 'Add base + slide two', copy: 'Start the horizontal window sum with the base and third vertical columns.' },
  { kind: 'slide1', title: 'Slide vertical by one', copy: 'Expose the middle vertical column for every output lane.' },
  { kind: 'addPlus1', title: 'Add the middle column', copy: 'Every active lane now contains its complete 3×3 sum.' },
  { kind: 'multiply', title: 'Multiply by the reciprocal of nine', copy: 'Multiply each 16-bit sum by 7282 and retain the high half.' },
  { kind: 'narrow', title: 'Narrow to output bytes', copy: 'Convert the 16-bit quotient lanes to 8-bit output pixels.' },
  { kind: 'store', title: 'One store after completion', copy: 'Write the finished byte vector to output memory once. No partial sums touch memory.' },
  { kind: 'next', title: 'Load the next three row-vector chunks', copy: 'Advance x by vl and repeat the same vertical-first sequence for the next chunk.' },
];

function parsePgm(buffer) {
  const bytes = new Uint8Array(buffer);
  let position = 0;
  const whitespace = value => value === 9 || value === 10 || value === 13 || value === 32;
  const token = () => {
    while (position < bytes.length) {
      if (bytes[position] === 35) {
        while (position < bytes.length && bytes[position] !== 10) position += 1;
      } else if (whitespace(bytes[position])) position += 1;
      else break;
    }
    const start = position;
    while (position < bytes.length && !whitespace(bytes[position])) position += 1;
    return new TextDecoder().decode(bytes.slice(start, position));
  };
  const magic = token();
  const width = Number(token());
  const height = Number(token());
  const maxValue = Number(token());
  if (magic !== 'P5' || !width || !height || maxValue !== 255) {
    throw new Error('Expected an 8-bit binary P5 PGM');
  }
  if (position < bytes.length && whitespace(bytes[position])) position += 1;
  const pixels = bytes.slice(position, position + width * height);
  if (pixels.length !== width * height) throw new Error('PGM pixel payload is incomplete');
  return { width, height, pixels };
}

function blurImage(image) {
  const { width, height, pixels } = image;
  const output = new Uint8Array(pixels);
  for (let y = 1; y < height - 1; y += 1) {
    for (let x = 1; x < width - 1; x += 1) {
      let sum = 0;
      for (let ky = -1; ky <= 1; ky += 1) {
        const row = (y + ky) * width;
        for (let kx = -1; kx <= 1; kx += 1) sum += pixels[row + x + kx];
      }
      output[y * width + x] = Math.floor(sum / 9);
    }
  }
  return { width, height, pixels: output };
}

function paintImage(canvas, image) {
  canvas.width = image.width;
  canvas.height = image.height;
  const context = canvas.getContext('2d');
  const rgba = context.createImageData(image.width, image.height);
  for (let i = 0; i < image.pixels.length; i += 1) {
    const value = image.pixels[i];
    rgba.data[4 * i] = value;
    rgba.data[4 * i + 1] = value;
    rgba.data[4 * i + 2] = value;
    rgba.data[4 * i + 3] = 255;
  }
  context.putImageData(rgba, 0, 0);
}

function neighborhood() {
  // Three columns sampled from three consecutive eight-byte image rows.
  if (!state.image) return [1, 2, 3, 9, 10, 11, 17, 18, 19];
  const values = [];
  for (let ky = -1; ky <= 1; ky += 1) {
    for (let kx = -1; kx <= 1; kx += 1) {
      values.push(state.image.pixels[(state.selectedY + ky) * state.image.width + state.selectedX + kx]);
    }
  }
  return values;
}

function makePixelGrid(container, values, options = {}) {
  container.replaceChildren();
  values.forEach((value, index) => {
    const cell = document.createElement('span');
    cell.className = 'pixel-cell';
    if (index === 4) cell.classList.add('center');
    if (options.loadedUntil !== undefined && index < options.loadedUntil) cell.classList.add('loaded');
    if (options.current === index) cell.classList.add('current');
    cell.textContent = value;
    container.appendChild(cell);
  });
}

function updateInspector() {
  const values = neighborhood();
  const sum = values.reduce((total, value) => total + value, 0);
  document.querySelector('#pixel-coordinate').textContent = `(${state.selectedX}, ${state.selectedY})`;
  makePixelGrid(document.querySelector('#neighborhood-grid'), values);
  document.querySelector('#sum-expression').textContent = `${values.join(' + ')} = ${sum}`;
  document.querySelector('#pixel-result').textContent = `${sum} / 9 = ${Math.floor(sum / 9)}`;
  updateMarkers();
  renderScalar();
  renderRvv();
}

function updateMarkers() {
  if (!state.image) return;
  ['input-marker', 'output-marker'].forEach(id => {
    const marker = document.querySelector(`#${id}`);
    marker.style.left = `${(state.selectedX + .5) / state.image.width * 100}%`;
    marker.style.top = `${(state.selectedY + .5) / state.image.height * 100}%`;
  });
}

function choosePixel(event) {
  if (!state.image) return;
  const rectangle = event.currentTarget.getBoundingClientRect();
  const rawX = Math.floor((event.clientX - rectangle.left) / rectangle.width * state.image.width);
  const rawY = Math.floor((event.clientY - rectangle.top) / rectangle.height * state.image.height);
  state.selectedX = Math.max(1, Math.min(state.image.width - 2, rawX));
  state.selectedY = Math.max(1, Math.min(state.image.height - 2, rawY));
  state.scalarStep = 0;
  state.rvvStep = 0;
  updateInspector();
}

function moveSelectedPixel(event) {
  if (!state.image || !['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(event.key)) return;
  event.preventDefault();
  if (event.key === 'ArrowLeft') state.selectedX -= 1;
  if (event.key === 'ArrowRight') state.selectedX += 1;
  if (event.key === 'ArrowUp') state.selectedY -= 1;
  if (event.key === 'ArrowDown') state.selectedY += 1;
  state.selectedX = Math.max(1, Math.min(state.image.width - 2, state.selectedX));
  state.selectedY = Math.max(1, Math.min(state.image.height - 2, state.selectedY));
  state.scalarStep = 0;
  state.rvvStep = 0;
  updateInspector();
}

function scalarLineForStep(step) {
  if (step === 0) return 0;
  if (step <= 9) return step === 1 ? 1 : 2;
  if (step === 10) return 3;
  return 4;
}

function renderScalar() {
  const values = neighborhood();
  const loaded = Math.min(9, Math.max(0, state.scalarStep));
  const sum = values.slice(0, loaded).reduce((total, value) => total + value, 0);
  let title = 'Choose the 3×3 window';
  let copy = 'The border is handled separately. Interior output uses the center pixel and its eight neighbors.';
  if (state.scalarStep >= 1 && state.scalarStep <= 9) {
    title = `Load ${state.scalarStep} of 9`;
    copy = `Read ${values[state.scalarStep - 1]} and add it to the scalar accumulator.`;
  } else if (state.scalarStep === 10) {
    title = 'Divide with native integer arithmetic';
    copy = `${sum} / 9 truncates toward zero, which is floor division for this unsigned sum.`;
  } else if (state.scalarStep === 11) {
    title = 'Store one output pixel';
    copy = `Write ${Math.floor(sum / 9)} at (${state.selectedX}, ${state.selectedY}), then move the window one column.`;
  }
  document.querySelector('#scalar-step-count').textContent = `Step ${state.scalarStep + 1} / ${scalarStageCount}`;
  document.querySelector('#scalar-step-title').textContent = title;
  document.querySelector('#scalar-step-copy').textContent = copy;
  document.querySelector('#scalar-accumulator').textContent = state.scalarStep >= 10 ? `${sum} → ${Math.floor(sum / 9)}` : sum;
  makePixelGrid(document.querySelector('#scalar-grid'), values, {
    loadedUntil: loaded,
    current: state.scalarStep >= 1 && state.scalarStep <= 9 ? state.scalarStep - 1 : -1,
  });
  document.querySelector('#scalar-progress').style.width = `${state.scalarStep / (scalarStageCount - 1) * 100}%`;
  document.querySelector('#scalar-prev').disabled = state.scalarStep === 0;
  document.querySelector('#scalar-next').disabled = state.scalarStep === scalarStageCount - 1;
  const activeLine = scalarLineForStep(state.scalarStep);
  document.querySelectorAll('[data-scalar-line]').forEach(line => line.classList.toggle('active', Number(line.dataset.scalarLine) === activeLine));
}

function vectorData() {
  const outputLanes = 6;
  const loadLanes = outputLanes + 2;
  if (!state.image) {
    // Four contiguous row-major rows. The fourth becomes the next bottom.
    const top = [1, 2, 3, 4, 5, 6, 7, 8];
    const middle = [9, 10, 11, 12, 13, 14, 15, 16];
    const bottom = [17, 18, 19, 20, 21, 22, 23, 24];
    const nextBottom = [25, 26, 27, 28, 29, 30, 31, 32];
    return buildVectorData(top, middle, bottom, nextBottom, outputLanes);
  }
  const xStart = Math.max(0, Math.min(state.image.width - loadLanes, state.selectedX - 1));
  const y = state.selectedY;
  const row = offset => Array.from({ length: loadLanes }, (_, lane) => state.image.pixels[(y + offset) * state.image.width + xStart + lane]);
  const nextOffset = y + 2 < state.image.height ? 2 : 1;
  return buildVectorData(row(-1), row(0), row(1), row(nextOffset), outputLanes);
}

function buildVectorData(top, middle, bottom, nextBottom, outputLanes) {
  const alignments = row => ({
    base: row.slice(0, outputLanes),
    plus1: row.slice(1, outputLanes + 1),
    plus2: row.slice(2, outputLanes + 2),
  });
  const horizontal = parts => parts.base.map((value, lane) => value + parts.plus1[lane] + parts.plus2[lane]);
  const topParts = alignments(top);
  const middleParts = alignments(middle);
  const bottomParts = alignments(bottom);
  const topSums = horizontal(topParts);
  const middleSums = horizontal(middleParts);
  const bottomSums = horizontal(bottomParts);
  const topMiddle = top.map((value, lane) => value + middle[lane]);
  const vertical = topMiddle.map((value, lane) => value + bottom[lane]);
  const verticalParts = alignments(vertical);
  const basePlus2 = verticalParts.base.map((value, lane) => value + verticalParts.plus2[lane]);
  const offset0 = topParts.base.map((value, lane) => value + middleParts.base[lane] + bottomParts.base[lane]);
  const offset1 = topParts.plus1.map((value, lane) => value + middleParts.plus1[lane] + bottomParts.plus1[lane]);
  const offset2 = topParts.plus2.map((value, lane) => value + middleParts.plus2[lane] + bottomParts.plus2[lane]);
  const zeros = Array(outputLanes).fill(0);
  const afterOffset0 = [...offset0];
  const afterOffset1 = afterOffset0.map((value, lane) => value + offset1[lane]);
  const sums = afterOffset1.map((value, lane) => value + offset2[lane]);
  const results = sums.map(value => Math.floor(value * 7282 / 65536));
  return {
    top, middle, bottom, nextBottom,
    topParts, middleParts, bottomParts,
    topSums, middleSums, bottomSums,
    topMiddle, vertical, verticalParts, basePlus2,
    offset0, offset1, offset2,
    zeros, afterOffset0, afterOffset1, sums, results, outputLanes,
  };
}

function registerRow(label, values, className = '', lookaheadFrom = Infinity, highlighted = [], destination = -1) {
  const row = document.createElement('div');
  row.className = `register-row ${className}`;
  const name = document.createElement('span');
  name.className = 'register-label';
  name.textContent = label;
  const lanes = document.createElement('div');
  lanes.className = 'lanes';
  values.forEach((value, index) => {
    const lane = document.createElement('span');
    lane.className = 'lane';
    if (index >= lookaheadFrom) lane.classList.add('lookahead');
    if (highlighted.includes(index)) lane.classList.add('window');
    if (index === destination) lane.classList.add('destination');
    lane.textContent = value;
    lane.title = `lane ${index}`;
    lanes.appendChild(lane);
  });
  row.append(name, lanes);
  return row;
}

function renderEquivalenceProof(stage, vectors) {
  const proof = document.querySelector('#equivalence-proof');
  const visibleKinds = ['verticalTopMid', 'verticalBottom', 'slide2', 'addBase', 'slide1', 'addPlus1', 'multiply', 'narrow', 'store'];
  if (!visibleKinds.includes(stage.kind)) {
    proof.hidden = true;
    proof.replaceChildren();
    return;
  }

  const cells = [
    vectors.top[0], vectors.top[1], vectors.top[2],
    vectors.middle[0], vectors.middle[1], vectors.middle[2],
    vectors.bottom[0], vectors.bottom[1], vectors.bottom[2],
  ];
  const matrix = cells.map(value => `<span>${value}</span>`).join('');
  const columns = [0, 1, 2].map(index => ({
    values: [vectors.top[index], vectors.middle[index], vectors.bottom[index]],
    sum: vectors.vertical[index],
  }));
  const columnCards = columns.map((column, index) =>
    `<div class="proof-column">column ${index}<br>${column.values.join(' + ')}<br>= ${column.sum}</div>`).join('');
  let equation = `<div class="proof-equation">lane 0: ${vectors.top[0]} + ${vectors.middle[0]} = ${vectors.topMiddle[0]}</div>`;
  if (stage.kind === 'verticalBottom') {
    equation = `<div class="proof-equation">lane 0 vertical: ${vectors.topMiddle[0]} + ${vectors.bottom[0]} = ${vectors.vertical[0]}</div>`;
  } else if (stage.kind === 'slide2') {
    equation = `<div class="proof-equation">slide +2 aligns vertical column ${vectors.vertical[2]} with output lane 0</div>`;
  } else if (stage.kind === 'addBase') {
    equation = `<div class="proof-equation">lane 0 partial: ${vectors.vertical[0]} + ${vectors.vertical[2]} = ${vectors.basePlus2[0]}</div>`;
  } else if (stage.kind === 'slide1') {
    equation = `<div class="proof-equation">slide +1 aligns vertical column ${vectors.vertical[1]} with output lane 0</div>`;
  } else if (['addPlus1', 'multiply', 'narrow', 'store'].includes(stage.kind)) {
    equation = `<div class="proof-equation">lane 0 complete: ${vectors.vertical[0]} + ${vectors.vertical[1]} + ${vectors.vertical[2]} = ${vectors.sums[0]}</div>`;
  }
  const result = ['multiply', 'narrow', 'store'].includes(stage.kind)
    ? `<span class="proof-result">Scalar = RVV = floor(${vectors.sums[0]} / 9) = ${vectors.results[0]}</span>`
    : '';

  proof.hidden = false;
  proof.innerHTML = `<h4>Follow output lane 0</h4>` +
    `<p>The same nine pixels are first grouped into three vertical columns, then combined horizontally.</p>` +
    `<div class="proof-flow"><div class="proof-matrix">${matrix}</div>` +
    `<div class="proof-columns">${columnCards}</div></div>` +
    equation + result;
}

function renderRvv() {
  const stage = rvvStages[state.rvvStep];
  const vectors = vectorData();
  let copy = stage.copy || '';
  if (stage.kind === 'verticalTopMid') {
    copy = `Lane 0: widen(${vectors.top[0]}) + ${vectors.middle[0]} = ${vectors.topMiddle[0]}.`;
  } else if (stage.kind === 'verticalBottom') {
    copy = `Lane 0: ${vectors.topMiddle[0]} + ${vectors.bottom[0]} = ${vectors.vertical[0]}.`;
  } else if (stage.kind === 'addBase') {
    copy = `Lane 0: ${vectors.vertical[2]} + ${vectors.vertical[0]} = ${vectors.basePlus2[0]}.`;
  } else if (stage.kind === 'addPlus1') {
    copy = `Lane 0: ${vectors.basePlus2[0]} + ${vectors.vertical[1]} = ${vectors.sums[0]}.`;
  } else if (stage.kind === 'multiply') {
    copy = `Lane 0: high16(${vectors.sums[0]} × 7282) = ${vectors.results[0]}. Scalar also gives floor(${vectors.sums[0]} / 9) = ${Math.floor(vectors.sums[0] / 9)}.`;
  }
  document.querySelector('#rvv-step-count').textContent = `Stage ${state.rvvStep + 1} / ${rvvStages.length}`;
  document.querySelector('#rvv-step-title').textContent = stage.title;
  document.querySelector('#rvv-step-copy').textContent = copy;
  document.querySelector('#rvv-progress').style.width = `${state.rvvStep / (rvvStages.length - 1) * 100}%`;
  document.querySelector('#rvv-prev').disabled = state.rvvStep === 0;
  document.querySelector('#rvv-next').disabled = state.rvvStep === rvvStages.length - 1;

  const stack = document.querySelector('#register-stack');
  stack.replaceChildren();
  if (stage.kind === 'memory') {
    stack.append(registerRow('memory[0–7]', vectors.top, 'active'));
    stack.append(registerRow('memory[8–15]', vectors.middle, 'active'));
    stack.append(registerRow('memory[16–23]', vectors.bottom, 'active'));
  } else if (stage.kind === 'loadTop') {
    stack.append(registerRow('top loaded once', vectors.top, 'active', vectors.outputLanes));
  } else if (stage.kind === 'loadMid') {
    stack.append(registerRow('top (resident)', vectors.top, '', vectors.outputLanes));
    stack.append(registerRow('mid loaded once', vectors.middle, 'active', vectors.outputLanes));
  } else if (stage.kind === 'loadBot') {
    stack.append(registerRow('top (resident)', vectors.top, '', vectors.outputLanes));
    stack.append(registerRow('mid (resident)', vectors.middle, '', vectors.outputLanes));
    stack.append(registerRow('bot loaded once', vectors.bottom, 'active', vectors.outputLanes));
  } else if (stage.kind === 'zero') {
    stack.append(registerRow('temp_output', vectors.zeros, 'result'));
  } else if (stage.kind === 'verticalTopMid') {
    stack.append(registerRow('top', vectors.top, 'active', vectors.outputLanes));
    stack.append(registerRow('middle', vectors.middle, 'active', vectors.outputLanes));
    stack.append(registerRow('top + middle', vectors.topMiddle, 'result', vectors.outputLanes));
  } else if (stage.kind === 'verticalBottom') {
    stack.append(registerRow('top + middle', vectors.topMiddle, 'active', vectors.outputLanes));
    stack.append(registerRow('bottom', vectors.bottom, 'active', vectors.outputLanes));
    stack.append(registerRow('vertical', vectors.vertical, 'result', vectors.outputLanes));
  } else if (stage.kind === 'slide2') {
    stack.append(registerRow('vertical', vectors.verticalParts.base, 'active'));
    stack.append(registerRow('slide +2', vectors.verticalParts.plus2, 'shift'));
  } else if (stage.kind === 'addBase') {
    stack.append(registerRow('vertical', vectors.verticalParts.base, 'active'));
    stack.append(registerRow('slide +2', vectors.verticalParts.plus2, 'shift'));
    stack.append(registerRow('base + plus2', vectors.basePlus2, 'result'));
  } else if (stage.kind === 'slide1') {
    stack.append(registerRow('partial sum', vectors.basePlus2, 'active'));
    stack.append(registerRow('slide +1', vectors.verticalParts.plus1, 'shift'));
  } else if (stage.kind === 'addPlus1') {
    stack.append(registerRow('base + plus2', vectors.basePlus2, 'active'));
    stack.append(registerRow('slide +1', vectors.verticalParts.plus1, 'shift'));
    stack.append(registerRow('3×3 sum', vectors.sums, 'result'));
  } else if (stage.kind === 'multiply') {
    stack.append(registerRow('3×3 sum', vectors.sums, 'active'));
    stack.append(registerRow('floor(sum/9)', vectors.results, 'result'));
  } else if (stage.kind === 'narrow') {
    stack.append(registerRow('16-bit quotient', vectors.results, 'active'));
    stack.append(registerRow('8-bit pixels', vectors.results, 'result'));
  } else if (stage.kind === 'store') {
    stack.append(registerRow('output bytes', vectors.results, 'result'));
  } else if (stage.kind === 'next') {
    stack.append(registerRow('next top chunk', vectors.top, 'active', vectors.outputLanes));
    stack.append(registerRow('next mid chunk', vectors.middle, 'active', vectors.outputLanes));
    stack.append(registerRow('next bot chunk', vectors.bottom, 'active', vectors.outputLanes));
  } else {
    stack.append(registerRow('active lanes', Array.from({length: 8}, (_, i) => i < 6 ? `out${i}` : 'look'), 'active', 6));
  }

  renderEquivalenceProof(stage, vectors);

  const tracePanel = document.querySelector('#division-trace');
  if (stage.kind === 'multiply') {
    tracePanel.hidden = false;
    tracePanel.innerHTML = `Lane 0 · n = <strong>${vectors.sums[0]}</strong><br>` +
      `n × 7282 = ${vectors.sums[0] * 7282}<br>` +
      `high 16 bits = floor((n × 7282) / 65536)<br>` +
      `result = <strong>${vectors.results[0]}</strong>`;
  } else tracePanel.hidden = true;

  const lines = { memory: 0, loadTop: 1, loadMid: 2, loadBot: 3, verticalTopMid: 4, verticalBottom: 5, slide2: 6, addBase: 7, slide1: 8, addPlus1: 9, multiply: 10, narrow: 11, store: 12, next: 13 };
  const activeLine = lines[stage.kind];
  const activeCodeLines = [];
  document.querySelectorAll('[data-rvv-line]').forEach(line => {
    const active = Number(line.dataset.rvvLine) === activeLine;
    line.classList.toggle('active', active);
    if (active) activeCodeLines.push(line);
  });
  if (activeCodeLines.length > 0) {
    activeCodeLines[0].scrollIntoView({ block: 'nearest', behavior: 'smooth' });
  }
}

function moveStep(kind, delta) {
  if (kind === 'scalar') {
    state.scalarStep = Math.max(0, Math.min(scalarStageCount - 1, state.scalarStep + delta));
    renderScalar();
  } else {
    state.rvvStep = Math.max(0, Math.min(rvvStages.length - 1, state.rvvStep + delta));
    renderRvv();
  }
}

function activateTab(name, focus = false) {
  const tabs = [...document.querySelectorAll('[role="tab"]')];
  tabs.forEach(tab => {
    const active = tab.dataset.tab === name;
    tab.setAttribute('aria-selected', active);
    tab.tabIndex = active ? 0 : -1;
    if (active && focus) tab.focus();
  });
  document.querySelectorAll('[role="tabpanel"]').forEach(panel => { panel.hidden = panel.dataset.panel !== name; });
  history.replaceState(null, '', `#${name}`);
}

function initializeTabs() {
  const tabs = [...document.querySelectorAll('[role="tab"]')];
  tabs.forEach((tab, index) => {
    tab.addEventListener('click', () => activateTab(tab.dataset.tab));
    tab.addEventListener('keydown', event => {
      if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
      event.preventDefault();
      let next = index;
      if (event.key === 'ArrowLeft') next = (index - 1 + tabs.length) % tabs.length;
      if (event.key === 'ArrowRight') next = (index + 1) % tabs.length;
      if (event.key === 'Home') next = 0;
      if (event.key === 'End') next = tabs.length - 1;
      activateTab(tabs[next].dataset.tab, true);
    });
  });
  const requested = location.hash.slice(1);
  if (tabs.some(tab => tab.dataset.tab === requested)) activateTab(requested);
}

function initializeTheme() {
  const update = theme => {
    document.documentElement.dataset.theme = theme;
    document.querySelectorAll('[data-theme-value]').forEach(button => button.classList.toggle('active', button.dataset.themeValue === theme));
    try { localStorage.setItem('boxfilter-theme', theme); } catch (_) {}
  };
  document.querySelectorAll('[data-theme-value]').forEach(button => button.addEventListener('click', () => update(button.dataset.themeValue)));
  update(document.documentElement.dataset.theme || 'dark');
}

function formatNumber(value) { return Number(value).toLocaleString('en-US'); }

function loadPerformance() {
  const data = window.BOXFILTER_PERFORMANCE;
  if (!data) return;
  document.querySelector('#average-cycles').textContent = formatNumber(data.averageCycles);
  document.querySelector('#minimum-cycles').textContent = formatNumber(data.minimumCycles);
  document.querySelector('#average-instructions').textContent = formatNumber(data.averageInstructions);
  document.querySelector('#minimum-instructions').textContent = formatNumber(data.minimumInstructions);
  const status = (id, met) => {
    const host = document.querySelector(id);
    host.innerHTML = `<span class="status-chip ${met ? 'pass' : 'fail'}">${met ? 'Met' : 'Not met'}</span>`;
  };
  status('#cycles-status', data.minimumCycles <= 60000);
  status('#instructions-status', data.minimumInstructions <= 20000);
  document.querySelector('#performance-source').textContent = `Source: ${data.source}; ${data.iterations} completed iteration, full 640×960 image0.`;
}

async function loadImage() {
  const status = document.querySelector('#image-status');
  try {
    const response = await fetch('../images/image0.pgm');
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    state.image = parsePgm(await response.arrayBuffer());
    state.blurred = blurImage(state.image);
    state.selectedX = Math.floor(state.image.width / 2);
    state.selectedY = Math.floor(state.image.height / 2);
    paintImage(document.querySelector('#input-canvas'), state.image);
    paintImage(document.querySelector('#output-canvas'), state.blurred);
    status.textContent = `${state.image.width} × ${state.image.height} · ready`;
    status.classList.add('ready');
    updateInspector();
  } catch (error) {
    status.textContent = `PGM unavailable · ${error.message}`;
    status.classList.add('error');
    updateInspector();
  }
}

function initialize() {
  initializeTabs();
  initializeTheme();
  document.querySelector('#input-canvas').addEventListener('click', choosePixel);
  document.querySelector('#output-canvas').addEventListener('click', choosePixel);
  document.querySelector('#input-canvas').addEventListener('keydown', moveSelectedPixel);
  document.querySelector('#output-canvas').addEventListener('keydown', moveSelectedPixel);
  document.querySelector('#scalar-reset').addEventListener('click', () => { state.scalarStep = 0; renderScalar(); });
  document.querySelector('#scalar-prev').addEventListener('click', () => moveStep('scalar', -1));
  document.querySelector('#scalar-next').addEventListener('click', () => moveStep('scalar', 1));
  document.querySelector('#rvv-reset').addEventListener('click', () => { state.rvvStep = 0; renderRvv(); });
  document.querySelector('#rvv-prev').addEventListener('click', () => moveStep('rvv', -1));
  document.querySelector('#rvv-next').addEventListener('click', () => moveStep('rvv', 1));
  renderScalar();
  renderRvv();
  loadPerformance();
  loadImage();
}

initialize();
