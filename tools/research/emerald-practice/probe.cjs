// Original research harness. No ROM/save input, server, artwork or production API.
// node probe.cjs [directory containing installed package.json] [private semantic JSON]
'use strict';
const started = performance.now();
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {createRequire} = require('node:module');
const {createHash} = require('node:crypto');
const {spawn} = require('node:child_process');
const root = path.resolve(process.argv[2] || __dirname);
const dependency = createRequire(path.join(root, 'package.json'));
assert.equal(dependency('pokemon-showdown/package.json').version, '0.11.11');
const {Battle, BattleStream, Dex} = dependency('pokemon-showdown');
const loadedMs = performance.now() - started;
const dex = Dex.mod('gen3');
const keys = ['hp', 'atk', 'def', 'spa', 'spd', 'spe'];
const uniform = n => Object.fromEntries(keys.map(k => [k, n]));
const sample = (species, ability, moves) => ({species, ability, moves, item: '',
    level: 50, gender: 'M', nature: 'Hardy', happiness: 200,
    ivs: uniform(31), evs: uniform(0)});
const samples = [sample('Blaziken', 'Blaze', ['Flamethrower', 'Sky Uppercut']),
    sample('Swampert', 'Torrent', ['Surf', 'Earthquake'])];
const options = (pair, seed = [1, 2, 3, 4]) => ({formatid: 'gen3customgame', seed,
    p1: {name: 'One', team: [structuredClone(pair[0])]},
    p2: {name: 'Two', team: [structuredClone(pair[1])]}});
const digest = value => createHash('sha256').update(JSON.stringify(value)).digest('hex');

function validate(set) {
    // A bounded probe input gate, not production legality or exact-build validation.
    assert(dex.species.get(set.species).exists);
    assert(Number.isInteger(set.level) && set.level >= 1 && set.level <= 100);
    assert(['M', 'F', 'N'].includes(set.gender));
    assert(Number.isInteger(set.happiness) && set.happiness >= 0 && set.happiness <= 255);
    assert(dex.abilities.get(set.ability).exists && dex.natures.get(set.nature).exists);
    assert(set.item === '' || dex.items.get(set.item).exists);
    for (const k of keys) {
        assert(Number.isInteger(set.ivs[k]) && set.ivs[k] >= 0 && set.ivs[k] <= 31);
        assert(Number.isInteger(set.evs[k]) && set.evs[k] >= 0 && set.evs[k] <= 255);
    }
    assert(set.moves.length >= 1 && set.moves.length <= 4);
    for (const move of set.moves) assert(dex.moves.get(move).exists && dex.moves.get(move).gen <= 3);
}

function battle(pair, seed = [1, 2, 3, 4]) {
    pair.forEach(validate);
    return new Battle(options(pair, seed));
}
function finish(pair, seed) {
    const b = battle(pair, seed);
    try {
        for (let i = 0; i < 200 && !b.ended; ++i) {
            const choices = [b.p1, b.p2].map(side => {
                const moves = side.activeRequest.active[0].moves;
                let n = moves.findIndex(m => !m.disabled && m.pp !== 0 && dex.moves.get(m.id).category !== 'Status');
                if (n < 0) n = moves.findIndex(m => !m.disabled && m.pp !== 0);
                assert(n >= 0);
                return `move ${n + 1}`;
            });
            b.makeChoices(...choices);
        }
        assert(b.ended, 'Probe battle exceeded bounded turn limit');
        // Protocol wall-clock markers are presentation metadata, not battle RNG.
        return {turns: b.turn, winner: b.winner, hash: digest(b.log.filter(line => !line.startsWith('|t:|')))};
    } finally { b.destroy(); }
}

async function waitingChild() {
    const stream = new BattleStream();
    let reported = false;
    const reading = (async () => {
        for await (const output of stream) {
            if (!reported && output.includes('|request|')) {
                reported = true;
                process.stdout.write('READY\n');
            }
        }
    })();
    await stream.write('>start ' + JSON.stringify(options(samples)));
    // Exercise disposal while the engine waits for a player's controller choice.
    process.stdin.resume();
    process.stdin.on('end', () => stream.destroy());
    await reading;
}

function cancelledChild() {
    return new Promise((resolve, reject) => {
        const at = performance.now();
        const child = spawn(process.execPath, [__filename, root, '--wait'], {stdio: ['pipe', 'pipe', 'pipe']});
        const timeout = setTimeout(() => { child.kill('SIGKILL'); reject(new Error('Child timeout')); }, 10000);
        let ready = false, errors = '', readyMs = 0;
        child.on('error', error => {clearTimeout(timeout); reject(error);});
        child.stderr.on('data', data => { errors += data; });
        child.stdout.on('data', data => {
            if (!ready && data.toString().includes('READY')) {
                ready = true; readyMs = performance.now() - at; child.kill('SIGTERM');
            }
        });
        child.on('exit', (code, signal) => {
            clearTimeout(timeout);
            if (!ready || errors) reject(new Error('Child failed: ' + errors));
            else resolve({ready, readyMs, code, signal});
        });
    });
}

