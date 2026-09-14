# Кто включает основной CarPlay: context 78 / displayable 59

Проверено 2026-09-13 по текущему проекту и декомпилированной штатной Java MU1316.
Поведение не менялось. Live SSH к 100.127.17.86 завершился таймаутом, поэтому
причина задержки именно текущего подключения пока не измерена.

## Почему altScreen может появиться раньше

`TerminalModeBapCombi.ActiveDeviceStateListener.updateActiveDeviceState()` вызывает
наш `CarPlayApp.onActivate()` уже на **ACTIVATING**, не дожидаясь ACTIVE или первого
кадра основного видео. Асинхронный lifecycle worker запускает `AltScreenModule`.
В `AltScreenModule.start()` сразу выставляются `connected=true`, `desiredCtx=81`.
Единственный cluster worker выбирает этот контекст на логическом terminal 1;
при первом захвате используется bounce и 180 ms settle. Условие владения далее —
`connected || videoAvailable`, а не готовность основного 110.

Displayable 99 принадлежит постоянному отдельному `altscreen_render`. Его ранний
RGBA-загрузчик может рисовать собственную картинку до декодирования H.264/111.
Первый действительно показанный декодированный кадр заменяет загрузчик. Поэтому
ранняя видимость 99 не означает, что телефон уже прислал основное изображение 110
или что завершился штатный переход TerminalMode.

Источники в проекте:

- `java_patch/de/audi/app/terminalmode/combi/TerminalModeBapCombi.java:89`
- `java_patch/com/luka/carplay/core/CarPlayApp.java:83`
- `java_patch/com/luka/carplay/core/AltScreenModule.java:309`, `:386`, `:579`
- `altscreen_render/screen_egl_present.c`: early RGBA output, loader thread/handoff
- `docs/CARPLAY_STARTUP_LOADER_PLAN.md`

## Кто физически запрашивает контекст основного HMI

Штатная цепочка, найденная в `Tools/jxe2jar/out/MU1316-vf/`:

```text
ExternalEventsListener.init()
  model TERMINAL_MODE_DISPLAY_CONTEXT_CHOICE (3200035) = 78
  [это только значение модели, не немедленный захват экрана]

TerminalModeScreenBag1.tERMINALMODEIPODOUT()
  создаёт ScreenWidgetEVO(3200000)
  его DisplayControllerEvo привязан к модели 3200035

при подключении нового HMI screen:
  EALManager.newScreenConnected() -> screenPainted=false
  EALManager.draw(): !screenPainted && isContextSwitchNeeded()
    -> lockDisplay(terminal)
    -> handleContextSwitch()
    -> DisplayControllerEvo.prepareAndSwitchToTargetContext()
    -> DisplayControllerEvo.switchToTargetContext()
    -> DisplayManagerMIB2High.switchContext(78, terminal=0, listener)
    -> DisplayManager.switchContext()
    -> DSIDisplayManagement.switchContext(mappedContext, internalDisplay, sessionId)
    -> native DisplayManager / QNX composition
    -> HMI renderer.render(project), unlockDisplay()
```

`DisplayControllerEvo.processModelUpdateEvent()` также может вызвать переключение,
если меняется привязанная модель. Начальное `staticContext=8` у этого виджета не
перебивает модель: `getTargetContextID()` сначала возвращает её значение 78.

Контекст содержит `{16,59}` в основной папке проекта и `{16,103,59}` в эксперименте
курсора. Массив идёт спереди назад. Код объявления массива не определяет момент
показа — его определяет активация соответствующего HMI screen / display controller.
Наш запрет посторонних context writers в DisplayManagerMIB2High действует только
на **terminal 1** (VC), не на основной terminal 0.

Нижний Java `DisplayManager.switchContext()` ждёт подтверждение **до 200 ms**, а
при его отсутствии повторяет DSI-запрос. Здесь не обнаружено фиксированной
10-секундной паузы перед context 78. Подтверждённый контекст обновляется отдельно
от запроса; окно 59 и decoded first frame — ещё два отдельных события.

Точки в штатных исходниках:

- `de/audi/atip/model/ICoreTerminalModeModelBank.java:12`: model 3200035.
- `de/audi/tghu/terminalmode/hmi/evohigh/TerminalModeScreenBag1.java:63`, `:136`.
- `de/esolutions/hmi/widgets/audi/base/eal/EALManager.java:968`, `:1124`, `:1136`.
- `de/esolutions/hmi/widgets/audi/evo/DisplayControllerEvo.java:94`, `:221`, `:305`.
- `de/audi/tghu/fwhmi/DisplayManager.java:151`, `:180`, `:187`, `:230`.

## Что заставляет штатную HMI перейти к CarPlay

Для перехода по запросу устройства:

