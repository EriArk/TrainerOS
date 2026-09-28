'use strict';
// One battle per child, bounded newline JSON. No raw Showdown/eval protocol.
const {createEngine} = require('./emerald-engine.cjs');
const engine = createEngine(process.argv[2]);
let session, buffer = Buffer.alloc(0), requests=0, closing=false;
function send(value) { process.stdout.write(JSON.stringify(value)+'\n'); }
function close() { if (closing) return; closing=true; session?.close(); process.stdin.pause(); }
process.stdin.on('data', chunk => {
    if (closing) return;
    buffer=Buffer.concat([buffer,chunk]);
    if (buffer.length>16384) { send({type:'error',error:'Input too large'});close();return; }
    let at;
    while (!closing && (at=buffer.indexOf(10))>=0) {
        const line=buffer.subarray(0,at);buffer=buffer.subarray(at+1);
        try {
            if (++requests>1205) throw new Error('Request limit exceeded');
            const input=JSON.parse(line.toString('utf8'));
            if (input.command==='cancel') {send({type:'cancelled'});close();return;}
            if (input.command==='start' && !session) {session=engine.start(input);send(session.state());}
            else if (input.command==='turn' && session) send(session.turn(input.turn,input.moves,input.request));
            else throw new Error('Unsupported command');
        } catch(error) {send({type:'error',error:error.message});close();}
    }
});
process.stdin.on('end',close);
send({type:'ready',protocol:1});
