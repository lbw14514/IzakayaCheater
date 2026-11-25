const $ = (id) => document.getElementById(id);

const AUTO_ATTACH_KEY = 'izakaya.autoAttach';

function readAutoAttach() {
  try {
    return localStorage.getItem(AUTO_ATTACH_KEY) !== '0';
  } catch {
    return true;
  }
}

const state = { slot: null, boss: 0, method: 'B', busy: false, page: 'live', attachBusy: false, specialBusy: false, autoAttach: readAutoAttach() };
let bosses = [];
const LIVE_CATEGORIES = [
  ['badges', '徽章'],
  ['clothes', '衣服'],
  ['records', '唱片'],
  ['fishing', '垂钓'],
  ['partners', '伙伴'],
  ['decorations', '装饰'],
  ['ingredients', '食材'],
  ['foods', '料理'],
  ['beverages', '酒水'],
  ['cookers', '厨具'],
  ['recipes', '食谱'],
  ['izakayas', '店面']
];
const FEATURE_SWITCHES = [
  { key: 'freeze_time', label: '暂停时间' },
  { key: 'freeze_day', label: '锁白天' },
  { key: 'freeze_night', label: '锁夜晚' },
  { key: 'max_combo', label: '最高 Combo' },
  { key: 'lock_patience', label: '锁客人耐心' },
  { key: 'max_mood', label: '最高心情' },
  { key: 'instant_cook', label: '瞬烹' },
  { key: 'perfect_qte', label: '完美 QTE' },
  { key: 'infinite_exp', label: '无限经验' },
  { key: 'money_multiplier', label: '金钱倍率', value: 2.0 },
  { key: 'pickup_multiplier', label: '拾取倍率', value: 2.0 },
  { key: 'exp_multiplier', label: '经验倍率', value: 2.0 },
  { key: 'move_speed_multiplier', label: '移动速度', value: 2.0 },
  { key: 'game_speed', label: '游戏速度', value: 2.0 }
];

const FEATURE_HOTKEYS = [
  { codes: ['Digit1', 'Numpad1'], label: 'Ctrl+1' },
  { codes: ['Digit2', 'Numpad2'], label: 'Ctrl+2' },
  { codes: ['Digit3', 'Numpad3'], label: 'Ctrl+3' },
  { codes: ['Digit4', 'Numpad4'], label: 'Ctrl+4' },
  { codes: ['Digit5', 'Numpad5'], label: 'Ctrl+5' },
  { codes: ['Digit6', 'Numpad6'], label: 'Ctrl+6' },
  { codes: ['Digit7', 'Numpad7'], label: 'Ctrl+7' },
  { codes: ['Digit8', 'Numpad8'], label: 'Ctrl+8' },
  { codes: ['Digit9', 'Numpad9'], label: 'Ctrl+9' },
  { codes: ['Digit0', 'Numpad0'], label: 'Ctrl+0' },
  { codes: ['Minus', 'NumpadSubtract'], label: 'Ctrl+-' },
  { codes: ['Equal', 'NumpadAdd'], label: 'Ctrl+=' },
  { codes: ['Period', 'NumpadDecimal'], label: 'Ctrl+.' },
  { codes: ['Slash', 'NumpadDivide'], label: 'Ctrl+/' }
];

let liveStatusBusy = false;
let liveBusy = false;

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
  $('modalCancel').style.display = '';
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
  $('bgImg').src = url;
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

function selectSlot(slot) {
  state.slot = slot;
  document.querySelectorAll('.save-item').forEach((el) => {
    el.classList.toggle('active', Number(el.dataset.slot) === slot);
  });
  updateSlotTag();
  if (state.page === 'bond') loadBonds();
  if (state.page === 'settings') loadBackups();
}

function updateSlotTag() {
  $('slotTag').textContent = state.slot === null ? '未选择存档' : 'Mystia#' + state.slot;
}

const SAVE_WRITE_CONFIRM = '将会修改存档数据 操作不可逆 是否继续';

const BOSS_METHODS = [
  { key: 'A', label: '排队事件 已禁用', endpoint: '/api/boss/queue', flag: 'hasQueue', disabled: true,
    title: '排队事件', text: SAVE_WRITE_CONFIRM,
    desc: '已禁用 只往存档写 scheduledEvents 实测不触发 真正生效需要在游戏运行时调用 ScheduleEventExtern' },
  { key: 'B', label: '标记通关', endpoint: '/api/boss/clear', flag: 'hasClear',
    title: '标记通关', text: SAVE_WRITE_CONFIRM,
    desc: '写入 finishedMissions 把最终战标记为已完成 等于跳过 需要自己去再战 不会直接开打' },
  { key: 'C', label: '邀请函', endpoint: '/api/boss/invite', flag: 'hasInvite',
    title: '邀请函', text: SAVE_WRITE_CONFIRM,
    desc: '添加邀请函物品 2014 到 2019 可在游戏内使用' }
];

async function loadBosses() {
  bosses = await api('/api/bosses');
  const sel = $('bossSelect');
  sel.innerHTML = '';
  bosses.forEach((b) => {
    const o = document.createElement('option');
    o.value = b.id;
    o.textContent = b.label;
    sel.appendChild(o);
  });
  state.boss = bosses.length ? bosses[0].id : 0;
  sel.value = state.boss;
  syncDropdown(sel);
  renderBossMethods();
  refreshBoss();
}

function renderBossMethods() {
  const sel = $('bossMethod');
  sel.innerHTML = '';
  BOSS_METHODS.forEach((m) => {
    const o = document.createElement('option');
    o.value = m.key;
    o.textContent = m.label;
    o.disabled = Boolean(m.disabled);
    sel.appendChild(o);
  });
  sel.value = state.method;
  syncDropdown(sel);
}

