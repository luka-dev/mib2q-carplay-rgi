# devnp-mrvl_wlan-sdiorm.so: полный Wi-Fi-патч

## 📦 Выбранный результат

Файл ГУ: `/mnt/app/armle/lib/dll/devnp-mrvl_wlan-sdiorm.so`. Выбран один полный бинарник, без цепочки накладываемых кандидатов. Штатный SDIO-сервис и io-pkt он не заменяет.

| Свойство | Значение |
| --- | --- |
| Поддержка установщика | Проверенный `MHI2Q_US_AUG22_P5087` / MU1316 |
| Stock | 418361 байт; `d7fbf49b297665cbb8c6ba71b41ac892ccd062e0dcbd8339f3286555db1f394a` |
| Full | 484052 байта; `47a370bc30ca1b038fa5baee1c3eb6f3ef567788a70f57aed3957e98a35a1f05` |
| Артефакты | [driver.so, stock.so, manifest и receipt](../../deploy/wifi/runtime/) |

## 🔧 Что изменено

| Механизм | Назначение |
| --- | --- |
| RX expiry/window | Правильная арифметика 12-битных sequence numbers по модулю 4096 и границы reorder window |
| RX timer hardening | Защита соответствующего пути таймера |
| RX empty/lazy timers | Не будить таймер ради пустого окна; обслуживать накопленные кадры с учётом возраста |
| RX age | Eligibility cap в 6 ticks при 119 Гц, около 50,4 мс; фактическое исполнение зависит от планировщика |
| RX lifecycle | Состояния, поколения и cookies для create/delete/flush, ограниченные пулы и безопасный fallback |
| TX early wake | Объединение запросов пробуждения вместо независимого callback на каждое завершение |
| TX lock ordering | `pthread_mutex_trylock` в gate/request; занятый чужим потоком lock переводит в штатный путь |
| TX exhaustion | При исчерпании ресурса отключение managed gate и возврат к штатному поведению |
| Runtime lifetime | Привязка состояния к жизненному циклу, удержание модуля через `RTLD_NODELETE` |

12 точек изменения: expiry `0x47368`, window `0x46d64`, harden `0x46ed8`, RX `0x473a8`, create `0x47d10`, delete `0x46e8c`, flush `0x472fc`, attach `0x15034`, detach `0x15a38`, RX tail `0x47748`, TX gate `0xf188`, TX request `0xf1a8`. Точные исходные/новые байты и описание ELF находятся в [receipt](../../deploy/wifi/runtime/build-receipt.json).

Payload добавляет исполняемый сегмент и отдельное состояние, сохраняя исходные LOAD, импорты и корректируя relocations. Отсутствие W+X и корректность ELF проверяются отдельно; успешная загрузка модели не доказывает производительность радио.

## 🧪 Проверка и воспроизведение

Сборщик: [build_patched_so.py](../../wireless/driver_re/build_patched_so.py); payload и политики лежат рядом. Нужны ARM toolchain, Python-зависимости моделей и исходные штатные файлы, которые пользователь получает из своей firmware.

```sh
python3 -m wireless.driver_re.build_patched_so STOCK_DRIVER.so /tmp/wifi-full.so   --io-pkt STOCK_ANALYSIS_IO_PKT --build-dir /tmp/wifi-full-build
```

Аналитический `io-pkt` для сборки: `baf6e5df318ba3ea22cf669017b04185a4da8abb77adb4cd6e40629a8e80dae7`. На ГУ проверен также runtime `2f8b1ae6a80a0d0b925996c16288375c76628a470992ed350742dfc06adbf890`. Это разные допустимые роли файлов, а не патч io-pkt. Полученный SHA нужно сравнить с выбранным full; receipt из поставки фиксирует соответствующий результат.

Сохранены native/Unicorn/QNX harness: `full_tx_runtime_probe.py`, `full_rx_runtime_probe.py`, `qnx_full_driver_loader.py` и тесты политик. Последние сохранённые функциональные результаты: TX 28 случаев, RX 12. Модели проверяют инструкции, lock/lifecycle и граничные случаи; физический SDIO, RF и end-to-end latency в них не моделируются. [Результаты на машине](PERFORMANCE.md) — отдельный уровень доказательства.

## 📦 Установка и откат

[Wifi runtime helper](../../deploy/binpatch/wifi_runtime.py) проверяет train, полный SHA и совместимый io-pkt, сохраняет один `.carplay-stock`, заменяет файл через staging/rename и не перезапускает сеть. Неизвестный бинарник оставляет без изменения. Старый трёхфрагментный патч поверх full не применяется.

После нового запуска io-pkt/перезагрузки владельцем проверяется загруженный модуль, а не только SHA файла на диске. Откат — проверенный stock, затем новый запуск. Прежние full-кандидаты не являются вариантами отката: `ff67efd8` содержит ABBA-deadlock, остальные промежуточные варианты описаны в [истории концепций](REJECTED_CONCEPTS.md).
