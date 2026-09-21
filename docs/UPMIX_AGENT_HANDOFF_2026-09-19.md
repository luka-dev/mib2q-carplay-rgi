# Audi Virtual Cabin — amp DSP + upmix sound handoff (2026-09-19)

> USER CONSTRAINT Sep20: **STATIC ONLY**. Continue from local firmware,
> disassembly, extracted calibration and offline model/tests. No vehicle
> connection, live RAM readback, diagnostic requests, passive vehicle capture,
> deployment or settings changes. This overrides older live-readback next steps.

## Передача следующему агенту — 60-секундный прогон с одним параметром (21.09, утро)

> **ОБНОВЛЕНИЕ 21.09 (день): после прослушивания в наушниках Luka выбрал следующие
> настройки для машины: retention 0,5 + `AUDIO_HSSR_BED_DELAYS=on` +
> `AUDIO_HSSR_BED_ATTACK=owner` + `AUDIO_HSSR_ATTACK_COLLAPSE=0.75` (остальное как было).
> Они уже в `carplay_startup.sh` (коммит) и в сборке `build/bed-experiment-20260921/car-LOG1/`,
> но **в машину ещё не поставлены**: Luka сказал «деплоить пока не надо». Деплой = 2 .so +
> скрипт, без перезагрузки, проверка по строке `audio upmix scene bed: delays=1 attack=1`
> в `/tmp/carplay_hook.log` при следующем подключении. Блок ниже описывает состояние ДО этого.**
>
> **Диск (21.09, 13:10):** по слову Luka удалены все WAV из гридов `output/upmix-*-2026092[01]`
> (report.json / .log / review сохранены, в каждой папке `WAVS_DELETED.txt`), папка прослушивания
> `output/listen-bed-20260921`, незавершённый retention-грид (24/28) и временные файлы сессии;
> освобождено 17 ГБ. 60-секундные исходные нарезки в `/private/tmp/upmix-bank1-audit.BR9uxY` (769 МБ)
> оставлены, это входы всех гридов. Retention-грид 0,5/0,917 НЕ досчитан и не нужен: п. 1 закрыт
> выбором r = 0,5 после прослушивания.
>
> **Чистка мёртвого кода (21.09, день, по слову Luka «режь всё»):** удалены float/cabin/quiet
> рендер-ветки (`AUDIO_FLOAT_RENDER`/`AUDIO_CABIN_RENDER`/`AUDIO_QUIET_LFE`, `process_f32`,
> `cabin_policy` в `hssr_dsp.c`, float-хелперы SoundCheck, `float_render_test`, оценочные
> скрипты float/cabin), `#ifdef`-эксперименты CENTER_NULL/SIDE_OBJECT и CAPTURE_TRANSFERS,
> миграционная ветка `SPATIAL_5_1`/`SPATIAL_103*`, алиас `AUDIO_SPATAIL`, ручки
> `SPATIAL_103_DCX/MODE/MASK/INDEX`, тюнер `/tmp/spatial_bit`. −837/+165 строк.
> Выход S16 при выбранной схеме, при дефолтах и при `--mode off` бит-в-бит равен
> прежнему (`cmp` на двух треках × 3 режима). Все 10 хост-тестов аудио зелёные;
> падения в `run_tests.sh` только InstallTest (были и раньше). Упоминания
> `AUDIO_FLOAT_RENDER` ниже в этом файле — историческая справка.
>
> **Realtime-апмикс (21.09, вечер):** HSSR теперь и для стокового realtime media-потока
> (тип 100, `audioType=media`: Spotify и все без 103) за `AUDIO_REALTIME_UPMIX=on` в скрипте.
> Отдельный экземпляр DSP и контроллера (`spa_rt_upmix` в `audio_hook.c`), правило периода
> `spa_media_route_realtime_upmix_allowed` (`media_route.h`, тест `media_route_test`):
> только realtime-период, только 2→6, только media и ни одного звонок/Siri/alert.
> Защита по времени на аудиопотоке (4 периода > 8 мс → off до закрытия handle, WARN).
> Простаивающий воркер HSSR теперь спит 4 мс вместо 250 мкс. Подробно:
> `docs/reference/AUDIOTYPE_DOLBY_AND_PCM_UPMIX.md`. **Не задеплоено.**
>
> **РЕШЕНИЕ LUKA 21.09: текущая калибровка на данный момент НОРМ.** Что стоит в
> машине сейчас: S16, `AUDIO_STEREO_SURROUND=0.40`, `AUDIO_STEREO_FRONT_BASS_DB=-3`,
> `AUDIO_STEREO_LFE_GAIN=0` (assist), retention 0, float-рендер выключен. Это ровно
> базовый вариант `startup` из 60-секундного прогона ниже; по модели его басовый
> баланс при равной громкости почти как у dry (вуферы −0,2 дБ, середина −0,7 дБ).
> **Ничего не подкручивать, дефолты и env не менять, деплоев не делать.**
> Float-кандидат (0,30 / −1 / 0,75) НЕ применять: это +1,9 дБ баса, а не его снижение.
> Любое изменение только после прослушивания Luka и только одним рычагом за раз
> (таблица «симптом → рычаг» в конце этого раздела).

Выполнены пункты 1–3 «финальных проверок» ниже (длинные фрагменты и хвосты,
один параметр за раз, баланс при выровненном уровне). Только офлайн; код,
настройки, машина и деплой не трогались. Полный отчёт:
[UPMIX_LONG_ONEVAR_2026-09-21.md](UPMIX_LONG_ONEVAR_2026-09-21.md).

- **6-секундные префиксы были нерепрезентативны.** На 60 с startup даёт средний
  сдвиг бас/середина **+0,50 дБ** (было −1,01), front bass 0 → **+3,38** (было +1,86);
  по трекам расхождение до 4,8 дБ. Четыре из семи 6-секундных вступлений были тихими.
  Выводы только по 6 с отозваны.
- **Один параметр за раз** (среднее по 7 трекам, относительно startup):
  front bass −3→0: вуферы +2,90, саб +3,3…3,4, середина/тыл/центр 0 — единственный
  чистый басовый рычаг, и он **двигает модельный саб**; surround 0,40→0,30: бас 0,
  фронт-середина +0,62, тыл −0,5…−0,6, центр +0,95; retention 0→0,75: бас 0,
  фронт-середина +2,43, тыл +1,15, центр +3,27, пики до +1,8 — это восстановление
  середины, не басовое исправление; float: −0,1…−0,3 везде, пики PCM6 −0,57.
  Прежний float-кандидат (0,30/−1/0,75) = восстановление середины + срез тыла +
  **прибавка баса ~1,9 дБ** (−3→−1); его «−0,46» не было снижением баса.
- **При выровненном уровне** апмикс на startup примерно на 3 дБ тише dry через ту же
  цепь: фронт падает (середина −3,8), модельный тыл остаётся на уровне dry (−0,07).
  В электрической модели тыл не становится громче, тише становится фронт.
- **Хвосты чистые:** все 42 строки ниже −80 dBFS со второго окна 250 мс и точный ноль
  с ~1,25 с; PCM6 обнуляется ≤49 мс после программы. Накопления состояния нет;
  для следующих прогонов достаточно хвоста 1 с.
- **Пики:** +4,3…+5,1 dBFS у всех вариантов, dry +6,35; 6 из 7 треков выше 0 dBFS в
  каждой строке. Это условный бюджет уровня (gain 1, лимитер 100), не клиппинг.
  Float-guard на 60 с активен на каждом треке (мин. gain 0,26 на Deep Down Low);
  S16 телеметрии не имеет.

План следующей версии сцены (три принципа Dolby: энергия переноса, задержки bed по
частоте, атаки): [UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md](UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md).
**Статус 21.09 (день): п. 2 и п. 3 реализованы** (`hssr_dsp.c`, `stereo_upmix.c`,
`audio_hook.c`, хост-рендерер, тесты), все механизмы — отдельные env-флаги
(`AUDIO_HSSR_BED_DELAYS`, `AUDIO_HSSR_BED_ATTACK`, `AUDIO_HSSR_ATTACK_COLLAPSE`),
default off, при выключенных флагах выход бит-в-бит прежний (проверено `cmp`).
ARM-сборка прошла и отложена в `build/bed-experiment-20260921/`, **в машину не ставилась**.
PCM6-метрики и чтение — §6 плана. П. 1 (retention 0,5 / 0,917) считается на гриде.

