const MODE_NAMES = ['Active', 'Passive', 'Standby'];
const CLASS_NAMES = {
  0:'Novice',1:'Swordman',2:'Mage',3:'Archer',4:'Acolyte',5:'Merchant',6:'Thief'
};
const STAT_NAMES = ['str','agi','vit','int','dex','luk'];
const STAT_LABELS = ['STR','AGI','VIT','INT','DEX','LUK'];

const SKILL_MAP = {
   1:{en:'NV_Basic',zh:'基本技能'},   5:{en:'Bash',zh:'狂击'},
   6:{en:'Provoke',zh:'挑衅'},       7:{en:'Magnum Break',zh:'怒爆'},
  11:{en:'Napalm Beat',zh:'石化术'}, 13:{en:'Soul Strike',zh:'灵魂攻击'},
  14:{en:'Cold Bolt',zh:'冰箭术'},  17:{en:'Fire Ball',zh:'火球术'},
  19:{en:'Fire Bolt',zh:'火箭术'},  20:{en:'Lightning Bolt',zh:'雷击术'},
  25:{en:'Pneuma',zh:'光猎'},       26:{en:'Teleport',zh:'瞬间移动'},
  28:{en:'Heal',zh:'治愈术'},       29:{en:'Increase AGI',zh:'加速术'},
  32:{en:'Angelus',zh:'天使之护'},  33:{en:'Blessing',zh:'天使之赐福'},
  34:{en:'Blessing',zh:'天使之赐福'},46:{en:'Double Strafe',zh:'二连矢'},
  48:{en:'Improvise Concentration',zh:'心神凝聚'},
  54:{en:'Resurrection',zh:'复活术'},
  62:{en:'Bowling Bash',zh:'怪物互击'},
  73:{en:'Kyrie Eleison',zh:'圣母之祈福'},
  75:{en:'Gloria',zh:'幸运之颂歌'},
  79:{en:'Magnus Exorcismus',zh:'十字驱魔'},
 111:{en:'Adrenaline Rush',zh:'速度激发'},
 112:{en:'Weapon Perfection',zh:'武器精炼'},
 138:{en:'Enchant Poison',zh:'涂毒'},
 361:{en:'Assumptio',zh:'霸邪之阵'},
 383:{en:'Wind Walker',zh:'风之步'},
 879:{en:'First Aid',zh:'急救'},
 880:{en:'Trick Dead',zh:'装死'},
};

function skillDisplayName(id) {
  const s = SKILL_MAP[id];
  return s ? `${s.zh} (${s.en})` : `Skill ${id}`;
}

// Skill name: use server-provided name, fallback to id
function skillName(sk) { return sk.name || 'Skill ' + sk.id; }

function $(id){ return document.getElementById(id); }
function qs(s){ return document.querySelector(s); }
function qsa(s){ return document.querySelectorAll(s); }

const SC_CONSTANTS = [
  {v:30, n:'Blessing'}, {v:32, n:'Inc AGI'}, {v:39, n:'Kyrie'},
  {v:348, n:'Assumptio'}, {v:273, n:'Adrenaline Rush'}, {v:280, n:'Weapon Perfection'},
  {v:281, n:'Enchant Poison'}, {v:288, n:'Wind Walker'}
];
const SC_TO_SKILL = {30:33, 32:29, 39:73, 348:361, 273:111, 280:112, 281:138, 288:383};

let autoTimer = null;
let currentAid = 0;
let currentBotData = null;

function showMsg(msg,type){
  const el=$('status');
  el.textContent=msg; el.className='status-msg '+type;
}
function hideMsg(){ $('status').className='status-msg hidden'; }
function jobName(cls){ return CLASS_NAMES[cls]||'Class '+cls; }

async function fetchStatus(aid){
  hideMsg();
  try{
    const r=await fetch('/api/bot/status?aid='+aid);
    const j=await r.json();
    if(j.code!==0){ showMsg(j.msg||'Bot not found','error'); return null; }
    return j.data;
  }catch(e){ showMsg('Connection error: '+e.message,'error'); return null; }
}

