const $ = (id) => document.getElementById(id);

const state = { slot: null, boss: 0, busy: false };
let bosses = [];

async function api(path, body) {
  const init = body === undefined ? {} : {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  };
  const res = await fetch(path, init);
  if (!res.ok) throw new Error('HTTP ' + res.status);
  return res.json();
}

function toast(message, type) {
  const el = document.createElement('div');
  el.className = 'toast' + (type ? ' ' + type : '');
  el.textContent = message;
  $('toastWrap').appendChild(el);
  setTimeout(() => {
    el.classList.add('out');
    setTimeout(() => el.remove(), 280);
  }, 3000);
}

let modalResolve = null;

function confirmBox(title, text) {
  $('modalTitle').textContent = title;
  $('modalText').textContent = text;
  $('modalMask').classList.add('show');
  return new Promise((resolve) => { modalResolve = resolve; });
}

function closeModal(ok) {
  $('modalMask').classList.remove('show');
  const r = modalResolve;
  modalResolve = null;
  if (r) r(ok);
}

function applyBg(url) {
  document.documentElement.style.setProperty('--bg', 'url("' + url + '")');
}

async function loadMeta() {
  const m = await api('/api/meta');
  $('ver').textContent = 'v' + m.version;
  if (m.customBg) applyBg('assets/custom.jpg?t=' + Date.now());
}

function sizeText(bytes) {
  if (bytes >= 1024 * 1024) return (bytes / 1024 / 1024).toFixed(2) + ' MB';
  return Math.max(1, Math.round(bytes / 1024)) + ' KB';
}

async function loadSaves(keepSlot) {
  const data = await api('/api/saves');
  $('saveFolder').textContent = data.folder;
  const list = $('saveList');
  list.innerHTML = '';
  if (!data.saves.length) {
    const empty = document.createElement('div');
    empty.className = 'empty';
    empty.textContent = '未找到存档';
    list.appendChild(empty);
    $('saveCount').textContent = '未找到存档';
    state.slot = null;
    updateSlotTag();
    return;
  }
  $('saveCount').textContent = '已找到 ' + data.saves.length + ' 个存档';
  data.saves.forEach((s) => {
    const item = document.createElement('div');
    item.className = 'save-item';
    item.dataset.slot = s.slot;
    const dot = document.createElement('span');
    dot.className = 'dot';
    dot.textContent = s.slot;
    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = 'Mystia#' + s.slot + '.memory';
    const meta = document.createElement('span');
    meta.className = 'meta';
    meta.textContent = sizeText(s.size);
    item.appendChild(dot);
    item.appendChild(name);
    item.appendChild(meta);
    item.addEventListener('click', () => selectSlot(s.slot));
    list.appendChild(item);
  });
  const wanted = keepSlot && data.saves.some((s) => s.slot === state.slot) ? state.slot : data.saves[0].slot;
  selectSlot(wanted);
}

async function selectSlot(slot) {
  state.slot = slot;
  document.querySelectorAll('.save-item').forEach((el) => {
    el.classList.toggle('active', Number(el.dataset.slot) === slot);
  });
  updateSlotTag();
  try {
    const info = await api('/api/save?slot=' + slot);
    if (info.ok && typeof info.fund === 'number') $('moneyInput').value = info.fund;
  } catch (e) {
    $('moneyInput').value = '';
  }
}

function updateSlotTag() {
  $('slotTag').textContent = state.slot === null ? '未选择存档' : '当前 Mystia#' + state.slot + '.memory';
}

async function loadBosses() {
  bosses = await api('/api/bosses');
  const sel = $('bossSelect');
  sel.innerHTML = '';
  bosses.forEach((b) => {
    const o = document.createElement('option');
    o.value = b.id;
    o.textContent = b.label + '（' + b.methods + '）';
    sel.appendChild(o);
  });
  state.boss = bosses.length ? bosses[0].id : 0;
  sel.value = state.boss;
  refreshBoss();
}

function refreshBoss() {
  const b = bosses.find((x) => x.id === state.boss);
  if (!b) return;
  $('bossDesc').textContent = b.desc;
  const map = { btnQueue: 'hasQueue', btnClear: 'hasClear', btnInvite: 'hasInvite' };
  Object.keys(map).forEach((id) => {
    const enabled = Boolean(b[map[id]]);
    $(id).disabled = !enabled;
    $(id).title = enabled ? '' : '该 Boss 不支持此方案';
  });
}

