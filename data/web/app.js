const FONTS = [
  [0, 'Normal (5x7)'],
  [1, 'Tiny Wide (6x5)'],
  [2, 'Small Wide (6x8)'],
  [3, 'Org 01 (5x5)'],
  [4, 'Tom Thumb (3x5)'],
  [5, 'Picopixel (3x5)'],
  [6, 'Zolo Micro (4x4)'],
  [7, 'Wide (5x5)'],
  [8, 'Space Grotesk Bold (32px)'],
  [9, 'Inter MediumBold (16px)']
];

let toastTimer = null;

function showToast(msg) {
  const t = document.getElementById('toast');
  if (!t) return;
  t.textContent = msg;
  t.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove('show'), 1800);
}

function escHtml(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function saveSceneConfig(reset) {
  const sid = document.getElementById('scene_id').value;
  const parts = ['scene_id=' + encodeURIComponent(sid)];
  if (reset) {
    parts.push('reset=1');
  } else {
    document.querySelectorAll('#scene-cfg [data-k]').forEach(el => {
      parts.push(encodeURIComponent(el.dataset.k) + '=' + encodeURIComponent(el.value));
    });
  }
  fetch('/scene_config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: parts.join('&')
  }).then(() => {
    showToast(reset ? 'Reset to defaults' : 'Updated');
    if (reset) loadSceneConfig();
  });
}

function renderParamFields(container, params, prefix) {
  let h = '';
  for (const p of params) {
    const k = escHtml(p.k);
    const l = escHtml(p.l);
    const id = prefix + k;
    if (p.t === 'color') {
      h += '<div class="field-inline"><label for="' + id + '">' + l + '</label>' +
           '<input id="' + id + '" type="color" data-k="' + k + '" value="' + escHtml(p.v) + '"></div>';
    } else if (p.t === 'text') {
      h += '<div class="field"><label for="' + id + '">' + l + '</label>' +
           '<input id="' + id + '" data-k="' + k + '" value="' + escHtml(p.v) + '"></div>';
    } else if (p.t === 'font') {
      let sel = '<select id="' + id + '" data-k="' + k + '">';
      for (const [fId, fName] of FONTS) {
        sel += '<option value="' + fId + '"' + (Number(p.v) === fId ? ' selected' : '') + '>' + fName + '</option>';
      }
      sel += '</select>';
      h += '<div class="field"><label for="' + id + '">' + l + '</label>' + sel + '</div>';
    } else if (p.t === 'number') {
      h += '<div class="field"><label for="' + id + '">' + l + '</label>' +
           '<input id="' + id + '" type="number" data-k="' + k + '" min="' + p.min + '" max="' + p.max + '" value="' + escHtml(p.v) + '"></div>';
    } else if (p.t === 'select') {
      let sel = '<select id="' + id + '" data-k="' + k + '">';
      (p.opts || []).forEach((opt, idx) => {
        sel += '<option value="' + idx + '"' + (Number(p.v) === idx ? ' selected' : '') + '>' + escHtml(opt) + '</option>';
      });
      sel += '</select>';
      h += '<div class="field"><label for="' + id + '">' + l + '</label>' + sel + '</div>';
    }
  }
  container.innerHTML = h;
}