async function sendCmd(aid,cmd,params){
  hideMsg();
  try{
    const r=await fetch('/api/bot/cmd',{
      method:'POST', headers:{'Content-Type':'application/json'},
      body:JSON.stringify({aid,cmd,params:params||{}})
    });
    return await r.json();
  }catch(e){ showMsg('Connection error: '+e.message,'error'); return null; }
}

function renderOverview(data){
  $('bot-name').textContent=data.bot_name;
  // Exp bars
  const basePct=data.next_base_exp>0?Math.round(data.base_exp/data.next_base_exp*100):0;
  const jobPct=data.next_job_exp>0?Math.round(data.job_exp/data.next_job_exp*100):0;
  $('baseExpBar').style.width=basePct+'%';
  $('baseExpText').textContent=data.base_exp.toLocaleString()+' / '+data.next_base_exp.toLocaleString()+' ('+basePct+'%)';
  $('jobExpBar').style.width=jobPct+'%';
  $('jobExpText').textContent=data.job_exp.toLocaleString()+' / '+data.next_job_exp.toLocaleString()+' ('+jobPct+'%)';

  $('lv').textContent=data.base_level+'/'+data.job_level;
  $('job').textContent=jobName(data.class);
  $('pos').textContent=data.map+' ('+data.x+','+data.y+')';
  $('zeny').textContent=data.zeny.toLocaleString();
  $('mode').textContent=MODE_NAMES[data.ai_mode]||data.ai_mode;

  const hpPct=Math.round(data.hp/data.max_hp*100);
  const spPct=Math.round(data.sp/data.max_sp*100);
  $('hpBar').style.width=hpPct+'%';
  $('spBar').style.width=spPct+'%';
  $('hpText').textContent=data.hp+' / '+data.max_hp;
  $('spText').textContent=data.sp+' / '+data.max_sp;

  qsa('.mode').forEach(b=>b.classList.toggle('active',parseInt(b.dataset.mode)===data.ai_mode));
}

function renderStats(data){
  $('sp_pts').textContent=data.status_point;
  const el=$('stat-list');
  el.innerHTML='';
  STAT_NAMES.forEach((s,i)=>{
    const val=data[s];
    const pct=Math.min(100,Math.round(val/99*100));
    const row=document.createElement('div');
    row.className='stat-row';
    row.innerHTML=`
      <span class="stat-label">${STAT_LABELS[i]}</span>
      <div class="stat-bar"><div class="stat-fill" style="width:${pct}%"></div></div>
      <span class="stat-val">${val}</span>
      <div class="stat-add">
        <button data-stat="${s}" data-amt="1" ${data.status_point<1?'disabled':''}>+1</button>
        <button data-stat="${s}" data-amt="5" ${data.status_point<5?'disabled':''}>+5</button>
        <button data-stat="${s}" data-amt="10" ${data.status_point<10?'disabled':''}>+10</button>
      </div>`;
    el.appendChild(row);
  });
  el.querySelectorAll('.stat-add button').forEach(b=>{
    b.addEventListener('click',async e=>{
      const stat=e.target.dataset.stat;
      const amt=parseInt(e.target.dataset.amt);
      const r=await sendCmd(currentAid,'statusup',{stat,amount:amt});
      if(r&&r.code===0){
        showMsg('+'+amt+' '+stat.toUpperCase(),'info');
        refresh();
      }else showMsg(r?r.msg:'Failed','error');
    });
  });
}

let skillsTabData = null;
let skillsTabActive = 0;

