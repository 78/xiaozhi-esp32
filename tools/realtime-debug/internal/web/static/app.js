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
    if(state.sessions.some(other=>other.device_boot_id===item.boot_id && other.device_trace_id===item.trace_session_id)) continue;
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
    byId('live-label').textContent = status.connected ? `代理${status.proxy_connected?'已连接':'未连接'} · 诊断${status.collector_connected?'已连接':'未连接'}` : '等待设备';
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
  byId('session-subtitle').textContent = `${formatTime(info.summary.started_at)} · ${info.summary.events} 条事件 · ${info.summary.gaps} 个缺口 · reqid: ${info.summary.request_id || '未提供'}`;
  byId('export').href = `/api/sessions/${encodeURIComponent(id)}/export`;
  byId('export').hidden = false;
  renderAudio(info);
  await refreshEvents();
  byId('detail').innerHTML = '<div class="empty">选择时间线中的事件</div>';
}

function renderAudio(info) {
  renderConversation(info.conversation||[]);
  const tracks = [['input-left', 'input', 'ES8311 原始输入'], ['uplink', 'uplink', '云端上行'], ['playback', 'playback', '云端下行（非实际播放）']];
  const signature = JSON.stringify(tracks.map(([, source]) => (info[source] || []).map(segment => [segment.index, segment.length, segment.sample_rate, segment.channels, segment.bits_per_sample])));
  if (signature === state.audioSignature) return;
  state.audioSignature = signature;
  const root = byId('audio-tracks');
  root.replaceChildren();
  for (const [track, source, label] of tracks) {
    for (const segment of info[source] || []) {
      const row = document.createElement('div');
      row.className = 'audio-row';
      const title = document.createElement('span');
      title.textContent = `${label} ${segment.index + 1}${segment.gap_before?' · 暂停 / 缺口后':''}${segment.response_id?' · '+segment.response_id:''}`;
      const audio = document.createElement('audio');
      audio.controls = true;
      audio.preload = 'none';
      audio.src = `/api/sessions/${encodeURIComponent(source==='input' ? (info.input_session_id||state.id) : state.id)}/audio/${track}/${segment.index}.wav`;
      const sourceID=source==='input'?(info.input_session_id||state.id):state.id;
      const waveButton=document.createElement('button');waveButton.className='wave-button';waveButton.textContent='波形';
      waveButton.onclick=async()=>{try{
        const data=await api(`/api/sessions/${encodeURIComponent(sourceID)}/waveform/${source}/${segment.index}`);
        const canvas=document.createElement('canvas');canvas.width=600;canvas.height=70;canvas.style.width='100%';canvas.style.height='70px';
        const ctx=canvas.getContext('2d');ctx.strokeStyle='#176b66';ctx.beginPath();
        data.peaks.forEach((peak,i)=>{const x=i*600/data.peaks.length;ctx.moveTo(x,35-peak[0]*32);ctx.lineTo(x,35-peak[1]*32)});ctx.stroke();
        waveButton.replaceWith(canvas);
      }catch(error){waveButton.textContent='读取波形失败'}};
      row.append(title, audio, waveButton);
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

function renderConversation(events) {
  const conversation=byId('conversation');conversation.replaceChildren();
  for(const event of events.filter(e=>e.meta.transcript)) {
    const bubble=document.createElement('article');bubble.className='bubble '+event.meta.role;
    const label=document.createElement('small');label.textContent=event.meta.role==='user'?'你':'助手';
    const text=document.createElement('p');text.textContent=event.meta.transcript;
    bubble.append(label,text);conversation.append(bubble);
  }
  if(!conversation.childElementCount)conversation.innerHTML='<div class="empty">暂无转写文字，可查看音轨和事件</div>';
}

function renderEvents() {
  const root = byId('event-list');
  root.replaceChildren();
  const search = byId('search').value.trim().toLowerCase();
  const kind = byId('kind').value;
  const events = state.events.filter(event => {
    if (!kind && !search && (event.kind.startsWith('pcm_') || event.meta.event_type==='input_audio_buffer.append' || event.meta.event_type==='response.audio.delta')) return false;
    if (kind && event.kind !== kind) return false;
    return !search || [event.kind, event.meta.event_type, event.meta.request_id, event.meta.session_id, event.meta.response_id, event.meta.detail, event.meta.message_id].some(value => String(value || '').toLowerCase().includes(search));
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
  document.querySelector('.detail-pane').hidden=false;
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
  add('reqid', event.meta.request_id);
  add('云端会话 ID', event.meta.session_id);
  add('关联设备诊断会话', event.meta.device_trace_id);
  add('转发结果', event.meta.forward_error || (event.meta.forward_done_at ? '已写入对端连接' : '—'));
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

byId('close-detail').onclick=()=>document.querySelector('.detail-pane').hidden=true;
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
