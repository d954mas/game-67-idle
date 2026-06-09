# Store Readiness

Тезис: внешний тест с детьми нельзя считать просто “залил билд и дал ссылку”.

## Что Нужно Для P0

- parent note;
- analytics off by default;
- no ads/IAP/account/chat/free text;
- local save or reviewed server-data flow;
- reset progress;
- понятное объяснение `67`.

## Что Блокирует

- неизвестные SDK;
- analytics before guardian notice;
- player-facing тяжелая драма;
- child data in URL/logs/payloads;
- нет reset/delete path.

Связи: [[playtest_gates]], [[child_safe_copy]], [[design_decisions#Privacy]]