async function renderSkills(data){
  $('sk_pts').textContent=data.skill_point;
  const el=$('skill-list');
  el.innerHTML='';

  const r = await sendCmd(currentAid, 'get_class_skills');
  if (!r || r.code !== 0 || !r.data || r.data.length === 0) {
    el.innerHTML='<p style="color:#888;font-size:13px;">Failed to load class skills.</p>';
    return;
  }
  skillsTabData = r.data;

  // Create sub-tabs
  const tabBar = document.createElement('div');
  tabBar.className = 'sub-tabs';
  r.data.forEach((g, i) => {
    const btn = document.createElement('button');
    btn.textContent = g.job_name;
    btn.dataset.idx = i;
    if (i === skillsTabActive) btn.classList.add('active');
    btn.addEventListener('click', () => {
      skillsTabActive = i;
      renderSkillGroup(i);
      tabBar.querySelectorAll('button').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
    });
    tabBar.appendChild(btn);
  });
  el.appendChild(tabBar);

  // Render initial group
  renderSkillGroup(skillsTabActive);
}

function renderSkillGroup(idx) {
  const group = skillsTabData[idx];
  if (!group) return;
  const container = $('skill-list');

  // Remove old skill rows (keep sub-tab bar)
  while (container.children.length > 1) container.removeChild(container.lastChild);

  const frag = document.createDocumentFragment();
  group.skills.forEach(sk => {
    const row = document.createElement('div');
    row.className = 'skill-bar';

    const zh = SKILL_MAP[sk.id]?.zh || '';
    const fullName = zh ? `${zh} (${sk.name})` : (sk.name || 'Skill '+sk.id);
    const pct = sk.max_lv > 0 ? Math.round(sk.cur_lv / sk.max_lv * 100) : 0;
    const isMax = sk.cur_lv >= sk.max_lv;

    // Prereq info
    let prereqHtml = '';
    let prereqOk = true;
    sk.prereqs.forEach(p => {
      const ok = p.cur_lv >= p.lv;
      if (!ok) prereqOk = false;
      const pname = SKILL_MAP[p.id]?.zh || skillDisplayName(p.id);
      prereqHtml += `<span class="${ok?'req-ok':'req-miss'}">${pname} Lv.${p.lv}${ok?'✓':'✗'}</span> `;
    });

    // Level requirement checks
    let levelReqOk = true;
    let levelReqHtml = '';
    if (sk.baselv > 0 || sk.joblv > 0) {
      if (sk.baselv > 0 && currentBotData.base_level < sk.baselv) levelReqOk = false;
      if (sk.joblv > 0 && currentBotData.job_level < sk.joblv) levelReqOk = false;
      levelReqHtml = `需求: Lv.${sk.baselv}/${sk.joblv} `;
    }

    const canLearn = !isMax && levelReqOk && prereqOk && currentBotData.skill_point > 0;

    let disableReason = '';
    if (isMax) disableReason = 'MAX';
    else if (!levelReqOk) disableReason = '等级不足';
    else if (!prereqOk) disableReason = '前置不足';
    else if (currentBotData.skill_point < 1) disableReason = '技能点不足';

    row.innerHTML = `
      <span class="sk-name">${fullName}</span>
      <div class="sk-prog"><div class="sk-prog-fill ${isMax?'sk-max-fill':''}" style="width:${pct}%"></div></div>
      <span class="sk-lv">${isMax ? 'MAX' : `Lv.${sk.cur_lv}/${sk.max_lv}`}</span>
      <button data-id="${sk.id}" ${canLearn?'':'disabled'} title="${disableReason}">${canLearn?'Learn +1':disableReason}</button>
      <div class="sk-info">${levelReqHtml}${prereqHtml ? '前置: ' + prereqHtml : ''}</div>`;

    row.querySelector('button').addEventListener('click', async e => {
      const id = parseInt(e.target.dataset.id);
      const r2 = await sendCmd(currentAid, 'learn_skill', {skill_id: id});
      if (r2 && r2.code === 0) {
        showMsg('Learned ' + (SKILL_MAP[id]?.zh || 'Skill ' + id) + ' +1', 'info');
        refresh();
      } else {
        showMsg(r2 ? r2.msg : 'Failed', 'error');
        refresh();
      }
    });

    frag.appendChild(row);
  });
  container.appendChild(frag);
}

