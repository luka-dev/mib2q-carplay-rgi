# Техническое устройство bundle

## Почему файлов четыре

Два исправления независимы:

```text
старый stopncm                         SI после retries
sleep 1                               final cleanup
destroy нового carplay0               SwitchPort(direction=0)
        ↓                                     ↓
нет рабочего NCM                      start_stack::none навсегда
```

NCM-исправление обязательно состоит из трёх файлов. Lua передаёт
`busno/devno/iface`; `startncm.sh` и `stopncm.sh` используют их как identity
одного USB enumeration и берут общий per-interface lock. Если stop победил, он
заканчивает teardown до нового mount. Если start победил, поздний stop увидит
другое поколение и не уничтожит новый `carplay0`.

Новый start также:

- проверяет `/dev/io-usb/otg_dev` через `ls`;
- не продолжает работу после failed mount;
- публикует generation только после mount/up;
- исправляет UTF-8 en-dash на ASCII `-s 2 -r 1`;
- пишет решения в `/tmp/carplay_hook.log` как `NCM_FENCE`.

## SI re-arm

Stock MU1316 P5087 после исчерпания CarPlay `TIMEOUT_STARTUP` retries вызывает:

```text
MediaConnectorHandler_Cleanup
→ EnqueueSwitchPort(direction=0)
→ MediaConnectionService_OnJobSwitchPort
→ cleanup + start_stack::none
→ job complete без нового start
```

В binary меняется одна ARM-инструкция:

```text
VA:          0x17FDB4
file offset: 0x7FDB4

stock bytes:   00 30 a0 e3    MOV R3, #0
payload bytes: 2e 33 09 e0    AND R3, R9, LR, LSR #6
```

В этой точке `R9` содержит cleanup error flag, а CarPlay branch получает
`CONNECTION_FLAG_REPORT_CARPLAY_SUPPORTED == 0x40`. Только сочетание
`error=1 + CarPlay` даёт direction 1. Direction 1 уже существует в stock SI и
делает activate-CarPlay, switch-port и `start_stack::device,2`.

Патч не увеличивает retry counter, не добавляет цикл и не касается владения
`dio_manager`.

## Scope по системам проекции

| Система | SI app ID | Влияние SI-патча |
|---|---:|---|
| CarPlay | 0 | error-cleanup меняется с direction 0 на 1 |
| Android Auto / GAL | 1 | идёт по другой cleanup-ветке; инструкция не достигается |
| MirrorLink | 2 | идёт по той же отдельной ветке, что GAL |
| CarLife | 3 | CarPlay flag обнуляется; direction остаётся 0 |

Matching Lua включает strong generation identity только для CarPlay. CarLife
сохраняет stock-compatible no-argument teardown. Поэтому bundle не заявляет,
что лечит отдельный rapid-replug CarLife, пока такой live-дефект не доказан и
не спроектирован отдельный identity contract.

## Что bundle не лечит

Если погиб сам OTG `usblauncher`, one-shot flag распаянного `0x2530`-хаба может
не позволить main launcher создать его снова. Ранее внешний kill/relaunch
возвращал USB node «за спиной» владельца, но не поднимал CarPlay. Поэтому в
bundle нет `rc.local` watchdog, port-reset loop или второго владельца SI.

Также bundle не меняет:

- `verifyPortReset` state 12/14;
- `maxSetupRetries`, startup/restart timing или child config;
- audio hook, APAC, altScreen, renderers, Java или PF;
- Tailscale, Wi-Fi, LTE и memory/CPU policy.

## Точные image boundaries

Все восемь файлов проверяются через `MANIFEST.sha256`. SI дополнительно имеет
размер `802798` байт. Полные хеши находятся в корне bundle; unknown target
никогда не перезаписывается автоматически.
