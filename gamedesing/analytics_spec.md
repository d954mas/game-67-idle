# Analytics And Playtest Spec

Статус: v0.1.

## Principle

ЦА 5-10 лет. Аналитика должна быть privacy-safe:

- не собирать имя, возраст, email, телефон, геолокацию, advertising ID;
- не использовать behavioral ads;
- не собирать свободный текст от детей;
- использовать анонимный session id;
- хранить только gameplay events.

For external child tests, analytics are off by default until guardian notice and
consent are handled. Persistent identifiers must be treated as privacy-sensitive
unless they are strictly limited to permitted internal operations.

Parent-facing requirements are defined in `parent_playtest_note.md`.

Machine-readable implementation contract: `data/analytics_events.json`.

## Playtest Hypotheses

1. Дети понимают `X/67` над героем как главный прогресс без объяснений.
2. `67-жест` считывается как главное действие.
3. Первая покупка до 30-60 секунд удерживает игрока.
4. Первый idle-доход до 5 минут снижает усталость от кликов.
5. Банан/Клубника/06:00 запоминаются как безопасные мемные символы.
6. 30-минутная сессия до `15/67` вызывает желание продолжить до `67/67`.

## Event Taxonomy

| Event | When | Parameters |
| --- | --- | --- |
| `session_start` | start | platform, build, layout |
| `intro_seen` | intro shown | skipped |
| `first_click` | first action click | seconds_from_start |
| `activity_used` | any activity | activity_id, reward |
| `upgrade_seen` | upgrade visible | upgrade_id, state |
| `upgrade_bought` | purchase | upgrade_id, cost, status_before, status_after |
| `status_changed` | X/67 changes | from, to, source |
| `job_started` | job timer starts | job_id, district_id |
| `job_completed` | job reward | job_id, duration, reward |
| `district_unlocked` | new district | district_id, status |
| `event_seen` | event modal | event_id |
| `event_choice` | choice clicked | event_id, choice_id, reward |
| `tab_opened` | bottom tab | tab_id |
| `final_15_reached` | mini-final | session_seconds |
| `session_end` | quit/close | session_seconds, status, coins |

Every event includes:

- `session_id`;
- `build`;
- `platform`;
- `session_seconds`;
- `analytics_enabled`.

Do not include:

- free text;
- exact age;
- name/contact data;
- IP/geolocation/ad id in gameplay payloads.

## Funnel

| Step | Target |
| --- | --- |
| First click | 80% within 5 seconds |
| First upgrade | 70% within 60 seconds |
| First status-up | 60% within 2 minutes |
| First idle income | 60% within 5 minutes |
| First event | 50% within 8 minutes |
| `10/67` | 35% within first session |
| `15/67` | 25-35% within first session |

## Failure Signals

- No click for 10 seconds.
- More than 5 clicks on locked upgrade.
- No purchase after 90 seconds.
- Stuck more than 3 minutes with no affordable upgrade and no active job.
- Leaves before first status-up.
- Opens tabs repeatedly without buying/starting anything.

## Playtest Script

### Participants

- 5-8 children in target age range for first qualitative pass.
- Parent/guardian present.
- No coaching unless child is fully blocked for 60 seconds.

### Observation Questions

- What did the child click first?
- Did they understand `X/67`?
- Did they repeat any meme words?
- Did they notice the 67 gesture?
- Which screen confused them?
- Did parent flag tone/content concerns?

### Post-Session Questions

For child:

- What number were you trying to reach?
- Who was funny?
- What did you want to buy next?
- Do you want to reach `67/67`?

For parent:

- Did the tone feel safe?
- Did any word/theme feel too adult?
- Was the screen readable?