function renderAll(data){
  currentAid=parseInt($('aid').value);
  currentBotData=data;
  $('bot-detail').classList.remove('hidden');
  renderOverview(data);
  renderStats(data);
  renderSkills(data);
  renderInventory();
  renderRules();
}

async function refresh(){
  const aid=parseInt($('aid').value);
  if(!aid){ showMsg('Enter Account ID','error'); return; }
  currentAid=aid;
  const data=await fetchStatus(aid);
  if(data) renderAll(data);
}

// Tab mapping
const TAB_MAP = {overview:'tab-overview',stats:'tab-stats',skills:'tab-skills',inventory:'tab-inventory',rules:'tab-rules'};
qsa('.tab').forEach(t=>{
  t.addEventListener('click',()=>{
    qsa('.tab').forEach(x=>x.classList.remove('active'));
    qsa('.tab-content').forEach(x=>x.classList.remove('active'));
    t.classList.add('active');
    $(TAB_MAP[t.dataset.tab]||'tab-overview').classList.add('active');
    if (t.dataset.tab==='rules') renderRules();
    if (t.dataset.tab==='inventory') renderInventory();
  });
});

// ─── Rules Editor ──────────────────────────────────────

const SRC_NAMES = ['Master','Bot'];
const COND_TYPE_NAMES = ['HP%','SP%','SC Missing','SC Active','Is Dead','Dist >','Dist <','Has Enemy'];
const COND_TYPE_SC_MISS = 2;
const COND_TYPE_SC_ACTIVE = 3;
const COND_TYPE_DEAD = 4;
const OP_NAMES = ['<','>','==','!=','<=','>='];

async function fetchRules() {
  const r = await sendCmd(currentAid,'get_rules');
  return r && r.code === 0 ? r.data : [];
}

function renderRules() {
  fetchRules().then(rules => {
    const el = $('rules-container');
    el.innerHTML = '';
    if (!rules || rules.length === 0) {
      el.innerHTML = '<p style="color:#888;font-size:13px;">No rules defined. Click "+ New Rule" to create one.</p>';
      return;
    }
    rules.forEach((rule, i) => {
      el.appendChild(renderRuleCard(rule, i));
    });
  });
}