function refreshBoss() {
  const b = bosses.find((x) => x.id === state.boss);
  if (!b) return;

  const methodSel = $('bossMethod');
  BOSS_METHODS.forEach((m) => {
    const option = [...methodSel.options].find((o) => o.value === m.key);
    if (option) option.disabled = Boolean(m.disabled) || !b[m.flag];
  });

  let method = BOSS_METHODS.find((m) => m.key === state.method);
  if (!method || method.disabled || !b[method.flag]) {
    const first = BOSS_METHODS.find((m) => !m.disabled && b[m.flag]);
    if (first) {
      state.method = first.key;
      methodSel.value = first.key;
      method = first;
    }
  }

  const supported = method && !method.disabled && Boolean(b[method.flag]);
  $('bossDesc').textContent = method
    ? method.desc + (supported ? '' : (method.disabled ? '' : '\n当前 Boss 不支持此方式'))
    : '';
  const runButton = $('btnBossRun');
  runButton.disabled = !supported;
  runButton.title = supported ? '' : (method && method.disabled ? '该方式已禁用' : '当前 Boss 不支持此方式');
  syncDropdown(methodSel);
}

function syncDropdown(select) {
  const wrap = select.closest('.dropdown');
  if (!wrap) return;
  const toggle = wrap.querySelector('.dd-toggle');
  const panel = wrap.querySelector('.dd-panel');
  panel.innerHTML = '';
  [...select.options].forEach((option) => {
    const item = document.createElement('div');
    item.className = 'dd-item'
      + (option.disabled ? ' disabled' : '')
      + (option.selected ? ' selected' : '');
    item.dataset.value = option.value;
    item.textContent = option.textContent;
    panel.appendChild(item);
  });
  const current = select.options[select.selectedIndex];
  toggle.textContent = current ? current.textContent : '';
}

function closeDropdowns(except) {
  document.querySelectorAll('.dropdown.open').forEach((wrap) => {
    if (wrap === except) return;
    wrap.classList.remove('open');
    wrap.querySelector('.dd-panel').hidden = true;
    wrap.querySelector('.dd-toggle').setAttribute('aria-expanded', 'false');
  });
}

function enhanceSelect(select) {
  const wrap = document.createElement('div');
  wrap.className = 'dropdown' + (select.classList.contains('level-select') ? ' dd-level' : '');
  const toggle = document.createElement('button');
  toggle.type = 'button';
  toggle.className = 'input dd-toggle';
  toggle.setAttribute('aria-haspopup', 'listbox');
  toggle.setAttribute('aria-expanded', 'false');
  const panel = document.createElement('div');
  panel.className = 'dd-panel';
  panel.hidden = true;
  select.parentNode.insertBefore(wrap, select);
  select.classList.add('dd-native');
  wrap.appendChild(select);
  wrap.appendChild(toggle);
  wrap.appendChild(panel);

  const setOpen = (open) => {
    closeDropdowns(open ? wrap : null);
    wrap.classList.toggle('open', open);
    panel.hidden = !open;
    toggle.setAttribute('aria-expanded', String(open));
  };

  toggle.addEventListener('click', () => setOpen(panel.hidden));
  panel.addEventListener('click', (e) => {
    const item = e.target.closest('.dd-item');
    if (!item || item.classList.contains('disabled')) return;
    select.value = item.dataset.value;
    setOpen(false);
    syncDropdown(select);
    select.dispatchEvent(new Event('change'));
  });
  syncDropdown(select);
}

document.addEventListener('click', (e) => {
  if (!e.target.closest('.dropdown')) closeDropdowns(null);
});

function setPage(page) {
  state.page = page;
  document.querySelectorAll('.nav-btn').forEach((button) => {
    const active = button.dataset.target === page;
    button.classList.toggle('active', active);
    button.setAttribute('aria-selected', active ? 'true' : 'false');
  });
  document.querySelectorAll('.stack > .card[data-page]').forEach((card) => {
    card.hidden = card.dataset.page !== page;
  });
  if (page === 'live') ensureLiveAttached();
  if (page === 'bond') loadBonds();
  if (page === 'settings') loadBackups();
}

function renderLiveCategories() {
  const grid = $('obtainGrid');
  grid.innerHTML = '';
  LIVE_CATEGORIES.forEach(([key, label]) => {
    const button = document.createElement('button');
    button.className = 'chip';
    button.dataset.obtain = key;
    button.textContent = '全部' + label;
    button.disabled = true;
    grid.appendChild(button);
  });
}

function renderFeatureSwitches() {
  const grid = $('featureSwitches');
  grid.innerHTML = '';
  FEATURE_SWITCHES.forEach((feature, index) => {
    const row = document.createElement('div');
    row.className = 'switch-row';

    const toggle = document.createElement('label');
    toggle.className = 'switch-toggle';

    const input = document.createElement('input');
    input.type = 'checkbox';
    input.dataset.feature = feature.key;
    input.addEventListener('change', () => setFeature(feature, input.checked));

    const track = document.createElement('span');
    track.className = 'switch-track';
    const thumb = document.createElement('span');
    thumb.className = 'switch-thumb';
    track.appendChild(thumb);

    const label = document.createElement('span');
    label.className = 'switch-label';
    label.textContent = feature.label;

    toggle.appendChild(input);
    toggle.appendChild(track);
    toggle.appendChild(label);
    row.appendChild(toggle);

    if (feature.value !== undefined) {
      const value = document.createElement('input');
      value.type = 'text';
      value.className = 'input switch-value';
      value.inputMode = 'decimal';
      value.dataset.input = 'decimal';
      value.value = String(feature.value);
      value.setAttribute('aria-label', feature.label + '数值');
      value.addEventListener('input', () => {
        const parsed = Number.parseFloat(value.value);
        if (Number.isFinite(parsed)) feature.value = parsed;
      });
      value.addEventListener('change', () => {
        if (input.checked) setFeature(feature, true);
      });
      row.appendChild(value);
    }

    const key = document.createElement('kbd');
    key.textContent = FEATURE_HOTKEYS[index].label;

    row.appendChild(key);
    grid.appendChild(row);
  });
}

