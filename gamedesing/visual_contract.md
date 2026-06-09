# Visual Contract

Статус: v0.1.  
Назначение: связать `data/balance.json` с тем, что разработчик реально рисует
в прототипе. Если ассета нет, использовать fallback из этого документа.

## Rule

Любой `visibleResult`, `visualStage`, `screenCompanion`, `microReaction` должен
дать игроку физически заметное изменение: предмет, фон, жест, вспышку, реакцию
или новую карточку.

## VisualStage Map

| id | Meaning | Required asset/fallback |
| --- | --- | --- |
| `start_yard` | стартовая дворовая база | simple yard background, hero center |
| `cap` | первая внешняя перемена героя | cap layer on hero |
| `safe_sleep` | уютный угол 67 | warm mat/corner + night light |
| `poster_room` | комната/плакат 67 | poster on wall |
| `mini_hq` | мини-штаб | brighter base, table/sign |
| `scooter` | самокат | scooter prop near hero |
| `old_bike` | старый велик | bike prop, unlock delivery feel |
| `mini_tycoon` | post-MVP dream | 67-car silhouette, do not count as P0 progress |

## Companion Map

| id | When | Visual |
| --- | --- | --- |
| `mini_helper` | first idle helper | small helper near hero |
| `team_67` | late P0 | 2-3 teammates behind hero |

## Animation Map

| id | Trigger | Fallback |
| --- | --- | --- |
| `hands_seesaw_67` | main button | two-frame hand seesaw |
| `speech_burst_67` | shout activity | text bubble `67` pop |
| `badge_flash_67` | badge action/final | yellow badge flash |
| `banana_twitch` | micro reaction | Банан sprite shakes 2 frames |
| `strawberry_wink` | micro reaction | Клубника wink frame |
| `goal_spark` | next goal progress | spark on goal card |
| `map_ping` | district unlock | map card pulse |
| `deal_stamp` | deal complete | stamp `Дело сделано` |

## Fake Shot Board

Required before external playtest:

1. First screen `1/67`.
2. First click `Сделать 67`.
3. First purchase.
4. Status-up `2/67`.
5. City map.
6. Deals timer.
7. Event modal.
8. Home growth.
9. Mini-final `15/67`.

Связи: `mobile_web_ux_spec.md`, `asset_manifest.md`, `data/balance.json`.