function renderRuleCard(rule, idx) {
  const card = document.createElement('div');
  card.className = 'rule-card';
  card.dataset.idx = idx;

  // Header
  card.draggable = true;
  card.addEventListener('dragstart', () => card.classList.add('dragging'));
  card.addEventListener('dragend', () => card.classList.remove('dragging'));
  card.addEventListener('dragover', e => {
    e.preventDefault();
    const container = $('rules-container');
    const dragging = container.querySelector('.dragging');
    if (!dragging || dragging === card) return;
    const rect = card.getBoundingClientRect();
    const mid = rect.top + rect.height / 2;
    if (e.clientY < mid) {
      container.insertBefore(dragging, card);
    } else {
      container.insertBefore(dragging, card.nextSibling);
    }
  });

  const hdr = document.createElement('div');
  hdr.className = 'rule-header';
  hdr.innerHTML = `
    <span class="drag-handle">&#x2630;</span>
    <input type="text" class="r-name" value="${escHtml(rule.name)}" placeholder="Rule name">
    <label style="font-size:12px;color:#aaa;"><input type="checkbox" class="r-enabled" ${rule.enabled?'checked':''}> Enabled</label>
    <button class="r-del" title="Delete rule">&#x2715;</button>`;
  hdr.querySelector('.r-del').addEventListener('click', () => {
    card.remove();
  });
  card.appendChild(hdr);

  // Condition logic (AND/OR)
  const logicDiv = document.createElement('div');
  logicDiv.className = 'cond-logic';
  logicDiv.innerHTML = `
    <label><input type="radio" name="cl_${idx}" value="0" ${rule.cond_logic===0?'checked':''}> ALL (AND)</label>
    <label><input type="radio" name="cl_${idx}" value="1" ${rule.cond_logic===1?'checked':''}> ANY (OR)</label>`;
  card.appendChild(logicDiv);

  // Conditions
  const condDiv = document.createElement('div');
  condDiv.innerHTML = '<div style="font-size:12px;color:#888;margin:4px 0;">Conditions</div>';
  const condList = document.createElement('div');
  rule.conditions.forEach((c, ci) => addConditionRow(condList, c, ci));
  condDiv.appendChild(condList);
  const addCondBtn = document.createElement('button');
  addCondBtn.className = 'add-cond';
  addCondBtn.textContent = '+ Condition';
  addCondBtn.addEventListener('click', () => {
    const c = {source:0, type:0, op:0, value:50, extra:0};
    addConditionRow(condList, c, condList.children.length);
  });
  condDiv.appendChild(addCondBtn);
  card.appendChild(condDiv);

  // Actions
  const actDiv = document.createElement('div');
  actDiv.innerHTML = '<div style="font-size:12px;color:#888;margin:8px 0 4px;">Actions</div>';
  const actList = document.createElement('div');
  rule.actions.forEach((a, ai) => addActionRow(actList, a, ai));
  actDiv.appendChild(actList);
  const addActBtn = document.createElement('button');
  addActBtn.className = 'add-act';
  addActBtn.textContent = '+ Action';
  addActBtn.addEventListener('click', () => {
    const a = {type:0, target:0, skill_id:28, message:''};
    addActionRow(actList, a, actList.children.length);
  });
  actDiv.appendChild(addActBtn);
  card.appendChild(actDiv);

  return card;
}

function addConditionRow(container, cond, idx) {
  const row = document.createElement('div');
  row.className = 'cond-row';
  row.dataset.idx = idx;

  const isScType = cond.type >= COND_TYPE_SC_MISS && cond.type <= COND_TYPE_SC_ACTIVE;
  const extraSelectOpts = SC_CONSTANTS.map(sc => {
    const sk = SKILL_MAP[SC_TO_SKILL[sc.v]];
    const label = sk ? `${sk.zh} (${sk.en})` : `${sc.n} (${sc.v})`;
    return `<option value="${sc.v}" ${cond.extra===sc.v?'selected':''}>${label}</option>`;
  }).join('');

  row.innerHTML = `
    <select class="c-src">
      ${SRC_NAMES.map((n,i)=>`<option value="${i}" ${cond.source===i?'selected':''}>${n}</option>`).join('')}
    </select>
    <select class="c-type">
      ${COND_TYPE_NAMES.map((n,i)=>`<option value="${i}" ${cond.type===i?'selected':''}>${n}</option>`).join('')}
    </select>
    <select class="c-op" style="display:${isScType?'none':'inline-block'}">
      ${OP_NAMES.map((n,i)=>`<option value="${i}" ${cond.op===i?'selected':''}>${n}</option>`).join('')}
    </select>
    <input type="number" class="c-val" value="${cond.value}" min="0" max="999" style="display:${isScType?'none':'inline-block'}" style="width:70px;">
    <select class="c-extra" style="display:${isScType?'inline-block':'none'};min-width:140px;">
      ${extraSelectOpts}
    </select>
    <button class="rm" title="Remove">&#x2715;</button>`;
  
  // Show/hide extra field for SC types
  const typeSel = row.querySelector('.c-type');
  const opSel = row.querySelector('.c-op');
  const valInp = row.querySelector('.c-val');
  const extraSel = row.querySelector('.c-extra');
  typeSel.addEventListener('change', () => {
    const t = parseInt(typeSel.value);
    const sc = t >= COND_TYPE_SC_MISS && t <= COND_TYPE_SC_ACTIVE;
    opSel.style.display = sc ? 'none' : 'inline-block';
    valInp.style.display = sc ? 'none' : 'inline-block';
    extraSel.style.display = sc ? 'inline-block' : 'none';
  });

  row.querySelector('.rm').addEventListener('click', () => row.remove());
  container.appendChild(row);
}

