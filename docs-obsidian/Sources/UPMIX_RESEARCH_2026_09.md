# Современное разделение источников для Audi upmix

## Вывод и область применимости

Обзор охватывает доступные публикации, препринты и официальные реализации по состоянию на 12 сентября 2026 года. Задача — получить естественную заднюю музыкальную сцену, сохранив lead, основные мелодические партии и басовую основу спереди. Это не эквивалент обычного разделения vocals/drums/bass/other и не соревнование по одному SDR.

Оптимальный следующий шаг — не назначить единственного нового «лучшего учителя», а проверить несколько специалистов на одинаковых исходниках и построить компактный student с полноценным стереовходом, детальным выходным спектром и отдельной оценкой музыкальной роли. VocalRep — содержательно интересный кандидат для lead/harmony, но не универсальный инструментальный сепаратор и не доказанный победитель для этого апмикса. Для инструментального разделения остаются актуальны BS/Mel-Band RoFormer, SCNet и query-based подходы; для встроенного исполнения нужен самостоятельный streaming-дизайн.

Далее явно разделены опубликованные результаты, выполненные проектные эксперименты и предлагаемые изменения. Ни одна новая внешняя модель в рамках этого обзора не обучалась, не проходила локальный сравнительный inference и не устанавливалась в автомобиль. Описанная архитектура — рекомендация для пилота, не выбранный production-кандидат.

## 1. Что уже проверено в проекте

Исторические испытания нельзя подменять повторным чтением названий моделей. В проекте уже использованы следующие конкретные компоненты:

| Компонент | Реально проверенная функция | Что из этого не следует |
|---|---|---|
| BS-RoFormer `model_bs_roformer_ep_317_sdr_12.9755.ckpt` | All-vocals/instrumental teacher; native-rate подготовка с общим gain | Не разделяет автоматически lead и бэк; число в имени checkpoint не является нашим результатом |
| `htdemucs_6s` | Vocals, drums, bass, guitar, piano, other | Guitar не означает вторичную гитару; piano не гарантированно чистый |
| `MDX23C-DrumSep-aufr33-jarredou.ckpt` | Kick/snare/toms/HH/ride/crash из Demucs drums | Ошибки первого разделения остаются возможны во втором |
| Karaoke Mel-RoFormer `...aufr33_viperx_sdr_10.1956.ckpt` | Lead estimate; остаток относительно all-vocals; проверены vocal-only и original-mix входы | Остаток не равен подтверждённому backing vocal |
| MIT AST и LAION CLAP | Независимые описания содержимого, тихие/сомнительные stems, контроли | Не валидированные судьи чистоты или музыкальной роли |

Источники идентичности: [подготовка специалистов](../../tools/stereo-upmix-render/prepare_specialist_teacher_audit.py), [native teacher](../../tools/stereo-upmix-render/prepare_hssr_native_teacher.py), [V12, воспроизводимость и SHA](UPMIX_CABIN_V12.md).

Первый подробный аудит шести фрагментов обнаружил чувствительность karaoke к входному контексту. На *Us V Them* direct-mix увеличивал RMS lead на 11,50 dB относительно vocal-only, но это не доказывало правильность нового разделения. На *Bellingham (Slowed)* смена входа почти не помогала. На *Cities In Dust* остаток включал wordless vocal sample; его принадлежность вокалу не решает, является ли он фоновым или главным хуком. Эти результаты относятся к историческим исходникам, а не автоматически ко всем новым FLOAT-версиям. [Аудит V14](UPMIX_TEACHER_CONTENT_AUDIT_V14.md).

