# Каталог игр с передачей геймпада

**Решение владельца, 12 октября 2026:** полный каталог отложен до следующего
этапа. Существующие данные сохраняются. Сейчас приоритет у общих подключений
платформ и PS2 с обоими режимами управления; расширять список игр не требуется.

Уточнение владельца, 11 октября 2026: для PS1, PS2 и GameCube нужны оба режима —
одновременная игра отдельными геймпадами и передача одного геймпада по очереди.
Также нужен полный список игр, которым требуется второй режим.

Отдельный [разбор готовых механизмов](HOTSEAT_RUNTIME_RESEARCH.md) фиксирует
ограничения PS2, RetroArch и Dolphin. Наличие игрового режима в этом каталоге
не означает, что его удалённая передача контроллера уже реализована.

## Собранные данные — 11 октября 2026

В [машиночитаемом каталоге](../data/hotseat-catalog.json) уже **66 записей о режимах
43 игр на конкретных платформах**:
**48** записей с подтверждённым общим устройством,
**15** с другой схемой и **3** требующих уточнения.
Это факты из руководств и публикаций издателей, не проверка эмуляторов.
**Полный список ещё не составлен; HS-01 / #126 остаётся открытым.**

Каждая строка ограничена указанным изданием и режимом. «На выбор» означает, что
можно использовать общий или отдельные контроллеры. «Разрешена» подтверждает общий
контроллер, но не утверждает отсутствие других вариантов. «Другая схема» относится
к указанному режиму, а не ко всей игре. Неизвестный предел обозначен `?`.
Ёмкость игры не равна ёмкости подготовленного сетевого профиля TrainerOS.

