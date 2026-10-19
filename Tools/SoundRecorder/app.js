import {encodeWav} from './wav.js';
const $ = id => document.getElementById(id);
const cues = [
 ['candle','match-strike','Match strike','One clean strike. Leave a moment of silence at each end.'],
 ['candle','cup-fabric','Cupping the flame','A small sleeve and hand movement. Try soft, quick and trembling variations.'],
 ['candle','draft-wind','A narrow draft','A restrained breath of wind, then a stronger gust. Keep away from the microphone capsule.'],
 ['house','chair-creak','Chair creak','A slow shift of weight. Then a sharper rock against the floor.'],
 ['house','floor-creak','Floorboard','One footstep at a time, with space between each sound.'],
 ['house','door-latch','Door & latch','Separate the handle, hinge and latch into their own takes.'],
 ['keeper','wooden-joint','Wooden joints','Dry, close clicks and tension in old wood. Avoid a constant rhythm.'],
 ['keeper','cloth-drag','Dragging cloth','A long, rough pull followed by a short, sudden movement.'],
 ['puzzles','crowbar-plank','Crowbar & plank','Pressure, a strained creak, then the release. Record the pieces separately.'],
 ['puzzles','fuse-insert','Fuse insertion','A close mechanical click with a little weight behind it.'],
 ['puzzles','lockpick-click','Lockpick detail','Small, clean metal movements. Leave timing loose for the final minigame.'],
 ['room','room-tone','Room tone','Hold still and let the space breathe. Keep a quiet stretch with no handling noise.']
];
let chosen=cues[0], state='idle', stream, context, processor, source, mute, chunks=[], frames=0, rate=48000, blob, blobUrl, peak=0, recent=[], stopTimer;
const nav=$('cues');
for (const cue of cues) {
 const b=document.createElement('button');b.className='cue';b.innerHTML=`<small>${cue[0].toUpperCase()}</small>${cue[2]}`;
 b.onclick=()=>{if(state==='idle'){chosen=cue;$('custom').value='';selectCue();}};nav.append(b);
}
function selectCue(){ $('group').textContent=chosen[0].toUpperCase();$('cue-title').textContent=chosen[2];$('direction').textContent=chosen[3];[...nav.children].forEach((b,i)=>b.classList.toggle('selected',chosen===cues[i])); }
function controls(){
 const busy=['starting','recording','stopping','finishing','saving'].includes(state);
 $('record').disabled=state!=='idle'&&state!=='recording';$('record').classList.toggle('active',state==='recording');$('record').querySelector('span').textContent=state==='recording'?'Stop recording':'Record take';
 $('review').hidden=!blob; $('save').disabled=state!=='review';$('discard').disabled=busy;
 $('custom').disabled=state!=='idle';$('note').disabled=busy;for(const b of nav.children)b.disabled=state!=='idle';
}
function showError(message){$('error').textContent=message;}
function display(samples){
 let chunkPeak=0;for(const sample of samples)chunkPeak=Math.max(chunkPeak,Math.abs(sample));peak=Math.max(peak,chunkPeak);
 const db=20*Math.log10(Math.max(chunkPeak,0.000001));$('level-fill').style.width=`${Math.max(0,Math.min(100,(db+60)/60*100))}%`;$('level-fill').style.background=chunkPeak>.98?'#da6848':'#c8aa74';
 $('level-label').textContent=peak>.98?'Clipping detected — try a quieter take':`Peak ${Math.round(20*Math.log10(Math.max(peak,.000001)))} dBFS`;
 const seconds=frames/rate;$('timer').textContent=`${String(Math.floor(seconds/60)).padStart(2,'0')}:${(seconds%60).toFixed(1).padStart(4,'0')}`;
 recent.push(chunkPeak);if(recent.length>160)recent.shift();draw();
}
function draw(){const c=$('wave'),g=c.getContext('2d');g.clearRect(0,0,c.width,c.height);g.strokeStyle='#41483d';g.beginPath();g.moveTo(0,80);g.lineTo(c.width,80);g.stroke();g.strokeStyle='#b99e70';g.lineWidth=3;recent.forEach((v,i)=>{const x=i*c.width/160,h=Math.max(1,Math.sqrt(v)*65);g.beginPath();g.moveTo(x,80-h);g.lineTo(x,80+h);g.stroke();});}
async function releaseMic(){
 // Detach references before awaiting close: an older cleanup must never clear a new capture.
 const oldStream=stream, oldContext=context, nodes=[source,processor,mute];stream=source=processor=mute=context=null;
 for(const track of oldStream?.getTracks()||[]){track.onended=null;try{track.stop();}catch{}}
 for(const node of nodes){try{node?.disconnect();}catch{}}
 try{if(oldContext&&oldContext.state!=='closed')await oldContext.close();}catch{}
}
async function finishTake(warning=''){
 if(!['recording','stopping'].includes(state))return;
 state='finishing';clearTimeout(stopTimer);controls();
 try{blob=new Blob([encodeWav(chunks,rate)],{type:'audio/wav'});blobUrl=URL.createObjectURL(blob);$('playback').src=blobUrl;$('download').href=blobUrl;$('download').download=`S_${chosen[1]}.wav`;}
 catch(e){showError(e.message);blob=null;}
 finally{await releaseMic();chunks=[];state=blob?'review':'idle';$('status').textContent=blob?'Listen, then save your take':'No take captured';if(warning)showError(warning);controls();}
}
async function start(){
 if(state!=='idle')return;
 showError('');state='starting';controls();$('status').textContent='Waiting for microphone…';chunks=[];frames=0;peak=0;recent=[];draw();
 try{
  if(!navigator.mediaDevices?.getUserMedia)throw new Error('Microphone capture needs a supported browser opened at this local address.');
  stream=await navigator.mediaDevices.getUserMedia({audio:{channelCount:1,sampleRate:48000,echoCancellation:false,noiseSuppression:false,autoGainControl:false}});
  context=new AudioContext({sampleRate:48000});rate=context.sampleRate;await context.audioWorklet.addModule('/capture.js');await context.resume();
  processor=new AudioWorkletNode(context,'tallow-capture');source=context.createMediaStreamSource(stream);mute=context.createGain();mute.gain.value=0;
  processor.port.onmessage=event=>{
   if(event.data.type==='chunk'&&['recording','stopping'].includes(state)){chunks.push(event.data.samples);frames+=event.data.samples.length;display(event.data.samples);}
   if(event.data.type==='done')void finishTake(event.data.reason==='limit'?'The 60-second limit was reached. This take is ready to save.':'');
  };
  processor.onprocessorerror=()=>void finishTake('Audio capture stopped unexpectedly. The received audio is retained; check the ending before saving.');
  for(const track of stream.getTracks())track.onended=()=>stop();
  if(stream.getTracks().some(track=>track.readyState==='ended'))throw new Error('The microphone disconnected before recording started.');
  source.connect(processor);processor.connect(mute);mute.connect(context.destination);state='recording';$('status').textContent='Recording';$('format').textContent=`${Math.round(rate/1000)} kHz · 16-bit mono WAV`;controls();
 }catch(e){await releaseMic();state='idle';showError(e.name==='NotAllowedError'?'Microphone access was declined. Allow it in your browser when you are ready to record.':e.message);$('status').textContent='Ready when you are';controls();}
}
function stop(){if(state==='recording'){state='stopping';$('status').textContent='Finishing take…';controls();stopTimer=setTimeout(()=>void finishTake('Capture did not finish normally. Received audio is retained; the last fraction of a second may be missing.'),2000);try{processor.port.postMessage('stop');}catch{void finishTake('Capture stopped unexpectedly. Received audio is retained; check the ending before saving.');}}}
function discard(){if(!['review','saved'].includes(state))return;$('playback').pause();$('playback').removeAttribute('src');$('playback').load();if(blobUrl)URL.revokeObjectURL(blobUrl);blob=blobUrl=null;state='idle';$('discard').textContent='Discard take';$('note').value='';$('status').textContent='Ready for another take';controls();}
async function loadTakes(){const r=await fetch('/api/takes');if(!r.ok)throw new Error('Could not read the saved takes.');const d=await r.json();$('save-location').textContent=d.folder;const list=$('take-list');list.replaceChildren();for(const take of d.takes.slice(0,12)){const row=document.createElement('li'),label=document.createElement('span'),detail=document.createElement('small');label.textContent=take.file;detail.textContent=`${take.duration.toFixed(1)}s · ${take.note||'No note'}`;row.append(label,detail);list.append(row);}if(!list.children.length){const li=document.createElement('li');li.className='empty';li.textContent='Your first saved take will appear here.';list.append(li);}}
async function save(){
 if(state!=='review')return;state='saving';showError('');controls();
 try{const q=new URLSearchParams({cue:chosen[1],note:$('note').value});const r=await fetch(`/api/takes?${q}`,{method:'POST',headers:{'Content-Type':'audio/wav'},body:blob});const data=await r.json();if(!r.ok)throw new Error(data.error||'The take could not be saved.');state='saved';$('status').textContent=`Saved ${data.file}`;if(data.warning)showError(data.warning);await loadTakes();$('discard').textContent='Record another take';}
 catch(e){if(state==='saved'){showError('Take saved. Refresh the page to update the list.');}else{state='review';showError(`${e.message} You can also download the WAV.`);}}
 controls();
}
$('record').onclick=()=>state==='recording'?stop():start();$('save').onclick=save;$('discard').onclick=discard;
$('custom').onchange=()=>{const value=$('custom').value.toLowerCase().trim().replace(/[^a-z0-9_-]+/g,'-');if(!value)return;if(!/^[a-z][a-z0-9_-]{0,63}$/.test(value)){showError('Start the cue name with a letter.');return;}chosen=['custom',value,value.replace(/[-_]/g,' '),'Record one clear sound with a quiet moment before and after.'];selectCue();};
document.addEventListener('keydown',e=>{if(e.repeat||['INPUT','TEXTAREA','BUTTON','AUDIO'].includes(document.activeElement?.tagName))return;if(e.code==='Space'&&['idle','recording'].includes(state)){e.preventDefault();state==='recording'?stop():start();}else if(e.key==='Enter'&&state==='review'){e.preventDefault();save();}});
window.addEventListener('beforeunload',e=>{if(['starting','recording','stopping','finishing','review','saving'].includes(state)){e.preventDefault();e.returnValue='';}});
window.addEventListener('pagehide',()=>{stream?.getTracks().forEach(t=>t.stop());});
selectCue();controls();draw();loadTakes().catch(e=>showError(e.message));
