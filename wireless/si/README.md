# SI wireless graft: stages 1 and 2

_MHI2Q MU1316/P5087 — HU stage 1: publish, native NOOP/deletion и USB connect/disconnect/reconnect. Обновление 2026-10-21: stage 2 прошёл native QEMU startup/queue/discovery, UTF-8, отказ Coding и положительный SI-owned DIO receiver lifecycle с тестовым RCC FEC peer; полный комплект активирован на HU 2026-10-21, реальный iPhone вывел wireless-видео; ввод/звук/AltScreen/метаданные пока не проходят._

[Stage-2 implementation and exact evidence limits](../../docs/wireless-carplay/SI_STAGE2_OFFLINE.md). The ABI 1 contracts below describe stage 1. Use `--stage2` explicitly for ABI 2, five hooks (including connect and WLAN transport admission) and wireless control.

Current local extension: [production wireless channels](../../docs/wireless-carplay/WIRELESS_PRODUCTION_CHANNELS.md) documents paired AltScreen, semantic iAP2, native GPS and SIA3 v2 ownership. The new `features` full-cycle profile exercises real AltAV forwarding, RGI/lanes, artwork and vehicle data; old-child state/watchdog/flush/teardown/restart are rejected in the native fixture. Use fresh matched receipts, not the historical counts below. No subsequent HU deployment has occurred.