async function run(button, path, body, title, text) {
  if (state.slot === null) {
    toast('请先选择一个存档', 'err');
    return;
  }
  if (text) {
    const ok = await confirmBox(title, text);
    if (!ok) return;
  }
  const old = button.textContent;
  button.disabled = true;
  button.textContent = '处理中…';
  try {
    const res = await api(path, Object.assign({ slot: state.slot }, body));
    toast(res.msg, res.ok ? 'ok' : 'err');
    await loadSaves(true);
  } catch (e) {
    toast('调用失败：' + e.message, 'err');
  } finally {
    button.textContent = old;
    refreshBoss();
  }
}

$('btnRefresh').addEventListener('click', async () => {
  try {
    await loadSaves(true);
    toast('存档列表已刷新', 'ok');
  } catch (e) {
    toast('刷新失败：' + e.message, 'err');
  }
});

$('btnMoney').addEventListener('click', () => {
  const v = Number.parseInt($('moneyInput').value, 10);
  if (!Number.isFinite(v)) {
    toast('请输入有效金额', 'err');
    return;
  }
  run($('btnMoney'), '/api/fund', { value: v }, '写入金钱', '会把该存档的金钱改为 ' + v + '，是否继续？');
});

document.querySelectorAll('.chip').forEach((chip) => {
  chip.addEventListener('click', () => { $('moneyInput').value = chip.dataset.money; });
});

$('btnMaps').addEventListener('click', () => {
  run($('btnMaps'), '/api/maps', {}, '解锁全部地图', '会为该存档解锁全部地图（本体 + DLC1~5），并补写 DLC 激活项，是否继续？');
});

$('btnBonds').addEventListener('click', () => {
  run($('btnBonds'), '/api/bonds', {}, '全部满好感', '会把这个存档里所有角色的好感改满（等级 5），是否继续？');
});

$('btnQueue').addEventListener('click', () => {
  run($('btnQueue'), '/api/boss/queue', { boss: state.boss }, '方案A', '方案A 只往存档写 scheduledEvents，实测触发不了，是否继续？');
});

$('btnClear').addEventListener('click', () => {
  run($('btnClear'), '/api/boss/clear', { boss: state.boss }, '方案B：标记通关', '方案B 会把该 Boss 标记为已通关以解锁再战，是否继续？');
});

$('btnInvite').addEventListener('click', () => {
  run($('btnInvite'), '/api/boss/invite', { boss: state.boss }, '方案C：添加邀请函', '方案C 会给该存档添加邀请函（物品 2014~2019），是否继续？');
});

$('bossSelect').addEventListener('change', (e) => {
  state.boss = Number(e.target.value);
  refreshBoss();
});

$('btnBg').addEventListener('click', () => $('bgFile').click());

$('bgFile').addEventListener('change', (e) => {
  const file = e.target.files && e.target.files[0];
  e.target.value = '';
  if (!file) return;
  if (file.size > 12 * 1024 * 1024) {
    toast('图片过大，请选择 12MB 以内的图片', 'err');
    return;
  }
  const reader = new FileReader();
  reader.onload = async () => {
    try {
      const res = await api('/api/bg', { data: reader.result });
      toast(res.msg, res.ok ? 'ok' : 'err');
      if (res.ok) applyBg('assets/custom.jpg?t=' + Date.now());
    } catch (err) {
      toast('设置背景失败：' + err.message, 'err');
    }
  };
  reader.readAsDataURL(file);
});

$('btnBgReset').addEventListener('click', async () => {
  try {
    const res = await api('/api/bg/reset', {});
    toast(res.msg, res.ok ? 'ok' : 'err');
    if (res.ok) applyBg('assets/default.jpg');
  } catch (e) {
    toast('重置失败：' + e.message, 'err');
  }
});

$('btnAbout').addEventListener('click', async () => {
  try {
    const m = await api('/api/meta');
    await confirmBox('关于', '东方夜雀食堂修改器 v' + m.version + '\n存档目录：' + m.folder + '\n\n操作前请备份存档，并关闭 Steam 云同步。');
  } catch (e) {
    toast('读取信息失败：' + e.message, 'err');
  }
});

$('modalOk').addEventListener('click', () => closeModal(true));
$('modalCancel').addEventListener('click', () => closeModal(false));
$('modalMask').addEventListener('click', (e) => { if (e.target === $('modalMask')) closeModal(false); });
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && $('modalMask').classList.contains('show')) closeModal(false);
});

if ('scrollRestoration' in history) history.scrollRestoration = 'manual';

(async function init() {
  window.scrollTo(0, 0);
  try {
    await loadMeta();
    await loadBosses();
    await loadSaves(false);
  } catch (e) {
    toast('无法连接本地服务：' + e.message, 'err');
  }
  window.scrollTo(0, 0);
})();
