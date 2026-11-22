const FONTS = [
  [0, 'Normal (5x7)'],
  [1, 'Tiny Wide (6x5)'],
  [2, 'Small Wide (6x8)'],
  [3, 'Org 01 (5x5)'],
  [4, 'Tom Thumb (3x5)'],
  [5, 'Picopixel (3x5)']
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

function fontSelect(id, val) {
  let h = '<select id="f_' + id + '" data-k="' + id + '">';
  for (const [fId, fName] of FONTS) {
    h += '<option value="' + fId + '"' + (Number(val) === fId ? ' selected' : '') + '>' + fName + '</option>';
  }
  return h + '</select>';
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

function loadSceneConfig() {
  const sid = document.getElementById('scene_id').value;
  const box = document.getElementById('scene-cfg');
  fetch('/scene_config?scene_id=' + encodeURIComponent(sid))
    .then(r => r.json())
    .then(d => {
      if (d.is_auto) {
        box.innerHTML = '<p class="hint">Auto-selected by date. Pick a specific scene above to customize it.</p>';
        return;
      }
      const params = d.params || [];
      if (!params.length) {
        box.innerHTML = '<p class="hint">No editable parameters for this scene.</p>';
        return;
      }
      let h = '<h3>Scene Settings</h3>';
      for (const p of params) {
        const k = escHtml(p.k);
        const l = escHtml(p.l);
        if (p.t === 'color') {
          h += '<div class="field-inline"><label for="f_' + k + '">' + l + '</label>' +
               '<input id="f_' + k + '" type="color" data-k="' + k + '" value="' + escHtml(p.v) + '"></div>';
        } else if (p.t === 'text') {
          h += '<div class="field"><label for="f_' + k + '">' + l + '</label>' +
               '<input id="f_' + k + '" data-k="' + k + '" value="' + escHtml(p.v) + '"></div>';
        } else if (p.t === 'font') {
          h += '<div class="field"><label for="f_' + k + '">' + l + '</label>' + fontSelect(k, p.v) + '</div>';
        } else if (p.t === 'number') {
          h += '<div class="field"><label for="f_' + k + '">' + l + '</label>' +
               '<input id="f_' + k + '" type="number" data-k="' + k + '" min="' + p.min + '" max="' + p.max + '" value="' + escHtml(p.v) + '"></div>';
        }
      }
      h += '<div class="actions">' +
           '<button type="button" id="btn-apply" class="btn btn-primary">Apply</button>' +
           '<button type="button" id="btn-reset" class="btn btn-secondary">Reset Defaults</button></div>';
      box.innerHTML = h;
      box.querySelectorAll('[data-k]').forEach(el => el.addEventListener('change', () => saveSceneConfig(false)));
      document.getElementById('btn-apply').addEventListener('click', () => saveSceneConfig(false));
      document.getElementById('btn-reset').addEventListener('click', () => saveSceneConfig(true));
    });
}

function updateOptions() {
  const params = 'scene_id=' + encodeURIComponent(document.getElementById('scene_id').value) +
                 '&show_ip=' + (document.getElementById('show_ip').checked ? '1' : '0') +
                 '&show_cpu=' + (document.getElementById('show_cpu').checked ? '1' : '0');
  return fetch('/options', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: params
  });
}

document.getElementById('scene_id').addEventListener('change', () => {
  updateOptions().then(() => loadSceneConfig());
});

document.getElementById('show_cpu').addEventListener('change', () => {
  updateOptions().then(() => showToast('Display updated'));
});

document.getElementById('show_ip').addEventListener('change', () => {
  updateOptions().then(() => showToast('Display updated'));
});

loadSceneConfig();