function addActionRow(container, act, idx) {
  const row = document.createElement('div');
  row.className = 'act-row';
  row.dataset.idx = idx;
  const ACT_TYPE_NAMES = ['Use Skill','Attack Nearest','Recall','Say'];

  // Build skill dropdown from currentBotData.skills
  let skillOpts = '<option value="0">-- Skill --</option>';
  if (currentBotData && currentBotData.skills) {
    currentBotData.skills.forEach(sk => {
      const sel = act.skill_id === sk.id ? 'selected' : '';
      skillOpts += `<option value="${sk.id}" ${sel}>${skillDisplayName(sk.id)}</option>`;
    });
  } else {
    skillOpts += `<option value="${act.skill_id}" selected>${skillDisplayName(act.skill_id)}</option>`;
  }

  row.innerHTML = `
    <select class="a-type">
      ${ACT_TYPE_NAMES.map((n,i)=>`<option value="${i}" ${act.type===i?'selected':''}>${n}</option>`).join('')}
    </select>
    <select class="a-tgt" style="display:${act.type===0?'':'none'}">
      ${SRC_NAMES.map((n,i)=>`<option value="${i}" ${act.target===i?'selected':''}>${n}</option>`).join('')}
    </select>
    <select class="a-skid" style="display:${act.type===0?'':'none'};min-width:140px;">
      ${skillOpts}
    </select>
    <input type="text" class="a-msg" value="${escHtml(act.message)}" placeholder="Message" style="flex:1;display:${act.type===3?'':'none'}">
    <button class="rm" title="Remove">&#x2715;</button>`;

  const typeSel = row.querySelector('.a-type');
  const tgtSel = row.querySelector('.a-tgt');
  const skidSel = row.querySelector('.a-skid');
  const msgInp = row.querySelector('.a-msg');
  
  function toggleFields() {
    const t = parseInt(typeSel.value);
    tgtSel.style.display = (t === 0) ? 'inline-block' : 'none';
    skidSel.style.display = (t === 0) ? 'inline-block' : 'none';
    msgInp.style.display = (t === 3) ? 'inline-block' : 'none';
  }
  typeSel.addEventListener('change', toggleFields);
  toggleFields();

  row.querySelector('.rm').addEventListener('click', () => row.remove());
  container.appendChild(row);
}

function collectRules() {
  const cards = qs('#rules-container').querySelectorAll('.rule-card');
  const rules = [];
  cards.forEach(card => {
    const rule = {
      name: card.querySelector('.r-name').value,
      enabled: card.querySelector('.r-enabled').checked,
      cond_logic: parseInt(card.querySelector('input[type=radio]:checked').value),
      conditions: [],
      actions: []
    };
    card.querySelectorAll('.cond-row').forEach(row => {
      rule.conditions.push({
        source: parseInt(row.querySelector('.c-src').value),
        type: parseInt(row.querySelector('.c-type').value),
        op: parseInt(row.querySelector('.c-op').value),
        value: parseInt(row.querySelector('.c-val').value) || 0,
        extra: parseInt(row.querySelector('.c-extra').value) || 0
      });
    });
    card.querySelectorAll('.act-row').forEach(row => {
      const t = parseInt(row.querySelector('.a-type').value);
      rule.actions.push({
        type: t,
        target: t === 0 ? parseInt(row.querySelector('.a-tgt').value) : 0,
        skill_id: t === 0 ? parseInt(row.querySelector('.a-skid').value) || 0 : 0,
        message: t === 3 ? row.querySelector('.a-msg').value : ''
      });
    });
    rules.push(rule);
  });
  return rules;
}

async function saveRules() {
  const rules = collectRules();
  const r = await sendCmd(currentAid, 'save_rules', {rules});
  if (r && r.code === 0) {
    showMsg('Rules saved!','info');
    renderRules();
  } else {
    showMsg(r ? r.msg : 'Failed','error');
  }
}