function buildScheduleFieldsHtml(o) {
  const isRec = Boolean(o.recurring);
  let wdHtml = '';
  for (const [bit, name] of WDAY_BITS) {
    wdHtml += '<label style="display:inline-flex;align-items:center;gap:4px;margin-right:8px;">' +
              '<input type="checkbox" class="occ-wd" value="' + bit + '"' + ((o.wd & bit) ? ' checked' : '') + '>' + name + '</label>';
  }
  return '<h3>Occasion Schedule</h3>' +
    '<div class="field"><label for="occ-label">Label (max 16 chars)</label>' +
      '<input id="occ-label" maxlength="16" value="' + escHtml(o.label) + '"></div>' +
    '<div class="field"><label>Start (Day / Month / Hour / Min)</label><div style="display:grid;grid-template-columns:repeat(4,1fr);gap:6px;">' +
      '<input id="occ-sd" type="number" min="1" max="31" value="' + o.sd + '">' +
      '<input id="occ-sm" type="number" min="1" max="12" value="' + o.sm + '">' +
      '<input id="occ-sh" type="number" min="0" max="23" value="' + o.sh + '">' +
      '<input id="occ-smin" type="number" min="0" max="59" value="' + o.smin + '"></div></div>' +
    '<div class="field"><label>End (Day / Month / Hour / Min)</label><div style="display:grid;grid-template-columns:repeat(4,1fr);gap:6px;">' +
      '<input id="occ-ed" type="number" min="1" max="31" value="' + o.ed + '">' +
      '<input id="occ-em" type="number" min="1" max="12" value="' + o.em + '">' +
      '<input id="occ-eh" type="number" min="0" max="23" value="' + o.eh + '">' +
      '<input id="occ-emin" type="number" min="0" max="59" value="' + o.emin + '"></div></div>' +
    '<label class="toggle-row" style="margin-top:8px;">' +
      '<span>Reoccuring (within date range)</span>' +
      '<input type="checkbox" id="occ-rec-en"' + (isRec ? ' checked' : '') + '>' +
    '</label>' +
    '<div id="occ-rec"' + (isRec ? '' : ' hidden') + '>' +
      '<div class="field"><label>Weekdays</label><div>' + wdHtml + '</div></div>' +
      '<div class="field"><label>From / To (Hour : Min)</label><div style="display:grid;grid-template-columns:repeat(4,1fr);gap:6px;">' +
        '<input id="occ-fh" type="number" min="0" max="23" value="' + o.fh + '">' +
        '<input id="occ-fmin" type="number" min="0" max="59" value="' + o.fmin + '">' +
        '<input id="occ-th" type="number" min="0" max="23" value="' + o.th + '">' +
        '<input id="occ-tmin" type="number" min="0" max="59" value="' + o.tmin + '"></div></div>' +
    '</div>';
}

function readOccasionFromScenePane() {
  let wd = 0;
  document.querySelectorAll('#scene-cfg .occ-wd:checked').forEach(cb => { wd |= Number(cb.value); });
  const params = {};
  document.querySelectorAll('#scene-fields [data-k]').forEach(el => { params[el.dataset.k] = el.value; });
  return {
    label: (document.getElementById('occ-label').value || '').slice(0, 16) || sceneLabelById(document.getElementById('scene_id').value).slice(0, 16),
    recurring: document.getElementById('occ-rec-en').checked ? 1 : 0,
    sm: Number(document.getElementById('occ-sm').value),
    sd: Number(document.getElementById('occ-sd').value),
    sh: Number(document.getElementById('occ-sh').value),
    smin: Number(document.getElementById('occ-smin').value),
    em: Number(document.getElementById('occ-em').value),
    ed: Number(document.getElementById('occ-ed').value),
    eh: Number(document.getElementById('occ-eh').value),
    emin: Number(document.getElementById('occ-emin').value),
    wd,
    fh: Number(document.getElementById('occ-fh').value),
    fmin: Number(document.getElementById('occ-fmin').value),
    th: Number(document.getElementById('occ-th').value),
    tmin: Number(document.getElementById('occ-tmin').value),
    scene_id: Number(document.getElementById('scene_id').value),
    params
  };
}

function loadSceneConfig() {
  const sid = document.getElementById('scene_id').value;
  const box = document.getElementById('scene-cfg');
  fetch('/scene_config?scene_id=' + encodeURIComponent(sid))
    .then(r => r.json())
    .then(d => {
      if (d.is_auto) {
        box.innerHTML = '<p class="hint">Auto-selected by Occasions. Pick a specific scene above to configure or add an occasion.</p>';
        return;
      }
      const params = d.params || [];
      const draft = editingOccDraft || {
        label: sceneLabelById(sid).slice(0, 16), recurring: 0,
        sm: 1, sd: 1, sh: 0, smin: 0, em: 12, ed: 31, eh: 23, emin: 59,
        wd: 127, fh: 0, fmin: 0, th: 23, tmin: 59
      };
      const isEdit = editingOccIdx >= 0;
      const paramsHtml = params.length
        ? '<h3>Scene Settings</h3><div id="scene-fields"></div>'
        : '<p class="hint">No editable parameters for this scene.</p><div id="scene-fields"></div>';
      const cancelBtn = isEdit ? '<button type="button" id="btn-occ-cancel" class="btn btn-secondary">Cancel</button>' : '';
      box.innerHTML = paramsHtml + buildScheduleFieldsHtml(draft) +
        '<div class="actions">' +
          '<button type="button" id="btn-apply" class="btn btn-secondary">Apply</button>' +
          '<button type="button" id="btn-occ-commit" class="btn btn-primary">' + (isEdit ? 'Update Occasion' : 'Add Occasion') + '</button>' +
          cancelBtn +
        '</div>';

      if (params.length) {
        renderParamFields(document.getElementById('scene-fields'), params, 'f_');
        box.querySelectorAll('#scene-fields [data-k]').forEach(el => el.addEventListener('change', () => saveSceneConfig(false)));
      }
      document.getElementById('occ-rec-en').addEventListener('change', (e) => {
        document.getElementById('occ-rec').hidden = !e.target.checked;
      });
      document.getElementById('btn-apply').addEventListener('click', () => saveSceneConfig(false));
      document.getElementById('btn-occ-commit').addEventListener('click', () => {
        const entry = readOccasionFromScenePane();
        let targetIdx = editingOccIdx;
        if (targetIdx >= 0) {
          occasions[targetIdx] = entry;
        } else {
          occasions.push(entry);
          targetIdx = occasions.length - 1;
        }
        editingOccIdx = -1;
        editingOccDraft = null;
        saveOccasions().then(() => {
          loadSceneConfig();
          const occCard = document.getElementById('card-occasions');
          if (occCard) occCard.open = true;
          const row = document.querySelector('#occ-list [data-occ-idx="' + targetIdx + '"]');
          if (row) row.scrollIntoView({ behavior: 'smooth', block: 'center' });
        });
      });
      if (isEdit) {
        document.getElementById('btn-occ-cancel').addEventListener('click', () => {
          editingOccIdx = -1;
          editingOccDraft = null;
          loadSceneConfig();
        });
      }
    });
}

