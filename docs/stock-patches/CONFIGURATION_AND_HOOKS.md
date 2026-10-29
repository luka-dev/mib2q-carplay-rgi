# Конфигурация и библиотеки проекта

## 🔧 PF: обязательные порты

Сохранено изменение разрешённого диапазона `6030` → `6030:6032`: `6031` нужен AltScreen, `6032` — буферному аудио. Это делает `patch_pf()` в [установщике](../../install_MoreIncredibleBash/mod/custom.sh). Четыре штатных профиля и портовые варианты находятся в [deploy/pf](../../deploy/pf/).

IP телефона не закрепляется. Экспериментальные HFSC/ALTQ, QoS-классы, ограничения второго клиента, watcher и публикация IP активной сессии отменены. Receiver-демону не требуется сообщать PF адрес для приоритета. Старое предложение сделать его источником такого IP относится к отклонённой концепции.

Совместимость со старыми пакетами сохраняется: установщик игнорирует их SDIO/PF QoS payload и удаляет устаревшие watcher/flag. Эти защитные проверки и отрицательные тесты остаются в коде намеренно.

## 🔧 Другие изменения штатной конфигурации

Это функциональные исправления установки и подключения; они не являются дополнительными Wi-Fi/SDIO performance-патчами.

| Файл/область | Изменение | Источник и границы |
| --- | --- | --- |
| `smartphone_integrator.json`, child `carplay` | Запуск через `carplay_startup.sh`; остальные child сохраняются | `patch_json()`, [шаблон](../../deploy/smartphone_integrator/carplay_child.json) |
| SI `usb.otgStackCleanupScript` | Generation-aware cleanup вместо безусловного teardown нового NCM | `patch_otg_cleanup()`, [разбор гонки](../connect-and-usb/USB_REPLUG_RECOVERY.md) |
| `startncm.sh`, `stopncm.sh` | Передача поколения USB: поздний teardown старой сессии не уничтожает новый `carplay0` | `patch_ncm()`, только известный stock/собственные версии |
| `usblauncher_otg.lua` | Две строки NCM start/stop, аргументы bus/dev/iface; учёт CarLife | `ncm_lua()`, уникальные ожидаемые строки, неизвестный layout оставляется |
| `dio_manager.json` | Регистрация сообщений hook в правильных направлениях; NMEA mask `OR 0x08` для PASCD | `patch_dio()`, [DIO registration](../build-and-deploy/DEPLOY_AND_RELEASE.md) |
| `lang_data.zip` | Тексты CarPlay UpMix в slots 1000..1005 локалей HMIToneEvoHigh | `patch_lang()`, [исходники](../../deploy/lang/) |
| `mcd.conf` | Активный priority `11,10` → `11,3` | [Описание MCD](mcd.md) |

Реализация и проверки известного layout находятся в [custom.sh](../../install_MoreIncredibleBash/mod/custom.sh). Штатные резервные копии используют `.carplay-stock`; при обновлении сохраняется первая копия. Восстановление целого `lang_data.zip` также отменит внесённые позднее чужие изменения этого ZIP.

## 📦 libcarplay_hook.so

Это собственный interposer проекта, а не изменённая копия stock `.so`. Он обеспечивает функции AltScreen/RGI, аудио, lifecycle и runtime decoder optimization. Полное описание функций живёт в соответствующих предметных документах, а не в истории Wi-Fi-кандидатов.

| Функция | Документ/исходник |
| --- | --- |
| Maps 30 FPS | [Финальная политика](../altscreen-and-video/MAPS_30FPS_PROFILE.md), [maps_profile.h](../../hook/altscreen/maps_profile.h) |
| Decoder runtime | [decoder_optimize_hook.c](../../hook/decoder_optimize/decoder_optimize_hook.c) |
| Видео | [VIDEO_PIPELINE](../altscreen-and-video/VIDEO_PIPELINE.md) |
| Буферное аудио | [BUFFERED_AUDIO_PATH](../audio/BUFFERED_AUDIO_PATH.md) |
| Маршрутизация 5.1 | [DISCRETE_5_1_ROUTING](../audio/DISCRETE_5_1_ROUTING.md) |

Maps-политика всегда добавляет PPID `bf889dfdaea54096`, если исходный IdentificationInfo не содержит PPID. Профиль задаёт `mapsRefreshRate:30`; исходный PPID не перезаписывается. Это целевая частота sender policy, а не гарантия 30 показанных кадров при любой нагрузке. Переключателя environment нет.

## 📦 libcarplay_apac.so

Это собственная библиотека декодера APAC, связанная с hook. Она не является бинарным патчем штатного аудиодрайвера. Декодирование и downmix в 6 каналов описаны в [APAC_CODEC](../audio/APAC_CODEC.md); геометрия конечного QNX PCM/DMA/CSD задаётся [deva-патчем](deva-ctrl-qc.so.md).

## 🧪 Состав пакета

[scripts/network_patcher.py](../../scripts/network_patcher.py) принимает только Wi-Fi runtime schema 2 и проверяет точный набор файлов и SHA. [prepare_install.sh](../../scripts/prepare_install.sh) не добавляет SDIO/QoS stage. Проверки: `tests/test_network_patcher.py`, `scripts/test_install_network_policy.sh`, `scripts/test_install_e2e.sh`. Изменения активного wireless-протокола не становятся автоматически частью ранее собранного установочного ZIP.
