'use strict';
// TrainerOS' bounded bridge to the pinned offline simulator. No save/file API.
const reference = require('../../../data/emerald-reference.json');
const statNames = ['hp', 'atk', 'def', 'spa', 'spd', 'spe'];
const assert = (ok, message) => { if (!ok) throw new Error(message); };
const integer = (n, min, max) => Number.isInteger(n) && n >= min && n <= max;

function createEngine(engineRoot) {
    const path = require('node:path');
    const pkg = require(path.join(engineRoot, 'package.json'));
    assert(pkg.version === '0.11.11', 'Unsupported engine version');
    const {Battle, Dex} = require(engineRoot);
    const dex = Dex.mod('gen3');
    function member(record) {
        assert(record && integer(record.number, 1, 386), 'Invalid species');
        const facts = Object.values(reference.species).find(x => x.number === record.number);
        assert(facts, 'Unknown species');
        let species = facts.id;
        if (record.number === 386) {
            assert(record.form === '10033', 'Unsupported Deoxys form');
            species = 'deoxysspeed';
        } else if (record.number === 201) {
            assert(record.form === '201' || integer(Number(record.form),10001,10027), 'Invalid Unown form');
            // Letter forms share Gen III battle facts. The UI keeps the actual form.
        } else assert(record.form === facts.form, 'Unsupported stored form');
        const speciesData = dex.species.get(species);
        assert(speciesData.exists && speciesData.num === record.number, 'Species mapping failed');
        assert(integer(record.level,1,100) && integer(record.friendship,0,255), 'Invalid level/friendship');
        const ratio = facts.genderRatio;
        assert(['M','F','N'].includes(record.gender) && (ratio===255 ? record.gender==='N'
            : ratio===254 ? record.gender==='F' : ratio===0 ? record.gender==='M' : record.gender!=='N'), 'Invalid gender');
        const ability = dex.abilities.all().find(a => a.num === record.ability && a.gen <= 3);
        assert(ability && facts.abilityIds.includes(record.ability) && record.ability !== 0, 'Unsupported ability');
        const nature = reference.natures[record.nature];
        assert(integer(record.nature,0,24) && nature, 'Invalid nature');
        assert(integer(record.item,0,376), 'Invalid item');
        const item = record.item === 0 ? null : dex.items.get(reference.items[record.item] || '');
        assert(!record.item || (item.exists && item.gen <= 3 && item.id !== 'enigmaberry'), 'Unsupported held item');
        for (const [key, max] of [['ivs',31],['evs',255],['stats',999]])
            assert(Array.isArray(record[key]) && record[key].length===6 && record[key].every(v=>integer(v,key==='stats'?1:0,max)), 'Invalid '+key);
        assert(record.evs.reduce((a,b)=>a+b,0)<=510, 'Invalid EV total');
        assert(Array.isArray(record.moves) && record.moves.length===4, 'Invalid move slots');
        const moves = [];
        for (const [slot, value] of record.moves.entries()) {
            assert(value && integer(value.id,0,354) && integer(value.ppUps,0,3), 'Invalid move');
            if (!value.id) { assert(value.maxPp===0, 'Invalid empty move'); continue; }
            const fact = reference.moves[value.id];
            const move = dex.moves.get(fact?.name || '');
            assert(move.exists && move.num===value.id && move.gen<=3, 'Unsupported move mapping');
            const maxPp = fact.pp + Math.floor(fact.pp*value.ppUps/5);
            assert(value.maxPp===maxPp && (!move.noPPBoosts || value.ppUps===0), 'Invalid PP');
            assert(!moves.some(m=>m.id===move.id), 'Duplicate move');
            moves.push({id:move.id, slot:slot+1, maxPp, ppUps:value.ppUps});
        }
        assert(moves.length, 'No moves');
        return {record, moves, set:{species:speciesData.name, level:record.level,
            ability:ability.name, nature, item:item?.name || '', gender:record.gender,
            happiness:record.friendship,
            ivs:Object.fromEntries(statNames.map((s,i)=>[s,record.ivs[i]])),
            evs:Object.fromEntries(statNames.map((s,i)=>[s,record.evs[i]])),
            moves:moves.map(m=>m.id)}};
    }
    function start(input) {
        const teams=input.teams || input.members?.map(m=>[m]);
        assert(input.protocol===1 && Array.isArray(teams) && teams.length===2 && teams.every(t=>Array.isArray(t) && t.length>=1 && t.length<=6), 'Invalid teams');
        assert(Array.isArray(input.seed) && input.seed.length===4 && input.seed.every(v=>integer(v,0,65535)), 'Invalid seed');
        const members = teams.map(t=>t.map(member));
        // The pinned start hook runs before switches, ability events or requests.
        // Patch each instance, never global simulator data or prototypes.
        class PreparedBattle extends Battle {
            start() {
                for (const [i, side] of this.sides.entries()) {
                  for (const [index, mon] of side.pokemon.entries()) {
                    const source = members[i][index];mon.trainerSource=index;
                    const stats = [mon.maxhp,...statNames.slice(1).map(s=>mon.storedStats[s])];
                    assert(stats.every((n,s)=>n===source.record.stats[s]), 'Saved stats do not match battle facts');
                    assert(mon.baseMoveSlots.length===source.moves.length, 'Move initialization failed');
                    source.moves.forEach((move,slot)=> {
                        assert(mon.baseMoveSlots[slot].id===move.id, 'Move order changed');
                        mon.baseMoveSlots[slot].pp = mon.baseMoveSlots[slot].maxpp = move.maxPp;
                        mon.ppUps[slot] = move.ppUps;
                    });
                  }
                }
                super.start();
            }
        }
        // Explicit manual setup permits destroy on a rejected second individual.
        const battle = new PreparedBattle({formatid:'gen3customgame',seed:[...input.seed]});
        try {
            members.forEach((team,i)=>battle.setPlayer('p'+(i+1), {name:'Partner '+(i+1),team:team.map(m=>m.set)}));
        } catch (e) { battle.destroy(); throw e; }
        let logOffset=0, requestId=0;
        function choices(side) {
            const request = side.activeRequest?.active?.[0];
            if (battle.ended) return [];
            if (side.activeRequest?.wait) return [{slot:9,move:'Wait',pp:0,maxPp:0,command:''}];
            const choices = (request?.moves || []).map((move,i)=> {
                const mon=side.active[0], index=mon.moveSlots.findIndex(m=>m.id===move.id);
                return {
                // Locked/recharge requests can contain just one move, regardless
                // of its original slot. Transform creates a temporary new moveset.
                slot:index<0?0:mon.transformed?index+1:(members[side.n][mon.trainerSource].moves[index]?.slot ?? 0),
                command:'move '+(i+1), move:move.move, pp:move.pp ?? 0, maxPp:move.maxpp ?? 0,
                disabled:!!move.disabled
            };}).filter(m=>!m.disabled);
            if (side.activeRequest?.forceSwitch || request && !request.trapped) {
                side.pokemon.forEach((mon,index)=>{if(!mon.fainted && !side.active.includes(mon))
                    choices.push({slot:10+mon.trainerSource,command:'switch '+(index+1),move:mon.species.name,pp:mon.hp,maxPp:mon.maxhp,switch:true});});
            }
            return choices;
        }
        function state() {
            const events = battle.log.slice(logOffset).filter(l=>!l.startsWith('|t:|'));
            logOffset=battle.log.length;
            return {type:'state',turn:battle.turn,request:++requestId,ended:battle.ended,winner:battle.winner || '',events,
                sides:battle.sides.map(side=> {
                    const mon=side.active[0];
                    return {hp:mon.hp,maxHp:mon.maxhp,status:mon.status,member:mon.trainerSource,wait:!!side.activeRequest?.wait,
                        remaining:side.pokemon.filter(p=>!p.fainted).length,total:side.pokemon.length,
                        forceSwitch:!!side.activeRequest?.forceSwitch,
                        moves:choices(side).map(({command,...m})=>m)};
                })};
        }
        return {state, close:()=>battle.destroy(),
            turn(turn, slots, request) {
                assert(!battle.ended && turn===battle.turn && turn<=200, 'Stale or finished turn');
                assert(request===undefined || request===requestId, 'Stale battle choice');
                assert(Array.isArray(slots) && slots.length===2, 'Invalid choices');
                const selected = battle.sides.map((s,i)=>choices(s).find(m=>m.slot===slots[i]));
                assert(selected.every(Boolean), 'Unavailable move');
                // Both choices validated before either player is advanced.
                battle.makeChoices(...selected.map(m=>m.command));
                if (!battle.ended && battle.turn>200) battle.tie();
                return state();
            }
        };
    }
    return {start};
}
module.exports = {createEngine};
