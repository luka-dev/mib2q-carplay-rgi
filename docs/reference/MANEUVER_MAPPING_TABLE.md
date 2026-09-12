# Полная таблица iAP2 → BAP → maneuver_render

> Исторический снимок до согласования renderer. Текущие исправления, проверки и новый лист 54 типов: [MANEUVER_RENDERER_PARITY_FIXES.md](MANEUVER_RENDERER_PARITY_FIXES.md).

> Эта таблица сохраняет исходный аудит до пересмотра иконок. **Актуальный выбор всех 54 типов и внесённые изменения: [CARPLAY_HUD_ICON_SELECTION.md](CARPLAY_HUD_ICON_SELECTION.md).**

Сгенерировано `tools/build_maneuver_comparison.py` из исполнения реального Java-кода. Все 54 типа; RHT и LHT.

**BAP → HUD selector и дороги получены исполнением оригинального V850-кода G1952/M2325.** Исполнением ARM проверена связь внутреннего индекса с поверхностью. Принудительно выбран активный 3D-режим 3; это не эмуляция всех режимов панели и не запись CAN.

Углы ниже — явно заданные примеры. Для EXIT_1…EXIT_19 намеренно использован один угол +90°: номер съезда сам по себе не определяет его направление. Входные junctionAngles=[−90,0,+90], z=0. Для типов 6/7/19/28…46 junctionType=1, для остальных 0. Полная сетка с другими входами лежит в CSV.

BAP: `main / direction HEX`; renderer: `ICON / signed direction / angle degrees`. `sN` — **внутренний selector HUD packet[22]**, не BAP-код.

| MT | iAP2 | Угол | BAP RHT → LHT | Sidestreets RHT → LHT (hex) | Renderer RHT → LHT | HUD 3D RHT → LHT |
|---:|---|---:|---|---|---|---|
| 0 | NO_TURN | 0 | 11/00 | ∅ | APPROACH/0/0 | s0 ∅ |
| 1 | LEFT_TURN | -90 | 13/40 | 00 c0 | TURN/0/-90 | s28 r0298 |
| 2 | RIGHT_TURN | 90 | 13/C0 | 40 00 | TURN/0/90 | s31 r0301 |
| 3 | STRAIGHT_AHEAD | 0 | 13/00 | c0 40 | APPROACH/0/0 | s26 r0296 |
| 4 | U_TURN | -180 | 25/40 | ∅ | UTURN/-1/-180 | s110 r0380 |
| 5 | FOLLOW_ROAD | 0 | 11/00 | ∅ | APPROACH/0/0 | s0 ∅ |
| 6 | ENTER_ROUNDABOUT | 0 | 13/C0 → 13/40 | 40 → c0 | ROUNDABOUT/0/0 | s31 r0301 → s28 r0298 |
| 7 | EXIT_ROUNDABOUT | 0 | 26/C0 → 27/40 | ∅ | ROUNDABOUT/0/0 | s112 r0382 → s113 r0383 |
| 8 | OFF_RAMP | 30 | 15/E0 | 00 | EXIT/1/30 | s35 r0305 |
| 9 | ON_RAMP | 30 | 13/E0 | ∅ | MERGE/1/30 | s32 r0302 |
| 10 | ARRIVE_END_OF_NAVIGATION | 0 | 3/00 | ∅ | ARRIVED/0/0 | s120 r0271+r0272 |
| 11 | START_ROUTE | 0 | 13/00 | c0 40 | APPROACH/0/0 | s26 r0296 |
| 12 | ARRIVE_AT_DESTINATION | 0 | 3/00 | ∅ | ARRIVED/0/0 | s120 r0271+r0272 |
| 13 | KEEP_LEFT | -45 | 13/20 | 00 c0 40 | TURN/0/-45 | s27 r0297 |
| 14 | KEEP_RIGHT | 45 | 13/E0 | c0 40 00 | TURN/0/45 | s32 r0302 |
| 15 | ENTER_FERRY | 0 | 11/00 | ∅ | APPROACH/0/0 | s0 ∅ |
| 16 | EXIT_FERRY | 0 | 13/00 | c0 40 | APPROACH/0/0 | s26 r0296 |
| 17 | CHANGE_FERRY | 0 | 11/00 | ∅ | APPROACH/0/0 | s0 ∅ |
| 18 | START_ROUTE_WITH_U_TURN | -180 | 25/40 | ∅ | UTURN/-1/-180 | s110 r0380 |
| 19 | U_TURN_AT_ROUNDABOUT | 180 | 21/80 → 22/80 | ∅ → 40 00 c0 | ROUNDABOUT/0/180 | s54 r0324 → s70 r0340 |
| 20 | LEFT_TURN_AT_END | -90 | 13/40 | 00 c0 | TURN/0/-90 | s28 r0298 |
| 21 | RIGHT_TURN_AT_END | 90 | 13/C0 | 40 00 | TURN/0/90 | s31 r0301 |
| 22 | HIGHWAY_OFF_RAMP_LEFT | -30 | 16/20 | 00 | EXIT/-1/-30 | s37 r0307 |
| 23 | HIGHWAY_OFF_RAMP_RIGHT | 30 | 15/E0 | 00 | EXIT/1/30 | s35 r0305 |
| 24 | ARRIVE_DESTINATION_LEFT | -90 | 3/40 | ∅ | ARRIVED/-1/-90 | s2 r0273+r0271 |
| 25 | ARRIVE_DESTINATION_RIGHT | 90 | 3/C0 | ∅ | ARRIVED/1/90 | s3 r0274+r0271 |
| 26 | U_TURN_WHEN_POSSIBLE | -180 | 25/40 | ∅ | UTURN/-1/-180 | s110 r0380 |
| 27 | ARRIVE_END_OF_DIRECTIONS | 0 | 3/00 | ∅ | ARRIVED/0/0 | s120 r0271+r0272 |
| 28 | ROUNDABOUT_EXIT_1 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 29 | ROUNDABOUT_EXIT_2 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 30 | ROUNDABOUT_EXIT_3 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 31 | ROUNDABOUT_EXIT_4 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 32 | ROUNDABOUT_EXIT_5 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 33 | ROUNDABOUT_EXIT_6 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 34 | ROUNDABOUT_EXIT_7 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 35 | ROUNDABOUT_EXIT_8 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 36 | ROUNDABOUT_EXIT_9 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 37 | ROUNDABOUT_EXIT_10 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 38 | ROUNDABOUT_EXIT_11 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 39 | ROUNDABOUT_EXIT_12 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 40 | ROUNDABOUT_EXIT_13 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 41 | ROUNDABOUT_EXIT_14 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 42 | ROUNDABOUT_EXIT_15 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 43 | ROUNDABOUT_EXIT_16 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 44 | ROUNDABOUT_EXIT_17 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 45 | ROUNDABOUT_EXIT_18 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 46 | ROUNDABOUT_EXIT_19 | 90 | 21/C0 → 22/C0 | 40 00 | ROUNDABOUT/0/90 | s58 r0328 → s74 r0344 |
| 47 | SHARP_LEFT_TURN | -135 | 13/60 | 40 00 c0 | TURN/0/-135 | s29 r0299 |
| 48 | SHARP_RIGHT_TURN | 135 | 13/A0 | 40 00 c0 | TURN/0/135 | s30 r0300 |
| 49 | SLIGHT_LEFT_TURN | -45 | 13/20 | 00 c0 40 | TURN/0/-45 | s27 r0297 |
| 50 | SLIGHT_RIGHT_TURN | 45 | 13/E0 | c0 40 00 | TURN/0/45 | s32 r0302 |
| 51 | CHANGE_HIGHWAY | 0 | 13/00 | c0 40 | APPROACH/0/0 | s26 r0296 |
| 52 | CHANGE_HIGHWAY_LEFT | -30 | 12/40 | ∅ | EXIT/-1/-30 | s24 r0294 |
| 53 | CHANGE_HIGHWAY_RIGHT | 30 | 12/C0 | ∅ | EXIT/1/30 | s25 r0295 |