async function setFeature(feature, enabled) {
  const input = document.querySelector(`input[data-feature="${feature.key}"]`);
  try {
    const payload = { key: feature.key, enabled };
    if (feature.value !== undefined) payload.value = Number(feature.value);
    const res = await api('/api/trainer/feature', payload);
    if (!res.ok) throw new Error(res.msg || '开关未生效');
  } catch (e) {
    if (input) input.checked = !enabled;
    toast('开关失败 ' + e.message, 'err');
  }
}

function setLiveReady(moneyReady, obtainReady) {
  $('btnLiveMoney').disabled = !moneyReady;
  document.querySelectorAll('[data-obtain]').forEach((button) => {
    button.disabled = !obtainReady;
  });
}

async function ensureLiveAttached() {
  if (state.page !== 'live') return;
  try {
    const st = await api('/api/trainer/status');
    if (state.autoAttach && st.game && !st.attached) {
      await api('/api/trainer/attach', {});
    }
  } catch (e) {
    // 状态轮询会显示具体错误
  }
  loadTrainerStatus();
}

async function attachGame() {
  if (state.attachBusy) return;
  state.attachBusy = true;
  const button = $('btnAttachGame');
  if (button) button.disabled = true;
  try {
    const res = await api('/api/trainer/attach', {});
    if (!res.ok) throw new Error(res.msg || '附加失败');
    toast('已附加游戏', 'ok');
  } catch (e) {
    toast('附加失败 ' + e.message, 'err');
  } finally {
    state.attachBusy = false;
    loadTrainerStatus();
  }
}

async function runAutoSelfCheck() {
  try {
    const res = await api('/api/selfcheck');
    const lines = res.lines || [];
    console.info('[自检]\n' + lines.join('\n'));
    const bad = lines.filter((line) => /缺失|读取失败|管理员权限 否/.test(line));
    if (bad.length) toast('自检发现 ' + bad.length + ' 项异常 ', 'err');
  } catch (e) {
    console.warn('自检失败', e);
  }
}

async function loadTrainerStatus() {
  if (liveStatusBusy) return;
  liveStatusBusy = true;
  try {
    const st = await api('/api/trainer/status');
    if (state.autoAttach && st.game && !st.attached && !state.attachBusy) {
      state.attachBusy = true;
      try {
        await api('/api/trainer/attach', {});
      } catch (e) {
        // 下一次轮询继续尝试
      }
      state.attachBusy = false;
      if (state.page === 'live') loadTrainerStatus();
      return;
    }
    const tag = $('liveTag');
    const procTag = $('procTag');
    const procDesc = $('procDesc');
    const attached = Boolean(st.attached);
    const attachButton = $('btnAttachGame');
    if (attachButton) {
      attachButton.hidden = !st.game || attached;
      attachButton.disabled = state.attachBusy;
    }
    const freezeSwitch = document.querySelector('input[data-feature="freeze_time"]');
    if (freezeSwitch) freezeSwitch.checked = Boolean(st.frozen);
    if (st.featureStates) {
      const states = {};
      st.featureStates.split(' ').forEach((item) => {
        const parts = item.split('=');
        if (parts.length === 2) states[parts[0]] = parts[1] === '1';
      });
      FEATURE_SWITCHES.forEach((feature) => {
        if (feature.key === 'freeze_time') return;
        if (!Object.prototype.hasOwnProperty.call(states, feature.key)) return;
        const input = document.querySelector(`input[data-feature="${feature.key}"]`);
        if (input) input.checked = states[feature.key];
      });
    }
    if (st.featureValues) {
      const values = {};
      st.featureValues.split(' ').forEach((item) => {
        const parts = item.split('=');
        if (parts.length === 2) values[parts[0]] = Number.parseFloat(parts[1]);
      });
      FEATURE_SWITCHES.forEach((feature) => {
        if (!Object.prototype.hasOwnProperty.call(values, feature.key)) return;
        const parsed = values[feature.key];
        if (!Number.isFinite(parsed)) return;
        feature.value = parsed;
        const input = document.querySelector(`input[data-feature="${feature.key}"]`);
        const row = input && input.closest('.switch-row');
        const valueInput = row && row.querySelector('input.switch-value');
        if (valueInput && document.activeElement !== valueInput) {
          valueInput.value = String(parsed);
        }
      });
    }
    if (!attached) {
      FEATURE_SWITCHES.forEach((feature) => {
        if (feature.key === 'freeze_time') return;
        const input = document.querySelector(`input[data-feature="${feature.key}"]`);
        if (input) input.checked = false;
      });
    }
    const moneyReady = attached && Boolean(st.moneyReady);
    const obtainReady = attached && Boolean(st.game);
    const ready = moneyReady || obtainReady;

    const lamp = $('procLamp');
    if (!st.game) {
      lamp.className = 'status-lamp off';
      procTag.textContent = '游戏未启动';
      procDesc.textContent = '未检测到游戏进程';
    } else if (!attached) {
      lamp.className = 'status-lamp detected';
      procTag.textContent = '游戏运行中';
      procDesc.textContent = '已检测到游戏进程 未注入';
    } else if (!ready) {
      lamp.className = 'status-lamp detected';
      procTag.textContent = '已注入 未就绪';
      procDesc.textContent = '部分功能未就绪';
    } else {
      lamp.className = 'status-lamp live';
      procTag.textContent = '已注入';
      procDesc.textContent = '常用功能可用';
    }

    if (!st.game) {
      tag.textContent = '未检测到游戏';
    } else if (!attached) {
      tag.textContent = '未连接服务';
    } else if (!ready) {
      tag.textContent = '已连接服务 未就绪';
    } else {
      tag.textContent = '已连接服务';
      if (moneyReady) $('liveMoney').placeholder = '当前金钱 ' + st.money;
    }

    setLiveReady(moneyReady, obtainReady);
  } catch (e) {
    $('liveTag').textContent = '状态读取失败';
    $('procLamp').className = 'status-lamp off';
    $('procTag').textContent = '检测失败';
    $('procDesc').textContent = e.message;
    setLiveReady(false, false);
  } finally {
    liveStatusBusy = false;
  }
}

