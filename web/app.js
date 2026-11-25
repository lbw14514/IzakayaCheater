const $ = (id) => document.getElementById(id);

const state = { slot: null, boss: 0, method: 'B', busy: false, page: 'live', attachBusy: false };
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
    desc: '写入 finishedMissions 把最终战标记为已完成 等于跳过 需要自己去再战 不会开打' },
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
    if (st.game && !st.attached) {
      await api('/api/trainer/attach', {});
    }
  } catch (e) {
    // 状态轮询会显示具体错误
  }
  loadTrainerStatus();
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
    if (st.game && !st.attached && !state.attachBusy) {
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

if ('scrollRestoration' in history) history.scrollRestoration = 'manual';

(async function init() {
  window.scrollTo(0, 0);
  renderLiveCategories();
  renderFeatureSwitches();
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
