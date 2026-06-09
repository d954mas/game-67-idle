# Parent Playtest Note

Статус: v0.1.
Дата: 2026-06-09.

Этот текст нужен для страницы или письма родителю/опекуну перед external
playtest. Это не финальная legal policy, но P0-прототип должен соответствовать
этим ограничениям.

## Что Это За Игра

`67` - это смешной ранг силы героя. Цель игрока: подняться с `1/67` до `15/67`
в прототипе и увидеть мечту `67/67`.

Игра про мемное становление: герой делает `67-жест`, выполняет простые дела,
открывает районы, улучшает базу, получает команду и возвращает крутость.

## Что Есть В P0

- нет рекламы;
- нет покупок;
- нет аккаунта;
- нет чата;
- нет свободного текстового ввода от ребенка;
- нет сбора имени, возраста, email, телефона, геолокации или advertising ID;
- нет behavioral ads;
- прогресс хранится локально в браузере/прототипе;
- можно сбросить save для повторного теста.

## Analytics For Playtest

Default for external child tests: analytics off until guardian notice/consent is
handled.

If analytics are enabled for supervised internal testing, collect only gameplay
events:

- anonymous session id;
- platform/build;
- time in session;
- clicked gameplay actions;
- bought upgrades;
- reached milestones;
- event choices.

Do not collect:

- name;
- exact age;
- free text;
- photos/audio/video;
- geolocation;
- contact info;
- ad identifiers.

Retention:

- keep playtest gameplay logs only as long as needed for analysis;
- delete on parent/guardian request;
- do not share with ad networks.

## Parent Controls

Prototype must provide:

- `Сбросить прогресс`;
- `Отключить аналитику`;
- short explanation of what `67` means;
- contact path for deleting playtest data if any server-side data exists.

## Tone Safety

Player-facing P0 text must avoid adult drama and heavy hardship. Use:

- `подстава`;
- `обнулили мем`;
- `Банан мутит`;
- `Клубника знает секрет`;
- `сила растет`;
- `до 67 ближе`.

Do not use:

- romantic betrayal;
- homelessness framing;
- debt/fines/crisis framing;
- revenge/dominance framing;
- child labor framing.

## Release Note

Before mobile store or public web release, replace this note with a reviewed
privacy policy and platform-specific child-safety compliance checklist.
