# BAP mainElement → 3D HUD selectors

Измерено исполнением V850 G1952/M2325. Условия: z=0, sideStreets=[], активный 3D-режим 3; один первый descriptor, остальные слоты пусты. Все числа direction — HEX, sN — внутренний индекс GDC.

Проверены direction 00,10,…,F0 и FF. В таблице перечислены непустые состояния; остальные проверенные направления дают s0. Стрелка и дороги зависят от разных таблиц; полные road selectors находятся в CSV. Выбор режима панели здесь не проверяется.

| Main | BAP | Direction HEX → selector |
|---:|---|---|
| 0 | NO_SYMBOL | ∅ |
| 1 | NO_INFO | ∅ |
| 2 | DIRECTION_TO_DESTINATION | 00,10,20,30,40,50,60,70,80,90,A0,B0,C0,D0,E0,F0,FF → s1 |
| 3 | ARRIVED | 00 → s120; 10,20,30,50,60,70,80,90,A0,B0,D0,E0,F0,FF → s4; 40 → s2; C0 → s3 |
| 4 | NEAR_DESTINATION | 00,10,20,30,40,50,60,70,80,90,A0,B0,C0,D0,E0,F0,FF → s1 |
| 5 | ARRIVED_DESTINATION_OFFMAP | 00 → s120; 10,20,30,50,60,70,80,90,A0,B0,D0,E0,F0,FF → s4; 40 → s2; C0 → s3 |
| 6 | OFF_ROAD | 00 → s5; 10 → s6; 20 → s7; 30 → s8; 40 → s9; 50 → s10; 60 → s11; 70 → s12; 80 → s13; 90 → s14; A0 → s15; B0 → s16; C0 → s17; D0 → s18; E0 → s19; F0 → s20 |
| 7 | OFF_MAP | 00 → s5; 10 → s6; 20 → s7; 30 → s8; 40 → s9; 50 → s10; 60 → s11; 70 → s12; 80 → s13; 90 → s14; A0 → s15; B0 → s16; C0 → s17; D0 → s18; E0 → s19; F0 → s20 |
| 8 | NO_ROUTE | 00 → s5; 10 → s6; 20 → s7; 30 → s8; 40 → s9; 50 → s10; 60 → s11; 70 → s12; 80 → s13; 90 → s14; A0 → s15; B0 → s16; C0 → s17; D0 → s18; E0 → s19; F0 → s20 |
| 9 | CALC_ROUTE | 00 → s5; 10 → s6; 20 → s7; 30 → s8; 40 → s9; 50 → s10; 60 → s11; 70 → s12; 80 → s13; 90 → s14; A0 → s15; B0 → s16; C0 → s17; D0 → s18; E0 → s19; F0 → s20 |
| 10 | RECALC_ROUTE | 00 → s5; 10 → s6; 20 → s7; 30 → s8; 40 → s9; 50 → s10; 60 → s11; 70 → s12; 80 → s13; 90 → s14; A0 → s15; B0 → s16; C0 → s17; D0 → s18; E0 → s19; F0 → s20 |
| 11 | FOLLOW_STREET | 40 → s21; C0 → s22 |
| 12 | CHANGE_LANE | 40 → s24; C0 → s25 |
| 13 | TURN | 00 → s26; 20 → s27; 40 → s28; 60 → s29; A0 → s30; C0 → s31; E0 → s32 |
| 14 | TURN_ON_MAINROAD | 00 → s26; 20 → s27; 40 → s28; 60 → s29; A0 → s30; C0 → s31; E0 → s32 |
| 15 | EXIT_RIGHT | 00 → s33; C0 → s34; E0 → s35 |
| 16 | EXIT_LEFT | 00 → s36; 20 → s37; 40 → s38 |
| 17 | SERVICE_ROAD_RIGHT | 00,10,20,30,40,50,60,70,80,90,A0,B0,C0,D0,E0,F0,FF → s39 |
| 18 | SERVICE_ROAD_LEFT | 00,10,20,30,40,50,60,70,80,90,A0,B0,C0,D0,E0,F0,FF → s40 |
| 19 | FORK_2 | 40 → s41; C0 → s42 |
| 20 | FORK_3 | 00 → s43; 40 → s44; C0 → s45 |
| 21 | ROUNDABOUT_TRS_RIGHT | 00 → s46; 10 → s47; 20 → s48; 30 → s49; 40 → s50; 50 → s51; 60 → s52; 70 → s53; 80 → s54; 90 → s55; A0 → s56; B0 → s57; C0 → s58; D0 → s59; E0 → s60; F0 → s61 |
| 22 | ROUNDABOUT_TRS_LEFT | 00 → s62; 10 → s63; 20 → s64; 30 → s65; 40 → s66; 50 → s67; 60 → s68; 70 → s69; 80 → s70; 90 → s71; A0 → s72; B0 → s73; C0 → s74; D0 → s75; E0 → s76; F0 → s77 |
| 23 | SQUARE_TRS_RIGHT | 00 → s78; 10 → s79; 20 → s80; 30 → s81; 40 → s82; 50 → s83; 60 → s84; 70 → s85; 80 → s86; 90 → s87; A0 → s88; B0 → s89; C0 → s90; D0 → s91; E0 → s92; F0 → s93 |
| 24 | SQUARE_TRS_LEFT | 00 → s94; 10 → s95; 20 → s96; 30 → s97; 40 → s98; 50 → s99; 60 → s100; 70 → s101; 80 → s102; 90 → s103; A0 → s104; B0 → s105; C0 → s106; D0 → s107; E0 → s108; F0 → s109 |
| 25 | UTURN | 40 → s110; C0 → s111 |
| 26 | EXIT_ROUNDABOUT_TRS_RIGHT | 00,10,20,30,40,50,60,70,80,90,A0,B0,C0,D0,E0,F0,FF → s112 |
| 27 | EXIT_ROUNDABOUT_TRS_LEFT | 00,10,20,30,40,50,60,70,80,90,A0,B0,C0,D0,E0,F0,FF → s113 |
| 28 | PREPARE_TURN | 00 → s23; 40 → s21; C0 → s22 |
| 29 | PREPARE_ROUNDABOUT | 00 → s23; 40 → s21; C0 → s22 |
| 30 | PREPARE_SQUARE | 00 → s23; 40 → s21; C0 → s22 |
| 31 | PREPARE_U_TURN | 40 → s114; C0 → s115 |
| 34 | MICHIGAN_TURN | 40 → s116; C0 → s117 |
| 35 | DOUBLE_TURN | 40 → s118; C0 → s119 |
| 38 | DIRECTION_TO_WAYPOINT | 00 → s5; 10 → s6; 20 → s7; 30 → s8; 40 → s9; 50 → s10; 60 → s11; 70 → s12; 80 → s13; 90 → s14; A0 → s15; B0 → s16; C0 → s17; D0 → s18; E0 → s19; F0 → s20 |

**FOLLOW_STREET 11/00 с z=0 и пустыми sideStreets имеет отдельное правило импортера:** первый descriptor очищается и используются следующие байты. s0 в этой таблице не означает, что весь HUD будет пустым в обычном режиме движения.

ARRIVED 3/00 выбирает s120, а не прежний предполагаемый s1. EXIT_RIGHT использует s33–35; s39 принадлежит SERVICE_ROAD_RIGHT.

- [Полная цепочка и рампы](HUD_RAMP_3D_CHAIN.md).
- [Все 54 iAP2 типа, RHT/LHT](MANEUVER_MAPPING_TABLE.md).
- [Измеренные селекторы и дороги](../../output/maneuver-audit/mcu-chain-output.csv).
- [Воспроизводимый декодер по таблицам прошивки](../../tools/hud_mcu_decoder.py).