async function liveAction(button, path, body, title, text) {
  if (liveBusy) return;
  if (text) {
    const ok = await confirmBox(title, text);
    if (!ok) return;
  }
  liveBusy = true;
  const old = button.textContent;
  button.disabled = true;
  button.textContent = '处理中';
  try {
    const res = await api(path, body || {});
    toast(res.msg, res.ok ? 'ok' : 'err');
    await loadTrainerStatus();
  } catch (e) {
    toast('调用失败 ' + e.message, 'err');
  } finally {
    button.textContent = old;
    liveBusy = false;
  }
}

const bondMembers = [];
const HIDDEN_BOND_IDS = new Set([12, 13]);
const NON_BOND_NAMES = new Set(['蹦蹦跳跳的三妖精', '怨灵']);
let characterNames = null;

function isRealBondName(name) {
  if (!name) return false;
  const text = String(name).trim();
  if (!text) return false;
  if (NON_BOND_NAMES.has(text)) return false;
  if (/占位角色|未知/.test(text)) return false;
  if (/^角色\s*\d+$/.test(text)) return false;
  return true;
}

async function getCharacterNames() {
  if (characterNames) return characterNames;
  try {
    const res = await fetch('character_names.json');
    characterNames = res.ok ? await res.json() : {};
  } catch (e) {
    characterNames = {};
  }
  return characterNames;
}

function bondCheckedIds() {
  return [...$('bondPersonPanel').querySelectorAll('input[type="checkbox"]:checked')]
    .flatMap((checkbox) => (checkbox.dataset.ids || checkbox.value).split(',').map(Number));
}

function updateBondToggle() {}

function setBondPanel() {
  $('bondPersonPanel').hidden = false;
}

function renderBondMembers() {
  const panel = $('bondPersonPanel');
  panel.innerHTML = '';
  bondMembers.forEach((bond) => {
    const item = document.createElement('label');
    item.className = 'bond-item';
    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.value = String(bond.ids[0]);
    checkbox.dataset.ids = bond.ids.join(',');
    checkbox.addEventListener('change', updateBondToggle);
    const name = document.createElement('span');
    name.textContent = bond.name;
    const level = document.createElement('em');
    level.textContent = 'Lv' + bond.level;
    item.appendChild(checkbox);
    item.appendChild(name);
    item.appendChild(level);
    panel.appendChild(item);
  });
  updateBondToggle();
}

async function loadBonds() {
  const tag = $('bondTag');
  bondMembers.length = 0;
  if (state.slot === null) {
    renderBondMembers();
    setBondPanel(false);
    tag.textContent = '未选择存档';
    $('bondDesc').textContent = '请先选择一个存档';
    return;
  }
  try {
    const data = await api('/api/bonds/list?slot=' + state.slot);
    if (!data.ok || !data.bonds.length) {
      renderBondMembers();
      setBondPanel(false);
      tag.textContent = '没有好感数据';
      $('bondDesc').textContent = '该存档没有找到好感数据';
      return;
    }
    await getCharacterNames();
    const names = characterNames || {};
    const groups = new Map();
    data.bonds.forEach((bond) => {
      if (HIDDEN_BOND_IDS.has(Number(bond.id))) return;
      const name = bond.name || names[String(bond.id)];
      if (!isRealBondName(name)) return;
      if (!groups.has(name)) groups.set(name, { name, ids: [], level: bond.level });
      const group = groups.get(name);
      group.ids.push(bond.id);
      group.level = Math.max(group.level, bond.level);
    });
    bondMembers.push(...groups.values());
    renderBondMembers();
    if (!bondMembers.length) {
      tag.textContent = '没有可编辑角色';
      $('bondDesc').textContent = '该存档只有占位角色';
      return;
    }
    tag.textContent = 'Mystia#' + state.slot;
    $('bondDesc').textContent = '已读取 ' + bondMembers.length + ' 个角色 选人物和等级后写入';
  } catch (e) {
    renderBondMembers();
    setBondPanel(false);
    tag.textContent = '读取失败';
    $('bondDesc').textContent = e.message;
  }
}