Что дальше (только офлайн): (a) EQ10 повторно не прогонялся — при желании тот же
грид с `--input-eq-bank 10`; (b) пункт 4 «чувствительность к допущениям» остаётся;
(c) если нужен «меньше баса без потери середины», единственный чистый рычаг в модели —
front bass (тянет и саб); retention/surround — это баланс фронт/тыл/центр. Выбор
пресета остаётся за прослушиванием, модель его не заменяет.

Таблица «что слышно → один рычаг» (эффекты из измеренных дельт, env в
`carplay_startup.sh`, применяется при следующем подключении телефона, без перезагрузки):

| Симптом | Рычаг | Эффект на шаг (модель) | Побочка |
|---|---|---|---|
| баса много | `AUDIO_STEREO_FRONT_BASS_DB` −3 → −4/−5 | вуферы −1, саб −1,1 дБ за 1 дБ | ничего больше |
| баса мало | тот же, −3 → −2 | +1 / +1,1 дБ | ничего больше |
| фронт пустой, вокал провалился | `AUDIO_STEREO_FRONT_RETENTION` 0 → 0,25…0,5 | фронт-середина ≈ +0,8…+1,5, бас 0 | тыл +0,4…0,7, центр +1…2, пики выше; 0,75 не брать |
| тыла много | `AUDIO_STEREO_SURROUND` 0,40 → 0,30 | тыл −0,5, фронт-середина +0,6, центр +1 | бас 0 |
Стоимость: ~11,5 мин на строку 64 с, 6,4 ГБ пик на шард; 3 шарда ≈ 3,5 ч на 42 строки;
грид на диске 10,5 ГБ (`output/upmix-long-onevar-eq7-20260921`, можно удалить 16-канальные
WAV после ревью — хеши в report.json).

## Предыдущая передача — итог 6-секундного прогона 21.09 (частично устарела, см. выше)

Последний запрос пользователя: прогнать звук через полную доступную модель и
проверить, всё ли нормально. Прогон завершён; **безоговорочного «всё нормально»
нет**. Это проверка условной электрической DSP-модели, не замер машины и не
подтверждение субъективного качества Virtual Cabin.

- 56 расчётов: 7 треков × 2 гипотезы входного EQ (7/10) × 4 варианта.
  Только первые 6 секунд каждого трека + 0,25 секунды нулевого хвоста,
  **не полные песни**. Сценарий `full_static_nearest_zero_aux`.
- При LFE assist gain = 0 и front bass = 0 средний сдвиг баса 60–120 Гц
  относительно середины 500–2000 Гц составляет **+1,85 дБ** к dry через тот же
  BANK1. Это согласуется с жалобой пользователя на бас, но не доказывает её
  причину в салоне. Dry здесь **не штатное стерео/bank0**; loudness matching нет.
- Параметры локального startup (S16, surround 0,40, front bass −3, retention 0)
  дают около −1,02 дБ; float-кандидат (surround 0,30, front bass −1,
  retention 0,75) — около −0,46 дБ. LFE assist gain везде 0 дБ, **не mute**.
  Кандидат меняет несколько параметров одновременно: эффект retention отдельно
  этим сравнением не установлен. Front bass влияет и на модельный саб после
  downstream-микширования; старый комментарий «sub untouched» не описывает весь тракт.
- Максимальные sample peaks модели: +4,85 / +4,59 / +4,22 dBFS соответственно;
  dry достигает +5,87 dBFS. Это **не доказанный клиппинг усилителя**: pre/post
  gain = 1 и limiter threshold = 100 заданы условно. Реальные громкость,
  SoundCheck, лимитеры и headroom неизвестны. Startup проверен при unity
  programme gain, без реальных SoundCheck metadata и унаследованных env overrides.
- Все 42 upmix PCM6 конечны, без отсчётов у предела S16; все 56 выходов конечны.
  Пересчитаны метрики всех сохранённых WAV, проверены 309 файловых хешей.
  Отсутствие float-guard telemetry в S16 не означает отсутствие защиты.

Что делать дальше **только офлайн/статикой**:

1. Прочитать [полный отчёт](UPMIX_FULL_MODEL_SOUND_CHECK_2026-09-21.md);
   воспроизводимые результаты — `output/upmix-full-model-check-eq7-20260921/report.json`,
   `output/upmix-full-model-check-eq10-20260921/report.json`, проверка —
   `output/upmix-full-model-check-20260921/review.json` и `review.py`.
2. Разделить влияние настроек: сравнивать по одному изменяемому параметру,
   затем проверить более длинные и новые музыкальные фрагменты.
3. Проверить чувствительность к явно заданным уровням/состояниям и искать их
   привязку в локальных данных. Не выдавать выбранную гипотезу за текущую машину
   и не назначать компенсирующий общий gain только по этим положительным пикам.
4. Сохранять ограничения: aux = 0, adaptation = 0, свежие истории; реальная
   активность второго B не проверена. Не закрыты consumers 1162..1165,
   динамические аудиопереходы, точность SHARC, физическая распиновка и акустика.
   Подробности — [static completion](UPMIX_STATIC_COMPLETION_2026-09-20.md).

В этом музыкальном прогоне production-код/настройки не менялись, деплоя,
подключения к машине и коммита не было. Чужой WIP сохранять.

### Финальные проверки модели перед выбором настроек

Следующий план согласован как направление проверки. **Пункты 1–3 выполнены
21.09 утром (см. верхний раздел); пункт 4 остаётся.** Всё только офлайн/статикой,
без изменения настроек машины.

1. **Длинные фрагменты и хвосты.** Прогнать доступные 60-секундные фрагменты,
   затем подавать тишину до затухания. Заранее определить порог затухания и
   максимальную длительность хвоста; достижение лимита не считать затуханием.
   Проверить накопление состояния, пики, активность доступной защиты и
   остаточный сигнал. Нынешние 6 секунд + 0,25 секунды хвоста этого не закрывают.
2. **Один изменяемый параметр за раз.** При общей исходной конфигурации отдельно
   сравнить front bass, surround, retention и S16/float; остальные параметры,
   входной сигнал и состояние DSP фиксировать. Не приписывать эффект нынешнего
   float-кандидата одному retention: одновременно изменены несколько настроек.
3. **Баланс при одинаковой громкости.** Для анализа тембра сравнить варианты
   с документированным методом выравнивания уровня: бас/середина, вокальный
   диапазон, тыл и саб. Пики, защиту и запас уровня считать отдельно по исходным
   ненормализованным сигналам; исходные WAV сохранять. Не объявлять более тихий
   вариант более качественным только из-за снижения абсолютных уровней.
4. **Чувствительность к допущениям.** Проверить явно обозначенные гипотезы
   входного EQ, усилений, лимитеров и ненулевых auxiliary-входов. Отделять
   подтверждённые локальными данными параметры от условных проб. Если вывод
   «бас исправлен» держится только на одном условном состоянии, настройку
   нельзя считать устойчивой или подтверждённой для машины.

**Отдельный незакрытый пункт — динамические аудиопереходы:** переключения
должны моделироваться с сохранением историй фильтров и задержек и проверкой
щелчков/выбросов. Имеющийся расчёт событий изменения коэффициентов сам по себе
не является таким аудиорендером; отсутствие этой реализации явно отмечать.

Приоритет следующего агента: **длинный прогон + сравнения с одним изменяемым
параметром**, затем анализ при выровненном уровне и чувствительность к состояниям.
Даже успешный результат подтверждает только поведение условной электрической
модели, не идеальное звучание салона: реальные уровни, акустические передаточные
функции и оставшиеся статические пробелы из предыдущего раздела сохраняются.

## Detailed chronological evidence

