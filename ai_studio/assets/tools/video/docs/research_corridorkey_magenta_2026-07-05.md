# CorridorKey на магенте — исследование + эксперименты (2026-07-05)

## RU сводка

| Вопрос | Ответ |
|---|---|
| Настройки/issues/форки под магенту в комьюнити? | **Нет.** `--screen-color` жёстко = {auto, green, blue}. Все форки (EZ-CorridorKey GUI, CorridorKey-Engine, ComfyUI-CorridorKey) — только green/blue. Ни issue, ни PR про магенту. |
| Цветовой prior — в обучении или runtime-хинт? | **Запечён в весах чекпоинта** (отдельные green/blue веса, одна архитектура). Сеть на вход берёт только `[sRGB + alpha-hint]` (4 канала), кода цвета нет. `--screen-color` делает лишь 2 вещи: выбирает чекпоинт + канал despill (1=G, 2=B). AlphaHint (GVM/BiRefNet) — сегментация субъекта, цвето-агностична. |
| Можно ли обмануть препроцессингом? | **Да, чисто.** Трюк, который переводит магенту→зелёный, обманывает и сеть, и despill. |
| Лучший трюк | **hue+180° (magenta 300°→green 120°), green-чекпоинт, despill G, hue-назад на FG.** Value-preserving → тёмные субъекты выживают. На жёстком f1: субъект dE76 **2.0** (лучше blue-бейзлайна 2.7), край contam **0%**. |
| Рекомендация | **(b)+(a):** «glow генерим на GREEN» остаётся правилом, магента по умолчанию = key_matte. НО если метод CorridorKey доступен на мадженте — заменить blue-on-magenta на hue180-шим (строгий апгрейд). CK не делать дефолтом для магенты. |

## Community (честно — пусто)

Форки — только обёртки: EZ-CorridorKey (десктоп-GUI, «Chroma» = маркетинг, green+blue), 99oblivius/CorridorKey-Engine (скорость/мульти-GPU), SeanBRVFX/ComfyUI-CorridorKey (нода). Официальные гайды (corridorkey.pages.dev), Hackaday, aescripts — все подтверждают только green/blue. Заметка из выдачи: «высокий despill выносит зелёный и перекидывает в R+B → уводит спилл в magenta/purple» — despill сам по себе green-центричный. Прямой WebFetch к github был заблокирован egress (Avast), данные через поиск.

## Цветовой prior — доказательства в коде

- `--screen-color` ограничен `{auto,green,blue}`: `corridorkey_cli.py:367-381`.
- Единственная таблица цвета = канал despill: `CorridorKeyModule/core/color_utils.py:311` → `{"green":1,"blue":2}`; despill одноканальный, channel-agnostic `color_utils.py:210-303`. Магента = R+B, один канал вычищает лишь половину → отсюда **розовый rim** у blue-on-magenta.
- Вход сети = 4 канала `[RGB + hint]`, никакого цвета: `inference_engine.py:216-243`, forward `model_transformer.py:242-297`.
- Два чекпоинта, одна архитектура, разные веса: `backend.py:34-42`. AlphaHint = цвето-агностичная сегментация: `README:99-109`.

## Числа трюков

f1 = тёмный lineart на мадженте (тяжёлый кейс); f4 = реальный меч. Метрики против МАГЕНТА-ключа, over white.

| fixture | условие | субъект dE76 ↓ | rim meanDom | rim maxDom | contam% ↓ |
|---|---|---|---|---|---|
| f1 | key_matte | 0 (точно) | −8.0 | −8.0 | 0 |
| f1 | blue baseline (despill=max) | 2.7 | +4.86 | 11.8 | 0 |
| f1 | **hue180_green** | **2.0** | +2.69 | 8.2 | **0** |
| f1 | invert_green | 9.22 | −0.14 | 3.7 | 0 |
| f1 | swap_rg_green | 11.43 | +4.13 | 7.8 | 0 |
| f4 | key_matte | 0 | −23.8 | 4.0 | 0 |
| f4 | blue baseline | 4.49 | −18.5 | 23.4 | 0.14 |
| f4 | hue180_green | 6.26 | −18.8 | 11.4 | **0** |
| f4 | **invert_green** | **4.23** | −18.9 | 7.5 | **0** |
| f4 | swap_rg_green | 5.85 | −6.9 | 38.2 | 2.09 |

(Опубликованный в оригинальном эвале blue-on-magenta — contam 8.2%, meanDom +8.2 — там despill был не на максимуме. На full despill розовый rim слабее, но остаётся: meanDom +4.86 «в магенту».)

Вывод по трюкам: **hue180** — единственный, кто улучшает И субъект, И край относительно blue-бейзлайна на тяжёлом f1 (сохраняет value → тёмный navy остаётся navy). **invert** чуть чище край на ярком субъекте (f4), но убивает тёмные цвета (f1 dE 9.2). **swap→cyan** (не green!) — худший; отклонить.

## Рекомендация

**Первично (b):** на плоской мадженте key_matte уже даёт rim contam 0 И точный цвет субъекта (dE 0) за 20-240 мс — ни один CK-трюк это не бьёт. «Генерировать glow на GREEN» остаётся правилом, магента = key_matte по умолчанию. На green CorridorKey работает нативно.

**Точечно (a):** если метод CorridorKey досягаем на мадженте — заменить blue-on-magenta путь на **hue180-green шим** (rotate +180° → green ckpt + despill G → rotate FG −180°; alpha без изменений; авто-триггер при не-green/blue ключе). Строгий апгрейд над CK-на-мадженте (rim 8.2%→0, субъект 2.7→2.0 dE), но НЕ дефолт для магенты.

**Апстрим:** шим обобщается на любой ключ (повернуть hue ключа к 120°/240°) — фича произвольной хромы, которой у CorridorKey нет. Кандидат в feature-request.

## Отклонённые альтернативы

- **RGB-инверсия** — магента→green точно, чистейший rim, но инвертирует value → тёмные субъекты уходят в пастель (f1 dE 9.2). Конкурентна только на ярких субъектах.
- **Swap R↔G** — магента→**cyan**, не green; green-despill по cyan → худший rim (f4 2.09%). Отклонить.
- **Ретрейн magenta-чекпоинта** — архитектура позволяет (`backend.py`), но нужен magenta-screen датасет, которого нет; не стоит того против бесплатного hue-шима.

## Открытые риски

- Только 2 статичных фикстуры, обе жёсткокрайние. Реальный выигрыш CK (мягкие/полупрозрачные края) на мадженте НЕ протестирован — единственный soft-фикстур (glow) на green. Появится мягкая магента — перепроверить hue180 на ней.
- hue180 = HSV-поворот (cv2); серые пиксели hue-стабильны, но FG — нейро-реконструкция, поэтому dE субъекта 2-6, не 0.

Доказательства: `C:\projects\video_gen_experiment\static_eval\magenta_tricks\` — `f1_magenta_linework_board.png`, `f4_real_magenta_board.png`, `trick_metrics.json`, `drift.json`; исходный эвал — `static_eval/metrics.json`. Раннеры: `static_eval/trick_run.py` (CK venv), `static_eval/trick_metrics.py` (ComfyUI python).
