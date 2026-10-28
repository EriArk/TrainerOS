"""Publish the actual Blender renders as an offline four-outfit review."""
import hashlib
import html
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
folder=ROOT/'assets/avatar/collections/starter-racing-v1'
manifest=json.loads((folder/'collection.json').read_text())
proof=json.loads((folder/'render-evidence.json').read_text())
assert proof['collectionSha256']==hashlib.sha256((folder/'collection.json').read_bytes()).hexdigest()
assert len(proof['shots'])==35
for name,digest in proof['shots'].items():
    assert hashlib.sha256((folder/'preview'/name).read_bytes()).hexdigest()==digest
labels={'starter-sundress':'Сарафан','starter-denim':'Футболка и джинсовые бриджи',
        'racing-classic':'Гоночный комплект','racing-rose':'Гоночный комплект · Rose'}
poses={'front':'Спереди','three-quarter':'¾','side':'Сбоку','back':'Сзади',
       'arms_up':'Руки вверх','step':'Шаг','squat':'Приседание','twist':'Поворот',
       'without-helmet':'Без шлема'}
buttons=''.join(f'<button data-pose="{key}">{label}</button>' for key,label in poses.items())
cards=''.join(f'<figure><a class="shot-link" data-set="{key}" href="preview/{key}-front.png"><img class="shot" data-set="{key}" src="preview/{key}-front.png" alt="{label}"></a><figcaption><span>{"Стартовый" if key.startswith("starter") else "Гонки"}</span><h2>{label}</h2><a href="{key}-outfit.glb" download>Модель комплекта ↓</a></figcaption></figure>' for key,label in labels.items())
page='''<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>TrainerOS — четыре новых костюма</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#202a2c;color:#f5efdf;font:16px/1.6 system-ui,sans-serif}
main{max-width:1440px;margin:auto;padding:36px 24px 70px}header{max-width:830px}small,figcaption span{color:#edb595;font-size:12px;letter-spacing:.1em;text-transform:uppercase}
h1{font-size:clamp(32px,5vw,58px);line-height:1.08;margin:14px 0}p{color:#c8cfcc}a{color:inherit}
.hero{width:100%;border-radius:20px;display:block;margin:28px 0}.controls{display:flex;flex-wrap:wrap;gap:8px;margin:25px 0}
button{border:1px solid #ffffff30;border-radius:99px;padding:9px 17px;color:inherit;background:#ffffff08;font:inherit;cursor:pointer}
button[aria-pressed=true]{background:#edb595;color:#202a2c}.cards{display:grid;grid-template-columns:repeat(4,1fr);gap:16px}
figure{margin:0;background:#ffffff08;border-radius:16px;overflow:hidden}.shot{display:block;width:100%}figcaption{padding:15px}h2{font-size:19px;line-height:1.3;margin:6px 0 14px}
.links{display:flex;flex-wrap:wrap;gap:22px;margin:28px 0}.note{border-left:3px solid #edb595;padding:5px 18px;max-width:900px}
@media(max-width:900px){.cards{grid-template-columns:repeat(2,1fr)}}@media(max-width:520px){main{padding:25px 12px}.cards{gap:8px}figcaption{padding:10px}h2{font-size:16px}.controls{gap:6px}button{padding:7px 12px}}
</style><main><header><small>TrainerOS · Примерка новых моделей</small><h1>На каждый день.<br>И на старт.</h1>
<p>Цельный сарафан со складочками и джинсовые бриджи с футболкой — два стартовых образа. Для гонок — два комбинезона со шлемами, перчатками и ботинками; второй с более мягким силуэтом и молочно-розовой отделкой.</p></header>
<a href="preview/four-outfits-lineup.png"><img class="hero" src="preview/four-outfits-lineup.png" alt="Четыре комплекта на персонажах TrainerOS"></a>
<div class="controls" aria-label="Ракурс">BUTTONS</div><section class="cards">CARDS</section>
<div class="links"><a href="starter-racing-wardrobe.blend">Blender-исходник ↓</a><a href="README.md">Состав и статус</a></div>
<p class="note">Кадры сняты с настоящих 3D-моделей. Это примерка для оценки внешнего вида. Выдача стартовых вещей, подключение к установленному гардеробу и проверка игровых движений ещё впереди.</p>
<script>function choose(pose){document.querySelectorAll('.shot,.shot-link').forEach(e=>{let p=pose;if(p==='without-helmet'&&e.dataset.set.startsWith('starter'))p='front';const path='preview/'+e.dataset.set+'-'+p+'.png';if(e.tagName==='IMG')e.src=path;else e.href=path;});document.querySelectorAll('[data-pose]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.pose===pose)));}document.querySelectorAll('[data-pose]').forEach(b=>b.onclick=()=>choose(b.dataset.pose));choose('front');</script></main></html>'''
(folder/'review.html').write_text(page.replace('BUTTONS',buttons).replace('CARDS',cards),encoding='utf8')
print('Review published:',folder/'review.html')