function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

// Rules event listeners
$('newRule').addEventListener('click', () => {
  const el = $('rules-container');
  // Remove empty placeholder
  if (el.children.length === 1 && el.children[0].tagName === 'P') el.innerHTML = '';
  const rule = {
    name: 'New Rule',
    enabled: true,
    cond_logic: 0,
    conditions: [{source:0, type:0, op:0, value:50, extra:0}],
    actions: [{type:0, target:0, skill_id:28, message:''}]
  };
  el.appendChild(renderRuleCard(rule, el.children.length));
});
$('saveRules').addEventListener('click', saveRules);

// Mode buttons
qsa('.mode').forEach(b=>b.addEventListener('click',async e=>{
  const aid=parseInt($('aid').value);
  if(!aid) return;
  const mode=parseInt(e.target.dataset.mode);
  const r=await sendCmd(aid,'ai_mode',{mode});
  if(r&&r.code===0){ showMsg('Mode: '+MODE_NAMES[mode],'info'); refresh(); }
  else showMsg(r?r.msg:'Failed','error');
}));

$('refresh').addEventListener('click',refresh);
$('recall').addEventListener('click',async()=>{
  const aid=parseInt($('aid').value);
  if(!aid) return;
  const r=await sendCmd(aid,'recall');
  if(r&&r.code===0){ showMsg('Recalled!','info'); refresh(); }
  else showMsg(r?r.msg:'Failed','error');
});
$('revive').addEventListener('click',async()=>{
  const aid=parseInt($('aid').value);
  if(!aid) return;
  const r=await sendCmd(aid,'revive');
  if(r&&r.code===0){ showMsg('Revived!','info'); refresh(); }
  else showMsg(r?r.msg:'Failed','error');
});
$('destroy').addEventListener('click',async()=>{
  const aid=parseInt($('aid').value);
  if(!aid) return;
  if(!confirm('Destroy bot? Cannot be undone!')) return;
  const r=await sendCmd(aid,'destroy');
  if(r&&r.code===0){ showMsg('Destroyed!','info'); $('bot-detail').classList.add('hidden'); }
  else showMsg(r?r.msg:'Failed','error');
});

// Auto refresh toggle
$('autoRef').addEventListener('change',()=>{
  if($('autoRef').checked){
    autoTimer=setInterval(refresh,5000);
  }else{
    clearInterval(autoTimer); autoTimer=null;
  }
});

$('aid').addEventListener('keydown',e=>{ if(e.key==='Enter') refresh(); });

// ─── Inventory ──────────────────────────────────────

async function renderInventory() {
  const el = $('inventory-bot');
  el.innerHTML = '<p style="color:#888;">Loading...</p>';
  const r = await sendCmd(currentAid, 'get_inventory');
  if (!r || r.code !== 0) {
    el.innerHTML = '<p style="color:#c44;">Failed to load inventory.</p>';
    return;
  }

  const botItems = r.bot || [];
  const masterItems = r.master || [];

  // Render bot inventory
  if (botItems.length === 0) {
    el.innerHTML = '<p style="color:#888;">Bot inventory is empty.</p>';
  } else {
    let html = '<table class="inv-table"><tr><th>Item</th><th>Qty</th><th>Status</th><th>Actions</th></tr>';
    botItems.forEach(item => {
      const status = item.is_equipped ? '<span class="inv-equip">Equipped</span>' : '';
      let actions = '';
      if (item.is_equipped) {
        actions += `<button class="inv-unequip" data-idx="${item.index}">Unequip</button> `;
      } else if (item.type === 4 || item.type === 5) {
        actions += `<button class="inv-equip" data-idx="${item.index}">Equip</button> `;
      }
      if (item.type === 0 || item.type === 2) {
        actions += `<button class="inv-use" data-idx="${item.index}">Use</button> `;
      }
      actions += `<input type="number" class="take-amt" value="1" min="1" max="${item.amount}" style="width:50px;"> `;
      actions += `<button class="inv-take" data-idx="${item.index}" data-max="${item.amount}">Take</button>`;
      html += `<tr><td class="inv-name">${escHtml(item.name)}</td><td>${item.amount}</td><td>${status}</td><td class="inv-actions">${actions}</td></tr>`;
    });
    html += '</table>';
    el.innerHTML = html;
  }

  // Populate master inventory for Give section
  const sel = $('give-item-select');
  sel.innerHTML = '<option value="">-- Select item --</option>';
  if (masterItems.length === 0) {
    sel.innerHTML += '<option value="" disabled>Master inventory is empty</option>';
  } else {
    masterItems.forEach(item => {
      const opt = document.createElement('option');
      opt.value = item.index;
      opt.dataset.nameid = item.nameid;
      opt.dataset.max = item.amount;
      opt.textContent = `${item.name} (x${item.amount})`;
      sel.appendChild(opt);
    });
  }
}