```text
DIO/CarPlay сообщает DSI updateMode(resources, appStates)
  -> DSICarplayListenerImpl.updateMode()
  -> CarPlayDSIManager.updateMode()
  -> SmartphoneManager.updateMode()
  -> UpdateMode.execute()
  -> StateHandlerImpl.changeState(..., IRequestor.DEVICE)
  -> Resource.SCREEN: MAINUNIT -> DEVICE
  -> ScreenChange.execute()
  -> переключение ChoiceModel 4095 (SWITCH_TO_TERMINAL_MODE_CHOICE)
  -> SystemSMM mediator: model 4095 -> event 2000
  -> штатный переход в TerminalMode / screen 3200000
  -> цепочка DisplayController/EAL выше
```

`SmartphoneManager.updateMode()` сначала предлагает изменение активной команде;
если она его не потребила, создаётся `UpdateMode`. Команды сериализованы, поэтому
по одному входящему DSI событию нельзя утверждать, что screen сменился в тот же миг.
`StateHandlerImpl.changeState()` выполняет эти действия только после `serviceStarted`;
до этого сохраняет состояние через `UpdateMainunitState`.

Штатный CarPlay override для SCREEN является `AbstractStateChangeActionHook`:
он сохраняет базовое действие (`executeSuperAction()=true`). При запросе со стороны
MU дополнительно запускает `RequestModeChangeCarPlay` (таймаут 500 ms).

При открытии CarPlay пользователем есть обратная цепочка: показанный screen
вызывает `AbstractTerminalModeHMIApplication.screenVisible()` -> AP 1001 ->
`ExternalEventsListener` (debounce 70 ms) -> `HMIActivated` -> SCREEN owner DEVICE ->
согласование mode с телефоном. AP 1001 поэтому нельзя считать универсальным
первичным событием включения видеоконтекста: он также приходит как следствие
показа HMI.

Дополнительные точки штатных исходников:

- `de/audi/app/terminalmode/smartphone/carplay/CarPlayDSIManager.java:1018`, `:524`.
- `de/audi/app/terminalmode/SmartphoneManager.java:49`.
- `de/audi/app/terminalmode/statemachine/commands/UpdateMode.java:37`.
- `de/audi/app/terminalmode/statemachine/StateHandlerImpl.java:148`, `:677`.
- `de/audi/app/terminalmode/statemachine/commands/ScreenChange.java:16`.
- `de/audi/atip/model/ICoreSystemModelBank.java:366`: model 4095.
- `de/audi/tghu/system/sm/SystemSMMInitMediators.java:92`.
- `de/audi/app/terminalmode/AbstractTerminalModeHMIApplication.java:119`.

## Почему «10 секунд» пока не диагноз

Окно 59 создаётся внутри stock stream 110/OMX после установления отдельного
screen TCP-соединения. В libairplay `_ScreenThread @ 0x24220` есть **10-секундный
SocketAccept timeout**, а не обязательная задержка: `mov r1,10 @ 0x242a0`, вызов
SocketAccept @ 0x242b0. При успешном раннем соединении ждать все 10 секунд не нужно.
Фingerprint/адреса — в `docs/reference/STOCK_STREAM_110_HU.md` основной папки.

Сохранённый `log/live_20260812_black110_restart/sloginfo.txt` основной папки:

| Событие | Штатное время в этой трассе |
| --- | --- |
| Первая попытка 110: NONE -> INITIALIZING | 00:18:25.036224 |
| INITIALIZING -> STOPPING, screen thread exit | 00:18:35.040612 |
| Новая попытка: NONE -> INITIALIZING | 00:18:45.063130 |
| Создание окна ID 59 | 00:18:45.226294 |
| INITIALIZING -> READY | 00:18:45.348164 |

Итого во второй попытке окно создано за ~163 ms, READY за ~285 ms. Первая попытка
завершилась примерно через 10 секунд; это согласуется с найденным accept timeout,
но данная трасса не содержит прямой строки с результатом SocketAccept. Это старый
сбойный запуск, **не подтверждённая причина сегодняшнего наблюдения**. READY также
не подтверждает физически показанный первый кадр.

Кроме native accept timeout, штатные AbstractCommand и RequestAudioConnection
имеют 10-секундные пределы ожидания. Их наличие не доказывает срабатывание: нужны
события исполнения/завершения команд. В данных сохранённых slog-файлах отсутствуют
подробные Java строки switchContext/DSICarplayListenerImpl, поэтому склеить полную
временную шкалу main HMI по ним нельзя.

## Следующая измеримая проверка

На одном подключении собрать монотонные отметки: TMDevice ACTIVATING; DSI
updateMode с owner SCREEN; ScreenChange; requested/confirmed context 78;
создание окна 59; начало stream 110 TCP; OMX output/успешный post. Это разделит
задержку до запроса HMI, ожидание подтверждения композиции и ожидание видеоданных.
Принудительный context 78 сам по себе не создаёт окно 59 и не ускоряет декодер.
