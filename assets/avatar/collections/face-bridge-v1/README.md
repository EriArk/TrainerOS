# Переносица — авторская правка глубины

[Сравнение до/после](review.html). Исходные одобренные файлы и runtime pack revision 7
сохранены. Это примерка общего лица, ещё не принятие и не установка на устройства.

Узкая правка только середины оставляла две вертикальные борозды у внутренних
уголков глаз. Здесь поднята глубина всей области между ними. Переход к лбу,
носу и внешним участкам лица ограничен локальной маской. Координаты X/Y,
исходные UV, материалы, скелет и клипы сохраняются.

`prepared-face.glb` и все двенадцать `eyes-*.glb` — согласованный набор.
Использовать новое лицо со старыми глазами нельзя: более ровная кожа может
перекрыть внутренние уголки. Глаза, веки, радужки и брови получают смещение
фактических треугольников лица. Всем слоям глаз добавлен одинаковый зазор 1 мм
на различие триангуляции. В готовых аватарах инструмент также переносит
коррекции выражений и символические глаза; неиспользуемые варианты не меняются.
Длинные узкие треугольники глаз разделены до рёбер не длиннее 3 мм в проекции
на лицо, включая проверяемые выражения. Контур сохраняется; UV и привязка новых
вершин интерполируются. Это предотвращает пересечение кожи серединой треугольника
при корректном положении его углов. Исходные вершины остаются первыми в массивах.

Воспроизведение из корня репозитория (Python, numpy, scipy):

```powershell
py -3.12 tools/avatar/repair_face_bridge.py assets/avatar/approved/2026-10-26/chibi-eyes/avatar-prepared-face.glb assets/avatar/collections/face-bridge-v1/prepared-face.glb --eye-set --report assets/avatar/collections/face-bridge-v1/repair.json
py -3.12 tools/avatar/check_face_bridge.py
```

Для примерки на собранном аватаре используйте исходный resolved GLB revision 7:

```powershell
py -3.12 tools/avatar/repair_face_bridge.py resolved.glb review-avatar.glb
py -3.12 tools/avatar/check_face_bridge.py --resolved-before resolved.glb --resolved-after review-avatar.glb
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --python tools/avatar/render_face_bridge.py -- --before resolved.glb --after review-avatar.glb --output preview
```

Для проверки закрытых глаз добавить `--after-only --expression eyes_closed`.
`repair.json` хранит хеши исходников и экспортов, `validation.json` — фактические
результаты проверки глубины середины и боковых полос, зазоров и сохранности.
Рендеры сделаны в Blender под одинаковым светом. Они не доказывают все сочетания
выражений, движение в игре или отображение Qt/Godot на Odin/Flip.
