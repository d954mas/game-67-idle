# Child Safety And Store Compliance Checklist

Статус: v0.2.  
Дата: 2026-06-09.  
Это design checklist, не юридическое заключение.

## Scope

ЦА P0: дети 5-10 лет, supervised playtest.  
Таргеты: mobile web, desktop web harness, будущий mobile store build.

P0 считается child-directed. Поэтому любые данные, SDK, аналитика, реклама и
тексты проверяются как детский продукт.

## Official References

- FTC COPPA Rule: https://www.ftc.gov/legal-library/browse/rules/childrens-online-privacy-protection-rule-coppa
- FTC COPPA FAQ: https://www.ftc.gov/business-guidance/resources/complying-coppa-frequently-asked-questions
- Google Play Families Policy: https://support.google.com/googleplay/android-developer/answer/9893335
- Apple App Review Guidelines: https://developer.apple.com/app-store/review/guidelines/

## P0 Go Conditions

Machine-readable release checklist: `data/release_readiness.json`.

Перед внешним тестом на реальных детях:

- есть parent-facing note: `gamedesing/parent_playtest_note.md`;
- analytics off by default;
- no ads, no IAP, no rewarded video;
- no account, chat, free text, photo, audio, video, precise location;
- no name, exact age, email, phone, ad id;
- save is local-only unless отдельное server-data review сделано;
- есть `Reset progress`;
- есть понятное объяснение, что `67` - мемный ранг силы;
- player-facing текст проходит forbidden-word scan;
- test host/logging не сохраняет лишние child data beyond standard ops.

## Store Build Go Conditions

До public store/mobile release нужно закрыть:

- reviewed privacy policy;
- age rating questionnaire;
- child-directed declaration where required;
- SDK inventory: analytics, crash, hosting, CDN, fonts, payment, ads;
- data map: what is collected, purpose, retention, deletion path;
- parental notice/consent flow if any personal data or persistent identifiers are used beyond permitted internal operations;
- platform-specific Kids/Families review for Apple/Google;
- no behavioral advertising;
- no external links without required safeguards;
- no monetization until separate child-safety review.

## Content Safety

P0 допускает мемное становление, но не тяжелую драму.

Use:

- `1/67 -> 15/67 -> 67/67`;
- `Дела`;
- `команда 67`;
- `ночник 06:00`;
- `подстава`, `обнулили мем`, `Банан мутит`.

Do not use player-facing:

- realistic homelessness or severe poverty;
- romantic/sexual betrayal;
- debt, fines, stock market, gambling;
- violence, weapons, gore;
- humiliation as punishment;
- revenge/dominance framing;
- child labor framing.

## Data Rules

Allowed for internal supervised P0 only if documented:

- anonymous session id;
- build/platform;
- gameplay event names;
- clicked actions;
- upgrade ids;
- status milestones;
- session duration.

Not allowed in P0 payloads:

- name/contact data;
- exact age;
- free text;
- photos/audio/video;
- precise location;
- advertising id;
- cross-app tracking ids.

Persistent identifiers are privacy-sensitive. Treat them as off unless needed for
permitted internal operations and explicitly reviewed.

## Analytics Rules

Default external child test:

- analytics disabled;
- local save only;
- observations collected by adult tester outside the game.

If analytics are enabled in a supervised/internal test:

- guardian notice/consent is handled first;
- event taxonomy matches `gamedesing/analytics_spec.md`;
- no third-party ad network receives events;
- retention period is defined;
- deletion path exists.

## Web Hosting Rules

Before public web link:

- confirm what server logs store: IP, user agent, timestamp, route;
- avoid query params with child/player info;
- avoid third-party embeds, trackers, remote fonts unless reviewed;
- add visible parent note or link before test starts;
- document reset/delete behavior.

## Release-Mode Flow

External P0 web/mobile test starts in `child_test_safe` mode:

1. Open parent notice before gameplay or next to the test invite.
2. Explain `67`: fictional power rank and gesture animation, no chat or external
   links.
3. Keep analytics disabled by default.
4. Adult tester may enable analytics only after guardian notice/consent.
5. If analytics stay disabled, collect observations outside the game.
6. Local save stores progress only; reset clears it.
7. Server-side logs, if any, are reviewed before the public link is shared.
8. Retention for any collected playtest data is set before the test invite.
9. Deletion/contact path is included in the parent note or invite.

Required implementation flags:

- `analyticsEnabled=false` by default;
- `externalLinksEnabled=false` in P0;
- `adsEnabled=false`;
- `iapEnabled=false`;
- `freeTextEnabled=false`;
- `resetProgress` is visible to adult tester.

## Red Flags

No-Go if any is true:

- analytics sends data before parent/guardian notice;
- build includes ads or monetization;
- child can type free text;
- content reads as real homelessness, adult betrayal, debt, fear, or punishment;
- external SDK list is unknown;
- parent cannot understand what `67` means;
- test cannot be reset for repeated playtests.

Связи: `gamedesing/playtest_acceptance_gates.md`,
`gamedesing/parent_playtest_note.md`, `gamedesing/analytics_spec.md`.