async function loadBackups() {
  const tag = $('backupTag');
  const list = $('backupList');
  const desc = $('backupDesc');
  if (!list || !tag || !desc) return;
  list.innerHTML = '';

  if (state.slot === null) {
    tag.textContent = '未选择存档';
    desc.textContent = '请先选择一个存档';
    return;
  }

  tag.textContent = 'Mystia#' + state.slot;
  try {
    const data = await api('/api/backups?slot=' + state.slot);
    if (!data.ok) throw new Error(data.msg || '读取备份失败');
    const backups = (data.backups || []).slice().sort((a, b) => b.id - a.id);
    if (!backups.length) {
      const empty = document.createElement('div');
      empty.className = 'backup-empty';
      empty.textContent = '该存档还没有备份 第一次写入后自动生成初始备份';
      list.appendChild(empty);
      desc.textContent = '写入前自动备份 可恢复到任意版本 恢复前请先关闭游戏';
      return;
    }

    const totalStored = backups.reduce((sum, item) => sum + (Number(item.storedSize) || 0), 0);
    desc.textContent = '共 ' + backups.length + ' 个版本 占用 ' + sizeText(totalStored) + ' 恢复前请先关闭游戏';
    backups.forEach((entry) => {
      const item = document.createElement('div');
      item.className = 'backup-item';

      const info = document.createElement('div');
      info.className = 'backup-info';
      const time = document.createElement('div');
      time.className = 'backup-time';
      time.textContent = entry.time || '未知时间';
      const meta = document.createElement('div');
      meta.className = 'backup-meta';
      const kind = entry.kind === 'initial' ? '初始未修改' : entry.kind === 'full' ? '全量' : '增量';
      const stored = Number(entry.storedSize) || 0;
      meta.textContent = kind + ' · 占用 ' + sizeText(stored) + ' · 存档 ' + sizeText(Number(entry.size) || 0);
      info.appendChild(time);
      info.appendChild(meta);

      const button = document.createElement('button');
      button.className = 'btn';
      button.type = 'button';
      button.textContent = '恢复';
      button.addEventListener('click', () => restoreBackup(entry));

      item.appendChild(info);
      item.appendChild(button);
      list.appendChild(item);
    });
  } catch (e) {
    const empty = document.createElement('div');
    empty.className = 'backup-empty';
    empty.textContent = '读取失败 ' + e.message;
    list.appendChild(empty);
    desc.textContent = '备份读取失败';
  }
}

async function restoreBackup(entry) {
  if (state.slot === null) {
    toast('请先选择一个存档', 'err');
    return;
  }
  const ok = await confirmBox('恢复备份', '会把当前存档替换为 ' + entry.time + ' 的版本 当前状态会先自动备份 是否继续');
  if (!ok) return;
  try {
    const res = await api('/api/backups/restore', { slot: state.slot, id: entry.id });
    if (!res.ok) throw new Error(res.msg || '恢复失败');
    toast('已恢复备份', 'ok');
    await loadSaves(true);
  } catch (e) {
    toast('恢复失败 ' + e.message, 'err');
  }
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
  button.textContent = '处理中';
  try {
    const res = await api(path, Object.assign({ slot: state.slot }, body));
    toast(res.msg, res.ok ? 'ok' : 'err');
    await loadSaves(true);
  } catch (e) {
    toast('调用失败 ' + e.message, 'err');
  } finally {
    button.textContent = old;
    button.disabled = false;
    refreshBoss();
  }
}

document.querySelectorAll('.nav-btn').forEach((button) => {
  button.addEventListener('click', () => setPage(button.dataset.target));
});

$('btnAttachGame').addEventListener('click', attachGame);

const autoAttachToggle = $('autoAttachToggle');
if (autoAttachToggle) {
  autoAttachToggle.checked = state.autoAttach;
  autoAttachToggle.addEventListener('change', () => {
    state.autoAttach = autoAttachToggle.checked;
    try {
      localStorage.setItem(AUTO_ATTACH_KEY, state.autoAttach ? '1' : '0');
    } catch (e) {
      // 存储失败时只影响下次启动
    }
    loadTrainerStatus();
  });
}

$('btnRefresh').addEventListener('click', async () => {
  try {
    await loadSaves(true);
    toast('存档列表已刷新', 'ok');
  } catch (e) {
    toast('刷新失败：' + e.message, 'err');
  }
});

$('btnBondApply').addEventListener('click', async () => {
  if (state.slot === null) {
    toast('请先选择一个存档', 'err');
    return;
  }
  const ids = bondCheckedIds();
  if (!ids.length) {
    toast('请先选择人物', 'err');
    return;
  }
  const checked = document.querySelectorAll('#bondPersonPanel input[type="checkbox"]:checked').length;
  const level = Number.parseInt($('bondLevel').value, 10);
  const ok = await confirmBox('修改好感度', '会把选中的 ' + checked + ' 个角色好感度改为等级 ' + level + ' 是否继续');
  if (!ok) return;
  const button = $('btnBondApply');
  button.disabled = true;
  button.textContent = '处理中';
  try {
    const res = await api('/api/bonds/set', { slot: state.slot, ids, level });
    toast(res.msg, res.ok ? 'ok' : 'err');
    await loadBonds();
  } catch (e) {
    toast('调用失败 ' + e.message, 'err');
  } finally {
    button.disabled = false;
    button.textContent = '写入选中角色';
  }
});

$('btnLiveMoney').addEventListener('click', () => {
  const value = Number.parseInt($('liveMoney').value, 10);
  if (!Number.isFinite(value) || value < 0) {
    toast('请输入有效金额', 'err');
    return;
  }
  liveAction($('btnLiveMoney'), '/api/trainer/money', { value }, '实时改金钱',
    '会立即修改运行中游戏的金钱为 ' + value + ' 是否继续');
});

$('obtainGrid').addEventListener('click', (e) => {
  const button = e.target.closest('[data-obtain]');
  if (!button || button.disabled) return;
  const label = button.textContent;
  liveAction(button, '/api/trainer/obtain', { category: button.dataset.obtain }, label,
    '会立即把' + label + '加入运行中游戏 是否继续');
});

$('bossSelect').addEventListener('change', (e) => {
  state.boss = Number(e.target.value);
  refreshBoss();
});

$('bossMethod').addEventListener('change', (e) => {
  state.method = e.target.value;
  refreshBoss();
});

$('btnBossRun').addEventListener('click', () => {
  const method = BOSS_METHODS.find((m) => m.key === state.method);
  if (!method) return;
  run($('btnBossRun'), method.endpoint, { boss: state.boss }, method.title, method.text);
});

