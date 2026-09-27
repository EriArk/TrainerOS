'use strict';
const assert = require('node:assert/strict');
const path = require('node:path');
const crypto = require('node:crypto');
const {spawn} = require('node:child_process');
const {createEngine} = require('../src/integrations/practice/emerald-engine.cjs');
const root = path.resolve(process.argv[2] || 'tools/research/emerald-practice/node_modules/pokemon-showdown');
const engine=createEngine(root);
const empty=()=>({id:0,ppUps:0,maxPp:0});
function member() {return {number:257,form:'257',level:50,friendship:0,gender:'M',nature:0,ability:66,item:0,
    ivs:[31,31,31,31,31,31],evs:[0,0,0,0,0,0],stats:[155,140,90,130,90,100],
    moves:[{id:53,ppUps:0,maxPp:15},empty(),{id:216,ppUps:2,maxPp:28},empty()]};}
function input() {return {protocol:1,seed:[1,2,3,4],members:[member(),member()]};}
const original=input(),before=JSON.stringify(original),session=engine.start(original);
let state=session.state();
assert.deepEqual(state.sides[0].moves.map(m=>[m.slot,m.pp,m.maxPp]),[[1,15,15],[3,28,28]]);
assert.equal(state.sides[0].hp,155);assert.equal(state.sides[0].status,'');
assert.match(state.sides[0].moves[1].move,/Return 1/);
assert.throws(()=>session.turn(1,[2,1]),/Unavailable/);
state=session.turn(1,[1,3]);assert.equal(state.turn,2);
assert.equal(state.sides[0].moves[0].pp,14);assert.equal(state.sides[1].moves[1].pp,27);
assert.throws(()=>session.turn(1,[1,3]),/Stale/);session.close();assert.equal(JSON.stringify(original),before);
for(const mutate of [
    x=>x.members[0].stats[1]++, x=>x.members[0].ivs.pop(),x=>x.members[0].evs.fill(255),
    x=>x.members[0].friendship=256,x=>x.members[0].gender='N',x=>x.members[0].nature=25,
    x=>x.members[0].moves[0].maxPp=24,x=>x.members[0].ability=1,x=>x.members[0].form='10033',
    x=>x.members[0].item=175, // Enigma Berry's save-specific effect is deliberately unsupported.
    x=>x.members[0].moves=[empty(),empty(),empty(),empty()],x=>x.members.push(member()),
    x=>x.seed=[1,2,3,65536],x=>x.members[0].moves[0].id=355
]) {const value=input();mutate(value);assert.throws(()=>engine.start(value));}
// Mixed PP Ups and sparse slot identity must survive a locked move request.
const locked=input();locked.members[0].moves[2]={id:200,ppUps:1,maxPp:18};
const lockBattle=engine.start(locked);lockBattle.state();const lockState=lockBattle.turn(1,[3,3]);
assert.equal(lockState.sides[0].moves[0].slot,3);assert.equal(lockState.sides[0].moves.length,1);lockBattle.close();
const happy=input();happy.members[0].friendship=255;
const happyBattle=engine.start(happy);assert.match(happyBattle.state().sides[0].moves[1].move,/Return 102/);happyBattle.close();
const transformed=input();transformed.members[0]={...member(),number:132,form:'132',gender:'N',ability:7,
    stats:[123,68,68,68,68,68],moves:[{id:144,ppUps:0,maxPp:10},empty(),empty(),empty()]};
transformed.members[1].moves[0]={id:150,ppUps:0,maxPp:40};
const ditto=engine.start(transformed);ditto.state();const copied=ditto.turn(1,[1,1]);
assert.deepEqual(copied.sides[0].moves.map(m=>[m.slot,m.pp]),[[1,5],[2,5]]);
assert.equal(ditto.turn(2,[2,1]).sides[0].moves[1].pp,4);ditto.close();
const exhausted=input();exhausted.members.forEach(m=>m.moves=[{id:150,ppUps:0,maxPp:40},empty(),empty(),empty()]);
const splash=engine.start(exhausted);let dry=splash.state();
for(let i=0;i<40;++i)dry=splash.turn(dry.turn,[1,1]);
assert.deepEqual(dry.sides[0].moves.map(m=>m.slot),[0]);
splash.turn(dry.turn,[0,0]);splash.close();
function complete() {
    const value=input(), battle=engine.start(value),events=[];let s=battle.state();events.push(...s.events);
    while(!s.ended){s=battle.turn(s.turn,[s.sides[0].moves[0].slot,s.sides[1].moves[0].slot]);events.push(...s.events);}
    assert.ok(s.turn<=201);assert.ok(s.winner);battle.close();
    return crypto.createHash('sha256').update(JSON.stringify(events)).digest('hex');
}
const replay=complete();assert.equal(complete(),replay);
async function protocol(payload) {
    return new Promise((resolve,reject)=> {
        const child=spawn(process.execPath,[path.resolve('src/integrations/practice/emerald-worker.cjs'),root],{stdio:['pipe','pipe','pipe']});
        let out='',err='';const timer=setTimeout(()=>{child.kill();reject(Error('Worker timeout'));},5000);
        child.stdout.on('data',d=>out+=d);child.stderr.on('data',d=>err+=d);
        child.on('error',reject);child.on('close',code=>{clearTimeout(timer);assert.equal(code,0,err);resolve(out.trim().split('\n').map(JSON.parse));});
        child.stdin.end(payload);
    });
}
(async()=> {
    let result=await protocol(JSON.stringify({...input(),command:'start'})+'\n'+JSON.stringify({command:'cancel'})+'\n');
    assert.deepEqual(result.map(r=>r.type),['ready','state','cancelled']);
    result=await protocol('{"command":"eval","code":"throw 1"}\n');assert.equal(result.at(-1).type,'error');
    result=await protocol('x'.repeat(20000));assert.equal(result.at(-1).type,'error');
    console.log(JSON.stringify({ok:true,replay,checks:'PP, sparse and locked slots, friendship, rejection, completion, stdio bounds/cancel'}));
})().catch(e=>{console.error(e);process.exitCode=1;});
