// Native Node tests with synthetic samples and DOM/WebAudio stubs. No browser or microphone.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const here=new URL('.',import.meta.url);
const wavSource=fs.readFileSync(new URL('wav.js',here),'utf8');
const {encodeWav}=await import('data:text/javascript;base64,'+Buffer.from(wavSource).toString('base64'));
let Worklet;
vm.runInNewContext(fs.readFileSync(new URL('capture.js',here),'utf8'),{Float32Array,sampleRate:100,AudioWorkletProcessor:class{constructor(){this.messages=[];this.port={postMessage:m=>this.messages.push(m)};}},registerProcessor:(_,c)=>Worklet=c});
const capture=new Worklet();
for(let i=0;i<18;i++)capture.process([[new Float32Array(128).fill(.25)]]);
capture.port.onmessage({data:'stop'});capture.port.onmessage({data:'stop'});
assert.equal(capture.messages.filter(m=>m.type==='done').length,1);
const chunks=capture.messages.filter(m=>m.type==='chunk').map(m=>m.samples);
assert.equal(chunks.reduce((n,c)=>n+c.length,0),2304,'stop must flush partial worklet block');
assert.equal(encodeWav(chunks,48000).byteLength,44+2304*2);
const limited=new Worklet();for(let i=0;i<60;i++)limited.process([[new Float32Array(128)]]);
assert.equal(limited.messages.filter(m=>m.type==='chunk').reduce((n,m)=>n+m.samples.length,0),6000);
assert.equal(limited.messages.at(-1).reason,'limit');
const nodes=new Map();
function node(){return {children:[],style:{},value:'',textContent:'',hidden:false,disabled:false,classList:{toggle(){}},append(...c){this.children.push(...c)},replaceChildren(){this.children=[]},querySelector(){return node()},getContext(){return {clearRect(){},beginPath(){},moveTo(){},lineTo(){},stroke(){}}},pause(){},load(){},removeAttribute(){}};}
let stopped=0,closed=0,failClose=false,saveFailure=true;const timers=new Map();let timerId=0;
class Context{
 constructor(){this.sampleRate=48000;this.state='running';this.audioWorklet={addModule:async()=>{}};this.destination={};}
 async resume(){} async close(){closed++;this.state='closed';if(failClose)throw new Error('close failed');}
 createMediaStreamSource(){return {connect(){},disconnect(){}};}
 createGain(){return {gain:{},connect(){},disconnect(){}};}
}
const env={encodeWav,Float32Array,Blob,URLSearchParams,console,Math,Number,String,Error,URL:{createObjectURL:()=> 'blob:test',revokeObjectURL(){}},
 document:{getElementById:id=>{if(!nodes.has(id))nodes.set(id,node());return nodes.get(id)},createElement:node,addEventListener(){}},window:{addEventListener(){}},
 navigator:{mediaDevices:{getUserMedia:async()=>({getTracks:()=>[track]})}},AudioContext:Context,AudioWorkletNode:class{constructor(){this.port={postMessage(){}};}connect(){}disconnect(){}},
 fetch:async(_url,opts)=>opts?{ok:!saveFailure,json:async()=>saveFailure?{error:'disk full'}:{file:'take.wav'}}:{ok:true,json:async()=>({folder:'/test',takes:[]})},
 setTimeout:fn=>{timers.set(++timerId,fn);return timerId},clearTimeout:id=>timers.delete(id)};
const track={readyState:'live',stop(){stopped++},onended:null};
const app=fs.readFileSync(new URL('app.js',here),'utf8').replace("import {encodeWav} from './wav.js';",'');
vm.createContext(env);vm.runInContext(app+'\nglobalThis.api={start,stop,save,discard,releaseMic,get state(){return state},get blob(){return blob},get processor(){return processor}};',env);
const tick=()=>new Promise(resolve=>setImmediate(resolve));
await env.api.start();assert.equal(env.api.state,'recording');
env.api.processor.port.onmessage({data:{type:'chunk',samples:new Float32Array(128)}});
failClose=true;env.api.processor.port.onmessage({data:{type:'done'}});await tick();
assert.equal(env.api.state,'review','close failure must not strand review');assert.equal(stopped,1);assert.equal(closed,1);
const retained=env.api.blob;await env.api.save();assert.equal(env.api.state,'review');assert.equal(env.api.blob,retained,'failed save must retain download/retry blob');
env.api.discard();failClose=false;await env.api.start();
env.api.processor.port.onmessage({data:{type:'chunk',samples:new Float32Array(256)}});
track.onended();assert.equal(env.api.state,'stopping');
for(const fn of [...timers.values()])fn();await tick();
assert.equal(env.api.state,'review','unresponsive worklet must retain received audio');assert.equal(env.api.blob.size,44+512);
assert.equal(stopped,2);assert.equal(closed,2);
console.log('PASS: worklet tail flush, 60s limit, WAV size, mic cleanup, failed-save retention and interrupted-capture recovery');