// Delegate event listeners for inventory actions
document.getElementById('inventory-bot').addEventListener('click', async e => {
  const aid = currentAid;
  if (!aid) return;

  if (e.target.classList.contains('inv-equip')) {
    const idx = parseInt(e.target.dataset.idx);
    const r = await sendCmd(aid, 'equip', {index: idx});
    if (r && r.code === 0) { showMsg('Equipped!', 'info'); renderInventory(); }
    else showMsg(r ? r.msg : 'Failed', 'error');
  } else if (e.target.classList.contains('inv-unequip')) {
    const idx = parseInt(e.target.dataset.idx);
    const r = await sendCmd(aid, 'unequip', {index: idx});
    if (r && r.code === 0) { showMsg('Unequipped!', 'info'); renderInventory(); }
    else showMsg(r ? r.msg : 'Failed', 'error');
  } else if (e.target.classList.contains('inv-use')) {
    const idx = parseInt(e.target.dataset.idx);
    const r = await sendCmd(aid, 'use_item', {index: idx});
    if (r && r.code === 0) { showMsg('Item used!', 'info'); renderInventory(); }
    else showMsg(r ? r.msg : 'Failed', 'error');
  } else if (e.target.classList.contains('inv-take')) {
    const idx = parseInt(e.target.dataset.idx);
    const row = e.target.closest('tr');
    const amtInput = row ? row.querySelector('.take-amt') : null;
    const amount = amtInput ? parseInt(amtInput.value) || 1 : 1;
    const r = await sendCmd(aid, 'take_item', {index: idx, amount: amount});
    if (r && r.code === 0) { showMsg('Taken!', 'info'); renderInventory(); }
    else showMsg(r ? r.msg : 'Failed', 'error');
  }
});

$('give-item-btn').addEventListener('click', async () => {
  const aid = currentAid;
  if (!aid) return;
  const sel = $('give-item-select');
  const idx = parseInt(sel.value);
  if (isNaN(idx)) { showMsg('Select an item', 'error'); return; }
  const amount = parseInt($('give-item-amount').value) || 1;
  const maxAmt = parseInt(sel.options[sel.selectedIndex].dataset.max) || 1;
  if (amount > maxAmt) { showMsg(`Only ${maxAmt} available`, 'error'); return; }
  const nameid = parseInt(sel.options[sel.selectedIndex].dataset.nameid) || 0;
  const r = await sendCmd(aid, 'give_item', {index: idx, amount: amount, nameid: nameid});
  if (r && r.code === 0) {
    showMsg('Item given to bot!', 'info');
    renderInventory();
    $('give-item-select').value = '';
    $('give-item-amount').value = 1;
  } else {
    showMsg(r ? r.msg : 'Failed', 'error');
  }
});

// Load from URL
const p=new URLSearchParams(window.location.search);
if(p.has('aid')){ $('aid').value=p.get('aid'); refresh(); }