document.addEventListener('keydown', (e) => {
  if (!e.ctrlKey || e.altKey || e.shiftKey || e.metaKey) return;
  if (e.target && e.target.closest && e.target.closest('input:not([type="checkbox"]), textarea, select')) return;
  const index = FEATURE_HOTKEYS.findIndex((item) => item.codes.includes(e.code));
  if (index < 0) return;
  const feature = FEATURE_SWITCHES[index];
  if (!feature) return;
  const input = document.querySelector(`input[data-feature="${feature.key}"]`);
  if (!input) return;
  e.preventDefault();
  if (e.repeat) return;
  input.checked = !input.checked;
  setFeature(feature, input.checked);
});

if (window.chrome && window.chrome.webview) {
  window.chrome.webview.addEventListener('message', (event) => {
    let data = event.data;
    if (typeof data === 'string') {
      try { data = JSON.parse(data); } catch { return; }
    }
    if (!data || data.type !== 'toggle-feature') return;
    const feature = FEATURE_SWITCHES.find((item) => item.key === data.key);
    if (!feature) return;
    const input = document.querySelector(`input[data-feature="${feature.key}"]`);
    if (!input) return;
    input.checked = !input.checked;
    setFeature(feature, input.checked);
  });
}

document.addEventListener('beforeinput', (e) => {
  const input = e.target;
  if (!(input instanceof HTMLInputElement)) return;
  const mode = input.dataset.input;
  if (!mode || !e.data || e.isComposing) return;
  if (mode === 'int' && !/^\d+$/.test(e.data)) e.preventDefault();
  if (mode === 'decimal' && !/^[\d.]+$/.test(e.data)) e.preventDefault();
});

document.addEventListener('input', (e) => {
  const input = e.target;
  if (!(input instanceof HTMLInputElement)) return;
  const mode = input.dataset.input;
  if (!mode) return;
  const raw = input.value;
  let clean = raw;
  if (mode === 'int') {
    clean = raw.replace(/[^\d]/g, '');
  } else if (mode === 'decimal') {
    clean = raw.replace(/[^\d.]/g, '');
    const dot = clean.indexOf('.');
    if (dot !== -1) {
      clean = clean.slice(0, dot + 1) + clean.slice(dot + 1).replace(/\./g, '');
    }
  }
  if (clean !== raw) input.value = clean;
});

$('btnBg').addEventListener('click', () => $('bgFile').click());

$('bgFile').addEventListener('change', (e) => {
  const file = e.target.files && e.target.files[0];
  e.target.value = '';
  if (!file) return;
  if (file.size > 12 * 1024 * 1024) {
    toast('图片过大 请选择 12MB 以内的图片', 'err');
    return;
  }
  const reader = new FileReader();
  reader.onload = async () => {
    try {
      const res = await api('/api/bg', { data: reader.result });
      toast(res.msg, res.ok ? 'ok' : 'err');
      if (res.ok) applyBg('assets/custom.jpg?t=' + Date.now());
    } catch (err) {
      toast('设置背景失败 ' + err.message, 'err');
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
    toast('重置失败 ' + e.message, 'err');
  }
});

function showAbout(m) {
  $('modalTitle').textContent = '关于';
  const box = $('modalText');
  box.textContent = '';
  const rows = [
    '东方夜雀食堂修改器 v' + m.version,
    '存档目录 ' + m.folder,
    '操作前请备份存档 并关闭 Steam 云同步'
  ];
  rows.forEach((text, i) => {
    if (i) box.appendChild(document.createElement('hr'));
    const p = document.createElement('p');
    p.textContent = text;
    box.appendChild(p);
  });
  box.appendChild(document.createElement('hr'));
  const repo = document.createElement('p');
  repo.appendChild(document.createTextNode('仓库地址 '));
  const link = document.createElement('a');
  link.href = 'https://github.com/lbw14514/IzakayaCheater';
  link.target = '_blank';
  link.rel = 'noreferrer';
  link.textContent = 'https://github.com/lbw14514/IzakayaCheater';
  repo.appendChild(link);
  box.appendChild(repo);
  const star = document.createElement('p');
  star.textContent = '喜欢本项目的话希望可以给咱一颗星';
  box.appendChild(star);
  $('modalCancel').style.display = 'none';
  $('modalMask').classList.add('show');
  return new Promise((resolve) => { modalResolve = resolve; });
}

$('btnAbout').addEventListener('click', async () => {
  try {
    const m = await api('/api/meta');
    await showAbout(m);
  } catch (e) {
    toast('读取信息失败 ' + e.message, 'err');
  }
});

$('modalOk').addEventListener('click', () => closeModal(true));
$('modalCancel').addEventListener('click', () => closeModal(false));
$('modalMask').addEventListener('click', (e) => { if (e.target === $('modalMask')) closeModal(false); });
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && $('modalMask').classList.contains('show')) closeModal(false);
});

