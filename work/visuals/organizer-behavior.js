(() => {
 const root=document.getElementById('flip2-landscape');
 const $=s=>root.querySelector(s), $$=s=>Array.from(root.querySelectorAll(s));
 const esc=value=>String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
 const canvas=$('.f2-canvas'),content=$('.f2-content'),views=$$('[data-view]'),nav=$$('[data-page]');
 const handle=$('.f2-handle'),bay=$('.f2-bay'),drawerShell=$('.f2-drawer-shell'),menu=$('.f2-menu'),resumes=$$('[data-resume]');
 const pageNames=['HOME','WORLDS','POKÉDEX','TRAINER','HALL OF FAME'];
 const worlds=['Kanto','Johto','Hoenn','Sinnoh','Unova','Kalos','Alola','Galar','Paldea'];
 const worldStates=['Champion','In progress','In progress','In progress','Not visited','Not visited','Not visited','Not visited','Not visited'];
 const rows=[[1,'Bulbasaur','Grass / Poison','Kanto'],[4,'Charmander','Fire','Kanto'],[7,'Squirtle','Water','Kanto'],[25,'Pikachu','Electric','Kanto,Johto,Hoenn,Sinnoh'],[133,'Eevee','Normal','Kanto,Johto,Sinnoh'],[152,'Chikorita','Grass','Johto'],[155,'Cyndaquil','Fire','Johto'],[158,'Totodile','Water','Johto'],[252,'Treecko','Grass','Hoenn'],[253,'Grovyle','Grass','Hoenn'],[254,'Sceptile','Grass','Hoenn'],[255,'Torchic','Fire','Hoenn'],[256,'Combusken','Fire / Fighting','Hoenn'],[257,'Blaziken','Fire / Fighting','Hoenn'],[258,'Mudkip','Water','Hoenn'],[259,'Marshtomp','Water / Ground','Hoenn'],[260,'Swampert','Water / Ground','Hoenn'],[261,'Poochyena','Dark','Hoenn'],[262,'Mightyena','Dark','Hoenn'],[263,'Zigzagoon','Normal','Hoenn'],[387,'Turtwig','Grass','Sinnoh'],[390,'Chimchar','Fire','Sinnoh'],[393,'Piplup','Water','Sinnoh'],[495,'Snivy','Grass','Unova'],[650,'Chespin','Grass','Kalos'],[722,'Rowlet','Grass / Flying','Alola'],[810,'Grookey','Grass','Galar'],[906,'Sprigatito','Grass','Paldea']];
 const pokemon=rows.map((p,i)=>({id:p[0],name:p[1],types:p[2].split(' / '),worlds:p[3].split(','),caught:i%3!==1,seen:i%5!==4||i%3!==1,favorite:[25,252,258].includes(p[0])}));
 const state={page:0,world:0,detail:false,dex:0,worldFilter:'All worlds',typeFilter:'All types',statusFilter:'All entries',query:'',sort:'Number',drawer:false,resume:0,menu:false,profile:null,draft:null,editor:false,theme:'turquoise',finish:'resin',wear:20,parallax:true};
 let filtered=[],pick=null,keyboard=null,menuReturn=null,noticeTimer;
 const picker=document.createElement('aside');picker.className='f2-picker';picker.hidden=true;picker.setAttribute('aria-label','Choose a category');$('.f2-field').append(picker);
 const keysPanel=document.createElement('section');keysPanel.className='f2-keyboard';keysPanel.hidden=true;keysPanel.setAttribute('aria-label','Controller keyboard');$('.f2-field').append(keysPanel);
 const profileButton=document.createElement('button');profileButton.type='button';profileButton.className='f2-color-button f2-home-profile';$('.f2-trainer-panel').append(profileButton);
 function scale(){canvas.style.transform=`scale(${root.clientWidth/960})`;}
 new ResizeObserver(scale).observe(root);scale();
 function focus(el,dom=true){$$('.f2-focused').forEach(x=>x.classList.remove('f2-focused'));if(el){el.classList.add('f2-focused');if(dom)el.focus({preventScroll:true});}}
 function find(query){return typeof query==='string'?$(query):query;}
 function interactive(container){return Array.from(container.querySelectorAll('button,input')).filter(e=>!e.disabled&&!e.closest('[inert],[hidden]')&&e.getClientRects().length);}
 function lock(){
  const overlay=Boolean(pick||keyboard||state.menu);
  content.inert=overlay||state.drawer;drawerShell.inert=overlay;
  menu.inert=!state.menu||Boolean(pick||keyboard);menu.setAttribute('aria-hidden',String(!state.menu));
  bay.inert=!state.drawer;bay.setAttribute('aria-hidden',String(!state.drawer));
  picker.inert=!pick||Boolean(keyboard);keysPanel.inert=!keyboard;
 }
 function notify(message){clearTimeout(noticeTimer);$('.f2-notice').textContent=message;$('.f2-notice').classList.add('f2-visible');noticeTimer=setTimeout(()=>$('.f2-notice').classList.remove('f2-visible'),1700);}
 function setWorlds(dom=true){
  if(state.detail){
   $('.f2-worlds-body').innerHTML=`<div class="f2-detail"><div><div class="f2-eyebrow">${worldStates[state.world]}</div><h2 class="f2-heading">${worlds[state.world]}</h2></div><div class="f2-caption">${state.world===2?'Pokémon Emerald · Route 119 · 5 badges':'Your adventures in this World will appear here.'}</div></div><div class="f2-world-actions"><button type="button" class="f2-action f2-color-button">${state.world===2?'Pokémon Emerald · Continue':'Back to Worlds'}</button></div>`;
   $('.f2-action').onclick=()=>state.world===2?notify('Emerald · Resume preview'):back();focus($('.f2-action'),dom);$('.f2-world-hint').textContent='B Back to Worlds';
  }else{
   const offset=Math.max(0,Math.floor(state.world/3)-1);
   $('.f2-worlds-body').innerHTML='<div class="f2-grid-window"><div class="f2-world-grid">'+worlds.map((w,i)=>`<button type="button" class="f2-world" data-world="${i}"><span class="f2-world-name">${w}</span><span class="f2-world-state">${worldStates[i]}</span></button>`).join('')+'</div></div>';
   $('.f2-world-grid').style.transform=`translateY(${-offset*130}px)`;
   $$('[data-world]').forEach((b,i)=>{b.inert=i<offset*3||i>=offset*3+6;b.onclick=()=>{state.world=i;state.detail=true;setWorlds();};});
   $('.f2-world-hint').textContent=offset?'↑ Previous worlds':'↓ More worlds';focus($$('[data-world]')[state.world],dom);
  }
  $('.f2-world-position').textContent=`${state.world+1} / 9 worlds`;
 }
 views[2].innerHTML='<div class="f2-dex-toolbar"><button type="button" class="f2-filter" data-filter="world"><small>WORLD</small><b>All worlds</b></button><button type="button" class="f2-filter" data-filter="type"><small>TYPE</small><b>All types</b></button><button type="button" class="f2-filter" data-filter="status"><small>COLLECTION</small><b>All entries</b></button><button type="button" class="f2-filter f2-search" data-search><small>SEARCH</small><b>Name or number…</b></button><button type="button" class="f2-filter" data-sort><small>SORT</small><b>№</b></button></div><div class="f2-dex-workspace"><div class="f2-dex-side"><div class="f2-dex-window"><div class="f2-dex-list"></div></div><div class="f2-dex-count"></div></div><div class="f2-dex-detail"></div></div>';
 function applyFilters(){
  const query=state.query.trim().toLowerCase().replace(/^#/,'');
  filtered=pokemon.filter(p=>(state.worldFilter==='All worlds'||p.worlds.includes(state.worldFilter))&&(state.typeFilter==='All types'||p.types.includes(state.typeFilter))&&(state.statusFilter==='All entries'||state.statusFilter==='Caught'&&p.caught||state.statusFilter==='Seen'&&p.seen||state.statusFilter==='Favorites'&&p.favorite||state.statusFilter==='Not caught'&&!p.caught)&&(!query||p.name.toLowerCase().includes(query)||String(p.id).includes(query)));
  filtered.sort(state.sort==='Name'?(a,b)=>a.name.localeCompare(b.name):(a,b)=>a.id-b.id);state.dex=Math.min(state.dex,Math.max(0,filtered.length-1));
 }
 function dex(dom=true,target=null){
  applyFilters();
  $('[data-filter="world"] b').textContent=state.worldFilter;$('[data-filter="type"] b').textContent=state.typeFilter;$('[data-filter="status"] b').textContent=state.statusFilter;
  $('[data-search] b').textContent=state.query||'Name or number…';$('[data-sort] b').textContent=state.sort==='Number'?'№':'A–Z';
  const offset=Math.max(0,state.dex-4);
  $('.f2-dex-list').innerHTML=filtered.length?filtered.map((p,i)=>`<button type="button" class="f2-species" data-species="${i}" data-pokemon-id="${p.id}"><span class="f2-number">${p.id}</span>${p.name}<span class="f2-caught-mark">${p.favorite?'★':p.caught?'●':'○'}</span></button>`).join(''):'<div class="f2-dex-empty">No entries match.<br>Try another category.</div>';
  $('.f2-dex-list').style.transform=`translateY(${-offset*52}px)`;
  $$('[data-species]').forEach((b,i)=>{b.inert=i<offset||i>=offset+5;b.onclick=()=>{state.dex=i;dex();};});
  const p=filtered[state.dex];
  $('.f2-dex-count').innerHTML=`<span>${filtered.length?state.dex+1:0} / ${filtered.length} entries</span><span>↑ ↓ Browse</span>`;
  $('.f2-dex-detail').innerHTML=p?`<div class="f2-eyebrow">NATIONAL № ${p.id}</div><h2 class="f2-heading">${p.name}</h2><div class="f2-specimen"><div class="f2-dex-emblem" aria-label="Species artwork placeholder">${p.id}</div><div>${p.types.map(t=>`<span class="f2-type-pill">${t}</span>`).join('')}<div class="f2-detail-label">REGIONAL COLLECTIONS</div><div class="f2-caption">${p.worlds.join(' · ')}</div></div></div><div class="f2-detail-label">YOUR FIELD NOTES</div><div class="f2-caption">${p.caught?'Seen and caught':p.seen?'Seen · Not caught yet':'Not discovered yet'}</div><div class="f2-dex-actions"><button type="button" class="f2-color-button f2-pink" data-favorite>${p.favorite?'★ Favorited':'☆ Favorite'}</button><span>Part of your personal collection</span></div>`:'<h2 class="f2-heading">A new discovery awaits</h2><div class="f2-caption" style="margin-top:20px">Clear some filters to explore your collection.</div><div class="f2-dex-actions"><button type="button" class="f2-color-button" data-reset>Reset filters</button></div>';
  if(p)$('[data-favorite]').onclick=()=>{p.favorite=!p.favorite;dex(true,'[data-favorite]');};
  if($('[data-reset]'))$('[data-reset]').onclick=()=>{Object.assign(state,{query:'',worldFilter:'All worlds',typeFilter:'All types',statusFilter:'All entries',dex:0});dex();};
  focus(find(target)||$$('[data-species]')[state.dex]||$('[data-reset]'),dom);
 }
 function openPicker(title,options,value,callback,opener){
  pick={title,options,index:Math.max(0,options.indexOf(value)),callback,opener};lock();renderPicker();
 }
 function renderPicker(){
  picker.hidden=false;picker.innerHTML=`<h2>${esc(pick.title)}</h2><div class="f2-choice-window"><div class="f2-choice-list">${pick.options.map((v,i)=>`<button type="button" class="f2-color-button" data-choice="${i}">${esc(v)}</button>`).join('')}</div></div><div class="f2-picker-footer">A Select · B Return · ${pick.index+1}/${pick.options.length}</div>`;
  const offset=Math.max(0,pick.index-5);picker.querySelector('.f2-choice-list').style.transform=`translateY(${-offset*47}px)`;
  Array.from(picker.querySelectorAll('[data-choice]')).forEach((b,i)=>{b.inert=i<offset||i>=offset+6;b.onclick=()=>{const active=pick;pick=null;picker.hidden=true;lock();active.callback(active.options[i]);focus(find(active.opener));};});
  focus(picker.querySelector(`[data-choice="${pick.index}"]`));
 }
 const typeNames=['All types','Normal','Fire','Water','Electric','Grass','Ice','Fighting','Poison','Ground','Flying','Psychic','Bug','Rock','Ghost','Dragon','Dark','Steel','Fairy'];
 $('[data-filter="world"]').onclick=()=>openPicker('World',['All worlds',...worlds],state.worldFilter,value=>{state.worldFilter=value;state.dex=0;dex(false);},'[data-filter="world"]');
 $('[data-filter="type"]').onclick=()=>openPicker('Pokémon type',typeNames,state.typeFilter,value=>{state.typeFilter=value;state.dex=0;dex(false);},'[data-filter="type"]');
 $('[data-filter="status"]').onclick=()=>openPicker('Your collection',['All entries','Seen','Caught','Not caught','Favorites'],state.statusFilter,value=>{state.statusFilter=value;state.dex=0;dex(false);},'[data-filter="status"]');
 $('[data-sort]').onclick=()=>{state.sort=state.sort==='Number'?'Name':'Number';state.dex=0;dex(true,'[data-sort]');};
 $('[data-search]').onclick=()=>openKeyboard('Pokédex search',state.query,32,value=>{state.query=value;state.dex=0;dex(false);},'[data-search]');
 function initials(name){return name.trim().split(/\s+/).map(p=>p[0]).join('').slice(0,2).toUpperCase()||'+';}
 function updateHome(){
  $('.f2-trainer-name').textContent=state.profile?.name||'Your Trainer';$('.f2-trainer-small').textContent=state.profile?'ID 00482':'Your story starts here';$('.f2-trainer-panel .f2-avatar').textContent=state.profile?initials(state.profile.name):'+';
  profileButton.textContent=state.profile?'View Trainer':'Create Trainer';
 }
 function profile(dom=true){
  const p=state.profile;
  if(state.editor){
   const draft=state.draft;
   views[3].innerHTML=`<div class="f2-editor-page"><div class="f2-page-top"><h1 class="f2-heading">${p?'Edit your Trainer':'Meet your Trainer'}</h1><span class="f2-caption">A little more you</span></div><div class="f2-profile-station"><div class="f2-profile-pass"><div class="f2-avatar">${esc(initials(draft.name))}</div><div class="f2-eyebrow">TRAINER PASS</div><div style="margin-top:10px;font-size:18px">${esc(draft.emblem)}</div></div><div class="f2-profile-fields"><label class="f2-form-field">TRAINER NAME · A opens keyboard<input id="f2-name" type="text" maxlength="24" value="${esc(draft.name)}" autocomplete="off" aria-label="Trainer name"></label><label class="f2-form-field">EMBLEM<button type="button" class="f2-color-button f2-sky" data-emblem>${esc(draft.emblem)}</button></label><label class="f2-form-field">FAVORITE POKÉMON<button type="button" class="f2-color-button f2-pink" data-profile-favorite>${esc(draft.favorite)}</button></label><div class="f2-edit-hint">Your name, your favorite, your next adventure.</div><div class="f2-edit-error" role="status"></div></div></div><div class="f2-mounted-actions"><button type="button" class="f2-color-button f2-sky" data-cancel-profile>Cancel</button><button type="button" class="f2-color-button" data-save-profile>Save Trainer</button></div></div>`;
   $('#f2-name').oninput=e=>{draft.name=e.target.value;views[3].querySelector('.f2-avatar').textContent=initials(draft.name);};
   $('[data-emblem]').onclick=()=>openPicker('Trainer emblem',['Leaf','Spark','Wave','Star'],draft.emblem,value=>{draft.emblem=value;profile(false);},'[data-emblem]');
   $('[data-profile-favorite]').onclick=()=>openPicker('Favorite Pokémon',['Not selected',...pokemon.map(x=>x.name).sort()],draft.favorite,value=>{draft.favorite=value;profile(false);},'[data-profile-favorite]');
   $('[data-save-profile]').onclick=()=>{const name=draft.name.trim();if(!name){$('.f2-edit-error').textContent='Choose a name for your Trainer.';focus($('#f2-name'));return;}state.profile={name,emblem:draft.emblem,favorite:draft.favorite};state.editor=false;state.draft=null;updateHome();profile();notify('Trainer saved for this preview');};
   $('[data-cancel-profile]').onclick=()=>{state.editor=false;state.draft=null;profile();};focus($('#f2-name'),dom);return;
  }
  views[3].innerHTML=p?`<div class="f2-editor-page"><div class="f2-page-top"><h1 class="f2-heading">Trainer</h1><span class="f2-caption">Every journey leaves a story</span></div><div class="f2-profile-station"><div class="f2-profile-pass"><div class="f2-avatar">${esc(initials(p.name))}</div><h2 style="font-size:23px;overflow-wrap:anywhere">${esc(p.name)}</h2><div class="f2-caption">${esc(p.emblem)} · ID 00482</div></div><div><div class="f2-stat-line"><span>Favorite Pokémon</span><strong>${esc(p.favorite)}</strong></div><div class="f2-stat-line"><span>Worlds visited</span><strong>4 / 9</strong></div><div class="f2-stat-line"><span>Time together</span><strong>128 h 42 min</strong></div></div></div><div class="f2-mounted-actions"><button type="button" class="f2-color-button" data-edit-profile>Edit Trainer</button></div></div>`:`<div class="f2-editor-page"><div class="f2-page-top"><h1 class="f2-heading">Your story starts here</h1></div><div class="f2-first-profile"><div class="f2-avatar" aria-hidden="true">+</div><div><h2 class="f2-heading">Hello, Trainer!</h2><p class="f2-caption" style="margin-top:15px;max-width:440px">Choose a name, pick a favorite Pokémon, and make this little field companion yours.</p></div></div><div class="f2-mounted-actions"><button type="button" class="f2-color-button" data-edit-profile>Create Trainer</button></div></div>`;
  $('[data-edit-profile]').onclick=()=>{state.draft=state.profile?{...state.profile}:{name:'',emblem:'Leaf',favorite:'Not selected'};state.editor=true;profile();};focus($('[data-edit-profile]'),dom);
 }
 profileButton.onclick=()=>{setPage(3);if(!state.profile)$('[data-edit-profile]').click();};
 const keyLayout=[
  ...Array.from('ABCDEFGHIJKLMNOPQRSTUVWXYZ',(label,i)=>({label,row:Math.floor(i/10),col:i%10,width:1})),
  {label:'Delete',row:2,col:6,width:2},{label:'Clear',row:2,col:8,width:2},
  {label:'Space',row:3,col:0,width:6},{label:'Apply',row:3,col:6,width:4},
  ...Array.from('123456789',(label,i)=>({label,row:Math.floor(i/3),col:11+i%3,width:1})),
  {label:'0',row:3,col:11,width:3}
 ];
 const keyLabels=keyLayout.map(k=>k.label);
 function openKeyboard(title,value,max,callback,opener){keyboard={title,value,max,callback,opener,index:0,column:0};lock();renderKeyboard();}
 function moveKeyboard(direction){
  const from=keyLayout[keyboard.index],horizontal=direction==='left'||direction==='right',forward=direction==='right'||direction==='down';
  const candidates=keyLayout.map((k,index)=>({...k,index})).filter(k=>horizontal?k.row===from.row&&(forward?k.col>from.col:k.col<from.col):k.row===from.row+(forward?1:-1));
  const distance=k=>Math.max(k.col-keyboard.column,keyboard.column-(k.col+k.width-1),0);
  candidates.sort(horizontal?(a,b)=>forward?a.col-b.col:b.col-a.col:(a,b)=>distance(a)-distance(b)||a.col-b.col);
  const next=candidates[0];if(!next)return;
  keyboard.index=next.index;keyboard.column=horizontal?(forward?next.col:next.col+next.width-1):Math.max(next.col,Math.min(next.col+next.width-1,keyboard.column));
  focus(keysPanel.querySelector(`[data-key-index="${next.index}"]`));
 }
 function renderKeyboard(){
  const block=numeric=>keyLayout.map((k,i)=>({k,i})).filter(({k})=>(k.col>=11)===numeric).map(({k,i})=>`<button type="button" data-key="${k.label}" data-key-index="${i}" style="grid-column:${k.col-(numeric?11:0)+1}/span ${k.width};grid-row:${k.row+1}"${k.label==='Delete'?' aria-label="Delete last character"':''}>${k.label==='Delete'?'⌫ Delete':k.label}</button>`).join('');
  keysPanel.hidden=false;keysPanel.innerHTML=`<div class="f2-keyboard-top"><div class="f2-keyboard-label">${esc(keyboard.title)}</div><div class="f2-keyboard-output" aria-live="polite">${esc(keyboard.value)||'…'}</div></div><div class="f2-key-grid"><div class="f2-letter-block" role="group" aria-label="Letters and editing">${block(false)}</div><div class="f2-number-block" role="group" aria-label="Numbers">${block(true)}</div></div><div class="f2-keyboard-hints"><span>✚ / Left stick Choose · A Press · B Cancel</span><span>${keyboard.value.length} / ${keyboard.max}</span></div>`;
  keysPanel.querySelectorAll('[data-key]').forEach(b=>b.onclick=()=>pressKey(b.dataset.key));focus(keysPanel.querySelector(`[data-key-index="${keyboard.index}"]`));
 }
 function pressKey(key){
  if(key==='Apply'){const active=keyboard;keyboard=null;keysPanel.hidden=true;lock();active.callback(active.value);focus(find(active.opener));return;}
  if(key==='Delete')keyboard.value=keyboard.value.slice(0,-1);else if(key==='Clear')keyboard.value='';else if(keyboard.value.length<keyboard.max)keyboard.value+=(key==='Space'?' ':key);
  const index=keyLabels.indexOf(key);if(index>=0)keyboard.index=index;renderKeyboard();
 }
 function homeDrawer(open){if(state.page!==0)return;state.drawer=open;root.classList.toggle('f2-drawer-open',open);handle.setAttribute('aria-expanded',String(open));$('.f2-handle-key').textContent='Y';$('.f2-handle-verb').textContent=open?'Close':'Open';lock();focus(open?resumes[state.resume]:handle);}
 function system(open){if(open)menuReturn=$('.f2-focused');state.menu=open;root.classList.toggle('f2-menu-open',open);lock();if(open)focus(menu.querySelector('button'));else focus(menuReturn?.isConnected?menuReturn:null);}
 function closeTransient(){pick=null;keyboard=null;picker.hidden=true;keysPanel.hidden=true;state.menu=false;state.drawer=false;root.classList.remove('f2-drawer-open','f2-menu-open');handle.setAttribute('aria-expanded','false');$('.f2-handle-key').textContent='Y';$('.f2-handle-verb').textContent='Open';lock();}
 function setPage(index,dom=true){
  closeTransient();state.page=Math.max(0,Math.min(4,index));
  views.forEach((v,i)=>{v.classList.toggle('f2-active',i===state.page);v.inert=i!==state.page;});
  nav.forEach((b,i)=>i===state.page?b.setAttribute('aria-current','page'):b.removeAttribute('aria-current'));drawerShell.hidden=state.page!==0;$('.f2-context').textContent=pageNames[state.page];$('.f2-notice').classList.remove('f2-visible');
  if(state.page===0)focus(handle,dom);else if(state.page===1)setWorlds(dom);else if(state.page===2)dex(dom);else if(state.page===3)profile(dom);else focus(nav[4],dom);
 }
 function back(){
  if(keyboard){const prev=keyboard.opener;keyboard=null;keysPanel.hidden=true;lock();focus(find(prev));}
  else if(pick){const prev=pick.opener;pick=null;picker.hidden=true;lock();focus(find(prev));}
  else if(state.menu)system(false);
  else if(state.drawer)homeDrawer(false);
  else if(state.page===1&&state.detail){state.detail=false;setWorlds();}
  else if(state.page===3&&state.editor){state.editor=false;state.draft=null;profile();}
 }
 function localItems(){return keyboard?interactive(keysPanel):pick?interactive(picker):state.menu?interactive(menu):state.drawer?[...resumes,handle]:state.page===0?[handle,profileButton]:interactive(views[state.page]);}
 function spatial(direction){
  const from=$('.f2-focused'),items=localItems();if(!from){focus(items[0]);return;}
  const r=from.getBoundingClientRect(),x=r.x+r.width/2,y=r.y+r.height/2;
  const horizontal=direction==='left'||direction==='right',sign=direction==='up'||direction==='left'?-1:1;
  const candidates=items.filter(e=>e!==from).map(e=>{const b=e.getBoundingClientRect(),dx=b.x+b.width/2-x,dy=b.y+b.height/2-y;return {e,along:(horizontal?dx:dy)*sign,cross:Math.abs(horizontal?dy:dx)};}).filter(v=>v.along>2).sort((a,b)=>(a.along+a.cross*2.2)-(b.along+b.cross*2.2));
  if(candidates[0])focus(candidates[0].e);
 }
 function action(name){
  if(name==='previous'||name==='next'){setPage(state.page+(name==='next'?1:-1));return;}
  if(name==='back'){back();return;}
  if(name==='system'){if(!keyboard&&!pick)system(!state.menu);return;}
  if(name==='continue'){if(!keyboard&&!pick&&!state.menu&&state.page===0)homeDrawer(!state.drawer);return;}
  const current=$('.f2-focused');
  if(name==='confirm'){if(current?.id==='f2-name'){openKeyboard('Trainer name',state.draft.name,24,value=>{state.draft.name=value;profile(false);},'#f2-name');return;}if(current&&!current.closest('[inert],[hidden]'))current.click();return;}
  const delta=name==='up'||name==='left'?-1:1;
  if(keyboard){moveKeyboard(name);return;}
  if(pick){pick.index=Math.max(0,Math.min(pick.options.length-1,pick.index+delta));renderPicker();return;}
  if(state.menu){spatial(name);return;}
  if(state.drawer){state.resume=Math.max(0,Math.min(2,state.resume+delta));focus(resumes[state.resume]);return;}
  if(state.page===1&&!state.detail&&current?.hasAttribute('data-world')){state.world=Math.max(0,Math.min(8,state.world+((name==='up'||name==='down')?delta*3:delta)));setWorlds();return;}
  if(state.page===2&&current?.hasAttribute('data-species')&&(name==='up'||name==='down')){if(name==='up'&&state.dex===0)focus($('[data-filter="world"]'));else{state.dex=Math.max(0,Math.min(filtered.length-1,state.dex+delta));dex();}return;}
  spatial(name);
 }
 nav.forEach(b=>b.onclick=()=>setPage(Number(b.dataset.page)));$$('[data-step]').forEach(b=>b.onclick=()=>action(Number(b.dataset.step)<0?'previous':'next'));
 handle.onclick=()=>homeDrawer(!state.drawer);resumes.forEach((b,i)=>b.onclick=()=>{state.resume=i;homeDrawer(false);notify(`${b.dataset.resume} · Resume preview`);});$('.f2-system-trigger').onclick=()=>action('system');
 Array.from(menu.querySelectorAll('button')).forEach(b=>b.onclick=()=>{if(b.hasAttribute('data-menu-close'))system(false);else $('.f2-menu-note').textContent=`${b.textContent} · Design preview`;});
 root.addEventListener('focusin',e=>{if(e.target.matches('button,input'))focus(e.target,false);if(keyboard&&e.target.hasAttribute('data-key-index')){keyboard.index=Number(e.target.dataset.keyIndex);const k=keyLayout[keyboard.index];keyboard.column=Math.max(k.col,Math.min(k.col+k.width-1,keyboard.column));}});
 root.addEventListener('keydown',e=>{
  if(keyboard&&e.key.length===1&&!e.ctrlKey&&!e.metaKey){e.preventDefault();if(keyboard.value.length<keyboard.max)keyboard.value+=e.key;renderKeyboard();return;}
  if(keyboard&&e.key==='Backspace'){e.preventDefault();pressKey('Delete');return;}
  if(e.target.matches('input')&&!keyboard&&!['Enter','Escape','ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Tab'].includes(e.key))return;
  const map={ArrowUp:'up',ArrowDown:'down',ArrowLeft:'left',ArrowRight:'right',Enter:'confirm',Escape:'back',q:'previous',Q:'previous',e:'next',E:'next',m:'system',M:'system',y:'continue',Y:'continue'};
  if(map[e.key]){e.preventDefault();action(map[e.key]);}
  if(e.key==='Tab'){const items=localItems();if(items.length){e.preventDefault();const i=items.indexOf(document.activeElement);focus(items[(i+(e.shiftKey?-1:1)+items.length)%items.length]);}}
 });
 canvas.addEventListener('pointermove',e=>{if(!state.parallax||matchMedia('(prefers-reduced-motion: reduce)').matches)return;const r=canvas.getBoundingClientRect();root.style.setProperty('--f2-x',`${((e.clientX-r.left)/r.width-.5)*5}px`);root.style.setProperty('--f2-y',`${((e.clientY-r.top)/r.height-.5)*3}px`);});
 canvas.addEventListener('pointerleave',()=>{root.style.setProperty('--f2-x','0px');root.style.setProperty('--f2-y','0px');});
 function material(){const hue={turquoise:176,red:5,green:141,blue:211,orange:26}[state.theme],sat=state.finish==='satin'?30:51;root.style.setProperty('--f2-hue',hue);['--f2-top','--f2-panel','--f2-low'].forEach((key,i)=>root.style.setProperty(key,`hsl(${hue} ${sat}% ${[33,23,16][i]}%)`));root.style.setProperty('--f2-ink',`hsl(${hue} 25% 21%)`);root.style.setProperty('--f2-wear',state.wear/100);if(!state.parallax){root.style.setProperty('--f2-x','0px');root.style.setProperty('--f2-y','0px');}}
 updateHome();setPage(0,false);material();
 if(globalThis.Tweak){const tweak=new Tweak({container:root,onChange:material});tweak.addSelect(state,'theme',{label:'Цветовая тема',options:[{label:'Бирюзовая · Flip 2',value:'turquoise'},{label:'Красная',value:'red'},{label:'Зелёная',value:'green'},{label:'Синяя',value:'blue'},{label:'Оранжевая',value:'orange'}]});tweak.addSelect(state,'finish',{label:'Материал',options:[{label:'Полимер',value:'resin'},{label:'Матовый',value:'satin'}]});tweak.addSlider(state,'wear',{label:'Потёртости',min:0,max:50,unit:'%'});tweak.addToggle(state,'parallax',{label:'Мягкий параллакс'});}
 let previousButtons=[],held=null,nextRepeat=0;
 function poll(now){
  if(!root.isConnected)return;let pads=[];try{pads=navigator.getGamepads?navigator.getGamepads():[];}catch{return;}
  const pad=Array.from(pads).find(p=>p&&p.mapping==='standard');
  if(pad){const pressed=pad.buttons.map(b=>b.pressed);if(root.contains(document.activeElement)){
   [[0,'confirm'],[1,'back'],[3,'continue'],[4,'previous'],[5,'next'],[9,'system']].forEach(([i,a])=>{if(pressed[i]&&!previousButtons[i])action(a);});
   const direction=pressed[12]||pad.axes[1]<-.5?'up':pressed[13]||pad.axes[1]>.5?'down':pressed[14]||pad.axes[0]<-.5?'left':pressed[15]||pad.axes[0]>.5?'right':null;
   if(direction!==held){held=direction;if(direction){action(direction);nextRepeat=now+340;}}else if(direction&&now>=nextRepeat){action(direction);nextRepeat=now+115;}
  }previousButtons=pressed;}else{previousButtons=[];held=null;}requestAnimationFrame(poll);
 }
 requestAnimationFrame(poll);
})();