function updateOptions() {
  const parts = [
    'scene_id=' + encodeURIComponent(document.getElementById('scene_id').value),
    'show_ip=' + (document.getElementById('show_ip').checked ? '1' : '0'),
    'show_cpu=' + (document.getElementById('show_cpu').checked ? '1' : '0'),
    'panel_chain=' + encodeURIComponent(document.getElementById('panel_chain').value),
    'brightness=' + encodeURIComponent(document.getElementById('brightness').value)
  ];
  document.querySelectorAll('.pcal').forEach(el => {
    parts.push('p' + el.dataset.p + '_' + el.dataset.c + '=' + encodeURIComponent(el.value));
  });
  return fetch('/options', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: parts.join('&')
  });
}

document.getElementById('scene_id').addEventListener('change', () => {
  editingOccIdx = -1;
  editingOccDraft = null;
  updateOptions().then(() => loadSceneConfig());
});

document.getElementById('show_cpu').addEventListener('change', () => {
  updateOptions().then(() => showToast('Display updated'));
});

document.getElementById('show_ip').addEventListener('change', () => {
  updateOptions().then(() => showToast('Display updated'));
});

document.getElementById('panel_chain').addEventListener('change', () => {
  updateOptions().then(() => showToast('Saved (reset to apply)'));
});

const brtEl = document.getElementById('brightness');
const brtVal = document.getElementById('brightness-val');
brtEl.addEventListener('input', () => {
  brtVal.textContent = brtEl.value;
});
brtEl.addEventListener('change', () => {
  updateOptions().then(() => showToast('Brightness updated'));
});

document.querySelectorAll('.pcal').forEach(el => {
  el.addEventListener('change', () => {
    updateOptions().then(() => showToast('Calibration updated'));
  });
});

document.getElementById('btn-reboot').addEventListener('click', () => {
  showToast('Resetting...');
  fetch('/reboot', { method: 'POST' }).then(() => {
    setTimeout(() => location.reload(), 3000);
  });
});

const aboutModal = document.getElementById('about-modal');
document.getElementById('btn-about').addEventListener('click', () => {
  aboutModal.hidden = false;
});
document.getElementById('btn-about-close').addEventListener('click', () => {
  aboutModal.hidden = true;
});
aboutModal.addEventListener('click', (e) => {
  if (e.target === aboutModal) aboutModal.hidden = true;
});
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && !aboutModal.hidden) aboutModal.hidden = true;
});

// --- Occasions management ---
// tm_wday bit 0=Sun(1), 1=Mon(2), 2=Tue(4), 3=Wed(8), 4=Thu(16), 5=Fri(32), 6=Sat(64)
const WDAY_BITS = [
  [2, 'Mon'], [4, 'Tue'], [8, 'Wed'], [16, 'Thu'], [32, 'Fri'], [64, 'Sat'], [1, 'Sun']
];
let occasions = [];
let editingOccIdx = -1;
let editingOccDraft = null;

function pad2(n) { return String(n).padStart(2, '0'); }

function sceneLabelById(id) {
  const opt = document.querySelector('#scene_id option[value="' + id + '"]');
  return opt ? opt.textContent : ('Scene ' + id);
}

