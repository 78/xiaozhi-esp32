const state = { sessions: [], id: null, events: [], selectedSeq: null, hasMore: false, audioSignature: '', liveTimer: null };
const byId = id => document.getElementById(id);
const formatTime = value => value ? new Date(value).toLocaleString() : '';
const api = async path => {
  const response = await fetch(path, { cache: 'no-store' });
  if (!response.ok) throw new Error(`${response.status}: ${await response.text()}`);
  return response.json();
};

function renderSessions() {
  const list = byId('session-list');
  list.replaceChildren();
  byId('session-count').textContent = `${state.sessions.length} 个会话`;
  if (!state.sessions.length) {
    list.innerHTML = '<div class="empty">暂无采集记录</div>';
    return;
  }
  for (const item of state.sessions) {
    const button = document.createElement('button');
    button.className = `session-item${item.id === state.id ? ' active' : ''}`;
    const name = document.createElement('span');
    name.className = 'session-name';
    name.textContent = item.device_id || item.trace_session_id;
    const meta = document.createElement('span');
    meta.className = 'session-meta';
    const status = item.truncated ? '截断' : item.live ? '采集中' : item.interrupted ? '采集中断' : '已结束';
    meta.textContent = `${formatTime(item.started_at)} · ${item.mode || '未标记'} · ${status} · ${item.events} 条`;
    if (item.gaps) meta.classList.add('warn');
    button.append(name, meta);
    button.onclick = () => selectSession(item.id);
    list.append(button);
  }
}

async function refreshSessions() {
  try {
    state.sessions = await api('/api/sessions');
    renderSessions();
    if (state.id && !state.sessions.some(item => item.id === state.id)) state.id = null;
    if (!state.id && state.sessions.length) await selectSession(state.sessions[0].id);
  } catch (error) {
    byId('session-list').textContent = `读取会话失败：${error.message}`;
  }
}

async function refreshStatus() {
  try {
    const status = await api('/api/status');
    byId('live-dot').classList.toggle('online', status.connected);
    byId('live-label').textContent = status.connected ? `${status.device_id || '设备'} 已连接` : '等待设备';
  } catch {
    byId('live-dot').classList.remove('online');
    byId('live-label').textContent = '服务已断开';
  }
}

async function selectSession(id) {
  state.id = id;
  state.selectedSeq = null;
  state.audioSignature = '';
  renderSessions();
  const info = await api(`/api/sessions/${encodeURIComponent(id)}`);
  byId('session-title').textContent = info.summary.device_id || info.summary.trace_session_id;
  byId('session-subtitle').textContent = `${formatTime(info.summary.started_at)} · ${info.summary.events} 条事件 · ${info.summary.gaps} 个缺口`;
  byId('export').href = `/api/sessions/${encodeURIComponent(id)}/export`;
  byId('export').hidden = false;
  renderAudio(info);
  await refreshEvents();
  byId('detail').innerHTML = '<div class="empty">选择时间线中的事件</div>';
}

function renderAudio(info) {
  const signature = JSON.stringify([info.uplink, info.playback].map(segments => (segments || []).map(segment => [segment.index, segment.length, segment.sample_rate, segment.channels, segment.bits_per_sample])));
  if (signature === state.audioSignature) return;
  state.audioSignature = signature;
  const root = byId('audio-tracks');
  root.replaceChildren();
  for (const [track, label] of [['uplink', '上行 PCM'], ['playback', '播放 PCM']]) {
    for (const segment of info[track] || []) {
      const row = document.createElement('div');
      row.className = 'audio-row';
      const title = document.createElement('span');
      title.textContent = `${label} ${segment.index + 1}`;
      const audio = document.createElement('audio');
      audio.controls = true;
      audio.preload = 'none';
      audio.src = `/api/sessions/${encodeURIComponent(state.id)}/audio/${track}/${segment.index}.wav`;
      row.append(title, audio);
      root.append(row);
    }
  }
  root.hidden = !root.childElementCount;
}

async function refreshEvents(more = false) {
  if (!state.id) return;
  try {
    const after = more && state.events.length ? `&after=${encodeURIComponent(state.events[state.events.length - 1].meta.seq)}` : '';
    const page = await api(`/api/sessions/${encodeURIComponent(state.id)}/events?limit=200${after}`);
    state.events = more ? state.events.concat(page) : page;
    state.hasMore = page.length === 200;
    byId('load-more').hidden = !state.hasMore;
    renderEvents();
  } catch (error) {
    byId('event-list').textContent = `读取事件失败：${error.message}`;
  }
}

