# Кандидат улучшения вывода VC: реализация 2026-09-15

Изменения собраны локально, на HU не выгружались. Это кандидат для проверки,
а не подтверждение стабильных 30 уникальных кадров/с на приборке.

## altscreen_render

Настройка DisplayPictureBuffer теперь повторяет проверенный OEM ABI:
20 байт, версия 0x101, GetExtensionIndex → GetParameter → enabled/mode по
+12/+16 → SetParameter → readback. Слово +8 сохраняется из GetParameter.
Все ошибки проверяются; decoder не стартует с неподтверждённым режимом.
На input/output явно задаётся и проверяется `xFramerate=30<<16`.

FillDone ставит индекс native output buffer в ограниченную FIFO и возвращается.
Один presenter worker выполняет прежний RGBA draw и возвращает OMX-буфер
только после завершения GPU. Ёмкость очереди равна фактическому OMX output pool:
дополнительных кадровых аллокаций нет. Слот не может одновременно находиться
у OMX и в очереди/на GPU; duplicate/foreign callbacks и ошибки recycling
переводят decoder в существующий путь восстановления.

При остановке закрывается gate callbacks и останавливается worker. Не начатые
кадры следующему сеансу не достаются; выполняющийся GPU draw должен завершиться
до OMX state/free операций. Drain ограничен прежними 1500 мс; при зависании
завершается изолированный renderer без освобождения используемой GPU памяти.

Сохранены native 32m allocation, проверенный RGBA plane 99, обновление UV-binding,
`glFinish`, два RGBA-буфера, swap interval 2 и простой PRISM loader. Возраст
input timestamp не используется для отбрасывания выходных кадров.
Поток показа не запускает новый анимационный loop и работает по готовности буфера.
Recovery остаётся в decode-input worker с сериализацией через gl_lock.

Диагностика разделяет spacing входов **presenter worker**, время самого swap
и полную стоимость present, включая loader/lock/UV/GPU/swap/unbind. Worker spacing
не следует интерпретировать как частоту прихода пакетов от iPhone.

## maneuver_render

OMX/callback очередь здесь не нужна: рендеринг уже выполняется своим потоком.
Исправлен пропуск установки swap interval на пересозданной EGLSurface.
Первичная и повторная привязка теперь используют один helper: проверяются
MakeCurrent и SwapInterval(2), повреждённая поверхность освобождается для retry.
У обоих renderers native Screen interval задаётся до создания новой EGLSurface,
а EGL interval проверяется после привязки. Само наличие interval 2 не объявляется
гарантией 30 FPS MOST capture.

## Общий штатный захват

**videoencoderservice оставляем штатным.** Кандидат `libvc_capture_clock.so`
остановлен после уточнения пользователя о плавной штатной карте. Само
округление 1000/30 до 33 мс не доказывает причину наших 20–27 FPS.
Существующие исходники/пакет эксперимента не относятся к текущей выгрузке.

Текущий комплект `build/renderers-stock-capture-20260915` содержит только
два renderer ELF. Включения нового preload или изменения framework.json нет.

## Проверки

- `scripts/run_tests.sh`: общий host suite прошёл.
- Реальный sink с OMX/GPU stubs: каждый шаг vendor ABI и readback, обе FPS
  настройки, задержки декодирования 20/25/30 fps, FIFO, callback при занятом GPU,
  пустые выходы, recovery health, stop с queued/live кадрами, duplicate/foreign
  outputs, ошибка recycle, bounded fatal drain. ASan/UBSan прошли.
- Presenter с GL stubs: loader join, UV refresh, GPU ownership; искусственные
  50 мс glFinish + 0.3 мс swap отражаются как 50.3 мс полного present.
- Maneuver surface: реальная функция создания/восстановления с EGL stubs,
  повторная установка interval, ошибки bind/interval и повторное восстановление.
- Docker QNX 6.5 GCC 8.5: оба ARM renderer.

Артефакты и логи изолированы в `build/fps-candidate-20260915`.
Проверки остановленного capture-эксперимента в этот набор не входят.
На HU изменения не выгружались.
Следующий шаг после разрешения на HU: проверить OMX readback, частоту входа
111/выхода decoder, полное время present и reconnect/смену окна. Общий штатный
захват при этом не изменяется.