const SPECIAL_GROUPS = [
  {
    title: '白天调试',
    note: '先点注册调试方法 白天场景使用 场景不对会失败',
    actions: [
      { key: 'day_register', label: '注册调试方法' },
      { key: 'day_add_exp', label: '增加经验', arg: 'int', default: 10000 },
      { key: 'day_max_resources', label: '最大资源' },
      { key: 'day_random_resources', label: '随机资源' },
      { key: 'day_test_resources', label: '测试资源' },
      { key: 'day_silver_frog', label: '银蛙币', arg: 'int', default: 10 },
      { key: 'day_unlock_special', label: '解锁特殊客和地图' },
      { key: 'day_unlock_music', label: '解锁全部音乐' },
      { key: 'day_next_day', label: '跳到下一天', danger: true },
      { key: 'day_set_days', label: '设置天数', arg: 'int', default: 1, danger: true },
      { key: 'day_add_days', label: '增加天数', arg: 'int', default: 1, danger: true },
      { key: 'day_warp_hours', label: '快进小时', arg: 'int', default: 1 },
      { key: 'day_warp_actions', label: '快进行动', arg: 'int', default: 1 },
      { key: 'day_clear_ing', label: '清空食材', danger: true },
      { key: 'day_clear_bev', label: '清空酒水', danger: true },
      { key: 'day_clear_items', label: '清空道具', danger: true },
      { key: 'day_start_missions', label: '开始全部任务', danger: true },
      { key: 'day_finish_all', label: '完成全部事件任务', danger: true },
      { key: 'day_refresh_collectable', label: '刷新采集点' },
      { key: 'day_reset_collectable_cd', label: '重置采集冷却' },
      { key: 'day_refresh_npc', label: '刷新 NPC' },
      { key: 'day_goto_kourindou', label: '前往香霖堂', danger: true },
      { key: 'day_restock_kourindou', label: '香霖堂补货' },
      { key: 'day_invite_tewi', label: '邀请帝' },
      { key: 'day_money_box', label: '博丽钱箱捐款', arg: 'int', default: 1000 },
      { key: 'day_multi_partner', label: '多伙伴模式', arg: 'bool' },
      { key: 'day_max_partner', label: '伙伴数量', arg: 'int', default: 4 },
      { key: 'day_roguelike', label: '进入 RogueLike', danger: true },
      { key: 'day_reimu_spell', label: '灵梦符卡' },
      { key: 'day_challenge_lv1', label: '白玉楼挑战 1', danger: true },
      { key: 'day_challenge_lv2', label: '白玉楼挑战 2', danger: true },
      { key: 'day_challenge_final', label: '白玉楼最终挑战', danger: true },
      { key: 'day_weird_cooking', label: '奇怪料理挑战', danger: true },
      { key: 'day_festival_music', label: '祭祀音乐挑战', danger: true },
      { key: 'day_festival_cooking', label: '祭祀料理挑战', danger: true },
      { key: 'day_creators_box', label: '创作者盒子' },
      { key: 'day_panel1', label: '无限选择面板 1' },
      { key: 'day_panel2', label: '无限选择面板 2' },
      { key: 'day_panel3', label: '无限选择面板 3' },
      { key: 'day_collider', label: '玩家碰撞体', arg: 'bool', danger: true },
      { key: 'day_input', label: '玩家输入', arg: 'bool', danger: true }
    ]
  },
  {
    title: '夜间调试',
    note: '夜间场景使用 生成客人和 QTE 需要对应场景',
    actions: [
      { key: 'night_register', label: '注册调试方法' },
      { key: 'night_cheated_buff', label: '作弊 Buff' },
      { key: 'night_eternity_fever', label: '永久 Fever' },
      { key: 'night_remove_buffs', label: '移除全部 Buff' },
      { key: 'night_free_cook', label: '免费烹饪秒数', arg: 'int', default: 60 },
      { key: 'night_income_add', label: '营业收入增加', arg: 'int', default: 1000 },
      { key: 'night_income_set', label: '营业收入设置', arg: 'int', default: 1000 },
      { key: 'night_combo_add', label: '连击增加', arg: 'int', default: 10 },
      { key: 'night_combo_set', label: '连击设置', arg: 'int', default: 10 },
      { key: 'night_modify_time', label: '修改营业时间', arg: 'int', default: 60 },
      { key: 'night_refill_bev', label: '补充酒水' },
      { key: 'night_refill_ing', label: '补充食材' },
      { key: 'night_refill_food', label: '补充料理' },
      { key: 'night_spawn_normal', label: '生成普通客' },
      { key: 'night_spawn_special', label: '生成特殊客' },
      { key: 'night_pay_leave', label: '全部付款离开' },
      { key: 'night_qte_random', label: '随机 QTE' },
      { key: 'night_qte_throw', label: '投掷上菜 QTE' },
      { key: 'night_qte_patient', label: '耐心 QTE' },
      { key: 'night_qte_finish', label: '立即吃完 QTE' },
      { key: 'night_passion_up', label: '提升气氛' },
      { key: 'night_passion_down', label: '降低气氛' },
      { key: 'night_throw_deliver', label: '投递上菜', arg: 'int', default: 1 },
      { key: 'night_fever', label: 'Fever', arg: 'int', default: 1 }
    ]
  },
  {
    title: '地图传送',
    note: '仅白天场景有效 传送会改变场景状态',
    actions: [
      { key: 'map_beast_forest', label: '兽道', arg: 'fixed', text: 'BeastForest' },
      { key: 'map_human_village', label: '人里', arg: 'fixed', text: 'HumanVillage' },
      { key: 'map_hakurei_shrine', label: '博丽神社', arg: 'fixed', text: 'HakureiShrine' },
      { key: 'map_scarlet_mansion', label: '红魔馆', arg: 'fixed', text: 'ScarletMansion' },
      { key: 'map_bamboo_forest', label: '竹林', arg: 'fixed', text: 'BambooForest' },
      { key: 'map_hakugyokurou', label: '白玉楼', arg: 'fixed', text: 'Hakugyokurou' },
      { key: 'map_youkai_mountain', label: '妖怪之山', arg: 'fixed', text: 'YoukaiMountain' },
      { key: 'map_magic_forest', label: '魔法森林', arg: 'fixed', text: 'MagicForest' },
      { key: 'map_former_hell', label: '旧地狱', arg: 'fixed', text: 'FormerHell' },
      { key: 'map_myouren_temple', label: '命莲寺', arg: 'fixed', text: 'MyourenTemple' },
      { key: 'map_garden_of_the_sun', label: '太阳花田', arg: 'fixed', text: 'GardenOfTheSun' },
      { key: 'map_flandre_home', label: '红魔馆地下', arg: 'fixed', text: 'FlandreHome' }
    ]
  },
  {
    title: '全局',
    note: '场景切换和日志操作 实验性',
    actions: [
      { key: 'global_console_on', label: '开启内置调试控制台' },
      { key: 'global_console_off', label: '关闭内置调试控制台' },
      { key: 'global_goto_main', label: '回到主菜单', danger: true },
      { key: 'global_goto_empty', label: '进入空场景', danger: true },
      { key: 'global_export', label: '导出存档和日志' },
      { key: 'global_open_log', label: '打开日志目录' },
      { key: 'main_test_result', label: '测试结算场景', danger: true }
    ]
  },
  {
    title: 'Steam 成就（危险）',
    note: '会永久修改 Steam 成就 可能违反 Steam 服务条款 且无法撤销',
    danger: true,
    actions: [
      { key: 'ach_check_all', label: '检查全部成就条件', danger: true },
      { key: 'ach_finish', label: '解锁指定成就 ID', arg: 'text', placeholder: 'Achievement_1_1stPlay', danger: true },
      { key: 'ach_finish_check', label: '解锁并检查指定 ID', arg: 'text', placeholder: 'Achievement_1_1stPlay', danger: true }
    ]
  }
];