Затем FLOAT-корпус был пересобран: **50 песен, все 587 фрагментов, 194,67 уникальной минуты**. Для каждого выполнены base teacher, Demucs/DrumSep, обе karaoke-ветки и AST/CLAP-аудит. Терминальный статус — `complete-needs-semantic-review`: техническая обработка завершена, семантическое одобрение всех результатов отсутствует. Specialist WAV сохранены для 544 фрагментов; у исторических 43 остались отчёты, но не эти WAV. Нельзя объявлять весь проход незавершённым из-за этого пробела или, наоборот, весь корпус чистым. [Полный FLOAT-аудит V15](UPMIX_FULL_FLOAT_DATASET_V15.md).

Предыдущие студентские модели решали другую задачу. V11 улучшал instrumental/vocal selectivity относительно V9, но исторические отдельные stem-прогоны не равны точной атрибуции внутри исходной смеси. V12 fine-tunes не оправдали замену V11. V15 замораживал семантические головы и обучал только routing/pan: он не мог заново научиться полноценному разделению инструментов. [V11](../../tools/stereo-upmix-render/bench/results/2026-09-07-hssr-selection-v11.md), [V12](UPMIX_CABIN_V12.md), [V15 training](UPMIX_V15_STREAMING_TRAINING.md).

Позднее MoisesDB дал реальные исходные tracks: 240 песен, artist-disjoint разбиение 181/40/19. E усиливал защиту, но сильно подавлял backing. F сравнил одинаковые модели с грубыми и более подробными спектральными входами. Обе F завершили 12 эпох и были отклонены: максимальный цифровой lead transfer 18,1715%/17,9289%. На *Trail Away* у spectral-варианта инструментальная ветка пропускала 17,6642%, backing-ветка — 0,01359%. **Исправление только вокального учителя не устраняет этот инструментальный обход защиты.** Это измерения fixed-side proxy, не SPL в салоне. [Moises capacity](UPMIX_V16_MOISES_CAPACITY.md), [F: завершение и отказ](UPMIX_V16_SCENE_F.md).

G пока является необученным source-estimation прототипом. MSE-oracle прослушивания — тоже не результаты обученной G. Жалоба на «пожёванность» 32-полосных вариантов требует сравнения raw Side, coarse, coarse+smoothing и fine при сопоставимой утечке и сохранности полезного материала. Источник артефактов пока не изолирован. [G preflight](UPMIX_V16_SOURCE_G_PREFLIGHT.md).

## 2. VocalRep: специалист, а не весь апмикс