async function main() {
    const checks = [];
    assert.equal(dex.moves.get('shadowball').category, 'Physical');
    assert.equal(dex.moves.get('bite').category, 'Special');
    assert.equal(dex.species.get('Clefairy').types.join('/'), 'Normal');
    assert.equal(dex.species.get('Deoxys-Speed').baseStats.spe, 180);
    checks.push('Gen III type split, pre-Fairy typing and Speed Deoxys reference');
    const firstAt = performance.now();
    let b = battle(samples);
    const firstBattleMs = performance.now() - firstAt;
    const readyRssKiB = process.memoryUsage().rss / 1024;
    try {
        const mon = b.p1.active[0];
        assert.equal(mon.maxhp, 155);
        assert.deepEqual(mon.storedStats, {atk: 140, def: 90, spa: 130, spd: 90, spe: 100});
        assert.equal(mon.hpType, 'Dark'); assert.equal(mon.hpPower, 70);
        assert.equal(mon.moveSlots[0].maxpp, 24); // Engine defaults to 3 PP Ups.
        assert.equal(dex.moves.get('flamethrower').pp, 15);
        assert.equal(b.choose('p1', 'move 99'), false);
        assert.equal(b.turn, 1); // A rejected choice cannot silently advance a turn.
        checks.push('Known stat/Hidden Power vector and automatic max-PP mismatch');
    } finally { b.destroy(); }
    b = battle([{...samples[0], nature: 'Adamant', happiness: 0}, samples[1]]);
    try {
        const mon = b.p1.active[0];
        assert.equal(mon.storedStats.atk, 154); assert.equal(mon.storedStats.spa, 117);
        const power = () => dex.moves.get('return').basePowerCallback.call(b, mon);
        assert.equal(power(), 1); mon.happiness = 255; assert.equal(power(), 102);
        checks.push('Nature rounding and friendship-dependent Return');
    } finally { b.destroy(); }
    b = battle([{...samples[0], ivs: uniform(0)}, samples[1]]);
    try {
        assert.equal(b.p1.active[0].hpType, 'Fighting');
        assert.equal(b.p1.active[0].hpPower, 30);
    } finally { b.destroy(); }
    const first = finish(samples);
    assert.deepEqual(finish(samples), first);
    checks.push('Seeded complete battle replay');
    const times = [];
    for (let i = 0; i < 100; ++i) {
        const at = performance.now(); finish(samples, [i + 1, 2, 3, 4]);
        times.push(performance.now() - at);
    }
    times.sort((a, b) => a - b);
    let privateProof = null;
    if (process.argv[3]) {
        const input = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
        assert.equal(input.mode, 'fresh-copies');
        assert(input.members.length >= 2 && input.members.length <= 6);
        const before = digest(input);
        let ppDifferences = 0;
        for (const member of input.members) {
            b = battle([member.set, samples[1]]);
            try {
                const mon = b.p1.active[0];
                assert.deepEqual([mon.maxhp, ...keys.slice(1).map(k => mon.storedStats[k])], member.stats);
                ppDifferences += mon.moveSlots.filter((m, i) => m.maxpp !== member.maxPp[i]).length;
            } finally { b.destroy(); }
        }
        const pair = input.members.slice(0, 2).map(m => m.set);
        const result = finish(pair); assert.deepEqual(finish(pair), result);
        assert.equal(digest(input), before);
        privateProof = {members: input.members.length, statsMatch: true,
            maxPpDifferences: ppDifferences, replay: result, semanticInputUnchanged: true};
        checks.push('Private semantic copy: six-stat comparison and repeatable battle');
    }
    const cancellation = await cancelledChild();
    checks.push('Separate stdio engine cancelled while awaiting choice');
    console.log(JSON.stringify({engine: 'pokemon-showdown@0.11.11', node: process.version,
        arch: process.arch, platform: process.platform, loadedMs, firstBattleMs, readyRssKiB, checks, replay: first,
        battles: 100, p50Ms: times[50], p95Ms: times[95], maxMs: times[99],
        maxRssKiB: process.resourceUsage().maxRSS, elapsedMs: performance.now() - started,
        cancellation, privateProof}, null, 2));
}
(process.argv[3] === '--wait' ? waitingChild() : main()).catch(error => {
    console.error(error); process.exitCode = 1;
});