function renderSpecialGroups() {
  const root = $('specialGroups');
  if (!root) return;
  root.innerHTML = '';
  SPECIAL_GROUPS.forEach((group) => {
    const section = document.createElement('div');
    section.className = 'special-group' + (group.danger ? ' danger' : '');

    const head = document.createElement('div');
    head.className = 'special-group-head';
    const title = document.createElement('h3');
    title.textContent = group.title;
    head.appendChild(title);
    if (group.danger) {
      const tag = document.createElement('span');
      tag.className = 'tag tag-warn';
      tag.textContent = '危险';
      head.appendChild(tag);
    }
    section.appendChild(head);

    if (group.note) {
      const note = document.createElement('p');
      note.className = 'special-group-note';
      note.textContent = group.note;
      section.appendChild(note);
    }

    const actions = document.createElement('div');
    actions.className = 'special-actions';
    group.actions.forEach((action) => {
      const row = document.createElement('div');
      row.className = 'special-action' + (action.danger ? ' danger' : '');

      if (action.arg === 'int') {
        const input = document.createElement('input');
        input.className = 'input';
        input.type = 'number';
        input.value = String(action.default ?? 0);
        input.setAttribute('aria-label', action.label + ' 数值');
        row.appendChild(input);
      } else if (action.arg === 'text') {
        const input = document.createElement('input');
        input.className = 'input';
        input.type = 'text';
        input.placeholder = action.placeholder || '';
        input.setAttribute('aria-label', action.label + ' 文本');
        row.appendChild(input);
      } else if (action.arg === 'bool') {
        const input = document.createElement('input');
        input.type = 'checkbox';
        input.checked = Boolean(action.default);
        input.setAttribute('aria-label', action.label + ' 启用');
        row.appendChild(input);
      }

      const button = document.createElement('button');
      button.type = 'button';
      button.className = 'btn';
      button.textContent = action.label;
      button.addEventListener('click', () => runSpecialAction(action, row));
      row.appendChild(button);
      actions.appendChild(row);
    });
    section.appendChild(actions);
    root.appendChild(section);
  });
}

async function runSpecialAction(action, row) {
  if (state.specialBusy) return;
  let value = 0;
  let text = '';
  if (action.arg === 'int') {
    const input = row.querySelector('input[type="number"]');
    value = Number.parseInt(input ? input.value : '0', 10) || 0;
  } else if (action.arg === 'bool') {
    const input = row.querySelector('input[type="checkbox"]');
    value = input && input.checked ? 1 : 0;
  } else if (action.arg === 'text') {
    const input = row.querySelector('input[type="text"]');
    text = input ? input.value.trim() : '';
    if (!text) {
      toast('请填写成就 ID', 'err');
      return;
    }
  } else if (action.arg === 'fixed') {
    text = action.text || '';
  }

  if (action.danger) {
    const isAchievement = action.key.startsWith('ach_');
    const message = isAchievement
      ? '会永久修改 Steam 成就 可能违反 Steam 服务条款 且无法撤销 是否继续'
      : '这是实验性危险操作 可能修改存档或切换场景 可能失败或崩溃 是否继续';
    const ok = await confirmBox('危险操作', message);
    if (!ok) return;
  }

  state.specialBusy = true;
  try {
    const res = await api('/api/special', { key: action.key, value, value2: 0, text });
    if (!res.ok) throw new Error(res.msg || '执行失败');
    toast('已执行 ' + action.label, 'ok');
  } catch (e) {
    toast('执行失败 ' + e.message, 'err');
  } finally {
    state.specialBusy = false;
  }
}

if ('scrollRestoration' in history) history.scrollRestoration = 'manual';

(async function init() {
  window.scrollTo(0, 0);
  renderLiveCategories();
  renderFeatureSwitches();
  renderSpecialGroups();
  enhanceSelect($('bossSelect'));
  enhanceSelect($('bossMethod'));
  enhanceSelect($('bondLevel'));
  setPage('live');
  runAutoSelfCheck();
  try {
    await loadMeta();
    await loadBosses();
    await loadSaves(false);
  } catch (e) {
    toast('无法连接本地服务 ' + e.message, 'err');
  }
  setInterval(loadTrainerStatus, 3000);
  window.scrollTo(0, 0);
})();