Latest local update (2026-10-21): the integrated production daemon → native SI/DIO fixture now delivers stereo playback, speech/microphone transport, exact rotary/SELECT reports over encrypted Events and iAP2 Now Playing through the native event queue. `full-metadata-12` verifies two cycles, Cyrillic track text, retained fields on partial updates, playback/app state and elapsed DSI publications. Hardware/HMI edges are simulated; the subsequent fixes have not been deployed. Use `python3 wireless/si/build_si_review.py --full-cycle` to rebuild and package matched integrated fault/stress evidence. See [native metadata and HID evidence](../../docs/wireless-carplay/SI_STAGE2_OFFLINE.md#native-now-playing-and-hid).

Full wireless kit update (2026-10-21): ABI-2 SI is active after the owner reboot; actual image guard and NOOP/deletion pass. Real physical MFi, identification, Wi-Fi bootstrap, SI-owned DIO and main wireless video are confirmed after recreating the old Bluetooth bond. Video size, input, audio, AltScreen and metadata are not accepted. Wi-Fi-off eventually closes the session and SI returns idle. One repeat attempt, complete logs/pcaps and exact source snapshot were saved for offline work. See [physical evidence and remaining checks](../../docs/wireless-carplay/SI_STAGE2_OFFLINE.md#hu-installation-2026-10-21).

Offline update after HU shutdown: transport readiness now precedes native HMI configuration and the first AirPlay start. Original ARM tests pass; native QEMU registers a 1024×480 Screen, knob and trackpad across two DIO generations (32/32 checks). Buffered audio now composes behind the verified wireless HTTP router. Decoder anchors resolve from the explicit library, but QNX refuses text writes, so depth 2 is not enabled there. These changes have not been deployed. [Exact evidence, commands and remaining gaps](../../docs/wireless-carplay/SI_STAGE2_OFFLINE.md#offline-fixes-after-hu-shutdown).

Earlier HU update (2026-10-21): the exec-only launcher is installed and verified after two owner-operated HU reboots. Actual SI image guard, fresh NOOP/deletion and first USB connection pass. Reconnect failed: the native iAP driver timed out waiting for interface enable, then SI recorded DIO TIMEOUT_STARTUP. After a second reboot the first connection works again. The SI patch remains installed; reconnect acceptance is open, and full vehicle power-off/deep sleep was not tested. [Boot audit, deployment and failure evidence](../../docs/wireless-carplay/SI_STAGE1_HU_TEST.md).

---

[Сравнительный RE](../../docs/wireless-carplay/SI_MH2P_COMPARATIVE_RE.md) · [План](../../docs/wireless-carplay/SI_WIRELESS_EXTENSION_PLAN.md) · [Landmines](../../docs/LANDMINES.md)

## 📦 Состав

| Файлы | Назначение |
|---|---|
| [build_candidate.py](build_candidate.py) | Pinned re-arm SI → inplace-кандидат, manifest и generated runtime guard |
| [si_adapter.c](si_adapter.c) | Lifecycle/spawn/system/open interposers, worker, status/NOOP endpoint |
| [si_runtime_guard.c](si_runtime_guard.c) | Проверка immutable-байтов и готовности native SI через callback чтения памяти |
| [si_envv.c](si_envv.c) | Удаление hook namespace и замена env-ключей без дублей |
| [si_protocol.h](si_protocol.h) | Контракт stage 1, native адреса и NOOP |
| [build_si_adapter.sh](build_si_adapter.sh) | QNX-сборка, guards, отдельный diagnostic artifact, receipt |
| [test_si_stage1.py](test_si_stage1.py) | Общий host-набор, включая ARM и native C guard |
| [test_si_trampolines.py](test_si_trampolines.py) | 192 исполнения ARM: null root/slot, pass-through и consumed |
| [test_si_runtime_guard.py](test_si_runtime_guard.py) | 30 отрицательных/положительных сценариев того же C guard |
| [qemu_stage1.py](qemu_stage1.py) | Native broker/Service Manager → SI, ABI 1/2, отдельная VM и receipts |
| [qemu_vehicle_providers.py](qemu_vehicle_providers.py), [test_qemu_vehicle_providers.py](test_qemu_vehicle_providers.py) | Отдельный диагностический профиль: нативные persistence/diagnosis/fec и копия данных машины; RCC adaptation/FEC ещё не восстановлены |
| [si_payload.c](si_payload.c), [test_si_payload.py](test_si_payload.py) | Доставка неизменённых ELF через zlib/QNX4; readback, отрицательные проверки ASan/UBSan |
| [si_qemu_control_probe.c](si_qemu_control_probe.c) | SIW2 → реальная очередь/C++ records; независимое чтение памяти SI и UTF-8 regression |
| [si_startup.c](si_startup.c), [build_si_startup.py](build_si_startup.py) | Exec-only QNX launcher: PID/аргументы сохраняются, диагностические файлы необязательны, candidate/adapter остаются прежними |
| [test_si_startup.py](test_si_startup.py), [qemu_si_boot.py](qemu_si_boot.py) | Host PID/env/exec и native Service Manager boot, включая fault injection недоступной диагностики |
| [si_hu_diagnose.sh](si_hu_diagnose.sh) | Read-only post-boot снимок SI/DIO, USB, interface и логов; без перезапусков и изменений |

## ⚙️ Контракт кандидата

Default `inplace` сохраняет `e_phoff=0x34`, шесть PHDR и расположение исходных секций. Запись PT_ARM_EXIDX заменяется hook PT_LOAD с правами **R+X**, VA `0x1C6000`. Root находится отдельно: **RW, `0x1C5E00`**, в явно зарезервированном zero-filled BSS: `p_memsz` RW-сегмента расширен до `root+4`, `p_filesz` и VA остальных секций сохранены. Существующий re-arm на file offset `0x7FDB4` сохраняется.

Три входа: prepare `0x1800B0`, cleanup `0x17FC48`, FakeConnect job dispatch `0x128DBC`. Трамплин читает адрес root из literal, разыменовывает root, затем читает обработчик из таблицы. При нулевом root/слоте выполняется оригинальный пролог. На consumed-job пути восстанавливаются все шесть сохранённых регистров, после чего `r0=0` и возврат по исходному LR.

Prepare/cleanup в stage 1 — только уведомления, их verdict игнорируется. Wireless prepare/cleanup ещё не реализованы; перед stage 2 нужен отдельный ABI с native контекстом и проверенной маршрутизацией.

## 🔐 Проверка образа и готовности

Worker запускается только после возврата из одного из трёх импортированных lifecycle-методов `servmngt::Application`: `reportAppRunMode`, `reportSystemRunMode`, `setWatchdogPeriod`. Конструкторов, getenv-интерпозера и старта потоков из generic open/spawn нет.

Чтение производится через `/proc/<pid>/as` с `pread`, без SIGSEGV-проб, `siglongjmp` и замены process-wide signal handlers. Generated `si_candidate_guard.h` сверяет ELF/PHDR, re-arm, patched entries, native queue/readiness primitives и всю immutable hook-страницу. Это проверка набора pinned runtime bytes; полный SHA-256 артефакта хранится в manifest/receipt.

Root должен быть свободен. До публикации требуется controller singleton `0x1C5928`, DSI implementation в `controller+0xCC4`, завершённый startup `Impl+0x90==0`, listener из `CControllerInternalJobListener::s_listenerArray` (`0x1C3C90[AO=1]`), равный `controller+0xD8`, и thunk `0x1653C0` в vtable `listener+4`, slot 24. Состояние `2` — startup pending (`0x1809E8`), callback завершения пишет `0` (`0x180724`). GOT lookup `0x128994` резолвит job-listener array, а не mediator registration array `0x1C5118`. Все эти связи проверены на живом штатном SI и кандидате. Ожидание ограничено 30 секундами, проверки повторяются перед publish/ENQ.

## 🌐 Окружение и локальный endpoint

| Настройка | Default |
|---|---|
| `MHI2Q_SI_HOOK` | Выключено; включение только точным `1` |
| `IPL_CONFIG_DIR_SMARTPHONE_INTEGRATOR` | Обязательный SI-gate |
| `MHI2Q_SI_HOOK_ROOT` | `0x1C5E00`; другое значение запрещает публикацию |
| `MHI2Q_SI_HOOK_CHILD` | Точный basename `carplay_startup.sh` либо полный путь |
| `MHI2Q_SI_HOOK_CHILD_ENVS` | Пусто; stage 1 не назначает wireless всем wired-child |
| `MHI2Q_SI_HOOK_STATUS` | `/tmp/mhi2q_si_adapter` с PID-суффиксом |
| `MHI2Q_SI_HOOK_VERBOSE` | Выключено; `1` включает счётчики/диагностику |
| `MHI2Q_SI_HOOK_TRACK_JOBS` | Выключено; `1` наблюдает native deleting destructor только наших NOOP-объектов |

Для контролируемого spawn-теста явно задать `MHI2Q_SI_HOOK_CHILD_ENVS=MHI2Q_DIO_OWNER_MODE=wireless`. Это fixture, не готовый selector транспорта для production. Гал/mirrorlink/carlife не получают инъекцию по совпадению части имени. Все `MHI2Q_SI_HOOK` и `MHI2Q_SI_HOOK_*` удаляются у детей; существующие дубли инъецируемого ключа заменяются одним значением. Ошибка преобразования запрещает spawn вместо передачи исходного непроверенного envv. Содержимое envv в лог не выводится.

`STATUS` возвращает счётчики, `ENQ <deviceID hex> 1000` допускает только NOOP после публикации и проверки готовности. У принятых сокетов установлены receive/send timeouts. Native очередь и удаление наших NOOP проверены на HU. Native enqueue возвращает boolean (1=accepted), который адаптер переводит в свой 0=success. Наблюдатель удаления временно заменяет deleting-destructor slot только приватной vtable наших NOOP, восстанавливает native vptr и зовёт original `0x129050`; `job_deleted` увеличивается после возврата. Shutdown races и полноценный wireless IPC ещё требуют отдельной проверки. Счётчики открытия USB/PPS — диагностика, а не доказательство отсутствия всех USB-команд.

## ✅ Сборка и проверки

```sh
python3 wireless/si/test_si_stage1.py
bash wireless/si/build_si_adapter.sh
# Только для BSP-стенда, отдельный файл с slogf shim:
bash wireless/si/build_si_adapter.sh --diagnostic
```

Сборка автоматически обновляет candidate и generated guard. Production: `libsi_adapter.so`; diagnostic: `libsi_adapter.diagnostic.so`. Оба без дополнительных constructors/emutls; slogf shim экспортируется только diagnostic-вариантом. Точный набор экспортов проверяется. Артефакты и receipts — в `build/si-wireless-stage1/`.

ARM-тесты исполняют сгенерированные инструкции в Unicorn и сравнивают pass-through с оригинальными native прологами. Обработчик реально вызывается и портит caller-saved регистры; проверяются аргументы, SP, LR, регистры и активный стек. C guard проверяется как host shared library с моделью памяти и отказами чтения. Env-тесты покрывают namespace, дубли, неправильные ключи, overflow и точное совпадение пути.

## 📍 Что остаётся

[Физический протокол HU](../../docs/wireless-carplay/SI_STAGE1_HU_TEST.md) фиксирует byte-exact SSH-доставку, lifecycle binding, `published=1`, native NOOP с удалением и wired connect/disconnect/reconnect при одном PID SI. Production `.so` используется без BSP slogf shim. Кандидат оставлен на HU с ручным откатом; таймеров отката нет.

Stage 2 добавляет `si_wire`, `si_controller`, `si_native`, Rust `si_control`, CPRX v3 и bootstrap-runtime. Host: `python3 wireless/si/test_si_stage2.py`; native QEMU: `python3 wireless/si/qemu_stage1.py --stage2 --workdir build/si-wireless-stage2/qemu-acceptance`; пакет с обеими проверками: `python3 wireless/si/build_si_review.py`. Native очередь, записи, stored-ID, DSI-публикация и UTF-8 проверены в настоящем SI. Положительный admission/media требует конфигурационных providers; задержанные ASI appState, orphan recovery и stage 2 на HU ещё не проверены. [Причины прежних ошибок стенда и точные границы доказательств](../../docs/wireless-carplay/SI_STAGE2_OFFLINE.md#native-qemu-bootstrap-and-discovered-utf-8-bug).

## Positive receiver fixture

```sh
python3 wireless/si/qemu_stage1.py --stage2 --vehicle-providers \
  --carplay-coded --rcc-fec-fixture \
  --workdir build/si-native-qemu/positive --timeout 210
```

Real SI → native prepare/spawn → stock DIO/SDK → native ASI → `/info` and CPRX v3,
then cancel and reconnect with a new child/session, retained pairing identity and
external mDNS. RCC grants and the reconstructed coding input are explicitly
simulated; C bootstrap and hardware backends are fixture inputs. No phone media
or HMI/AltScreen success is claimed. TCG child startup budget is 30 seconds;
production remains 10. See the linked implementation record for exact boundaries.
