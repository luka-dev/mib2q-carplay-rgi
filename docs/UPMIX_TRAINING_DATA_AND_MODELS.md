# Модели и датасеты, использованные для обучения апмикса (провенанс)

Статус на 2026-09-18. Что именно стоит в машине, на чём это обучено, какие нейросети-учителя
размечали данные и откуда каждый источник взят. Лицензионные ограничения указаны как есть.

## 1. Модели, которые едут в машину (`build/`)

| Файл | Что это | Обучено на | Откуда веса |
|---|---|---|---|
| `hssr_model.bin` (q6d, SHA `96abe57d…`) | HSSR-160 control model: 2×GRU-160, 451 464 параметра, 32 полосы × (6 классов + rearability + pan) + 8 глобальных выходов; архитектура `tools/stereo-upmix-render/train_hssr.py` | MoisesDB v0.1, 181 TRAIN / 40 DEV песен, цели с настоящими классами `q6_hssr_moises_cache.py` (0 вокал, 1 бас, 2 барабаны+перкуссия, 3 гитара+щипковые, 4 струнные+прочее, 5 клавиши), rearability по пространственной эвристике V11; дообучение от весов V11, lr 5e-4, лучшая эпоха 3 | обучено нами на NAS (Ryzen, torch 2.10 CPU), 2026-09-18; чекпоинты `runtime/q6d-hssr-moises-train-a/` на NAS; копия в git `build/hssr-q6d-moises-a/` |
| `hssr_guard.bin` (V11, SHA `ab01463e…`) | та же архитектура, «страхующая» модель: её вероятность вокала защищает голос, который q6d путает с клавишами | MUSDB18 (150 треков, 100/50, сжатое издание `*.stem.mp4`; классы vocals/bass/drums/other, из `other` три подкласса эвристикой) + супервизия учителями-разделителями на приватном корпусе (см. `docs/HSSR_DATASET_TRAINING_AUDIT.md`, `tools/stereo-upmix-render/prepare_hssr_*_supervision.py`) | обучено предыдущей сессией 2026-09-07 (`hssr-selection-v11`); копия в git `build/hssr-selection-v11/` |

Обе модели существуют только как control models для DSP в `hook/audio/hssr_dsp.c`; они не разделяют звук
на стемы. По условиям MUSDB18 и MoisesDB (research / non-commercial) веса помечены в `train_hssr.py`
как development-only.

## 2. Датасеты

| Датасет | Состав | Источник и лицензия | Где лежит | Для чего использован |
|---|---|---|---|---|
| **MoisesDB v0.1** (Moises.ai; Pereira et al., 2023, arXiv 2307.15913) | 240 песен, стемы с 12-классовой таксономией (lead/backing/other vocal, bass, drums, high/other percussion, guitar, keys, strings & winds, other plucked, other); наша разбивка 181 TRAIN / 40 DEV / 19 TEST, TEST запечатан и никогда не читался | официальная подписанная выдача, получена Luka 2026-09-11 (репозиторий датасета: github.com/moises-ai/moises-db), SHA-256 архива и всех 2825 файлов сверены; лицензия исследовательская, некоммерческая | NAS `external/moisesdb/moisesdb_v0.1`; рабочий кэш 48 кГц float16 `runtime/q3-cache-a` (221 песни, 86 ГБ) | Q2/Q3/Q4/Q5 эксперименты, обучение q6d, все пробы утечки в этом журнале |
| **MUSDB18** (SigSep) | 150 треков, ~10 ч, vocals/bass/drums/other | официальная раздача SigSep (Zenodo), издание со сжатыми стемами `*.stem.mp4`; research / non-commercial | локальная копия `build/reference-audio/musdb` удалена при чистке; на NAS не проверял | обучение HSSR V11 (ныне страхующая модель) |
| **AlbumDB** (McKenzie, Moliner, Wright; статья AES) | 10 песен одного артиста: мастер-стерео + именованные стемы (`stems_mixed`), 0,57 ч; плюс авторские 5.1 | Zenodo record 19683001, лицензия CC-BY-4.0, версия и лицензия проверяются в `acquire_albumdb.py` | NAS `external/albumdb-19683001-masters-a`, `albumdb-stems-19683001-b`, `albumdb-paper51-48k-a`; кэш `runtime/q4-albumdb-cache-a`; локально `~/Desktop/AUDI_2/upmix-data/q4-albumdb-cache-a` | DEV-цели для маски защиты (Q4), эталон 5.1 |
| **Amvizo showcase** | 20 ремиксов (Bee Gees, Belinda Carlisle, Coldplay, Eurythmics, Evanescence, Kate Bush, Metallica, Michael Jackson, Miley Cyrus, Queen, Simple Minds, Calvin Harris & The Weeknd, Elton John + Dua Lipa, две темы из игр/кино, пять треков TeknoAXE), 1,37 ч; 5.1 FLAC + стерео MP3 | amvizo.com/downloads/music/flac/5.1/ (бесплатная витрина; сами песни — коммерческие ремиксы, только для приватных исследований) | NAS `external/p1-sources-20260913-a/amvizo/{flac,mp3}`; локально `~/Music/AudioInterfaceV1/mp3` | разметка учителями (`runtime/q4-teacher-cache-a`, 20 песен), эталон нативного 5.1 |
| **HiRez.one** | Odyssey «Emphasis» в 4.0 / 5.1 / 7.1 FLAC, The Amnis Initiative «Singularity» | hirez.one/streaming/… (демо-раздача), MD5 декодированного PCM сверены | NAS `external/p1-sources-20260913-a/hirez` | нативные многоканальные эталоны для пилотов P1 |
| **Приватный корпус** | 43 тренировочных + 7 отложенных коммерческих мастеров, MP3 | скачаны через spotdown.org (Spotify); только приватное использование, не публиковать | `tests/fixtures/private-audio/{training,heldout}` (277 МБ в git) | супервизия V11 разделителями, слепое прослушивание; запланированная цепочка B (разметка учителями) не выполнена |
| Ambisonic DML metadata | только метаданные | часть плана `p1-sources-20260913-a` | NAS | не использовалось в обучении |