## Изображения

Оригинальные 3D-слои HUD показаны без реконструкции серых дорог. Это сравнение формы стрелки с полным кадром renderer; не попиксельная проверка всего экрана.

- [RHT: типы 0–9](../../output/maneuver-audit/comparison_rht_01.png)
- [RHT: типы 10–19](../../output/maneuver-audit/comparison_rht_02.png)
- [RHT: типы 20–29](../../output/maneuver-audit/comparison_rht_03.png)
- [RHT: типы 30–39](../../output/maneuver-audit/comparison_rht_04.png)
- [RHT: типы 40–49](../../output/maneuver-audit/comparison_rht_05.png)
- [RHT: типы 50–53](../../output/maneuver-audit/comparison_rht_06.png)
- [LHT: круги, разворот и рампы](../../output/maneuver-audit/comparison_lht_01.png)

## Машинные данные

- [Все 7 890 прогонов Java](../../output/maneuver-audit/java_mapping.csv)
- [108 пар RHT/LHT с примерами контракта](../../output/maneuver-audit/canonical_comparison.csv)
- [Все входы Java + измеренные HUD selector/marker/roads](../../output/maneuver-audit/java_mapping-hud.csv)
- [Проверка V850 G1952/M2325](../../output/maneuver-audit/mcu-chain-checks.json)
- [122 захвата renderer / 48 команд](../../output/maneuver-audit/renderer-captures.json)

**`stock_fct23_hex` содержит артефакт реконструированного JXE:** вместо пустых строк там встречается `isNativeLittleEndian`. Это не запись с CAN. Для исходных sidestreets использовать `bap_sidestreets`; `descriptor_contract_example` — расчётный пакет из реальных полей bridge с пустыми отсутствующими слотами. Подробности и выводы: [MANEUVER_HUD_RENDERER_AUDIT.md](MANEUVER_HUD_RENDERER_AUDIT.md).