VocalRep, ACL Findings, июль 2026, получает три группы: lead, harmony/backing и общее instrumental accompaniment. Вокальная идентичность и структурные ограничения используются для устойчивости ролей во времени. В статье на PHV-40 harmony SI-SDR составляет 2,87 dB против 1,65 у переобученного SCNet Large и 0,78 у переобученного Band-Roformer. Однако lead SI-SDR 7,99 уступает 8,08 у Band-Roformer; суммарный vocals на MUSDB18-HQ — 11,45 против 11,85. Нет победы по всем показателям. Сравнение PHV-40 использует трёхголовые baseline-модели, переобученные авторами на их 250-часовом in-house наборе, а не наш karaoke checkpoint. [VocalRep, §4–5, таблицы 1–2](https://aclanthology.org/2026.findings-acl.1785.pdf).

Практическое следствие: VocalRep заслуживает проверки как независимый lead/backing эксперт. Он не выдаёт отдельные HH, вторичные гитары, pads или пространственные хвосты. Его instrumental — общий музыкальный остаток, а не разрешение отправить всё назад. Улучшение вокальной структуры потенциально помогает очищать instrumental от голоса, но этот эффект необходимо измерить на наших фрагментах.

Официальный repository уже существует: заявлены basic inference и Git-LFS checkpoint, включая speaker-guided двухпроходную обработку. Авторы одновременно предупреждают об экспериментальном состоянии реализации; README явно указывает отсутствие открытой лицензии и отсутствие разрешения использовать код без неё. Доступность файла и права на использование — разные проверки. Частный контекст не принимается здесь за подтверждённое юридическое разрешение; техническая оценка качества от этого не прекращается. Фактическая загрузка LFS-объекта и воспроизведение статьи локально не проверены. [Официальный VocalRep repository](https://github.com/Zhenqiang-Weng/VocalRep).

Есть и принципиальное ограничение идеи speaker identity: один певец может записать одновременно lead, doubles и настоящий бэк; два разных певца могут быть co-lead. Поэтому «тот же голос» не равно «главная партия», а «другой голос» не равно «фон». Для автомобильной политики нужны отдельные примеры именно музыкальных ролей, включая wordless hook, duet, унисон и lead reverb.

## 3. Сильные современные решения для offline-уровня

### Детальные комплексные маски

BS-RoFormer использует band-split признаки и RoPE-transformer, но его decoder выдаёт **комплексную маску каждого FFT-бина внутри полосы**, а не один коэффициент на всю полосу. Поэтому он может разделять детали, которые наш scalar Side controller вынужден менять вместе. Это важнее самого слова Transformer. Исходная сильная конфигурация использует крупные вычислительные ресурсы и дополнительный закрытый training set; архитектура, опубликованный score и конкретные community weights должны учитываться раздельно. [Lu et al., BS-RoFormer, §3.3–4](https://arxiv.org/html/2309.02612v2).

Mel-Band RoFormer заменяет неперекрывающиеся heuristic bands перекрывающимися mel-полосами. В контролируемом сравнении авторов улучшались vocals, drums и other, но увеличение глубины не давало универсального выигрыша. Это кандидат для качественного offline teacher, не аргумент в пользу грубых mel-gains на выходе и не основание заменить уже проверенный teacher без A/B. [Wang, Lu, Won, Mel-Band RoFormer](https://arxiv.org/html/2310.01809v1).

SCNet — альтернативное семейство sparse-compression encoder/decoder и sequence modeling. Оно полезно как второй, архитектурно отличающийся инструментальный baseline: согласие двух близких RoFormer checkpoints менее информативно, чем согласие моделей с различными ошибками. Официальная реализация доступна; заявленное качество конкретного XL/Large checkpoint всё равно следует воспроизводить отдельно. [Tong et al., SCNet](https://arxiv.org/html/2401.13276v1), [официальный repository](https://github.com/starrytong/SCNet).

### Адаптивное сжатие и воспроизводимое обучение

SFC, февраль 2026, исследует input-adaptive спектральное сжатие через cross-attention или Mamba вместо отдельных фиксированных band encoders. Полезна сама постановка: экономить latent representation, не обеднять восстанавливаемый спектр. Есть положительная ablation и с GRU-backbone. Но основные модели используют миллионы параметров и offline temporal processing; адаптивный frequency encoder не доказывает пригодность всей сети к QNX real time. Частотная двунаправленность внутри текущего кадра не равна чтению будущего во времени. [Saijo, Bando, SFC](https://arxiv.org/html/2602.08671v1).

Replication study BSRNN, март 2026, показала, насколько результат зависит от всего pipeline: авторы не воспроизвели исходный score буквально и получили улучшенный вариант через отдельные изменения, выпустив код и веса. Для нового пилота это предпочтительнее незафиксированного набора «похожих слоёв». Репликация не превращает offline BSRNN в causal-модель: временную архитектуру и метрики надо заново проверять после адаптации. [Magron et al.](https://arxiv.org/html/2603.09187v1), [код исследования](https://github.com/magronp/bsrnn).

MSST, препринт июля 2026, унифицирует конфигурации training/evaluation/inference, несколько архитектур, losses и способы fine-tuning. Его практическая ценность для проекта — фиксированный общий сравнительный стенд. Overlap inference, TTA и ensemble могут повышать offline качество ценой вычислений; это не операции, которые автоматически можно перенести в streaming hook. [Solovyev et al., MSST](https://arxiv.org/html/2607.23395v1), [repository](https://github.com/ZFTurbo/Music-Source-Separation-Training).

### За пределами четырёх инструментальных stems

Banquet использует один query-conditioned decoder и оценивался на MoisesDB; работа поддерживает finer-grained инструменты за пределами VDBO. Последующее исследование hyperellipsoidal queries допускает запросы на группы источников. Это более подходящее направление для разнообразных guitars/pads, чем трактовка всего `other` как ambience. Но класс инструмента и его важность в аранжировке остаются разными задачами: query «guitar» не отделяет автоматически accompaniment от главного риффа. [Watcharasupat, Lerch, Banquet, 2024](https://arxiv.org/abs/2406.18747), [query-by-region, 2025](https://arxiv.org/abs/2501.16171), [авторский код](https://github.com/kwatcharasupat/query-bandit).

### Генеративное восстановление

SAM Audio, декабрь 2025, — promptable separator с генеративной flow-matching архитектурой; доступны текстовые, временные и визуальные подсказки. Это интересный offline эксперт на неоднозначных источниках, а не встроенный ARM student. Официальный inference рекомендует CUDA, веса требуют доступа, а дополнительные варианты/reranking увеличивают стоимость. [Meta, SAM Audio](https://arxiv.org/abs/2512.18099), [официальный код](https://github.com/facebookresearch/sam-audio).

DiCoSe в редакции апреля 2026 добавляет diffusion/consistency refinement поверх deterministic separator; авторы показывают улучшения и сокращение refinement до одного шага. Это реальный современный ответ на задачу восстановления деталей, а не просто маскирования. Однако один шаг остаётся дополнительной нейросетью, и естественность восстановленного тембра не гарантирует верность исходной партии. Для faithful upmix такой подход следует держать отдельным offline экспериментом, не делать его безусловной ground truth. [Karchkhadze et al., v2](https://arxiv.org/html/2412.06965v2).

## 4. Что действительно работает с малой задержкой

Следующие числа — заявления соответствующих авторов, не наш benchmark. SDR/cSDR, training data, измерительная платформа и latency conventions отличаются; таблица не является единым leaderboard.

| Работа | Размер и опубликованный результат | Задержка и платформа | Значение для проекта |
|---|---|---|---|
| RT-STT, 2025 | 383 тыс. параметров; mean cSDR 5,17 dB | 23 ms algorithmic; 1,01 ms inference FP16 на RTX 3080 Ti | Компактный causal design; GPU timing не является ARM timing |
| Band-SCNet, Interspeech 2025 | 2,59 млн; mean SDR 7,79 dB; 17,59 GFLOP/s | 92 ms; RTF 0,478 на одном потоке i7-13700H | Качественный causal ориентир, но большой budget относительно текущего прототипа |
| Pi 4 engineering prototype, 4 сентября 2026 | 0,86 млн; авторский mean четырёх stem SDR 3,79 dB | 186 ms lookahead, 279 ms end-to-end; ~15,5 ms на блок 23,2 ms, Cortex-A72 | CPU-only feasibility example, не подтверждение достаточного качества или переноса на Krait |
| Moises-Light, 2025 | Около 5 млн **на stem**, таблица указывает ×4; mean cSDR 9,96 dB | Offline chunks 9 s; low-latency ARM не доказан | Размер одной модели нельзя путать с общей стоимостью четырёх |

Источники таблицы: [RT-STT](https://arxiv.org/html/2511.13146v1), [Band-SCNet, таблица 1 и §4.2](https://www.isca-archive.org/interspeech_2025/yang25d_interspeech.pdf), [Brian Matzelle, Pi 4 self-report](https://www.matzelle.co/blog/2026-09-04-four-stem-separation-on-a-raspberry-pi), [Moises-Light](https://arxiv.org/html/2510.06785v1).

RT-STT обрезает спектр до 384 бинов при FFT1024/44,1 kHz — примерно до 16,5 kHz. Moises-Light использует 2048 бинов из FFT6144/44,1 kHz — примерно до 14,7 kHz. Эти верхние частоты рассчитаны из конфигураций статей. Такую экономию нельзя незаметно переносить в проект, где важны HH, cymbals и верхние хвосты. Нужен явно определённый full-band путь, иначе «воздух» исчезнет по конструкции. FP16/TensorRT-ускорение RT-STT также не означает аналогичного ускорения int8 на нашей машине. [RT-STT, §III–IV](https://arxiv.org/html/2511.13146v1), [Moises-Light, §4](https://arxiv.org/html/2510.06785v1).

Сентябрьский Pi-пример особенно показателен: работа на ARM достигнута ценой существенной задержки и ограниченного качества. В статье автора residual/complement formulation отдельно связывается с артефактами изолированных stems. Это аргумент проверять student на том выходе, который реально используется, а не только на хорошем sounding accompaniment. Результат одного инженерного blog-post не заменяет peer review и наши аппаратные измерения. [Pi prototype](https://www.matzelle.co/blog/2026-09-04-four-stem-separation-on-a-raspberry-pi).

## 5. Предлагаемая архитектура G

Рекомендация: сохранить направление full complex L/R, но не принимать текущую маленькую G за уже достаточную архитектуру. В ней FFT512/48 kHz даёт 93,75 Hz между бинами, после encoder остаются 65 frequency groups, а выходные masks линейно интерполируются до 257 бинов. Это существенно лучше side-only доступности источников, но не независимый полноразрешающий decoder. Текущий loss — balanced complex MSE, без законченного waveform/multi-resolution/perceptual objective. [Исходник SourceHead](../tools/stereo-upmix-render/hssr_source_model.py).

Пилот должен проверить несколько строго ограниченных вариантов, а не сразу заново считать весь корпус. Базовый кандидат — causal band-split recurrent separator с сохранением стереоконтекста, shared temporal processing и decoder, который восстанавливает отдельные комплексные бины. Второй кандидат — компактный causal convolutional encoder/decoder с recurrent bottleneck по мотивам low-latency исследований. Третий, только если первые показывают ограничение encoder, — адаптивное frequency compression по мотивам SFC.

Для сравнения нужны одинаковые source targets, duration, split, gain augmentation и runtime accounting. FFT512/1024/2048 следует сравнивать как разные time/frequency compromises, с отдельным учётом WOLA latency и атаки. Более длинное прошлое доступно causal-модели, но нельзя скрыто менять past-only на centered STFT или full-song normalization. Любой lookahead должен иметь явный budget и согласованную задержку всех передних/задних путей, навигации и взаимодействий с CarPlay.

Вычисление сложных семантических признаков не обязано происходить на каждом FFT-hop. Практичная гипотеза — быстрый source path и более медленный past-context role path с uncertainty. Роль меняется на масштабе фразы, тогда как атака требует быстрого управления. Но замедление роли не должно создавать зависшую rear permission на смене певца, партии, трека или после seek/FLUSH. Эти сценарии являются тестами, не предположениями.

Нельзя объединять «выделено как инструмент» с «разрешено назад». Source estimates должны сначала проходить отдельную musical-role оценку; все задние пути, включая instrumental и ambience, должны участвовать в защите lead. Unknown остаётся защищённым. При неуверенности система уменьшает вмешательство, но полное подавление задней сцены не считается успешным separation.

Complex masking остаётся оценкой источника по смеси, а не точным извлечением скрытой студийной дорожки. Ограниченная маска не восстановит произвольную точную компенсацию источников. Residual mapping или генеративное уточнение можно изучать отдельно, но их дополнительные детали нужно проверять на верность, а не принимать за восстановленный оригинал. Цель — сохранять музыкальное событие, его тембр, атаку и хвост; не «дорисовать убедительный другой инструмент».

## 6. Учителя, данные и losses

MoisesDB содержит fine-grained tracks и полезен для supervised separation; однако metadata вроде guitar family не размечает foreground/secondary в каждой фразе. Доступные реальные stems должны иметь больший доказательный вес, чем автоматически сгенерированные псевдометки. Их overlap, bleed, неизвестные роли и разбиение по артистам необходимо сохранять. [Pereira et al., MoisesDB](https://arxiv.org/abs/2307.15913), [официальное описание](https://github.com/moises-ai/moises-db/blob/main/README.md).

Старые 587 фрагментов не следует снова массово прогонять без конкретного вопроса. Их выгоднее использовать как regression/specialist disagreement set: выбирать кандидатов по уже известным сбоям и существующим исходникам. При сравнении VocalRep с karaoke должны быть одинаковые FLOAT input, native sample rate, контекст, общий gain и выходной evaluation interval. Один громкий backing не означает лучший backing. Одно чистое all-vocals не означает лучшее разделение lead/harmony.

Для новых псевдометок нужен class-specific confidence и явный quarantine. Согласие нескольких моделей — повод повысить приоритет проверки, но не доказательство ground truth; одинаковые training data могут создавать общие ошибки. Песни без референсов пригодны для прослушивания и анализа согласия, а не для точного процента истинного lead leakage. Необходимо отдельно разметить hook/co-lead/double, первичный synth/secondary pad и основную/вторичную guitar.

Рекомендуемый loss-pilot содержит waveform L1, complex-STFT error и multi-resolution спектральные ошибки, а затем отдельные тихие/отсутствующие источники, защиту lead/foundation/unknown и сохранность стереосцены. Это проектное предложение, не готовая «лучшая формула» из одной статьи. Коэффициенты выбираются по ablation; агрессивный loss на тихих bins может раздувать шум, а сглаживание — стирать атаки. Mixture consistency полезна для бухгалтерии источников, но неверные stems тоже могут идеально суммироваться в mix.

Условия использования различаются для кода, checkpoint и данных. MIT у training framework не распространяется автоматически на вложенные веса; MoisesDB официально указан как CC BY-NC-SA 4.0. SAM имеет собственную лицензию, VocalRep публикует отдельное ограничение в README. Этот обзор фиксирует текст источников, не выдаёт юридическое заключение и не приравнивает частное использование к отсутствию условий. [MSST](https://github.com/ZFTurbo/Music-Source-Separation-Training), [MoisesDB](https://github.com/moises-ai/moises-db/blob/main/README.md), [SAM Audio](https://github.com/facebookresearch/sam-audio), [VocalRep](https://github.com/Zhenqiang-Weng/VocalRep).

## 7. Оценка естественности и реальной полезности

Работа *Beyond SDR* исследует изменение транзиентов и динамики на MUSDB18-HQ: высокая корреляция SDR с одной характеристикой не гарантирует сохранность других; длина контекста способна менять форму атаки. Это ранний препринт, не окончательная оценка всех моделей. В метаданных есть конфликт: идентификатор `2609.04224`, но страница указывает 12 июля 2026; точная сентябрьская дата не подтверждается. Он полезен как обоснование дополнительных тестов, а не как повод объявить SCNet безусловно лучше RoFormer. [Ding, Beyond SDR](https://arxiv.org/html/2609.04224v1).

SAM Audio Judge, январь 2026, оценивает precision, recall, faithfulness и overall quality без чистого reference. Это тематически более подходящий дополнительный critic для separated audio, чем AST/CLAP classification. Но его оценки тоже требуют калибровки на музыке проекта и не устанавливают, что wordless hook можно отправить назад. Не следует доверять teacher и его собственному judge как двум независимым доказательствам. [SAM Audio Judge](https://arxiv.org/html/2601.19702v1).

Сравнительная карточка кандидата должна показывать отдельно: утечку защищённых источников, сохранность полезных источников, waveform/phase fidelity, атаки и хвосты, стабильность L/R, фронт после перераспределения и аппаратную стоимость. Нужны median, худшие случаи и распределение по песням; число фрагментов не должно давать длинной песне несоразмерный вес. Отсутствующий backing — задача ложного срабатывания, а не «идеальный SDR» на нуле.

Для mask-based linear paths корректна атрибуция при масках, вычисленных один раз на исходной смеси и затем применённых к известным источникам. Перезапуск нелинейного separator на изолированном lead создаёт другой контекст и не измеряет ту же утечку. После нелинейных rails/limiting требуется отдельная оговорка или фиксация gain-траектории; акустическое прослушивание не заменяется цифровым процентом.

Listening UI должен сравнивать hidden reference и несколько вариантов при общем, явно записанном уровне: чистый target, current Side32, raw Side, сглаженный Side32, fine mask и learned source candidate. Раздельные оценки нужны для голоса, «жевания», пропущенных атак, хвостов и spatial instability. MUSHRA-подобная разработческая процедура полезна, но тест одного слушателя без стандартных условий не должен называться стандартизованным MUSHRA. Для финальных малых артефактов применима логика BS.1116. [ITU-R BS.1534](https://www.itu.int/rec/R-REC-BS.1534/en), [ITU-R BS.1116](https://www.itu.int/rec/R-REC-BS.1116/en).

## 8. Virtual Cabin и последовательность решений

Публикация 2026 года *Preserving Spatial Information in Music Source Separation* рассматривает сохранность пространственных cues, включая ILD, в пространственно модифицированном HTDemucs. Доступное институциональное описание поддерживает необходимость spatial evaluation; полный текст ограничен, поэтому количественных выводов из него здесь нет. Binaural/HRTF evaluation не является измеренной моделью Audi cabin. [Acerbi et al., Politecnico di Milano](https://re.public.polimi.it/handle/11311/1324389).

Для Audi остаются проектные ограничения: 3D OFF не означает обход штатного DSP; Focus All/minimum Surround нуждаются в live confirmation. Rear-door woofer и tweeter используют общий электрический feed: нельзя выделить отдельный PCM-выход «только HH в твиттер». C-pillar surrounds — другие цепи. Известная входная матрица J525 не равна полному акустическому transfer; её нельзя инвертировать как готовую cabin correction. PCM C первоначально нулевой, bass assist сдержанный, Sound Check один раз после float-render, финальные rails связаны. [Cabin routing review](UPMIX_CABIN_ROUTING_REVIEW.md), [G preflight](UPMIX_V16_SOURCE_G_PREFLIGHT.md).

Порядок следующей работы:

1. Закончить и интерпретировать strict protected oracle, не выдавая lead-only optimistic bound за безопасный rear target.
2. Изолировать «пожёванность» контролируемым Side/coarse/smoothing/fine сравнением. Сначала выяснить, что портит waveform, затем менять сеть.
3. На ограниченном общем development-наборе сравнить существующие teachers с VocalRep как вокальным кандидатом и SCNet/другим независимым инструментальным кандидатом. Доступность/условия checkpoint проверяются до запуска; никаких автоматических массовых скачиваний.
4. Проверить два небольших full-L/R source-decoder пилота, включая стерео, phase, attacks и тихие источники. Отдельно измерить реализацию операторов на целевом ARM/QNX.
5. Обучать musical-role policy только на поддержанных метках. Проверять vocal leakage через ВСЕ задние ветки, особенно instrumental, где F уже провалилась.
6. После выбора — полноценный training, C/export parity, continuous songs/reset/FLUSH, независимый heldout и сравнение в автомобиле.

Итоговый выбор должен отвечать не «какая сеть новее», а «какой pipeline сохраняет больше действительно фоновой музыки при меньшем повреждении оригинала и допустимой стоимости». На текущих доказательствах VocalRep — кандидат для одного важного звена; полный переход на него вместо инструментальной системы не обоснован.

## Источники

Ключевые первичные публикации и реализации; даты относятся к статье/указанной редакции, не к дате поискового индексирования.

1. Shen et al. [VocalRep](https://aclanthology.org/2026.findings-acl.1785/), ACL Findings, июль 2026; [официальная реализация](https://github.com/Zhenqiang-Weng/VocalRep), состояние на 12.09.2026.
2. Lu et al. [Music Source Separation with Band-Split RoPE Transformer](https://arxiv.org/abs/2309.02612), 2023, использована v2.
3. Wang, Lu, Won. [Mel-Band RoFormer for Music Source Separation](https://arxiv.org/abs/2310.01809), 2023.
4. Tong et al. [SCNet: Sparse Compression Network for Music Source Separation](https://arxiv.org/abs/2401.13276), 2024; [авторский код](https://github.com/starrytong/SCNet).
5. Saijo, Bando. [Input-Adaptive Spectral Feature Compression by Sequence Modeling for Source Separation](https://arxiv.org/abs/2602.08671), февраль 2026.
6. Magron, Serizel, Douwes. [The Costs of Reproducibility in Music Separation Research](https://arxiv.org/abs/2603.09187), март 2026; [репликация BSRNN](https://github.com/magronp/bsrnn).
7. Solovyev et al. [MSST](https://arxiv.org/abs/2607.23395), июль 2026; [framework](https://github.com/ZFTurbo/Music-Source-Separation-Training).
8. Watcharasupat, Lerch. [Banquet](https://arxiv.org/abs/2406.18747), 2024; [Hyperellipsoidal Queries](https://arxiv.org/abs/2501.16171), 2025; [код](https://github.com/kwatcharasupat/query-bandit).
9. Shi et al. [SAM Audio](https://arxiv.org/abs/2512.18099), декабрь 2025; [Meta repository](https://github.com/facebookresearch/sam-audio).
10. [SAM Audio Judge](https://arxiv.org/abs/2601.19702), январь 2026.
11. Karchkhadze et al. [Improving Music Source Separation with Diffusion and Consistency Refinement](https://arxiv.org/abs/2412.06965), v2, апрель 2026; исходная v1 — декабрь 2024.
12. Wu et al. [Towards Practical Real-Time Low-Latency Music Source Separation](https://arxiv.org/abs/2511.13146), ноябрь 2025.
13. Yang et al. [Band-SCNet](https://www.isca-archive.org/interspeech_2025/yang25d_interspeech.html), Interspeech 2025.
14. Hung, Pereira, Korzeniowski. [Moises-Light](https://arxiv.org/abs/2510.06785), октябрь 2025.
15. Matzelle. [Four-Stem Music Separation, Live, on a Raspberry Pi 4](https://www.matzelle.co/blog/2026-09-04-four-stem-separation-on-a-raspberry-pi), 4 сентября 2026; инженерский self-report, не peer-reviewed paper.
16. Ding. [Beyond SDR](https://arxiv.org/abs/2609.04224), 2026; конфликт точной даты отмечен в тексте.
17. Pereira et al. [MoisesDB](https://arxiv.org/abs/2307.15913), 2023; [официальный dataset repository](https://github.com/moises-ai/moises-db).
18. Acerbi et al. [Preserving Spatial Information in Music Source Separation](https://re.public.polimi.it/handle/11311/1324389), 2026; доступен институциональный abstract.
19. ITU-R. [BS.1534-3](https://www.itu.int/rec/R-REC-BS.1534/en), 2015, актуальный статус проверен; [BS.1116](https://www.itu.int/rec/R-REC-BS.1116/en).
20. Проектные доказательства: [V14 content audit](UPMIX_TEACHER_CONTENT_AUDIT_V14.md), [V15 complete FLOAT audit](UPMIX_FULL_FLOAT_DATASET_V15.md), [V16 F](UPMIX_V16_SCENE_F.md), [G preflight](UPMIX_V16_SOURCE_G_PREFLIGHT.md). Локальные инженерные отчёты, не внешняя независимая валидация.
