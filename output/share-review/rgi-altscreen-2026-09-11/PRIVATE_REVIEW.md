# Локальный отчёт: altScreen + hook, без RGI

Обновление по указанию пользователя: RGI исключён из передаваемого набора; добавлены небольшие практические примеры hook. Граница раскрытия учитывает уже публичный репозиторий https://github.com/luka-dev/mib2q-carplay-rgi.

Публичный main проверен на commit `f36790d450392516ed9cbfab9cc06aa13a08bf31` от 2026-09-08. Recursive tree сохранён отдельно в `output/share-review/public-rgi-baseline/baseline.json`; шесть загруженных файлов проверены против Git blob IDs публичного tree. В этом snapshot уже опубликованы RTLD_NEXT, lazy init, process filtering, module framework и часть общего lifecycle. Поэтому эти механизмы не считаются новым закрытым раскрытием. Отсутствие altScreen в проверенном текущем tree не доказывает его отсутствие во всей истории/других ветках; полный historical audit не заявляется.

## Текущий комплект

Для передачи: `output/share/altscreen-hook-excerpts` либо `output/share/altscreen-hook-excerpts.zip`. Всего 12 файлов: 8 C/header файлов (269 строк), README, hook notes, public-source note и checksums.

- Видео: прежние AVCC→Annex-B и bounded framed reader.
- Hook: fictional demo_video_submit, RTLD_NEXT lookup, pthread_once, original call, completed-call counters с сохранением return/errno.
- Process scope: точный basename-match и отдельный UNKNOWN для недоступного /proc. Это адаптация публичного фильтра, а не экспорт production ABI.
- PUBLIC_SOURCE.md явно различает публичную основу hook-примеров и локальные video excerpts. Ссылка на публичный проект разрешена осознанно; персональные пути и закрытые репозитории не раскрыты.

RGI-файлы и прежний ZIP удалены из `output/share`. Оригинальный проект, включая его RGI, не менялся. Закрытые altScreen entry points, negotiation/display contract, offsets, AES, session commands, recovery, lifecycle fixes и platform decoder/backend не включены. Также отсутствуют audio, coverart, Java, deployment, SDK, бинарники, private logs и captures.

## Проверки

1. Video/process helpers: C99, -Wall -Wextra -Werror -pedantic, ASan/UBSan; повреждённые NAL/record lengths, EOF, output bounds, exact process-name matching.
2. Реальная динамическая interposition в локальном macOS harness с вымышленным provider: 256 вызовов из четырёх потоков; аргументы, return и errno сохранены, counters точны. Harness использует flat namespace и явную dependency для корректного порядка поиска RTLD_NEXT. Это host-проверка, не HU-совместимость.
3. Отдельная библиотека без provider: два последовательных вызова возвращают -1/ENOSYS; рекурсивного fallback нет.
4. Package allowlist, text-only, отсутствие hidden files/symlinks, проверка известных private identifiers и отдельный запрет RGI/content. Единственная разрешённая repository URL указывает на проверенный public commit; единственный lookup target — fictional demo_video_submit.
5. Контрольные суммы, нормализация ZIP metadata, byte-for-byte сравнение ZIP с текущей папкой. Старый экспорт был удалён после сверки с его первоначальными хешами; добавившийся Finder .DS_Store также удалён.

`review.json` содержит актуальные результаты, inventory и SHA-256 ZIP; `test_results.json` — результаты host harness. `previous_export_review.json` хранит только локальные metadata старой версии, не исходники для передачи. Все review scripts, шаблоны, тесты, reference checkout, бинарники и этот отчёт находятся вне передаваемой папки и ZIP.

Полезный код может помочь другой разработке; ограничение достигается узким объёмом и отсутствием новой platform/session integration, а не обещанием нулевого прогресса.
