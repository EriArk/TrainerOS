from pathlib import Path
import json,hashlib,shutil,html,re
root=Path(__file__).resolve().parents[2];job=root/'build/golf-outfits';out=root/'assets/avatar/collections/golf-v1';preview=out/'preview';preview.mkdir(exist_ok=True)
poses={'front':'Спереди','three-quarter':'Три четверти','back':'Сзади','side':'Сбоку','without-hat':'Без головного убора','wave':'Приветствие','arms_up':'Руки вверх','step':'Шаг','squat':'Приседание','sit':'Сидя'}
shots=[sex+'-'+pose for sex in ['mens','womens'] for pose in poses]+['golf-collection']
for name in shots:
    src=job/'render'/(name+'.png');assert src.exists(),name
    shutil.copy2(src,preview/src.name)
manifest=json.loads((out/'collection.json').read_text())
proof={'renderer':'Blender 5.2.3 / Cycles','date':'2026-10-28','collectionSha256':hashlib.sha256((out/'collection.json').read_bytes()).hexdigest(),'outfitSha256':{key:hashlib.sha256((out/(key+'-outfit.glb')).read_bytes()).hexdigest() for key in ['mens','womens']},'shots':{name:hashlib.sha256((preview/(name+'.png')).read_bytes()).hexdigest() for name in shots},'scope':'Art review on retained character; five sampled existing animation poses, not a golf swing or collision certification.'}
proof['hairFitManifestSha256']=hashlib.sha256((out/'hair-fit/manifest.json').read_bytes()).hexdigest()
proof['artRevision']=manifest['artRevision']
(out/'render-evidence.json').write_text(json.dumps(proof,indent=2)+'\n')
labels={'polo-short-sleeve':'Поло с рукавами','polo-sleeveless':'Поло без рукавов','golf-shorts':'Шорты','pleated-skirt':'Юбка','ankle-socks':'Короткие носки','crew-socks':'Носки повыше','golf-shoes-classic':'Обувь / зелёная','golf-shoes-sport':'Обувь / персиковая','golf-cap':'Кепка','golf-visor':'Козырёк'}
buttons=''.join(f'<button type="button" data-pose="{key}">{name}</button>' for key,name in poses.items())
items=''.join(f'<a href="{key}.glb" download><span>{name}</span><small>GLB ↓</small></a>' for key,name in labels.items())
page='''<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>TrainerOS — гольфовая коллекция</title>
<style>*{box-sizing:border-box}body{margin:0;background:#172822;color:#f4f0df;font:16px/1.55 system-ui,sans-serif}main{max-width:1200px;margin:auto;padding:32px 22px 70px}header{display:flex;justify-content:space-between;gap:24px;align-items:center}.eyebrow{color:#efa58b;font-size:12px;letter-spacing:.18em;text-transform:uppercase}h1{font-size:clamp(30px,5vw,56px);line-height:1.1;margin:12px 0}p{max-width:820px;color:#c8d1c8}a{color:inherit}.hero{width:100%;border-radius:22px;margin:24px 0;display:block}.swatches{display:flex;gap:9px}.swatches i{width:23px;height:23px;border-radius:50%;border:1px solid #ffffff30}.controls{display:flex;flex-wrap:wrap;gap:8px;margin:18px 0}button{font:inherit;border:1px solid #ffffff20;background:#ffffff08;border-radius:999px;padding:8px 15px;color:inherit;cursor:pointer}button[aria-pressed=true]{background:#efa58b;color:#172822}.pair{display:grid;grid-template-columns:1fr 1fr;gap:18px}.pair figure{margin:0;background:#ffffff08;border-radius:18px;overflow:hidden}.pair img{display:block;width:100%}.pair figcaption{padding:12px 18px}.files{display:grid;grid-template-columns:repeat(2,1fr);gap:10px;margin:18px 0}.files a{display:flex;justify-content:space-between;align-items:center;padding:14px 18px;border:1px solid #ffffff20;border-radius:12px;text-decoration:none}.files a:hover{background:#ffffff0c}small{color:#efa58b}.links{display:flex;flex-wrap:wrap;gap:22px;margin:22px 0}.note{border-left:3px solid #efa58b;padding:8px 18px;margin-top:26px}@media(max-width:600px){main{padding:22px 12px}.pair{gap:8px}.files{grid-template-columns:1fr}header{display:block}.swatches{margin-top:14px}.pair figcaption{padding:8px;font-size:14px}}</style>
<main><header><div><div class="eyebrow">TrainerOS · примерка 01</div><h1>На поле для гольфа</h1></div><div class="swatches" aria-label="Молочный, зелёный, персиковый"><i style="background:#f4f0df"></i><i style="background:#234f43"></i><i style="background:#efa58b"></i></div></header>
<p>Два образа, десять отдельных вещей. Поло, низ, носки, обувь и головной убор можно комбинировать. Под юбкой — существующие шорты персонажа.</p>
<img class="hero" src="preview/golf-collection.png" alt="Оба гольфовых комплекта на моделях">
<h2>Примерка со всех сторон</h2><div class="controls">BUTTONS</div><div class="pair"><figure><a id="mens-link" href="preview/mens-front.png"><img id="mens" src="preview/mens-front.png" alt="Мужской комплект"></a><figcaption>Поло с рукавами · шорты · кепка</figcaption></figure><figure><a id="womens-link" href="preview/womens-front.png"><img id="womens" src="preview/womens-front.png" alt="Женский комплект"></a><figcaption>Поло без рукавов · юбка · козырёк</figcaption></figure></div>
<h2>Каждая вещь отдельно</h2><div class="files">ITEMS</div><div class="links"><a href="golf-wardrobe.blend">Blender-исходник ↓</a><a href="mens-outfit.glb">Мужской комплект ↓</a><a href="womens-outfit.glb">Женский комплект ↓</a><a href="README.md">Инструкция</a></div>
<p class="note">Это реальные рендеры подготовленных моделей. Кепка показана с короткой стрижкой, козырёк — с каре. Коллекция пока на примерке: подключение к системному гардеробу и проверка игровых движений гольфа — следующий этап.</p>
<script>const buttons=document.querySelectorAll('[data-pose]');function choose(pose){for(const sex of ['mens','womens']){const path='preview/'+sex+'-'+pose+'.png';document.getElementById(sex).src=path;document.getElementById(sex+'-link').href=path;}for(const button of buttons)button.setAttribute('aria-pressed',String(button.dataset.pose===pose));}for(const button of buttons)button.onclick=()=>choose(button.dataset.pose);choose('front');</script></main></html>'''.replace('BUTTONS',buttons).replace('ITEMS',items)
version=proof['collectionSha256'][:12]
page=page.replace('примерка 01',f"примерка {manifest['artRevision']:02d}")
page=re.sub(r'(preview/[a-z_-]+\.png)',lambda m:m[0]+'?v='+version,page)
page=page.replace("+pose+'.png'","+pose+'.png?v="+version+"'")
(out/'review.html').write_text(page,encoding='utf8')
for path in [out/'README.md',root/'docs/AVATAR_GOLF_20261028.md',out/'review.html']:
    content=path.read_text(encoding='utf8')
    links=re.findall(r'\]\(([^)#]+)(?:#[^)]*)?\)',content) if path.suffix=='.md' else re.findall(r'(?:src|href)="([^"]+)"',content)
    for link in links:
        if ':' not in link and not link.startswith('#'):assert (path.parent/link.split('?')[0]).exists(),(path,link)
print('Published 21 real renders and all model links:',out/'review.html')
