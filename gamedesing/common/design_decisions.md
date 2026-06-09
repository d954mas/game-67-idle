# Design Decisions

Зафиксированные решения проекта.

## Target

Product target: mobile portrait + web.  
PC/native = dev harness.

Связи: [[playtest_gates]]

## Format

P0 = life-sim-lite incremental, не full open world.

Связи: [[what_worked#Life-Sim Lite]], [[core_loop]]

## Tabs

P0 tabs: `Город`, `Дела`, `Улучшения`, `Дом`.

Почему: меньше навигации, яснее путь ребенка.

Связи: [[core_loop]]

## Jobs Naming

Internal: `jobs`.  
Player-facing: `Дела`.

Почему: нет ощущения детского труда.

Связи: [[child_safe_copy]], [[what_worked#Дела Вместо Работы]]

## Final Path

Любой финальный выбор должен вести к `15/67`.

Почему: ребенок не должен проиграть из-за “хорошего” выбора.

Связи: [[playtest_gates]]

## Privacy

External child playtest: analytics off by default until guardian notice/consent.

Связи: [[parent_playtest_note]]
