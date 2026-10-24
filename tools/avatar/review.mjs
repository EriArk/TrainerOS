// Local authoring check. Supply constructor directory, GLB, and output directory.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {createRequire} from 'node:module';
const [constructor,model,out]=process.argv.slice(2);
const require=createRequire(path.resolve(constructor,'package.json'));
const {chromium}=require('playwright');
await fs.mkdir(out,{recursive:true});
const html=`<html><body style="margin:0"><script type="importmap">{"imports":{"three":"/vendor/build/three.module.js","three/addons/":"/vendor/examples/jsm/"}}</script><script type="module">
import * as T from 'three';import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
const scene=new T.Scene();scene.background=new T.Color('#e5e8ee');const renderer=new T.WebGLRenderer({antialias:true});renderer.setSize(720,900);renderer.outputColorSpace=T.SRGBColorSpace;renderer.toneMapping=T.ACESFilmicToneMapping;document.body.append(renderer.domElement);
scene.add(new T.HemisphereLight(0xfff6e8,0x8995ad,2.3));for(const p of [[1,2,3],[-2,1,1]]){const l=new T.DirectionalLight(0xffffff,2);l.position.set(...p);scene.add(l);}
const camera=new T.PerspectiveCamera(30,720/900,.01,20);const g=await new GLTFLoader().loadAsync('/avatar.glb');scene.add(g.scene);const hairs=[];g.scene.traverse(o=>{if(o.name.startsWith('hair-'))hairs.push(o);});if(hairs.some(o=>o.name==='hair-swept'))hairs.forEach(o=>o.visible=o.name==='hair-swept');const mixer=new T.AnimationMixer(g.scene);
window.showPose=(name,angle=0)=>{mixer.stopAllAction();if(name){const clip=g.animations.find(a=>a.name===name);if(!clip)throw Error(name);const a=mixer.clipAction(clip);a.play();mixer.setTime(clip.duration*.5);}camera.position.set(Math.sin(angle)*2.1,.66,Math.cos(angle)*2.1);camera.lookAt(0,.52,0);renderer.render(scene,camera);return {clips:g.animations.map(a=>a.name),triangles:renderer.info.render.triangles}};window.ready=true;window.showPose('');
</script></body></html>`;
const server=http.createServer(async(req,res)=>{try{if(req.url==='/'){res.setHeader('Content-Type','text/html');res.end(html);return;}let p;if(req.url==='/avatar.glb')p=model;else if(req.url.startsWith('/vendor/'))p=path.join(constructor,'node_modules/three',req.url.slice(8));else{res.writeHead(404).end();return;}res.setHeader('Content-Type',p.endsWith('.js')?'text/javascript':'application/octet-stream');res.end(await fs.readFile(p));}catch{res.writeHead(500).end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({channel:'msedge',headless:true});const page=await browser.newPage({viewport:{width:720,height:900}});const errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:'+server.address().port);await page.waitForFunction(()=>window.ready,{},{timeout:60000});
let status;for(const [name,angle] of [['',0],['',3.14],['Arms_Up',0],['Squat',1.1],['Squat',3.14],['Step',.7]]){status=await page.evaluate(([n,a])=>window.showPose(n,a),[name,angle]);await page.screenshot({path:path.join(out,(name||'neutral')+'-'+angle+'.png')});}
await fs.writeFile(path.join(out,'review.json'),JSON.stringify({errors,status},null,2));await browser.close();server.close();if(errors.length)process.exitCode=1;