Локальные пробы этого журнала: три DEV-песни MoisesDB (The Followers «Radio», Skipsam Soo «Joy To The
World», Fröst «Scars On The Lining») и один приватный трек, разложенный htdemucs_6s на 6 стемов.

## 3. Нейросети-учителя (размечали данные, в машину не едут)

| Модель | Файл | Автор / источник | Роль у нас |
|---|---|---|---|
| Demucs v4 `htdemucs_6s` | `5c90dfd2-34c22ccb.th` + `htdemucs_6s.yaml` | Meta AI (MIT); веса через зеркало UVR github.com/TRvlvr/model_repo, оригинал dl.fbaipublicfiles.com/demucs | 6 стемов: vocals, drums, bass, guitar, piano, other |
| BS-RoFormer | `model_bs_roformer_ep_317_sdr_12.9755.ckpt` | viperx; каталог ZFTurbo Music-Source-Separation-Training / UVR | вокал (заменяет vocals от Demucs) |
| Mel-Band RoFormer karaoke | `mel_band_roformer_karaoke_aufr33_viperx_sdr_10.1956.ckpt` | aufr33 + viperx; huggingface.co/jarredou/aufr33-viperx-karaoke-melroformer-model | лид против бэков на вокальном стеме |
| MDX23C DrumSep | `MDX23C-DrumSep-aufr33-jarredou` | aufr33 / jarredou (GitHub releases) | только в ревью учителей V14: kick/snare/toms/hh/ride/crash |
| MIT AST AudioSet + CLAP | `MIT/ast-finetuned-audioset-10-10-0.4593` | HuggingFace | только аудит содержимого стемов, не разметка |

Запуск учителей: пакет `audio-separator`, venv py3.12 + torch 2.14 на Mac (MPS); веса в `~/.cache/upmix-teachers/`.
Скрипт: `tools/stereo-upmix-render/q4_teachers.py`.

## 4. Что рассматривалось, но не использовалось

- CUpGAN-A (AES): архитектура известна, веса и код не публичны — `docs/CUPGAN_A_FEASIBILITY.md`.
- VocalRep, Banquet и другие работы из обзора `docs/UPMIX_RESEARCH_2026_09.md` — только литература.
- Ableton/GarageBand-синтез стемов: отложен, MoisesDB даёт настоящие клавиши в настоящих миксах.

## 5. Где лежат обучающие артефакты

NAS `/mnt/shared/users/luka/Audi/upmix-training-2026-09-11/` (SMB `/Volumes/private/Audi/…`):
`external/` — исходные датасеты; `runtime/q3-cache-a` — кэш MoisesDB; `runtime/q6-hssr-moises-cache-a`,
`q6b-…`, `q6d-…` — кэши обучения HSSR (features float32 → npz, float16-двойники для `--lazy-cache`);
`runtime/q6d-hssr-moises-train-a` — чекпоинты и `vocal_recall*.json`. Инвентарь попыток:
`docs/UPMIX_NAS_ATTEMPT_INVENTORY.md`.