async function refreshCurrentEvents() {
  if (!state.id) return;
  if (!state.events.length) {
    await refreshEvents();
    return;
  }
  const after = encodeURIComponent(state.events[state.events.length - 1].meta.seq);
  const page = await api(`/api/sessions/${encodeURIComponent(state.id)}/events?limit=200&after=${after}`);
  if (page.length) {
    state.events.push(...page);
    state.hasMore = page.length === 200;
    byId('load-more').hidden = !state.hasMore;
    renderEvents();
  }
}

function renderEvents() {
  const root = byId('event-list');
  root.replaceChildren();
  const search = byId('search').value.trim().toLowerCase();
  const kind = byId('kind').value;
  const events = state.events.filter(event => {
    if (kind && event.kind !== kind) return false;
    return !search || [event.kind, event.meta.event_type, event.meta.response_id, event.meta.detail, event.meta.message_id].some(value => String(value || '').toLowerCase().includes(search));
  });
  if (!events.length) {
    root.innerHTML = '<div class="empty">没有匹配的事件</div>';
    return;
  }
  for (const event of events) {
    const button = document.createElement('button');
    button.className = `event-item${event.meta.seq === state.selectedSeq ? ' active' : ''}`;
    const time = document.createElement('span');
    time.className = 'event-time';
    time.textContent = event.meta.mono_us ? `${(event.meta.mono_us / 1000000).toFixed(3)}s` : new Date(event.received_at).toLocaleTimeString();
    const type = document.createElement('span');
    type.className = `event-kind${event.kind === 'ws_rx' ? ' rx' : event.kind === 'trace_gap' ? ' gap' : ''}`;
    type.textContent = event.kind;
    const detail = document.createElement('span');
    detail.className = 'event-text';
    detail.textContent = event.meta.event_type || event.meta.detail || event.meta.response_id || event.meta.message_id || '';
    const size = document.createElement('span');
    size.className = 'event-size';
    size.textContent = event.payload_length || event.audio_length ? `${event.payload_length || event.audio_length} B` : '';
    button.append(time, type, detail, size);
    button.onclick = () => selectEvent(event);
    root.append(button);
  }
}

async function selectEvent(event) {
  state.selectedSeq = event.meta.seq;
  renderEvents();
  const root = byId('detail');
  root.replaceChildren();
  const add = (label, value) => {
    const heading = document.createElement('div');
    heading.className = 'detail-label';
    heading.textContent = label;
    const text = document.createElement('div');
    text.className = 'detail-value';
    text.textContent = value || '—';
    root.append(heading, text);
  };
  add('事件', event.kind);
  add('序号 / 设备时间', `${event.meta.seq} / ${event.meta.mono_us || 0} µs`);
  add('接收时间', formatTime(event.received_at));
  add('回复 / 消息 ID', `${event.meta.response_id || '—'} / ${event.meta.message_id || '—'}`);
  add('连接代次', String(event.meta.connection_generation || 0));
  const heading = document.createElement('div');
  heading.className = 'detail-label';
  heading.textContent = '元数据';
  const pre = document.createElement('pre');
  pre.textContent = JSON.stringify(event.raw_meta || event.meta, null, 2);
  root.append(heading, pre);
  if (event.payload_length) {
    const link = document.createElement('a');
    link.href = `/api/sessions/${encodeURIComponent(state.id)}/events/payload/${encodeURIComponent(event.meta.seq)}`;
    link.download = `${event.kind}-${event.meta.seq}.bin`;
    link.textContent = event.meta.part_count > 1 ? `下载完整原始消息 (${event.meta.part_count} 片)` : `下载原始消息 (${event.payload_length} B)`;
    root.append(link);
    if (event.payload_length < 65536 && /^(text\/|application\/json)/.test(event.meta.content_type || '')) {
      try {
        const response = await fetch(link.href);
        const raw = await response.text();
        const view = document.createElement('pre');
        try { view.textContent = JSON.stringify(JSON.parse(raw), null, 2); }
        catch { view.textContent = raw; }
        root.append(view);
      } catch { /* raw download remains available */ }
    }
  }
}

byId('refresh').onclick = refreshSessions;
byId('load-more').onclick = () => refreshEvents(true);
byId('search').oninput = renderEvents;
byId('kind').onchange = renderEvents;
const live = new EventSource('/api/live');
live.onopen = refreshStatus;
live.onerror = () => { byId('live-dot').classList.remove('online'); byId('live-label').textContent = '服务重连中'; };
live.addEventListener('update', () => {
  if (state.liveTimer) return;
  state.liveTimer = setTimeout(async () => {
    state.liveTimer = null;
    await refreshSessions();
    await refreshStatus();
    if (state.id) {
      await refreshCurrentEvents();
      const info = await api(`/api/sessions/${encodeURIComponent(state.id)}`);
      renderAudio(info);
    }
  }, 500);
});
refreshSessions();
refreshStatus();