function occSummary(o) {
  let s = o.sd + '/' + o.sm + ' ' + pad2(o.sh) + ':' + pad2(o.smin) + ' - ' + o.ed + '/' + o.em + ' ' + pad2(o.eh) + ':' + pad2(o.emin);
  if (o.recurring) {
    const days = WDAY_BITS.filter(([b]) => (o.wd & b) !== 0).map(([, n]) => n).join(',');
    s += ' | ' + (days || 'None') + ' ' + pad2(o.fh) + ':' + pad2(o.fmin) + '-' + pad2(o.th) + ':' + pad2(o.tmin);
  }
  return s;
}

function saveOccasions() {
  return fetch('/occasions', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(occasions)
  }).then(() => {
    showToast('Occasions saved');
    renderOccasionsList();
  });
}

function editOccasionInScenePane(idx) {
  const o = occasions[idx];
  if (!o) return;
  editingOccIdx = idx;
  editingOccDraft = JSON.parse(JSON.stringify(o));

  document.getElementById('scene_id').value = String(o.scene_id);
  const parts = ['scene_id=' + encodeURIComponent(o.scene_id)];
  if (o.params) {
    for (const [k, v] of Object.entries(o.params)) {
      parts.push(encodeURIComponent(k) + '=' + encodeURIComponent(v));
    }
  }
  updateOptions()
    .then(() => fetch('/scene_config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: parts.join('&')
    }))
    .then(() => loadSceneConfig())
    .then(() => {
      const sceneCard = document.getElementById('card-scene');
      if (sceneCard) {
        sceneCard.open = true;
        sceneCard.scrollIntoView({ behavior: 'smooth', block: 'start' });
      }
    });
}

function renderOccasionsList() {
  const box = document.getElementById('occ-list');
  if (!occasions.length) {
    box.innerHTML = '<p class="hint">No occasions configured (falls back to NormalDays).</p>';
    return;
  }
  let h = '';
  occasions.forEach((o, idx) => {
    h += '<div class="occ-item" data-occ-idx="' + idx + '" style="display:flex;justify-content:space-between;align-items:center;gap:8px;padding:8px 4px;border-bottom:1px solid rgba(255,255,255,0.08);cursor:pointer;">' +
         '<div style="min-width:0;flex:1;">' +
           '<div><b>' + escHtml(o.label) + '</b> &rarr; ' + escHtml(sceneLabelById(o.scene_id)) + '</div>' +
           '<div class="hint" style="margin:2px 0 0;">' + escHtml(occSummary(o)) + '</div>' +
         '</div>' +
         '<div style="display:flex;gap:4px;flex-shrink:0;">' +
           '<button type="button" class="btn btn-secondary" data-occ-up="' + idx + '"' + (idx === 0 ? ' disabled' : '') + '>&uarr;</button>' +
           '<button type="button" class="btn btn-secondary" data-occ-dn="' + idx + '"' + (idx === occasions.length - 1 ? ' disabled' : '') + '>&darr;</button>' +
           '<button type="button" class="btn btn-secondary" data-occ-del="' + idx + '">&times;</button>' +
         '</div></div>';
  });
  box.innerHTML = h;
  box.querySelectorAll('.occ-item').forEach(row => row.addEventListener('click', (e) => {
    if (e.target.closest('button')) return;
    editOccasionInScenePane(Number(row.dataset.occIdx));
  }));
  box.querySelectorAll('[data-occ-up]').forEach(b => b.addEventListener('click', () => {
    const i = Number(b.dataset.occUp);
    if (i > 0) { [occasions[i - 1], occasions[i]] = [occasions[i], occasions[i - 1]]; saveOccasions(); }
  }));
  box.querySelectorAll('[data-occ-dn]').forEach(b => b.addEventListener('click', () => {
    const i = Number(b.dataset.occDn);
    if (i < occasions.length - 1) { [occasions[i + 1], occasions[i]] = [occasions[i], occasions[i + 1]]; saveOccasions(); }
  }));
  box.querySelectorAll('[data-occ-del]').forEach(b => b.addEventListener('click', () => {
    const i = Number(b.dataset.occDel);
    if (editingOccIdx === i) { editingOccIdx = -1; editingOccDraft = null; loadSceneConfig(); }
    occasions.splice(i, 1);
    saveOccasions();
  }));
}

function loadOccasions() {
  fetch('/occasions').then(r => r.json()).then(list => {
    occasions = Array.isArray(list) ? list : [];
    renderOccasionsList();
  });
}

document.getElementById('btn-occ-reset').addEventListener('click', () => {
  fetch('/occasions', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'reset=1'
  }).then(() => {
    editingOccIdx = -1;
    editingOccDraft = null;
    showToast('Occasions reset');
    loadOccasions();
    loadSceneConfig();
  });
});

loadSceneConfig();
loadOccasions();