JSON пока не подключён к runtime. У всех записей `runtime_verified: false`, правила
автоматического сопоставления пусты. Названия нужны человеку; применять профиль по
одному совпадению названия или выдавать права на сейв по этому каталогу нельзя.
На PS1 уже установлен отдельный двухместный режим передачи P1 с обычными
приглашениями и Home; [Hot Shots Golf 2 прошла реальные ходы на двух устройствах](emulators/pcsx-rearmed.md#ordinary-hotseat-delivery--12-october-2026).
Это не проверка записей этого каталога и не автоматическое сопоставление по имени.
GameCube также получил [двухместную передачу P1 и проверку Mario Golf Stroke Play](emulators/dolphin.md#ordinary-gamecube-hotseat-delivery--12-october-2026).
Это отдельное свидетельство для точного образа; таблица и JSON пока не выполняют
сопоставление и сохраняют прежний уровень проверки. PS2, полный каталог
и остальные режимы остаются в HS-01.

## PlayStation

| Игра / издание | Режим | Людей | Геймпадов / устройств | Передача | Подтверждение и ограничения |
| --- | --- | --- | --- | --- | --- |
| Actua Golf 2 — Europe | Strokeplay | 2–4 | 1–4 | На выбор | [No. of Players; Game Styles / Strokeplay, Matchplay](https://archive.org/download/SonyPlaystationManuals/Actua%20Golf%202%20%28Europe%29_djvu.txt) Один общий, два общих или отдельные через Multitap. |
| Actua Golf 2 — Europe | Matchplay | 2 | 1–2 | На выбор | [No. of Players; Game Styles / Strokeplay, Matchplay](https://archive.org/download/SonyPlaystationManuals/Actua%20Golf%202%20%28Europe%29_djvu.txt) |
| FOX Sports Golf '99 — USA | Strokeplay | 2–4 | 1–4 | На выбор | [No. of Players; Game Styles](https://archive.org/download/SonyPlaystationManuals/FOX%20Sports%20Golf%20%2799%20%28USA%29_djvu.txt) Общий или отдельные контроллеры; Matchplay не включён в эту строку. |
| Backstreet Billiards — USA | VS / bank shot | 2 | 1–2 | На выбор | [PDF p. 13: bank shot](https://www.videogamemanual.com/ps1/Backstreet%20Billiards%20%28USA%29.pdf) При общем контроллере розыгрыш начального удара пропускается; остальные режимы не обобщены. |
| Brunswick Circuit Pro Bowling — USA | Multiplayer | 2–6 | 1–6 | На выбор | [PDF p. 8 / printed p. 6: Multiplayer](https://www.videogamemanual.com/ps1/Brunswick%20Circuit%20Pro%20Bowling%20%28USA%29.pdf) Отдельные контроллеры до шести требуют двух Multitap; это ёмкость игры, не готового профиля TrainerOS. |
| Brunswick Circuit Pro Bowling 2 — USA | Bowler Registry / Change Controllers | 2–? | 1–4 | На выбор | [PDF p. 13 / printed p. 11: Change Controllers](https://www.videogamemanual.com/ps1/Brunswick%20Circuit%20Pro%20Bowling%202%20%28USA%29.pdf) Все люди могут использовать один контроллер; предел участников пока не установлен, число геймпадов его не заменяет. |
| Wheel of Fortune — USA | Local game | 2–3 | 1 | Требуется | [Controls: human players / Controller number 1](https://archive.org/download/SonyPlaystationManuals/Wheel%20of%20Fortune%20%28USA%29_djvu.txt) Второй контроллер явно отключён игрой. |
| Micro Machines V3 — USA | Party Play / Time Trial Single Race | 2–? | 1–? | Разрешена | [Party Play Mode: Time Trial Single Race; Time Trial Tournament; Player Share](https://archive.org/download/SonyPlaystationManuals/Micro%20Machines%20V3%20%28USA%29_djvu.txt) Участники едут по очереди. Предел группы не выводится из соседнего Knockout. |
| Micro Machines V3 — USA | Party Play / Time Trial Tournament | 2–? | 1–? | Разрешена | [Party Play Mode: Time Trial Single Race; Time Trial Tournament; Player Share](https://archive.org/download/SonyPlaystationManuals/Micro%20Machines%20V3%20%28USA%29_djvu.txt) |
| Micro Machines V3 — USA | Player Share | 4/6/8 | 2–4 | Другая схема | [Party Play Mode: Time Trial Single Race; Time Trial Tournament; Player Share](https://archive.org/download/SonyPlaystationManuals/Micro%20Machines%20V3%20%28USA%29_djvu.txt) Двое одновременно делят кнопки одного геймпада; общий исключительный владелец здесь не подходит. Только 4/6/8 участников. |
| Hi-Octane: The Track Fights Back! — USA | Hot Seat | 2–8 | Не установлен | Уточнить | [Race Mode / Hot Seat; Hot Seat Options](https://archive.org/download/SonyPlaystationManuals/Hi-Octane%20-%20The%20Track%20Fights%20Back%21%20%28USA%29_djvu.txt) Смена игрока по игровому таймеру подтверждена. Назначение единственного порта не указано прямо; проверить в игре. |
| Chessmaster II — USA | Vs. Human | 2 | 2 | Другая схема | [PDF p. 16: Players](https://www.videogamemanual.com/ps1/Chessmaster%20II%20%28USA%29.pdf) Руководство прямо требует два контроллера даже при поочерёдных шахматных ходах. |
| Worms — USA | Local teams | 2–4 | 1–2 | На выбор | [Team Select; PDF p. 18: Note 2](https://www.videogamemanual.com/ps1/Worms%20%28USA%29.pdf) Оба порта активны для удобства передачи управления. Это не два независимо закреплённых за командами контроллера. |
| Hot Shots Golf — USA | Stroke Play | 2–4 | 1–4 | На выбор | [PDF pp. 8, 10: Multitap; Game Selection](https://www.videogamemanual.com/ps1/Hot%20Shots%20Golf%20%28USA%29.pdf) Один или два общих контроллера; четыре отдельных через Multitap. |
| Hot Shots Golf — USA | Match Play | 2 | 1–2 | На выбор | [PDF pp. 8, 10: Multitap; Game Selection](https://www.videogamemanual.com/ps1/Hot%20Shots%20Golf%20%28USA%29.pdf) Не путать с VS Mode против CPU и одиночным Tournament. |
| Peter Jacobsen's Golden Tee Golf — USA | Strokes | 2–4 | 1–4 | На выбор | [PDF pp. 5, 10–12: controller selection; Game Modes](https://www.videogamemanual.com/ps1/Peter%20Jacobsen%27s%20Golden%20Tee%20Golf%20%28USA%29.pdf) Общий P1, два общих или четыре отдельных через Multitap. Tournament и Shadow Game рассчитаны на одного активного игрока. |
| Peter Jacobsen's Golden Tee Golf — USA | Skins | 2–4 | 1–4 | На выбор | [PDF pp. 5, 10–12: controller selection; Game Modes](https://www.videogamemanual.com/ps1/Peter%20Jacobsen%27s%20Golden%20Tee%20Golf%20%28USA%29.pdf) Общий P1, два общих или четыре отдельных через Multitap. Tournament и Shadow Game рассчитаны на одного активного игрока. |
| Peter Jacobsen's Golden Tee Golf — USA | Bingo-Bango-Bongo | 2–4 | 1–4 | На выбор | [PDF pp. 5, 10–12: controller selection; Game Modes](https://www.videogamemanual.com/ps1/Peter%20Jacobsen%27s%20Golden%20Tee%20Golf%20%28USA%29.pdf) Общий P1, два общих или четыре отдельных через Multitap. Tournament и Shadow Game рассчитаны на одного активного игрока. |
| Peter Jacobsen's Golden Tee Golf — USA | Speed Golf | 2–4 | 1–4 | На выбор | [PDF pp. 5, 10–12: controller selection; Game Modes](https://www.videogamemanual.com/ps1/Peter%20Jacobsen%27s%20Golden%20Tee%20Golf%20%28USA%29.pdf) Общий P1, два общих или четыре отдельных через Multitap. Tournament и Shadow Game рассчитаны на одного активного игрока. |
| Peter Jacobsen's Golden Tee Golf — USA | Club Roulette | 2–4 | 1–4 | На выбор | [PDF pp. 5, 10–12: controller selection; Game Modes](https://www.videogamemanual.com/ps1/Peter%20Jacobsen%27s%20Golden%20Tee%20Golf%20%28USA%29.pdf) Общий P1, два общих или четыре отдельных через Multitap. Tournament и Shadow Game рассчитаны на одного активного игрока. |
| Jeopardy! — USA | Normal | 2–3 | 2–3 | Другая схема | [PDF p. 7: Normal](https://www.videogamemanual.com/ps1/Jeopardy%21%20%28USA%29.pdf) Каждому нужен отдельный контроллер для сигнала ответа; для троих требуется Multitap. |
| Jeopardy! 2nd Edition — USA | Local game | 2–3 | 2–3 | Другая схема | [PDF p. 5: controllers](https://www.videogamemanual.com/ps1/Jeopardy%21%202nd%20Edition%20%28USA%29.pdf) Каждому человеку нужен отдельный контроллер; для троих требуется Multitap. |

## PlayStation 2

| Игра / издание | Режим | Людей | Геймпадов / устройств | Передача | Подтверждение и ограничения |
| --- | --- | --- | --- | --- | --- |
| Chessmaster — USA | Versus Mode | 2 | 1–2 | На выбор | [PDF p. 24: section 6, Versus Mode](https://www.videogamemanual.com/PS2/Chessmaster%20%28USA%29.pdf) Речь о PS2 Chessmaster, а не об автоматическом переносе этого правила на PS1 Chessmaster II. |
| Hot Shots Golf 3 — USA | Multi-Player / Stroke Play | 2–4 | 1–4 | На выбор | [PDF pp. 9, 22: Player Select; Multi-Player Golf](https://www.videogamemanual.com/PS2/Hot%20Shots%20Golf%203%20%28USA%29.pdf) Каждому участнику можно назначить тот же контроллер; отдельные четыре требуют Multitap. |
| Monopoly — USA (2008) | Local game | 2–4 | 1–4 | На выбор | [PDF p. 7 / printed p. 5: Choose how many controllers](https://www.videogamemanual.com/PS2/Monopoly%20%28USA%29.pdf) Издание с Passport; не Monopoly Party. |
| Monopoly Party — USA | Classic | 2–4 | 1–4 | На выбор | [PDF pp. 4, 7, 21, 27: Classic/Party; Controller Setup; Auctions](https://www.videogamemanual.com/PS2/Monopoly%20Party%20%28USA%29.pdf) Аукционы с общим контроллером тоже идут по очереди. |
| Monopoly Party — USA | Party | 2–4 | 2–4 | Другая схема | [PDF pp. 4, 7, 21, 27: Classic/Party; Controller Setup; Auctions](https://www.videogamemanual.com/PS2/Monopoly%20Party%20%28USA%29.pdf) Каждому человеку нужен свой контроллер; действия происходят одновременно. |
| Outlaw Golf 2 — USA | Exhibition / Stroke | 2–4 | 1–4 | На выбор | [PDF p. 16: Player Setup](https://www.videogamemanual.com/PS2/Outlaw%20Golf%202%20%28USA%29.pdf) Четырёх участников можно назначить одному контроллеру; Tour не включён в это подтверждение. |
| Flipnic: Ultimate Pinball — USA | 2P Play | 2 | 2 | Другая схема | [PDF pp. 5, 14: 2P Play](https://www.videogamemanual.com/PS2/Flipnic-%20Ultimate%20Pinball%20%28USA%29.pdf) Для 2P Stage руководство требует контроллеры в обоих портах. |
| Eagle Eye Golf — USA | Multiplayer modes | 2–4 | Не установлен | Уточнить | [PDF pp. 5, 20: controllers; multiplayer modes](https://www.videogamemanual.com/PS2/Eagle%20Eye%20Golf%20%28USA%29.pdf) Руководство описывает передачу двух контроллеров четырьмя людьми. Один общий контроллер пока не подтверждён; не объявлять его невозможным. |
| Hot Shots Golf: Fore! — USA | Multi-Player Golf | 2–4 | Не установлен | Уточнить | [PDF pp. 9, 22: Player Select; Multiplayer Mode](https://www.videogamemanual.com/PS2/Hot%20Shots%20Golf-%20FORE%21%20%28USA%29.pdf) Описана передача двух подключённых контроллеров. Правило одного контроллера из Hot Shots Golf 3 не переносится автоматически. |
| Tiger Woods PGA Tour 2003 — USA | Stroke Play | 2–4 | 1–4 | На выбор | [PDF pp. 4, 19: sharing controllers; Stroke Play](https://www.videogamemanual.com/PS2/Tiger%20Woods%20PGA%20Tour%202003%20%28USA%29.pdf) Подтверждение не переносится на одновременный Speed Golf и на другие ежегодные выпуски. |
| Risk: Global Domination — USA | Local multiplayer | 2–6 | 2 | Другая схема | [PDF p. 6: Multiplayer Notes](https://www.videogamemanual.com/PS2/Risk-%20Global%20Domination%20%28USA%29.pdf) Передаются ДВА контроллера: P1 атакующему, P2 защищающемуся. Нужны отдельные владельцы двух портов; один общий P1 не покрывает этот режим. |
| Real World Golf — USA | Multiplayer rounds / Party Golf | 2–4 | Не установлен | Другая схема | [PDF pp. 7, 11–13: Gametrak; Current Player](https://www.videogamemanual.com/PS2/Real%20World%20Golf%20%28USA%29.pdf) Игроки передают перчатки Gametrak и подтверждают готовность педалью. Сетевую эмуляцию этой периферии не считать реализованной. |
| Wheel of Fortune — USA | Normal Game | 2–3 | 1 | Требуется | [PDF pp. 4, 6: Getting Started; Normal Game](https://www.videogamemanual.com/PS2/Wheel%20of%20Fortune%20%28USA%29.pdf) Недостающих до трёх участников заменяет CPU; Tournament имеет отдельные условия допуска. |
| Wheel of Fortune — USA | Two Player Only | 2 | 1 | Требуется | [PDF pp. 4, 6: Getting Started; Normal Game](https://www.videogamemanual.com/PS2/Wheel%20of%20Fortune%20%28USA%29.pdf) Двое без третьего компьютерного участника. |
| Worms 3D — USA | Local multiplayer | 2–4 | 1 | Требуется | [PDF pp. 4, 12, 22: controller port 1; Hot Seat Time; multiplayer](https://www.videogamemanual.com/PS2/Worms%203D%20%28USA%29.pdf) Между ходами предусмотрено время передачи; любое нажатие завершает эту паузу. Версия GameCube проверяется отдельно. |
| Worms Forts: Under Siege — USA | Local multiplayer | 2–4 | 1–? | Разрешена | [PDF pp. 13, 24, 27: Hot Seat Time; multiplayer](https://www.videogamemanual.com/PS2/Worms%20Forts-%20Under%20Siege%21%20%28USA%29.pdf) Передача контроллера прямо описана; альтернативная схема портов не установлена. Нажатие завершает паузу передачи. |
| Ultimate Board Game Collection — USA | Game Start Options / multiple Human slots | 2–? | 1–? | Разрешена | [PDF pp. 7–8: Game Start Options](https://www.videogamemanual.com/PS2/Ultimate%20Board%20Game%20Collection%20%28USA%29.pdf) Только игры сборника, позволяющие назначить несколько Human. Перечень таких игр и предел участников ещё не установлены; не считать все 20 игр многопользовательскими. |
| Ultimate Board Game Collection — USA | Naval Battle | 1 | 1 | Другая схема | [PDF pp. 7–8: Game Start Options](https://www.videogamemanual.com/PS2/Ultimate%20Board%20Game%20Collection%20%28USA%29.pdf) Руководство прямо ограничивает этот вариант одним человеком; режим против компьютера не является Hotseat. |
| Ultimate Board Game Collection — USA | Dominoes | 1 | 1 | Другая схема | [PDF pp. 7–8: Game Start Options](https://www.videogamemanual.com/PS2/Ultimate%20Board%20Game%20Collection%20%28USA%29.pdf) Руководство прямо ограничивает этот вариант одним человеком; режим против компьютера не является Hotseat. |

## GameCube

| Игра / издание | Режим | Людей | Геймпадов / устройств | Передача | Подтверждение и ограничения |
| --- | --- | --- | --- | --- | --- |
| Mario Golf: Toadstool Tour — USA | Stroke Play | 2–4 | 1–4 | На выбор | [PDF pp. 3, 14: Connecting Controllers; Stroke Play](https://archive.org/download/NintendoGameCubeManuals/Mario%20Golf%20-%20Toadstool%20Tour%20%28USA%29_text.pdf) Правило общего контроллера из вступления и Stroke Play из Play Modes; прочие режимы проверяются отдельно. |
| Monopoly Party — USA | Classic | 2–4 | 1–4 | На выбор | [PDF pp. 5, 13, 16: Classic/Party; Controller Setup; Auctions](https://archive.org/download/NintendoGameCubeManuals/Monopoly%20Party%20%28USA%29_text.pdf) |
| Monopoly Party — USA | Party | 2–4 | 2–4 | Другая схема | [PDF pp. 5, 13, 16: Classic/Party; Controller Setup; Auctions](https://archive.org/download/NintendoGameCubeManuals/Monopoly%20Party%20%28USA%29_text.pdf) Общий геймпад недопустим для этого режима. |
| Backyard Baseball — USA | Home Run Derby | 2–8 | 1 | Требуется | [PDF p. 10: Home Run Derby / Contestants](https://archive.org/download/NintendoGameCubeManuals/Backyard%20Baseball%20%28USA%29_text.pdf) Восемь участников относятся к Home Run Derby. Обычный матч использует два отдельных геймпада. |
| Madagascar — USA | Shuffleboard | 2 | 1–? | Разрешена | [Minigames / Shuffleboard](https://archive.org/download/NintendoGameCubeManuals/Madagascar%20%28USA%29_djvu.txt) Не переносить на мини-гольф и другие мини-игры без отдельного подтверждения. |
| Super Monkey Ball — USA | Main Game / Normal | 2–4 | 1–? | Разрешена | [PDF pp. 6, 9, 13, 18: Number of Players; Alternating games](https://www.gamingalexandria.com/gc/Super%20Monkey%20Ball/Super%20Monkey%20Ball%20-%20Clearscan.pdf) Общий контроллер разрешён; наличие отдельных контроллеров для этого режима здесь не установлено. |
| Super Monkey Ball — USA | Monkey Target | 2–4 | 1–? | Разрешена | [PDF pp. 6, 9, 13, 18: Number of Players; Alternating games](https://www.gamingalexandria.com/gc/Super%20Monkey%20Ball/Super%20Monkey%20Ball%20-%20Clearscan.pdf) Общий контроллер разрешён; наличие отдельных контроллеров для этого режима здесь не установлено. |
| Super Monkey Ball — USA | Monkey Bowling / Standard | 2–4 | 1–? | Разрешена | [PDF pp. 6, 9, 13, 18: Number of Players; Alternating games](https://www.gamingalexandria.com/gc/Super%20Monkey%20Ball/Super%20Monkey%20Ball%20-%20Clearscan.pdf) Общий контроллер разрешён; наличие отдельных контроллеров для этого режима здесь не установлено. |
| Super Monkey Ball — USA | Monkey Golf / Stroke Play | 2–4 | 1–? | Разрешена | [PDF pp. 6, 9, 13, 18: Number of Players; Alternating games](https://www.gamingalexandria.com/gc/Super%20Monkey%20Ball/Super%20Monkey%20Ball%20-%20Clearscan.pdf) Общий контроллер разрешён; наличие отдельных контроллеров для этого режима здесь не установлено. |
| Super Monkey Ball — USA | Monkey Golf / Match Play | 2 | 1–? | Разрешена | [PDF pp. 6, 9, 13, 18: Number of Players; Alternating games](https://www.gamingalexandria.com/gc/Super%20Monkey%20Ball/Super%20Monkey%20Ball%20-%20Clearscan.pdf) Общий контроллер разрешён; наличие отдельных контроллеров для этого режима здесь не установлено. |
| Super Monkey Ball — USA | Monkey Billiards / VS | 2 | 1–? | Разрешена | [PDF pp. 6, 9, 13, 18: Number of Players; Alternating games](https://www.gamingalexandria.com/gc/Super%20Monkey%20Ball/Super%20Monkey%20Ball%20-%20Clearscan.pdf) Общий контроллер разрешён; наличие отдельных контроллеров для этого режима здесь не установлено. |
| Super Monkey Ball 2 — USA | Challenge Mode | 2–4 | 1–4 | На выбор | [PDF pp. 7, 10: alternating controls; Party Game descriptions](https://www.gamingalexandria.com/gc/Super%20Monkey%20Ball%202/Super%20Monkey%20Ball%202%20-%20Clearscan.pdf) Выбор персонажа выполняется тем контроллером, которым игрок будет пользоваться; одновременные режимы не включены. |
| Super Monkey Ball 2 — USA | Monkey Bowling 2 | 2–4 | 1–4 | На выбор | [PDF pp. 7, 10: alternating controls; Party Game descriptions](https://www.gamingalexandria.com/gc/Super%20Monkey%20Ball%202/Super%20Monkey%20Ball%202%20-%20Clearscan.pdf) Выбор персонажа выполняется тем контроллером, которым игрок будет пользоваться; одновременные режимы не включены. |
| Super Monkey Ball 2 — USA | Monkey Golf 2 / Stroke | 2–4 | 1–4 | На выбор | [PDF pp. 7, 10: alternating controls; Party Game descriptions](https://www.gamingalexandria.com/gc/Super%20Monkey%20Ball%202/Super%20Monkey%20Ball%202%20-%20Clearscan.pdf) Выбор персонажа выполняется тем контроллером, которым игрок будет пользоваться; одновременные режимы не включены. |
| Super Monkey Ball 2 — USA | Monkey Billiards 2 / VS | 2 | 1–2 | На выбор | [PDF pp. 7, 10: alternating controls; Party Game descriptions](https://www.gamingalexandria.com/gc/Super%20Monkey%20Ball%202/Super%20Monkey%20Ball%202%20-%20Clearscan.pdf) Выбор персонажа выполняется тем контроллером, которым игрок будет пользоваться; одновременные режимы не включены. |
| Swingerz Golf — USA | Stroke Play | 2–4 | 1–4 | На выбор | [Connecting Controllers to Controller Sockets; Game Modes / Stroke Play](https://archive.org/download/NintendoGameCubeManuals/Swingerz%20Golf%20%28USA%29.pdf) Руководство допускает меньше контроллеров, чем игроков; C Stick используется для удара. |
| Mario Party 7 — USA | 8-Player Minigames / 4-Team | 8 | 4 | Другая схема | [PDF p. 11: Playing 4-Team Battles / 8-Player Minigames](https://www.gamingalexandria.com/gc/Mario%20Party%207/Mario%20Party%207%20-%20Cleartype.pdf) Два человека одновременно используют разные стороны одного геймпада. |
| Pac-Man Vs. — USA | Versus | 2–4 | Не установлен | Другая схема | [PDF pp. 1, 2, 4: setup; swapping](https://www.gamingalexandria.com/gc/Pac-Man%20Vs/Pac-Man%20Vs%20-%20Cleartype.pdf) Меняются ролями GBA и обычного контроллера; требуются отдельный экран и связка GBA + GameCube. Это не передача единственного P1. |
| Tony Hawk's Underground — USA | 2-Player / HORSE | 2 | 2 | Другая схема | [PDF pp. 7–8: 2-Player; HORSE](https://archive.org/download/NintendoGameCubeManuals/Tony%20Hawk%27s%20Underground%20%28USA%29_text.pdf) Для входа в 2-Player требуются два контроллера. Очередность попыток не доказывает общий P1. |

## PSP

| Игра / издание | Режим | Людей | Геймпадов / устройств | Передача | Подтверждение и ограничения |
| --- | --- | --- | --- | --- | --- |
| History: Egypt - Engineering an Empire — PSP release described by developer, 2010 | Hotseat battle / campaign | 2 | 1 | Требуется | [Developer announcement: two-player head-to-head on the same PSP](https://blog.playstation.com/?p=30192) Кампания поддерживает совместную игру против CPU и соперничество; реальная сетевая сессия TrainerOS не проверена. |

## Vita

| Игра / издание | Режим | Людей | Геймпадов / устройств | Передача | Подтверждение и ограничения |
| --- | --- | --- | --- | --- | --- |
| Worms Revolution Extreme — Vita release described by Team17, 2013 | Local pass-and-play | 2–4 | 1 | Требуется | [Team17 announcement: multiplayer on the same Vita](https://blog.playstation.com/archive/2013/10/08/worms-revolution-extreme-launches-ps-vita-assault-tomorrow) Подтверждён игровой режим; сетевой запуск Vita3K и перенос touch-функций не проверены. |
| Zen Pinball 2 — Vita, hands-on build described by PlayStation, 2012 | Pass-and-play | 2–4 | 1–? | Разрешена | [Hands On at gamescom: pass-and-play mode](https://blog.playstation.com/?p=82575) Наличие/права на отдельные столы и совместимость конкретного релиза проверяются отдельно. |

## Wii U

| Игра / издание | Режим | Людей | Геймпадов / устройств | Передача | Подтверждение и ограничения |
| --- | --- | --- | --- | --- | --- |
| Game & Wario — European release announcement, 2013 | ARTWORK | 2–5 | 1 | Требуется | [Nintendo announcement: FRUIT / ARTWORK / DISCO](https://www.nintendo.com/en-gb/News/2013/May/The-wacky-world-of-Game-Wario-for-Wii-U-gets-unleashed-in-your-home-on-28th-June-752954.html) Нужны рисование на touch-экране и общение; простого перенаправления кнопок недостаточно. |
| Game & Wario — European release announcement, 2013 | FRUIT | 2–5 | 1 | Требуется | [Nintendo announcement: FRUIT / ARTWORK / DISCO](https://www.nintendo.com/en-gb/News/2013/May/The-wacky-world-of-Game-Wario-for-Wii-U-gets-unleashed-in-your-home-on-28th-June-752954.html) Вору нужен скрытый экран GamePad; агенты видят TV и получают GamePad для ответа. Нельзя транслировать всем скрытую картинку. |
| Game & Wario — European release announcement, 2013 | DISCO | 2 | 1 | Другая схема | [Nintendo announcement: FRUIT / ARTWORK / DISCO](https://www.nintendo.com/en-gb/News/2013/May/The-wacky-world-of-Game-Wario-for-Wii-U-gets-unleashed-in-your-home-on-28th-June-752954.html) Оба игрока держат разные концы GamePad и используют touch одновременно. |

## Охват исследования и оставшаяся работа

В JSON сохранён реестр использованных руководств: источник, издание, SHA-256,
наличие просмотренного текстового слоя и ссылки на разобранные режимы. Сами
руководства, сканы и полные чужие тексты в репозиторий не включены.
Автоматический поиск по тексту нужен для обнаружения кандидатов; отсутствие
совпадения **не** означает отсутствие Hotseat.

| Источник / платформа | Зафиксированный объём | Что проверено | Что остаётся |
| --- | --- | --- | --- |
| PlayStation | Индекс videogamemanual: 1426 PDF-записей | В реестре 101 источников; текстовый поиск выполнен для 83; вручную классифицированы режимы 14 игр | Остальные источники, режимы и издания не классифицированы |
| PlayStation 2 | Индекс videogamemanual: 1888 PDF-записей | В реестре 48 источников; текстовый поиск выполнен для 48; вручную классифицированы режимы 15 игр | Остальные источники, режимы и издания не классифицированы |
| GameCube | Архив NintendoGameCubeManuals: 263 текстовых записей | В реестре 262 источников; текстовый поиск выполнен для 262; вручную классифицированы режимы 10 игр | Остальные источники, режимы и издания не классифицированы |

Эти числа относятся к **руководствам и изданиям**, включая дубли, сборники и диски,
а не к числу уникальных игр платформы. Индексы не покрывают все регионы, цифровые
выпуски и новые/homebrew-релизы. Для полного каталога пока отсутствует полный
реестр релизов с учётом регионов; процент полноты по всей платформе неизвестен.
PSP/Vita/Wii U представлены отдельными подтверждёнными записями. Полная сверка
старших и младших семейств принятого плана, включая Wii, ещё открыта.

Исходные индексы: [PS1](https://www.videogamemanual.com/ps1/index.html),
[PS2](https://www.videogamemanual.com/PS2/index.html),
[GameCube](https://archive.org/details/NintendoGameCubeManuals),
[дополнительные руководства PS1](https://archive.org/details/SonyPlaystationManuals).
Снимок проверен 11 октября 2026. Результаты не ограничены библиотекой владельца.

### Кандидаты, которым ещё нельзя автоматически предлагать общий геймпад

В этот перечень входят источники для дальнейшей проверки, а не утверждения о
поддержке. У части руководств найден режим на несколько человек без явного числа
контроллеров; у части нужна другая версия руководства или проверка меню игры.
Все такие случаи остаются неизвестными. Строки выше с конкретными режимами
также не закрывают автоматически остальные режимы той же игры.

**PlayStation:** American Pool (USA); Austin Powers Pinball (USA); Big Strike Bowling (USA); Billiards (USA); Bowling (USA); Chess (USA); Chessmaster 3-D, The (USA); Extreme Pinball (USA); Flintstones, The - Bedrock Bowling (USA); Fox Sports Golf '99 (USA); Game of Life, The (USA); Hogs of War (USA); Hot Shots Golf 2 (USA); KISS Pinball (USA); Monopoly (USA); PGA Tour 97 (USA); PGA Tour 98 (USA); Patriotic Pinball (USA); Pool Hustler (USA); Pro 18 - World Tour Golf (USA); Pro Pinball - Big Race USA (USA); Pro Pinball - Fantastic Journey (USA); Pro Pinball - Timeshock! (USA); Pro-Pinball (USA); Putter Golf (USA); Risk - The Game of Global Domination (USA); Saban's Power Rangers Zeo - Full Tilt Battle Pinball (USA); Scrabble (USA); Secret of Googol 5, The - Googolfest - Party Isle & Toy Isle (USA); Secret of Googol 6, The - Googolfest - Arcade Isle & Moon Feast Isle (USA); Tecmo World Golf - Japan (USA); Tiger Woods 99 PGA Tour Golf (USA); Tiger Woods PGA Tour 2000 (USA); Tiger Woods PGA Tour Golf (USA); Tony Hawk's Pro Skater (USA); Tony Hawk's Pro Skater 2 (GH) (USA); Tony Hawk's Pro Skater 3 (USA); Tony Hawk's Pro Skater 4 (USA); True Pinball (USA); VR Golf '97 (USA); Virtual Pool (USA); Virtual Pool 3 (USA); Wheel of Fortune (USA); Wheel of Fortune - 2nd Edition (USA); World Cup Golf - Professional Edition (USA); Worms Armageddon (USA); Worms World Party (USA).

**PlayStation 2:** AMF Xtreme Bowling (USA); Brunswick Pro Bowling (USA); Disney Golf (USA); Hard Rock Casino (USA); High Rollers Casino (USA); Jeopardy! (USA); PDC World Championship Darts (USA); Pinball Hall of Fame- The Gottlieb Collection (USA); Pinball Hall of Fame- The Williams Collection (USA); Pool Paradise (Archer Maclean Presents) (USA); ProStroke Golf- World Tour 2007 (USA); Q-Ball Billiards Master (USA); Real Pool (USA); Strike Force Bowling (USA); Swing Away Golf (USA); Tiger Woods PGA Tour 06 (USA); Tiger Woods PGA Tour 07 (USA); Tiger Woods PGA Tour 08 (USA); Tiger Woods PGA Tour 09 (USA); Tiger Woods PGA Tour 10 (USA); Tiger Woods PGA Tour 2001 (USA); Tiger Woods PGA Tour 2002 (USA); Tiger Woods PGA Tour 2004 (USA); Tiger Woods PGA Tour 2005 (USA); Tony Hawk's American Wasteland (USA); Tony Hawk's Downhill Jam (USA); Tony Hawk's Pro Skater 3 (USA); Tony Hawk's Pro Skater 4 (USA); Tony Hawk's Project 8 (USA); Tony Hawk's Proving Ground (USA); Tony Hawk's Underground (USA); Tony Hawk's Underground 2 (USA); World Championship Pool 2004 (USA).

**GameCube:** дополнительно разобрать остальные режимы Mario Golf и Super Monkey Ball,
Tiger Woods PGA Tour 2003–2005, Sonic Mega Collection (отдельные встроенные игры),
WarioWare, Outlaw Golf, Pool Paradise, Worms 3D и отсутствующие в просмотренном архиве
региональные издания. Значок «alternating» сам по себе не доказывает общий геймпад.

PDF Hot Shots Golf 2 в просмотренном индексе содержит только четыре страницы
рекламных/служебных материалов. Это неполное руководство; отсутствие описания
мультиплеера в нём не является отрицательным результатом.

Для завершения нужны: полный реестр релизов; проверка оставшихся кандидатов;
добавление точных идентификаторов изданий; отдельно — реализация и проверка
общего управления в доступных эмуляторах. Консольные, физические и сетевые проверки
в этой работе не выполнялись. PS3/Switch не включены.

## Охват

Каталог общий для платформ принятого мультиплеерного плана: до Wii U включительно,
плюс Vita. Начать с PS1/PS2/GameCube. PS3/Switch пока исключены. Каталог не должен
ограничиваться играми, установленными у владельца, или несколькими примерами.

Единица учёта — игра на конкретной платформе и её конкретный локальный режим.
Одна игра может поддерживать разные схемы управления. Издания и регионы можно
объединять, если подтверждение действительно распространяется на них; известные
различия и непроверенные варианты нужно сохранять явно.

## Что устанавливать для каждой игры

| Поле | Содержание |
| --- | --- |
| Идентичность | Название, платформа, охват изданий; доступные устойчивые идентификаторы каталога/издания |
| Режим игры | Конкретный пункт локального мультиплеера и число участников |
| Контроллеры | Сколько логических геймпадов использует этот режим |
| Передача | Требуется; доступна как вариант; используются отдельные контроллеры; пока неизвестно |
| Подтверждение | Руководство/документация игры, явное описание режима или наблюдение в игре; ссылка, страница/раздел и дата проверки |
| Примечание | Ограничения, различия изданий, необходимость дополнительного оборудования |

Отдельно хранить статус поддержки маршрута в TrainerOS и проверки на устройстве.
Факт о режиме игры не доказывает работоспособность эмулятора, сетевого соединения
или передачи управления. Каталожная идентичность не разрешает чтение/изменение
сейвов и не заменяет проверку совместимости перед запуском.

«Пошаговая игра» и «2–4 игрока» не означают, что все пользуются одним геймпадом.
Если игра допускает и один общий, и отдельные геймпады, сохранить оба варианта.
Не превращать отсутствие сведений в отрицательный ответ. Не угадывать режим по
названию файла, жанру или общему числу игроков из ScreenScraper.

## Составление и полнота

1. Зафиксировать исходный каталог игр и его версию/дату для каждой платформы.
2. По имеющимся сведениям найти кандидатов, затем проверить именно схему
   контроллеров и режим игры. Сначала использовать руководства и документацию;
   требующие устройства проверки оставить до включения портативок.
3. Вести подтверждённый список, необязательные варианты и нерешённые случаи.
   Для проверки охвата также учитывать просмотренные игры без такого режима.
4. Для каждой платформы показывать общий объём исходного каталога, сколько игр
   проверено и сколько осталось. Наличие нескольких популярных примеров не
   позволяет назвать список полным. Пропуски источников и изданий остаются открытыми.
5. Полноту заявлять относительно зафиксированного охвата, с датой и оставшимися
   исключениями. Новые игры/издания должны дополнять каталог без переделки сети.

Каталог даёт данные для выбора/предложения режима. Передача управления остаётся
общим механизмом TrainerOS: один активный владелец геймпада и явная передача
следующему участнику. Не писать отдельный сетевой протокол или автоматическое
распознавание хода для каждой игры. Полный список и работающая передача управления
проверяются отдельно; одно не заменяет другое.