> LATEST MUSIC CHECK Sep21: [full conditional model](UPMIX_FULL_MODEL_SOUND_CHECK_2026-09-21.md).
> New56/56 rows:7tracks×6s+.25tail×EQ7/10×dry/zero-trim/startup/floatcandidate.
> Scenario full_static_nearest_zero_aux.309 file hashes +56 saved WAV metrics
> verified. All42 upmix rows have zero PCM near-rails/digitalC; dry884 near-rail
> samples are inherited from input masters, not introduced by upmix.
> Mean bass60–120/mid500–2000 delta vs same-BANK1 dry: zero-trim+1.85dB,
> startup−3 about−1.02dB, floatcandidate−.46dB. Worst peaks+4.85/+4.59/+4.22;
> dry reaches+5.87 too. Runtime gain1/limiter100 are hypotheses, not evidence
> of actual amplifier clipping. Startup means .40/−3/LFE0/S16/retention0 at
> UNITY programme gain; real SoundCheck metadata/inherited environment absent.
> No code/default/deploy changes. Next: level/state evidence and isolated
> controlled comparisons; continue static-only, retain known alias/transition gaps.
>
> LATEST STATIC NUMERICS/PUBLICATION PASS Sep20:
> [Current integration/limits](UPMIX_STATIC_COMPLETION_2026-09-20.md).
> **704 Python tests pass (121 extraction +583 model/cabin/runner)**; existing
> C S16/float sanitizer and NEON binaries rerun PASS. No C/default/deploy/commit.
> New `full_static_nearest_*` / `full_static_floor_*` scenarios bind actual
> log10/pow10 algorithms and stored internal FFT table in addition to the
> retained spectral controls. FIX rounding is explicit, not observed MODE1.
> Aux and adaptive axes are now separable; old neutral/retained host-math
> choices remain. Prior76 musical rows below are not upgraded retroactively.
> New publication helper covers full829 target/effective/steps and raw flag;
> mode5 schedule covers313 accesses/512samples (all31 ramped fields once).
> **This is coefficient state, NOT dynamic delay/filter audio transitions.**
> Filter alias proof now checks72 retained-bank/descriptor combinations, but
> consumers1162..1165 and complete fixed-mode5 alias closure remain unresolved.
> Completed output/upmix-static-state-grid-20260920/report.json512/512,64
> contexts:16inputEQ×2maps×2FIX×2adaptive×2complexaux×2secondBaux.4096-frame
> synthetic probes only;68 referenced hashes verified, worst-difference context
> rerun exactly. No transition/full-track/acoustic acceptance implied.
> Fresh output/upmix-e2e-static-math-20260920/report.json4/4 compares retained
> host vs new math on DeepDownLow6s+tail,EQ10,dry+candidate.83 file hashes and
> all4 saved-WAV metrics verified; old-mode PCM6/output/metrics unchanged.
> FLOAT32 max difference1.1921e-7; peaks unchanged+5.872/+4.139dBFS. Not a
> production bass fix or a reason to ignore positive electrical model peaks.
> Do not relabel those static gaps as only an unavailable live measurement.
> Additional [spectral pointer review](SHARC_B_SPECTRAL_POINTER_DOMAINS_2026-09-20.md)
> conditionally excludes analysis-history pointer reads12D55E/12D5F7 and ramp
> read12ADC6 using separate allocation/caller provenance; not global closure.
> Next static work: remaining nested-allpass ring/other alias provenance;
> interleave coefficient events with preserved audio/delay
> histories if transition rendering is needed; retain arithmetic boundaries.
>
> LATEST RETAINED FIRST-B SPECTRAL PASS Sep20:
> [Integration/results](UPMIX_RETAINED_B_2026-09-20.md),
> [header bindings](SHARC_B_SPECTRAL_BINDING_2026-09-20.md),
> [spatial init](SHARC_B_SPATIAL_BINDING_2026-09-20.md),
> [independent clamp/table audit](SHARC_FIRST_B_SPECTRAL_AUDIT_2026-09-20.md).
> **658 Python tests (121 extraction +537 model/cabin/runner) pass**; C
> S16/float ASan+UBSan and NEON rerun pass; diffcheck clean. No C/default/deploy.
> NEW `full_retained_zero_aux` / `full_retained_aux_probe` eliminate unity
> spectral placeholders: B01 common245-word header supplies pre/main/post/level/
> smoothing/object97; real74-band map, effective spatial bank/init gate1,
> firmware-polynomial window and stored packing twiddles. Old neutral diagnostics
> remain explicitly separate. Level-mask bug fixed: p14MAX,p15MIN (transient
> order different and correct). Fresh host math is NOT SHARC bit-exact.
> Retained scenario uses consistent API20chosen THEN API16=5 pointer maps,
> crossfeed493 and object97=.5011872053. Delivery/effective snapshot/fresh state
> are assumed, not actual car or full replay. All ongoing-state caveats remain.
> Completed output/upmix-e2e-retained-spectral-20260920/report.json70/70 and
> output/upmix-e2e-retained-aux-20260920/report.json6/6. Six-second prefixes+.25s
> tails only.174+85 unique file hashes verified; six saved output metric rows
> recomputed exactly. All35 mainPCM6 hashes/baseline_v7 metrics unchanged.
> Retained float tilt+1.069dB→candidate retain75/surr.30/bass−1 tilt−.461dB;
> candidate maxpeak+3.282dBFS still prevents a hardware-safe preset assertion.
> XLSX read-only evidence: [speaker workbook](B9_SPEAKER_WORKBOOK_EVIDENCE_2026-09-20.md).
> External19-driver/16-pair topology/pinout, NOT DSPcfg→pin proof or Bcoeffs/IR.
> SourceD10=#REF! and conflicting tweeter ranges preserved, not silently repaired.
> NEXT: longer/held-out music and explicit state sensitivity; actual current
> selectors/history/aux, hardware arithmetic/protection and acoustic IRs remain
> unavailable by static analysis. Prior dynamic-pointer/lifecycle boundaries
> below remain, but retained spectral parameter binding is no longer a TODO.
>
> LATEST PRACTICAL END-TO-END PASS Sep20:
> [Whole-upmix runner and interpretation](UPMIX_END_TO_END_2026-09-20.md).
> **625 Python tests pass (121 extraction +504 model/cabin/runner)**; fresh C
> S16/float ASan+UBSan and native NEON pass. No production/default edits here.
> Actual fresh host C→PCM6→explicit DSP→common-window metrics is now executable
> with `audit_upmix_end_to_end.py`; `_batch.py --jobs N` isolates parallel tracks.
> Completed music report: output/upmix-e2e-music-parallel-20260920/report.json,
>70/70 rows (7tracks×[4variants+dry]×2scenarios); aux report:
> output/upmix-e2e-aux-probe-20260920/report.json,6/6 rows with saved16chFLOAT32.
> Both use6-second prefixes +0.25s tail, NOT full tracks.169+80 referenced file
> fingerprints verified; all6 saved aux metric rows independently recomputed.
> Main28 candidatePCM6 files: zero near-rail hits, digitalC exactly0. S16 logs
> lack float guard counters, marked incomplete rather than invented.
> Paired bass/mid tilt float reference→retain75/surr0.30/bass−1:
> v7+1.370→−0.356dB; composed+1.417→−0.344dB. Not isolated retention effect.
> Composed scenario retains first-B tails but uses SYNTHETIC unity spectral
> controls; +11/+9/+1dB tail gains can produce very high peaks. No car-clipping
> diagnosis or compensating production gain follows. Actual current B/selectors,
> spectral masks, hardware timing and cabin IRs remain unverified. Full-fidelity
> and subjective-optimum gates stay false. Host sync inference≠QNX async/QSA.
> NEXT: use the runner for longer/held-out musical comparisons; bind remaining
> first-B spectral controls before using composed absolute levels for tuning.
> Earlier sequential e2e-music directory is partial16/70, not accepted evidence.
> No car I/O, deployment, default changes or commit; dirty shared work preserved.
>
> LATEST PARALLEL STATIC CLOSURE, third pass Sep20:
> **592 tests pass (121 extraction +471 model/cabin)**; git diff --check clean.
> [Effective first-B tails](SHARC_B_TAIL_BINDING_2026-09-20.md): all3 tails and
> finish bind to supplied829-word config +18 pointers, joined to supplied spectral
> reference for mode5. Object97 gain/spectral state remain explicit, not recovered
> as a complete live preset. Independent review fixed float32 count rounding and
> preservation of requested delay/config provenance.
> [Nested zero-delay](SHARC_NESTED_ZERO_DELAY_2026-09-20.md): active requested0
> means one addressed history cell under valid-memory/zero-cursor assumptions;
> B01bank4 now supported conditionally, not bypassed.
> [Second-B controls](SHARC_SECOND_B_2026-09-20.md):16 input-EQ records, explicit
> loader counts8/4/4 hypothesis,3master/48fanout gain targets,scalar/filter and16
> delay targets bound. Mode1 step generation now numerical with explicit prior
> mode/target/steps/time; updates once per64 frames, not a generic linear ramp.
> [Wrapper control](SHARC_B_WRAPPER_CONTROL_2026-09-20.md): callback states/errors,
> exact inhibit==1, alternate stereo fanout, transition clearing through12DF07.
> Later output-mode/reset and real event scheduling still outside the model.
> [Pointer alias audit](SHARC_B_BINDING_2026-09-20.md#affine-propagation-audit-and-bounded-smoothing-closure):
>12FAF2 checks only a lower bound. Proven callsites under retained config/counts
> and pointer maps avoid cells1162..1165; no global dead-field proof. Mutated
> counts invalidate that exclusion; remaining dynamic traversals stay explicit.
> All-stage synthetic integration: output/upmix-composed-static-20260920/report.json.
> Both B-boundary injection vs internal composition max error0. All electrical
> scenario-presence gates true FOR THIS FIXTURE; live/acoustic/optimum false.
> It does NOT inherit392-cell coverage from different historical v7 scenarios.
> NEXT STATIC: remaining spectral parameter/control binding; dynamic pointer
> traversals12D97A/12D97C/12DCDB/12DCE5; post12DF07 output-mode/reset; transitions,
> allocation/errors and finite-precision hardware equivalence. Actual external
> selectors/aux PCM/acoustic IRs remain unavailable under STATIC ONLY.
> No car I/O, production/default change, deployment or commit in this pass.
>
> Previous parallel static closure, second pass Sep20: **536 tests pass**
> (121 extraction +415 model/cabin). Static/offline only; no production/default
> changes, car I/O, measurements, deployment or commit in this pass.
> **C4D17 detector bug fixed:** stage12 compacts samples0/16/32/48 before stage13,
> not the original first4 samples. Full-mode reference is now v3; both profiles'
> adaptive scenarios rerun. Default identity-path musical tuning is unaffected.
> [Calibration composition and corrected reports](SHARC_COMPLEX_CALIBRATION_2026-09-20.md).
> Fresh extraction: output/r2/soundamp/remaining-blocks-20260920-v3/ (168 ranges,
> UNKNOWN0). Older extraction preserved for historical report hashes; its detector
> selection text is superseded. Only complex_control_profiles differs.
> [Second-B numerical reference](SHARC_SECOND_B_2026-09-20.md): all30 nodes21..50
> from explicit aux8/9/10/state to16 outputs, fixed writer-target partial overlay,
> now integrated into AmpChain opt-in v9 and cabin bridge; default remains v7.
> [Nonzero first-B EQ](SHARC_B_EQ_DESIGN_2026-09-20.md): all3 successful designer
> paths, 297 decoded outer-instruction comparisons; not bit-exact transcendental math.
> [Wrapper timing](SHARC_B_WRAPPER_TIMING_2026-09-20.md): on-time successful
> two-bank processing adds1024 samples; plus neutral FFT512 gives1536/32ms.
> This is conditional steady timing, not proof of the live callback schedule.
> NEXT STATIC: indirect pointer1162..1165 consumers (pointees are now classified
> delay curves), remaining input/mode/control-writer binding, effective-state and
> transition/error lifecycle; numerical precision beyond host math references.
> Actual aux PCM/current external selectors/measured cabin IRs cannot be inferred
> from this firmware. Full-fidelity gate remains false.
>
> Previous parallel static closure Sep20 (490 tests at that point):
> [B bindings](SHARC_B_BINDING_2026-09-20.md): effective829-word snapshot→mode2/5
> output finish; all15 scalar replay targets incl API21/22 final gain420 now bound.
> Targets are not smoothed effective state; remaining spectral/pointer/lifecycle
> assumptions still explicit. [C4D17 timing](SHARC_COMPLEX_TIMING_CLOSURE_2026-09-20.md):
> two calibration-noise shaping biquads/gates/sequence added; NOT normal music.
> Second-B producer C4CF9→C4DFB traced to aux graph8/9/10, not music0..7.
> [Cabin bridge/legacy repair](UPMIX_CABIN_BRIDGE_2026-09-20.md): stale documentary
> transpose corrected and crosschecked all64 coefficients; obsolete six-bus tool
> fails closed without historical-only opt-in. New amp_cabin_render joins explicit
> AmpChain to16cfg→ears IRs OR bypasses DSP for6 MOST→ears IRs. Only synthetic
> smoke fixtures exist; no measured acoustic fidelity claimed.
> [Readiness contract](UPMIX_MODEL_READINESS_2026-09-20.md):392/392 stress grid
> passes strict hashes/shapes/axes; full-fidelity gate fails as intended.
>490 relevant Python tests pass:121 extraction +369 all model/cabin (includes
>356 amp tests +13 documentary/digital tests). No C production/default changes
> in this step. No car I/O, measurements, deployment or commit.
> NEXT STATIC: remaining B1162..1165 indirect consumers, nonzero EQ designer,
> full mode/input/wrapper binding and numerical second-B aux processing;
> calibration detector/LMS composition. Actual aux/cabin PCM, current external
> selectors and measured IRs cannot be invented from firmware.
> CRITICAL Sep20: [matrix orientation correction](SHARC_MATRIX_CONTROL_2026-09-20.md).
> A903C writes RAM[8*out+in]→C022A[8*in+out];126A02/126A07 read in*8+out.
> RAM rows are OUTPUTS. v5/v6 wrongly transposed: model-based “sub only LFE”,
> “woofers sum L/Ls/C/LFE” and old speaker bass figures below are WITHDRAWN.
> AmpChain now v7/v8; schema2, corrected CSV.403 tests(121+282),168 SHARC
> ranges UNKNOWN0,43 V850 listings. Two correction tables are0; third−9..9dB
> affects output rows4..7 only at ASAD C0/processing5A. Zero→0; pair odd
> correction still turns mute−100 into floor−99 through clamp. No car-state proof.
> OFFLINE RERENDER DONE: [seven-track v7/v8 results](UPMIX_MATRIX_RERENDER_2026-09-20.md).
> All28 PCM hashes match. Baseline woofer60–120Hz−0.380dB versus dry BANK1,
> front mid500–2000Hz−4.684dB: relative bass/mid tilt+4.303dB. NOT stock/SPL.
> On two tracks zeroing LFE increases sub20–60Hz by1.26dB; old transpose alone
> reproduces historical v2 band powers exactly.405 tests(121+284).
> MIDRANGE ATTRIBUTION DONE: [seven-track decomposition](UPMIX_MIDRANGE_ATTRIBUTION_2026-09-20.md).
> Raw L/R500–2000Hz already−4.483dB; centre summation change adds−0.663dB.
> C exactly0; negligible rear/LFE effect on front/centre mid in this scenario.
> Diagnostic half/full front recovery restores mids but also raises rear output;
> not a production patch.411 tests(121+290), recomposition error<3.36e−12dB.
> LOCAL IMPLEMENTATION DONE: [corrected guards/retention/sweep](UPMIX_GUARD_RETENTION_2026-09-20.md).
> C guards now share corrected48 coefficients; S16 has linked fallback when LFE
> alone cannot bound buses. Fixed transfer overflow. New independent front retention
> API/CLI/env(default0, no startup changes).35 fresh renders on7tracks: float75%
> tilt+0.880dB, rear-mid+1.051; all input buses bounded, no PCM rail hits, but
> post-EQ model peaks+4.330dBFS. Not production/listening accepted.416 Python tests;
> C upmix/float/hook sanitizer tests pass. Full suite has unrelated installer failures.
> OFFLINE TUNING DONE: [105-render grid/bass + EQ/B stress](UPMIX_TUNING_SWEEP_2026-09-20.md).
>75% retention/rear0.30/bass0 gives mean tilt+0.679dB; bass−1 gives−0.215dB
> and cuts guard-frame occupancy11.344%→5.028%.100%/0.30 gives−0.134dB.
> Post-EQ peaks still+4.481/+4.701/+4.964dBFS; input guard is insufficient
> for downstream bounds.392 EQ/B scenario measurements complete; peaks up
> to+6.180dBFS. EQ10 vs7 on dry can add mean4.979dB woofer /5.719dB sub:
> explicit hypotheses, not live selector proof. B stress uses arbitrary donors.
>28 matched6ch files in output/upmix-tuning-matched-final-20260920/pcm,
> reread verified: max peak−1.199895dBFS and matching error<0.00000456dB
> under v7 only.421 Python tests(121+300); float sanitizer tests pass.
> NEXT: subjective/selectivity evaluation remains unperformed; measured cabin
> transfer/live selectors not available under STATIC ONLY. No production preset
> or universal downstream gain selected. No car I/O/deploy/commit; no ARM build.
> NEXT STATIC:
> API29 and outermost request/UI mapping; possible indirect
> pointer consumers1162..1165 and other extended families/error lifecycle,
> nonzero12AB1F EQ math if needed,
> B-target composition, wrapper timing/routing
> and outer request binding. Values depending
> on external runtime inputs stay explicit alternatives, not asserted live state.

> Latest static Sep20: [outer A21/API18 audit](SHARC_OUTER_CONTROL_2026-09-20.md).
> F5/A21 op2→D40C8 and alternate A018/A21 op0→D4130 forward two BE32 raw
> command/value words to queue369→AC648. Extended writes are reachable, not
> proven used during playback. Separate read route370 waits/reads B95E8;
> write response is staging data, not verified DSP result. Corrected prior gap:
> dedicated API18 sender AA886(events22/368) uses float(s8(gp−15840)).
> Block22/D28 CA4B4 selector5/0 accepts−12..12 and clamps−9..9; init0, zero→0.
> Same byte changes other coefficient blocks: now bound by critical correction above, no Bass UI claim.
> 392 tests(117+275),166 SHARC ranges UNKNOWN0,41 V850 listings. No car I/O.

> Previous static Sep20: FIFO1326C7 canonical indexed writes now bound. Config
> indices1..842 write BOTH effective object299+i and target+i; target store is a
> branch delay slot.1..829 only rebase;830..842 replay, mode20 then16 BOTH using
> effective20/object1132, then current-bank replace.829..842 overlap effective
> controls16..29; target829 overlaps B8CFD interpolation steps. Raw writes bypass
> normal float/defer validation and do not update shadows. Core index5 reachesgain97.
> Command20000003(rawbank1..4) CRCs effective config then copies829 words to source
> RAM, NOT flash. Normal pending pass precedes FIFO; full error timing not emulated.
> Explicit-state effect plans;1052 decoded-record and3213 validation comparisons.
> 383 tests (110+273),166 ranges UNKNOWN0. No sender/live-state/bass-cause proof.
> [Evidence](SHARC_B_CONFIG_2026-09-20.md#extended-fifo1326c7-direct-writes-and-source-ram-mutation).

> Previous static Sep20: pointer1161 is post spectral crossfeed via core+1102.
> Lout=(1-c)L+cR, Rout=(1-c)R+cL: sum/identical mono unchanged by THIS stage.
> All eight stored banks bound; B01 bank2 c=.27/.28 depends on last API16/20.
> Explicit pointer resolver now supplies post_crossfeed; decoded caller oracle added.
> Consumers1162..1165 STILL unresolved: aligned16,093-instruction B-span census
> finds no direct full-object/core-relative reads, NOT proof of no indirect use.
> Additional12B348 callsites1327EA/132805 in1326C7 are next queue-analysis lead.
> 369 tests (107+262),159 listed ranges UNKNOWN0; defaults unchanged, no vehicle I/O.
> [Evidence](SHARC_B_CONFIG_2026-09-20.md#pointer1161-crossfeed-and-the-four-unresolved-fields).

> Previous static Sep20: API16/20 helper12B348 binds18 pointers into EFFECTIVE
> config299. Shared1156..1165 are last-applied-wins; independent suffixes retained.
> Same effective16=5/20=3 can select different records: B01 calibration1 four-tail
> final gain0.630957 vs0.446684 depending apply order, NOT total bass/SPL proof.
> AUTO20=6 resolves from effective config487 before defer/shadow (1 iff exact1,
> otherwise3); all eight retained image banks resolve3. Same pending pass applies
>16 before20, but V850 sender order does not prove queue/pass timing.
> Explicit-state pointer model + decoded-instruction tests; defaults unchanged.
> 361 tests (104+257),159 SHARC ranges UNKNOWN0. No car I/O or live-state claim.
> [Pointer evidence](SHARC_B_CONFIG_2026-09-20.md#api1620-effective-configuration-pointer-selection).

> Previous static Sep20: API17 selects four829-word B calibration banks, NOT matrix
> bank0/1/2. Event46 uses A07 profile table+1 (in-domain candidates1/2 or rejected0).
> Copy validates version+CRC, rebases15 descriptors WITHOUT clearing slots,
> replays effective19/18/22/21/23/24/25/27/29, publishes. Replay25 is RAW1137,
> unlike single-update baseline subtraction; all image baselines remain0.
> All four loader/B01 banks extracted, CRC checked; B01 differences vs bank1:
> 31/53/271 words for banks2/3/4. Explicit bank bindings and model helpers added.
> 348 tests (100+248),155 SHARC ranges UNKNOWN0,25 V850 listings. No live-state claim.
> [B configuration evidence](SHARC_B_CONFIG_2026-09-20.md). Defaults unchanged.

> Previous static Sep20: all15 API19 /12 API18 scalar destinations now bound,
> including selector-dependent delay curves52/53 and four EQ rebuilds55..58.
> B01 individual two-tail delays: API19−50/0/+100→162/96/19 samples with other
> slots0. Shadow16==5 takes priority over shadow20==3; all three image variants match.
> Four EQ sections feed six/two-channel finish; loader/B01 bases and18/19 curves0,
> hence identity ONLY with other contributions0 and retained image config.
> api18_19_parameter_targets requires all prior slots/selectors; nonzero EQ
> designer is rejected, not silently identity.334 tests (92+242),148 SHARC ranges
> UNKNOWN0,25 V850 listings. Defaults unchanged; car state/B=0 still unproven.
> [Evidence](SHARC_LEVEL_CONTROL_2026-09-20.md#api1819-full-scalar-destinations-and-eq-rebuilds).

> Previous static Sep20: all9 branch-gain slots bound to APIs19/18/22/21/23/24/25/27/29.
> Loader curves zero; B01 nonzero for18/19/22/27. API18=+9 can add1.5 to six gain,
> API27>=180 adds0.7; API19 changes two gain. Conditional target math, NOT Bass
> naming/live-state proof. V850 event writers linked;18/29 only generic proxy
> currently bound. API18/19 also change other DSP descriptors, not yet composed.
> amp_b_control.branch_gain_targets requires all coordinates + prior slot banks;
> descriptor51 slot1 retained.323 tests pass (89+234),146 ranges UNKNOWN0,
> 25 V850 listings;123 synthetic +260 B01 decoded-kernel comparisons.
> Defaults unchanged. [Evidence](SHARC_LEVEL_CONTROL_2026-09-20.md).

> Previous static Sep20: API25 curves now bound to linear branch gains:
> descriptor46→config122/object421/four,50→293/592/six,51→377/676/two.
> Loader AND B01 curve773..790 and baseline791 are zero: level slot6 contributes0,
> NOT B=0/unity. Initial bases1/1/1.6; eight other additive slots remain explicit.
> Publication selector==1 copies immediately; otherwise prepares steps. Count
> config2 differs: loader200, B01=1; not yet a duration in ms/samples.
> New explicit curve reference +119 bounded decoded-SHARC cases;316 tests pass
> (87 extraction +229 model),137 bounded ranges UNKNOWN0. Defaults unchanged.
> Details: [level source audit](SHARC_LEVEL_CONTROL_2026-09-20.md).

> Previous static Sep20: trim writers C7870/C53A0 and override inner ID375 bound.
> Raw80 leaves a trim field unchanged; stored trim≤−13 mutes downstream.
> Event52→API25 uses max(0,selected target+100), NOT the smoothed EQ word.
> SHARC API25 shadow1151 updates; defer1178 gates effective1137 and helper
> 12CE71(value−baseline1090). Curve coordinate is NOT audio dB gain.
> 308 tests pass (86 extraction +222 model);22 V850 listings,133 SHARC ranges
> UNKNOWN0. Explicit control helpers only; no default/live-state inference.
> Details and next bindings: [level source audit](SHARC_LEVEL_CONTROL_2026-09-20.md).

> Previous static Sep20: gp−77F8 is lane0 of4 smoothed source-level Q24 controls,
> NOT PCM energy. Normal target=min(26,raw0..126−100+trim), subject to active,
> inhibit, trim and mute gates; raw0 mutes. Override source is separately bound.
> Normal EQ enable initializesA5, override mode/enable5A, cached index0. Commit
> gate can retain the previous bank; queue17 is not delivery proof. Selector
> uses current ramp value, before a separate gain-publication offset.
> New explicit control helpers + bounded decoded-V850 execution comparison196
> cases. 298 tests pass (84 extraction +214 model);16 new V850 listings valid.
> Raw70/trim0→level−30→bank10/+6dB@55Hz, NOT70% MMI. EQ gain is not total
> amplifier gain including volume attenuation. No automatic live/default preset.
> Details: [level source audit](SHARC_LEVEL_CONTROL_2026-09-20.md).
> Prior status blocks below are historical.

> Latest Sep20 integration: AmpChain now accepts explicit common-input and
> first-B mode5 scenarios. Common processed lanes feed BOTH A07 and B; B maps
> to mixer slots0,1,4,5,6,7,8,9, with gains applied once. Physical PCM6→graph8
> mapping, B parameters/history and wrapper delay must be supplied, not guessed.
> Defaults stay v5/B=0; opt-in version is v6-explicit-common-input. 287 tests
> pass (81 extraction +206 model), including10 common-input integration tests.
> Two existing PCM6 clips, bank10 versus flat bank7, B1/B2=0 and C4D17 identity:
> 20–60Hz gains +5.95..5.98dB front woofers, +5.87..5.94dB subwoofer.
> These are conditional electronic results, NOT stock comparison or cabin SPL.
> Report: `output/r2/soundamp/remaining-blocks-20260919/common-input-music-smoke.json`.
> Live readback remains blocked by lack of a verified external-J525 RAM
> transport; RCC /dev/io-amp is not it. No new car contact/settings/deploy/commit.
> NEXT (revised by static-only constraint above): trace selector source/flags,
> physical input routing, B parameter loading and wrapper timing in firmware.
> Synthetic B routing tests are not a calibrated/live full-B music render.
> Details/API: [input-path audit](SHARC_INPUT_PATH_2026-09-20.md).
> All status/next-step blocks below are historical.

> Latest Sep20: input writer bindings and C4A54 steady delay recovered.
> **Common input EQ C4AB7 feeds BOTH A07 and B**, not only B.16 level-selected
> banks,55Hz peak up to+20.5dB independent of tone Bass/LFE controls. Bank7 flat;
> disable flag selects bank0 (HF cuts), not7. No live selector/level readback.
> Candidate selector B0066 uses signed gp−77F8/2^24 (truncate), threshold table;
> commit gp−7844=A5; event17 sends8 selectors if gp−3E2A=A5, otherwise0.
> Shared tone/graphic writer families, unity input-gain targets and bypass
> prefilter bound to calibrated sources. C4A54 target is uniform8-lane delay:
> clamp(48*enabled_request_ms−2967,0,9600). State machine/TCBs decoded;
> steady delay independently tested with a shared-ring DMA simulation.
> New explicit `BInputPathFixedReference` and32 upstream response scenarios.
> 277 tests pass;127 ranges UNKNOWN0. AmpChain defaults unchanged, no car access.
> NEXT: verified live flags/level/tone/request; physical graph0..7 binding;
> join common input scenario to BOTH A/B for music comparison, preserving
> the distinction between calibrated targets, settled scenario and actual RAM.
> Full evidence: [input-path audit](SHARC_INPUT_PATH_2026-09-20.md).
> Prior status/next-step blocks below are historical.

> Sep20 static follow-up: mode5 gain source recovered. Init copies source+38
> to object+92 (207 words), so object+97 comes from source+43. B95E5 points
> to C3C6A: loader C3C95=0.5; B01 CodeFlash1200B8=0.5011872053 (about−6dB).
> B01 upload ACFB4 is conditional on gp−455C=A5, valid count, delivery and
> initialization order; neither value is a live readback. Normal initialization
> registers interpolation base object+299, so12FAF2 skips object+97 entirely.
> Pre-B graph inventory is now recorded: transfer/delay→shared biquad→gain→
> pack→five shared biquads→three per-channel banked biquads→unpack→B.
> CORRECTION: A07 matrix C4AED is AFTER B, not before these inputs.
> New `amp_b_input_eq.py` models12618C with explicit effective parameters;
> record order [-a2,-a1,b2,b1], implicit b0=1, per-channel gain once, no mixing.
> 257 tests pass (74 extraction +183 model);121 bounded ranges UNKNOWN0.
> Next static work: bind upstream coefficient/gain/selector writers and resolve
> C4A54 transfer/delay state machine/source mapping; then compose the actual
> upstream scenario. Other modes/lifecycle and execution validation remain.
> No car access, production changes, default AmpChain changes or commits.
> Memora handoff1140 points here. Older milestones below are historical.

> Latest static follow-up: Bank1→library mode5 linkage verified. C2852 uses the
> same gp−15760 byte to select64-byte matrix bank; event48 sends API16 from
> table2/5/5. Sink0x26 raw bank1 survives all ASAD cases, so refresh requests5.
> This is NOT a live playback readback. Prior statement that the index was
> unrelated to matrix bank is withdrawn; MMI3D is still separate.
> `BMode5FixedReference` now composes three boundary PCM lanes to eight B outputs:
> prepare (S0+g*S2,S1+g*S2), effective linear g at object+97 explicitly supplied.
> No half scale/clipping. Inputs graph24/23/22 are NOT raw MOST L/R/C.
> 245 tests;110 bounded ranges UNKNOWN0; AmpChain default unchanged.
> Next static work: upstream graph inputs and object+97 loading/interpolation,
> then remaining modes/lifecycle and independent execution validation.

> Live check, Sep19 evening EDT: MMX SSH/RCC qnet now reachable. Current
> dio_manager environment confirms LFE_GAIN=0, FRONT_BASS_DB=-3, surround0.40,
> upmix on. On-disk hook674618B SHA2566c0baa3b…; older vehicle table is stale.
> **J525 RAM was not read**: verified read-only MOST/V850→SHARC transport still
> missing; RCC /dev/io-amp is the internal TI DSP, not J525. Do not promise
> C4ADD/BC9EC readback solely because SSH works. See
> [live attempt and next prerequisite](UPMIX_LIVE_READBACK_2026-09-19.md).

> Latest B continuation (AmpChain still v5): fixed two/six/four-channel tails
> and eight-channel finish reconstructed; spectral subtraction, two adaptive
> band masks and temporal/frequency smoothers now have isolated references.
> Six/four branches run for library selectors2/5; other modes clear six lanes
> BEFORE stateful EQ, not proof of permanent silence. Selector≠MMI mode label.
> B lanes6/7 still join mixer slots8/9 at row1 −99 dB; six others at0 dB.
> Four-lane addition has no averaging: coherent unity test gives +6.02 dB,
> NOT a measured live boost or established bass cause. 12F87B correctly uses
> /10, unlike C4D17 /20; never apply one conversion globally.
> Real FFT packing/scales, sine window and overlap now have a reference
> (`amp_b_transform.py`): neutral processing is unity delayed512 samples.
> Complex FFT is idealized; no instruction-level/hardware equivalence claim.
> `amp_b_spatial.py` now composes 130771's finite successful path with explicit
> state/parameters/selectors: statistics/coherence, minimum vs fixed mean,
> complex separator12FFBA, diffuse mask1304E4, residual/gate/blend. Separator
> lambda uses Re(cross)^2, NOT |cross|^2; object+10/+11≠parameter p10/p11.
> `amp_b_pipeline.py` now composes spectral wrappers and fixed mode2 from two
> prepared PCM lanes to eight B outputs: pre→four-tail, post→six-tail,
> main→two-tail. Post starts from saved original spectra, not main-filtered
> audio; magnitude smoothing covers only first256/513 bins. Explicit state/
> parameters required. Normalization correction: 1317A2's supplied pointer is
> overwritten power scratch, NOT a precomputed-power bypass. Old API/test claim
> withdrawn; baseline reports never used that option.
> Activation CORRECTION: previous BC9EC polarity was reversed. In state3,
> BC9EC==1 clears output; BC9EC!=1 calls library. Boot0 permits processing,
> NOT evidence of B silence. Live B=0 remains unproven; ready/errors still apply.
> Alternate graph handler124D64 copies L/R to four output pairs, not mute.
> Host20001 selects process124DF3;20003 selects copy124D64. Bulk queue C28A8
> arg1 sends events0..53: event5 bypass THEN event44 process. Direct caller
> C2658 reaches it with1; no live delivery/completion/Bank association proof.
> Library parameter16/object+1128 initializes2; V850 table6EAA0 maps indices
> 0/1/2→2/5/5. Keep active handler, callback gate and algorithm selector separate.
> Next: configuration phase/source and later handler changes; live readback
> must include graph trace saved-handler indirection (1292D8), not just C4ADD.
> Earlier composition milestone:237 tests,109 bounded ranges. Default model
> unchanged, independent composition error 7.30e−15; no car/production changes.
> Next: validate composition/spatial/complex FFT against execution; mode5 input
> preparation is now covered above, other dispatcher/lifecycle paths remain; obtain live
> selector/parameter objects, band map B9097/count B9098 and histories.
> No integration into default AmpChain; no live bass diagnosis established.
> Details: [remaining blocks](SHARC_REMAINING_BLOCKS_2026-09-19.md).
> Earlier status blocks below are historical; v5 `/20` correction remains valid.

> Latest v5 correction: C4D17 uses `10^(code/20)` (instruction constant 0.05),
> NOT v3/v4's erroneous `/10`. Withdraw earlier synthetic target numbers and
> 24/20-dB ceilings; corrected profile0/1 ceilings are 15/10 dB, not measured
> boosts. Both synthetic reports regenerated. Added startup/mask/calibration
> sequencer + noise-core references and first-B filter/guard/ramp kernels.
> Upstream graph11 scalar is +6 dB in loader; API takes audio AFTER it.
> 115 tests pass; default identity-path numbers unchanged. Remaining: live
> state/source/scale, hardware validation, calibration audio composition,
> B spectral/mixing callees. No vehicle or production changes. See
> [remaining blocks](SHARC_REMAINING_BLOCKS_2026-09-19.md).

> Latest continuation, v4: C4D17 fixed full-mode reference is composed and
> optional in AmpChain (explicit auxiliary required); default remains identity.
> Recovered 64-frame scheduler, 4-frame **prefix** detector selection, 16-block
> power history and separate residual/music-sum scratch binding. Stage6 is a
> weighted input sum, NOT ten audio pre-gains. Actual-FIR synthetic scenarios
> show zero auxiliary is not bypass for profile0; these are NOT live gain
> measurements. 99 tests pass. Mode2/startup, graph11 source/scale, hardware
> equivalence and B algorithms remain open. No vehicle/production changes.
> Details and reproducible commands: [remaining blocks](SHARC_REMAINING_BLOCKS_2026-09-19.md).
> Older status blocks below are retained as history, not the latest model state.

> Latest reverse-engineering follow-up: [remaining DSP blocks](SHARC_REMAINING_BLOCKS_2026-09-19.md).
> Model v3 adds decoded post-EQ binding and explicit-runtime gain/limiter paths;
> C4D17 five-band kernel is isolated/tested, but full C4D17 and first B library
> remain unresolved. No deploy or production level changes. Do not call the
> amplifier fully modelled or treat limiter mode 0 as bypass.

> Latest continuation: C4D17 target math/stages19..26, 1023-sample stereo delay,
> and canonical adaptive-FIR residual kernel now have tests. Startup signatures
> match and both enabling V850 flags initialize to A5; do not assume adaptation
> off. First-B library dispatch and transient-mute logic decoded, algorithm
> callees still open. AmpChain remains v3 with explicit identity/B=0 hypotheses,
> not a completed whole-DSP render. See the linked audit for details/readback list.

> Model update later on 19.09: the old absolute speaker levels below omitted
> the second output mixer. It is now implemented together with pre-EQ delays,
> verified-unity EQ banks and the input-matrix floor. Use
> [SOUNDAMP_FULL_DSP_MODEL.md](SOUNDAMP_FULL_DSP_MODEL.md) and
> [the v2 report](../output/upmix-bank1-model-v2-20260919/report.json) for current
> numbers. The vehicle/build state in this historical handoff was not refreshed.

Prepared for Luka's next agent. Private paths, **no passwords** (SSH secrets live in
`../../audi_ssh.sh`, never print them). Supersedes the sound/amp parts of
[UPMIX_AGENT_HANDOFF_2026-09-17.md](UPMIX_AGENT_HANDOFF_2026-09-17.md); that file
still holds the training/NAS side.

## Your assignment

Independently review **what the J525 amplifier ("AMP") does to our 5.1 stream and
what the Virtual Cabin upmix actually sounds like**. Verify, do not trust: the
2026-09-18 build shipped a +9.5 dB subwoofer change nobody announced, and the
6-bus cabin model gave a wrong bass diagnosis for a day. Read-only first, then
concrete findings, then a bounded next step. Vehicle restarts, factory parameters
and pushes need Luka's word each time.

Standing rules: never reboot/restart the unit or `j9` yourself (deploying files is
fine); no `Co-Authored-By`/Claude trailers in commits (Luka is sole author); the
working tree is shared with another agent (their uncommitted WIP: `docs/AUDIO_5_1_STATE.md`,
`docs/INTERNAL_AMP_*.md`, altscreen/bus/Java files) — stage only your own files and
build deploy artifacts from a clean `git worktree`.

## 1. What plays in the car right now

| item | on the unit (`/mnt/app/root/hooks`) | built, waiting for deploy (`build/`) |
|---|---|---|
| hook | `libcarplay_hook.so` 672 060 B, sha `6882c720…` (build 18.09, ccb09c6/65e4f47) | 674 610 B, sha `5226c5ee…` (also: cluster root URLs per RGI state, zone safeArea 360×300; see `docs/reference/RGI_THIRD_PARTY_NAV_APPS.md`) |
| sub level | LFE assist **12288 = +9.5 dB** (the boom) | code default 4096 + `AUDIO_STEREO_LFE_GAIN=0` |
| front shelf | none | `AUDIO_STEREO_FRONT_BASS_DB=-3` (RBJ low shelf 200 Hz on L/R after transfer) |
| QSA write crash | present (dio_manager SIGSEGV on a call during music) | fixed (f9c4a65, `spa_stock_write_result`) |
| rear | 0.40 (−8 dB), 12 ms delay, +3 dB HF shelf 4 kHz, −4 dB low shelf 500 Hz before transfer | same |
| model | `hssr_model.bin` (q6d MoisesDB control) + `hssr_guard.bin` (V11) beside it | same |

Deploy recipe (unit was unreachable all afternoon 19.09, retry):
`root@100.127.17.86` via Tailscale, password from `../../audi_ssh.sh`
(`eval "$(grep '^PASS=' /Users/luka/Desktop/AUDI_2/audi_ssh.sh | sed 's/^PASS=/export SSHPASS=/')"`),
ControlMaster socket `/tmp/audi-cm/sock`; no scp/tar/cksum on the unit: `mount -uw /mnt/app`,
`cat > file.new` then `mv`, read back and `cmp`. Targets: `build/libcarplay_hook.so`,
`build/libcarplay_apac.so` (pair), `deploy/smartphone_integrator/carplay_startup.sh`
(fetch the unit's copy first, patch only the env lines if it differs). No restart:
env is read per CarPlay generation, i.e. at the next phone connect. Traps: memory note `audi_ssh_transport_traps.md` (`~/.claude/projects/-Users-luka-Desktop-AUDI-2-Patches-mhi2-carplay/memory/`).

Luka's listening feedback so far: 17.09 «шикарно, яркие объекты сзади»; 19.09
«чуть меньше ВАУ, но явно слышно сзади в высоких»; bass so strong the MMI Bass had
to go to −30 %; Luka wants bass guitar to stay audible behind, does **not** want to
touch the MMI Subwoofer knob.

## 2. The chain, end to end

```
iPhone stereo PCM ─► hook (buffered type-103 render, Sound Check, stereo upmix HSSR-universal,
  scene: rear delay 576 fr / 0.40 / shelves, LFE V2 = mono LR4 60 Hz × lfe_gain_q15,
  front shelf, int16 rail limiters) ─► 6-ch PCM L R C LFE Ls Rs (C deliberately 0)
  ─► MOST sink 0x26 ─► J525 bank 1 (5.1), profile 0, MMI 3D OFF = pair-mixer row 1
  ─► 8×8 matrix ─► 8-bus EQ (55 biquads) ─► pair mixer (A: 0/0/+9/+9/+4/+4/−6/−6/mute/mute)
  ─► complex block 0xC4D17 (UNDECODED) ─► 10→16 selector ─► 143 output biquads
  ─► delays 0…70 samples ─► 16 power stages ─► 19 drivers
```

Historical claims below — **not a current fact list**. Matrix-dependent routing
and bass conclusions are withdrawn; use [corrected results](UPMIX_MATRIX_RERENDER_2026-09-20.md).
The original list was derived from the
V850 CodeFlash `Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin`:

- **Sub (R211) gets only our LFE** (matrix LFE→LFE +3 dB, bus LP 66 / HP 33 Hz). Stereo sent
  as 5.1 without LFE = silent sub. With `AUDIO_STEREO_LFE_GAIN=0` the modelled sub sits at
  −14 dB re input L in 20–60 Hz, at +9.5 → −4.5 dB; front woofers −17 (20–60) / −11 (60–120).
- **Front woofers R21/R23** sum L −2, Ls −3, C −8, LFE −8 into HP 62 / LP 240 (4th-order).
- **Rear doors** get Ls/Rs only (+10 dB net, notches −8 dB @113/138/174, HP 100 Hz 3rd order,
  +3.6 dB at 8 kHz). Our rear carries almost no bass (−42 dB), so “rear HP” is moot.
- **Centre mid/tweeter and front mid/treble carry no bass** (HP 250/255 Hz). The amp builds the
  centre from L+R (−6 dB, then +9): mono content plays on the centre mid at −3 dB re input,
  louder than each front mid (−6.4). Vocal focus is the amp's, not ours.
- A-pillar, dashboard and “effect” speakers are **silent in 3D OFF** (buses 8/9 muted,
  matrix rows 7/8 −100).
- Front mid/treble delayed 67/70 samples, dash 59/66, centre tweeter 17, rest 0.
- Biquad math is exact: peak Q = q_code/10; crossovers ζ = q/10 or the ζ table; type 27 =
  1st-order high shelf. Reproduce with `tools/soundamp-virtual-cabin/v850_biquad_trace.py`
  (recipe in its docstring; Ghidra's decompile had the soft-float operand order wrong).
- Stereo bank 0 feeds **no** centre and **no** sub from L/R: stock stereo derives them in the
  complex block, so the model cannot compare stock vs ours there.

Unknowns, flagged in code and doc: complex block 0xC4D17 (always fully scheduled during
playback; modelled as identity), pair-mixer B path from node 0x124DF3 (assumed silent in
3D OFF although row 1 stores B = 0 dB for buses 0/1/4/5/6/7), profile 0 assumed, speaker
labels = audit candidates not a proven T38 pinout, no acoustics/volume/tone stages.

## 3. Tools and how to run them

```sh
# host renderer (bit-exact with the hook's S16 path)
cc -std=gnu99 -O2 -Ihook -Ihook/audio tools/stereo-upmix-render/stereo_upmix_render.c \
   hook/audio/stereo_upmix.c hook/audio/hssr_dsp.c hook/audio/hssr_neon.c \
   hook/audio/hssr_control_worker.c hook/audio/soundcheck.c -lm -lpthread -o /tmp/stereo_upmix_render
/tmp/stereo_upmix_render --mode hssr-universal --model build/hssr_model.bin --guard-model build/hssr_guard.bin \
   --lfe assist --lfe-gain 0 --surround 0.40 --front-bass -3 IN.wav OUT6.wav

python3 tools/soundamp-virtual-cabin/amp_dsp_chain.py            # self-check + static per-speaker table
python3 tools/soundamp-virtual-cabin/amp_dsp_chain.py OUT6.wav   # per-speaker bands of one render
python3 tools/soundamp-virtual-cabin/amp_music_report.py --renderer /tmp/stereo_upmix_render \
   --model build/hssr_model.bin --guard build/hssr_guard.bin --out /tmp/rep \
   --variant fb3:0.40:-3:0 --variant lfe95:0.40:-3:9.5 tests/fixtures/private-audio/*.wav
python3 tools/stereo-upmix-render/cabin_model.py ...              # older 6-bus (pre output-EQ) view, superseded
bash scripts/test_hook_audio.sh                                   # host tests (stereo_upmix_test etc.)
bash scripts/build_hook.sh                                        # Docker qnx65-armv7-toolchain, from a clean worktree
HOOK_AUDIO=0 bash scripts/build_hook.sh                           # hook WITHOUT the type-103 audio module (stock audio, no libcarplay_apac.so)
```

Excerpts used on 19.09: nine 60 s 48 kHz cuts (Archers, Bellingham, Boost Up, Crazy Train,
Deep Down Low, Heart-Shaped Box, The Blue-Eyed Devil, UP UP UP, take your vibes and go)
from `tests/fixtures/private-audio` heldout. Result table in
`docs/SOUNDAMP_FULL_DSP_MODEL.md` §“Music through the chain”.

Env knobs (all in `deploy/smartphone_integrator/carplay_startup.sh`): `AUDIO_STEREO_UPMIX`,
`AUDIO_STEREO_SURROUND` (0…0.5, our rear gain), `AUDIO_STEREO_LFE` (off|assist),
`AUDIO_STEREO_LFE_GAIN` (dB re 4096, −24…+12), `AUDIO_STEREO_FRONT_BASS_DB` (−12…0),
`AUDIO_HSSR_MODEL`, `AUDIO_HSSR_GUARD_MODEL` (else `hssr_guard.bin` beside the model),
`AUDIO_FLOAT_RENDER` / `AUDIO_CABIN_RENDER` / `AUDIO_QUIET_LFE` (opt-in float paths, little
listened), `AUDIO_SOUNDCHECK*`. Hook log is WARN-only on the unit (`/tmp/carplay_hook.log`).

Amp evidence: `output/r2/soundamp/output_binding_audit.json` (records, gains, delays, routing),
`output/r2/soundamp/*_evidence.md`, `docs/SHARC_*.md`, `docs/UPMIX_STATIC_READINESS_AND_SOUNDCHECK.md`
(complex block coverage), Ghidra project `Firmwares/SoundAmp/AMP_SHARC_MATRIX.gpr`, radare2 works
for V850 (`r2 -a v850 -b 32`), `tools/inspect_amp_visa.py` for SHARC VISA.

## 4. What to check, in order

1. **Deploy the waiting build** (hook pair + startup script) when the unit answers, then have
   Luka listen at MMI Bass 0 / Subwoofer 0: boom (20–60 Hz) should be gone, punch (60–250)
   3 dB lower. If the sub is now too shy, `AUDIO_STEREO_LFE_GAIN=+3` steps; reconnect phone.
2. **Measure in the car** instead of modelling: phone mic at the driver's seat, same track,
   same volume, stock stereo (`AUDIO_STEREO_UPMIX=off` → 2-ch sink) vs ours. Spectra
   20–20 kHz give the stock sub/centre/rear levels the model cannot produce. Prepare the
   analysis script before the drive so the car session is just recording.
3. **Rear “WOW” headroom**: the amp already adds +10 dB to Ls/Rs; our rear is only ~3 dB below
   the front electrically in 500–8 kHz. 0.40 → 0.50 hits the int16 rail (rear bus 0 dBFS).
   The honest path is `AUDIO_FLOAT_RENDER` (one final S16 conversion, −1 dBFS linked rail);
   it is opt-in and barely listened — audit it before proposing it.
4. **Centre**: consider a vocal head in the protection mask (C = vocal, subtracted from L/R)
   for focus; model work, not a knob.
5. **Complex block / B path**: only if you can decode 0xC4D17 at level 0 or 0x124DF3's outputs.
   Until then keep them flagged; do not silently upgrade the identity assumption.
6. **Level discipline**: every level lives in env + startup script now. Never move a default
   inside a feature commit without a line in `docs/UPMIX_Q4_SECOND_SCENE_STATUS.md`.

## 5. Uncommitted state you inherit (19.09 15:10)

Mine, not yet committed (Luka commits on request): `hook/audio/{audio_hook.c,h,stereo_upmix.c,h}`,
`tests/stereo_upmix_test.c`, `tools/stereo-upmix-render/stereo_upmix_render.c`,
`deploy/smartphone_integrator/carplay_startup.sh`, `docs/UPMIX_Q4_SECOND_SCENE_STATUS.md`,
new: `docs/SOUNDAMP_FULL_DSP_MODEL.md`, this file, `tools/soundamp-virtual-cabin/{amp_dsp_chain,
amp_music_report,v850_biquad_trace}.py`, `tools/stereo-upmix-render/cabin_model.py`.
Not mine: `docs/AUDIO_5_1_STATE.md`, `docs/INTERNAL_AMP_*.md`, `install_MoreIncredibleBash/`,
`output/carplay-video-re-*`, `output/r2/mh2p-*`. Known pre-existing host failure:
`float_render_test` (`output[0] > 32767`, since 75c308f) — not touched.
